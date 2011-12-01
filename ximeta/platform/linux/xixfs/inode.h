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
#ifndef __XIXFS_INODE_H__
#define __XIXFS_INODE_H__

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <xixfsevent/xixfs_event.h>
#include "xixfs_types.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"

#if  LINUX_VERSION_KMEM_CACHE_T_DEPRECATED
extern struct kmem_cache *xixfs_inode_cachep;;
#else
extern kmem_cache_t * xixfs_inode_cachep;
#endif





int init_xixfs_inodecache(void);

void destroy_xixfs_inodecache(void);

struct inode *xixfs_alloc_inode(struct super_block *sb);



void 
xixfs_inode_hash_init(
	struct super_block *sb
);

void 
xixfs_inode_detach(
	struct inode *inode
);


void 
xixfs_inode_attach(
		struct inode *inode
);

struct inode *
xixfs_iget(
	struct super_block *sb, 
	uint64 LotNumber
);


struct inode *
xixfs_build_inode(
		struct super_block *sb,
		uint32			IsDir,
		uint64 			LotNumber
);


int
xixfs_fill_inode ( 
		struct super_block *sb,
		struct inode *inode,
		uint32		IsDir,
		uint64		LotNumber
);

#if LINUX_VERSION_25_ABOVE
int 
#else
void
#endif
xixfs_write_inode(
	struct inode *inode, 
	int wait
);


int 
xixfs_get_block(
	struct inode *inode, 
	sector_t iblock, 
	struct buffer_head *bh_result, 
	int create
);


struct inode *
xixfs_alloc_inode(
	struct super_block *sb
);

void 
xixfs_destroy_inode(
		struct inode *inode
);


void 
xixfs_delete_inode(
	struct inode *inode
);


void 
xixfs_clear_inode(
	struct inode *inode
);

int
xixfs_update_fileInfo ( 
		struct super_block *sb,
		struct inode *inode,
		uint32		IsDir,
		uint64		LotNumber
);


extern struct address_space_operations xixfs_aops;



#endif //#ifndef __XIXFS_INODE_H__
