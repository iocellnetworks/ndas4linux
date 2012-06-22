/*
 -------------------------------------------------------------------------
 Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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
#include <linux/parser.h>
#include <linux/statfs.h>
#else
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/nls.h>
#include <asm/uaccess.h>

#include <linux/mount.h>
#include <linux/seq_file.h>
#include <linux/dcache.h>
/*
	for void generate_random_uuid(unsigned char uuid_out[16]);
*/

#include <linux/random.h>

#include "xcsystem/debug.h"
#include "xcsystem/system.h"
#include "xcsystem/errinfo.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/ondisk.h"
#include "xixcore/volume.h"
#include "xixcore/hostreg.h"
#include "xixcore/bitmap.h"
#include "xixcore/lotlock.h"

#include "xixfs_types.h"
#include "xixfs_global.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"
#include "xixfs_name_cov.h"
#include "xixfs_ndasctl.h"
#include "xixfs_event_op.h"
#include "inode.h"
#include "dir.h"
#include "misc.h"

XIXFS_LINUX_DATA	xixfs_linux_global;

#if LINUX_VERSION_WORK_QUEUE
struct workqueue_struct *xixfs_workqueue;
#endif

/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSSUPER"


////////////////////////////////////////////////////////////////////////////////
//
//
//	Super block opreation 
//		
//
//
/////////////////////////////////////////////////////////////////////////////////

static 
void 
xixfs_put_super(
	struct super_block *sb
)
{

	int					RC = 0;
	PXIXFS_LINUX_VCB			pVCB = NULL;
	PXIXFS_LINUX_META_CTX		pCtx = NULL;
	PXIXCORE_META_CTX				pXixcoreCtx = NULL;
#if LINUX_VERSION_25_ABOVE		
	int					TimeOut = 0;
#endif	

	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	pCtx = &(pVCB->MetaCtx);
	pXixcoreCtx = &pVCB->XixcoreVcb.MetaContext;
	XIXCORE_ASSERT(pCtx);


      DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Enter xixfs_put_super\n"));
	
	xixcore_CleanUpAuxLotLockInfo(&pVCB->XixcoreVcb);

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected != 1) {
#if !LINUX_VERSION_25_ABOVE
		XIXFS_WAIT_CTX wait;
#endif
     		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL,
			("!!request Meta thread stop \n"));

		

		spin_lock(&(pCtx->MetaLock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_lock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx));
		
		XIXCORE_SET_FLAGS(pXixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_KILL_THREAD);
		spin_unlock(&(pCtx->MetaLock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_unlock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx));

#if !LINUX_VERSION_25_ABOVE
		xixfs_init_wait_ctx(&wait);
		xixfs_add_metaThread_stop_wait_list(&wait, pCtx);	
#endif
		wake_up(&(pCtx->VCBMetaEvent));
		
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("META THREAD STOP REQ\n"));
#if LINUX_VERSION_25_ABOVE		
		TimeOut = DEFAULT_XIXFS_UMOUNTWAIT;	
		RC = wait_for_completion_timeout(&(pCtx->VCBMetaThreadStopCompletion), TimeOut);
		INIT_COMPLETION(pCtx->VCBMetaThreadStopCompletion);
#else
		wait_for_completion(&(wait.WaitCompletion));
#endif
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("END META THREAD STOP REQ\n"));
		
		

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL,
			("!!replyed Meta thread stop \n"));

		
		RC = xixcore_DeregisterHost(&pVCB->XixcoreVcb);

		
		if(pVCB->XixcoreVcb.MetaContext.HostDirtyLotMap) {
			xixcore_FreeBuffer(pVCB->XixcoreVcb.MetaContext.HostDirtyLotMap->Data);
			kfree(pVCB->XixcoreVcb.MetaContext.HostDirtyLotMap);
			pVCB->XixcoreVcb.MetaContext.HostDirtyLotMap = NULL;
		}
		
		if(pVCB->XixcoreVcb.MetaContext.HostFreeLotMap) {
			xixcore_FreeBuffer(pVCB->XixcoreVcb.MetaContext.HostFreeLotMap->Data);
			kfree(pVCB->XixcoreVcb.MetaContext.HostFreeLotMap);
			pVCB->XixcoreVcb.MetaContext.HostFreeLotMap = NULL;
		}

		if(pVCB->XixcoreVcb.MetaContext.VolumeFreeMap) {
			xixcore_FreeBuffer(pVCB->XixcoreVcb.MetaContext.VolumeFreeMap->Data);
			kfree(pVCB->XixcoreVcb.MetaContext.VolumeFreeMap);
			pVCB->XixcoreVcb.MetaContext.VolumeFreeMap = NULL;
		}
		
		
	}

	xixcore_DeleteALLChildCacheEntry(&pVCB->XixcoreVcb);

	if(pVCB->XixcoreVcb.VolumeName){
		xixcore_FreeMem(pVCB->XixcoreVcb.VolumeName, XCTAG_VOLNAME);
	}

	spin_lock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	list_del(&(pVCB->VCBLink));
	spin_unlock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Exit xixfs_put_super\n"));

	unload_nls(pVCB->nls_map);
	
	kfree(pVCB);
#if LINUX_VERSION_25_ABOVE
	sb->s_fs_info = NULL;
#else
	sb->u.generic_sbp = NULL;
#endif
}

static 
void 
xixfs_write_super(
	struct super_block *sb
)
{
	int					RC = 0;
	PXIXFS_LINUX_VCB			pVCB = NULL;
	PXIXFS_LINUX_META_CTX		pCtx = NULL;
	PXIXCORE_META_CTX				pXixcoreCtx = NULL;
#if LINUX_VERSION_25_ABOVE			
	int					TimeOut = 0;
#endif
	XIXFS_WAIT_CTX		wait;
	
	
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	pCtx = &(pVCB->MetaCtx);
	XIXCORE_ASSERT(pCtx);
	pXixcoreCtx = &pVCB->XixcoreVcb.MetaContext;

      DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Enter xixfs_write_super\n"));
	
	spin_lock(&(pCtx->MetaLock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_lock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx));
	
	XIXCORE_SET_FLAGS(pXixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_UPDATE);
	spin_unlock(&(pCtx->MetaLock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_unlock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx));


	xixfs_init_wait_ctx(&wait);
	xixfs_add_resource_wait_list(&wait, pCtx);


	wake_up(&(pCtx->VCBMetaEvent));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("META UPDATE REQ\n"));
	
#if LINUX_VERSION_25_ABOVE	
	TimeOut = DEFAULT_XIXFS_UPDATEWAIT;
	RC = wait_for_completion_timeout(&(wait.WaitCompletion), TimeOut);
#else
	wait_for_completion(&(wait.WaitCompletion));
	RC =1;
#endif
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("END META UPDATE REQ\n"));
	
	if(RC<= 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
		("Fail xixfs_write_super -->wait_event_timeout \n"));
		return ;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Exit xixfs_write_super\n"));
	
	return;	
}


#if LINUX_VERSION_NEW_SUPER_OP
static 
int 
xixfs_statfs(
	struct dentry * dentry, 
	struct kstatfs  *buf
)
{
	PXIXFS_LINUX_VCB pVCB = XIXFS_SB(dentry->d_sb);
	XIXFS_ASSERT_VCB(pVCB);
	/* If the count of free cluster is still unknown, counts it here. */

      DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Enter xixfs_statfs\n"));


	buf->f_type = dentry->d_sb->s_magic;
	buf->f_bsize = CLUSTER_SIZE;
	buf->f_blocks =( (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE) * pVCB->XixcoreVcb.NumLots );
	if(pVCB->XixcoreVcb.IsVolumeWriteProtected) {
		buf->f_bfree = ( (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE) * pVCB->XixcoreVcb.NumLots );
		buf->f_bavail = ( (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE) * pVCB->XixcoreVcb.NumLots );
	}else{
		buf->f_bfree = ( (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE) * xixcore_FindSetBitCount(pVCB->XixcoreVcb.NumLots, xixcore_GetDataBufferOfBitMap(pVCB->XixcoreVcb.MetaContext.VolumeFreeMap->Data)));
		buf->f_bavail =( (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE) * xixcore_FindSetBitCount(pVCB->XixcoreVcb.NumLots, xixcore_GetDataBufferOfBitMap(pVCB->XixcoreVcb.MetaContext.VolumeFreeMap->Data)));
	}
	buf->f_namelen = 1024;

      DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Exit xixfs_statfs\n"));
	  
	return 0;
}
#else
static 
int 
xixfs_statfs(
	struct super_block *sb,
#if LINUX_VERSION_25_ABOVE	
	struct kstatfs *buf
#else
	struct statfs *buf
#endif
)
{
	PXIXFS_LINUX_VCB pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);
	/* If the count of free cluster is still unknown, counts it here. */

      DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Enter xixfs_statfs\n"));


	buf->f_type = sb->s_magic;
	buf->f_bsize = CLUSTER_SIZE;
	buf->f_blocks =( (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE) * pVCB->XixcoreVcb.NumLots );
	if(pVCB->XixcoreVcb.IsVolumeWriteProtected) {
		buf->f_bfree = ( (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE) * pVCB->XixcoreVcb.NumLots );
		buf->f_bavail = ( (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE) * pVCB->XixcoreVcb.NumLots );
	}else{
		buf->f_bfree = ( (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE) * xixcore_FindSetBitCount(pVCB->XixcoreVcb.NumLots, xixcore_GetDataBufferOfBitMap(pVCB->XixcoreVcb.MetaContext.VolumeFreeMap->Data)));
		buf->f_bavail =( (pVCB->XixcoreVcb.LotSize/CLUSTER_SIZE) * xixcore_FindSetBitCount(pVCB->XixcoreVcb.NumLots, xixcore_GetDataBufferOfBitMap(pVCB->XixcoreVcb.MetaContext.VolumeFreeMap->Data)));
	}
	buf->f_namelen = 1024;

      DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Exit xixfs_statfs\n"));
	  
	return 0;
}
#endif


static 
int 
xixfs_remount(
	struct super_block *sb, 
	int *flags, 
	char *data
)
{

	PXIXFS_LINUX_VCB	pVCB = XIXFS_SB(sb);
	
	XIXFS_ASSERT_VCB(pVCB);

      DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Enter xixfs_remount\n"));

	if( XIXCORE_TEST_FLAGS(sb->s_flags ,MS_RDONLY) ) {
		if( !XIXCORE_TEST_FLAGS(*flags, MS_RDONLY )) {
			return -1;
		}	
	}
	
	*flags |= MS_NODIRATIME ;

      DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Exit xixfs_remount\n"));
	
	return 0;
}


static 
int 
xixfs_show_options(
	struct seq_file *m, 
	struct vfsmount *mnt
)
{
	PXIXFS_LINUX_VCB pVCB = XIXFS_SB(mnt->mnt_sb);
	struct xixfs_mount_option*opts = &pVCB->mountOpts;

      DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Enter xixfs_show_options\n"));

	seq_printf(m, ",uid=%u", -1);
	seq_printf(m, ",gid=%u", -1);
	seq_printf(m, ",umast = %u", 0);
	seq_printf(m, ",iocharset=%s", opts->iocharset);
	seq_printf(m, ",debug=%s", ((opts->enable_debug)?"enabled":"diabled"));

      DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VFSAPIT|DEBUG_TARGET_VCB),
		("Exit xixfs_show_options\n"));
	
	return 0;
}



////////////////////////////////////////////////////////////////////////////////
//
//
//	expert super block operation 
//		
//
//
/////////////////////////////////////////////////////////////////////////////////
/*

static
struct dentry *
xixfs_decode_fh(
	struct super_block *sb, 
	__u32 *fh, 
	int len, 
	int fhtype,
	int (*acceptable)(void *context, struct dentry *de),
	void *context
)
{
	return NULL;
}
	      

static 
int
xixfs_encode_fh(
	struct dentry *de,
	__u32 *fh, 
	int *lenp,
	int connectable
)
{
	return -1;
}



static 
struct dentry *
xixfs_get_dentry(
	struct super_block *sb, 
	void *inump
)
{
	return -1;
}



static 
struct dentry *
xixfs_get_parent(
	struct dentry *child
)
{
	return NULL;
}
*/





static struct super_operations xixfs_sops = {
	.alloc_inode    	= xixfs_alloc_inode,
	.destroy_inode  	= xixfs_destroy_inode,
	.write_inode    	= xixfs_write_inode,
	.delete_inode   	= xixfs_delete_inode,
	.put_super      	= xixfs_put_super,
	.write_super    	= xixfs_write_super,
	.statfs         		= xixfs_statfs,
	.remount_fs     	= xixfs_remount,
	.clear_inode    	= xixfs_clear_inode,
	.read_inode   		= make_bad_inode,
	.show_options		= xixfs_show_options,
};


/*
static struct super_operations fat_sops = {
	.alloc_inode	= fat_alloc_inode,
	.destroy_inode	= fat_destroy_inode,
	.write_inode	= fat_write_inode,
	.delete_inode	= fat_delete_inode,
	.put_super	= fat_put_super,
	.write_super	= fat_write_super,
	.statfs		= fat_statfs,
	.clear_inode	= fat_clear_inode,
	.remount_fs	= fat_remount,

	.read_inode	= make_bad_inode,

	.show_options	= fat_show_options,
};

*/


/*
static struct export_operations xixfs_export_ops = {
	.decode_fh	= xixfs_decode_fh,
	.encode_fh	= xixfs_encode_fh,
	.get_dentry	= xixfs_get_dentry,
	.get_parent	= fat_get_parent,
};
*/
/*
static struct export_operations fat_export_ops = {
	.decode_fh	= fat_decode_fh,
	.encode_fh	= fat_encode_fh,
	.get_dentry	= fat_get_dentry,
	.get_parent	= fat_get_parent,
};
*/



////////////////////////////////////////////////////////////////////////////////
//
//
//
//	Parse Mount parameters.
//
//
//
////////////////////////////////////////////////////////////////////////////////
static char xixf_default_iocharset[] = "utf8";

#if LINUX_VERSION_25_ABOVE	
enum{
	XIXOPT_charset, 
	XIXOPT_quiet, 
	XIXOPT_debug, 
	XIXOPT_debug_level,
	XIXOPT_debug_target,
	XIXOPT_err
	
};


static match_table_t xixfs_tokens = {
	{XIXOPT_charset, "iocharset=%s"},
	{XIXOPT_quiet, "quiet"},
	{XIXOPT_debug, "debug"},
	{XIXOPT_debug_level, "dlevel=%x"},
	{XIXOPT_debug_target, "dtarget=%x"},
	{XIXOPT_err, NULL}
};







static 
int 
parse_options(
	char *options, 
	struct xixfs_mount_option *opts
)
{
	char *p;
	substring_t args[MAX_OPT_ARGS];
	int option;
	char *iocharset;

	opts->enable_debug = 0;
	opts->debug_level_mask = 0;
	opts->debug_target_mask = 0;
	opts->iocharset = xixf_default_iocharset;
	

	if (!options)
		return 0;

	while ((p = strsep(&options, ",")) != NULL) {
		int token;
		if (!*p)
			continue;

		token = match_token(p, xixfs_tokens, args);
		
		switch (token) {
		case XIXOPT_charset:
			if (opts->iocharset != xixf_default_iocharset)
				kfree(opts->iocharset);
			iocharset = match_strdup(&args[0]);
			if (!iocharset)
				return -ENOMEM;
			opts->iocharset = iocharset;
			break;	
		case XIXOPT_quiet:
			opts->enable_debug = 0;
			break;
		case XIXOPT_debug:
			opts->enable_debug= 1;
			break;
		case XIXOPT_debug_level:
			if (match_hex(&args[0], &option))
				return 0;
			opts->debug_level_mask = option;
			break;
		case XIXOPT_debug_target:
			if (match_hex(&args[0], &option))
				return 0;
			opts->debug_target_mask= option;
			break;
		default:
			printk(KERN_ERR
				 "XIXFS: Unrecognized mount option \"%s\" "
				       "or missing value\n", p);
			return -EINVAL;
		}
	}
	
	return 0;
}
#else
static 
int 
parse_options(
	char *options, 
	struct xixfs_mount_option *opts
)
{
	char *p;
	char *this_char,*value,save,*savep;
	int  len = 0;
	int  ret = 0;
	
	opts->enable_debug = 0;
	opts->debug_level_mask = 0;
	opts->debug_target_mask = 0;
	opts->iocharset = xixf_default_iocharset;
	

	if (!options)
		return 0;

	save = 0;
	savep = NULL;
	for (this_char = strtok(options,","); this_char;  this_char = strtok(NULL,",")) {
		if ((value = strchr(this_char,'=')) != NULL) {
			save = *value;
			savep = value;
			*value++ = 0;
		}

		if (!strcmp(this_char,"iocharset") && value) {
			p = value;
			while (*value && *value != ',')
				value++;
			len = value - p;
			if (len) {
				char *buffer;

				buffer = kmalloc(len + 1, GFP_KERNEL);
				if (buffer != NULL) {
					opts->iocharset = buffer;
					memcpy(buffer, p, len);
					buffer[len] = 0;
					printk(KERN_DEBUG "xixfs: IO charset %s\n", buffer);
					ret = 0;
				} else
					ret =  -ENOMEM;
			}
		}else if(!strcmp(this_char,"quiet")) {
			opts->enable_debug = 0;
			ret = 0;
		}else if(!strcmp(this_char,"debug")) {
			opts->enable_debug = 1;
			ret = 0;
		}else if (!strcmp(this_char,"dlevel")) {
			if (!value || !*value) ret = -1;
			else {
				opts->debug_level_mask = simple_strtoul(value,&value,0);
				if (*value) ret = -1;
				else ret = 0;
			}
		}
		else if (!strcmp(this_char,"dtarget")) {
			if (!value || !*value) ret= -1;
			else {
				opts->debug_target_mask = simple_strtoul(value,&value,0);
				if (*value) ret = -1;
				else ret = 0;
			}
		}
		
		if (this_char != options) *(this_char-1) = ',';
		if (value) *savep = save;
		if (ret <  0)
			break;
		
	 }	
	return ret;
}
#endif



////////////////////////////////////////////////////////////////////////////////
//
//
//
//	Initialize Supre block information.
//
//
//
////////////////////////////////////////////////////////////////////////////////

int 
xixfs_checkBootSector(
	struct super_block * 	sb,
	uint32				sectorsize,
	uint32				sectorsizeBit,
	uint64				*VolumeLotIndex,
	uint32				*LotSize,
	uint8				VolumeId[16]
	)
{
	int 						RC = 0;
	PXIX_BUF				BootInfo = NULL;
	PXIX_BUF				LotHeader = NULL;
	PXIX_BUF				VolumeInfo = NULL;
	PXIDISK_VOLUME_INFO				pVolumeHeader = NULL;
	PPACKED_BOOT_SECTOR			pBootSector = NULL;
	uint32					reason = 0;
	uint64					vIndex = 0;
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_VOLINFO, 
			("Enter xixfs_checkBootSector .\n" ));


	VolumeInfo = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_VOLUME_INFO_SIZE);
	if(!VolumeInfo){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("FAIL xixfs_checkBootSector : can't allocat xbuf .\n" ));	
		return -ENOMEM;
	}

	BootInfo = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_VOLUME_INFO_SIZE);
	if(!BootInfo ){
		xixcore_FreeBuffer(&VolumeInfo->xixcore_buffer);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("FAIL xixfs_checkBootSector : can't allocat xbuf .\n" ));	
		return -ENOMEM;
	}

	LotHeader = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
	if(!LotHeader ){
		xixcore_FreeBuffer(&VolumeInfo->xixcore_buffer);
		xixcore_FreeBuffer(&BootInfo->xixcore_buffer);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("FAIL xixfs_checkBootSector : can't allocat xbuf .\n" ));	
		return -ENOMEM;
	}


	memset(xixcore_GetDataBuffer(&VolumeInfo->xixcore_buffer), 0, XIDISK_DUP_VOLUME_INFO_SIZE);
	memset(xixcore_GetDataBuffer(&BootInfo->xixcore_buffer),0, XIDISK_DUP_VOLUME_INFO_SIZE);
	memset(xixcore_GetDataBuffer(&LotHeader->xixcore_buffer),0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	
	RC = xixcore_RawReadBootSector((PXIXCORE_BLOCK_DEVICE)sb->s_bdev, 
									sectorsize, 
									sectorsizeBit,
									0,
									&BootInfo->xixcore_buffer,
									&reason
									);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("FAIL  xixfs_checkBootSector: xixcore_RawReadBootSector %x .\n", RC ));
		if( RC != XCCODE_CRCERROR){
			goto error_out;
		}
	}

	pBootSector = (PPACKED_BOOT_SECTOR)xixcore_GetDataBufferWithOffset(&BootInfo->xixcore_buffer);

	if( (pBootSector->Oem[0] != 'X' ) 
		|| (pBootSector->Oem[1] != 'I' ) 
		|| (pBootSector->Oem[2] != 'F' ) 
		|| (pBootSector->Oem[3] != 'S' )
		|| (pBootSector->Oem[4] != 0 )
		|| (pBootSector->Oem[5] != 0 )
		|| (pBootSector->Oem[6] != 0 )
		|| (pBootSector->Oem[7] != 0 )
	){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Final FAIL  xixfs_checkBootSector: xixcore_RawReadBootSector %x .\n", RC ));
		RC = -EINVAL;
		goto error_out;
			
	}else{


		if( RC == XCCODE_CRCERROR){
			xixcore_ZeroBufferOffset(&BootInfo->xixcore_buffer);
			memset(xixcore_GetDataBuffer(&BootInfo->xixcore_buffer), 0, XIDISK_DUP_VOLUME_INFO_SIZE);
			RC = xixcore_RawReadBootSector((PXIXCORE_BLOCK_DEVICE)sb->s_bdev, 
											sectorsize, 
											sectorsizeBit,
											0,
											&BootInfo->xixcore_buffer,
											&reason
											);

			if( RC < 0 ) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("FAIL  xixfs_checkBootSector: xixcore_RawReadBootSector %x .\n", RC ));
				RC = -EINVAL;
				goto error_out;
			}
			
		}


		pBootSector = (PPACKED_BOOT_SECTOR)xixcore_GetDataBufferWithOffset(&BootInfo->xixcore_buffer);

		if( (pBootSector->Oem[0] != 'X' ) 
			|| (pBootSector->Oem[1] != 'I' ) 
			|| (pBootSector->Oem[2] != 'F' ) 
			|| (pBootSector->Oem[3] != 'S' )
			|| (pBootSector->Oem[4] != 0 )
			|| (pBootSector->Oem[5] != 0 )
			|| (pBootSector->Oem[6] != 0 )
			|| (pBootSector->Oem[7] != 0 )
		){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Final FAIL  xixfs_checkBootSector: xixcore_RawReadBootSector %x .\n", RC ));
			RC = -EINVAL;
			goto error_out;
				
		}


		RC = xixcore_checkVolume((PXIXCORE_BLOCK_DEVICE)sb->s_bdev, 	
								sectorsize, 
								sectorsizeBit,	
								pBootSector->LotSize,				
								(xc_sector_t)pBootSector->FirstVolumeIndex,				
								&VolumeInfo->xixcore_buffer,				
								&LotHeader->xixcore_buffer,				
								VolumeId				
								);

		if(RC < 0){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail  xixfs_CheckBootSector:xixcore_checkVolume .\n"));
			
			xixcore_ZeroBufferOffset(&VolumeInfo->xixcore_buffer);
			memset(xixcore_GetDataBuffer(&VolumeInfo->xixcore_buffer),0, XIDISK_DUP_VOLUME_INFO_SIZE);
			xixcore_ZeroBufferOffset(&LotHeader->xixcore_buffer);
			memset(xixcore_GetDataBuffer(&LotHeader->xixcore_buffer),0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
			RC = xixcore_checkVolume((PXIXCORE_BLOCK_DEVICE)sb->s_bdev, 	
									sectorsize, 
									sectorsizeBit,	
									pBootSector->LotSize,				
									(xc_sector_t)pBootSector->FirstVolumeIndex,				
									&VolumeInfo->xixcore_buffer,				
									&LotHeader->xixcore_buffer,				
									VolumeId				
									);

			if(RC < 0){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail  xixfs_CheckBootSector:xixcore_checkVolume .\n"));
			
				RC = -EINVAL;
				goto error_out;
			}else{
				vIndex = pBootSector->SecondVolumeIndex;
			}
		}else{
			vIndex = pBootSector->FirstVolumeIndex;
		}

		pVolumeHeader = (PXIDISK_VOLUME_INFO)xixcore_GetDataBufferWithOffset(&VolumeInfo->xixcore_buffer);
		
		if((pBootSector->VolumeSignature != pVolumeHeader->VolumeSignature)
			|| (pBootSector->NumLots != pVolumeHeader->NumLots)
			|| (pBootSector->LotSignature != pVolumeHeader->LotSignature)
			|| (pBootSector->XifsVesion != pVolumeHeader->XifsVesion)
			|| (pBootSector->LotSize != pVolumeHeader->LotSize)
		)
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail(0x%x)  Is XIFS_CURRENT_ Volsignature, Lotnumber, Lotsignature, XifsVersion .\n",
				pBootSector->VolumeSignature ));
			RC = -EINVAL;
			goto error_out;
		}

		RC = 0;
		*VolumeLotIndex = vIndex;
		*LotSize = pVolumeHeader->LotSize;
	
	}

error_out:
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_VOLINFO, 
			("End xixfs_checkBootSector %x.\n", RC ));


	xixcore_FreeBuffer(&VolumeInfo->xixcore_buffer);
	xixcore_FreeBuffer(&BootInfo->xixcore_buffer);
	xixcore_FreeBuffer(&LotHeader->xixcore_buffer);
	return RC;
}



#if LINUX_VERSION_25_ABOVE	
int 
#else
struct super_block * 
#endif
xixfs_set_super (
	struct super_block 	*sb, 
	void 				*data, 
	int					silent
	)
{
	PXIXFS_LINUX_VCB	pVcb = NULL;
	uint8		VolumeId[16];
	int			IsReadOnly = 0;
	struct nls_table	*name_tlb = NULL;
	int				RC = 0;
	
	struct xixfs_mount_option m_opts;
	uint32				step = 0;
	uint64				VolumeIndex = 0;
	uint32				LotSize = 0;
DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
	("xixfs_set_super STEP 0 .\n"));


	if(parse_options(data, &m_opts ) < 0 ) {
		printk(KERN_DEBUG "invalid opt\n");
#if LINUX_VERSION_25_ABOVE	
		return -1;
#else
		return NULL;
#endif
	}

	if(m_opts.enable_debug == 1) {
#if defined(XIXCORE_DEBUG)

	XixcoreDebugLevel = DEBUG_LEVEL_ERROR;
	XixcoreDebugTarget =  0; //DEBUG_TARGET_ALL; //DEBUG_TARGET_THREAD;//0;//DEBUG_TARGET_TEST;
	
	//XixcoreDebugLevel = m_opts.debug_level_mask;
	//XixcoreDebugTarget = m_opts.debug_target_mask;
#endif		
	}



	
	down(&(xixfs_linux_global.sb_datalock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("down(&(xixfs_linux_global.sb_datalock)) \n" ));


	if(xixfs_linux_global.IsInitialized == 0) {

		//
		//	initialize event ser/cli
		//
		memset((void *)&xixfs_linux_global.EventCtx, 0, sizeof(XIXFS_SERVICE_CTX));

		init_completion(&(xixfs_linux_global.EventCtx.SrvCompletion));
		init_waitqueue_head(&(xixfs_linux_global.EventCtx.SrvEvent));
		
		spin_lock_init(&(xixfs_linux_global.EventCtx.RecvPktListLock));
		INIT_LIST_HEAD(&(xixfs_linux_global.EventCtx.RecvPktList));						
		xixfs_linux_global.EventCtx.SrvCtx.IsSrvType = 1;
		xixfs_linux_global.EventCtx.SrvCtx.Context = (void *)&(xixfs_linux_global.EventCtx);
		xixfs_linux_global.EventCtx.SrvCtx.error_handler = xixfs_event_handler_SrvErrorNetwork;
		xixfs_linux_global.EventCtx.SrvCtx.packet_handler = xixfs_event_handler_RcvPaket;
		xixfs_linux_global.EventCtx.SrvThreadId = -1;
		
		
		init_completion(&(xixfs_linux_global.EventCtx.CliCompletion));
		init_waitqueue_head(&(xixfs_linux_global.EventCtx.CliEvent));
#if !LINUX_VERSION_25_ABOVE
		init_timer(&(xixfs_linux_global.EventCtx.CliEventTimeOut));
		xixfs_linux_global.EventCtx.CliEventTimeOut.function = xixfs_clievent_timeouthandler;
#endif
		
		spin_lock_init(&(xixfs_linux_global.EventCtx.SendPktListLock));
		INIT_LIST_HEAD(&(xixfs_linux_global.EventCtx.SendPktList));	
		xixfs_linux_global.EventCtx.CliCtx.IsSrvType = 0;
		xixfs_linux_global.EventCtx.CliCtx.Context = (void *)&(xixfs_linux_global.EventCtx);
		xixfs_linux_global.EventCtx.CliCtx.error_handler = xixfs_event_handler_CliErrorNetwork;
		xixfs_linux_global.EventCtx.CliCtx.packet_handler = NULL;
		xixfs_linux_global.EventCtx.CliThreadId = -1;

		spin_lock_init(&(xixfs_linux_global.EventCtx.Eventlock));

		xixfs_linux_global.EventCtx.GDCtx = &(xixfs_linux_global);

		//
		//	create event ser/cli
		//

		xixfs_linux_global.EventCtx.SrvThreadId = kernel_thread(
											xixfs_event_server, 
											&(xixfs_linux_global.EventCtx), 
											CLONE_KERNEL);


		if(xixfs_linux_global.EventCtx.SrvThreadId < 0 )
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_THREAD,
				("kernel_thread() for event server failed.\n"));

			up(&(xixfs_linux_global.sb_datalock));
#if LINUX_VERSION_25_ABOVE	
			return -1;
#else
			return NULL;
#endif
			
		}

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("EVENT SRV START REQ\n"));
		wait_for_completion(&(xixfs_linux_global.EventCtx.SrvCompletion));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("END EVENT SRV START REQ\n"));
		INIT_COMPLETION(xixfs_linux_global.EventCtx.SrvCompletion);

		/*
		 * Set Xixcore global's HostMac.
		 * TODO: May be called multiple times.
		 */
		memset(xixcore_global.HostMac, 0, 32);
		memcpy(&xixcore_global.HostMac[26], xixfs_linux_global.EventCtx.SrvCtx.myaddr.slpx_node, 6);


		xixfs_linux_global.EventCtx.CliThreadId =  kernel_thread(
											xixfs_event_client, 
											&(xixfs_linux_global.EventCtx), 
											CLONE_KERNEL);


		if(xixfs_linux_global.EventCtx.CliThreadId < 0 )
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_THREAD,
				("kernel_thread() for event client failed.\n"));

			up(&(xixfs_linux_global.sb_datalock));

#if LINUX_VERSION_25_ABOVE	
			return -1;
#else
			return NULL;
#endif
		}
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("EVENT CLIE START REQ\n"));
		wait_for_completion(&(xixfs_linux_global.EventCtx.CliCompletion));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("END EVENT CLIE START REQ\n"));
		
		INIT_COMPLETION(xixfs_linux_global.EventCtx.CliCompletion);

		xixfs_linux_global.IsInitialized = 1;
	}



	
	up(&(xixfs_linux_global.sb_datalock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("up(&(xixfs_linux_global.sb_datalock)) \n" ));


	if( xixfs_ScanForPreMounted(sb) < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) xixfs_ScanForPreMounted.\n", RC));
		goto error_out;
	}

	


	if(xixfs_checkNdas(sb, &IsReadOnly) < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail xixfs_checkNdas.\n"));
		goto error_out;
	}

	


	memset(VolumeId, 0, 16);


	if(xixfs_checkBootSector(sb, 512, 9, &VolumeIndex, &LotSize, VolumeId ) < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail xixfs_checkBootSector.\n"));
		goto error_out;				
	}



	name_tlb = load_nls(m_opts.iocharset);
	if(!name_tlb) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail load_nls utf8.\n"));
		goto error_out;
	}
	step = 1;
DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
	("xixfs_set_super STEP 1 .\n"));

	//
	//	make xixfs volume info and initialize it.
	//
	pVcb = kmalloc(sizeof(XIXFS_LINUX_VCB), GFP_KERNEL);


	if ( !pVcb ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail allocate VCB.\n"));
		goto error_out;
	}

	
	memset((void *)pVcb, 0, sizeof( XIXFS_LINUX_VCB ));

#if LINUX_VERSION_25_ABOVE	
	pVcb->N_TYPE.Type = XIXFS_NODE_TYPE_VCB;
	pVcb->N_TYPE.Size = sizeof(XIXFS_LINUX_VCB);
#else
	pVcb->n_type.Type = XIXFS_NODE_TYPE_VCB;
	pVcb->n_type.Size = sizeof(XIXFS_LINUX_VCB);
#endif

	INIT_LIST_HEAD(&pVcb->VCBLink);
	rwlock_init(&pVcb->VCBLock);
	pVcb->VCBState = XIXFS_VCB_STATUS_VOL_IN_MOUNTING;
	pVcb->VCBFlags = 0;
	pVcb->supreblockCtx = sb; 

	/*
	 * Initialize Xixcore VCB
	 */
	xixcore_InitializeVolume(
		&pVcb->XixcoreVcb,
		(PXIXCORE_BLOCK_DEVICE)sb->s_bdev,
		(PXIXCORE_SPINLOCK)&(pVcb->ChildCacheLock),
		IsReadOnly,
		512,
		9,
		SECTORSIZE_4096,
		VolumeId
		);

	if(IsReadOnly) {
		XIXCORE_SET_FLAGS(sb->s_flags,  MS_RDONLY);
	}

#if LINUX_VERSION_25_ABOVE	
	memcpy(&(pVcb->PartitionInfo), sb->s_bdev->bd_part, sizeof(struct hd_struct));

#else
	if( xixfs_getPartiton(sb, &(pVcb->PartitionInfo)) < 0) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail xixfs_getPartiton.\n"));
		goto error_out;		
	}
#endif	

	pVcb->nls_map = name_tlb;


	memset((void *)&pVcb->MetaCtx, 0, sizeof(XIXFS_LINUX_META_CTX));

#if LINUX_VERSION_25_ABOVE	
	init_completion(&(pVcb->MetaCtx.VCBMetaThreadStopCompletion));
#else
	spin_lock_init(&(pVcb->MetaCtx.VCBMetaThreadStopWaitSpinlock));
	INIT_LIST_HEAD(&(pVcb->MetaCtx.VCBMetaThreadStopWaitList));
#endif
	//init_waitqueue_head(&(pVcb->MetaCtx.VCBResourceEvent));
	spin_lock_init(&(pVcb->MetaCtx.VCBResourceWaitSpinlock));
	INIT_LIST_HEAD(&(pVcb->MetaCtx.VCBResourceWaitList));
	
	init_waitqueue_head(&(pVcb->MetaCtx.VCBMetaEvent));
#if !LINUX_VERSION_25_ABOVE
	init_timer(&pVcb->MetaCtx.VCBMetaTimeOut);
	pVcb->MetaCtx.VCBMetaTimeOut.function = xixfs_metaevent_timeouthandler;
	pVcb->MetaCtx.VCBMetaTimeOut.data = (unsigned long)&(pVcb->MetaCtx);
#endif

	pVcb->MetaCtx.MetaThread = -1;
	pVcb->MetaCtx.VCBCtx = pVcb;

	/*
	 * Initialize the meta context
	 * Set Xixcore VCB to Meta context
	 * Set MetaLock to Xixcore Meta context
	 */
	xixcore_InitializeMetaContext(
		&pVcb->XixcoreVcb.MetaContext,
		&pVcb->XixcoreVcb,
		(PXIXCORE_SPINLOCK)&pVcb->MetaCtx.MetaLock
		);

	step = 2;
DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
	("xixfs_set_super STEP 2 .\n"));

	xixcore_GetSuperBlockInformation(&pVcb->XixcoreVcb, LotSize, (xc_sector_t)VolumeIndex);	

	if(!pVcb->XixcoreVcb.IsVolumeWriteProtected) {
		
		RC = xixcore_RegisterHost(&pVcb->XixcoreVcb);
		
		if( RC < 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) xixfs_register_host.\n", RC));
			goto error_out;				
		}
		xixcore_CleanUpAuxLotLockInfo(&pVcb->XixcoreVcb);
	}

	step = 3;
DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
	("xixfs_set_super STEP 3 .\n"));

	pVcb->mountOpts.debug_level_mask = m_opts.debug_level_mask;
	pVcb->mountOpts.debug_target_mask = m_opts.debug_target_mask;
	pVcb->mountOpts.enable_debug = m_opts.enable_debug;
	pVcb->mountOpts.iocharset = m_opts.iocharset;

	
	
	spin_lock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	list_add(&(pVcb->VCBLink), &(xixfs_linux_global.sb_list));
	spin_unlock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));


	if( !pVcb->XixcoreVcb.IsVolumeWriteProtected ) {

		pVcb->MetaCtx.MetaThread =  kernel_thread(
									xixfs_ResourceThreadFunction, 
									&(pVcb->MetaCtx), 
									CLONE_KERNEL
									);
		
		if(pVcb->MetaCtx.MetaThread  < 0 )
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) start MetaThread.\n", RC));
			
			goto error_out;
		}
		
	}

	step = 4;
DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
	("xixfs_set_super STEP 4 .\n"));

	//set VFS_VARIABLE
#if LINUX_VERSION_25_ABOVE
	sb->s_fs_info = (void *)pVcb;
#else
	sb->u.generic_sbp = (void *)pVcb;
#endif
	sb_set_blocksize(sb, PAGE_SIZE);

#if LINUX_VERSION_25_ABOVE
	strcpy(sb->s_id,  XIXFS_STRING);
#endif

	if(pVcb->XixcoreVcb.IsVolumeWriteProtected) {
		XIXCORE_SET_FLAGS(sb->s_flags, MS_RDONLY);
	}

	XIXCORE_SET_FLAGS(sb->s_flags, MS_ACTIVE);
	// can be fixed
	sb->s_maxbytes = (0xffffffff);

#if LINUX_VERSION_25_ABOVE	
	XIXCORE_SET_FLAGS(sb->s_flags, MS_POSIXACL);
#endif

	sb->s_op = &xixfs_sops;

	xixfs_inode_hash_init(sb);
	
	 pVcb->RootInode = new_inode(sb);
	if (!pVcb->RootInode)
		goto error_out;
	
	pVcb->RootInode->i_ino = XIXFS_ROOT_INO;
	pVcb->RootInode->i_version = 1;

	step = 5;
DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
	("xixfs_set_super STEP 5 .\n"));

	RC = xixfs_fill_inode(sb, pVcb->RootInode,  1, pVcb->XixcoreVcb.RootDirectoryLotIndex);
	if (RC) {
		iput(pVcb->RootInode);
		pVcb->RootInode =NULL;
		goto error_out;
	}
#if LINUX_VERSION_25_ABOVE		
	xixfs_inode_attach(pVcb->RootInode);
#endif
	insert_inode_hash(pVcb->RootInode);

	sb->s_root = d_alloc_root(pVcb->RootInode);
	if (!sb->s_root) {
		printk(KERN_ERR "XIXFS: get root inode failed\n");
		goto error_out;
	}

	step = 6;
DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
	("xixfs_set_super STEP 6 .\n"));
	
	
#if LINUX_VERSION_25_ABOVE	
			return 0;
#else
			return sb;
#endif	
error_out:

DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
	("xixfs_set_super STEP error_out .\n"));

	if(step >= 5) {

		if(pVcb->RootInode ) {
			iput(pVcb->RootInode);
		}

	}


	if(step >= 4) {

		if(pVcb->XixcoreVcb.IsVolumeWriteProtected != 1) {
		
			if(pVcb->MetaCtx.MetaThread >= 0) {

#if !LINUX_VERSION_25_ABOVE
				XIXFS_WAIT_CTX wait;
#endif
      				spin_lock(&(pVcb->MetaCtx.MetaLock));
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(pCtx->MetaLock)) pCtx(%p)\n", &(pVcb->MetaCtx) ));
				
				XIXCORE_SET_FLAGS(pVcb->XixcoreVcb.MetaContext.VCBMetaFlags, XIXCORE_META_FLAGS_KILL_THREAD);
				spin_unlock(&(pVcb->MetaCtx.MetaLock));
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(pCtx->MetaLock)) pCtx(%p)\n", &(pVcb->MetaCtx) ));	

#if !LINUX_VERSION_25_ABOVE
				xixfs_init_wait_ctx(&wait);
				xixfs_add_metaThread_stop_wait_list(&wait, &(pVcb->MetaCtx));	
#endif
				
				wake_up(&(pVcb->MetaCtx.VCBMetaEvent));
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
					("META THREAD KILL REQ\n"));
#if LINUX_VERSION_25_ABOVE					
				wait_for_completion_timeout(&(pVcb->MetaCtx.VCBMetaThreadStopCompletion), DEFAULT_XIXFS_UMOUNTWAIT);		
				INIT_COMPLETION(pVcb->MetaCtx.VCBMetaThreadStopCompletion);
#else
				wait_for_completion(&(wait.WaitCompletion));
#endif
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
					("END META THREAD KILL REQ\n"));

				
			}
		}
	}


	if(step >= 3) {
		spin_lock(&(xixfs_linux_global.sb_list_lock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));
		
		list_del(&(pVcb->VCBLink));
		spin_unlock(&(xixfs_linux_global.sb_list_lock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
		
		if(pVcb->XixcoreVcb.IsVolumeWriteProtected != 1) {
			xixcore_DeregisterHost(&pVcb->XixcoreVcb);
		}
	}


		
	if(step >= 2) {
		if(pVcb) {
			kfree(pVcb);
		}
	}


	if(step >= 1) {
		if(name_tlb) {
			unload_nls(name_tlb);
		}
	}

#if LINUX_VERSION_25_ABOVE	
			return -1;
#else
			return NULL;
#endif
}

#if LINUX_VERSION_25_ABOVE

#if LINUX_VERSION_NEW_FILE_SYSTEM_TYPE
int
#else
struct super_block * 
#endif
xixfs_read_super (
	struct file_system_type *fs_type,
	int 					flags, 
	const char 			*dev_name,
#if LINUX_VERSION_NEW_FILE_SYSTEM_TYPE
	void					*data,
	struct vfsmount 		*mnt		
#else
	void 				*data
#endif
	)
{
#if LINUX_VERSION_NEW_FILE_SYSTEM_TYPE
	return get_sb_bdev(fs_type, flags, dev_name, data, xixfs_set_super, mnt);
#else
	return get_sb_bdev(fs_type, flags, dev_name, data, xixfs_set_super);
#endif
}



void 
xixfs_kill_block_super(struct super_block *sb)
{

	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
		("Enter xixfs_kill_block_super .\n"));


	kill_block_super(sb);
}



static struct file_system_type xix_fs_type = {
	.owner		= THIS_MODULE,
	.name		= "XixFs",
	.get_sb		= xixfs_read_super,
	.kill_sb	= xixfs_kill_block_super,
	.fs_flags	= FS_REQUIRES_DEV,
};
#else
static DECLARE_FSTYPE_DEV(xix_fs_type, "XixFs", xixfs_set_super);
#endif

#define REG_PATH_STRING	"/etc/xixfs.conf"


int initialize_xixfs(void)
{
	uint8				uuid[16];
	struct file *			tmpfile ;
	mm_segment_t		oldfs;
	loff_t 				pos = 0;
	int					RC;


	printk(KERN_ALERT  "enter xixfs initialize\n");
	


#ifdef XIXCORE_DEBUG
	XixcoreDebugLevel = DEBUG_LEVEL_ERROR;
	XixcoreDebugTarget = DEBUG_TARGET_ALL; //DEBUG_TARGET_THREAD;// 0;//DEBUG_TARGET_TEST;
#endif

	// check on disk data structure compatibility
	XIXCORE_ASSERT(sizeof(XIDISK_VOLUME_INFO) == 4096);
	XIXCORE_ASSERT(sizeof(XIDISK_LOT_MAP_INFO) == 4096);
	XIXCORE_ASSERT(sizeof(XIDISK_HOST_RECORD) == 4096);
	XIXCORE_ASSERT(sizeof(XIDISK_HOST_INFO) == 4096);
	XIXCORE_ASSERT(sizeof(XIDISK_LOCK) == 512);
	XIXCORE_ASSERT(sizeof(XIDISK_LOT_INFO) == 512);
	XIXCORE_ASSERT(sizeof(XIDISK_ADDR_MAP) == 131072);
	XIXCORE_ASSERT(sizeof(XIDISK_DIR_INFO) == 4096);
	XIXCORE_ASSERT(sizeof(XIDISK_CHILD_INFORMATION) == 4096);
	XIXCORE_ASSERT(sizeof(XIDISK_FILE_INFO) == 4096);
	XIXCORE_ASSERT(sizeof(XIDISK_COMMON_LOT_HEADER) == 4096);
	XIXCORE_ASSERT(sizeof(XIDISK_DIR_HEADER_LOT) == 16384);
	XIXCORE_ASSERT(sizeof(XIDISK_DIR_HEADER) == 147456);
	XIXCORE_ASSERT(sizeof(XIDISK_FILE_HEADER_LOT) == 16384);
	XIXCORE_ASSERT(sizeof(XIDISK_FILE_HEADER) == 147456);
	XIXCORE_ASSERT(sizeof(XIDISK_DATA_LOT) == 8192);
	XIXCORE_ASSERT(sizeof(XIDISK_VOLUME_LOT) == 16384);
	XIXCORE_ASSERT(sizeof(XIDISK_HOST_REG_LOT) == 16384);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
		("initialize_xixfs STEP 1 .\n"));

	// 1. Check uuid of host
	tmpfile = filp_open(REG_PATH_STRING, O_RDONLY, S_IFREG);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
		("initialize_xixfs STEP 1-1 .\n"));
	
	if(IS_ERR(tmpfile)) {

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_INIT, ("Can't open %s RegFile try Create \n", REG_PATH_STRING));

		tmpfile = filp_open(REG_PATH_STRING, O_CREAT, (S_IFREG|S_IRWXUGO));

		if ( !tmpfile ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_INIT, ("Can't open %s RegFile Fail return! \n", REG_PATH_STRING));
			return -EINVAL;
		}

		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
			("initialize_xixfs STEP 1-2 .\n"));

		memset(uuid, 0, 16);

		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
			("initialize_xixfs STEP 1-3 .\n"));

		generate_random_uuid(uuid);

		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
			("initialize_xixfs STEP 1-4 .\n"));

		oldfs = get_fs();

		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
			("initialize_xixfs STEP 1-4 .\n"));

		set_fs(KERNEL_DS);

		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
			("initialize_xixfs STEP 1-5 .\n"));

		if( tmpfile->f_op && tmpfile->f_op->llseek) {
			if( tmpfile->f_op->llseek(tmpfile, 0, 0) <  0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_INIT,("Can't pose %s RegFile Fail return! \n", REG_PATH_STRING));
				set_fs(oldfs);
				filp_close(tmpfile, NULL);
				return -EINVAL;
			}
		} else {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_INIT, ("f_op:%x or llseek() not available.\n", tmpfile->f_op));
		}


		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
			("initialize_xixfs STEP 2 .\n"));

		if( tmpfile->f_op && tmpfile->f_op->write) {
			if( tmpfile->f_op->write(tmpfile, uuid, 16, &pos) != 16 ) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_INIT,  ("Can't write %s RegFile Fail return! \n", REG_PATH_STRING));
				set_fs(oldfs);
				filp_close(tmpfile, NULL);
				return -EINVAL;
			}
		} else {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_INIT, ("f_op:%x or write() not available.\n", tmpfile->f_op));
		}

		set_fs(oldfs);
		filp_close(tmpfile, NULL);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
		("initialize_xixfs STEP 3 .\n"));

	} else {

		memset(uuid, 0, 16);
		oldfs = get_fs();
		set_fs(KERNEL_DS);

		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
			("initialize_xixfs STEP 3-1 .\n"));

		if( tmpfile->f_op &&  tmpfile->f_op->llseek ) {
			if( tmpfile->f_op->llseek(tmpfile, 0, 0) <  0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_INIT, ("Can't pose %s RegFile Fail return! \n", REG_PATH_STRING));
				set_fs(oldfs);
				filp_close(tmpfile, NULL);
				return -EINVAL;
			}
		} else {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_INIT, ("f_op:%x or llseek() not available.\n", tmpfile->f_op));
		}
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
			("initialize_xixfs STEP 3-2.\n"));

		if( tmpfile->f_op &&  tmpfile->f_op->read ) {
			if( tmpfile->f_op->read(tmpfile, uuid, 16, &pos) != 16 ) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_INIT, ("Can't read %s RegFile Fail return! \n", REG_PATH_STRING));
				set_fs(oldfs);
				filp_close(tmpfile, NULL);
				return -EINVAL;
			}
		} else {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_INIT, ("f_op:%x or read() not available.\n", tmpfile->f_op));
			set_fs(oldfs);
			filp_close(tmpfile, NULL);
			return -EINVAL;
		}
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
			("initialize_xixfs STEP 4 .\n"));
	}


	if( init_xixfs_inodecache() < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Can't make inode cache! \n"));
		return -EINVAL;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
		("initialize_xixfs STEP 5 .\n"));

	memset((void *)&xixfs_linux_global, 0, sizeof(XIXFS_LINUX_DATA));

	spin_lock_init(&(xixfs_linux_global.sb_list_lock));
	INIT_LIST_HEAD(&(xixfs_linux_global.sb_list));
	sema_init(&(xixfs_linux_global.sb_datalock), 1);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
		("initialize_xixfs STEP 5-1 .\n"));
	/*
	 * Initialize xixcore global data
	 */
	RC = xixcore_IntializeGlobalData(uuid, (PXIXCORE_SPINLOCK)&xixfs_linux_global.xixcore_tmp_lot_lock_list_lock);
	if(RC < 0)
		return RC;

//	Xixfs linux global not yet initialized.
//	xixfs_linux_global.IsInitialized = 1;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
		("initialize_xixfs STEP 5-3 .\n"));

	/*
	// 2. make thread for event processing.
	xixfs_linux_global.EventThreadId = kernel_thread(EventProcess, (void *)&xixfs_global, 0);
	*/

	if( xixfs_xbuf_setup() < 0 ) return -1;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_VCB|DEBUG_TARGET_VFSAPIT), 
		("initialize_xixfs STEP 6 .\n"));

#if LINUX_VERSION_WORK_QUEUE
	xixfs_workqueue = create_workqueue("xixfsWorkQue");
	if (!xixfs_workqueue) {
		return -1;
	}
#endif

	return register_filesystem(&xix_fs_type);	
}


int cleanup_xixfs(void)
{
#if LINUX_VERSION_25_ABOVE		
	int					TimeOut = 0;
#endif
	int					RC = 0;

	printk( KERN_DEBUG "enter xixfs Uninitialize\n");
	
	spin_lock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	if( !list_empty(&(xixfs_linux_global.sb_list)) ) {
		spin_unlock(&(xixfs_linux_global.sb_list_lock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			(" FAIL Cleanup_xixfs  : VCB is used by another !!!!\n") );
		return -1;
	}
	spin_unlock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));

	down(&(xixfs_linux_global.sb_datalock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("down(&(xixfs_linux_global.sb_datalock)) \n" ));

	if(xixfs_linux_global.IsInitialized) {

		spin_lock(&(xixfs_linux_global.EventCtx.Eventlock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.EventCtx.Eventlock)) \n" ));
		
		XIXCORE_SET_FLAGS(xixfs_linux_global.EventCtx.SrvEventFlags,  XIXFS_EVENT_SRV_SHUTDOWN);
		spin_unlock(&(xixfs_linux_global.EventCtx.Eventlock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.EventCtx.Eventlock)) \n" ));

		wake_up(&(xixfs_linux_global.EventCtx.SrvEvent));

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
					("EVNET SRV THREAD KILL 1 REQ\n"));
		
#if LINUX_VERSION_25_ABOVE		
		TimeOut = DEFAULT_XIXFS_UMOUNTWAIT;	
		RC = wait_for_completion_timeout(&(xixfs_linux_global.EventCtx.SrvCompletion), TimeOut);
#else
		wait_for_completion(&(xixfs_linux_global.EventCtx.SrvCompletion));
		RC = 1;
#endif
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
					("END EVENET SRV THREAD KILL 1 REQ\n"));
		
		INIT_COMPLETION(xixfs_linux_global.EventCtx.SrvCompletion);

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				(" EVENT SERVER Killed !!!!\n") );
		
		if(RC == 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				(" EVENT SERVER Kill Wait Fail TimeOut !!!!\n") );
		}

		spin_lock(&(xixfs_linux_global.EventCtx.Eventlock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.EventCtx.Eventlock)) \n" ));

		XIXCORE_SET_FLAGS(xixfs_linux_global.EventCtx.CliEventFlags,  XIXFS_EVENT_CLI_SHUTDOWN);
		spin_unlock(&(xixfs_linux_global.EventCtx.Eventlock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.EventCtx.Eventlock)) \n" ));

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				(" EVENT Client Kill wakeup !!!!\n") );
	

		wake_up(&(xixfs_linux_global.EventCtx.CliEvent));

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
					("EVENET CLI THREAD KILL 1 REQ\n"));

#if LINUX_VERSION_25_ABOVE		
		TimeOut = DEFAULT_XIXFS_UMOUNTWAIT;	
		RC = wait_for_completion_timeout(&(xixfs_linux_global.EventCtx.CliCompletion), TimeOut);
#else
		wait_for_completion(&(xixfs_linux_global.EventCtx.CliCompletion));
		RC = 1;
#endif
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
					("END EVENET CLI THREAD KILL 1 REQ\n"));
		
		INIT_COMPLETION(xixfs_linux_global.EventCtx.CliCompletion);

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				(" EVENT Client Kill ed !!!!\n") );
		
		if(RC == 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				(" EVENT CLI Kill Wait Fail TimeOut !!!!\n") );
		}

	}
	up(&(xixfs_linux_global.sb_datalock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("up(&(xixfs_linux_global.sb_datalock)) \n" ));

#if LINUX_VERSION_WORK_QUEUE
	destroy_workqueue(xixfs_workqueue);
#endif
	destroy_xixfs_inodecache();
	xixfs_xbuf_destroy();

	xixcore_FreeUpcaseTable(xixcore_global.xixcore_case_table);

	unregister_filesystem(&xix_fs_type);
	return 0;
}

/*
 *
 */
int 
xixfs_ScanForPreMounted(
		struct super_block *sb
		)
{

	struct list_head * plist = NULL;
	PXIXFS_LINUX_VCB		pVcb = NULL;
	
	plist = &(xixfs_linux_global.sb_list);
	spin_lock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	while(plist != &(xixfs_linux_global.sb_list)){
		pVcb = container_of(plist, XIXFS_LINUX_VCB, VCBLink);

		if( ( pVcb->VCBState == XIXFS_VCB_STATUS_VOL_MOUNTED )
			&& (pVcb->supreblockCtx == sb) )
		{
			spin_unlock(&(xixfs_linux_global.sb_list_lock));
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
			return -1;
		}

		plist = plist->next;
	}
	spin_unlock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	return 0;
	
}
