
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
#ifndef __XIXFS_NAME_CONV_H__
#define __XIXFS_NAME_CONV_H__

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/linkage.h>
#include <linux/pagemap.h>
#include <linux/wait.h>
#include <asm/atomic.h>
#include <linux/types.h>

#include "xixfs_types.h"
#include "xixfs_drv.h"

void *
xixfs_large_malloc(unsigned long size, gfp_t gfp_mask);

void
xixfs_large_free(void *addr);


__le16 * 
xixfs_generate_default_upcase(void);

void 
xixfs_conversioned_name_free(__le16 * name);

int 
xixfs_fs_to_uni_str(
	const PXIXFS_LINUX_VCB pVCB, 
	const char *ins,
	const int ins_len, 
	__le16 **outs
	);


int 
xixfs_uni_to_fs_str(
	const PXIXFS_LINUX_VCB pVCB, 
	const __le16 *ins,
	const int ins_len, 
	unsigned char **outs, 
	int outs_len
	);



#endif //#ifndef __XIXFS_NAME_CONV_H__
