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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#include <linux/version.h>
#include <linux/module.h> 

#include <linux/delay.h>

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>
#include <linux/completion.h>

#ifdef DEBUG

int	dbg_level_sdev = DBG_LEVEL_SDEV;

#define dbg_call(l,x...) do { 								\
    if (l <= dbg_level_sdev) {								\
        printk("%s|%d|%d|",__FUNCTION__, __LINE__,l); 		\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif

#include <ndas_id.h>

#include "ndas_sdev.h"
#include "ndas_session.h"
#include "ndas_udev.h"
#include "ndas_regist.h"
#include "ndas_bdev.h"
#include "ndas_procfs.h"

#ifdef DEBUG

#define sal_assert(expr) do { NDAS_BUG_ON(!(expr)); } while(0)
#define sal_error_print(x...) do { printk(x); printk("\n"); } while(0)

#else

#define sal_assert(expr) do {} while(0)
#define sal_error_print(x...) do {} while(0)

#endif

#ifndef NDAS_QUEUE_SCHEDULER

// add NDAS_EXTRA_CFALGS 
// -DNDAS_SCHEDULER=\"noop\" 
// or one of "noop" "anticipatory" "deadline" "cfq" 

#define NDAS_QUEUE_SCHEDULER "noop"

#endif


bool slot_allocated[NDAS_MAX_SLOT] = {false};


int	sdev_get_slot(int slot) {

	int i;

	if (slot != NDAS_INVALID_SLOT) {

		NDAS_BUG_ON( slot_allocated[slot] == true );
		slot_allocated[slot] = true;

		return slot;
	}

	for (i=NDAS_FIRST_SLOT_NR; i<NDAS_MAX_SLOT; i++) {

		if (slot_allocated[i] == false) {

			slot_allocated[i] = true;
			return i;
		}
	}

	return NDAS_INVALID_SLOT;
}

void sdev_put_slot(int slot) {

	NDAS_BUG_ON( slot_allocated[slot] == false );

	slot_allocated[slot] = false;
}

bool sdev_is_slot_allocated(int slot) {

	return (slot_allocated[slot] == true);
}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else

int ndas_blksize_size[NDAS_MAX_SLOT*NDAS_NR_PARTITION] 		= {0}; 	// size of each block in bytes
int ndas_blk_size[NDAS_MAX_SLOT*NDAS_NR_PARTITION] 		= {0}; 	// device size in blocks
int	ndas_hardsect_size[NDAS_MAX_SLOT*NDAS_NR_PARTITION] = {0};	// Hardware sector of a device
int ndas_max_sectors[NDAS_MAX_SLOT*NDAS_NR_PARTITION] 	= {0};  // Max number of sectors per request

struct hd_struct ndas_hd[NDAS_MAX_SLOT*NDAS_NR_PARTITION] = {{0}};

struct gendisk ndas_gd = {

    .major 		 =	NDAS_BLK_MAJOR,
    .major_name  =  "nd",
    .minor_shift =  NDAS_PARTN_BITS,
    .max_p 		 =  (1<<NDAS_PARTN_BITS),
    .part 		 =  ndas_hd,
    .sizes 		 =  ndas_blk_size,
    .fops 		 =  &ndas_fops,
};

static void sdev_set_blk_size(int slot, int blksize, int size, int hardsectsize, int max_sectors) 
{
    int i;

    dbg_call( 1, "slot=%d\n", slot );

    ndas_blksize_size[TOMINOR(slot, 0)]  = blksize;
    ndas_blk_size[TOMINOR(slot, 0)] 	 = size;
    ndas_hardsect_size[TOMINOR(slot, 0)] = hardsectsize;
    ndas_max_sectors[TOMINOR(slot, 0)] 	 = max_sectors;

    for (i=1; i<NDAS_NR_PARTITION; i++) {

        ndas_blksize_size[TOMINOR(slot, i)]  = blksize;
        ndas_blk_size[TOMINOR(slot, i)] 	 = 0;   // Don't know size yet.
        ndas_hardsect_size[TOMINOR(slot, i)] = hardsectsize;
        ndas_max_sectors[TOMINOR(slot, i)] 	 = max_sectors;
    }

    dbg_call( 1, "ed\n" );
}

static void sdev_invalidate_slot(int slot) 
{
    int i;
    int start;

    dbg_call( 1, "slot=%d\n", slot );

    start = (slot - NDAS_FIRST_SLOT_NR) << ndas_gd.minor_shift;

	for (i = ndas_gd.max_p - 1; i>=0; i--) {

		int minor = start + i;

	    invalidate_device( MKDEV(NDAS_BLK_MAJOR, minor), 0 );

        ndas_gd.part[minor].start_sect  = 0;
        ndas_gd.part[minor].nr_sects 	= 0;
    }

    dbg_call( 1, "ed\n" );
}

ndas_error_t sdev_read_partition(int slot) 
{
    int  start ,i;
    long sectors;

    dbg_call( 1, "slot=%d\n", slot );

    start = (slot - NDAS_FIRST_SLOT_NR) << ndas_gd.minor_shift;

    for (i = ndas_gd.max_p - 1; i>=0; i--) {

        int minor = start + i;
        
		invalidate_device( MKDEV(NDAS_BLK_MAJOR, minor), 1 );
        ndas_gd.part[minor].start_sect = 0;
        ndas_gd.part[minor].nr_sects = 0;
    }

    sectors = sdev_lookup_byslot(slot)->info.logical_blocks;

    dbg_call( 3, "sectors=%ld\n", sectors );

    grok_partitions( &ndas_gd, (slot - NDAS_FIRST_SLOT_NR), 1<<NDAS_PARTN_BITS, sectors );

    for (i=0; i<NDAS_NR_PARTITION; i++) {

        dbg_call( 1, "Part %d: (%ld,%ld)\n", 
            		  i, ndas_hd[TOMINOR(slot, i)].start_sect, 
            		  ndas_hd[TOMINOR(slot, i)].nr_sects );
    }

    dbg_call( 1, "ed\n" );

    return NDAS_OK;
}

#endif

static unit_ops_t *sdev_choose_ops(int type) 
{
#ifdef XPLAT_RAID
    if (type == NDAS_DISK_MODE_RAID0) {

        return &raid0;
    } 

    if (type == NDAS_DISK_MODE_RAID1) {

        return &raid1;
    }

    if (type == NDAS_DISK_MODE_RAID4) {

        return &raid5;
    }

    if (type == NDAS_DISK_MODE_MEDIAJUKE) {

        return &udev_ops;
    }
#endif

    NDAS_BUG_ON( type != NDAS_DISK_MODE_SINGLE );

    return &udev_ops;
}

ndas_error_t sdev_set_encryption_mode(int slot, bool headenc, bool dataenc) 
{
    // Temporarily disabled.

    return NDAS_OK;
}

ndas_error_t sdev_query_slot_info(int slot, ndas_slot_info_t* info)
{
    sdev_t *sdev = sdev_lookup_byslot(slot);

    dbg_call( 1, "slot=%d sdev=%p\n", slot, sdev );

    if ( sdev == NULL ) {

        dbg_call( 1, "invalid slot=%d\n", slot );
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

    memcpy( info, &sdev->info, sizeof(ndas_slot_info_t) );

    if (sdev->info.mode == NDAS_DISK_MODE_SINGLE) {

        info->logical_blocks = sdev->info.logical_blocks - (NDAS_BLOCK_SIZE_XAREA/(info->logical_block_size/SECTOR_SIZE));
    }

    dbg_call( 3, "ed\n" );

    return NDAS_OK;
}

ndas_error_t 
sdev_set_slot_handlers(int slot, ndas_error_t (*write_func) (int slot), void* arg)
{
    sdev_t * sdev = sdev_lookup_byslot(slot);

    if ( sdev == NULL ) {

		NDAS_BUG_ON(true);
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

#ifdef NDAS_HIX
    sdev->surrender_request_handler = write_func;
    sdev->arg = arg;
#endif

    return NDAS_OK;
}

ndas_error_t sdev_query_unit(int slot, ndas_unit_info_t* info)
{
    sdev_t *sdev = sdev_lookup_byslot(slot);

    dbg_call( 3, "slot=%d sdev=%p\n", slot, sdev );

    if (sdev == NULL) {

        dbg_call( 1, "invalid slot=%d\n", slot );
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

	if (sdev->child[0]) {

	    memcpy( info, &((udev_t *)sdev->child[0])->uinfo, sizeof(ndas_unit_info_t) );
	
	} else {

		NDAS_BUG_ON(true);
		memset( info, 0, sizeof(ndas_unit_info_t) );
	}

    return NDAS_OK;
}

#if 0
static inline int sdev_is_not_recoverable(ndas_error_t err) {

    return (err != NDAS_OK							&&
			err != NDAS_ERROR_INVALID_RANGE_REQUEST &&
        	err != NDAS_ERROR_READONLY 				&&
        	err != NDAS_ERROR_READONLY 				&&
        	err != NDAS_ERROR_OUT_OF_MEMORY 		&&
        	err != NDAS_ERROR_HARDWARE_DEFECT 		&&
        	err != NDAS_ERROR_BAD_SECTOR );
}
#endif

static void sdev_io_done(ndas_request_t *ndas_req, void* arg)
{
	struct completion *completion = (struct completion *)arg;

	complete(completion);
}

static void sdev_aio_done(ndas_request_t *ndas_req, void* arg)
{
	sdev_t *sdev = (sdev_t *)arg;

	NDAS_BUG_ON( sdev->info.slot != ndas_req->slot );

	if (unlikely(ndas_req->err != NDAS_OK && ndas_req->err != NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED && 
		ndas_req->err != NDAS_ERROR_ACQUIRE_LOCK_FAILED && !NDAS_SUCCESS(ndas_req->err))) {

		sdev->io_err = ndas_req->err;

	    dbg_call( 1, "ndas_req->cmd = %d ndas_req->err = %d\n", ndas_req->cmd, ndas_req->err );

		NDAS_BUG_ON( sdev->io_err != NDAS_ERROR_NETWORK_FAIL );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		//set_bit( NDAS_QUEUE_FLAG_SUSPENDED, &sdev->queue_flags );
#endif
	}

	ndas_req->next_done --;
	ndas_req->done[ndas_req->next_done]( ndas_req, ndas_req->done_arg[ndas_req->next_done] );
	
	return;
}

ndas_error_t sdev_io(int slot, ndas_request_t *ndas_req, bool bound_check)
{
    sdev_t *sdev = sdev_lookup_byslot(slot);
    bool 	do_sync_io;

	NDAS_BUG_ON( ndas_req->slot != slot );

    if (sdev == NULL) {

		NDAS_BUG_ON(true);
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

    if (ndas_req->cmd == NDAS_CMD_WRITE && !sdev->info.writable) {

		NDAS_BUG_ON(true);
        return NDAS_ERROR_READONLY;
	}

    if (!sdev->info.enabled) {

        dbg_call( 1, "ed sdev->info.enabled=%d\n", sdev->info.enabled );
        return NDAS_ERROR_SHUTDOWN;
    }

	if (unlikely(!NDAS_SUCCESS(sdev->io_err))) {

	    dbg_call( 1, "sdev->io_err = %d\n", sdev->io_err );

		NDAS_BUG_ON( sdev->io_err != NDAS_ERROR_NETWORK_FAIL );
		return sdev->io_err;
	}

    if (bound_check) {

        if ((ndas_req->u.s.sector >= sdev->info.logical_blocks) || 
			(ndas_req->u.s.sector + ndas_req->u.s.nr_sectors > sdev->info.logical_blocks)) {

            dbg_call( 1, "ndas_req = %p, req->sector = %llu\n", ndas_req, (long long)ndas_req->u.s.sector );
            dbg_call( 1, "ndas_req->u.s.nr_sectors=%lu\n", ndas_req->u.s.nr_sectors );

			NDAS_BUG_ON(true);

            return NDAS_ERROR_INVALID_RANGE_REQUEST;
        }
    }

    dbg_call( 8, "ing start sector %llu, count %ld\n", (long long)ndas_req->u.s.sector, ndas_req->u.s.nr_sectors );
    dbg_call( 8, "req->done_arg=%p\n", ndas_req->done_arg );

    if (ndas_req->next_done == 0) {

        dbg_call( 1, "do_sync_io\n" );

        do_sync_io = TRUE;

		init_completion(&ndas_req->completion);

	    ndas_req->done[ndas_req->next_done]		 = sdev_io_done;
    	ndas_req->done_arg[ndas_req->next_done]  = &ndas_req->completion;
		ndas_req->next_done ++;
		
    } else {

        do_sync_io = FALSE;

	    ndas_req->done[ndas_req->next_done]		 = sdev_aio_done;
    	ndas_req->done_arg[ndas_req->next_done]  = sdev;
		ndas_req->next_done ++;
    }

    sdev->ops->io( sdev->child[0], ndas_req ); // uop_io, r0op_io, r1op_io

    if (do_sync_io) {

		wait_for_completion(&ndas_req->completion);

        dbg_call( 5, "ed err=%d\n", ndas_req->err );

		sdev->io_err = ndas_req->err;

	if (unlikely(ndas_req->err != NDAS_OK && ndas_req->err != NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED && 
		ndas_req->err != NDAS_ERROR_ACQUIRE_LOCK_FAILED && !NDAS_SUCCESS(ndas_req->err))) {

	    dbg_call( 1, "ndas_req->cmd = %d ndas_req->err = %d\n", ndas_req->cmd, ndas_req->err );

		sdev->io_err = ndas_req->err;
		NDAS_BUG_ON( sdev->io_err != NDAS_ERROR_NETWORK_FAIL );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		//set_bit( NDAS_QUEUE_FLAG_SUSPENDED, &sdev->queue_flags );
#endif
	}

        return ndas_req->err;
    }

    dbg_call( 5, "ed\n" );

    return NDAS_OK;
}

static ndas_error_t sdev_mount_slot(int s)
{
	sdev_t	*slot = sdev_lookup_byslot(s); 


    dbg_call( 1, "ing s#=%d slot=%p\n", s, slot );

    if (slot == NULL) {

		NDAS_BUG_ON(true);
		return NDAS_ERROR_INTERNAL;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    try_module_get(THIS_MODULE);
#else
    MOD_INC_USE_COUNT;
#endif

    dbg_call( 1, "mode=%d\n", slot->info.mode );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

    slot->disk = NULL;

    spin_lock_init( &slot->disk_lock );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
    slot->queue = blk_init_queue( bdev_request_proc, &slot->disk_lock );

    blk_queue_max_phys_segments( slot->queue, NDAS_MAX_PHYS_SEGMENTS );
    blk_queue_max_hw_segments( slot->queue, NDAS_MAX_PHYS_SEGMENTS );
    blk_queue_hardsect_size( slot->queue, slot->info.logical_block_size );
    blk_queue_max_sectors( slot->queue, NDAS_MAX_REQUEST_SECTORS/(slot->info.logical_block_size/SECTOR_SIZE));
#else
    blk_init_queue( &slot->queue, bdev_request_proc, &slot->disk_lock );

    blk_queue_max_phys_segments( &slot->queue, NDAS_MAX_PHYS_SEGMENTS );
    blk_queue_max_hw_segments( &slot->queue, NDAS_MAX_PHYS_SEGMENTS );
    blk_queue_hardsect_size( &slot->queue, slot->info.logical_block_size );
    blk_queue_max_sectors( &slot->queue, NDAS_MAX_REQUEST_SECTORS/(slot->info.logical_block_size/SECTOR_SIZE));
#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
    blk_queue_ordered( slot->queue, QUEUE_ORDERED_TAG_FLUSH, bdev_prepare_flush );
#endif

    slot->disk = alloc_disk( NDAS_NR_PARTITION );

    if (slot->disk == NULL) {

        dbg_call( 1, "fail alloc disk\n" );
		NDAS_BUG_ON(true);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		module_put(THIS_MODULE);
#else
    	MOD_DEC_USE_COUNT;
#endif

		return NDAS_ERROR_INTERNAL;
    }

    slot->disk->major = NDAS_BLK_MAJOR;
    slot->disk->first_minor = (s - NDAS_FIRST_SLOT_NR) << NDAS_PARTN_BITS;
    slot->disk->fops = &ndas_fops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
    slot->disk->queue = slot->queue;
#else
    slot->disk->queue = &slot->queue;
#endif
    slot->disk->private_data = (void*) (long)s;
    slot->queue_flags = 0;

    dbg_call( 1, "mode=%d\n", slot->info.mode );

    if (slot->info.mode == NDAS_DISK_MODE_SINGLE || 
        slot->info.mode == NDAS_DISK_MODE_ATAPI  ||
        slot->info.mode == NDAS_DISK_MODE_MEDIAJUKE) {

		char ndas_sid[NDAS_SERIAL_LENGTH + 1];
       	char ndas_short_sid[NDAS_SERIAL_SHORT_LENGTH + 1];

		ndas_id_2_ndas_sid( slot->info.ndas_id, ndas_sid );		

        if (strlen(ndas_sid) > 8) {

            /* Extended serial number is too long as sysfs object name. Use last 8 digit only */

            strncpy( ndas_short_sid,
                	 ndas_sid + ( NDAS_SERIAL_EXTEND_LENGTH - NDAS_SERIAL_SHORT_LENGTH),
                	 8 );
        } else {

            strncpy( ndas_short_sid, ndas_sid, 8 );
        }

        ndas_short_sid[8] = 0;

        snprintf( slot->devname, sizeof(slot->devname)-1, "ndas-%s-%d", ndas_short_sid, slot->info.unit[0] );

        strcpy( slot->disk->disk_name, slot->devname );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18))
#else
        strcpy( slot->disk->devfs_name, slot->devname );
#endif
        set_capacity( slot->disk, slot->info.logical_blocks );

    } else {

		NDAS_BUG_ON(true);
    }

    if (slot->info.mode == NDAS_DISK_MODE_ATAPI) {

        slot->disk->flags = GENHD_FL_CD | GENHD_FL_REMOVABLE;
    }

    dbg_call( 1, "adding disk: slot=%d, first_minor=%d, capacity=%llu\n", 
				  s, slot->disk->first_minor, slot->info.logical_blocks );

    add_disk( slot->disk );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12))

#if CONFIG_SYSFS
    sal_assert(slot->queue->kobj.ktype);
    sal_assert(slot->queue->kobj.ktype->default_attrs);
    {
        struct queue_sysfs_entry {

        	struct attribute attr;
        	ssize_t (*show)(struct request_queue *, char *);
        	ssize_t (*store)(struct request_queue *, const char *, size_t);
        	};

        struct attribute *attr = slot->queue->kobj.ktype->default_attrs[4];
        struct queue_sysfs_entry *entry = container_of(attr , struct queue_sysfs_entry, attr);

        entry->store( slot->queue, NDAS_QUEUE_SCHEDULER, strlen(NDAS_QUEUE_SCHEDULER) );
    }
#else
#error "NDAS driver doesn't work well with CFQ scheduler of 2.6.13 or above kernel." \
       "if you forcely want to use it, please specify compiler flags by " \
       "export NDAS_EXTRA_CFLAGS=\"-DNDAS_DONT_CARE_SCHEDULER\" "\
       "then compile the source again."
#endif

#endif

    printk( "ndas: /dev/%s enabled\n", slot->devname );

#else 

    dbg_call( 1, "size=%lld\n", slot->info.logical_blocks );
    dbg_call( 1, "hardsectsize=%ld\n", slot->info.logical_block_size );

    sdev_set_blk_size( s, 
					   slot->info.logical_block_size, 
					   slot->info.logical_blocks, 
					   slot->info.logical_block_size, 
					   NDAS_MAX_REQUEST_SECTORS/(slot->info.logical_block_size/SECTOR_SIZE) );

#ifdef NDAS_DEVFS
    printk( "ndas: /dev/nd/disc%d enabled\n", s - NDAS_FIRST_SLOT_NR );
#else
    printk("ndas: /dev/nd%c enabled\n", s + 'a' - NDAS_FIRST_SLOT_NR );
#endif

#endif

#ifdef NDAS_MSHARE 
    if(sdev_lookup_byslot(s)->info.mode == NDAS_DISK_MODE_MEDIAJUKE) {

  	    ndas_CheckFormat(s);
    }
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else
    sdev_read_partition(s);
#endif

    dbg_call( 3, "ed\n" );

    return NDAS_OK;
}

static void sdev_dismount_slot(int slot)
{
    sdev_t *sd = sdev_lookup_byslot(slot);

    dbg_call( 1, "ing slot=%d\n", slot );

    if (sd == NULL) {

        dbg_call( 0, "ndas: fix me at slot_disable!!\n" );
        goto out;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

    del_gendisk( sd->disk );
    put_disk( sd->disk );
    sd->disk = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
    blk_cleanup_queue( sd->queue );
    sd->queue = NULL;
#else
    blk_cleanup_queue( &sd->queue );
#endif

#else

    sdev_set_blk_size( slot, 0, 0, 0, 0 );
    sdev_invalidate_slot( slot );

#endif

#ifdef NDAS_DEVFS
//	devfs_unregister(sd->devfs_handle);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    module_put(THIS_MODULE);
#else
    MOD_DEC_USE_COUNT;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
        printk( "ndas: /dev/%s is disabled\n", sd->devname );
#else
#ifdef NDAS_DEVFS
       printk( "ndas: /dev/nd/disc%d is disabled\n", slot - NDAS_FIRST_SLOT_NR );
#else
	   printk( "ndas: /dev/nd%c is disabled\n", slot + 'a' - NDAS_FIRST_SLOT_NR );
#endif
#endif

out:
    dbg_call( 1, "ed\n" );
}

static ndas_error_t sdev_enable_internal(int slot, bool exclusive_writable, bool writeshare)
{
	ndas_error_t err;
    sdev_t 		 *sdev = (sdev_t *) sdev_lookup_byslot(slot);

    dbg_call( 1, "slot = %d, sdev=%p\n", slot, sdev );

    if (sdev == NULL) {

        dbg_call( 1, "invalid slot\n" );
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

	if (sdev->info.disk_capacity_set == false) {

        dbg_call( 1, "sdev is not yet initialized\n" );
        return NDAS_ERROR_TRY_AGAIN;
	}

    if (sdev->info.enabled && NDAS_SUCCESS(sdev->io_err)) {

        dbg_call( 1, "already enabled\n" );
        return NDAS_ERROR_ALREADY_ENABLED_DEVICE;
    }

    if ((exclusive_writable || writeshare) && sdev->info.has_key==FALSE) {

        dbg_call( 1, "No NDAS key:writemode=%d writeshare=%d has_key=%d\n", 
					  exclusive_writable, writeshare, sdev->info.has_key );

        return NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
    }

	if (!NDAS_SUCCESS(sdev->io_err)) {

		dbg_call( 1, "re-enable slot = %d sdev = %p\n", slot, sdev );

	    sdev->ops->disable( sdev->child[0], NDAS_ERROR_SHUTDOWN_IN_PROGRESS ); 
		sdev->ops->deinit(sdev->child[0]); 				// uop_deinit, r0op_deinit
	    err = sdev->ops->init(sdev, sdev->child[0]); 	// uop_init, r0op_init, r1op_init, r5op_init

	    if (!NDAS_SUCCESS(err)) {

			NDAS_BUG_ON(true);
			return err;
    	}
	}

    // udev_enable, jbod_op_enable, rddc_op_enable

    err = sdev->ops->enable( sdev->child[0], ENABLE_FLAG(exclusive_writable, writeshare, FALSE , FALSE) );

    if (!NDAS_SUCCESS(err)) {

		sal_error_print( "ndas: slot %d can't be enabled: %s\n", 
						 sdev->info.slot, NDAS_GET_STRING_ERROR(err) );

		//NDAS_BUG_ON(true);
		return err;
    }

    if (writeshare) {

        sdev->info.writeshared = TRUE;

    } else {

        sdev->info.writeshared = FALSE;
    }

    sdev->info.writable = (exclusive_writable || writeshare);

	if (NDAS_SUCCESS(sdev->io_err)) {

		sdev->info.enabled = true;
		err = sdev_mount_slot(slot);

   		if (NDAS_SUCCESS(err)) {

			__u16 bit = 0;

			if (sdev->info.writeshared) {
 
                bit = REGDATA_BIT_SHARE_ENABLED;

			} else if (sdev->info.writable) {
 
                bit = REGDATA_BIT_WRITE_ENABLED;

			} else {
 
                bit = REGDATA_BIT_READ_ENABLED;
			}

            sdev->ndev[0]->enabled_bitmap |= bit << (sdev->info.unit[0]*2);

            dbg_call( 1, "sdev->ndev->enabled_bitmap=0x%x\n", sdev->ndev[0]->enabled_bitmap );
 
		} else {

			NDAS_BUG_ON(true);
			sdev_disable_slot(slot);
    	}
	
	} else {

		sdev->io_err = NDAS_OK;
	}

    dbg_call( 1, "ed\n" );

	return err;
}

// Write-share version of ndas_enable_slot. In this mode, writing can be executed in read-only connection.
// And writing is executed only when NDAS device lock is held.(not implemented)

ndas_error_t sdev_cmd_enable_writeshare(int slot)
{
    return sdev_enable_internal(slot, FALSE, TRUE);
}

ndas_error_t sdev_cmd_enable_exclusive_writable(int slot)
{
    return sdev_enable_internal(slot, TRUE, FALSE);
}

ndas_error_t sdev_cmd_enable_slot(int slot)
{
    return sdev_enable_internal(slot, FALSE, FALSE);
}

ndas_error_t sdev_disable_slot(int slot)
{
    sdev_t  *sdev = sdev_lookup_byslot(slot);
	__u16 	bit = 0;

    dbg_call( 1, "slot=%d\n", slot );

    if (sdev == NULL) {

        dbg_call( 1, "Invalid slot number %d\n", slot );
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

    if (!sdev->info.enabled) {

        dbg_call( 1, "Trying to disable disabled slot\n" );

        return NDAS_ERROR_ALREADY_DISABLED_DEVICE; // todo:
    }

	if (sdev->info.writeshared) {
 
		bit = REGDATA_BIT_SHARE_ENABLED;

	} else if (sdev->info.writable) {
 
    	bit = REGDATA_BIT_WRITE_ENABLED;

	} else {
 
    	bit = REGDATA_BIT_READ_ENABLED;
	}

    sdev->ndev[0]->enabled_bitmap &= ~(bit << (sdev->info.unit[0]*2));

	dbg_call( 1, "sdev->ndev->enabled_bitmap=0x%x\n", sdev->ndev[0]->enabled_bitmap );

    sdev_dismount_slot(slot);

    sdev->ops->disable( sdev->child[0], NDAS_ERROR_SHUTDOWN_IN_PROGRESS ); 

	sdev->info.enabled = false;

    return NDAS_OK;
}

sdev_t *sdev_create(sdev_create_param_t* sparam) 
{
    ndas_error_t 	err;
    sdev_t 		 	*sdev;
	unsigned char	i;

    dbg_call( 1, "ing mode=%d\n",  sparam->mode );

	if (sparam->slot >= NDAS_FIRST_SLOT_NR) {

		sdev = sdev_lookup_byslot(sparam->slot);

		if (sdev) {

			NDAS_BUG_ON(true);
	        dbg_call( 1, "already created sdev %d\n", sparam->slot );
    	    return sdev;
    	}
	}

 	sdev = ndas_kmalloc( sizeof(sdev_t) );

    if (!sdev) {
 
        return NULL;
	}

    memset( sdev, 0, sizeof(sdev_t) );

#ifdef DEBUG
    sdev->magic = SDEV_MAGIC;
#endif

    sema_init( &sdev->sdev_mutex, 1 );

    sdev->ops = sdev_choose_ops(sparam->mode);
	
	sdev->info.slot = sparam->slot;
    sdev->info.mode = sparam->mode;

	sdev->info.has_key 	= sparam->has_key;

	strncpy( sdev->info.ndas_id, sparam->ndas_id, NDAS_ID_LENGTH+1 );

	sdev->info.logical_block_size 	= sparam->logical_block_size;
	sdev->info.logical_blocks 		= sparam->logical_blocks;

	NDAS_BUG_ON( sdev->info.logical_block_size != SECTOR_SIZE );

    sdev->info.conn_timeout = NDAS_RECONNECT_TIMEOUT;

	sdev->info.nr_child	= sparam->nr_child;

	for (i=0; i<sdev->info.nr_child; i++) {

		sdev->info.child_slot[i] = sparam->child[i].slot;
		sdev->info.unit[i]		 = sparam->child[i].unit;

		sdev->info.headers_per_disk[i] 	   = sparam->child[i].headers_per_disk;
		sdev->info.sectors_per_cylinder[i] = sparam->child[i].sectors_per_cylinder;
		sdev->info.cylinders_per_header[i] = sparam->child[i].cylinders_per_header;

		sdev->ndev[i]  = sparam->child[i].ndev;
		sdev->child[i] = sparam->child[i].udev;
	}

	sdev->info.disk_capacity_set = true;

    err = sdev_register(sdev);

    if (!NDAS_SUCCESS(err)) {

        goto out2;
    }

    err = sdev->ops->init(sdev, sdev->child[0]); // uop_init, r0op_init, r1op_init, r5op_init

    if (!NDAS_SUCCESS(err)) {

        goto out3;
    }

    nproc_add_slot( sdev );

    dbg_call( 1, "ed sdev=%p\n", sdev );

    return sdev;

out3:
    sdev_unregister(sdev);
out2:
    sdev->child[0] = NULL;
	sdev->info.nr_child = 0;

    kfree(sdev);

    dbg_call( 1, "err = %d\n", err );

    return NULL;
}

void sdev_cleanup(sdev_t *sdev, void *arg) 
{
    dbg_call( 1, "slot=%d\n", sdev->info.slot );

    nproc_remove_slot(sdev);

    if (sdev->info.enabled) {

		NDAS_BUG_ON(true);
		sdev_disable_slot( sdev->info.slot );
    }

	sdev->ops->deinit(sdev->child[0]); 		// uop_deinit, r0op_deinit
    sdev->child[0] = NULL;
	sdev->info.nr_child = 0;
    sdev_unregister(sdev);

	ndas_kfree(sdev);

    return;
}

