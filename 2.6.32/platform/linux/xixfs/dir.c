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
 may be distributed under the terms of the GNU General Public License (GPL v2),
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
#else
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
#include <linux/locks.h>
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/nls.h>
#include <asm/uaccess.h>


#include "xcsystem/debug.h"
#include "xixcore/layouts.h"
#include "xcsystem/system.h"
#include "xixcore/ondisk.h"
#include "xixcore/volume.h"
#include "xixcore/hostreg.h"
#include "xixcore/bitmap.h"
#include "xixcore/dir.h"
#include "xixcore/file.h"
#include "xixcore/lotlock.h"

#include "xixfs_global.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"
#include "xixfs_name_cov.h"
#include "xixfs_ndasctl.h"
#include "xixfs_event_op.h"
#include "inode.h"
#include "dentry.h"
#include "misc.h"
#include "dir.h"
#include "file.h"


/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSDIR"

/*
int 
xixfs_fs_to_uni_str(
	const PXIXFS_LINUX_VCB pVCB, 
	const char *ins,
	const int ins_len, 
	__le16 **outs
	)
*/

int
xixfs_find_dir_entry(
	PXIXFS_LINUX_VCB		pVCB,
	PXIXFS_LINUX_FCB 		pFCB,
	uint8			*Name,
	uint32			NameLength,
	uint32			bIgnoreCase,
	PXIXCORE_DIR_EMUL_CONTEXT pDirContext,
	uint64 			*EntryIndex
)
{
	int 	RC = 0;
	memset(pDirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO),
		("ENTER xixfs_find_dir_entry\n"));
	

	RC =xixcore_InitializeDirContext( &pVCB->XixcoreVcb, pDirContext);

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_find_dir_entry: xixfs_InitializeDirContext\n",
				RC));
		goto error_out;
	}



	RC = xixcore_FindDirEntry (&pVCB->XixcoreVcb,
							&pFCB->XixcoreFcb,
							Name,
							NameLength,
							pDirContext,
							EntryIndex,
							bIgnoreCase
    							);

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_find_dir_entry: xixfs_FindDirEntry\n",
				RC));
		goto error_out;
	}	


	
	
error_out:

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO),
		("EXIT xixfs_find_dir_entry\n"));
	
	return RC;
}





int
xixfs_find_dir_entry_by_lotnumber(
	PXIXFS_LINUX_VCB		pVCB,
	PXIXFS_LINUX_FCB 		pFCB,
	uint64			LotNumber,
	PXIXCORE_DIR_EMUL_CONTEXT pDirContext,
	uint64 			*EntryIndex
)
{
	int 	RC = 0;
	
	memset(pDirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO),
		("ENTER xixfs_find_dir_entry_by_lotnumber\n"));


	RC =xixcore_InitializeDirContext( &pVCB->XixcoreVcb, pDirContext);

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_find_dir_entry: xixfs_InitializeDirContext\n",
				RC));
		goto error_out;
	}



	RC = xixcore_FindDirEntryByLotNumber(
							&pVCB->XixcoreVcb, 
							&pFCB->XixcoreFcb, 
							LotNumber,
							pDirContext,
							EntryIndex
							);
    							

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_find_dir_entry: xixfs_FindDirEntry\n",
				RC));
		goto error_out;
	}	


	
	
error_out:

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO),
		("EXIT xixfs_find_dir_entry_by_lotnumber\n"));
	
	return RC;
}




uint32 
IsEmptyDir(
		struct super_block *sb,
		struct inode *inode,
		uint64		LotNumber
)
{


	PXIXFS_LINUX_FCB 				pFCB = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	int 						RC = 0;
	
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	XIXCORE_ASSERT(sb);
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);

	

	RC = xixfs_update_fileInfo(sb, inode, 1 , pFCB->XixcoreFcb.LotNumber); 

	if(RC < 0 ) {
		return 1;
	}

	if(pFCB->XixcoreFcb.ChildCount ==  0) {
		return 1;
	}else {
		return 0;
	}
}


#if LINUX_VERSION_25_ABOVE	
static 
int 
xixfs_create(
	struct inode *dir, 
	struct dentry *dentry, 
	int mode,
	struct nameidata *nd
)
#else
static 
int 
xixfs_create(
	struct inode *dir, 
	struct dentry *dentry, 
	int mode
)
#endif
{

	int 						RC= 0;
	struct super_block 		*sb = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIXFS_LINUX_FCB				pFCB = NULL;
	struct inode 				*inode = NULL;
	uint8					*uni_name = NULL;
	int						uni_name_length = 0;
	//uint32					bLockKernel = 0;
	uint32					bDir = 0;
	XIXCORE_DIR_EMUL_CONTEXT DirContext;
	uint64					NewFileId = 0;
	uint64					EntryIndex = 0;


	
	XIXCORE_ASSERT(dir);
	pFCB = XIXFS_I(dir);
	XIXFS_ASSERT_FCB(pFCB);
	
	sb = dir->i_sb;
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
				("ENTER  xixfs_create  for (%s) mode(%d)\n", dentry->d_name.name, mode));

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_create : is read only .\n"));	
		RC = -EPERM;
		return RC;
	}

	//lock_kernel();
	//bLockKernel = 1;
	
	memset(&DirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));
	
	uni_name_length = xixfs_fs_to_uni_str( pVCB,
										dentry->d_name.name,
										dentry->d_name.len,
										(__le16 **)&uni_name
										);


	if(uni_name_length < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_create:xixfs_fs_to_uni_str \n", uni_name_length));
		uni_name = NULL;
		goto error_out;
	}
	

	

	RC = xixfs_find_dir_entry(
					pVCB,
					pFCB,
					uni_name,
					(uni_name_length * sizeof(__le16)),
					1,
					&DirContext,
					&EntryIndex 
					);

	if(RC == 0  ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_create: xixfs_find_dir_entry\n",
				RC));
		RC = -EEXIST;
		goto error_out;
	}	


	


	bDir = S_ISDIR(mode);



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("CHECK  CREATE  for (%s) is for DIR(%s)\n", dentry->d_name.name, ((bDir)?"DIR":"FILE")));


	RC = xixcore_CreateNewFile (
					&pVCB->XixcoreVcb,
					&pFCB->XixcoreFcb,
					((bDir)?0:1),
					uni_name,
					uni_name_length * sizeof(__le16),
					FILE_ATTRIBUTE_NORMAL,
					&DirContext,
					&NewFileId 
					);


	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ERROR exist entry xixfs_create: xixfs_CreateNewFile\n"));

		goto error_out;
	}


	inode = xixfs_build_inode(sb, bDir, NewFileId );

	if(IS_ERR(inode)) 
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ERROR exist entry xixfs_create: xixfs_build_inode\n"));

		RC  = -1;		

		goto error_out;
	}

	d_instantiate(dentry, inode);
	RC = 0;
	
error_out:	

	xixcore_CleanupDirContext(&DirContext);

	
	//if(bLockKernel) {
	//	unlock_kernel();
	//}

	if(uni_name) {
		xixfs_conversioned_name_free((__le16 *)uni_name);
	}


	if( RC < 0 && !IS_ERR(inode)) {
		iput(inode);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
				("EXIT  xixfs_create \n"));	


	//printk(KERN_DEBUG "EXIT(%d)  xixfs_create  for (%s) mode(%d)\n", RC, dentry->d_name.name, mode);
	return RC;
}


int
xixfs_check_valid_name(const unsigned char * name, unsigned int name_len) 
{
	if(name_len >2) {
		if(name[0] == '.') {
			return -EINVAL;
		}
	}
	
	return 0;
}

#if LINUX_VERSION_25_ABOVE	
struct dentry *
xixfs_lookup(
	struct inode *dir, 
	struct dentry *dentry,
	struct nameidata *nd
)
#else
struct dentry *
xixfs_lookup(
	struct inode *dir, 
	struct dentry *dentry
)
#endif
{

	struct super_block 			*sb = dir->i_sb;
	struct inode 					*inode = NULL;
	PXIXFS_LINUX_VCB					pVCB = NULL;
	PXIXFS_LINUX_FCB					pFCB = NULL;
	int 							RC = 0;
	uint8						*uni_name = NULL;
	int							uni_name_length = 0;
	XIXCORE_DIR_EMUL_CONTEXT 		DirContext;
	//uint32						bLockKernel = 0;
	uint32						bDir = 0;
	uint64						EntryIndex = 0;
	//uint32						type = 0;
	//uint64						StartIndex = 0;


	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	XIXCORE_ASSERT(dir);
	pFCB = XIXFS_I(dir);
	XIXFS_ASSERT_FCB(pFCB);


	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
			("ENTER  xixfs_lookup for (%s)  \n", 
					dentry->d_name.name));
	
	if(dentry == NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR no entry  xixfs_lookup\n"));
		RC = -EINVAL;
		return ERR_PTR(RC);
	}


	if(dentry->d_name.name == NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR no file name xixfs_lookup\n"));

		RC = -EINVAL;
		return ERR_PTR(RC);
	}

	//lock_kernel();
	//bLockKernel = 1;

	/*
	RC = xixfs_check_valid_name(dentry->d_name.name, dentry->d_name.len);


	if(RC < 0 ) {
		return ERR_PTR(RC);
	}
	*/

	dentry->d_op = &xixfs_dentry_operations;


	memset(&DirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));
	
	uni_name_length = xixfs_fs_to_uni_str( pVCB,
										dentry->d_name.name,
										dentry->d_name.len,
										(__le16 **)&uni_name
										);


	if(uni_name_length < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_lookup:xixfs_fs_to_uni_str \n", uni_name_length));
		uni_name =NULL;
		goto error_out;
	}
	

	RC = xixfs_find_dir_entry(
					pVCB,
					pFCB,
					uni_name,
					(uni_name_length * sizeof(__le16)),
					1,
					&DirContext,
					&EntryIndex
					);

	if(RC == -ENOENT ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_lookup: xixfs_find_dir_entry\n",
				RC));

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("NO ENTRY (%s)goto add\n", dentry->d_name.name));
		inode = NULL;
		goto add;
	}else if (RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_lookup: xixfs_find_dir_entry\n",
				RC));
		goto error_out;
	}



	if( XIXCORE_TEST_FLAGS(DirContext.Type, XIFS_FD_TYPE_DIRECTORY) ) {
		bDir = 1;
	}else {
		bDir = 0;
	}


	
	inode = xixfs_build_inode(
				sb, 
				bDir, 
				DirContext.StartLotIndex
				);


	if(IS_ERR(inode)){
		RC = PTR_ERR(inode);
		goto error_out;
	}

	if(dentry){
		//printk(KERN_DEBUG "RESULT(%d) xixfs_lookup  for (%s) \n", RC, dentry->d_name.name);
	}
add:
	RC = 0;
#if LINUX_VERSION_25_ABOVE	
	dentry = d_splice_alias(inode, dentry);
	if (dentry)
		dentry->d_op = &xixfs_dentry_operations;
#else
	dentry->d_op = &xixfs_dentry_operations;
	d_add(dentry, inode);
	dentry = NULL;
#endif


error_out:

	xixcore_CleanupDirContext(&DirContext);

	
	//if(bLockKernel) {
	//	unlock_kernel();
	//}

	if(uni_name) {
		xixfs_conversioned_name_free((__le16 *)uni_name);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
				("EXIT  xixfs_lookup \n"));


	if( RC < 0 && !IS_ERR(inode)) {
		iput(inode);
	}


	
	
	if (RC == 0 ) {
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ENTRY  inode(%p) dentry(%p)\n", inode, dentry));
		return dentry;
	}
		
	return ERR_PTR(RC);

}


int 
xixfs_link (
	struct dentry *old_dentry,
	struct inode *dir,
	struct dentry *new_dentry
)
{
	struct super_block 			*sb = dir->i_sb;
	struct inode 					*inode = NULL;
	PXIXFS_LINUX_VCB					pVCB = NULL;
	PXIXFS_LINUX_FCB					pDir = NULL;
	PXIXFS_LINUX_FCB					pFCB = NULL;
	int 							RC = 0;
	uint8						*uni_name = NULL;
	int							uni_name_length = 0;
	XIXCORE_DIR_EMUL_CONTEXT 		DirContext;
	//uint32						bLockKernel = 0;
	//PXIDISK_CHILD_INFORMATION 	pChild = NULL;
	uint64 						ChildIndex = 0;
	uint64						EntryIndex = 0;


	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	XIXCORE_ASSERT(dir);
	pDir = XIXFS_I(dir);
	XIXFS_ASSERT_FCB(pDir);

	XIXCORE_ASSERT(old_dentry->d_inode);
	inode = old_dentry->d_inode;
	XIXCORE_ASSERT(inode);
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("ENTER  xixfs_link  \n"));

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_CreateNewFile : is read only .\n"));	
		RC = -EPERM;
		return RC;
	}


	//lock_kernel();
	//bLockKernel = 1;

	memset(&DirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));
	
	uni_name_length = xixfs_fs_to_uni_str( pVCB,
										new_dentry->d_name.name,
										new_dentry->d_name.len,
										(__le16 **)&uni_name
										);


	if(uni_name_length < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_link:xixfs_fs_to_uni_str \n", uni_name_length));
		uni_name = NULL;
		goto error_out;
	}
	

	RC = xixfs_find_dir_entry(
					pVCB,
					pDir,
					uni_name,
					(uni_name_length * sizeof(__le16)),
					1,
					&DirContext,
					&EntryIndex
					);

	if( RC ==  0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_link : xixfs_find_dir_entry already exist\n",
				RC));
		RC = -EEXIST;
		goto error_out;
	}


	RC = xixcore_AddChildToDir(
						&pVCB->XixcoreVcb,
						&pDir->XixcoreFcb,
						pFCB->XixcoreFcb.LotNumber,
						((pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE)?XIFS_FD_TYPE_FILE : XIFS_FD_TYPE_DIRECTORY),
						uni_name,
						(uni_name_length * sizeof(__le16)),
						&DirContext,
						&ChildIndex
						);


	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR(0x%x) xixfs_CreateNewFile : xixfs_AddChildToDir.\n", 
				RC));	
	
		RC = -1;
		goto error_out;
	}	



#if LINUX_VERSION_25_ABOVE	
	inode->i_ctime = CURRENT_TIME_SEC;
#else
	inode->i_ctime = CURRENT_TIME;
#endif
	inode->i_nlink++;
	mark_inode_dirty(inode);
	atomic_inc(&inode->i_count);

	pFCB->XixcoreFcb.LinkCount++;
#if LINUX_VERSION_25_ABOVE	
	pFCB->XixcoreFcb.Create_time =  inode->i_ctime.tv_sec * XIXFS_TIC_P_SEC;
#else
	pFCB->XixcoreFcb.Create_time =  inode->i_ctime * XIXFS_TIC_P_SEC;
#endif
	XIXCORE_ASSERT(inode->i_nlink == pFCB->XixcoreFcb.LinkCount);

	XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, (XIXCORE_FCB_MODIFIED_LINKCOUNT| XIXCORE_FCB_MODIFIED_FILE_ATTR));

	mark_inode_dirty(inode);

	d_instantiate(new_dentry, inode);	
#if LINUX_VERSION_25_ABOVE	
	RC = xixfs_write_inode(inode, 1);

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_unlink: xixfs_write_inode\n",
				RC));
		RC = 0;
		goto error_out;
	}
#else
	xixfs_write_inode(inode, 1);
#endif
	RC = 0;


	if(RC == 0 ) {
		xixfs_SendRenameLinkBC(
			XIXFS_SUBTYPE_FILE_LINK, 
			pVCB->XixcoreVcb.HostMac, 
			pFCB->XixcoreFcb.LotNumber, 
			pVCB->XixcoreVcb.VolumeId,
			0, 
			0
			);
	}

	
error_out:

	xixcore_CleanupDirContext(&DirContext);

	
	//if(bLockKernel) {
	//	unlock_kernel();
	//}

	if(uni_name) {
		xixfs_conversioned_name_free((__le16 *)uni_name);
	}


	if( RC < 0 ) {
		iput(inode);
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("EXIT  xixfs_link  \n"));

	return RC;	
	
}


int 
xixfs_unlink(
	struct inode *dir, 
	struct dentry *dentry
)
{

	struct super_block 			*sb = dir->i_sb;
	struct inode 					*inode = NULL;
	PXIXFS_LINUX_VCB					pVCB = NULL;
	PXIXFS_LINUX_FCB					pDir = NULL;
	PXIXFS_LINUX_FCB					pFCB = NULL;
	int 							RC = 0;
	uint8						*uni_name = NULL;
	int							uni_name_length = 0;
	XIXCORE_DIR_EMUL_CONTEXT 		DirContext;
	//uint32						bLockKernel = 0;
	uint64						EntryIndex = 0;



	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	XIXCORE_ASSERT(dir);
	pDir = XIXFS_I(dir);
	XIXFS_ASSERT_FCB(pDir);

	XIXCORE_ASSERT(dentry->d_inode);
	inode = dentry->d_inode;
	XIXCORE_ASSERT(inode);
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("ENTER  xixfs_unlink  Name(%s) \n", dentry->d_name.name));
	

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_CreateNewFile : is read only .\n"));	
		RC = -EPERM;
		return RC;
	}


	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE) {

		if(pFCB->XixcoreFcb.HasLock != INODE_FILE_LOCK_HAS) {
			RC = xixcore_LotLock(
					&pVCB->XixcoreVcb, 
					pFCB->XixcoreFcb.LotNumber, 
					&pFCB->XixcoreFcb.HasLock, 
					1, 
					1
					);

			if(RC < 0 ) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR xixfs_unlink : xixfs_LotLock  .\n"));	
				RC = -EPERM;
				return RC;			
			}
		}
	}


	//lock_kernel();
	//bLockKernel = 1;

	
	memset(&DirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));
	
	uni_name_length = xixfs_fs_to_uni_str( pVCB,
										dentry->d_name.name,
										dentry->d_name.len,
										(__le16 **)&uni_name
										);


	if(uni_name_length < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_unlink:xixfs_fs_to_uni_str \n", uni_name_length));
		uni_name =NULL;
		goto error_out;
	}

									

	RC = xixfs_find_dir_entry(
					pVCB,
					pDir,
					uni_name,
					(uni_name_length * sizeof(__le16)),
					1,
					&DirContext,
					&EntryIndex
					);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_unlink: xixfs_find_dir_entry\n",
				RC));

		goto error_out;
	}


	
	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR) {
		if((!IsEmptyDir(sb, inode, pFCB->XixcoreFcb.LotNumber)) && (inode->i_nlink < 2)) {
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail  xixfs_unlink: Is not empty dir\n"));

			RC = -EINVAL;
			
			goto error_out;
		}
	}

	RC = xixcore_DeleteChildFromDir(
					&pVCB->XixcoreVcb,
					&pDir->XixcoreFcb,
					DirContext.ChildIndex,
					&DirContext
					);

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_unlink: xixfs_DeleteChildFromDir\n",
				RC));
		
		goto error_out;
	}



	

	inode->i_nlink --;
	pFCB->XixcoreFcb.LinkCount--;
#if LINUX_VERSION_25_ABOVE		
	inode->i_ctime = CURRENT_TIME_SEC;
	pFCB->XixcoreFcb.Create_time = inode->i_ctime.tv_sec* XIXFS_TIC_P_SEC;
#else
	inode->i_ctime = CURRENT_TIME;
	pFCB->XixcoreFcb.Create_time = inode->i_ctime* XIXFS_TIC_P_SEC;
#endif
	
	
	XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, (XIXCORE_FCB_MODIFIED_LINKCOUNT| XIXCORE_FCB_MODIFIED_FILE_ATTR));
	
	XIXCORE_ASSERT(inode->i_nlink == pFCB->XixcoreFcb.LinkCount);

	if(pFCB->XixcoreFcb.LinkCount == 0) {

		XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);

		xixfs_inode_detach(inode);
	}		
	
	


	//mark_inode_dirty(inode);
#if LINUX_VERSION_25_ABOVE	
	RC = xixfs_write_inode(inode, 1);

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_unlink: xixfs_write_inode\n",
				RC));
		
		goto error_out;
	}
#else
	xixfs_write_inode(inode, 1);
#endif
	RC = 0;
	
error_out:

	
	xixcore_CleanupDirContext(&DirContext);

	
	//if(bLockKernel) {
	//	unlock_kernel();
	//}

	if(uni_name) {
		xixfs_conversioned_name_free((__le16 *)uni_name);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("EXIT  xixfs_unlink (%d)  \n", RC));

	return RC;
}




int 
xixfs_mkdir(
	struct inode *dir, 
	struct dentry *dentry, 
	int mode
)
{
	int 						RC= 0;
	struct super_block 		*sb = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIXFS_LINUX_FCB				pFCB = NULL;
	struct inode 				*inode = NULL;
	uint8					*uni_name = NULL;
	int						uni_name_length = 0;
	//uint32					bLockKernel = 0;
	uint32					bDir = 0;
	XIXCORE_DIR_EMUL_CONTEXT DirContext;
	uint64					NewFileId = 0;
	uint64					EntryIndex = 0;

	
	XIXCORE_ASSERT(dir);
	pFCB = XIXFS_I(dir);
	XIXFS_ASSERT_FCB(pFCB);
	
	sb = dir->i_sb;
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("ENTER  xixfs_mkdir  \n"));

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_mkdir : is read only .\n"));	
		RC = -EPERM;
		return RC;
	}

	//lock_kernel();
	//bLockKernel = 1;

	memset(&DirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));
	
	uni_name_length = xixfs_fs_to_uni_str( pVCB,
										dentry->d_name.name,
										dentry->d_name.len,
										(__le16 **)&uni_name
										);


	if(uni_name_length < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_mkdir:xixfs_fs_to_uni_str \n", uni_name_length));
		uni_name =NULL;
		goto error_out;
	}
	

	

	RC = xixfs_find_dir_entry(
					pVCB,
					pFCB,
					uni_name,
					(uni_name_length * sizeof(__le16)),
					1,
					&DirContext,
					&EntryIndex
					);

	if(RC == 0  ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_mkdir: xixfs_find_dir_entry\n",
				RC));
		RC = -EEXIST;
		goto error_out;
	}	


	


	bDir = S_ISDIR(mode);
	if(bDir == 0) {
		bDir =1;
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("xixfs_mkdir: invalid mod(%x)\n",
				mode));		
	}
	
	XIXCORE_ASSERT(bDir);


	RC = xixcore_CreateNewFile (
					&pVCB->XixcoreVcb,
					&pFCB->XixcoreFcb,
					0,
					uni_name,
					uni_name_length * sizeof(__le16),
					FILE_ATTRIBUTE_NORMAL,
					&DirContext,
					&NewFileId 
					);


	if(RC <  0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ERROR exist entry xixfs_mkdir: xixfs_CreateNewFile\n"));

		
		goto error_out;
	}


	inode = xixfs_build_inode(sb, bDir, NewFileId );

	if(IS_ERR(inode)) 
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ERROR exist entry xixfs_mkdir: xixfs_build_inode\n"));

		RC  = -1;		

		goto error_out;
	}

	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
		("CREATE DIRECTORY: %s\n inode(%p) dentry(%p)\n", 
			dentry->d_name.name, inode, dentry));

	
	d_instantiate(dentry, inode);
	RC = 0;
	
error_out:	

	xixcore_CleanupDirContext(&DirContext);

	
	//if(bLockKernel) {
	//	unlock_kernel();
	//}

	if(uni_name) {
		xixfs_conversioned_name_free((__le16 *)uni_name);
	}


	if( RC < 0 && !IS_ERR(inode)) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_mkdir : release inode .\n"));	
				
		iput(inode);
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("EXIT  xixfs_mkdir (%d) \n", RC));


	//printk(KERN_DEBUG "EXIT(%d)  xixfs_mkdir  for (%s) mode(%d)\n", RC, dentry->d_name.name, mode);
	
	return RC;	
}



int 
xixfs_rmdir(
	struct inode *dir, 
	struct dentry *dentry
)
{
	int 						RC= 0;
	struct super_block 		*sb = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIXFS_LINUX_FCB				pDir = NULL;
	PXIXFS_LINUX_FCB				pFCB = NULL;
	struct inode 				*inode = NULL;
	uint8					*uni_name = NULL;
	int						uni_name_length = 0;
	//uint32					bLockKernel = 0;
	XIXCORE_DIR_EMUL_CONTEXT DirContext;
	uint64						EntryIndex = 0;

	
	XIXCORE_ASSERT(dir);
	pDir = XIXFS_I(dir);
	XIXCORE_ASSERT(pDir);
	
	sb = dir->i_sb;
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);

	inode = dentry->d_inode;
	XIXCORE_ASSERT(inode);
	pFCB = XIXFS_I(inode);
	XIXCORE_ASSERT(inode);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("ENTER  xixfs_rmdir  \n"));

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_CreateNewFile : is read only .\n"));	
		RC = -EPERM;
		return RC;
	}


	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_DIR) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ERROR exist entry xixfs_create: xixfs_build_inode\n"));
		
		return -EINVAL;
	}



	//lock_kernel();
	//bLockKernel = 1;

	memset(&DirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));
	
	uni_name_length = xixfs_fs_to_uni_str( pVCB,
										dentry->d_name.name,
										dentry->d_name.len,
										(__le16 **)&uni_name
										);


	if(uni_name_length < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_unlink:xixfs_fs_to_uni_str \n", uni_name_length));
		uni_name =NULL;
		goto error_out;
	}

									

	RC = xixfs_find_dir_entry(
					pVCB,
					pDir,
					uni_name,
					(uni_name_length * sizeof(__le16)),
					1,
					&DirContext,
					&EntryIndex
					);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_unlink: xixfs_find_dir_entry\n",
				RC));

		goto error_out;
	}



	
	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR) {

		if(!IsEmptyDir(sb, inode, pFCB->XixcoreFcb.LotNumber)) {
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail  xixfs_unlink: Is not empty dir\n"));

			RC = -EINVAL;
			
			goto error_out;
		}
	}

	RC = xixcore_DeleteChildFromDir(
					&pVCB->XixcoreVcb, 
					&pDir->XixcoreFcb, 
					DirContext.ChildIndex, 
					&DirContext
					);

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_unlink: xixfs_DeleteChildFromDir\n",
				RC));
		
		goto error_out;
	}
	


	

	inode->i_nlink --;
	pFCB->XixcoreFcb.LinkCount--;
#if LINUX_VERSION_25_ABOVE			
	inode->i_ctime = CURRENT_TIME_SEC;
	pFCB->XixcoreFcb.Create_time = inode->i_ctime.tv_sec * XIXFS_TIC_P_SEC;
#else
	inode->i_ctime = CURRENT_TIME;
	pFCB->XixcoreFcb.Create_time = inode->i_ctime * XIXFS_TIC_P_SEC;
#endif
	XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, (XIXCORE_FCB_MODIFIED_LINKCOUNT| XIXCORE_FCB_MODIFIED_FILE_ATTR));
	
	XIXCORE_ASSERT(inode->i_nlink == pFCB->XixcoreFcb.LinkCount);

	if(pFCB->XixcoreFcb.LinkCount == 0) {

		XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);

		xixfs_inode_detach(inode);
	}		
	
	


	mark_inode_dirty(inode);

#if LINUX_VERSION_25_ABOVE	
	RC = xixfs_write_inode(inode, 1);

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) xixfs_unlink: xixfs_write_inode\n",
				RC));
		
		goto error_out;
	}
#else
	xixfs_write_inode(inode, 1);
#endif
	RC = 0;
	
error_out:

	
	xixcore_CleanupDirContext(&DirContext);

	
	//if(bLockKernel) {
	//	unlock_kernel();
	//}

	if(uni_name) {
		xixfs_conversioned_name_free((__le16 *)uni_name);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("EXIT  xixfs_rmdir (%d)  \n", RC));
	
	return RC;
	
	
}


 
int 
xixfs_rename(
	struct inode *old_dir, 
	struct dentry *old_dentry,
	struct inode *new_dir, 
	struct dentry *new_dentry
)
{

	uint8						*Olduni_name = NULL;
	int							Olduni_name_length = 0;
	uint8						*Newuni_name = NULL;
	int							Newuni_name_length = 0;
	struct super_block 			*sb = old_dir->i_sb;
	struct inode 					*old_inode = NULL;
	//struct inode					*ndw_inode = NULL;
	PXIXFS_LINUX_VCB					pVCB = NULL;
	PXIXFS_LINUX_FCB					pOldDir = NULL;
	PXIXFS_LINUX_FCB					pNewDir = NULL;
	PXIXFS_LINUX_FCB					pOldFCB = NULL;
	int 									RC = 0;


	XIXCORE_DIR_EMUL_CONTEXT 		DirContext;
	uint64 							ChildIndex = 0;
	uint64							EntryIndex = 0;
	

	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	XIXCORE_ASSERT(old_dir);
	pOldDir = XIXFS_I(old_dir);
	XIXFS_ASSERT_FCB(pOldDir);

	XIXCORE_ASSERT(new_dir);
	pNewDir = XIXFS_I(old_dir);
	XIXFS_ASSERT_FCB(pNewDir);

	XIXCORE_ASSERT(old_dentry->d_inode);
	old_inode = old_dentry->d_inode;
	XIXCORE_ASSERT(old_inode);
	pOldFCB = XIXFS_I(old_inode);
	XIXFS_ASSERT_FCB(pOldFCB);
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("ENTER  xixfs_rename old Name(%s) new Name(%s) \n", 
			old_dentry->d_name.name,  new_dentry->d_name.name));


	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_rename : is read only .\n"));	
		RC = -EPERM;
		return RC;
	}


	if(pOldFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE) {

		if(pOldFCB->XixcoreFcb.HasLock != INODE_FILE_LOCK_HAS) {
			RC = xixcore_LotLock(
					&pVCB->XixcoreVcb, 
					pOldFCB->XixcoreFcb.LotNumber, 
					&pOldFCB->XixcoreFcb.HasLock, 
					1, 
					1
					);

			if(RC < 0 ) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR xixfs_rename : xixfs_LotLock  .\n"));	
				RC = -EPERM;
				return RC;			
			}
		}
	}
		
	




	if(new_dentry->d_inode != NULL) {

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail  xixfs_rename: invalid set new_dentry has a inode!\n"));
		
		return -EINVAL;
	}


	//lock_kernel();
	//bLockKernel = 1;

	memset(&DirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));


	Olduni_name_length = xixfs_fs_to_uni_str( pVCB,
										old_dentry->d_name.name,
										old_dentry->d_name.len,
										(__le16 **)&Olduni_name
										);



	if(Olduni_name_length < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_rename:xixfs_fs_to_uni_str \n", Olduni_name_length));
		Olduni_name =NULL;
		goto error_out;
	}


	Newuni_name_length = xixfs_fs_to_uni_str( pVCB,
										new_dentry->d_name.name,
										new_dentry->d_name.len,
										(__le16 **)&Newuni_name
										);



	if(Newuni_name_length < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_rename:xixfs_fs_to_uni_str \n", Newuni_name_length));
		Newuni_name = NULL;
		goto error_out;
	}



	
	if(old_dir == new_dir) {
		//	change only name --> change child entry and dentry related dir
		//						change name

		
		// check new file name
		RC = xixfs_find_dir_entry(
						pVCB,
						pOldDir,
						Newuni_name,
						(Newuni_name_length * sizeof(__le16)),
						1,
						&DirContext,
						&EntryIndex
						);

		if( RC == 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail (0x%x) xixfs_rename: xixfs_find_dir_entry already exist entry\n",
					RC));

			RC = -EEXIST;
			goto error_out;
		}		


		// find old file name entry
		RC = xixfs_find_dir_entry(
						pVCB,
						pOldDir,
						Olduni_name,
						(Olduni_name_length * sizeof(__le16)),
						1,
						&DirContext,
						&EntryIndex
						);

		if( RC <  0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail (0x%x) xixfs_rename: xixfs_find_dir_entry:can't find entry\n",
					RC));

			goto error_out;
		}		




		RC = xixcore_UpdateChildFromDir(
						&pVCB->XixcoreVcb,
						&pOldDir->XixcoreFcb,
						DirContext.StartLotIndex,
						DirContext.Type,
						DirContext.State,
						Newuni_name,
						(Newuni_name_length * sizeof(__le16)),
						DirContext.ChildIndex,
						&DirContext
						);
		
		if( RC <  0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail (0x%x) xixfs_rename: xixfs_UpdateChildFromDir\n",
					RC));

			goto error_out;
		}		


		if(SECTORALIGNSIZE_512(pOldFCB->XixcoreFcb.FCBNameLength) < (Newuni_name_length*sizeof(__le16)) ){
			uint8	*tmp  = NULL;
			uint32	size = 0;
			size = SECTORALIGNSIZE_512(Newuni_name_length*sizeof(__le16));
			tmp = kmalloc(size, GFP_KERNEL);
			if(tmp == NULL) {
				goto skip;
			}

			memset(tmp, 0, size);
			kfree(pOldFCB->XixcoreFcb.FCBName);
			pOldFCB->XixcoreFcb.FCBName = tmp;
		}

		pOldFCB->XixcoreFcb.FCBNameLength = Newuni_name_length*sizeof(__le16);
		memcpy(pOldFCB->XixcoreFcb.FCBName, Newuni_name, pOldFCB->XixcoreFcb.FCBNameLength);

		XIXCORE_SET_FLAGS(pOldFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME);		
skip:			

#if LINUX_VERSION_25_ABOVE		
		old_inode->i_mtime = CURRENT_TIME_SEC;
		pOldFCB->XixcoreFcb.Modified_time = old_inode->i_mtime.tv_sec* XIXFS_TIC_P_SEC;
#else
		old_inode->i_mtime = CURRENT_TIME;
		pOldFCB->XixcoreFcb.Modified_time = old_inode->i_mtime* XIXFS_TIC_P_SEC;
#endif
		XIXCORE_SET_FLAGS(pOldFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR);

		xixfs_write_inode(old_inode, 1);
		//d_instantiate(new_dentry, old_inode);
		RC = 0;
	}else {
		// 	change path 	--> remove child entry from old_dir and add new entry to new_dir

		// check new file name
		RC = xixfs_find_dir_entry(
						pVCB,
						pNewDir,
						Newuni_name,
						(Newuni_name_length * sizeof(__le16)),
						1,
						&DirContext,
						&EntryIndex
						);

		if( RC == 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail (0x%x) xixfs_rename: xixfs_find_dir_entry already exist entry\n",
					RC));

			RC = -EEXIST;
			goto error_out;
		}		


		
		// find old file name entry
		RC = xixfs_find_dir_entry(
						pVCB,
						pOldDir,
						Olduni_name,
						(Olduni_name_length * sizeof(__le16)),
						1,
						&DirContext,
						&EntryIndex
						);

		if( RC <  0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail (0x%x) xixfs_rename: xixfs_find_dir_entry:can't find entry\n",
					RC));

			goto error_out;
		}		

		ChildIndex = EntryIndex;


		// Add entry for New path

		RC = xixcore_AddChildToDir(
							&pVCB->XixcoreVcb,
							&pNewDir->XixcoreFcb,
							pOldFCB->XixcoreFcb.LotNumber,
							((pOldFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE)?XIFS_FD_TYPE_FILE : XIFS_FD_TYPE_DIRECTORY),
							Newuni_name,
							(Newuni_name_length * sizeof(__le16)),
							&DirContext,
							&ChildIndex
							);


		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x) xixfs_rename : xixfs_AddChildToDir.\n", 
					RC));	
		
			RC = -1;
			goto error_out;
		}	

		
		// Delete entry for old path
		RC = xixcore_DeleteChildFromDir(
						&pVCB->XixcoreVcb, 
						&pOldDir->XixcoreFcb, 
						EntryIndex, 
						&DirContext
						);

		if(RC < 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail (0x%x) xixfs_rename: xixfs_DeleteChildFromDir\n",
					RC));
			
			goto error_out;
		}

		if(SECTORALIGNSIZE_512(pOldFCB->XixcoreFcb.FCBNameLength) < (Newuni_name_length*sizeof(__le16)) ){
			uint8	*tmp  = NULL;
			uint32	size = 0;
			size = SECTORALIGNSIZE_512(Newuni_name_length*sizeof(__le16));
			tmp = kmalloc(size, GFP_KERNEL);
			if(tmp == NULL) {
				goto skip2;
			}

			memset(tmp, 0, size);
			kfree(pOldFCB->XixcoreFcb.FCBName);
			pOldFCB->XixcoreFcb.FCBName = tmp;
		}

		pOldFCB->XixcoreFcb.FCBNameLength = Newuni_name_length*sizeof(__le16);
		memcpy(pOldFCB->XixcoreFcb.FCBName, Newuni_name, pOldFCB->XixcoreFcb.FCBNameLength);

		XIXCORE_SET_FLAGS(pOldFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME);		
skip2:
	
#if LINUX_VERSION_25_ABOVE		
		old_inode->i_mtime = CURRENT_TIME_SEC;
		pOldFCB->XixcoreFcb.Modified_time = old_inode->i_mtime.tv_sec* XIXFS_TIC_P_SEC;
#else
		old_inode->i_mtime = CURRENT_TIME;
		pOldFCB->XixcoreFcb.Modified_time = old_inode->i_mtime* XIXFS_TIC_P_SEC;
#endif
		XIXCORE_SET_FLAGS(pOldFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR);
		//d_instantiate(new_dentry, old_inode);
		
		xixfs_write_inode(old_inode, 1);

		
	}
	


	if(RC == 0) {
		xixfs_SendRenameLinkBC(
				XIXFS_SUBTYPE_FILE_RENAME, 
				pVCB->XixcoreVcb.HostMac, 
				pOldFCB->XixcoreFcb.LotNumber, 
				pVCB->XixcoreVcb.VolumeId,
				pOldDir->XixcoreFcb.LotNumber, 
				pNewDir->XixcoreFcb.LotNumber
				);
	}



error_out:

	xixcore_CleanupDirContext(&DirContext);

	
	//if(bLockKernel) {
	//	unlock_kernel();
	//}

	if(Olduni_name) {
		xixfs_conversioned_name_free((__le16 *)Olduni_name);
	}

	if(Newuni_name) {
		xixfs_conversioned_name_free((__le16 *)Newuni_name);
	}
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FILEINFO|DEBUG_TARGET_VFSAPIT),
		("EXIT  xixfs_rename  (%d)\n", RC));
	
	return RC;
}





struct inode_operations xixfs_dir_inode_operations = {
	.create		= xixfs_create,
	.lookup		= xixfs_lookup,
	.unlink		= xixfs_unlink,
	.link 		= xixfs_link,
	.mkdir		= xixfs_mkdir,
	.rmdir		= xixfs_rmdir,
	.rename		= xixfs_rename,
};






int 
__xixfs_readdir(
	struct inode *inode, 
	struct file *filp, 
	void *dirent,
	 filldir_t filldir
)
{
	int					RC = 0;
	struct super_block 	*sb = NULL; //inode->i_sb;
	PXIXFS_LINUX_VCB			pVCB = NULL;
	PXIXFS_LINUX_FCB			pFCB = NULL;
	loff_t				vir_d_offset = 0;
	//uint32				bLockKernel =0;
	uint64				RealIndex = 0;
	XIXCORE_DIR_EMUL_CONTEXT 		DirContext;
	//uint64						EntryIndex = 0;
	__le16						*ResolvedName = NULL;
	int32						ResolvedNameLength = 0;
	unsigned long					inum = 0;
	struct inode					*targetInode = NULL;
	uint32						bCheckReal = 0;

	
	XIXCORE_ASSERT(inode);
	
	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);

	pFCB = XIXFS_I(inode);
	XIXCORE_ASSERT(inode);
	
	vir_d_offset = filp->f_pos;
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL,
		("__xixfs_readdir : SEARCH START OFFSET(%lld)\n", vir_d_offset));	


	if(vir_d_offset >= XIXFS_PSUDO_OFF) {
		bCheckReal = 1;
	}
	
	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_DIR) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR __xixfs_readdir : is not Dir\n"));

		return -EINVAL;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO,
		("!!!!!ENTER __xixfs_readdir \n"));


	//lock_kernel();
	//bLockKernel = 1;
	
	if(inode->i_ino== XIXFS_ROOT_INO) {
		XIXCORE_ASSERT(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.Type ,XIFS_FD_TYPE_ROOT_DIRECTORY)) ;

		if(vir_d_offset	 ==0) {
			if( filldir(
				dirent, 
				XIXFS_SELF, 
				strlen(XIXFS_SELF), 
				1, 
				XIXFS_ROOT_INO,
				DT_DIR) )
			{

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("!!!!!ERROR __xixfs_readdir : ROOT DIR CURRENT DIR RETURN  \n"));
				goto error_out;
			}

			vir_d_offset = 1;
			filp->f_pos = 1;

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("!!!!!ROOT DIR: CURRENT N(%s) NL(%d) OFF(%d) INODE(%d)\n",
					XIXFS_SELF, strlen(XIXFS_SELF), 1, XIXFS_ROOT_INO));
			
			
		}


		if(vir_d_offset == 1) {
			if( filldir(
				dirent, 
				XIXFS_PARENT, 
				strlen(XIXFS_PARENT), 
				2, 
				XIXFS_ROOT_INO, 
				DT_DIR) )
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("!!!!!ERROR __xixfs_readdir : ROOT DIR PARENT DIR RETURN\n"));
			
				goto error_out;
			}

			vir_d_offset = 2;
			filp->f_pos = 2;	

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_DIRINFO,
				("!!!!!ROOT DIR: PARENT N(%s) NL(%d) OFF(%d) INODE(%d)\n",
					XIXFS_PARENT, strlen(XIXFS_SELF), 2, XIXFS_ROOT_INO));
			
		}
	}else {

		
		
		if(vir_d_offset == 0) {
			if( filldir(
					dirent, 
					XIXFS_SELF, 
					strlen(XIXFS_SELF), 
					1, 
					inode->i_ino, 
					((pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR)? DT_DIR : DT_REG )) )
			{

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("!!!!!ERROR __xixfs_readdir : DIR(%lld) CURRENT DIR RETURN\n", 
						pFCB->XixcoreFcb.LotNumber));
				
				goto error_out;
			}

			vir_d_offset = 1;
			filp->f_pos = 1;

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_DIRINFO,
				("!!!!!DIR(%lld): CURRENT N(%s) NL(%d) OFF(%d) INODE(%ld)\n",
					pFCB->XixcoreFcb.LotNumber, XIXFS_SELF, strlen(XIXFS_SELF), 1, inode->i_ino));
			
			
		}


		if(vir_d_offset == 1) {
			if( filldir(
				dirent, 
				XIXFS_PARENT, 
				strlen(XIXFS_PARENT), 
				2, 
				parent_ino(filp->f_dentry), 
				((pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR)? DT_DIR : DT_REG ))  )
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("!!!!!ERROR __xixfs_readdir : DIR(%lld) PARENT DIR RETURN\n", 
						pFCB->XixcoreFcb.LotNumber));
				goto error_out;
			}

			vir_d_offset = 2;
			filp->f_pos = 2;

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_DIRINFO,
				("!!!!!DIR(%lld): PARENT N(%s) NL(%d) OFF(%d) INODE(%ld)\n",
					pFCB->XixcoreFcb.LotNumber, XIXFS_PARENT, strlen(XIXFS_SELF), 1, inode->i_ino));
			
		}	
	}

	
	RealIndex = vir_d_offset - XIXFS_PSUDO_OFF;

	

	memset(&DirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));

	RC =xixcore_InitializeDirContext( &pVCB->XixcoreVcb, &DirContext);

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail (0x%x) __xixfs_readdir: xixfs_InitializeDirContext\n",
				RC));
		goto error_out;
	}


	RC = xixcore_LookupInitialDirEntry( 
							&pVCB->XixcoreVcb,
							&pFCB->XixcoreFcb,
							&DirContext,
							(uint64) vir_d_offset 
							);


	if( RC< 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_DIRINFO,
				("ERROR(0x%x)__xixfs_readdir :  xixfs_LookupInitialDirEntry\n", RC));
		goto error_out;
	}

	
	if(RealIndex > 0 ) {
		DirContext.RealDirIndex = (RealIndex - 1);
	}


	if(bCheckReal ) {
		int64			NextRealIndex = 0;
		
		NextRealIndex = (uint64)xixcore_FindSetBit(1024, DirContext.RealDirIndex, DirContext.ChildBitMap);
		if(NextRealIndex == 1024){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Can't find Dir entry\n"));
			RC =  -ENOENT;
			goto error_out;
		}		
	}


	while(1) {

		RC = xixcore_UpdateDirNames(&DirContext);

		if( RC< 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR(0x%x)__xixfs_readdir  : xixfs_UpdateDirNames\n", RC));


			if(RC == -ENOENT) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("NO MORE ENTRY __xixfs_readdir  : xixfs_UpdateDirNames\n"));
				
				RC = 0;
			}
			
			break;
		}			


		ResolvedNameLength = xixfs_uni_to_fs_str(
										pVCB, 
										(__le16 *)DirContext.ChildName,
										DirContext.NameSize,
										(unsigned char **)&ResolvedName,
										0
										);

		if(ResolvedNameLength < 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR(0x%x)__xixfs_readdir  : xixfs_UpdateDirNames\n", RC));
			RC = -ENOMEM;
			goto error_out;			
		}

		RC = 0;

		targetInode = NULL;
		targetInode  = xixfs_iget(sb, DirContext.StartLotIndex);
		
		if(targetInode) {
			inum = targetInode->i_ino;
			iput(targetInode);
		}else {
			inum = iunique(sb, XIXFS_ROOT_INO);
		}

		if( filldir(dirent, 
				(char *)ResolvedName, 
				ResolvedNameLength, 
				DirContext.ChildIndex + XIXFS_PSUDO_OFF, 
				inum, 
				(XIXCORE_TEST_FLAGS(DirContext.Type, XIFS_FD_TYPE_DIRECTORY)? DT_DIR : DT_REG )
				)
		){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("!!!!!ERROR __xixfs_readdir : DIR(%lld) CURRENT DIR RETURN\n", 
						pFCB->XixcoreFcb.LotNumber));
			
			goto error_out;
		}
		vir_d_offset = DirContext.ChildIndex + XIXFS_PSUDO_OFF + 1;
		filp->f_pos = vir_d_offset;			


		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_DIRINFO,
				("!!!!!DIR(%lld): N(%s) NL(%d) OFF(%d) INODE(%ld)\n",
					pFCB->XixcoreFcb.LotNumber, (char *)ResolvedName, ResolvedNameLength, (DirContext.ChildIndex + XIXFS_PSUDO_OFF), inum));

		kfree(ResolvedName);
		ResolvedName = NULL;
		ResolvedNameLength = 0;
	} 

	

error_out:
	xixcore_CleanupDirContext(&DirContext);

	
	//if( bLockKernel ) {
	//	unlock_kernel();
	//}

	if( ResolvedName ) {
		kfree(ResolvedName);
		ResolvedName = NULL;
		ResolvedNameLength = 0;
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_DIRINFO,
		("!!!!!EXIT __xixfs_readdir \n"));
	return RC;
}


static int xixfs_readdir(struct file *filp, void *dirent, filldir_t filldir)
{
	struct inode *inode = filp->f_dentry->d_inode;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_readdir .\n"));	

	
	return __xixfs_readdir(inode, filp, dirent, filldir);
}




int 
xixfs_file_fsync(
	struct file *filp, 
	struct dentry *dentry, 
	int datasync
)
{
	struct inode * inode = dentry->d_inode;
	struct super_block * sb = NULL;
	PXIXFS_LINUX_FCB		pFCB = NULL;
	PXIXFS_LINUX_VCB		pVCB = NULL;
	int ret = 0;

	

	XIXCORE_ASSERT(dentry);
	inode = dentry->d_inode;
	XIXCORE_ASSERT(inode);

	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);
	

	/* sync the inode to buffers */
	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE) {
		ret =xixfs_sync_file(filp, dentry, datasync);
		return ret;
	
	}
	
	/* sync the superblock to buffers */
	lock_super(sb);
	if (sb->s_op->write_super)
		sb->s_op->write_super(sb);
	unlock_super(sb);
	return ret;
}

/*
static struct inode_operations msdos_dir_inode_operations = {
	.create		= msdos_create,
	.lookup		= msdos_lookup,
	.unlink		= msdos_unlink,
	.mkdir		= msdos_mkdir,
	.rmdir		= msdos_rmdir,
	.rename		= msdos_rename,
	.setattr	= fat_notify_change,
};

*/

struct file_operations xixfs_dir_operations = {
	.read		= generic_read_dir,
	.readdir		= xixfs_readdir,
	.fsync		= xixfs_file_fsync
};

/*
struct file_operations fat_dir_operations = {
	.read		= generic_read_dir,
	.readdir	= fat_readdir,
	.ioctl		= fat_dir_ioctl,
	.fsync		= file_fsync,
};
*/
