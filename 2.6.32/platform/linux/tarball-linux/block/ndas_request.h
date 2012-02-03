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

#ifndef _NDAS_REQUEST_H_
#define _NDAS_REQUEST_H_

#include <linux/uio.h>
#include <linux/uio.h>
#include <linux/blkdev.h>
#include <linux/interrupt.h>	/* for in_interrupt() */


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))

#define NDAS_MAX_REQUEST_SECTORS	BLK_DEF_MAX_SECTORS 	// ndascore can handle large request size

#else

#define NDAS_MAX_REQUEST_SECTORS	MAX_SECTORS 	// ndascore can handle large request size

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

#define NDAS_MAX_PHYS_SEGMENTS 		MAX_PHYS_SEGMENTS
#define NDAS_MAX_HW_SEGMENTS   		MAX_HW_SEGMENTS

#else

#define NDAS_MAX_PHYS_SEGMENTS 		MAX_SEGMENTS
#define NDAS_MAX_HW_SEGMENTS   		MAX_SEGMENTS

#endif

#define BIG_BLK_BUF_SEC_LEN 	16										// 8 KByte
#define BIG_MAX_BUFFER_ALLOC 	(16 * 1024 * 2 / BIG_BLK_BUF_SEC_LEN)	// 32 MByte

#define SMALL_BLK_BUF_SEC_LEN 	1
#define SMALL_MAX_BUFFER_ALLOC 	(1 * 1024 * 2 / SMALL_BLK_BUF_SEC_LEN)	// 1 MByte

#define NDAS_WRITE_BACK_BUF_LEN	(NDAS_MAX_REQUEST_SECTORS / BIG_BLK_BUF_SEC_LEN)

C_ASSERT( 300, NDAS_WRITE_BACK_BUF_LEN <= NDAS_MAX_PHYS_SEGMENTS );

#define NDAS_CMD_READ  		0
#define NDAS_CMD_WRITE      1
#define NDAS_CMD_FLUSH      2

#define NDAS_CMD_STOP		17
#define NDAS_CMD_LOCKOP     18

typedef enum _NDAS_LOCK_OP {

    NDAS_LOCK_TAKE = 1,		// Take device lock
    NDAS_LOCK_GIVE,        	// Give device lock
    NDAS_LOCK_GET_COUNT,    // Get the number that the lock is freed. Used for statistics
    NDAS_LOCK_GET_OWNER,    // Get the current lock owner or last owner.
    NDAS_LOCK_BREAK,    	// If lock is taken by other, try to break the lock and take it.     

} NDAS_LOCK_OP;


struct udev_s;

typedef struct ndas_request {

#ifdef DEBUG
   	int				magic;
#endif

	bool 			urgent;
	unsigned long	jiffies;


	struct list_head	queue;

   	unsigned char		cmd;		// NDAS_CMD_xxx 
    struct request  	*req;

	union {

		int		flags;			// NDAS_CMD_CONNECT

		struct {

			sector_t 	  sector;		// next sector to submit
			unsigned long nr_sectors;	// no. of sectors left to submit
		} s;

		struct {				// NDAS_CMD_LOCK

			int 			lock_id;
			NDAS_LOCK_OP	lock_op;
		} l;			
	} u;

	struct page		*private[NDAS_MAX_PHYS_SEGMENTS];

    struct iovec	req_iov[NDAS_MAX_PHYS_SEGMENTS];
    size_t		 	req_iov_num;

	unsigned char	*buf[NDAS_WRITE_BACK_BUF_LEN];

#define	MAX_DONE_STACK	4

	int 			slot;
 
	int				next_done;
    void 			(*done[MAX_DONE_STACK])( struct ndas_request *ndas_req, void *user_arg );
    void			*done_arg[MAX_DONE_STACK];

	ndas_error_t	err;
	bool			suspend_queue;

	struct completion	completion;

} ndas_request_t;

#ifdef DEBUG
#define NDAS_REQ_MAGIC 0x0621222f
#define NDAS_REQ_ASSERT(ndas_req) do { NDAS_BUG_ON((ndas_req)->magic != NDAS_REQ_MAGIC); } while(0)
#else
#define NDAS_REQ_ASSERT(ndas_req) do {} while(0)
#endif


extern struct kmem_cache  *ndas_req_kmem_cache;

#include <asm/hardirq.h>

static inline 
ndas_request_t *ndas_request_alloc(void) {

	NDAS_BUG_ON( ndas_req_kmem_cache == NULL );

    if (in_interrupt()) {

		return kmem_cache_alloc( ndas_req_kmem_cache, GFP_ATOMIC );
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

	if (in_atomic() || irqs_disabled()) {

		return kmem_cache_alloc( ndas_req_kmem_cache, GFP_ATOMIC );
	}
#endif

	return kmem_cache_alloc( ndas_req_kmem_cache, GFP_KERNEL) ;
}

static inline
void ndas_request_free(ndas_request_t *ndas_req)
{
	kmem_cache_free( ndas_req_kmem_cache ,ndas_req );
}

static inline 
void *ndas_kmalloc(unsigned int size)
{
    if (in_interrupt()) {

		return kmalloc( size, GFP_ATOMIC );
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

	if (in_atomic() || irqs_disabled()) {

		return kmalloc( size, GFP_ATOMIC );
	}

#endif

	return kmalloc( size, GFP_KERNEL );
}

static inline 
void ndas_kfree(void *ptr)
{
    kfree(ptr);
}

#endif // _NDAS_REQUEST_H_
