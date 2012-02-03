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
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h>
#include <asm/page.h>

#include "xixcore/xctypes.h"
#include "xixcore/buffer.h"

#include "xixfs_name_cov.h"
#include "xixfs_xbuf.h"

/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSXBUF"

#if  LINUX_VERSION_KMEM_CACHE_T_DEPRECATED
struct kmem_cache *xbuf_cachep;
#else
kmem_cache_t *xbuf_cachep;
#endif

int xixfs_xbuf_setup(void)
{
#if LINUX_VERSION_25_ABOVE
	xbuf_cachep = kmem_cache_create("xbufHead",
			sizeof(XIX_BUF), 0, SLAB_HWCACHE_ALIGN|SLAB_PANIC,NULL,NULL);
#else
	xbuf_cachep = kmem_cache_create("xbufHead",
			sizeof(XIX_BUF), 0, SLAB_HWCACHE_ALIGN,NULL,NULL);
#endif
	if (xbuf_cachep == NULL)
		return -ENOMEM;

	return 0;
}


void xixfs_xbuf_destroy(void)
{
#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
	kmem_cache_destroy(xbuf_cachep);
#else
	if (kmem_cache_destroy(xbuf_cachep))
		printk(KERN_INFO "xbuf not all structures were freed\n");
#endif
}

#if LINUX_VERSION_25_ABOVE
int xbuf_wait(void *word)
{
	schedule();
	return 0;
}

void wait_on_xbuf(PXIX_BUF xbuf)
{
	might_sleep();
	wait_on_bit(&(xbuf->xix_flags), XIX_BUF_FLAGS_OP, xbuf_wait, TASK_UNINTERRUPTIBLE);
}


void lock_xbuf(PXIX_BUF xbuf)
{
	set_bit(XIX_BUF_FLAGS_OP, &(xbuf->xix_flags));
}

void fastcall unlock_xbuf(PXIX_BUF xbuf)
{
	clear_bit(XIX_BUF_FLAGS_OP, &(xbuf->xix_flags));
	smp_mb__after_clear_bit();
	wake_up_bit(&xbuf->xix_flags, XIX_BUF_FLAGS_OP);
}

void 
xbuf_end_io(
	PXIX_BUF xbuf, 
	int32 uptodate
	)
{
	if(uptodate){
		set_bit(XIX_BUF_FLAGS_FINE, &xbuf->xix_flags);
	}else{
		set_bit(XIX_BUF_FLAGS_ERROR, &xbuf->xix_flags);
	}

	unlock_xbuf(xbuf);
}

#endif
