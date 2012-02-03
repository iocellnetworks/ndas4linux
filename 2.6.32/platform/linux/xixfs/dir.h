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
#ifndef __XIXFS_DIR_H__
#define __XIXFS_DIR_H__

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <xixfsevent/xixfs_event.h>
#include "xixfs_types.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"



#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,5)
#define parent_ino(d) ((d)->d_parent->d_inode->i_ino)
#endif

#define XIXFS_ROOT_INO		(0x20)
#define XIXFS_SELF			"."
#define XIXFS_PARENT			".."
#define XIXFS_PSUDO_OFF		(2)

extern struct inode_operations 	xixfs_dir_inode_operations;
extern struct file_operations 	xixfs_dir_operations;

#endif //#ifndef __XIXFS_DIR_H__

