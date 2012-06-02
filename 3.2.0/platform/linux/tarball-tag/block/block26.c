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
#include <linux_ver.h>
#include <linux/module.h> /* THIS_MODULE */
#include <ndasuser/ndasuser.h>
#include <ndasuser/io.h>
#include <sal/debug.h>
#include <ndasdev.h>
#include <procfs.h>

#include <linux/ide.h>

#include "block.h"
#include "ops.h"

/*
 * IDE command code
 */
#define IDE_CMD_FLUSH_CACHE 0xe7

struct block_device_operations ndas_fops = {
	.owner =            THIS_MODULE, 
	.open =            ndop_open, 
	.release =        ndop_release, 
	.ioctl =            ndop_ioctl, 
	.media_changed = ndop_media_changed,
	.revalidate_disk = ndop_revalidate_disk
};

KMEM_CACHE* nblk_kmem_cache;

static struct nbio_linux* nbio_alloc_splited(struct request *req)
{
	char *data;
	struct req_iterator iter;
	struct bio_vec* bvec;
	int slot = SLOT_R(req);
	int len = 0, chunk_size = 1 << 9;
	struct nbio_linux *nbio = kmem_cache_alloc(nblk_kmem_cache, GFP_ATOMIC);

	/* size: NBIO_SIZE(req->nr_sectors */
	if ( !nbio ) return NULL;

	
	dbgl_blk(4, "SLOT_R(req) = %d", slot);

	
	nbio->req = req;
	nbio->nr_blocks = 0;

	rq_for_each_segment(bvec, req, iter)
	{
		int j = bvec->bv_len; 
		data = page_address(bvec->bv_page) + bvec->bv_offset;
		while ( j > 0 ) 
		{
			int idx = SPLIT_IDX_SLOT(nbio->nr_blocks, blk_rq_sectors(req), slot); 
			nbio->blocks[idx].len = chunk_size;
			nbio->blocks[idx].ptr = data;
			j -= chunk_size;
			data += chunk_size;
			nbio->nr_blocks++;
		}
		len += bvec->bv_len;
	}
	
	nbio->nr_sectors = len >> 9;
	sal_assert( nbio->nr_sectors == blk_rq_sectors(req) );
	return nbio;
}

static inline struct nbio_linux* nbio_alloc(struct request *req) 
{
	int len = 0;
	struct req_iterator iter;
	struct bio_vec* bvec;
	struct nbio_linux *nbio = kmem_cache_alloc(nblk_kmem_cache, GFP_ATOMIC);
			/* size: NBIO_SIZE(req->nr_phys_segments) */
	if ( !nbio ) return NULL;

	nbio->req = req;
	nbio->nr_blocks = 0;

	/*
	 * Convert the request to nbio.
	 */

	rq_for_each_segment(bvec, req, iter)
	{
		
#ifdef CONFIG_HIGHMEM
		/* Set page to private to unmap later. */
		nbio->blocks[nbio->nr_blocks].private = bvec->bv_page;
		nbio->blocks[nbio->nr_blocks].ptr = kmap(bvec->bv_page) 
				+ bvec->bv_offset;
		nbio->blocks[nbio->nr_blocks].len = bvec->bv_len;
		nbio->nr_blocks++;
#else
#error Ur config-highmem is not defined.
		char *data;
		data = page_address(bvec->bv_page) + bvec->bv_offset;
		/* NBIO_ADJACENT is buggy in case of a CONFIG_HIGHMEM
		 * configuration (which is the default on newer kernels)
		 * The behavior is buggy starting at kernel commit
		 * e084b2d95e48b31aa45f9c49ffc6cdae8bdb21d4 (inside 2.6.31-rc5)
		 */
		if ( NBIO_ADJACENT(nbio,data) )
			nbio->blocks[nbio->nr_blocks - 1].len += bvec->bv_len;
		else {
			nbio->blocks[nbio->nr_blocks].len = bvec->bv_len;
			nbio->blocks[nbio->nr_blocks].ptr = data;
			nbio->nr_blocks++;
		}
#endif

		len += bvec->bv_len;

		/*
		 * Fail if no more memblocks is remaining.
		 */
		if( nbio->nr_blocks > ND_BLK_MAX_REQ_SEGMENT && 
				(len >> 9) < blk_rq_sectors(req))
		{
			printk(KERN_ERR "Too many memblock. %d %d %d %d %d\n",
				nbio->nr_blocks, req->nr_phys_segments, len, 
				(int)blk_rq_sectors(req), ND_BLK_MAX_REQ_SEGMENT);
			kmem_cache_free(nblk_kmem_cache ,nbio);
			return NULL;
		}
	} /* rq_for_each_segment */
	
	nbio->nr_sectors = len >> 9;
	sal_assert( nbio->nr_sectors == blk_rq_sectors(req));
	return nbio;
}

static int _is_not_recoverable(ndas_error_t err) {
	return (err != NDAS_ERROR_INVALID_RANGE_REQUEST &&
		err != NDAS_ERROR_READONLY &&
		err != NDAS_ERROR_READONLY &&
		err != NDAS_ERROR_OUT_OF_MEMORY &&
		err != NDAS_ERROR_HARDWARE_DEFECT &&
		err != NDAS_ERROR_BAD_SECTOR);
}


static inline void _end_io(ndas_error_t err, struct request *req, 
									size_t nr_sectors) 
{
	unsigned long flags;
	struct request_queue *q = req->q;
	int error = NDAS_SUCCESS(err) ? 0 : -EIO;

	spin_lock_irqsave(q->queue_lock, flags);
	if ( error ) {
		printk("ndas: IO error occurred at sector %d(slot %d): %s\n", 
			(int)blk_rq_pos(req),
			SLOT_R(req),
			NDAS_GET_STRING_ERROR(err));
		req->errors++;
		if ( _is_not_recoverable(err) ) {
			int slot = SLOT_R(req);
			struct ndas_slot *sd = NDAS_GET_SLOT_DEV(slot);

			if (!test_bit(NDAS_FLAG_QUEUE_SUSPENDED, &sd->queue_flags)) {
			    set_bit(NDAS_FLAG_QUEUE_SUSPENDED, &sd->queue_flags);
			}
		}
	}

	__blk_end_request(req, error, nr_sectors << 9);
	spin_unlock_irqrestore(q->queue_lock, flags);
}

/* 
 * signalled when the NDAS io is completed 
 */
NDAS_CALL
void nblk_end_io(int slot, ndas_error_t err, struct nbio_linux* nbio)
{
	struct request *req = nbio->req;
#ifdef CONFIG_HIGHMEM
	int i;
	for (i = 0; i < nbio->nr_blocks; i++) {
		if ( nbio->blocks[i].private ) 
			kunmap((struct page *)nbio->blocks[i].private);
	}
#endif     
	_end_io(err, req, nbio->nr_sectors);
	kmem_cache_free(nblk_kmem_cache ,nbio);
}
	
ndas_error_t nblk_handle_io(struct request *req) 
{
	ndas_error_t err;
	ndas_io_request *ndas_req;
	int slot = SLOT_R(req);
	struct ndas_slot *sd = NDAS_GET_SLOT_DEV(slot);
	struct nbio_linux *nbio;

	if ( sd->info.io_splits == 1 )  {
		nbio = nbio_alloc(req);
	} else {
		nbio = nbio_alloc_splited(req);
	}
	if (unlikely( !nbio )) {
		printk("ndas: Out of memory at sector %d(slot %d): %s\n", 
			(int)blk_rq_pos(req),
			SLOT_R(req),
			NDAS_GET_STRING_ERROR(NDAS_ERROR_OUT_OF_MEMORY));
		_end_io(NDAS_ERROR_OUT_OF_MEMORY, req, blk_rq_sectors(req));
		return NDAS_ERROR_OUT_OF_MEMORY;
	}

	if (unlikely(!sd->enabled)) {
		err =  NDAS_ERROR_NOT_CONNECTED;
		printk("ndas: Not connected %d(slot %d): %s\n", 
			(int)blk_rq_sectors(req),
			SLOT_R(req),
			NDAS_GET_STRING_ERROR(err));
		goto out;
	}

	ndas_req = &nbio->ndas_req;
	ndas_req->uio = nbio->blocks;
	ndas_req->buf = NULL;
	ndas_req->nr_uio = nbio->nr_blocks;
	ndas_req->num_sec = nbio->nr_sectors;
	ndas_req->start_sec = blk_rq_pos(req);

#ifndef NDAS_SYNC_IO
	ndas_req->done = (ndas_io_done) nblk_end_io;
	ndas_req->done_arg = nbio;
#else
	ndas_req->done = NULL;
	ndas_req->done_arg = NULL;
#endif

   if ( req->cmd_type == REQ_TYPE_ATA_TASKFILE )	
	{
		switch(req->cmd[0]) {
			case IDE_CMD_FLUSH_CACHE:
#ifdef DEBUG
				printk ("ndas: flushing with flags=%lx\n",
					(unsigned long)REQUEST_FLAGS(req));
#endif
			    err = ndas_flush(slot, ndas_req);
			break;
			default:
			    err = NDAS_ERROR_INVALID_OPERATION;
			    printk("ndas: SCSI operation 0x%x is not supported.\n", 
			    	(unsigned int)req->cmd[0]);
		}
		goto out;
	}

	switch ( REQ_DIR(req) ) {
	case READ:
	case READA:
		err = ndas_read(slot, ndas_req);
		break;
	case WRITE:
		err = ndas_write(slot, ndas_req);
		break;
	default:
		err = NDAS_ERROR_INVALID_OPERATION;
		printk("ndas: operation %u is not supported.\n", 
			(unsigned int)REQ_DIR(req));
	}
out:
#ifndef NDAS_SYNC_IO
	if ( !NDAS_SUCCESS(err) ) {
		nblk_end_io(slot, err, nbio);
	}
#else
	nblk_end_io(slot, err, nbio);
#endif
	return err;
}

#define NBLK_LOCK(q) ((q)->queue_lock)

/* block request procedure. 
   Entered with q->queue_lock locked 
*/
void nblk_request_proc(struct request_queue *q)
{
	struct request *req;
	ndas_error_t err = NDAS_OK;

	while((req = NBLK_NEXT_REQUEST(q)) != NULL)
	{
		if (test_bit(NDAS_FLAG_QUEUE_SUSPENDED, 
				&(NDAS_GET_SLOT_DEV(SLOT_R(req))->queue_flags)))  
		{
			printk ("ndas: Queue is suspended\n");
			/* Queue is suspended */
			blk_start_request(req);
			REQUEST_FLAGS(req) |= REQ_QUIET;
			REQUEST_FLAGS(req) |= REQ_FAILFAST_DEV 
				| REQ_FAILFAST_TRANSPORT 
				| REQ_FAILFAST_DRIVER; /* do not retry */
			REQUEST_FLAGS(req) |= REQ_FAILED;
			__blk_end_request_cur(req, -EIO);
			continue;
		}

		if (!BLK_CHECK_VALID_STATUS(req) && req->cmd_type != 
			REQ_TYPE_ATA_TASKFILE)
		{
			printk ("ndas: skip non-CMD request. cmd=%lx\n", 
				(unsigned long)REQUEST_FLAGS(req));
			continue;
		}

		if ( BLK_ATTEMPT_WRITE_RDONLY_DEVICE(req) ) {
			REQUEST_FLAGS(req) |= REQ_QUIET | REQ_FAILED | REQ_FAILFAST_DEV 
				| REQ_FAILFAST_TRANSPORT 
				| REQ_FAILFAST_DRIVER; /* do not retry */
			continue;
		}
		
		blk_start_request(req);
		spin_unlock_irq(NBLK_LOCK(q));
		err = nblk_handle_io(req);
		spin_lock_irq(NBLK_LOCK(q));

		if ( !NDAS_SUCCESS(err) ) {
			break;
		}
	}
}

int blk_init(void) 
{    
	int ret;
	nblk_kmem_cache = kmem_cache_create("ndas_bio", MAX_NBIO_SIZE, 0, 
													SLAB_HWCACHE_ALIGN, NULL);
	if (!nblk_kmem_cache) {
		printk(KERN_ERR "ndas: unable to create kmem cache\n");
		ret = -ENOMEM;
		goto errout;
	}

	if ( (ret = register_blkdev(NDAS_BLK_MAJOR, "ndas")) < 0 ) {
		printk(KERN_ERR "ndas: unable to get major %d for ndas block device\n", 
			NDAS_BLK_MAJOR);
		goto errout;
	}

	printk("ndas: registered ndas device at major number %d\n", NDAS_BLK_MAJOR);
	return 0;
errout:

	unregister_blkdev(NDAS_BLK_MAJOR, "ndas");
	
	if (nblk_kmem_cache) {
		kmem_cache_destroy(nblk_kmem_cache);
		nblk_kmem_cache = NULL;
	}
	return ret;
}

/*
 * block device clean up
 */
int blk_cleanup(void)
{
	unregister_blkdev(NDAS_BLK_MAJOR, "ndas");
	if (nblk_kmem_cache) {
		kmem_cache_destroy(nblk_kmem_cache);
		nblk_kmem_cache = NULL;
	}
	return 0;
}
