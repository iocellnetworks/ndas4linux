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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/
 
#ifndef _NDAS_BDEV_H_
#define _NDAS_BDEV_H_

#include <linux/uio.h>
#include <linux/blkdev.h>

#include <ndas_dev.h>

extern struct block_device_operations ndas_fops;

int  bdev_init(void);
int  bdev_cleanup(void);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
void bdev_prepare_flush(struct request_queue *q, struct request *req);
#endif

void bdev_request_proc(struct request_queue *q);

#endif // _NDAS_BDEV_H_
