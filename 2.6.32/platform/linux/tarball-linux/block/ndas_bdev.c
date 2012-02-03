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

#include <linux/version.h>
#include <linux/module.h> 

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include <linux/hdreg.h>
#include <linux/loop.h>

#include <ndas_dev.h>

#ifdef DEBUG

int	dbg_level_bdev = DBG_LEVEL_BDEV;

#define dbg_call(l,x...) do { 								\
    if (l <= dbg_level_bdev) {								\
        printk("%s|%d|%d|",__FUNCTION__, __LINE__,l); 	\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif

#include <linux/mmzone.h>
#include <linux/swap.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h> 		// VERIFY_WRITE
#include <linux/blkpg.h> 		// BLKPG

#define MAJOR_NR 	NDAS_BLK_MAJOR
#define	DEVICE_NAME	"ndasdevice"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#include <linux/buffer_head.h> 	// file_fsync
#else

// Setup some definitions for linux/blk.h

#define MAJOR_NR NDAS_BLK_MAJOR
#include <linux/major.h>
#if( MAJOR_NR == NDAS_BLK_MAJOR)
#undef    DEVICE_NAME
#define    DEVICE_NAME    "ndasdevice"
#undef    DEVICE_REQUEST
#define DEVICE_REQUEST    bdev_request_proc
#define DEVICE_NR(device) (MINOR(device) >> NDAS_PARTN_BITS)
#undef    DEVICE_ON
#define DEVICE_ON(device)
#undef    DEVICE_OFF
#define DEVICE_OFF(device)
#endif

#include <linux/blk.h>
#endif

#include "ndas_request.h"

#include "ndas_bdev.h"
#include "ndas_sdev.h"


#define DEBUG_GENDISK(g) 											\
({ 																	\
    static char __buf__[1024];										\
    if ( g ) 														\
        snprintf(__buf__,sizeof(__buf__), 							\
            "%p={major=%d,first_minor=%d,minors=0x%x,disk_name=%s," \
            "private_data=%p,capacity=%lld}", 						\
            g,														\
            g->major, 												\
            g->first_minor,											\
            g->minors,												\
            g->disk_name, 											\
            g->private_data, 										\
            (long long)g->capacity 									\
        ); 															\
    else 															\
        snprintf(__buf__,sizeof(__buf__), "NULL"); 					\
    (const char*) __buf__; 											\
})


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

#define NBLK_LOCK(q) ((q)->queue_lock)
#define REQ_DIR(req) rq_data_dir(req)
#define NBLK_NEXT_REQUEST(q) (		\
    blk_queue_plugged(q) ? NULL : 	\
    elv_next_request(q))

#else

#define NBLK_LOCK(q) (&io_request_lock)
#define REQ_DIR(req) ((req)->cmd)
#define NBLK_NEXT_REQUEST(q) (				\
    list_empty(&(q)->queue_head) ? NULL : 	\
    blkdev_entry_next_request(&(q)->queue_head))

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
#define REQUEST_FLAGS(req) ((req)->cmd_flags)
#else
#define REQUEST_FLAGS(req) ((req)->flags)
#endif

static int bdev_open(struct inode *inode, struct file *filp);
static int bdev_release(struct inode *inode, struct file *filp);
static int bdev_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static int bdev_getgeo(struct block_device *bdev, struct hd_geometry *geo);
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) // 2.5.x or above
static int bdev_media_changed(struct gendisk *);
static int bdev_revalidate_disk(struct gendisk *);
#else
static int bdev_check_netdisk_change(kdev_t rdev);
static int bdev_revalidate(kdev_t rdev);
#endif

#define OPEN_WRITE_BIT 16
#define IDE_CMD_FLUSH_CACHE 0xe7

struct block_device_operations ndas_fops = {

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
    .owner 				= THIS_MODULE, 
#endif
    .open 				= bdev_open, 
    .release 			= bdev_release, 
    .ioctl				= bdev_ioctl, 
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
	.getgeo				= bdev_getgeo,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    .media_changed		= bdev_media_changed,
    .revalidate_disk	= bdev_revalidate_disk
#else
    .check_media_change = bdev_check_netdisk_change, 
    .revalidate			= bdev_revalidate
#endif
};

struct kmem_cache *ndas_req_kmem_cache;

static unsigned int request_num = 0;
static unsigned int acu_count = 0;
static unsigned long prev_jiff = 0;
static unsigned int small_acu_count = 0;
static unsigned int small_request = 0;
static unsigned int big_request = 0;

static unsigned int sec_acu_count = 0;
static unsigned long sec_prev_jiff = 0;
static unsigned int peak_mbps = 0;

static unsigned int sec2_acu_count = 0;
static unsigned long sec2_prev_jiff = 0;
static unsigned int peak2_mbps = 0;


#ifdef NDAS_WRITE_BACK

char 			*big_blk_buf[BIG_MAX_BUFFER_ALLOC];
bool 			big_blk_buf_used[BIG_MAX_BUFFER_ALLOC];
unsigned int	remained_big_blk_buf = 0;
unsigned int	big_buffer_alloc = 0;

char 			*small_blk_buf[SMALL_MAX_BUFFER_ALLOC];
bool 			small_blk_buf_used[SMALL_MAX_BUFFER_ALLOC];
unsigned int	remained_small_blk_buf = 0;

static char *blk_buf_alloc(bool big) {

	int i;

	if (big) {

		for (i = 0; i < BIG_MAX_BUFFER_ALLOC; i++) {

			if (big_blk_buf[i] && big_blk_buf_used[i] == false) {

				big_blk_buf_used[i] = true;
				remained_big_blk_buf--;
				return big_blk_buf[i];
			}
		}
	
	} else {

		for (i = 0; i < SMALL_MAX_BUFFER_ALLOC; i++) {

			if (small_blk_buf[i] && small_blk_buf_used[i] == false) {

				small_blk_buf_used[i] = true;
				remained_small_blk_buf--;
				return small_blk_buf[i];
			}
		}
	}
	
	return NULL;
}

static void blk_buf_free(char *blk_buffer) {

	int i;

	for (i = 0; i < BIG_MAX_BUFFER_ALLOC; i++) {

		if (big_blk_buf[i] == blk_buffer) {

			NDAS_BUG_ON(big_blk_buf_used[i] == false);
			big_blk_buf_used[i] = false;
			remained_big_blk_buf++;
			return;
		}
	}

	for (i = 0; i < SMALL_MAX_BUFFER_ALLOC; i++) {

		if (small_blk_buf[i] == blk_buffer) {

			NDAS_BUG_ON(small_blk_buf_used[i] == false);
			small_blk_buf_used[i] = false;
			remained_small_blk_buf++;
			return;
		}
	}

	NDAS_BUG_ON(true);

	return;
}

#endif

#define NBIO_ADJACENT(ndas_req, data) ( 							\
    (ndas_req)->req_iov_num > 0 && 									\
    (ndas_req)->req_iov[(ndas_req)->req_iov_num - 1].iov_base + 	\
    (ndas_req)->req_iov[(ndas_req)->req_iov_num - 1].iov_len == (data) )

static ndas_request_t *bdev_req_alloc(int slot, struct request *req) 
{
    ndas_request_t *ndas_req;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	struct req_iterator	iter;
	struct bio_vec		*bvec;
#else
    struct buffer_head  *bh;
#endif

	ndas_req = ndas_request_alloc();

    if (!ndas_req) {

		NDAS_BUG_ON(true);
		return NULL;
	}

	memset( ndas_req, 0, sizeof(ndas_request_t) );

#ifdef DEBUG
    ndas_req->magic = NDAS_REQ_MAGIC;
#endif

	ndas_req->slot = slot;

    INIT_LIST_HEAD( &ndas_req->queue );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

	rq_for_each_segment(bvec, req, iter) {

		if (is_highmem(page_zone(bvec->bv_page))) {

			NDAS_BUG_ON(true);

			ndas_req->private[ndas_req->req_iov_num] = bvec->bv_page;
            ndas_req->req_iov[ndas_req->req_iov_num].iov_base = kmap(bvec->bv_page) + bvec->bv_offset;
            ndas_req->req_iov[ndas_req->req_iov_num].iov_len = bvec->bv_len;
            ndas_req->req_iov_num++;
			
			printk( KERN_INFO "kmap(%p) called.\n", bvec->bv_page );

        } else {

		    if (NBIO_ADJACENT(ndas_req, page_address(bvec->bv_page) + bvec->bv_offset)) {

				ndas_req->req_iov[ndas_req->req_iov_num - 1].iov_len += bvec->bv_len;
 
			} else if (unlikely(ndas_req->req_iov_num == NDAS_MAX_PHYS_SEGMENTS)) {

                printk( KERN_ERR "Too many memblock. %ld %d %d %ld %ld %d\n",
                				  (unsigned long)ndas_req->req_iov_num, req->nr_phys_segments, req->nr_hw_segments,
								  ndas_req->u.s.nr_sectors, req->nr_sectors, NDAS_MAX_PHYS_SEGMENTS );

				NDAS_BUG_ON(true);
				
                ndas_request_free(ndas_req);
                return NULL;
								
			} else {

                ndas_req->req_iov[ndas_req->req_iov_num].iov_len = bvec->bv_len;
                ndas_req->req_iov[ndas_req->req_iov_num].iov_base = page_address(bvec->bv_page) + bvec->bv_offset;
                ndas_req->req_iov_num++;
			}
		}

		ndas_req->u.s.nr_sectors += bvec->bv_len >> 9;

    } // rq_for_each_segment

#else

    for (bh = req->bh; bh; bh = bh->b_reqnext) {

        if (NBIO_ADJACENT(ndas_req, bh->b_data)) {

            ndas_req->req_iov[ndas_req->req_iov_num - 1].iov_len += bh->b_size;

			dbg_call( 8, "Merged. nr_blocks=%d\n", ndas_req->req_iov_num );

		} else if (unlikely(ndas_req->req_iov_num == NDAS_MAX_PHYS_SEGMENTS)) {

        	printk( KERN_ERR "Too many memblock. %d %d %ld %ld %d\n",
               				  ndas_req->req_iov_num, req->nr_hw_segments, ndas_req->u.s.nr_sectors,
							  req->nr_sectors, NDAS_MAX_PHYS_SEGMENTS );

			NDAS_BUG_ON(true);
				
            ndas_request_free(ndas_req);
            return NULL;
								
		} else {

            ndas_req->req_iov[ndas_req->req_iov_num].iov_len = bh->b_size;
            ndas_req->req_iov[ndas_req->req_iov_num].iov_base = bh->b_data;
            ndas_req->req_iov_num++;
        }

		ndas_req->u.s.nr_sectors += bh->b_size >> 9;

        dbg_call( 8, "nr_blocks: %d, len=%d\n", ndas_req->req_iov_num, bh->b_size ); 
    }

#endif

    ndas_req->req = req;

    NDAS_BUG_ON( ndas_req->u.s.nr_sectors != req->nr_sectors );

    return ndas_req;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))

void bdev_prepare_flush(struct request_queue *q, struct request *req)
{
//    printk("prepare_flush\n");
    memset(req->cmd, 0, sizeof(req->cmd));

    // Indicate IDE task command reqest.
    // We adopt IDE task command for NDAS block device's flush command.
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
    req->cmd_type = REQ_TYPE_FLUSH;
#else
    REQUEST_FLAGS(req) |= REQ_DRIVE_TASK;
#endif

    req->timeout = 0;

    // Set SYNCHRONIZE_CACHE scsi command.
    req->cmd[0]= IDE_CMD_FLUSH_CACHE;
    req->cmd_len = 10;
    req->buffer = req->cmd;
}
#endif

#endif

static void _bdev_end_io(struct request *req, ndas_error_t err, size_t nr_sectors) 
{
    int 			uptodate = NDAS_SUCCESS(err) ? 1 : 0;
	unsigned long 	flags;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    struct request_queue 	*q = req->q;
#endif

    if (!uptodate) {

        printk( "ndas: IO error occurred at sector %d(slot %d): %s\n", 
            	(int)req->sector, SLOT_R(req), NDAS_GET_STRING_ERROR(err) );

        req->errors++;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25))

	spin_lock_irqsave( q->queue_lock, flags );
	__blk_end_request( req, req->errors, req->nr_sectors << 9 );
	spin_unlock_irqrestore( q->queue_lock, flags );


#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))

	spin_lock_irqsave( q->queue_lock, flags );

	if (end_that_request_first(req, uptodate, nr_sectors) == 0) {

		end_that_request_last( req, uptodate );
    }

    spin_unlock_irqrestore( q->queue_lock, flags );

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

	spin_lock_irqsave( q->queue_lock, flags );

	if (end_that_request_first(req, uptodate, nr_sectors) == 0) {

		end_that_request_last(req);
    }

    spin_unlock_irqrestore( q->queue_lock, flags );

#else

    spin_lock_irqsave( &io_request_lock, flags );

    while (end_that_request_first(req, uptodate, DEVICE_NAME));

    end_that_request_last(req);

    spin_unlock_irqrestore( &io_request_lock, flags );

#endif
}

static void bdev_end_io(ndas_request_t *ndas_req, void *arg)
{
    struct request *req = ndas_req->req;

	int i;

    for (i = 0; i < ndas_req->req_iov_num; i++) {

        if (ndas_req->private[i]) {

			NDAS_BUG_ON(true);
            kunmap((struct page *)ndas_req->private[i]);
            ndas_req->private[i] = NULL;
		}
    }

	if (req) {

		dbg_call( 3, "%d %lx %ld %ld %llu\n", 
					 REQ_DIR(req), ndas_req->jiffies, jiffies - ndas_req->jiffies, 
					 ndas_req->u.s.nr_sectors, (long long)req->sector );

   		_bdev_end_io( req, ndas_req->err, ndas_req->u.s.nr_sectors );
	}

#ifdef NDAS_WRITE_BACK

	for (i=0; i<NDAS_WRITE_BACK_BUF_LEN; i++) {

		if (ndas_req->buf[i]) {

			blk_buf_free( ndas_req->buf[i] );
		}
	}

#endif
	
	ndas_request_free(ndas_req);
}

static ndas_error_t bdev_handle_io(struct request *req) 
{
    ndas_error_t	err;
    ndas_request_t 	*ndas_req;

    int 			 slot = SLOT_R(req);
    sdev_t 			*sdev = sdev_lookup_byslot(slot);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))

 	if (req->cmd_type != REQ_TYPE_FLUSH && req->cmd_type != REQ_TYPE_FS) {

		printk( "ndas: req->cmd_type = %d\n", req->cmd_type );
   		_bdev_end_io( req, NDAS_ERROR_INVALID_OPERATION, 0 );
	
		NDAS_BUG_ON(true);

		return NDAS_OK;
	};

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

	dbg_call( 3, "req : flags = %x, REQ_DIR(req) = %d, sector = %ld, nr_sectors = %d\n", 
				 (int)req->flags, (int)REQ_DIR(req), req->sector, (int)req->nr_sectors );

    if (!(req->flags & REQ_DRIVE_TASK) && !(req->flags & REQ_CMD)) {

		printk( "ndas: req->flags = %lx\n", req->flags );
   		_bdev_end_io( req, NDAS_ERROR_INVALID_OPERATION, 0 );

		NDAS_BUG_ON(true);

		return NDAS_OK;
	};

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else

	dbg_call( 4, "req : REQ_DIR(req) = %d, sector = %ld, nr_sectors = %ld\n",
				 REQ_DIR(req), req->sector, req->nr_sectors );

    if (unlikely(req->sector + req->nr_sectors > 
				 ndas_hd[MINOR(req->rq_dev)].start_sect + ndas_hd[MINOR(req->rq_dev)].nr_sects)) {

     	printk( "ndas: req->sector = %ld, req->nr_sectors = %ld, ndas_hd[MINOR(req->rq_dev)].nr_sects = %ld, " 
				"MINOR(req->rq_dev) = %d\n",
				req->sector, req->nr_sectors, ndas_hd[MINOR(req->rq_dev)].nr_sects, MINOR(req->rq_dev) );

		printk( "ndas: tried to access area that exceeds the partition boundary\n" );

		_bdev_end_io( req, NDAS_ERROR_INVALID_OPERATION, 0 );

		if (ndas_hd[MINOR(req->rq_dev)].nr_sects != 0) {

			NDAS_BUG_ON(true);
		}

		return NDAS_OK;
   	}

#endif

#ifdef NDAS_WRITE_BACK

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
    if (req->cmd_type == REQ_TYPE_FLUSH && req->cmd[0] == IDE_CMD_FLUSH_CACHE) {

   		_bdev_end_io( req, NDAS_OK, 0 );
		return NDAS_OK;
    }
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
    if (REQUEST_FLAGS(req) & (REQ_DRIVE_TASK) && req->cmd[0] == IDE_CMD_FLUSH_CACHE) {

   		_bdev_end_io( req, NDAS_OK, 0 );
		return NDAS_OK;
    }
#endif

#endif

	request_num ++;

	sec_acu_count 	+= req->nr_sectors * sdev->info.logical_block_size;
	sec2_acu_count 	+= req->nr_sectors * sdev->info.logical_block_size;
	acu_count 		+= req->nr_sectors * sdev->info.logical_block_size;

	if (req->nr_sectors >= BIG_BLK_BUF_SEC_LEN) {

		big_request ++;

	} else {

		if (REQ_DIR(req) == WRITE) {
	
			small_request ++;
			small_acu_count += req->nr_sectors * sdev->info.logical_block_size;
		}
	}

	if ((jiffies - sec_prev_jiff) >= msecs_to_jiffies(1000)) {

		int	 mbps = 0;
		long acu_jiff;

		acu_jiff = jiffies - sec_prev_jiff;

		if (jiffies_to_msecs(acu_jiff) <= 0) {

			dbg_call( 1, "jiffies = %lu sec_prev_jiff = %lu\n", jiffies, sec_prev_jiff );
			dbg_call( 1, "msecs_to_jiffies(1000) = %lu\n", msecs_to_jiffies(1000) );
			dbg_call( 1, "acu_jiff = %ld jiffies_to_msecs(acu_jiff) = %u\n", acu_jiff, jiffies_to_msecs(acu_jiff) );

			NDAS_BUG_ON(true);

		} else {

			mbps = sec_acu_count * 8 * 10 / jiffies_to_msecs(acu_jiff) / 1024 * msecs_to_jiffies(1000) / 1024;
		}

		sec_prev_jiff = jiffies;

		if (peak_mbps < mbps)
			peak_mbps = mbps;
 
		sec_acu_count = 0;
	}

	if ((jiffies - sec2_prev_jiff) >= msecs_to_jiffies(2000)) {

		int	 mbps = 0;
		long acu_jiff;

		acu_jiff = jiffies - sec2_prev_jiff;

		if (jiffies_to_msecs(acu_jiff) <= 0) {

			dbg_call( 1, "jiffies = %lu sec2_prev_jiff = %lu\n", jiffies, sec2_prev_jiff );
			dbg_call( 1, "msecs_to_jiffies(2000) = %lu\n", msecs_to_jiffies(2000) );
			dbg_call( 1, "acu_jiff = %ld jiffies_to_msecs(acu_jiff) = %u\n", acu_jiff, jiffies_to_msecs(acu_jiff) );

			NDAS_BUG_ON(true);

		} else {

			mbps = sec2_acu_count * 8 * 10 / jiffies_to_msecs(acu_jiff) / 1024 * msecs_to_jiffies(1000) / 1024;
		}
			
		sec2_prev_jiff = jiffies;

		if (peak2_mbps < mbps)
			peak2_mbps = mbps;
 
		sec2_acu_count = 0;
	}

	if ((jiffies - prev_jiff) >= msecs_to_jiffies(10000)) {

		int	 mbps = 0;
		long acu_jiff;

		acu_jiff = jiffies - prev_jiff;

		if (jiffies_to_msecs(acu_jiff) <= 0) {

			dbg_call( 1, "jiffies = %lu sec2_prev_jiff = %lu\n", jiffies, sec2_prev_jiff );
			dbg_call( 1, "msecs_to_jiffies(10000) = %lu\n", msecs_to_jiffies(10000) );
			dbg_call( 1, "acu_jiff = %ld jiffies_to_msecs(acu_jiff) = %u\n", acu_jiff, jiffies_to_msecs(acu_jiff) );

			NDAS_BUG_ON(true);

		} else {

			mbps = acu_count * 8 * 10 / jiffies_to_msecs(acu_jiff) / 1024 * msecs_to_jiffies(1000) / 1024;
		}

#ifdef NDAS_WRITE_BACK
		printk( "%4u requests(%4u, %4u), %5u ms, %5d Kbytes, %4d Bytes/ms, %2d.%1d Mbps "
 				"Peak: %2d.%1d Mbps Peak2: %2d.%1d Mbps %d %d\n", 
				request_num, small_request, big_request, acu_jiff, acu_count/1024, 
				acu_count/acu_jiff, mbps/10, mbps%10, peak_mbps/10, peak_mbps%10, 
				peak2_mbps/10, peak2_mbps%10, remained_big_blk_buf, remained_small_blk_buf );
#else
		printk( "%4u requests(%4u, %4u), %5lu ms, %5d Kbytes, %4lu Bytes/ms, %2d.%1d Mbps "
 				"Peak: %2d.%1d Mbps Peak2: %2d.%1d Mbps\n", 
				request_num, small_request, big_request, acu_jiff, acu_count/1024, 
				acu_count/acu_jiff, mbps/10, mbps%10, peak_mbps/10, peak_mbps%10, 
				peak2_mbps/10, peak2_mbps%10 );
#endif
	
		prev_jiff = jiffies;

		acu_count = 0;
		small_acu_count = 0;
		request_num = 0;
		small_request = 0;
		big_request = 0;

		peak_mbps = 0;
		peak2_mbps = 0;
	}

	ndas_req = bdev_req_alloc( slot, req );

    if (unlikely(!ndas_req)) {

        printk( "ndas: Out of memory at sector %d(slot %d): %s\n", 
            	(int)req->sector,
            	SLOT_R(req),
            	NDAS_GET_STRING_ERROR(NDAS_ERROR_OUT_OF_MEMORY) );

        _bdev_end_io(req, NDAS_ERROR_OUT_OF_MEMORY, req->nr_sectors);

        return NDAS_ERROR_OUT_OF_MEMORY;
    }

	ndas_req->jiffies = jiffies;
	dbg_call( 2, "hd %lx\n", ndas_req->jiffies );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
    if (req->cmd_type == REQ_TYPE_FLUSH) {
#else
    if (REQUEST_FLAGS(req) & (REQ_DRIVE_TASK)) {
#endif
        switch(req->cmd[0]) {
      
		case IDE_CMD_FLUSH_CACHE:

			dbg_call( 4, "ndas: flushing with flags=%lx\n", (unsigned long)REQUEST_FLAGS(req) );
			ndas_req->cmd = NDAS_CMD_FLUSH;
    		err = sdev_io( slot, ndas_req, true );

            break;

		default:

            printk( "ndas: SCSI operation 0x%x is not supported.\n", (unsigned int)req->cmd[0] );
			err = NDAS_ERROR_INVALID_OPERATION;

			NDAS_BUG_ON(true);
        }

	    if (!NDAS_SUCCESS(err)) {

			ndas_req->err = err;
    	    bdev_end_io( ndas_req, NULL );
    	}

    	return err;
    }

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    ndas_req->u.s.sector = req->sector;
#else
    ndas_req->u.s.sector = req->sector + ndas_hd[MINOR(req->rq_dev)].start_sect;

	dbg_call( 4, "%p %u %lx %ld %lld\n", 
				 req, REQ_DIR(req), ndas_req->jiffies, ndas_req->u.s.nr_sectors, ndas_req->u.s.sector );

	dbg_call( 4, "%d %lu\n", 
				 MINOR(req->rq_dev), ndas_hd[MINOR(req->rq_dev)].start_sect );
#endif

    ndas_req->done[ndas_req->next_done]		 = bdev_end_io;
    ndas_req->done_arg[ndas_req->next_done]  = NULL;
	ndas_req->next_done ++;

	ndas_req->urgent 	= false;

#ifdef NDAS_WRITE_BACK

	if (REQ_DIR(req) == WRITE) {

		dbg_call( 4, "%u %lx %ld %ld\n", 
						(int)REQ_DIR(req), ndas_req->jiffies, ndas_req->u.s.nr_sectors, ndas_req->sector );

		if (ndas_req->u.s.nr_sectors > 1024) {
 
			dbg_call( 0, "big request request.num_sec %ld\n", ndas_req->u.s.nr_sectors );
			NDAS_BUG_ON( true );
			_bdev_end_io(req, NDAS_OK, ndas_req->u.s.nr_sectors);
			ndas_req->req = NULL;
		
		} else {

			int require_blk;
			int i;

			bool big;

			big = (ndas_req->u.s.nr_sectors >= BIG_BLK_BUF_SEC_LEN);

			if (big) {

				require_blk = (ndas_req->u.s.nr_sectors + BIG_BLK_BUF_SEC_LEN -1) / BIG_BLK_BUF_SEC_LEN;
			
			} else {

				require_blk = (ndas_req->u.s.nr_sectors + SMALL_BLK_BUF_SEC_LEN -1) / SMALL_BLK_BUF_SEC_LEN;
			}				

			for (i=0; i<require_blk; i++) {

				ndas_req->buf[i] = blk_buf_alloc(big);

				if (ndas_req->buf[i] == NULL) {

					int j;

					for (j=0; j<i; j++) {

						blk_buf_free( ndas_req->buf[j] );
						ndas_req->buf[j] = NULL;
					}
						
					break;
				}
			}

			if (ndas_req->buf[0] == NULL) {

				printk( "fail to allocation big = %d\n", big );
				ndas_req->urgent = true;

			} else {
			 
				int 			i;
				int 			buf_index;
				unsigned long 	buf_copied;
				
				struct iovec temp_blocks[NDAS_MAX_PHYS_SEGMENTS];

			    for (i = 0; i < ndas_req->req_iov_num; i++) {
        
					if (ndas_req->private[i]) {
            
						kunmap((struct page *)ndas_req->private[i]);
			            ndas_req->private[i] = NULL;
					}
    			}

				buf_index = 0;
				buf_copied = 0;
				
				for (i=0; i<ndas_req->req_iov_num; i++) {

					unsigned int	block_copied = 0;
	
					do {

						unsigned long 	block_copy_len;
		
						if (big) {

							block_copy_len = BIG_BLK_BUF_SEC_LEN*SECTOR_SIZE - buf_copied;
					
						} else {

							block_copy_len = SMALL_BLK_BUF_SEC_LEN*SECTOR_SIZE - buf_copied;
						}

						block_copy_len = (block_copy_len <= (ndas_req->req_iov[i].iov_len - block_copied) ? 
											block_copy_len : (ndas_req->req_iov[i].iov_len - block_copied));

						memcpy( ndas_req->buf[buf_index]+buf_copied, 
								(char*)(ndas_req->req_iov[i].iov_base)+block_copied, block_copy_len );

						buf_copied += block_copy_len;
						block_copied += block_copy_len;

						NDAS_BUG_ON( !((big && buf_copied <= BIG_BLK_BUF_SEC_LEN*SECTOR_SIZE) || 
									   (!big && buf_copied <= SMALL_BLK_BUF_SEC_LEN*SECTOR_SIZE)) );

						NDAS_BUG_ON( block_copied > ndas_req->req_iov[i].iov_len );

						if ((big && buf_copied == BIG_BLK_BUF_SEC_LEN*SECTOR_SIZE) || 
							(!big && buf_copied == SMALL_BLK_BUF_SEC_LEN*SECTOR_SIZE)) {
	
							temp_blocks[buf_index].iov_base = ndas_req->buf[buf_index];
							temp_blocks[buf_index].iov_len = buf_copied;
						
							buf_index ++;
							buf_copied = 0;
						}

						if (block_copied == ndas_req->req_iov[i].iov_len) {
	
							break;
						}
					
					} while(1);
				}

				ndas_req->req_iov_num = buf_index;

				if (buf_copied) {

					temp_blocks[buf_index].iov_base = ndas_req->buf[buf_index];
					temp_blocks[buf_index].iov_len = buf_copied;
					ndas_req->req_iov_num ++;
				}

				NDAS_BUG_ON( ndas_req->req_iov_num > NDAS_MAX_PHYS_SEGMENTS );

				memcpy( ndas_req->req_iov, temp_blocks, sizeof(struct iovec) * ndas_req->req_iov_num );
			
				_bdev_end_io( req, NDAS_OK, ndas_req->u.s.nr_sectors );
				ndas_req->req = NULL;			
			}
		}
	}
	
#endif

	dbg_call( 4, "req = %p ndas_req = %p %u %lx %ld %lld\n", 
				 req, ndas_req, (int)REQ_DIR(req), ndas_req->jiffies, ndas_req->u.s.nr_sectors, (long long)ndas_req->u.s.sector );

	NDAS_BUG_ON( ndas_req->u.s.nr_sectors != req->nr_sectors );

    switch (REQ_DIR(req)) {

    case READ:
    case READA:

		ndas_req->cmd = NDAS_CMD_READ;
    	err = sdev_io( slot, ndas_req, true );
        break;

    case WRITE:

		ndas_req->cmd = NDAS_CMD_WRITE;
   		err = sdev_io( slot, ndas_req, true );

        break;

    default:

        err = NDAS_ERROR_INVALID_OPERATION;
		printk( "ndas: operation %u is not supported.\n", (unsigned int)REQ_DIR(req) );

		NDAS_BUG_ON(true);
    }

    if (!NDAS_SUCCESS(err)) {

		ndas_req->err = err;
   	    bdev_end_io( ndas_req, NULL );
    }

    return err;
}

// block request procedure. 
// Entered with q->queue_lock locked 

void bdev_request_proc(struct request_queue *q)
{
    ndas_error_t 	err = NDAS_OK;
    struct request *req;

    while ((req = NBLK_NEXT_REQUEST(q)) != NULL) {

#if 0
        if (unlikely(!sdev_lookup_byslot(SLOT_R(req))->info.enabled)) {

            printk ( "ndas: requested to disabled device\n" );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
            REQUEST_FLAGS(req) |= REQ_QUIET | REQ_FAILFAST | REQ_FAILED;
            end_request(req, 0);
#else
            end_request(0);
#endif

            continue;
        }

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

        if (unlikely(test_bit(NDAS_QUEUE_FLAG_SUSPENDED, &(sdev_lookup_byslot(SLOT_R(req))->queue_flags)))) {

            printk ( "ndas: Queue is suspended\n" );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
            REQUEST_FLAGS(req) |= REQ_QUIET | REQ_FAILFAST | REQ_FAILED;
#endif
            end_request(req, 0);

            continue;
        }
#endif

#if 0

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else
		if (req->rq_status == RQ_INACTIVE) {
			
           	printk ( "ndas: req->rq_status = %x\n", req->rq_status );
			NDAS_BUG_ON(true);
 
            end_request(0);
            continue;
        }
#endif

		if (sdev_lookup_byslot(SLOT_R(req)) && !sdev_lookup_byslot(SLOT_R(req))->info.writable && REQ_DIR(req) == WRITE) {
	
			NDAS_BUG_ON(true);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
            REQUEST_FLAGS(req) |= REQ_QUIET | REQ_FAILFAST | REQ_FAILED;
            end_request(req, 0);
#else
            end_request(0);
#endif
            continue;
        }

#endif

        blkdev_dequeue_request(req);

        spin_unlock_irq( NBLK_LOCK(q) );
        err = bdev_handle_io( req );
        spin_lock_irq( NBLK_LOCK(q) );
    }
}

static int bdev_open(struct inode *inode, struct file *filp)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else
    int part;
#endif

    int slot;
    
    dbg_call( 1, "ing inode=%p\n", inode );

    if (!filp) {

		NDAS_BUG_ON(true);
        dbg_call( 1,"Weird, open called with filp = 0\n" );

        return -EIO;
    }

    slot = SLOT_I(inode);

    dbg_call( 1, "slot = 0x%x\n", slot );
    dbg_call( 1, "dev_t = 0x%x\n", inode->i_bdev->bd_dev );

    if (slot >= NDAS_MAX_SLOT) {

        printk( "ndas: minor too big.\n" ); 
		NDAS_BUG_ON(true);
        return -ENXIO;
    }

    if (sdev_lookup_byslot(slot) == NULL) {
 
        printk( "ndas: slot is not exist\n" ); 
        return -ENODEV;
	}

    if (sdev_lookup_byslot(slot)->info.enabled == 0) {

        printk( "ndas: device is not enabled\n" );
        return -ENODEV;
    }

    dbg_call( 1, "filp->f_mode=%x\n", filp->f_mode ); 
    dbg_call( 1, "filp->f_flags=%x\n", filp->f_flags ); 

    if (filp->f_flags & O_NDELAY) {

        dbg_call( 1, "O_NDELAY\n" );
    }

    if (!(filp->f_flags & O_NDELAY) && 
		(filp->f_mode & (O_WRONLY | O_RDWR)) && !sdev_lookup_byslot(slot)->info.writable) {
 
        printk( "ndas: device is enabled with read only mode\n" );
        return -EROFS;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else

    if (filp->f_mode & O_ACCMODE) {

        bdev_check_netdisk_change( inode->i_rdev );
	}

    part = NDAS_PARTITION(inode->i_rdev);

    if (sdev_lookup_byslot(slot)->ref[part] == -1 || (sdev_lookup_byslot(slot)->ref[part] && (filp->f_flags & O_EXCL))) {

        dbg_call( 1, "adapter_state->nd_ref[%d] = %d\n", part, sdev_lookup_byslot(slot)->ref[part] );
        printk( "ndas: device is not exclusively opened\n" );
        return -EBUSY;
    }

    if (filp->f_flags & O_EXCL) {

        sdev_lookup_byslot(slot)->ref[part] = -1;

	} else {

        sdev_lookup_byslot(slot)->ref[part] ++;
	}

    dbg_call( 3, "sdev_lookup_byslot(slot)->nd_ref[%d] = %d, flags = %x, mode = %x\n", 
				 part, sdev_lookup_byslot(slot)->ref[part], filp ? filp->f_flags : 0, filp ? filp->f_mode : 0 );

    MOD_INC_USE_COUNT;

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    dbg_call( 1, "ed open slot = %d\n", slot );
#else
    dbg_call( 1, "ed open slot = %d, part = %d\n", slot, part );
#endif

    return 0;
}

/* close the NDAS device file */
static int bdev_release(struct inode *inode, struct file *filp)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else
    int                part;
#endif
    int                slot;

    dbg_call( 1, "inode=%p\n", inode );
    dbg_call( 1, "filp=%p\n", filp );
    
    if (!inode) {

		NDAS_BUG_ON(true);

        printk( "Weird, open called with inode = 0\n" );
        return -ENODEV;
    }

    slot = SLOT_I(inode);

    dbg_call( 1,"slot =%d\n", slot );

    if (slot >= NDAS_MAX_SLOT) {

		NDAS_BUG_ON(true);

        printk( "ndas: minor too big.\n" ); 
        return -ENXIO;
    }

    if (filp && (filp->f_mode & (O_WRONLY | O_RDWR | OPEN_WRITE_BIT))) {

        file_fsync(filp, filp->f_dentry, 0);
	}

    if (sdev_lookup_byslot(slot) == NULL) {
 
        printk( "ndas: slot is not exist\n" ); 
		NDAS_BUG_ON(true);

        return -ENODEV;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else

    part = NDAS_PARTITION(inode->i_rdev);

    dbg_call( 3, "slot=%d, part=%d\n", slot, part );

    if (sdev_lookup_byslot(slot)->ref[part] == -1) {

        sdev_lookup_byslot(slot)->ref[part] = 0;
    
	} else if(!sdev_lookup_byslot(slot)->ref[part]) {

        dbg_call( 3,"nd_ref[%d] = 0\n", part );

	} else {

         sdev_lookup_byslot(slot)->ref[part] --;
	}

    MOD_DEC_USE_COUNT;

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    dbg_call( 1, "ed open slot = %d\n", slot );
#else
    dbg_call( 1, "ed open slot = %d, part = %d\n", slot, part );
#endif

    return 0;
}

static int bdev_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    int		slot;
	int		result;

	dbg_call( 1, "ing inode=%p, filp=%p, cmd=0x%x, arg=%p\n", inode, filp, cmd, (void*) arg );

//	printk( "bdev_ioctl cmd=0x%x BLKGETSIZE=0x%x\n", cmd, BLKGETSIZE );

    if (!capable(CAP_SYS_ADMIN)) {

        return -EPERM;
	}

    if (!inode) {

		NDAS_BUG_ON(true);
        printk( "ndas: open called with inode = NULL\n" );
        return -EIO;
    }

    slot = SLOT_I(inode);

    dbg_call( 1, "slot =%d\n", slot );

    if (slot >= NDAS_MAX_SLOT) {

		printk( "ndas: Minor too big.\n" ); // Probably can not happen
		NDAS_BUG_ON(true);
        return -ENXIO;
    }

    if (sdev_lookup_byslot(slot) == NULL) {
 
        printk( "ndas: slot is not exist\n" ); 
		NDAS_BUG_ON(true);

        return -ENODEV;
	}

    switch(cmd) {

	case IOCTL_NDAS_LOCK: {

		pndas_lock 		ndas_lock;
		ndas_error_t 	error;
	    ndas_request_t	*ndas_req;

        dbg_call( 2, "IOCTL_NDAS_LOCK\n" );

		ndas_lock = (pndas_lock) arg;

		if (ndas_lock->lock_type != 0 && ndas_lock->lock_type != 1) {

			NDAS_BUG_ON(true);
			printk( "error : invalid GLOCK operation\n" );
			result = -EINVAL;
			break;
		}

		ndas_req = ndas_request_alloc();

		if (ndas_req == NULL) {

			NDAS_BUG_ON(true);
			return NDAS_ERROR_OUT_OF_MEMORY;
		}

		memset( ndas_req, 0, sizeof(ndas_request_t) );

#ifdef DEBUG
	    ndas_req->magic = NDAS_REQ_MAGIC;
#endif
		ndas_req->slot = slot;

		INIT_LIST_HEAD( &ndas_req->queue );
	
			ndas_req->cmd = NDAS_CMD_LOCKOP;

		if (ndas_lock->lock_type == 1) {

			ndas_req->u.l.lock_id = NDAS_LOCK_3;
			ndas_req->u.l.lock_op = NDAS_LOCK_TAKE;

		} else if ( ndas_lock->lock_type == 0 ) {

			ndas_req->u.l.lock_id = NDAS_LOCK_3;
			ndas_req->u.l.lock_op = NDAS_LOCK_GIVE;
		}

		error = sdev_io( slot, ndas_req, false );

		ndas_request_free( ndas_req );

        dbg_call( 2, "IOCTL_NDAS_LOCK ndas_lock->lock_type = %d, error = %d\n", ndas_lock->lock_type, error );

		ndas_lock->lock_status = NDAS_SUCCESS(error) ? 0 : -EINVAL;
		result = NDAS_SUCCESS(error) ? 0 : -EINVAL;
		
		break;	
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

    case BLKFLSBUF:

        result = -EINVAL;
        break;

#else

    case HDIO_GETGEO: {

 		struct hd_geometry geo;
		sdev_t			   *sdev;
		__u32 			   tmp;

        if (!arg) {

			NDAS_BUG_ON(true);
			result = -EINVAL;
		}

        if (!access_ok(VERIFY_WRITE, ((struct hd_geometry *)arg), sizeof(struct hd_geometry))) {

			NDAS_BUG_ON(true);
			result = -EFAULT;
		}

		memset( &geo, 0, sizeof(struct hd_geometry) );

	    sdev = sdev_lookup_byslot(slot);

		if (sdev == NULL) {
 
        	printk( "ndas: slot is not exist\n" ); 
			NDAS_BUG_ON(true);

        	result = -ENODEV;
			break;
		}

 		tmp = ndas_gd.part[MINOR(inode->i_rdev)].nr_sects / 
				(sdev->info.headers_per_disk[0] * sdev->info.sectors_per_cylinder[0]);
		
		if (tmp > 65536)
			geo.cylinders = 0xffff;
		else
			geo.cylinders = tmp;

		geo.heads   = sdev->info.headers_per_disk[0];
		geo.sectors = sdev->info.sectors_per_cylinder[0];

		geo.start = ndas_gd.part[MINOR(inode->i_rdev)].start_sect;

		result = copy_to_user(((struct hd_geometry *)arg), &geo, sizeof geo) ? -EFAULT : 0;

	    break;
    }

    case BLKGETSIZE: {

        if (!arg) {

			NDAS_BUG_ON(true);
			result = -EINVAL;
		}

        if (!access_ok(VERIFY_WRITE, ((long*)arg), sizeof(long))) {

			NDAS_BUG_ON(true);
			result = -EFAULT;
		}

        result = put_user( ndas_gd.part[MINOR(inode->i_rdev)].nr_sects, ((long*)arg) );

        dbg_call( 3, "#sectors=%ld\n", ndas_gd.part[MINOR(inode->i_rdev)].nr_sects );

	    break;
    }

    case BLKGETSIZE64: {

        if (!arg) {

			NDAS_BUG_ON(true);
			result = -EINVAL;
		}

        if (!access_ok(VERIFY_WRITE, ((long long*)arg), sizeof(long long))) {

			NDAS_BUG_ON(true);
			result = -EFAULT;
		}

        result = put_user( (long long)ndas_gd.part[MINOR(inode->i_rdev)].nr_sects, ((long long*)arg) );

        dbg_call( 3,"#sectors=%ld\n", ndas_gd.part[MINOR(inode->i_rdev)].nr_sects );

	    break;
    }

    case BLKRRPART: {

        dbg_call( 3, "BLKRRPART\n" );

		result = bdev_revalidate(inode->i_rdev);
        break;
	}

	case BLKROSET:
	case BLKROGET:
	case BLKFLSBUF:
	case BLKSSZGET:
	case BLKPG:
	case BLKELVGET:
	case BLKELVSET:
	case BLKBSZGET:
	case BLKBSZSET:

        result = blk_ioctl( inode->i_rdev, cmd, arg );
        break;

#endif

	case LOOP_CLR_FD:

		//invalidate_bdev(inode->i_bdev, 0);
       	//result = 0;
        result = -EINVAL;
		break;

    default: {

		printk( "cmd=0x%x\n", cmd );
		NDAS_BUG_ON(true);
        result = -EINVAL;
	}
    }

    dbg_call( 2, "ed cmd=%x result=%d\n", cmd, result );

    return result;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

static int bdev_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
    int 	slot;
	sdev_t	*sdev;
	__u32 	tmp;

	dbg_call( 1, "gendisk=%s\n", DEBUG_GENDISK(bdev->bd_disk) );

    if (bdev->bd_disk->major != NDAS_BLK_MAJOR) {

		NDAS_BUG_ON(true);
        dbg_call( 1, "not a netdisk\n" );
        return 0;
    }

    slot = SLOT(bdev->bd_disk);

    if (slot >= NDAS_MAX_SLOT) {

		NDAS_BUG_ON(true);
        dbg_call( 1, "Minor too big.\n" ); 
        return 0;
    }

    sdev = sdev_lookup_byslot(slot);

	if (sdev == NULL) {
 
        printk( "ndas: slot is not exist\n" ); 
		NDAS_BUG_ON(true);

        return -ENODEV;
	}

	NDAS_BUG_ON( get_capacity(bdev->bd_disk) > 0xFFFFFFFF );

	tmp = (u32)get_capacity(bdev->bd_disk) / (sdev->info.headers_per_disk[0] * sdev->info.sectors_per_cylinder[0]);
		
	if (tmp > 65536)
		geo->cylinders = 0xffff;
	else
		geo->cylinders = tmp;

	geo->heads   = sdev->info.headers_per_disk[0];
	geo->sectors = sdev->info.sectors_per_cylinder[0];

    dbg_call( 1, "get_capacity(bdev->bd_disk) = %llu\n", (long long)get_capacity(bdev->bd_disk) );

    dbg_call( 1, "slot = %d ed\n", slot );

    return 0;
}

#endif

static int bdev_media_changed (struct gendisk *g) 
{
	dbg_call( 2, "gendisk=%s\n", DEBUG_GENDISK(g) );
    return 1;
}

static int bdev_revalidate_disk(struct gendisk *g)
{
    int slot;

	dbg_call( 2, "gendisk=%s\n", DEBUG_GENDISK(g) );

    slot = SLOT(g);
    if (g->major != NDAS_BLK_MAJOR) {
        dbg_call( 1,"not a netdisk\n" );
        return 0;
    }
    
    if (slot >= NDAS_MAX_SLOT) {
        dbg_call( 1, "Minor too big.\n" ); 
        return 0;
    }

    dbg_call( 2, "ed\n" );
    return 0;
}

#else

static int bdev_check_netdisk_change(kdev_t rdev)
{
    dbg_call( 2, "kdev_t=%d\n", rdev );
    return 1;
}

static int bdev_revalidate(kdev_t rdev)
{
    int slot;

    dbg_call( 2, "kdev_t=%d\n", rdev );

    slot = SLOT(rdev);
    if (MAJOR(rdev) != NDAS_BLK_MAJOR) {
        dbg_call( 1,"not a netdisk\n" );
        return 0;
    }
    
    if (slot >= NDAS_MAX_SLOT) {
        dbg_call( 1,"Minor too big.\n" ); 
        return 0;
    }
    if ( NDAS_PARTITION(rdev) != 0 ) {

        dbg_call( 1,"not a disk\n" ); 
        return 0;
    }
    return sdev_read_partition(slot) == NDAS_OK ? 0 : 0;
}

#endif

int bdev_init(void) 
{
    int ret;

#ifdef NDAS_WRITE_BACK

	int i;


	printk( "totalram_pages = %ld, nr_free_pages = %ld\n", totalram_pages*PAGE_SIZE, nr_free_pages()*PAGE_SIZE );
	
	big_buffer_alloc = totalram_pages * PAGE_SIZE / SECTOR_SIZE / BIG_BLK_BUF_SEC_LEN;

	printk( "big_buffer_alloc = %u, BIG_MAX_BUFFER_ALLOC = %u\n", big_buffer_alloc, BIG_MAX_BUFFER_ALLOC );

	if (BIG_MAX_BUFFER_ALLOC < big_buffer_alloc) {

		big_buffer_alloc = BIG_MAX_BUFFER_ALLOC;
	}

	for (i = 0; i < big_buffer_alloc; i++) {

		big_blk_buf[i] = kmalloc(BIG_BLK_BUF_SEC_LEN*SECTOR_SIZE, GFP_KERNEL);
		dbg_call( 4, "blk_buf[%d] = %p\n", i, big_blk_buf[i] );
		big_blk_buf_used[i] = false;
		
		if (big_blk_buf[i]) {

			remained_big_blk_buf++;
		}
	}

	for (i = 0; i < SMALL_MAX_BUFFER_ALLOC; i++) {

		small_blk_buf[i] = kmalloc(SMALL_BLK_BUF_SEC_LEN*SECTOR_SIZE, GFP_KERNEL);
		dbg_call( 4, "small_blk_buf[%d] = %p\n", i, small_blk_buf[i] );
		small_blk_buf_used[i] = false;

		if (small_blk_buf[i]) {

			remained_small_blk_buf++;
		}
	}

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else

    blksize_size[MAJOR_NR] 	= ndas_blksize_size;
    max_sectors[MAJOR_NR] 	= ndas_max_sectors;
    hardsect_size[MAJOR_NR] = ndas_hardsect_size;
    blk_size[MAJOR_NR] 		= ndas_blk_size;

    blk_dev[MAJOR_NR].queue = NULL;
    blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), bdev_request_proc);
    blk_queue_headactive(BLK_DEFAULT_QUEUE(MAJOR_NR), 0);

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23))
    ndas_req_kmem_cache = kmem_cache_create( "ndas_req", sizeof(ndas_request_t), 0, SLAB_HWCACHE_ALIGN, NULL );
#else
    ndas_req_kmem_cache = kmem_cache_create( "ndas_req", sizeof(ndas_request_t), 0, SLAB_HWCACHE_ALIGN, NULL, NULL );
#endif

    if (!ndas_req_kmem_cache) {

		NDAS_BUG_ON(true);
        printk(KERN_ERR "ndas: unable to create kmem cache\n");
        ret = -ENOMEM;
        goto errout;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    if ((ret = register_blkdev(NDAS_BLK_MAJOR, DEVICE_NAME)) < 0) {
#else
    if ((ret = devfs_register_blkdev(MAJOR_NR, "ndasdevice", &ndas_fops)) < 0) {
#endif
		NDAS_BUG_ON(true);
        printk(KERN_ERR "ndas: unable to get major %d for ndas block device\n", NDAS_BLK_MAJOR);
        goto errout;
    }

    printk( "ndas: registered ndas device at major number %d\n", NDAS_BLK_MAJOR );
    return 0;

errout:

    if (ndas_req_kmem_cache) {

        kmem_cache_destroy(ndas_req_kmem_cache);
        ndas_req_kmem_cache = NULL;
    }

#ifdef NDAS_WRITE_BACK

	for (i = 0; i < big_buffer_alloc; i++) {

		if (big_blk_buf[i]) {

			NDAS_BUG_ON(big_blk_buf_used[i] == true);
			remained_big_blk_buf--;
			kfree(big_blk_buf[i]);
		}
	}

 	for (i = 0; i < SMALL_MAX_BUFFER_ALLOC; i++) {

		if (small_blk_buf[i]) {

			NDAS_BUG_ON(small_blk_buf_used[i] == true);
			remained_small_blk_buf--;
			kfree(small_blk_buf[i]);
		}
	}

	NDAS_BUG_ON( remained_big_blk_buf != 0 );
	NDAS_BUG_ON( remained_small_blk_buf != 0 );

#endif

    return ret;
}

// block device clean up

int bdev_cleanup(void)
{
#ifdef NDAS_WRITE_BACK
	int i;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	unregister_blkdev( NDAS_BLK_MAJOR, DEVICE_NAME );
#else
	devfs_unregister_blkdev( NDAS_BLK_MAJOR, DEVICE_NAME );
#endif

    if (ndas_req_kmem_cache) {

        kmem_cache_destroy(ndas_req_kmem_cache);
        ndas_req_kmem_cache = NULL;
    }

#ifdef NDAS_WRITE_BACK

	for (i = 0; i < big_buffer_alloc; i++) {

		if (big_blk_buf[i]) {

			NDAS_BUG_ON( big_blk_buf_used[i] == true );

			remained_big_blk_buf--;
			kfree(big_blk_buf[i]);
		}
	}

 	for (i = 0; i < SMALL_MAX_BUFFER_ALLOC; i++) {

		if (small_blk_buf[i]) {

			NDAS_BUG_ON( small_blk_buf_used[i] == true );

			remained_small_blk_buf--;
			kfree(small_blk_buf[i]);
		}
	}

	NDAS_BUG_ON( remained_big_blk_buf != 0 );
	NDAS_BUG_ON( remained_small_blk_buf != 0 );

#endif

    return 0;
}

