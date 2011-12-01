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
#else
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
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

#include "xixfs_global.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"
#include "xixfs_name_cov.h"
#include "xixfs_ndasctl.h"
#include "xixfs_event_op.h"

/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSDENTRY"

static inline 
unsigned long
xixfs_partial_name_hash(
	unsigned long c, 
	unsigned long prevhash
)
{
	return (prevhash + (c << 4) + (c >> 4)) * 11;
}

static 
inline 
unsigned int
xixfs_name_hash(const unsigned char *name, unsigned int len)
{
	unsigned long hash = 0;
	while (len--)
		hash = xixfs_partial_name_hash(*name++, hash);
	return hash;
}


static 
int 
xixfs_hash(struct dentry *dentry, struct qstr *qstr)
{

	PXIXFS_LINUX_VCB	pVCB = NULL;
	uint8			*Name = NULL;
	int32			NameLength = 0;
	int				RC = 0;

	XIXCORE_ASSERT(dentry->d_sb);
	pVCB = XIXFS_SB(dentry->d_sb);
	XIXFS_ASSERT_VCB(pVCB);


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_hash (%s)  dentry(%p) inode(%p).\n", dentry->d_name.name, dentry, dentry->d_inode));	

	NameLength = xixfs_fs_to_uni_str( pVCB,
									dentry->d_name.name,
									dentry->d_name.len,
									(__le16 **)&Name
									);



	if(NameLength < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_hash:xixfs_fs_to_uni_str \n", NameLength));
		return 0;
	}



	xixcore_UpcaseUcsName(
					(__le16 *)Name,
					(uint32)(NameLength),
					xixcore_global.xixcore_case_table, 
					xixcore_global.xixcore_case_table_length
					);



	RC = xixfs_name_hash((uint8 *)Name, (uint32)NameLength);

	qstr->hash = RC;

	if(Name) {
		xixfs_conversioned_name_free((__le16 *)Name);
	}


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("EXIT xixfs_hash value(%d) .\n", RC));	


	return 0;
	
}



static int 
xixfs_cmp(struct dentry *dentry, struct qstr *a, struct qstr *b)
{


	PXIXFS_LINUX_VCB	pVCB = NULL;
	uint8			*Name = NULL;
	int32			NameLength = 0;
	uint8			*NewName = NULL;
	int32			NewNameLength = 0;
	int				RC = 0;

	XIXCORE_ASSERT(dentry->d_sb);
	pVCB = XIXFS_SB(dentry->d_sb);
	XIXFS_ASSERT_VCB(pVCB);

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("ENTER xixfs_cmp .\n"));	


	NameLength = xixfs_fs_to_uni_str( pVCB,
									a->name,
									a->len,
									(__le16 **)&Name
									);



	if(NameLength < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_hash:xixfs_fs_to_uni_str \n", NameLength));
		
		Name = NULL;
		RC = -ENOMEM;
		goto error_out;
	}


	NewNameLength = xixfs_fs_to_uni_str( pVCB,
									b->name,
									b->len,
									(__le16 **)&NewName
									);



	if(NewNameLength < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail(%x)  xixfs_hash:xixfs_fs_to_uni_str \n", NewNameLength));
		
		NewName = NULL;
		RC = -ENOMEM;
		goto error_out;
	}



	RC = xixcore_IsNamesEqual(
					(__le16 *)Name, 
					(uint32)(NameLength),
					(__le16 *)NewName, 
					(uint32)(NewNameLength),
					1,
					xixcore_global.xixcore_case_table, 
					xixcore_global.xixcore_case_table_length
					);

	if( RC == 1) {
		RC = 0;
		//printk(KERN_DEBUG "Same string string a %s : string b %s\n", a->name, b->name);
	}else {
		RC = -EINVAL;
	}


error_out:


	if(Name) {
		xixfs_conversioned_name_free((__le16 *)Name);
	}


	if(NewName) {
		xixfs_conversioned_name_free((__le16 *)NewName);
	}

	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("EXIT xixfs_cmp .\n"));	

	return RC;
}



struct dentry_operations xixfs_dentry_operations = {
	.d_hash		= xixfs_hash,
	.d_compare	= xixfs_cmp,
};

/*
static struct dentry_operations msdos_dentry_operations = {
	.d_hash		= msdos_hash,
	.d_compare	= msdos_cmp,
};
*/
