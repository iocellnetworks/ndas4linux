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
#ifndef __XIXFS_NDASCTL_H__
#define __XIXFS_NDASCTL_H__

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <xixfsevent/xixfs_event.h>
#include "xixcore/buffer.h"
#include "xixfs_types.h"
#include "xixfs_xbuf.h"




int
xixfs_ndas_closeEvent(
	PXIXFS_EVENT_CTX	pEventCtx
);

int
xixfs_ndas_createEvent(
	PXIXFS_EVENT_CTX	pEventCtx
);



int
xixfs_ndas_sendEvent(
	PXIXFS_EVENT_CTX	pEventCtx
);





int 
xixfs_checkNdas(
	struct super_block *sb,
	int				*IsReadOnly
	);

#if !LINUX_VERSION_25_ABOVE
#include "ndasdev.h"
int 
xixfs_getPartiton(
	struct super_block *sb,
	pndas_xixfs_partinfo	 partinfo
	);



int
xixfs_direct_block_ios(
	struct block_device *bdev, 
	sector_t startsector, 
	int32 size, 
	PXIX_BUF xbuf, 
	int32 rw
);
#endif


#endif // #ifndef __XIXFS_NDASCTL_H__

