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

/*
 * Data stuctures and Functions to provide the NDAS block device in linux
 * 
 */
 
#ifndef _LINUX_BLOCK_H_
#define _LINUX_BLOCK_H_

#include <sal/mem.h>
#include <ndasuser/info.h> // ndas_slot_info_t
#include "linux_ver.h"

#ifdef DEBUG
#include <sal/debug.h>
#if !defined(DEBUG_LEVEL_BLOCK) 
    #if defined(DEBUG_LEVEL)
    #define DEBUG_LEVEL_BLOCK DEBUG_LEVEL
    #else
    #define DEBUG_LEVEL_BLOCK 1
    #endif
#endif
#define dbgl_blk(l,x...) do { \
    if ( l <= DEBUG_LEVEL_BLOCK ) {\
        sal_debug_print("BL|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x); \
    } \
} while(0) 
#else
#define dbgl_blk(l,fmt...) do {} while(0)
#endif

#define DRIVER_MASK        0xf0
#ifndef PARTN_BITS 	// 2.6.27 defines this already
	#define PARTN_BITS        4
#endif

#define NR_PARTITION    (1U << PARTN_BITS)    
#define PARTN_MASK        (NR_PARTITION -1)

#define DEFAULT_ND_BLKSIZE    1024
#define DEFAULT_ND_DEVSIZE    1024 /* This size will be override after partition read? */
#define DEFAULT_ND_HARDSECTSIZE 512

/*
  NOTE: Linux 2.6.16 or later does not allow larger than 1024 sector request length.
  Refer to  blk_queue_max_sectors() in ll_rw_blk.c
 */
#define DEFAULT_ND_MAX_SECTORS    2048 /* ndascore can handle large request size */

#include "ndasdev.h"

#include <linux/blkdev.h>

#define SLOT_D(_disk_) SLOT((_disk_))
#define SLOT_B(_bdev_) SLOT((_bdev_)->bd_disk)

#define SLOT_R(_request_) ((int) _request_->rq_disk->private_data)
#define SLOT(_gendisk_) \
({ \
    struct gendisk *__g__ = _gendisk_;\
    (((__g__->first_minor) >> PARTN_BITS) + NDAS_FIRST_SLOT_NR);\
})

/* 
 * Split the sectors into the given chunks for raid 0/4
 *
 * eg: 4 chunks(disks) from 128 sectors
 *    0-> 0   1->32   2->64   3->96
 *    4-> 1   5->33   6->65   7->97
 *   ...
 *  120->30 121->62 122->94 123->126
 *  124->31 125->63 126->95 127->127
 */
#define SPLIT_IDX(i, size, splits) \
    ( ((i) % (splits)) * ( (size) / (splits) ) + ((i) / (splits)))
    
#define SPLIT_IDX_SLOT(i, sz, slot) \
    SPLIT_IDX(i, \
        sz, \
        NDAS_GET_SLOT_DEV(slot)->info.io_splits)

/*
 * Get the byte size of the struct nbio_linux with the given number of blocks
 */
#define NBIO_SIZE(nr_blocks) ( \
    sizeof(struct nbio_linux) + \
    (nr_blocks) * sizeof(struct sal_mem_block))

/*
 * Decide if the nbio blocks and the given data address is adjacent to merge it
 */    
#define NBIO_ADJACENT(nbio, data) ( \
    (nbio)->nr_blocks > 0 && \
    (nbio)->blocks[(nbio)->nr_blocks - 1].ptr + \
    (nbio)->blocks[(nbio)->nr_blocks - 1].len == (data) )

#define NDAS_FLAG_QUEUE_SUSPENDED  (1<<0)

/*
 * The data structure to maintain the information about the slot
 */
struct ndas_slot {
    int         enabled;     /* 1 if enabled */
    ndas_slot_info_t info;
    ndas_device_info dinfo;
    int            slot;
    struct semaphore mutex;
    ndas_error_t         enable_result;
    char devname[32];
    spinlock_t lock;
    struct gendisk *disk;
    /* 
      pointer to disk->queue, but will be deallocated on unregistration
     */
    struct request_queue *queue;
    unsigned long        queue_flags;   /* NDAS_FLAG_xxx */

    struct proc_dir_entry *proc_dir;
    struct proc_dir_entry *proc_info;
    struct proc_dir_entry *proc_info2;
    struct proc_dir_entry *proc_unit;
    struct proc_dir_entry *proc_timeout;
    struct proc_dir_entry *proc_enabled;
    struct proc_dir_entry *proc_ndas_serial;
    struct proc_dir_entry *proc_devname;
    struct proc_dir_entry *proc_sectors;    
    struct list_head proc_node;
};

/*
 * Structure to gather the scattered memory blocks.
 * NDAS driver is platform neutral driver, 
 * so we use the platform neutral data structure - sal_mem_block.
 */
struct nbio_linux {
    ndas_io_request			ndas_req;
    struct request			*req;
    size_t						nr_sectors;
    int							nr_blocks;
    struct sal_mem_block	blocks[0];
};


// Just arbitary number enough to hold request segments
#ifdef CONFIG_HIGHMEM
	// Need more memblocks for highmemory support.
	// TODO: reduce memblocks for highmemory support.
	#define ND_BLK_MAX_REQ_SEGMENT 128
#else
	#define ND_BLK_MAX_REQ_SEGMENT 64
#endif

#define MAX_NBIO_SIZE NBIO_SIZE(ND_BLK_MAX_REQ_SEGMENT)

/*
 * Cached memory pool to be used for nbio 
 */
extern KMEM_CACHE *nblk_kmem_cache;

inline static void* nblk_mem_alloc(void) 
{
    return kmem_cache_alloc(nblk_kmem_cache, GFP_NOIO);
}
inline static void nblk_mem_free(void *object)
{
    kmem_cache_free(nblk_kmem_cache, object);
}

NDAS_CALL
void nblk_end_io(int slot, ndas_error_t err, struct nbio_linux* nio);

ndas_error_t nblk_handle_io(struct request *req);

#endif // _LINUX_BLOCK_H_
