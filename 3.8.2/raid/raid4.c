#include "xplatcfg.h"
#ifdef XPLAT_RAID

#include <sal/mem.h>
#include <ndasuser/ndaserr.h>
#include <ndasuser/def.h>
#include <ndasuser/info.h>
#include <ndasuser/io.h>

#include "netdisk/conn.h"
#include "netdisk/sdev.h"
#include "netdisk/ndev.h" // udev todo remove this 

#include "raid/raid.h"
#include "raid/bitmap.h"

#include "redundancy.h"
#include "raid4.h"


#define BITMAP_IO_UNIT (32*1024)
#define BITMAP_SECTOR(disk_size, bit_index) \
    ( (disk_size) +  NDAS_BLOCK_LOCATION_BITMAP + (bit_index) / 8 / 512)

#ifdef DEBUG
#define    debug_5(l, x...) \
do { if(l <= DEBUG_LEVEL_RAID4) {\
    sal_debug_print("R5|%d|%s|",l,__FUNCTION__); sal_debug_println(x);    \
} } while(0)        
#define    debug_5r(l, x...) \
do { if(l <= DEBUG_LEVEL_RAID_RECOVER) {\
    sal_debug_print("R5R|%d|%s|",l,__FUNCTION__); sal_debug_println(x);    \
} } while(0)        
#else
#define    debug_5(l, x...) do {} while(0)
#define    debug_5r(l, x...) do {} while(0)
#endif

#ifdef DEBUG
#define R5IO_MAGIC 0x555555dd
#define R5IO_ASSERT(rio) ({\
    sal_assert(rio);\
    TIR_ASSERT((rio)->tir); \
    sal_assert((rio)->magic == R5IO_MAGIC);\
})
#else
#define R1IO_ASSERT(tir) do {} while(0)
#endif

struct r5io_s {
#ifdef DEBUG
    int magic;
#endif
    sal_atomic    count;
    int                nr_io;
    int             err;
    int             using_parity;
    /* the index to the disk that is not used in the I/O*/
    int                unused;
    urb_ptr tir;
    logunit_t     *sdev;
    struct r5io_s *next;
    struct sal_mem_block *parity_uio;
};

LOCAL
void r5op_io(logunit_t *sdev, urb_ptr tir, xbool urgent);
LOCAL
ndas_error_t r5op_query(logunit_t * sdev, ndas_metadata_t *nmd);
LOCAL
NDAS_CALL void r5_has_read(int slot, ndas_error_t err,void* user_arg);
LOCAL
NDAS_CALL void r5_has_written(int slot, ndas_error_t err,void* user_arg);

LOCAL
urb_ptr subtir_alloc(logunit_t *sdev, urb_ptr tir,int index) 
{
    int shift_bits = bitmap_ffb(SDEV_RAID_INFO(sdev)->min_disks);
    urb_ptr subtir = tir_alloc(tir->cmd, &tir->req);
    
    if ( !subtir ) return NULL;
    
    subtir->req->nr_uio = tir->req->nr_uio >> shift_bits;
    subtir->req->done = tir->cmd == ND_CMD_READ ? r5_has_read : r5_has_written;
    subtir->req->done_arg = subtir;
    subtir->req->num_sec = tir->req->num_sec >> shift_bits;
    subtir->req->start_sec = tir->req->start_sec >> shift_bits;
    
    if ( index < SDEV_RAID_INFO(sdev)->min_disks) 
    {
        subtir->req->uio = tir->req->uio + index * subtir->req->nr_uio;
    }  
    
    return subtir;
}
LOCAL
void puio_free(struct sal_mem_block *blocks, int nr_uio)
{
    int i;
    for ( i = 0; i < nr_uio; i++)
        sal_free(blocks[i].ptr);
    sal_free(blocks);
}
LOCAL
struct sal_mem_block *puio_alloc(int nr_uio, int block_size) 
{
    int i;
    struct sal_mem_block * ret;
    ret = sal_malloc(sizeof(struct sal_mem_block) * nr_uio);
    if ( !ret ) return NULL;
    for ( i = 0; i < nr_uio; i++) {
        ret[i].ptr = sal_malloc(block_size);
        if ( !ret[i].ptr )
            goto err;
        ret[i].len = block_size;
        sal_memset(ret[i].ptr, 0, ret[i].len);
    }
    return ret;
err:
    while(--i>=0) {
        sal_free(ret[i].ptr);
    }
    sal_free(ret);
    return NULL;
}

LOCAL
struct r5io_s *
r5io_alloc(logunit_t *sdev, urb_ptr tir) 
{
    struct r5io_s *rio = sal_malloc(sizeof(struct r5io_s));
    if ( !rio ) 
        return NULL;
    
#ifdef DEBUG
    rio->magic = R5IO_MAGIC;
#endif
    rio->tir = tir;
    rio->sdev = sdev;
    sal_atomic_set(&rio->count, 0);
    rio->err = NDAS_OK;
    rio->nr_io = 0;
    rio->using_parity = 0;
    rio->unused = -1;
    rio->parity_uio = NULL;
    debug_5(5,"rio=%p", rio);
    return rio;
}

LOCAL void r5io_free(struct r5io_s* rio)
{
    debug_5(5,"rio=%p", rio);
    sal_free(rio);
}

LOCAL
void r5op_deinit(logunit_t *sdev) 
{
    int i;
    for ( i = 0 ; i < SDEV_RAID_INFO(sdev)->disks; i++ ) {
        if ( sdev->disks[i] && sdev->disks[i]->private )  {
            sal_free(sdev->disks[i]->private);
            sdev->disks[i]->private = NULL;
        }
    }
    if ( sdev->nmd) 
        nmd_destroy(sdev->nmd);
    sdev->nmd = NULL;
    rddc_free((struct rddc_s *) SDEV_RAID(sdev)->private);
    SDEV_RAID(sdev)->private = NULL;
    raid_struct_free(SDEV_RAID(sdev));
    sdev->private = NULL;
}

#if 0 /* not used */
LOCAL
xbool r5_apply_rmd(logunit_t *sdev) 
{
    int i = 0;
    logunit_t **tmp;

    sal_assert(sdev);
    sal_assert(sdev->nmd);
    sal_assert(sdev->nmd->rmd);
    if ( sdev->nmd->rmd->u.s.magic != RMD_MAGIC )
        return TRUE; // invalid RMD, ignore
    
    tmp = sal_malloc(SDEV_RAID_INFO(sdev)->disks * sizeof(logunit_t *));
    
    if ( !tmp ) 
        return FALSE;
    
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++)
    {
        int idx = sdev->nmd->rmd->UnitMetaData[i].iUnitDeviceIdx;
        if ( idx >= SDEV_RAID_INFO(sdev)->disks ) {
            goto out; // invalid RMD, ignore
        }
        tmp[i] = sdev->disks[idx];
        debug_5(5, "[%d]->[%d]", i, idx);
        debug_5(5, "%d:%p", i, sdev->disks[idx]);
    }
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        sdev->disks[i] = tmp[i];
    }
out:
    sal_free(tmp);
    return TRUE;
}
#endif

/*
 * called by sdev_create
 */
LOCAL
ndas_error_t r5op_init(logunit_t *sdev)
{
    ndas_error_t err = NDAS_ERROR_OUT_OF_MEMORY;
        
    debug_5(3, "slot=%d", sdev->info.slot);
    debug_5(3, "SDEV_RAID_INFO(sdev)->disks=%d", SDEV_RAID_INFO(sdev)->disks);

    SDEV_RAID_INFO(sdev)->min_disks = SDEV_RAID_INFO(sdev)->disks - 1;
    sdev->private = raid_struct_alloc();
    if ( !sdev->private )
        goto out;
    
    SDEV_RAID(sdev)->private = rddc_alloc(SDEV_RAID_INFO(sdev)->disks);

    if ( !SDEV_RAID(sdev)->private ) 
        goto out;
    
    err = nmd_init_rmd(sdev->nmd);
    if ( !NDAS_SUCCESS(err) ) {
        goto out;
    }
    
    err = r5op_query(sdev, sdev->nmd);
    if ( !NDAS_SUCCESS(err) )
    {
        if ( err == NDAS_ERROR_TOO_MANY_INVALID || err == NDAS_ERROR_TOO_MANY_OFFLINE)
            return NDAS_OK;
        goto out;
    }
    
    return NDAS_OK;
out:
    r5op_deinit(sdev);
    return err;
}

/* 
 */
LOCAL
ndas_error_t r5op_query(logunit_t * sdev, ndas_metadata_t *nmd) // uop_query
{
    int i = 0, count = 0;
    xbool has_key = TRUE;
    ndas_error_t err = NDAS_ERROR_OUT_OF_MEMORY;
    unsigned int min_mspr = -1;
    ndas_metadata_t *nmd_local; // local use
    
    debug_5(3, "slot=%d", sdev->info.slot);

    debug_5(5, "# of disks=%d", SDEV_RAID_INFO(sdev)->disks);

    sal_assert(sdev->private);
    
    nmd_local = nmd_create();
    if ( !nmd_local ) {
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++)
    {
        logunit_t *child = sdev->disks[i]; 
        ndas_metadata_t *n = ( count == 0 ) ? nmd : nmd_local;
        debug_5(6, "child=%p", child);
        
        if ( !child ) 
            continue;
        
        err = child->ops->query(child, n); // uop_query
            
        if ( !NDAS_SUCCESS(err) ) {
            debug_5(1, "%d:query err=%d", i, err);
            continue;
        }

        if ( !dib_compare(nmd->dib, n->dib) )
        {
            err = NDAS_ERROR_INVALID_METADATA;
            goto out;
        }
        
        if ( min_mspr > child->info.max_sectors_per_request )
            min_mspr = child->info.max_sectors_per_request;
        has_key &= child->has_key;
        count ++;
    }
    if ( count < SDEV_RAID_INFO(sdev)->min_disks) {
        err = NDAS_ERROR_TOO_MANY_OFFLINE;
        goto out;
    }
    sdev->has_key = has_key;
    sdev->info.io_splits = nmd->dib->u.s.nDiskCount - 1;
    sdev->info.sectors_per_disk = nmd->dib->u.s.sizeUserSpace;
    sdev->info.sectors = sdev->info.sectors_per_disk * (sdev->info.io_splits);
    sdev->info.capacity = sdev->info.sectors << 9;
    sdev->info.mode = NDAS_DISK_MODE_RAID4;
    sdev->info.sector_size = sdev->info.io_splits << 9;
    sdev->info.max_sectors_per_request = min_mspr;
    
    rddc_conf(sdev)->sectors_per_bit_shift = bitmap_ffb((unsigned long)nmd->dib->u.s.iSectorsPerBit);
    
    err = NDAS_OK;
    debug_5(5, "shift=%d", rddc_conf(sdev)->sectors_per_bit_shift);
    debug_5(5, "iSectorsPerBit=%d", nmd->dib->u.s.iSectorsPerBit);
    debug_5(5, "capacity_low=%Lu", sdev->info.capacity);
    debug_5(5, "sector_size=%u", sdev->info.sector_size);
out:    
    nmd_destroy(nmd_local);
    return err;
}

LOCAL
int r5op_status(logunit_t *sdev)
{
    int i = 0; 
    int status = 0;
    for ( i = 0 ; i < SDEV_RAID_INFO(sdev)->disks; i ++ )
    {
        if ( sdev->disks[i] ) {
        status |= sdev->disks[i]->ops->status(sdev->disks[i]); // uop_status
        }
    }
    
    if ( status & CONN_STATUS_CONNECTED) 
        return CONN_STATUS_CONNECTED;
    
    if ( status & CONN_STATUS_CONNECTING )
        return CONN_STATUS_CONNECTING;
    
    if ( status & CONN_STATUS_SHUTING_DOWN )
        return CONN_STATUS_SHUTING_DOWN;
    
    return CONN_STATUS_INIT;
}

LOCAL void r5_end_read(logunit_t *sdev, struct r5io_s *rio)
{
    urb_ptr tir = rio->tir;
    ndas_error_t err;
    int i,j, n = SDEV_RAID_INFO(sdev)->min_disks,
            target = rio->unused;
    
    sal_assert(tir->cmd == ND_CMD_READ);
    
    if ( !rio->using_parity || !NDAS_SUCCESS(rio->err) )
        goto out;
    
    sal_assert(target >= 0);
    sal_assert(rio->nr_io == SDEV_RAID_INFO(sdev)->min_disks);
    
    debug_5(5, "construct %dth disk from the others", target);
    for (i = 0; i < SDEV_RAID_INFO(sdev)->min_disks; i++) 
    {
        struct sal_mem_block *dst = tir->req.uio + target * (tir->req.nr_uio /n);
        struct sal_mem_block *src = tir->req.uio + i * (tir->req.nr_uio / n); 
        
        if ( i == target )
            continue;
        
        for (j = 0; j < tir->req.nr_uio / n; j++ ) 
        {
            memxor(dst[j].ptr, src[j].ptr, src[j].len);
        }
    }
    
out:    
    rio->tir->req.done(sdev->info.slot, rio->err, rio->tir->req.done_arg);
    tir_free(rio->tir);
    err = rio->err;
    r5io_free(rio);
    sal_atomic_dec(&rddc_conf(sdev)->nr_queued_io);
}

LOCAL inline
xbool read_retryable(logunit_t *sdev, struct r5io_s *rio)
{
    return rio->unused == SDEV_RAID_INFO(sdev)->min_disks 
        && !rio->using_parity;
}


LOCAL
int r5_fail_read(urb_ptr subtir, void *arg)
{
    long slot = (long)arg;
    ndas_error_t err;
    struct r5io_s *rio = (struct r5io_s *) subtir->arg;
    logunit_t *sdev = rio->sdev;
    int index = raid_find_index(sdev,slot);

    debug_5(1, "err=%d", err);
    debug_5(5, "rio->using_parity=%d", rio->using_parity);

    SDEV_REENTER(sdev, r5_fail_read, subtir, (void*) slot);

    err = rddc_member_disable(sdev, index);
    if ( !NDAS_SUCCESS(err) ) 
    {
        sal_assert(sdev_is_locked(sdev));
        rio->err = err;
        sdev_unlock(sdev);
        goto out;
    }
    sal_assert(!sdev_is_locked(sdev));
    if ( read_retryable(sdev, rio) ) 
    { 
        urb_ptr ptir = subtir_alloc(sdev, rio->tir, index);
        
        if ( ptir ) { // TODO race
            int i = rio->unused;
            debug_5(5, "fail index=%d retry index=%d", index, i);
            ptir->arg = rio;
            rio->using_parity = 1;
            rio->unused = index;
            sdev->disks[i]->ops->io(sdev->disks[i], ptir,FALSE);
            return 0;
        }
        
    }
    rio->err = err;
    
out:    
    if ( sal_atomic_inc(&rio->count) == rio->nr_io ) {
        r5_end_read(sdev, rio);
    }

	return 0;
}


LOCAL
void r5_has_read(int slot, ndas_error_t err,void* arg) 
{
    urb_ptr subtir = (urb_ptr) arg;
    struct r5io_s *rio = (struct r5io_s *) subtir->arg;
    logunit_t *sdev = rio->sdev;
    
    debug_5(5, "slot=%d/%d err=%d subtir=%p", slot, sdev->info.slot, err, subtir);
    debug_5(6, "uio=%p", subtir->req.uio->ptr);
    
    if ( NDAS_SUCCESS(err)) {
        if ( sal_atomic_inc(&rio->count) == rio->nr_io )
            r5_end_read(sdev, rio);
        return;
    }
    debug_5(5,"err=%d", err);

    r5_fail_read(subtir,(void *)slot);
}

LOCAL
void r5_end_write(urb_ptr subtir)
{
    ndas_error_t err;
    struct r5io_s *rio = (struct r5io_s *) subtir->arg;
    logunit_t *sdev = rio->sdev;
    
    if ( sal_atomic_inc(&rio->count) < rio->nr_io ) 
        return;
    
    rio->tir->req.done(sdev->info.slot, rio->err, rio->tir->req.done_arg);
    if ( rio->parity_uio ) {
        puio_free(rio->parity_uio, subtir->req.nr_uio);
    }
    tir_free(rio->tir);
    err = rio->err;
    r5io_free(rio);
    sal_atomic_dec(&rddc_conf(sdev)->nr_queued_io);
}
LOCAL
int r5_fail_write(urb_ptr subtir, void * arg)
{
    long slot = (long)arg;
    struct r5io_s *rio = (struct r5io_s *) subtir->arg;
    logunit_t *sdev = rio->sdev;
    int index = raid_find_index(sdev,slot);
    ndas_error_t err;
    
    debug_5(1, "fail slot=%d index=%d", slot, index);

    SDEV_REENTER(sdev, r5_fail_write, subtir, (void*) slot);
    
    err = rddc_member_disable(sdev, index);
    if ( !NDAS_SUCCESS(err) )
    {   
        rio->err = err;
        sdev_unlock(sdev);
    }
    r5_end_write(subtir);

	return 0;
}
/*
 * r5_has_written
 * 
 * 
*/
LOCAL
NDAS_CALL void r5_has_written(int slot, ndas_error_t err,void* user_arg) 
{
    urb_ptr subtir = (urb_ptr)user_arg;
    struct r5io_s *rio = (struct r5io_s *) subtir->arg;
    logunit_t *sdev = rio->sdev;
    debug_5(5, "slot=%d/%d err=%d subtir=%p tir=%p", slot, sdev->info.slot, err, subtir, rio->tir);
    
    if ( !NDAS_SUCCESS(err)) 
    {
        rddc_mark_bitmap(sdev, &rio->tir->req, bitmap_ffb(SDEV_RAID_INFO(sdev)->min_disks));
        r5_fail_write(subtir, (void *)slot);
        return;
    }
    r5_end_write(subtir);
}

LOCAL
void r5_io_error(logunit_t *sdev, urb_ptr tir, ndas_error_t err)
{
    tir->req.done(sdev->info.slot, err, tir->req.done_arg);
    tir_free(tir);
    sal_atomic_dec(&rddc_conf(sdev)->nr_queued_io);
}
LOCAL 
int farest_disk(logunit_t *sdev, ndas_io_request_ptr req) 
{
    int shift;
    int d = rddc_conf(sdev)->defect;
    int isdirty;
    bitmap_t *b= (bitmap_t *) sdev->nmd->bitmap;
    if ( d >= 0 ) return d;

    d = rddc_conf(sdev)->target;
    shift = bitmap_ffb(SDEV_RAID_INFO(sdev)->min_disks) + rddc_conf(sdev)->sectors_per_bit_shift;

    isdirty = b && ( bitmap_isset(b, req->start_sec >> shift) 
            || bitmap_isset(b, (req->start_sec + req->num_sec - 1 )>> shift) );
    
    if (  d >= 0 && isdirty )
        return d;
    
    return SDEV_RAID_INFO(sdev)->min_disks;
}

LOCAL
void r5_read(logunit_t *sdev, urb_ptr tir)
{
    int i = 0, count = 0;
    struct r5io_s *rio = NULL;
    urb_ptr subtir;
    debug_5(5,"slot=%d", sdev->info.slot);
    
    sal_assert(tir->cmd == ND_CMD_READ || tir->cmd == ND_CMD_VERIFY);
    
    rio = r5io_alloc(sdev, tir);
    if ( !rio ) {
        r5_io_error(sdev, tir, NDAS_ERROR_OUT_OF_MEMORY);
        return;
    }
    rio->unused = farest_disk(sdev, &tir->req);
    rio->nr_io = SDEV_RAID_INFO(sdev)->min_disks;
    rio->using_parity = rio->unused < rio->nr_io;
    
    for ( i = 0; i < rio->nr_io + 1 && count < rio->nr_io; i++) 
    {
        int index = i;
        if ( !sdev->disks[i] )
            continue;
        
        if ( i == rio->unused ) 
        {
            index = rio->nr_io; /* read the parity disk*/
        }
        /* 
         * If i is 'parity disk', use the uio for unused disk, 
         *  then later we will construct the data for the unused disk.
         */
        subtir = subtir_alloc(sdev, tir, i == rio->nr_io ? rio->unused : i);
        
        if ( !subtir ) 
        {
            rio->err = NDAS_ERROR_OUT_OF_MEMORY;
            if ( sal_atomic_inc(&rio->count) == rio->nr_io ) 
            {
                r5_end_read(sdev, rio);
                return;
            }
            debug_5(1, "out of mem skip index=%d i=%d defect=%d", index,
                i, rddc_conf(sdev)->defect);
            continue;
        }

        subtir->arg = rio;
        
        debug_5(8, "[%d]req->uio=%p req->nr_uio=%d req->start_sec=%Lu num_sec=%u", 
            index,
            subtir->req.uio, 
            subtir->req.nr_uio, 
            subtir->req.start_sec, 
            subtir->req.num_sec
        );
        
        sdev->disks[index]->ops->io(sdev->disks[index], subtir, FALSE); 
        count++;
    }

    return;
}

LOCAL
void r5_write(logunit_t *sdev, urb_ptr tir)
{
    int i, c = 0, defect;
    struct r5io_s *rio = NULL;
    urb_ptr subtir;
    int shift_bits = bitmap_ffb(SDEV_RAID_INFO(sdev)->min_disks);
    debug_5(3,"slot=%d", sdev->info.slot);
    
    sal_assert(tir->cmd == ND_CMD_WRITE || tir->cmd == ND_CMD_WV ||  tir->cmd == ND_CMD_FLUSH);
    
    rio = r5io_alloc(sdev, tir);

    if ( !rio ) {
        r5_io_error(sdev, tir, NDAS_ERROR_OUT_OF_MEMORY);
        return;
    }

    rio->parity_uio = puio_alloc(tir->req.nr_uio >> shift_bits, 512);

    if ( !rio->parity_uio ) {
        r5io_free(rio);
        r5_io_error(sdev, tir, NDAS_ERROR_OUT_OF_MEMORY);
        return;
    }    
    
    rio->nr_io = sal_atomic_read(&rddc_conf(sdev)->running);

    sal_assert(rio->nr_io >= SDEV_RAID_INFO(sdev)->min_disks);

    defect = rddc_conf(sdev)->defect;
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        if ( i < SDEV_RAID_INFO(sdev)->min_disks) 
        {
            int j;
            int sz_segment = tir->req.nr_uio >> shift_bits;
            struct sal_mem_block *uio = tir->req.uio  + i * sz_segment;
            
            for ( j = 0; j < sz_segment; j++)
            {
                sal_assert(rio->parity_uio[j].len == uio[j].len);
                
                memxor(rio->parity_uio[j].ptr, 
                    uio[j].ptr,
                    uio[j].len);
            }
        }
        
        if ( defect == i ) {
            rddc_mark_bitmap(sdev, &tir->req, bitmap_ffb(SDEV_RAID_INFO(sdev)->min_disks));
            debug_5(1, "skip index=%d", i);
            continue;
        }
        subtir = subtir_alloc(sdev, tir, i);
        if ( !subtir) 
        {
            rio->err = NDAS_ERROR_OUT_OF_MEMORY;
            rddc_mark_bitmap(sdev, &subtir->req, 0);
            if ( sal_atomic_inc(&rio->count) == rio->nr_io ) 
            {
                tir->req.done(sdev->info.slot, rio->err, tir->req.done_arg);
                r5io_free(rio);
                tir_free(rio->tir);
                return;
            }
            debug_5(1, "out of mem skip index=%d defect_disk_index=%d", 
                i, defect);
            continue;
        }
        if ( ++c > rio->nr_io ) {
            sal_assert(c <= rio->nr_io);
            debug_5(1, "too many requests i=%d c=%d", i,c);
            debug_5(1, "rddc_conf(sdev)->running=%d", sal_atomic_read(&rddc_conf(sdev)->running));
            debug_5(1, "rio->nr_io=%d", rio->nr_io);
            tir_free(subtir);
            continue;
        }
            
        subtir->arg = rio;
        
        if ( i == SDEV_RAID_INFO(sdev)->min_disks) 
        {
            subtir->req.uio = rio->parity_uio;
        } 
        
        debug_5(5, "[%d]req->uio=%p req->nr_uio=%d req->start_sec=%Lu num_sec=%u", 
            i,
            subtir->req.uio, 
            subtir->req.nr_uio, 
            subtir->req.start_sec, 
            subtir->req.num_sec
        );
        
        sdev->disks[i]->ops->io(sdev->disks[i], subtir, FALSE); 
    }
}

#define SECTOR_CONFLICT(x,nr_sec_x, y, nr_sec_y) ( (y) < ((x) + (nr_sec_x)) && (x) < ((y) + (nr_sec_y)))


/* 
 * called by ndas_io
 */
LOCAL
void r5op_io(logunit_t *sdev, urb_ptr tir, xbool urgent)
{
    debug_5(3, "sdev=%p tir=%p ", sdev, tir);
    debug_5(5, "req->uio=%p req->nr_uio=%d req->start_sec=%Lu num_sec=%u", 
        tir->req.uio, 
        tir->req.nr_uio, 
        tir->req.start_sec, 
        tir->req.num_sec);
    TIR_ASSERT(tir);
    sal_atomic_inc(&rddc_conf(sdev)->nr_queued_io);
    sal_assert( sdev->info.enabled );
    sal_assert( sal_atomic_read(&rddc_conf(sdev)->running) >= SDEV_RAID_INFO(sdev)->min_disks);
    
    if ( rddc_is_locked(sdev) ) 
    {  // double locking to avoid mutex every time
        sal_spinlock_take_softirq(rddc_conf(sdev)->mutex);
        if ( rddc_is_locked(sdev) ) 
        {
            if ( SECTOR_CONFLICT(rddc_conf(sdev)->rcvr->start,
                    rddc_conf(sdev)->rcvr->nr_sectors,
                    tir->req.start_sec, tir->req.num_sec) ) 
            {
                debug_5(1, "conflict with locked area[%Lu,%Lu), req=[%Lu,%Lu)",
                    rddc_conf(sdev)->rcvr->start, 
                    rddc_conf(sdev)->rcvr->start
                        + rddc_conf(sdev)->rcvr->nr_sectors,
                    tir->req.start_sec, 
                    tir->req.start_sec 
                        + tir->req.num_sec);
                
                list_add(&tir->queue, &rddc_conf(sdev)->rcvr->pending);
                sal_atomic_dec(&rddc_conf(sdev)->nr_queued_io);
                sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
                return;
            }
        }
        sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
    }
    if ( tir->cmd == ND_CMD_READ || tir->cmd == ND_CMD_VERIFY)
        r5_read(sdev, tir);
    else 
        r5_write(sdev, tir);
    return;
}

struct ops_s raid5 = {
    .create_disks = (create_disks_func) raid_create,
    .destroy_disks = (destroy_disks_func) raid_cleanup,
    .init = (init_func) r5op_init,
    .deinit = (deinit_func) r5op_deinit,
    .query = (query_func) r5op_query,
    .enable = (enable_func) rddc_op_enable,
    .disable = (disable_func) rddc_op_disable,
    .io = (io_func) r5op_io,
    .status = r5op_status,
    .changed = rddc_device_changed,
};

#endif // XPLAT_RAID

