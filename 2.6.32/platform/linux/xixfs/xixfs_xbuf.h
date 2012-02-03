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
#ifndef __XIXFS_XBUF_H__
#define __XIXFS_XBUF_H__

#include <linux/types.h>
#include <linux/fs.h>
#include <linux/linkage.h>
#include <linux/pagemap.h>
#include <linux/wait.h>
#include <asm/atomic.h>

#include "xixfs_types.h"

struct _xix_buf;

typedef void (XIX_BUF_END_IO_T)(struct _xix_buf * xbuf, int uptodata);

#define XIX_BUF_FLAGS_FINE	0
#define XIX_BUF_FLAGS_ERROR 1
#define XIX_BUF_FLAGS_OP	2

typedef struct _xix_buf 
{
	XIXCORE_BUFFER		xixcore_buffer;
	struct page			*xix_page;
	long				xix_flags;
#if LINUX_VERSION_25_ABOVE
	XIX_BUF_END_IO_T	*xix_end_io;
#endif

}XIX_BUF, *PXIX_BUF;

#if  LINUX_VERSION_KMEM_CACHE_T_DEPRECATED
extern struct kmem_cache *xbuf_cachep;
#else
extern kmem_cache_t *xbuf_cachep;
#endif

/*
 * Inititalize buffer system.
 */

int xixfs_xbuf_setup(void);

/*
 * Destroy the buffer system.
 */

void xixfs_xbuf_destroy(void);

#if LINUX_VERSION_25_ABOVE

int xbuf_wait(void *word);

void wait_on_xbuf(PXIX_BUF xbuf);

void lock_xbuf(PXIX_BUF xbuf);

void fastcall unlock_xbuf(PXIX_BUF xbuf);

void 
xbuf_end_io(
	PXIX_BUF xbuf, 
	int32 uptodate
	);

#endif

#endif //#ifndef __XIXFS_XBUF_H__
