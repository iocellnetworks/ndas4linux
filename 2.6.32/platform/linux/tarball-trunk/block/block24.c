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
#if !LINUX_VERSION_25_ABOVE
#include <linux/config.h>
#include <linux/module.h> // THIS_MODULE
#include <linux/slab.h> /* kmalloc, kfree */
#include <ndasuser/ndasuser.h>
#include <ndasuser/io.h>
#include <sal/debug.h>
#include "ndasdev.h"

#include "block.h"
#include "ops.h"
#include "procfs.h"
#include "mshare.h"

/*
  * Setup some definitions for linux/blk.h
  */
#define MAJOR_NR NDAS_BLK_MAJOR
#include <linux/major.h>
#if( MAJOR_NR == NDAS_BLK_MAJOR)
#undef    DEVICE_NAME
#define    DEVICE_NAME    "ndasdevice"
#undef    DEVICE_REQUEST
#define DEVICE_REQUEST    nblk_request_proc
#define DEVICE_NR(device) (MINOR(device) >> PARTN_BITS)
#undef    DEVICE_ON
#define DEVICE_ON(device)
#undef    DEVICE_OFF
#define DEVICE_OFF(device)
#endif
#include <linux/blk.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20))
#include "linux/compiler.h"
#endif

struct hd_struct  ndas_hd[256];
int            ndas_sizes[256]; // size of disk in kilobytes
int            ndas_blksizes[256]; // size of each block in bytes
int            ndas_hardsectsizes[256];  // Hardware sector of a device
int            ndas_max_sectors[256];   // Max number of sectors per request

//#define MAX_READ_AHEAD 48
//int             ndas_max_readahead[256];

struct block_device_operations ndas_fops = {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
    .owner =            THIS_MODULE, 
#endif
    .open =            ndop_open, 
    .release =        ndop_release, 
    .ioctl =            ndop_ioctl, 
    .check_media_change =    ndop_check_netdisk_change, 
    .revalidate =        ndop_revalidate
};

KMEM_CACHE* nblk_kmem_cache;

/* forward definitions */
ndas_error_t nblk_handle_io(struct request *req) ;

void ndas_ops_set_blk_size(int slot, int blksize, int size, int hardsectsize, int max_sectors) 
{
    int i;

    ndas_blksizes[TOMINOR(slot, 0)] = blksize;
    ndas_sizes[TOMINOR(slot, 0)] = size/2; // convert ndas_sizes is in 1k unit.
    ndas_hardsectsizes[TOMINOR(slot, 0)] = hardsectsize;
    ndas_max_sectors[TOMINOR(slot, 0)] = max_sectors;
    
    for(i=1; i<NR_PARTITION; i++) {
        ndas_blksizes[TOMINOR(slot, i)] = blksize;
        ndas_sizes[TOMINOR(slot, i)] = 0;   // Don't know size yet.
        ndas_hardsectsizes[TOMINOR(slot, i)] = hardsectsize;
        ndas_max_sectors[TOMINOR(slot, i)] = max_sectors;
    }
}

static struct nbio_linux* nbio_alloc_splited(struct request *req)
{
    int j;
    char *data;
    struct buffer_head *bh;
    int len = 0, chunk_size = 1 << 9;
    int slot = SLOT_R(req);
    struct nbio_linux *nbio = nblk_mem_alloc();

    if ( !nbio ) return NULL;

    nbio->req = req;
    nbio->nr_blocks = 0;
    for (bh = req->bh; bh; bh = bh->b_reqnext) 
    {
        j = bh->b_size; 
        data = bh->b_data;
        while ( j > 0 ) 
        {
            int idx = SPLIT_IDX_SLOT(nbio->nr_blocks, req->nr_sectors, slot);
            nbio->blocks[idx].len = chunk_size;
            nbio->blocks[idx].ptr = data;
            j -= chunk_size;
            data += chunk_size;
            nbio->nr_blocks++;
        }
        len += bh->b_size;
    }
    nbio->nr_sectors = len >> 9;
    dbgl_blk(6, "nbio->nr_blocks=%d nbio->nr_sectors=%d", 
        nbio->nr_blocks, nbio->nr_sectors);
    if ( nbio->nr_sectors != req->nr_sectors ) {
        dbgl_blk(1,"io->nr_sectors=%d", nbio->nr_sectors);
        dbgl_blk(1,"req->nr_sectors=%ld", req->nr_sectors);
    }
    return nbio;
}
static inline 
struct nbio_linux* nbio_alloc(struct request *req) 
{
    int len = 0;
    struct buffer_head *bh;
    struct nbio_linux *nbio = nblk_mem_alloc();

    dbgl_blk(8, "nr_segments=%d", req->nr_segments);

    if (unlikely(!nbio))
        return NULL;
    if(unlikely(req->nr_segments > ND_BLK_MAX_REQ_SEGMENT)) {
        return NULL;
    }

    nbio->req = req;
    nbio->nr_blocks = 0;

    for (bh = req->bh; bh; bh = bh->b_reqnext) 
    {
        if ( NBIO_ADJACENT(nbio,bh->b_data) )
        {
            nbio->blocks[nbio->nr_blocks - 1].len += bh->b_size;
            dbgl_blk(8, "Merged. nr_blocks=%d", nbio->nr_blocks);
        } else {
            nbio->blocks[nbio->nr_blocks].len = bh->b_size;
            nbio->blocks[nbio->nr_blocks].ptr = bh->b_data;
            nbio->nr_blocks++;
        }
        len += bh->b_size;
        dbgl_blk(8, "nr_blocks: %d, len=%ld", nbio->nr_blocks,  (long)bh->b_size); 
    }
    nbio->nr_sectors = len >> 9;;
    dbgl_blk(4, "nr_segments=%d nr_sectors=%d nr_blocks=%d", req->nr_segments, nbio->nr_sectors, nbio->nr_blocks);
    return nbio;
}

/* 
 * signalled when the NDAS io is completed 
 */
 static
inline
void
_end_io(ndas_error_t err, struct request *req, size_t nr_sectors) 
{
    unsigned long flags;
    int uptodate = NDAS_SUCCESS(err) ? 1 : 0;

    dbgl_blk(3,"ing err=%d, req=%p nr_sectors=%d", err, req, nr_sectors);

    if ( !uptodate ) {
        printk("ndasdevice: IO error %s occurred for sector %lu\n", 
            NDAS_GET_STRING_ERROR(err),
            req->sector + ndas_hd[MINOR(req->rq_dev)].start_sect);
        req->errors++;
    }
    spin_lock_irqsave(&io_request_lock, flags);
    while(end_that_request_first(req, uptodate, DEVICE_NAME));
    end_that_request_last(req);
    spin_unlock_irqrestore(&io_request_lock, flags);
}

NDAS_CALL 
void nblk_end_io(int slot, ndas_error_t err,struct nbio_linux *nbio)
{
    struct request *req = nbio->req;

    dbgl_blk(3,"ing err=%d, req=%p nr_sectors=%d nr_blocks=%d", err, req, nbio->nr_sectors, nbio->nr_blocks);

    _end_io(err, req, nbio->nr_sectors);

    nblk_mem_free(nbio);

    dbgl_blk(3,"ed");
}

ndas_error_t nblk_handle_io(struct request *req)
{
    ndas_error_t err;
    ndas_io_request *ndas_req;
    int slot = SLOT_R(req);
    int minor = MINOR(req->rq_dev);
    struct ndas_slot *sd = NDAS_GET_SLOT_DEV(slot);
    struct nbio_linux *nbio;

    dbgl_blk(6, "requested to slot %d", sd->slot);

    if (unlikely(!sd->enabled)) {
        _end_io(NDAS_ERROR_NOT_CONNECTED, req, req->nr_sectors);
        return NDAS_ERROR_NOT_CONNECTED;
    }

    if (sd->info.io_splits == 1)
        nbio = nbio_alloc(req);
    else
        nbio = nbio_alloc_splited(req);

    if (unlikely(!nbio) ) {
        _end_io(NDAS_ERROR_OUT_OF_MEMORY, req, req->nr_sectors);
        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    ndas_req = &nbio->ndas_req;
    ndas_req->num_sec = nbio->nr_sectors;// << __ffs(sd->info.io_splits);
    ndas_req->start_sec = req->sector + ndas_hd[minor].start_sect;
    ndas_req->nr_uio = nbio->nr_blocks;
    ndas_req->uio = nbio->blocks;
    ndas_req->buf = NULL;
#ifdef NDAS_SYNC_IO
    ndas_req->done = NULL; // Synchrounos IO : sjcho temp
#else
    ndas_req->done = (ndas_io_done) nblk_end_io; 
    ndas_req->done_arg = nbio;
#endif

    dbgl_blk(6, 
        "req(%p)=bh(%p)={cmd=%s, nr_uio=%d, nr_sectors=%d, start sector=%Lu max_sectors[60][%d]=%d}",
        req,
        req->bh, 
        (req->cmd == READ || req->cmd == READA) ?"READ":"WRITE",
        ndas_req->nr_uio, 
        ndas_req->num_sec,
        ndas_req->start_sec,
        minor,
        hardsect_size[MAJOR_NR][minor]);

    /* Returns error for out-of-bound access */
    if (unlikely(req->sector + ndas_req->num_sec > ndas_hd[minor].nr_sects)) {
       printk("ndas: tried to access area that exceeds the partition boundary\n");
       err = NDAS_ERROR_INVALID_OPERATION;
    } else {
        switch ( REQ_DIR(req)) {
        case READ:
        case READA:
            err = ndas_read(slot, ndas_req);
            break;
        case WRITE:
            err = ndas_write(slot, ndas_req);
            break;
        default:
            err = NDAS_ERROR_INVALID_OPERATION;
            printk("ndas: operation %d is not supported.\n", REQ_DIR(req));
        }
    }
#ifdef NDAS_SYNC_IO
    nblk_end_io(slot, err, nbio);
#else
    if ( !NDAS_SUCCESS(err) ) {
        nblk_mem_free(nbio);
    }
#endif
    return err;
}

#define NBLK_LOCK(q) (&io_request_lock)

/* block request procedure. 
   Assume this function is entered with io_request_lock held 
*/
void nblk_request_proc(request_queue_t *q) 
{
    struct request *req;
    ndas_error_t err = NDAS_OK;
    while((req = NBLK_NEXT_REQUEST(q)) != NULL) 
    {
        if (unlikely(!NDAS_GET_SLOT_DEV(SLOT_R(req))->enabled)) {
            printk ("ndas: requested to disabled device\n");
            end_request(0);
            continue;
        }
        if (unlikely( ! BLK_CHECK_VALID_STATUS(req) )) {
            printk ("ndas: skip non-CMD request\n");
            end_request(0);
            continue;
        }
       /* if ( BLK_ATTEMPT_WRITE_RDONLY_DEVICE(req) ) {
            end_request(0);
            continue;
        }*/ 

        blkdev_dequeue_request(req);

        spin_unlock_irq (NBLK_LOCK(q));

#ifdef  NDAS_MSHARE
        err = nblk_handle_juke_io(req);
#else
        err = nblk_handle_io(req);
#endif
        spin_lock_irq (NBLK_LOCK(q));

        if (unlikely( !NDAS_SUCCESS(err) )) {
            printk("ndas: IO error %s occurred for sector %ld\n",
                            NDAS_GET_STRING_ERROR(err),
                            req->sector + ndas_hd[MINOR(req->rq_dev)].start_sect);
            req->errors++;
            while(end_that_request_first(req, 0, DEVICE_NAME));
            end_that_request_last(req);
            continue;
        }
    }
}

int blk_init(void) 
{
    printk("ndas: nbio size = %d ndas max segs = %d linux max segs = %d\n",
        (int)MAX_NBIO_SIZE, ND_BLK_MAX_REQ_SEGMENT, MAX_SEGMENTS);

    if(devfs_register_blkdev(MAJOR_NR, "ndasdevice", &ndas_fops)) {
        printk(KERN_ERR "Unable to get major %d for ndas\n", MAJOR_NR);
        return -EBUSY;
    }

    nblk_kmem_cache = kmem_cache_create("ndas_bio", MAX_NBIO_SIZE, 0, SLAB_HWCACHE_ALIGN,
        NULL, NULL);
    if (!nblk_kmem_cache) {
        devfs_unregister_blkdev(NDAS_BLK_MAJOR, "ndasdevice");
        printk(KERN_ERR "ndas: unable to create kmem cache\n");
        return -ENOMEM;
    }
    
    blksize_size[MAJOR_NR] = ndas_blksizes;
    blk_size[MAJOR_NR] = ndas_sizes;
    max_sectors[MAJOR_NR] = ndas_max_sectors;
    hardsect_size[MAJOR_NR] = ndas_hardsectsizes;
    blk_dev[MAJOR_NR].queue = NULL;
    blk_init_queue(BLK_DEFAULT_QUEUE(MAJOR_NR), nblk_request_proc);
    blk_queue_headactive(BLK_DEFAULT_QUEUE(MAJOR_NR), 0);

    printk("ndas: registered ndas device at major number %d\n", NDAS_BLK_MAJOR);
    return 0;
}

/*
 * block device clean up
 */
int blk_cleanup(void)
{
    devfs_unregister_blkdev(NDAS_BLK_MAJOR, "ndasdevice") ;
    if (nblk_kmem_cache) {
        kmem_cache_destroy(nblk_kmem_cache);    
        nblk_kmem_cache = NULL;
    }
    return 0;
}

#ifdef NDAS_MSHARE


#include <ndasuser/mediaop.h>


int get_req_size(struct request * req, unsigned int max_req_size)
{
    struct buffer_head * bh;
    unsigned int len = 0, slen = 0;
    bh = req->bh;
    while(bh) {
        slen = bh->b_size;
        if(len + slen > max_req_size) break;
        len += slen;
        bh = bh->b_reqnext;
    }

    return len;
}

////////////////////////////////////////////////////////////////////
//
//
//      encryp / decrypt function
//
//
//
////////////////////////////////////////////////////////////////////

void encrypt_req( int slot, int partnum, struct request * req)
{
    struct buffer_head * bh;
    void * key = NULL;

    key = ndas_getkey(slot, partnum);
    if(key == NULL) return;
    
    bh = req->bh;
    while(bh) {
        ndas_encrypt(key, bh->b_data, bh->b_data, bh->b_size);
        bh = bh->b_reqnext;
    }
    return ;
}


void decrypt_req(int slot, int partnum, struct request * req)
{
    struct buffer_head * bh;
    void * key = NULL;
    
    key = ndas_getkey(slot, partnum);
    if(key == NULL) return;

    bh = req->bh;
    while(bh) {
        ndas_decrypt(key, bh->b_data, bh->b_data, bh->b_size);
        bh = bh->b_reqnext;
    }
    return ;
}




struct nbio_linux* nbio_alloc_with_offset(struct request *req, unsigned int offset, unsigned int req_size) 
{
    int len = 0;
    struct buffer_head *bh;
    int slot = SLOT_R(req);
    struct nbio_linux *nbio = nblk_mem_alloc();
            /* size: NBIO_SIZE(req->nr_phys_segments) */
    unsigned int max_sectors = NDAS_GET_SLOT_DEV(slot)->info.max_sectors_per_request;

    unsigned int total_chk_size = 0;
    
    unsigned int mg = 0;
    dbgl_blk(8, "nr_segments=%d", req->nr_segments);

    if ( !nbio ) return NULL;
    
    nbio->req = req;
    nbio->nr_blocks = 0;
//    nbio->dev = NDAS_GET_SLOT_DEV(slot);

    if(offset > 0){
        unsigned int bSearch = 0;
        for(bh = req->bh; bh; bh = bh->b_reqnext)
        {
            if(bSearch != 1){
                if( (total_chk_size + bh->b_size) > offset ) {
                    mg = total_chk_size + bh->b_size - offset;
                }else{
                    total_chk_size += bh->b_size;
                    continue;
                }

                if(bSearch != 1){
                    bSearch  = 1;
                    if(len + mg > max_sectors * 1024) {
                        dbgl_blk(1, "ed len=%d/%d nr_uio=%d", len, max_sectors*1024 , nbio->nr_blocks);
                        break;
                    }

                    if(len + mg > req_size) {
                        dbgl_blk(1, "ed len=%d/req_size%d nr_uio=%d", len, req_size , nbio->nr_blocks);
                        break;
                    }
                    
                    nbio->blocks[nbio->nr_blocks].len = mg;
                    nbio->blocks[nbio->nr_blocks].ptr = (bh->b_data + mg);
                    nbio->nr_blocks++;
                    len += mg;
                    if ( bh->b_reqnext == NULL ) 
                        dbgl_blk(8, "ed len=%d/%d nr_uio=%d", len, max_sectors*1024,nbio->nr_blocks );
                    }
            }else {
                    if(len + bh->b_size > max_sectors*1024) {
                        dbgl_blk(1, "ed len=%d/%d nr_uio=%d", len, max_sectors*1024 , nbio->nr_blocks);
                        break;
                    }

                    if(len + mg > req_size) {
                        dbgl_blk(1, "ed len=%d/req_size%d nr_uio=%d", len, req_size , nbio->nr_blocks);
                        break;
                    }
                    
                    if ( NBIO_ADJACENT(nbio,bh->b_data) )
                    {
                        nbio->blocks[nbio->nr_blocks - 1].len += bh->b_size;
                    } else {
                        nbio->blocks[nbio->nr_blocks].len = bh->b_size;
                        nbio->blocks[nbio->nr_blocks].ptr = bh->b_data;
                        nbio->nr_blocks++;
                    }
                    len += bh->b_size;
                    if ( bh->b_reqnext == NULL ) 
                        dbgl_blk(8, "ed len=%d/%d nr_uio=%d", len, max_sectors*1024,nbio->nr_blocks );

            }
        }
    }else{
    
        for (bh = req->bh; bh; bh = bh->b_reqnext) 
        {   
            if(len + bh->b_size > max_sectors*1024) {
                dbgl_blk(1, "ed len=%d/%d nr_uio=%d", len, max_sectors*1024 , nbio->nr_blocks);
                break;
            }

            if(len + mg > req_size) {
                dbgl_blk(1, "ed len=%d/req_size%d nr_uio=%d", len, req_size , nbio->nr_blocks);
                break;
            }
            
            if ( NBIO_ADJACENT(nbio,bh->b_data) )
            {
                nbio->blocks[nbio->nr_blocks - 1].len += bh->b_size;
            } else {
                nbio->blocks[nbio->nr_blocks].len = bh->b_size;
                nbio->blocks[nbio->nr_blocks].ptr = bh->b_data;
                nbio->nr_blocks++;
            }
            len += bh->b_size;
            if ( bh->b_reqnext == NULL ) 
                dbgl_blk(8, "ed len=%d/%d nr_uio=%d", len, max_sectors*1024,nbio->nr_blocks );
        }
    }
    nbio->nr_sectors = len >> 9;;
    return nbio;
}



ndas_error_t nblk_handle_juke_io(struct request *req) 
{
    ndas_error_t err = NDAS_OK;
    ndas_io_request request;
    int slot = SLOT_R(req);
    int partnum =PARTITION(req->rq_dev); 
    struct ndas_slot *sd = NDAS_GET_SLOT_DEV(slot);
    struct nbio_linux *nbio = NULL;
    

    unsigned int    req_sectors = 0;
    unsigned int    start_sector = 0;   
    unsigned int    org_start_sector = 0,  org_req_sectors =  0;
    unsigned int    chk_start_sector = 0, chk_req_sectors = 0;
    unsigned int    max_req_size = (NDAS_GET_SLOT_DEV(slot)->info.max_sectors_per_request * 1024) ;
    unsigned int    processed_sector_count = 0;

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,22))
    if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE  || partnum == 0){
#else
    if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE ){
#endif
        return nblk_handle_io(req);
    }


    if(!ndas_IsDiscSet(slot,partnum))
    {
        if(partnum == 0){
            printk("end reqest by ilgu\n");
                    while(end_that_request_first(req, 1, DEVICE_NAME));
                        end_that_request_last(req);
            return 0;
        }
        printk("Disc is not set!\n");
        return NDAS_ERROR_JUKE_DISC_NOT_SET;
    }
    

    chk_start_sector =org_start_sector = req->sector ;
    chk_req_sectors =org_req_sectors =  (get_req_size(req, max_req_size)/512);

    //printk("call is not encrypt!\n");
    if(REQ_DIR(req) == WRITE)  encrypt_req(slot, partnum,req);
    
    
    while(chk_req_sectors > 0){
            // step 1  : address translation
        if(!ndas_TranslateAddr(slot,partnum,chk_start_sector, chk_req_sectors, &start_sector, &req_sectors))
        {
            printk("can't translate address org_start(%d), org_sec(%d)\n", chk_start_sector, chk_req_sectors);
            return NDAS_ERROR_JUKE_DISC_ADDR_NOT_FOUND;
        }
        //printk("call is ndas_TranslateAddrorg_start(%d), org_sec(%d), req_start(%d), req_sec(%d)\n", chk_start_sector, chk_req_sectors, start_sector, req_sectors);
        // step 2 : alloc linux io req
        nbio = nbio_alloc_with_offset(req, (processed_sector_count * 512), (req_sectors * 512)); //  req, offset, req_size
    
        if ( !nbio ) {
            return NDAS_ERROR_OUT_OF_MEMORY;
        }

        if (!sd->enabled) {
            kmem_cache_free(nblk_kmem_cache ,nbio);
            printk("Requesting to disabled device\n");
            return NDAS_ERROR_NOT_CONNECTED;
        }

    
        //printk("IOR:%d\n", nbio->nr_sectors);
        request.uio = nbio->blocks;
        request.buf = NULL;
        request.nr_uio = nbio->nr_blocks;
        request.num_sec = nbio->nr_sectors;
        request.start_sec = start_sector;
        request.done = NULL; // Synchrounos IO : sjcho temp
        
        dbgl_blk(8, 
            "req(%p)=bh(%p)={cmd=%s, nr_uio=%d, nr_sectors=%d, start sector=%Lu}",
            req,
            req->bh, 
            (req->cmd == READ || req->cmd == READA) ?"READ":"WRITE",
            request.nr_uio, 
            request.num_sec,
            request.start_sec);

        switch ( REQ_DIR(req)) {
        case READ:
        case READA:
            //printk("ndas_read!\n");
            err = ndas_read(slot,&request);
            break;
        case WRITE:
            err = ndas_write(slot,&request);
            break;
        default:
            err = NDAS_ERROR_INVALID_OPERATION;
            printk("ndas: operation %d is not supported.\n", REQ_DIR(req));
        }
        //printk("after_call!\n");
        processed_sector_count += nbio->nr_sectors;
        chk_start_sector += nbio->nr_sectors;
        chk_req_sectors  -=nbio->nr_sectors;

        if(processed_sector_count == org_req_sectors) break;
        else kmem_cache_free(nblk_kmem_cache ,nbio);
    }   
    
    if(( REQ_DIR(req) == READ) || ( REQ_DIR(req) == READA))  decrypt_req(slot, partnum,req);
    

    nblk_end_io(slot, err, nbio);

    return err; 
}
#endif /*#ifdef NDAS_MSHARE */

#endif /* !LINUX_VERSION_25_ABOVE */

