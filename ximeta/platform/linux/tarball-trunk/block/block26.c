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
#include <linux_ver.h>
#if LINUX_VERSION_25_ABOVE
#include <linux/module.h> // THIS_MODULE
#include <ndasuser/ndasuser.h>
#include <ndasuser/io.h>
#include <sal/debug.h>
#include <ndasdev.h>
#include <procfs.h>
#include "block.h"
#include "ops.h"

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
#include <linux/ide.h>
#endif
//
// IDE command code
//
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

    nbio->req = req;
    nbio->nr_blocks = 0;
//    nbio->dev = NDAS_GET_SLOT_DEV(slot);

    rq_for_each_segment(bvec, req, iter)
    {
        int j = bvec->bv_len; 
        data = page_address(bvec->bv_page) + bvec->bv_offset;
        while ( j > 0 ) 
        {
            int idx = SPLIT_IDX_SLOT(nbio->nr_blocks, req->nr_sectors, slot); 
            nbio->blocks[idx].len = chunk_size;
            nbio->blocks[idx].ptr = data;
            j -= chunk_size;
            data += chunk_size;
            nbio->nr_blocks++;
        }
        len += bvec->bv_len;
    }
    
    nbio->nr_sectors = len >> 9;
    sal_assert( nbio->nr_sectors == req->nr_sectors );
    return nbio;
}
static inline 
struct nbio_linux* nbio_alloc(struct request *req) 
{
    int len = 0;
    struct req_iterator iter;
    struct bio_vec* bvec;
    struct nbio_linux *nbio = kmem_cache_alloc(nblk_kmem_cache, GFP_ATOMIC);
            /* size: NBIO_SIZE(req->nr_phys_segments) */
    if ( !nbio ) return NULL;

//    printk(KERN_ERR "req: sec:%u nr:%lu bio:%p\n", req->sector, req->nr_sectors, req->bio);

    nbio->req = req;
    nbio->nr_blocks = 0;
    //
	// Convert the request to nbio.
	//
    rq_for_each_segment(bvec, req, iter)
    {
        char *data;
#ifdef CONFIG_HIGHMEM
        if ( is_highmem(page_zone(bvec->bv_page)) )
        {
            // Set page to private to unmap later.
            nbio->blocks[nbio->nr_blocks].private = bvec->bv_page;
            nbio->blocks[nbio->nr_blocks].ptr = kmap(bvec->bv_page) + bvec->bv_offset;
            nbio->blocks[nbio->nr_blocks].len = bvec->bv_len;
            nbio->nr_blocks++;
            printk(KERN_INFO "kmap(%p) called.\n", bvec->bv_page);
        }
        else
        {
#endif

            data = page_address(bvec->bv_page) + bvec->bv_offset;
            if ( NBIO_ADJACENT(nbio,data) )
                nbio->blocks[nbio->nr_blocks - 1].len += bvec->bv_len;
            else {
#ifdef CONFIG_HIGHMEM
                nbio->blocks[nbio->nr_blocks].private = NULL;
#endif
                nbio->blocks[nbio->nr_blocks].len = bvec->bv_len;
                nbio->blocks[nbio->nr_blocks].ptr = data;
                nbio->nr_blocks++;
            }
#ifdef CONFIG_HIGHMEM
        }
#endif

        len += bvec->bv_len;

        //
        // Fail if no more memblocks is remaining.
        //
        if( nbio->nr_blocks > ND_BLK_MAX_REQ_SEGMENT &&
            (len >> 9) < req->nr_sectors)
        {
            printk(KERN_ERR "Too many memblock. %d %d %d %d %d %d\n",
            	nbio->nr_blocks, req->nr_phys_segments, req->nr_hw_segments, len, (int)req->nr_sectors, ND_BLK_MAX_REQ_SEGMENT);
            kmem_cache_free(nblk_kmem_cache ,nbio);
            return NULL;
        }
    } // rq_for_each_segment
    nbio->nr_sectors = len >> 9;
    sal_assert( nbio->nr_sectors == req->nr_sectors);
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


#ifdef DEBUG

static
unsigned int
_calc_checksum(unsigned int checksum, unsigned char *data, unsigned int data_len)
{
    int idx_data_int;

    for(idx_data_int = 0; idx_data_int < data_len / sizeof(unsigned int); idx_data_int++) {
        checksum += *((unsigned int *)data + idx_data_int);
    }
    return checksum;
}


static
void
_dump_ioreq(struct request *req) 
{
#if 0
    struct req_iterator iter;
    struct bio_vec* bvec;
#endif
    unsigned int checksum = 0;

#if 0
    rq_for_each_segment(bvec, req, iter)
    {
#ifdef CONFIG_HIGHMEM
        if ( is_highmem(page_zone(bvec->bv_page)) )
        {
            checksum = _calc_checksum(
                checksum,
                kmap(bvec->bv_page) + bvec->bv_offset,
                bvec->bv_len);
        }
        else
        {
#endif
            checksum = _calc_checksum(
                checksum,
                page_address(bvec->bv_page) + bvec->bv_offset,
                bvec->bv_len);

#ifdef CONFIG_HIGHMEM
        }
#endif

    } // rq_for_each_segment
#endif
    printk(KERN_ERR "end: sec:%u nr:%lu cs:%x bio:%p\n", req->sector, req->nr_sectors, checksum, req->bio);
}


#include "linux/buffer_head.h"
static
inline
void
_print_request(struct request *req)
{
    struct bio *bio;
    struct buffer_head *bh;
    void *b_end_io, *bi_end_io;
    int i, corrupt;

    bio = (struct bio *)req->bio;
    bh = bio->bi_private;
    bi_end_io = bio->bi_end_io;
    b_end_io = bh->b_end_io;
    corrupt = 0;
    for(i = 0; bio != NULL ; i++, bio = bio->bi_next) {
        bh = bio->bi_private;
        if(b_end_io != bh->b_end_io || bi_end_io != bio->bi_end_io) {
            printk("[%d] %p %p %p %p %p:", i, bio, bio->bi_next, bio->bi_end_io, bh, bh->b_end_io);
            sal_debug_hexdump((void *)bh, sizeof(struct buffer_head));
            // fix-up
            bh->b_end_io = b_end_io;
            corrupt = 1;
        }
    }
    if(corrupt) {
        printk("\n");
        *(char *)0 = 0;
    }
}
#endif

static
inline
void
_end_io(ndas_error_t err, struct request *req, size_t nr_sectors) 
{
    unsigned long flags;
    struct request_queue *q = req->q;
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
    int error = NDAS_SUCCESS(err) ? 0 : -EIO;
#else
    int uptodate = NDAS_SUCCESS(err) ? 1 : 0;
#endif

#if 0
    _dump_ioreq(req);
#endif

    spin_lock_irqsave(q->queue_lock, flags);
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
    if ( error ) {
#else
    if ( !uptodate ) {
#endif
        printk("ndas: IO error occurred at sector %d(slot %d): %s\n", 
            (int)req->sector,
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

//    _print_request(req);
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
    __blk_end_request(req, error, nr_sectors << 9);
#else
     if(!end_that_request_first(req, uptodate, nr_sectors)) {
        END_THAT_REQUEST_LAST(req, uptodate);
    } 
#endif
    spin_unlock_irqrestore(q->queue_lock, flags);
}
/* signalled when the NDAS io is completed 
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

//    _print_request(req);

    if ( sd->info.io_splits == 1 )  {
        nbio = nbio_alloc(req);
    } else {
        nbio = nbio_alloc_splited(req);
    }
    if (unlikely( !nbio )) {
        printk("ndas: Out of memory at sector %d(slot %d): %s\n", 
            (int)req->sector,
            SLOT_R(req),
            NDAS_GET_STRING_ERROR(NDAS_ERROR_OUT_OF_MEMORY));
        _end_io(NDAS_ERROR_OUT_OF_MEMORY, req, req->nr_sectors);
        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    if (unlikely(!sd->enabled)) {
        err =  NDAS_ERROR_NOT_CONNECTED;
        printk("ndas: Not connected %d(slot %d): %s\n", 
            (int)req->sector,
            SLOT_R(req),
            NDAS_GET_STRING_ERROR(err));
        goto out;
    }

    ndas_req = &nbio->ndas_req;
    ndas_req->uio = nbio->blocks;
    ndas_req->buf = NULL;
    ndas_req->nr_uio = nbio->nr_blocks;
    ndas_req->num_sec = nbio->nr_sectors;
    ndas_req->start_sec = req->sector;

#ifndef NDAS_SYNC_IO
    ndas_req->done = (ndas_io_done) nblk_end_io;
    ndas_req->done_arg = nbio;
#else
    ndas_req->done = NULL;
    ndas_req->done_arg = NULL;
#endif

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
   if ( req->cmd_type == REQ_TYPE_ATA_TASKFILE )	
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
    if(req->cmd_type == REQ_TYPE_ATA_TASK)
#else
    if(REQUEST_FLAGS(req) & (REQ_DRIVE_TASK))
#endif
    {
        switch(req->cmd[0]) {
            case IDE_CMD_FLUSH_CACHE:
#ifdef DEBUG
                printk ("ndas: flushing with flags=%lx\n", (unsigned long)REQUEST_FLAGS(req));
#endif
                err = ndas_flush(slot, ndas_req);
            break;
            default:
                err = NDAS_ERROR_INVALID_OPERATION;
                printk("ndas: SCSI operation 0x%x is not supported.\n", (unsigned int)req->cmd[0]);
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
        printk("ndas: operation %u is not supported.\n", (unsigned int)REQ_DIR(req));
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
        if (test_bit(NDAS_FLAG_QUEUE_SUSPENDED, &(NDAS_GET_SLOT_DEV(SLOT_R(req))->queue_flags)))  {
            printk ("ndas: Queue is suspended\n");
            /* Queue is suspended */
            blkdev_dequeue_request(req);
            REQUEST_FLAGS(req) |= REQ_QUIET;
            REQUEST_FLAGS(req) |= REQ_FAILFAST; /* do not retry */
            REQUEST_FLAGS(req) |= REQ_FAILED;
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
	     __blk_end_request(req, -EIO, req->nr_sectors << 9);
#else
            while (end_that_request_first(req, 0, req->nr_sectors))
                     ;
            END_THAT_REQUEST_LAST(req, 0);
#endif
            continue;
        }

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
	if (!BLK_CHECK_VALID_STATUS(req) && req->cmd_type != REQ_TYPE_ATA_TASKFILE)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
        if (!BLK_CHECK_VALID_STATUS(req) && req->cmd_type != REQ_TYPE_ATA_TASK)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
        if (!BLK_CHECK_VALID_STATUS(req) && !(REQUEST_FLAGS(req) & (REQ_DRIVE_TASK)))
#else
        if (!BLK_CHECK_VALID_STATUS(req))
#endif
        {
            printk ("ndas: skip non-CMD request. cmd=%lx\n", (unsigned long)REQUEST_FLAGS(req));
            end_request(req, 0);

            continue;
        }
        if ( BLK_ATTEMPT_WRITE_RDONLY_DEVICE(req) ) {
            REQUEST_FLAGS(req) |= REQ_QUIET | REQ_FAILFAST | REQ_FAILED;
            end_request(req, 0);

            continue;
        }
        
        blkdev_dequeue_request(req);
        spin_unlock_irq(NBLK_LOCK(q));
        err = nblk_handle_io(req);
        spin_lock_irq(NBLK_LOCK(q));

        if ( !NDAS_SUCCESS(err) ) {
            break;
		}
    }
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
void nblk_prepare_flush(request_queue_t *q, struct request *req)
{
//    printk("prepare_flush\n");
    memset(req->cmd, 0, sizeof(req->cmd));

    // Indicate IDE task command reqest.
    // We adopt IDE task command for NDAS block device's flush command.
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
    req->cmd_type = REQ_TYPE_ATA_TASKFILE;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
    req->cmd_type = REQ_TYPE_ATA_TASK;
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

int blk_init(void) 
{    
    int ret;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23))
    nblk_kmem_cache = kmem_cache_create("ndas_bio", MAX_NBIO_SIZE, 0, SLAB_HWCACHE_ALIGN,
        NULL);
#else
    nblk_kmem_cache = kmem_cache_create("ndas_bio", MAX_NBIO_SIZE, 0, SLAB_HWCACHE_ALIGN,
        NULL, NULL);
#endif
    if (!nblk_kmem_cache) {
        printk(KERN_ERR "ndas: unable to create kmem cache\n");
        ret = -ENOMEM;
        goto errout;
    }

    if ( (ret = register_blkdev(NDAS_BLK_MAJOR, "ndas")) < 0 ) {
        printk(KERN_ERR "ndas: unable to get major %d for ndas block device\n", NDAS_BLK_MAJOR);
        goto errout;
    }

#ifdef NDAS_HOTPLUG
    /* register NDAS bus */
    ret = bus_register(&ndas_bus_type);
    if ( ret ) {        
        printk("ndas: Unable to register bus ndas\n");    
        goto errout;
    }
    ndas_bus_type.devices.hotplug_ops = &nd_hotplug_ops;
    /* register NDAS driver */
    ret = driver_register(&v_ndas_driver.driver);
    if ( ret ) {
        printk("ndas: Unable to register driver ndas\n");
        goto errout;
    }
    driver_create_file(&v_ndas_driver.driver,&driver_attr_version);
#endif    
    
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


#ifdef NDAS_HOTPLUG
/* ndas device */

struct ndas_driver {
    struct device_driver driver;
};
/*
 * Set up "version" as a driver attribute.
 */
static ssize_t nddriver_show_version(struct device_driver *drv, char *buf)
{
    strcpy(buf, "1.1");
    return strlen("1.1") + 1;
}

static DRIVER_ATTR(version, S_IRUGO, nddriver_show_version, NULL);

static inline struct ndas_slot* to_ndas_dev(struct device* dev)
{
    return container_of(dev,struct ndas_slot, dev);
}

static int ndas_bus_match(struct device * dev, struct device_driver * drv)
{
    return 0;
}
static struct device * ndas_bus_add(struct device * parent, char * bus_id) {
    return NULL;
    
}
static int ndas_bus_hotplug (struct device *dev, char **envp, 
                int num_envp, char *buffer, int buffer_size)
{
    return 0;
}
static int ndas_bus_suspend(struct device * dev, u32 state)
{
    return 0;

}
static int ndas_bus_resume(struct device * dev)
{
    return 0;

}

static int nd_probe       (struct device * dev) {
    return 0;
}
static int nd_remove      (struct device * dev){
    return 0;
}
static void nd_stop     (struct device * dev){
}
static int nd_suspend     (struct device * dev, u32 state, u32 level){
    return 0;
}
static int nd_resume      (struct device * dev, u32 level){
    return 0;
}
struct bus_type ndas_bus_type = {
    .name = "ndas",
    .match = ndas_bus_match,
    .add = ndas_bus_add,    
    .hotplug = ndas_bus_hotplug,    
    .suspend = ndas_bus_suspend,
    .resume = ndas_bus_resume,
};
EXPORT_SYMBOL(ndas_bus_type);
static struct ndas_driver v_ndas_driver = {
    .driver = {
        .name = "ndas",
        .bus = &ndas_bus_type,
        .probe = nd_probe,
        .remove = nd_remove,
        .shutdown = nd_stop,
        .suspend = nd_suspend,
        .resume = nd_resume
    },
};

int nddev_filter(struct kset *kset, struct kobject *kobj)
{
    struct kobj_type *ktype = get_ktype(kobj);
    if (ktype == ndas_bus_type.devices.ktype) {
        struct device *dev = container_of(kobj, struct device,kobj); 
        if (dev->bus)
            return 1;
    }
    return 0;
}
char* nddev_name(struct kset *kset, struct kobject *kobj)
{
    struct device *dev = container_of(kobj, struct device,kobj);
    return dev->bus->name; 
}
int nddev_hotplug(struct kset *kset, struct kobject *kobj, char **envp,
        int num_envp, char *buffer, int buffer_size)
{
    //struct usb_interface *intf;
    struct ndas_slot *nd_adapter_dev;
    char *scratch;
    int i = 0;
    int length = 0;
    struct device *dev = container_of(kobj, struct device,kobj);
    
    /* ... */
    nd_adapter_dev = container_of(dev,struct ndas_slot, dev);
//    intf = to_usb_interface(dev);
//    usb_dev = interface_to_usbdev(intf);

    /* ... */
    scratch = buffer;
    envp[i++] = scratch;
    length += snprintf(scratch, buffer_size - length, "PRODUCT=%x",
               nd_adapter_dev->slot);
    if ((buffer_size - length <= 0) || (i >= num_envp))
        return -ENOMEM;
    ++length;
    scratch += length;

    /* ... */
    envp[i++] = NULL;
    return 0;
}

static struct kset_hotplug_ops nd_hotplug_ops = {
    .filter = nddev_filter,
    .name = nddev_name,
    .hotplug = nddev_hotplug,
};
#endif
#endif // 2.6

