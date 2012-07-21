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
#ifndef __XIXFS_FILE_H__
#define __XIXFS_FILE_H__

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <xixfsevent/xixfs_event.h>
#include "xixfs_types.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"


int 
xixfs_sync_file(
		struct file *file, 
		struct dentry *dentry, 
		int datasync
);



int 
xixfs_sync_inode(struct inode *inode);

extern struct file_operations xixfs_file_operations;
extern struct inode_operations xixfs_file_inode_operations;

#endif //#ifndef __XIXFS_FILE_H__
