/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 
      
 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#include "linux_ver.h"
#if LINUX_VERSION_25_ABOVE
#include <linux/fs.h> // blkdev_ioctl , register file system
#include <linux/buffer_head.h> // file_fsync
#include <linux/genhd.h>  // rescan_parition
#include <linux/workqueue.h>  // 
#include <linux/writeback.h>
#else
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
#include <linux/sched.h>
#include <linux/smp_lock.h> // lock_kernel(), unlock_kernel()
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/nls.h>
#include <asm/uaccess.h>
#include <linux/time.h>
#include <linux/fs.h>
#include <linux/sched.h>
#if defined(NDAS_ORG2423)
#include <linux/smp_lock.h>
#endif

#include "xcsystem/debug.h"
#include "xixcore/layouts.h"
#include "xcsystem/system.h"
#include "xixcore/ondisk.h"
#include "xixcore/volume.h"
#include "xixcore/hostreg.h" 
#include "xixcore/bitmap.h"
#include "xixcore/lotlock.h"

#include "xixfs_global.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"
#include "xixfs_name_cov.h"
#include "xixfs_ndasctl.h"
#include "xixfs_event_op.h"
#include "inode.h"
#include "misc.h"

#ifndef __user // for 2.4
#define __user
#endif

/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSFILE"

ssize_t
xixfs_file_read(
		struct file *filp, 
		char __user *buf, 
		size_t count, 
		loff_t *ppos
)
{
#if LINUX_VERSION_25_ABOVE
	struct address_space *mapping = filp->f_mapping;
#else
	struct address_space *mapping = filp->f_dentry->d_inode->i_mapping;
#endif
	struct inode *inode = mapping->host;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	ssize_t 	ret = 0;


	XIXCORE_ASSERT(inode);
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_file_read (%s).\n", filp->f_dentry->d_name.name));	

	

	if(XIXCORE_TEST_FLAGS( pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED )){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR DELETED FILE \n"));
		//printk(KERN_DEBUG "xixfs_file_read ERROR\n");
		return -EPERM;
	}



	//ret =  do_sync_read(filp, buf, count, ppos);
#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
	ret = do_sync_read(filp, buf, count, ppos);
#else
	ret = generic_file_read(filp, buf, count, ppos);
#endif
	//printk(KERN_DEBUG "xixfs_file_read ret (0x%x)\n", ret);
	return ret;
}

ssize_t 
xixfs_file_write(
		struct file *file, 
		const char __user *buf,
		size_t count, 
		loff_t *ppos
)
{

	ssize_t 				RC = 0;
	int64				index = 0;
#if LINUX_VERSION_25_ABOVE	
	struct address_space *mapping = file->f_mapping;
#else
	struct address_space *mapping = file->f_dentry->d_inode->i_mapping;
#endif
	struct inode *inode = mapping->host;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	PXIXFS_LINUX_VCB	pVCB = NULL;

	XIXCORE_ASSERT(inode);
	pVCB = XIXFS_SB(inode->i_sb);
	XIXFS_ASSERT_VCB(pVCB);
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_file_write (%s).\n", file->f_dentry->d_name.name));	


	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_file_write : is read only .\n"));	
		return -EPERM;
	}
	
	XIXCORE_ASSERT(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE);

	if(XIXCORE_TEST_FLAGS( pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED )){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR DELETED FILE \n"));
		
		return -EPERM;
	}




	if( pFCB->XixcoreFcb.HasLock == INODE_FILE_LOCK_HAS) {
		index =(int64) (*ppos);
		
#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE		
		RC = do_sync_write(file, buf, count, ppos);
#else
		RC = generic_file_write(file, buf, count, ppos);
#endif
		if(RC  > 0) {
			if(pFCB->XixcoreFcb.WriteStartOffset ==  -1) {
				pFCB->XixcoreFcb.WriteStartOffset = index;
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
			}

			if(pFCB->XixcoreFcb.WriteStartOffset > index ){
				pFCB->XixcoreFcb.WriteStartOffset = index;
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
			}				
		}

		return RC;
		
	} else {
		return -EPERM;
	}


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("EXIT xixfs_file_write .\n"));	

	return RC;
	
}

#if LINUX_VERSION_25_ABOVE	
ssize_t
xixfs_file_aio_read(
		struct kiocb *iocb,
#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
		const struct iovec *iov,
		unsigned long count,
#else
		char __user *buf, 
		size_t count, 
#endif
		
		loff_t pos
)
{

	struct address_space *mapping = iocb->ki_filp->f_mapping;
	struct inode *inode = mapping->host;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	ssize_t 	ret = 0;

	XIXCORE_ASSERT(inode);
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	if(XIXCORE_TEST_FLAGS( pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED )){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR DELETED FILE \n"));
		//printk(KERN_DEBUG "xixfs_file_aio_read ERROR\n");
		return -EPERM;
	}

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_file_aio_read .\n"));	
#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
	ret = generic_file_aio_read(iocb, iov, count, pos);
#else
	ret =  generic_file_aio_read(iocb, buf, count, pos);
#endif
	//printk(KERN_DEBUG "xixfs_file_aio_read ret (0x%x)\n", ret);
	return ret;
}


ssize_t 
xixfs_file_aio_write(
		struct kiocb *iocb, 
#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
		const struct iovec *iov,
		unsigned long count,
#else
		const char __user *buf,
		size_t count, 
#endif		
		loff_t pos
)
{
	ssize_t 				RC = 0;
	int64				index = 0;
	struct address_space *mapping = iocb->ki_filp->f_mapping;
	struct inode *inode = mapping->host;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	PXIXFS_LINUX_VCB	pVCB = NULL;

	XIXCORE_ASSERT(inode);
	pVCB = XIXFS_SB(inode->i_sb);
	XIXFS_ASSERT_VCB(pVCB);
	
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_file_aio_write .\n"));	

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_file_aio_write : is read only .\n"));	
		return -EPERM;
	}

	
	XIXCORE_ASSERT(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE);

	if(XIXCORE_TEST_FLAGS( pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED )){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR DELETED FILE \n"));
		
		return -EPERM;
	}



	if( pFCB->XixcoreFcb.HasLock == INODE_FILE_LOCK_HAS) {

		index =(int64) pos;

#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
		RC =   generic_file_aio_write(iocb, iov, count, pos);
#else
		RC =   generic_file_aio_write(iocb, buf, count, pos);
#endif				
		

		if(RC  > 0) {
			if(pFCB->XixcoreFcb.WriteStartOffset ==  -1) {
				pFCB->XixcoreFcb.WriteStartOffset = index;
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
			}

			if(pFCB->XixcoreFcb.WriteStartOffset > index ){
				pFCB->XixcoreFcb.WriteStartOffset = index;
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
			}	

			return RC;
		}
	} else {
		return -EPERM;
	}


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("EXIT xixfs_file_aio_write .\n"));	
	
	return RC;

}
#endif


int 
xixfs_ioctl (
		struct inode * inode, 
		struct file * filp, 
		unsigned int cmd,
		unsigned long arg
)
{
	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_ioctl .(%s).\n", filp->f_dentry->d_name.name));	
		
	return -ENOENT;
}

int 
xixfs_file_mmap(
		struct file * file, 
		struct vm_area_struct * vma
)
{
	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_file_mmap (%s).\n", file->f_dentry->d_name.name));	
	
	return generic_file_mmap(file, vma);
}

int 
xixfs_file_open(
		struct inode * inode, 
		struct file * filp
)
{

	struct super_block		*sb = NULL;
	PXIXFS_LINUX_VCB	pVCB = NULL;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	int 			RC = 0;

	
	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);
	
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_file_open (%s). MODE(0x%x). %d(%p)\n", filp->f_dentry->d_name.name, filp->f_flags, inode->i_ino, inode));	
		
	if( pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE) {

		if(XIXCORE_TEST_FLAGS(filp->f_flags, (O_WRONLY|O_RDWR| O_TRUNC|O_APPEND)) ){

			if(pFCB->XixcoreFcb.HasLock != INODE_FILE_LOCK_HAS) {

				RC = xixcore_LotLock(
							&pVCB->XixcoreVcb,
							pFCB->XixcoreFcb.LotNumber, 
							&pFCB->XixcoreFcb.HasLock, 
							1,
							1
							);

				/*
				if( RC < 0 ) {
					return -EINVAL;
				}
				*/
			}
		}
	}

	
	
	return  generic_file_open(inode, filp);
}

#if LINUX_VERSION_25_ABOVE
int 
xixfs_sync_inode(struct inode *inode)
{
	struct writeback_control wbc = {
		.sync_mode = WB_SYNC_ALL,
		.nr_to_write = 0,	/* sys_fsync did this */
	};
	return sync_inode(inode, &wbc);
}
#endif





int 
xixfs_sync_file(
		struct file *file, 
		struct dentry *dentry, 
		int datasync
)
{
	struct inode *inode = dentry->d_inode;
	PXIXFS_LINUX_FCB		pFCB = NULL;
	PXIXFS_LINUX_VCB		pVCB = NULL;
	int err = 0;
	int ret = 0;
	
	

	XIXCORE_ASSERT(inode);
	pVCB = XIXFS_SB(inode->i_sb);
	XIXFS_ASSERT_VCB(pVCB);
	
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_sync_file (%s).\n", file->f_dentry->d_name.name));	


	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_sync_file : is read only .\n"));	
		return -EPERM;
	}

	if(XIXCORE_TEST_FLAGS( pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED )){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR DELETED FILE \n"));
		
		return -EPERM;
	}


		

	XIXCORE_ASSERT(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE);

	if(pFCB->XixcoreFcb.HasLock == INODE_FILE_LOCK_HAS) {
#if LINUX_VERSION_25_ABOVE
		ret = sync_mapping_buffers(inode->i_mapping);
		if (!(inode->i_state & I_DIRTY))
			return ret;
		if (datasync && !(inode->i_state & I_DIRTY_DATASYNC))
			return ret;
		
	
		xixfs_sync_inode(inode);
#endif
		

		
		if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_MODIFIED_FILE)){
#if LINUX_VERSION_25_ABOVE
			err = xixfs_write_inode(inode, 1);
#else
			xixfs_write_inode(inode, 1);
#endif
			if(pFCB->XixcoreFcb.WriteStartOffset != -1){

					printk(KERN_DEBUG "Set Update Information!!!\n");
					xixfs_SendFileChangeRC(
							pVCB->XixcoreVcb.HostMac, 
							pFCB->XixcoreFcb.LotNumber, 
							pVCB->XixcoreVcb.VolumeId, 
							i_size_read(inode), 
							pFCB->XixcoreFcb.RealAllocationSize,
							pFCB->XixcoreFcb.WriteStartOffset
					);
					
					pFCB->XixcoreFcb.WriteStartOffset = -1;
				
			}
			
			if (ret == 0)
				ret = err;
			
			return ret;
			

		}



	
	}else {
		return -EPERM;
	}

	
	return ret;
}


#if LINUX_VERSION_25_ABOVE


#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
ssize_t
xixfs_file_splice_read(
		struct file *in, 
		loff_t *ppos,
		struct pipe_inode_info *pipe, 
		size_t len,
		unsigned int flags
)
{
	return generic_file_splice_read(in, ppos, pipe, len, flags);
	
}

ssize_t
xixfs_file_splice_write(
		struct pipe_inode_info *pipe, 
		struct file *out,
		loff_t *ppos, 
		size_t len, 
		unsigned int flags
)
{
	ssize_t 				RC = 0;
	int64				index = 0;
	struct address_space *mapping = out->f_mapping;
	struct inode *inode = mapping->host;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	PXIXFS_LINUX_VCB	pVCB = NULL;

	XIXCORE_ASSERT(inode);
	pVCB = XIXFS_SB(inode->i_sb);
	XIXFS_ASSERT_VCB(pVCB);
	
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_file_splice_write (%s).\n", out->f_dentry->d_name.name));	

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_file_splice_write : is read only .\n"));	
		return -EPERM;
	}


	if(XIXCORE_TEST_FLAGS( pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED )){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR DELETED FILE \n"));
		
		return -EPERM;
	}


	XIXCORE_ASSERT(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE);
	if(pFCB->XixcoreFcb.HasLock == INODE_FILE_LOCK_HAS) {

		index =(int64) (*ppos);
		
		RC =  generic_file_splice_write(pipe, out, ppos, len, flags);

		if(RC > 0  ) {
			if(pFCB->XixcoreFcb.WriteStartOffset ==  -1) {
				pFCB->XixcoreFcb.WriteStartOffset = index;
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
			}

			if(pFCB->XixcoreFcb.WriteStartOffset > index ){
				pFCB->XixcoreFcb.WriteStartOffset = index;
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
			}							
		}
		
		DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("EXIT xixfs_file_writev (%d).\n", RC));	

		return RC;
	}

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("EXIT xixfs_file_writev ERROR.\n"));	

	return -EPERM;
}
		
#else
ssize_t 
xixfs_file_readv(
		struct file *filp, 
		const struct iovec *iov,
		unsigned long nr_segs, 
		loff_t *ppos
)
{

	struct address_space *mapping = filp->f_mapping;
	struct inode *inode = mapping->host;
	PXIXFS_LINUX_FCB	pFCB = NULL;

	XIXCORE_ASSERT(inode);
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);

	if(XIXCORE_TEST_FLAGS( pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED )){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR DELETED FILE \n"));
		
		return -EPERM;
	}

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_file_readv (%s).\n", filp->f_dentry->d_name.name));	

	return generic_file_readv(filp, iov, nr_segs, ppos);
}

ssize_t 
xixfs_file_writev(
		struct file *file, 
		const struct iovec *iov,
		unsigned long nr_segs, 
		loff_t *ppos
)
{
	ssize_t 				RC = 0;
	int64				index = 0;
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = mapping->host;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	PXIXFS_LINUX_VCB	pVCB = NULL;

	XIXCORE_ASSERT(inode);
	pVCB = XIXFS_SB(inode->i_sb);
	XIXFS_ASSERT_VCB(pVCB);
	
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_file_writev (%s).\n", file->f_dentry->d_name.name));	

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_file_writev : is read only .\n"));	
		return -EPERM;
	}


	if(XIXCORE_TEST_FLAGS( pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED )){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR DELETED FILE \n"));
		
		return -EPERM;
	}


	XIXCORE_ASSERT(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE);
	if(pFCB->XixcoreFcb.HasLock == INODE_FILE_LOCK_HAS) {

		index =(int64) (*ppos);
		
		RC =  generic_file_writev(file, iov, nr_segs, ppos);

		if(RC > 0  ) {
			if(pFCB->XixcoreFcb.WriteStartOffset ==  -1) {
				pFCB->XixcoreFcb.WriteStartOffset = index;
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
			}

			if(pFCB->XixcoreFcb.WriteStartOffset > index ){
				pFCB->XixcoreFcb.WriteStartOffset = index;
				XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
			}							
		}
		
		DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("EXIT xixfs_file_writev (%d).\n", RC));	

		return RC;
	}

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("EXIT xixfs_file_writev ERROR.\n"));	

	return -EPERM;
}
#endif //	LINUX_VERSION_2_6_19_REPLACE_INTERFACE


ssize_t 
xixfs_file_sendfile(
		struct file *in_file, 
		loff_t *ppos,
		size_t count, 
		read_actor_t actor, 
		void *target
)
{
	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_file_sendfile .\n"));	
	
	return generic_file_sendfile(in_file, ppos, count, actor, target);
}
#endif

void 
xixfs_truncate (
		struct inode * inode
)
{

	struct super_block * sb = NULL;
	PXIXFS_LINUX_VCB		pVCB = NULL;
	PXIXFS_LINUX_FCB		pFCB = NULL;

	XIXCORE_ASSERT(inode);
	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);

	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_truncate %d(%p).\n", inode->i_ino, inode));

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_truncate : is read only .\n"));	
		return ;
	}


	if( pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE) {
		if(pFCB->XixcoreFcb.HasLock == INODE_FILE_LOCK_HAS) {
			lock_kernel();
			block_truncate_page(inode->i_mapping,
				inode->i_size, xixfs_get_block);

			//pFCB->FileSize = 0;
			//inode->i_size = pFCB->FileSize;
			//i_size_write(inode, 0);
			//inode->i_blocks = ((inode->i_size + ((1<<inode->i_blkbits) - 1))
			//   				& ~((loff_t)(1<<inode->i_blkbits)- 1)) >> inode->i_blkbits;
#if LINUX_VERSION_25_ABOVE			
			inode->i_mtime = inode->i_ctime = CURRENT_TIME_SEC;
			pFCB->XixcoreFcb.Modified_time = pFCB->XixcoreFcb.Create_time = inode->i_ctime.tv_sec * XIXFS_TIC_P_SEC;
#else
			inode->i_mtime = inode->i_ctime = CURRENT_TIME;
			pFCB->XixcoreFcb.Modified_time = pFCB->XixcoreFcb.Create_time = inode->i_ctime * XIXFS_TIC_P_SEC;
#endif
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, 
				(XIXCORE_FCB_MODIFIED_FILE_TIME| XIXCORE_FCB_MODIFIED_FILE_SIZE) );
			unlock_kernel();
			return ;
		} else {
			DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
				("ERROR: Has no lock. %d($p).\n", inode->i_ino, inode));	
		}
	}

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("EXIT xixfs_truncate .\n"));	

	return ;
}

/*
int 
xixfs_setattr(
		struct dentry *dentry, 
		struct iattr *iattr
)
{
	return -1;
}

ssize_t
xixfs_listxattr(
		struct dentry *dentry, 
		char *buffer, 
		size_t buffer_size
)
{
	return -1;
}

int
xixfs_setxattr(
		struct dentry *dentry, 
		const char *name, 
		const void *value, 
		size_t size, 
		int flag
)
{
	return -1;
}

int
generic_removexattr(
		struct dentry *dentry, 
		const char *name
)
{
	return -1;
}

*/



struct file_operations xixfs_file_operations = {
	.llseek         		= generic_file_llseek,
	.read           		= xixfs_file_read, 
	.write          		= xixfs_file_write,
#if LINUX_VERSION_25_ABOVE	
	.aio_read       		= xixfs_file_aio_read,
	.aio_write      		= xixfs_file_aio_write,
#endif
	.ioctl          		= xixfs_ioctl,
	.mmap           	= xixfs_file_mmap,
	.open           		= xixfs_file_open,
	.fsync          		= xixfs_sync_file,
#if LINUX_VERSION_25_ABOVE	
#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
	.splice_read		= xixfs_file_splice_read,
	.splice_write		= xixfs_file_splice_write,
#else
	.readv          		= xixfs_file_readv,
	.writev         		= xixfs_file_writev,
#endif
	.sendfile       		= xixfs_file_sendfile,
#endif		
};
/*
2.6.19
const struct file_operations fat_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
	.write		= do_sync_write,
	.aio_read	= generic_file_aio_read,
	.aio_write	= generic_file_aio_write,
	.mmap		= generic_file_mmap,
	.release	= fat_file_release,
	.ioctl		= fat_generic_ioctl,
	.fsync		= file_fsync,
	.sendfile	= generic_file_sendfile,
};


const struct file_operations ext2_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
	.write		= do_sync_write,
	.aio_read	= generic_file_aio_read,
	.aio_write	= generic_file_aio_write,
	.ioctl		= ext2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ext2_compat_ioctl,
#endif
	.mmap		= generic_file_mmap,
	.open		= generic_file_open,
	.release	= ext2_release_file,
	.fsync		= ext2_sync_file,
	.sendfile	= generic_file_sendfile,
	.splice_read	= generic_file_splice_read,
	.splice_write	= generic_file_splice_write,
};
end 2.6.19

struct file_operations fat_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= do_sync_read,
	.write		= do_sync_write,
	.readv		= generic_file_readv,
	.writev		= generic_file_writev,
	.aio_read	= generic_file_aio_read,
	.aio_write	= generic_file_aio_write,
	.mmap		= generic_file_mmap,
	.ioctl		= fat_generic_ioctl,
	.fsync		= file_fsync,
	.sendfile	= generic_file_sendfile,
};






struct file_operations ext2_file_operations = {
	.llseek		= generic_file_llseek,
	.read		= generic_file_read,
	.write		= generic_file_write,
	.aio_read	= generic_file_aio_read,
	.aio_write	= generic_file_aio_write,
	.ioctl		= ext2_ioctl,
	.mmap		= generic_file_mmap,
	.open		= generic_file_open,
	.release	= ext2_release_file,
	.fsync		= ext2_sync_file,
	.readv		= generic_file_readv,
	.writev		= generic_file_writev,
	.sendfile	= generic_file_sendfile,
};


*/
struct inode_operations xixfs_file_inode_operations = {
		.truncate       = xixfs_truncate,
		//.setxattr       = xixfs_setxattr,
		// .getxattr       = xixfs_getxattr,
		//.listxattr      = xixfs_listxattr,
		// .removexattr    = xixfs_removexattr,
		// .setattr        = xixfs_setattr
};


/*
struct inode_operations fat_file_inode_operations = {
	.truncate	= fat_truncate,
	.setattr	= fat_notify_change,
};


struct inode_operations ext2_file_inode_operations = {
	.truncate	= ext2_truncate,
#ifdef CONFIG_EXT2_FS_XATTR
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= ext2_listxattr,
	.removexattr	= generic_removexattr,
#endif
	.setattr	= ext2_setattr,
	.permission	= ext2_permission,
};



struct inode_operations ext3_file_inode_operations = {
	.truncate	= ext3_truncate,
	.setattr	= ext3_setattr,
#ifdef CONFIG_EXT3_FS_XATTR
	.setxattr	= generic_setxattr,
	.getxattr	= generic_getxattr,
	.listxattr	= ext3_listxattr,
	.removexattr	= generic_removexattr,
#endif
	.permission	= ext3_permission,
};


*/
