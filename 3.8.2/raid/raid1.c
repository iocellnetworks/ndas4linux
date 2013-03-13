#include "xplatcfg.h"

#ifdef XPLAT_RAID
#include <sal/mem.h>
#include <ndasuser/ndaserr.h>
#include <ndasuser/def.h>
#include <ndasuser/info.h>
#include "raid/raid.h"
#include "raid/bitmap.h"
#include "raid1.h"
#include "netdisk/conn.h"
#include "netdisk/sdev.h"

#include "redundancy.h"

#define BITMAP_IO_UNIT (32*1024)
#define BITMAP_SECTOR(disk_size, bit_index) \
    ( (disk_size) +  NDAS_BLOCK_LOCATION_BITMAP + (bit_index) / 8 / 512)

#ifdef DEBUG
#define    debug_1(l, x...) \
do { if(l <= DEBUG_LEVEL_RAID1) {\
    sal_debug_print("R1|%d|%s|",l,__FUNCTION__); sal_debug_println(x);    \
} } while(0)        
#else
#define    debug_1(l, x...) do {} while(0)
#endif

#ifdef DEBUG
#define R1IO_MAGIC 0x555555dd
#define R1IO_ASSERT(rio) ({\
    sal_assert(rio);\
    TIR_ASSERT((rio)->tir); \
    sal_assert((rio)->magic == R1IO_MAGIC);\
})
#else
#define R1IO_ASSERT(tir) do {} while(0)
#endif

struct r1io_s {
#ifdef DEBUG
    int magic;
#endif
    ndas_io_done    done;
    void            *done_arg;
    sal_atomic       count;
    int             nr_io;
    ndas_error_t    err;
    urb_ptr         orgtir;
    struct r1io_s   *other;
    logunit_t   *raid;
};

LOCAL
struct r1io_s *
r1io_alloc(logunit_t *sdev, urb_ptr orgtir, int nr_io) 
{
    struct r1io_s *rio = sal_malloc(sizeof(struct r1io_s));
    if ( !rio ) 
        return NULL;
    
#ifdef DEBUG
    rio->magic = R1IO_MAGIC;
#endif
    rio->done = orgtir->req.done;
    rio->done_arg = orgtir->req.done_arg;
    rio->raid = sdev;
    rio->orgtir = orgtir;
    rio->err = NDAS_OK;
    rio->nr_io = nr_io;
    sal_atomic_set(&rio->count, 0);
    
    debug_1(5,"ed rio=%p", rio);
    return rio;
}

LOCAL void r1io_free(struct r1io_s* rio)
{
    debug_1(5,"rio=%p", rio);
    sal_free(rio);
}

LOCAL
void r1op_deinit(logunit_t *sdev) 
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
xbool r1_apply_rmd(logunit_t *sdev) 
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
        debug_1(5, "[%d]->[%d]", i, idx);
        debug_1(5, "%d:%p", i, sdev->disks[idx]);
    }
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        sdev->disks[i] = tmp[i];
        rddc_disk(sdev,i)->spare = sdev->nmd->rmd->UnitMetaData[i].UnitDeviceStatus == RMD_UNIT_SPARE;
        rddc_disk(sdev,i)->defective = sdev->nmd->rmd->UnitMetaData[i].UnitDeviceStatus == RMD_UNIT_FAULT;
    }
out:
    sal_free(tmp);
    return TRUE;
}
#endif

LOCAL
ndas_error_t r1op_query(logunit_t * sdev, ndas_metadata_t *nmd);

/*
 * called by sdev_create
 */
LOCAL
ndas_error_t r1op_init(logunit_t *sdev)
{
    ndas_error_t err = NDAS_ERROR_OUT_OF_MEMORY;
        
    debug_1(3, "slot=%d", sdev->info.slot);

    debug_1(5, "# of disks=%d", SDEV_RAID_INFO(sdev)->disks);

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
    
    err = r1op_query(sdev, sdev->nmd);    
    if ( !NDAS_SUCCESS(err) )
    {
        if ( err == NDAS_ERROR_TOO_MANY_INVALID || err == NDAS_ERROR_TOO_MANY_OFFLINE ) 
            return NDAS_OK;
        goto out;
    }

    return NDAS_OK;
out:
    r1op_deinit(sdev);
    return err;
}


LOCAL
ndas_error_t r1op_query(logunit_t * sdev, ndas_metadata_t *nmd) // uop_query
{
    int i = 0, count = 0;
    xbool has_key = TRUE;
    ndas_error_t err = NDAS_ERROR_OUT_OF_MEMORY;
    unsigned int min_mspr = -1;
    ndas_metadata_t *nmd_local; // local use
        
    debug_1(3, "slot=%d", sdev->info.slot);

    debug_1(5, "# of disks=%d", SDEV_RAID_INFO(sdev)->disks);

    sal_assert(sdev->private);
    
    nmd_local = nmd_create();
    if ( !nmd_local ) 
        return NDAS_ERROR_OUT_OF_MEMORY;
    
    err = nmd_init_rmd(nmd_local);
    if ( !NDAS_SUCCESS(err) ) {
        goto out;
    }
            
    for ( i = 0 ; i < SDEV_RAID_INFO(sdev)->disks; i++ )
    {
        logunit_t *child = sdev->disks[i]; 
        ndas_metadata_t *n = ( count == 0 ) ? nmd : nmd_local;

        if ( !child ) 
            continue;
        
        err = child->ops->query(child, n); // uop_query
            
        if ( !NDAS_SUCCESS(err) ) {
            debug_1(1, "%d:query err=%d", i, err);
            continue;
        }
        
        if ( !dib_compare(nmd->dib, n->dib) )
        { 
            err = NDAS_ERROR_INVALID_METADATA;
            goto out;
        }
        
        err = udev_read_rmd(SDEV2UDEV(child), n->rmd);

        if ( !NDAS_SUCCESS(err) ) { // invalid rmd
            nmd->rmd->u.s.magic = 0;
        }
                
         if ( count > 0) 
        {
            if ( sdev_rmd_is_newer(sdev, nmd_local->rmd) )
                sal_memcpy(sdev->nmd->rmd, nmd_local->rmd, sizeof(NDAS_RAID_META_DATA));
        } 
        
        if ( min_mspr > child->info.max_sectors_per_request )
            min_mspr = child->info.max_sectors_per_request;
        debug_1(1, "child->has_key=%d", child->has_key);
        has_key &= child->has_key;
        count ++;
    }
    if ( count < 1 ) 
    {
        debug_1(1, "count=%d", count);
        err = NDAS_ERROR_TOO_MANY_OFFLINE;
        goto out;
    }

    
    sdev->has_key = has_key;
    sdev->info.sectors = nmd->dib->u.s.sizeUserSpace;
    sdev->info.capacity = sdev->info.sectors << 9;
    sdev->info.sectors_per_disk = nmd->dib->u.s.sizeUserSpace;
    sdev->info.io_splits = 1;
    sdev->info.mode = NDAS_DISK_MODE_RAID1;
    sdev->info.sector_size = (1 << 9);
    sdev->info.max_sectors_per_request = min_mspr;
    rddc_conf(sdev)->sectors_per_bit_shift = bitmap_ffb((unsigned long)nmd->dib->u.s.iSectorsPerBit);
    debug_1(1, "shift=%d", rddc_conf(sdev)->sectors_per_bit_shift);
    debug_1(1, "iSectorsPerBit=%d", nmd->dib->u.s.iSectorsPerBit);
    debug_1(1, "capacity_low=%Lu", sdev->info.capacity);
    debug_1(1, "sector_size=%u", sdev->info.sector_size);
    err = NDAS_OK;
out:
    nmd_destroy(nmd_local);
    return err;
}

LOCAL
int r1op_status(logunit_t *sdev)

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

LOCAL
int r1_fail_read(urb_ptr tir, void *arg)
{
    long index = (long)arg;
    ndas_error_t err;
    struct r1io_s *rio = (struct r1io_s *) tir->arg;
    logunit_t *raid = rio->raid;
    ndas_io_done done= rio->done;
    void *done_arg = rio->done_arg;

    SDEV_REENTER(raid, r1_fail_read, rio, (void*) index);
    
    err = rddc_member_disable(raid, index);
    if ( !NDAS_SUCCESS(err) ) 
    {
        sal_assert(sdev_is_locked(raid));
        sdev_unlock(raid);
        done(raid->info.slot, err, done_arg);
        r1io_free(rio);
        sal_atomic_dec(&rddc_conf(raid)->nr_queued_io);
        return 0;
    }
    sal_assert(!sdev_is_locked(raid));
    r1io_free(rio);
    tir = tir_clone(tir);
    if ( !tir ) {
        done(raid->info.slot, NDAS_ERROR_OUT_OF_MEMORY, done_arg);
        sal_atomic_dec(&rddc_conf(raid)->nr_queued_io);
        return 0;
    }
    raid->ops->io(raid, tir, TRUE);
    return 0;
}
LOCAL
NDAS_CALL void r1_has_read(int slot, ndas_error_t err,void* arg) 
{
    urb_ptr mytir = (urb_ptr) arg;
    struct r1io_s *rio = (struct r1io_s *) mytir->arg;
    logunit_t *sdev = rio->raid;

    debug_1(5, "slot=%d/%d err=%d", slot, sdev->info.slot, err);
    if ( !NDAS_SUCCESS(err)) 
    {
        int index = raid_find_index(sdev, slot);
        
        r1_fail_read(mytir, (void *)index);
        return;
    }
    
    rio->done(sdev->info.slot, err, rio->done_arg);
    r1io_free(rio);
    sal_atomic_dec(&rddc_conf(sdev)->nr_queued_io);
}
LOCAL int r1_fail_single_written(struct r1io_s *rio, void *arg)
{
    long index = (long)arg;
    ndas_error_t err;
    logunit_t *sdev = rio->raid;

    SDEV_REENTER(sdev, r1_fail_single_written, rio, (void*) index);

    err = rddc_member_disable(sdev, index);
    sal_assert(err == NDAS_ERROR_TOO_MANY_OFFLINE);
    rio->done(sdev->info.slot, NDAS_ERROR_TOO_MANY_OFFLINE, rio->done_arg);
    r1io_free(rio);
    sal_atomic_dec(&rddc_conf(sdev)->nr_queued_io);

	return 0;
}
LOCAL
NDAS_CALL void r1_has_single_written(int slot, ndas_error_t err,void* user_arg) 
{
    urb_ptr mytir = (urb_ptr) user_arg;
    struct r1io_s *rio = (struct r1io_s *) mytir->arg;
    logunit_t *sdev = rio->raid;
    
    debug_1(5, "slot=%d/%d err=%d tir=%p rio=%p", slot, sdev->info.slot, err, mytir, rio);
    if ( !NDAS_SUCCESS(err)) 
    {
        int index = raid_find_index(sdev, slot);
        rddc_mark_bitmap(sdev, &mytir->req, 0);
        r1_fail_single_written(rio, (void *)index);
        return;
    } 

    rio->done(sdev->info.slot, err, rio->done_arg);
    r1io_free(rio);
    sal_atomic_dec(&rddc_conf(sdev)->nr_queued_io);
}

/*
 * r1_has_double_written
 * 
 * 
 */

#ifdef DEBUG
int r1count = 0;
#endif

LOCAL void r1_end_double_written(urb_ptr tir)
{
    struct r1io_s *rio = (struct r1io_s *) tir->arg;
    logunit_t *sdev = rio->raid;
    
    if ( sal_atomic_inc(&rio->count) < rio->nr_io )
        return;

    rio->done(sdev->info.slot, rio->err, rio->done_arg);
    r1io_free(rio);   
    sal_atomic_dec(&rddc_conf(sdev)->nr_queued_io);
}
LOCAL int r1_fail_double_written(urb_ptr tir, void * arg)
{
    long index = (long)arg;
    ndas_error_t err;
    struct r1io_s *rio = (struct r1io_s *) tir->arg;
    logunit_t *sdev = rio->raid;

    SDEV_REENTER(sdev, r1_fail_double_written, tir, (void*) index);

    err = rddc_member_disable(sdev,index);
    if ( !NDAS_SUCCESS(err) )
    {
        rio->err = err;
        sdev_unlock(sdev);
    }
    r1_end_double_written(tir);

	return 0;
}
LOCAL
NDAS_CALL void r1_has_double_written(int slot, ndas_error_t err,void* user_arg) 
{
    urb_ptr mytir = (urb_ptr) user_arg;
    struct r1io_s *rio = (struct r1io_s *) mytir->arg;
    logunit_t *sdev = rio->raid;
    
    debug_1(5, "err=%d slot=%d/%d mytir=%p rio=%p", err, slot, sdev->info.slot, mytir, rio);
    if ( !NDAS_SUCCESS(err)) 
    {
        int index = raid_find_index(sdev, slot);

        rddc_mark_bitmap(sdev, &mytir->req, 0);
        r1_fail_double_written(mytir, (void *)index);
        return;
    }
    r1_end_double_written(mytir);
}


/* 
 * Choose which disk we will read the data from.
 * 1. the status of disk is CONN_STATUS_CONNECTED
 * 2. the bitmap for the sector of the disk is not set
 * 3. the head position is the nearest from the current request.
 */
LOCAL int r1_balance(logunit_t *sdev,xuint64 start, xuint32 nr_sec) 
{
    int i = 0;
    int ret = -1;
    xuint64 distance = -1;
    int d = rddc_conf(sdev)->defect;
    int isdirty;
    int shift;
    bitmap_t *b= (bitmap_t *) sdev->nmd->bitmap;
    debug_1(6, "slot=%d", sdev->info.slot);

    debug_1(8, "sdev->nmd->bitmap=%p", sdev->nmd->bitmap);
    debug_1(8, "sdev->nmd->dib=%p", sdev->nmd->dib);

    if ( d >= 0 )
        return raid_next(sdev, d);

    d = rddc_conf(sdev)->target;
    
    shift = rddc_conf(sdev)->sectors_per_bit_shift;
    isdirty = b && bitmap_isset(b, start >>  shift);

    if (  d >= 0 && isdirty )
        return raid_next(sdev, d);
    
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        struct rddc_disk_s *rd = rddc_disk(sdev,i);
        if ( !sdev->disks[i] ) 
            return raid_next(sdev, i);
        {
            xuint64 dis = U64_ABS(rd->header, start);
            debug_1(8, "min=%Lu dis[%d]=%Lu", distance, i,dis);
            if ( dis < distance ) {
                ret = i;
                distance = dis;    
            } 

        }
    }
    if ( ret != -1 ) {
        rddc_disk(sdev, ret)->header = start + nr_sec - 1; // todo add num_sec
    }
    debug_1(6, "ret=%d", ret);
    return ret;
}

LOCAL
void r1_io_error(logunit_t *sdev,  urb_ptr tir, ndas_error_t err)
{
    tir->req.done(sdev->info.slot, err, tir->req.done_arg);
    tir_free(tir);
    sal_atomic_dec(&rddc_conf(sdev)->nr_queued_io);
    debug_1(1, "err=%d", err);
}
LOCAL
void r1_read(logunit_t *sdev, urb_ptr tir)
{
    int disk;
    struct r1io_s *rio = NULL;
    debug_1(5,"slot=%d", sdev->info.slot);
    sal_assert(tir->cmd == ND_CMD_READ || ND_CMD_VERIFY);
    disk = r1_balance(sdev,tir->req.start_sec, tir->req.num_sec);
    if ( disk < 0 ) {
        r1_io_error(sdev, tir, NDAS_ERROR_SHUTDOWN);
        return;
    }
    rio = r1io_alloc(sdev, tir, 1);

    if ( !rio ) {
        r1_io_error(sdev, tir, NDAS_ERROR_OUT_OF_MEMORY);
        return;
    }
    
    tir->req.done = r1_has_read;
    tir->req.done_arg = tir;
    tir->arg = rio;
    
    debug_1(5,"balanced disk=%d slot=%d", disk, sdev->disks[disk]->info.slot);

    sdev->disks[disk]->ops->io(sdev->disks[disk], tir, FALSE); 
    return;
}
LOCAL 
void r1_single_write(logunit_t *sdev, urb_ptr tir)
{
    int defect = rddc_conf(sdev)->defect;
    int index = raid_next(sdev, defect);
    struct r1io_s *rio = r1io_alloc(sdev, tir, 1);

    debug_1(5,"slot=%d", sdev->info.slot);
    sal_assert(tir->cmd == ND_CMD_WRITE || tir->cmd == ND_CMD_WV || tir->cmd == ND_CMD_FLUSH);
    if ( !rio ) {
        r1_io_error(sdev, tir, NDAS_ERROR_OUT_OF_MEMORY);
        return;
    }
    
    tir->req.done = r1_has_single_written;
    tir->req.done_arg = tir;
    tir->arg = rio;
    
    sdev->disks[index]->ops->io(sdev->disks[index], tir, FALSE); 
    rddc_mark_bitmap(sdev, &tir->req, 0); 
        
    rddc_disk(sdev, index)->header = tir->req.start_sec + tir->req.num_sec - 1; 
    return;
}
LOCAL
void r1_double_write(logunit_t *sdev, urb_ptr orgtir)
{
    struct r1io_s *rio;
    urb_ptr clonetir = NULL;
    debug_1(5,"slot=%d count=%d", sdev->info.slot, ++r1count);
    sal_assert(orgtir->cmd == ND_CMD_WRITE || orgtir->cmd == ND_CMD_WV ||  orgtir->cmd == ND_CMD_FLUSH);
    
    clonetir = tir_clone(orgtir);
    if ( !clonetir ) goto out1;

    rio = r1io_alloc(sdev, orgtir, 2);
    if ( !rio ) goto out2;

    orgtir->req.done = r1_has_double_written;
    orgtir->req.done_arg = orgtir;
    orgtir->arg = rio;
    
    clonetir->req.done = r1_has_double_written;
    clonetir->req.done_arg = clonetir;
    clonetir->arg = rio;
    
    rddc_disk(sdev, 0)->header = orgtir->req.start_sec + orgtir->req.num_sec - 1;
    rddc_disk(sdev, 1)->header = clonetir->req.start_sec + clonetir->req.num_sec - 1;
    
    debug_1(5,"sdev->disks[0]=%p", sdev->disks[0]);
    debug_1(5,"sdev->disks[1]=%p", sdev->disks[1]);
    sdev->disks[0]->ops->io(sdev->disks[0], orgtir, FALSE); 
    sdev->disks[1]->ops->io(sdev->disks[1], clonetir, FALSE); 
    return;
out2:
    tir_free(clonetir);
out1:    
    r1_io_error(sdev, orgtir, NDAS_ERROR_OUT_OF_MEMORY);
    return;
}

#define SECTOR_CONFLICT(x,nr_sec_x, y, nr_sec_y) ( (y) < ((x) + (nr_sec_x)) && (x) < ((y) + (nr_sec_y)))


/* 
 * called by ndas_io
 */
LOCAL
void r1op_io(logunit_t *sdev, urb_ptr tir, xbool urgent)
{
    debug_1(3, "sdev=%p tir=%p ", sdev, tir);
    debug_1(5, "req->uio=%p req->nr_uio=%d req->start_sec=%Lu num_sec=%u", 
        tir->req.uio, 
        tir->req.nr_uio, 
        tir->req.start_sec, 
        tir->req.num_sec);
    TIR_ASSERT(tir);

    sal_atomic_inc(&rddc_conf(sdev)->nr_queued_io);

    if ( rddc_is_locked(sdev) ) 
    {
        sal_spinlock_take_softirq(rddc_conf(sdev)->mutex);
        if ( rddc_is_locked(sdev) ) 
        {
            if ( SECTOR_CONFLICT(rddc_conf(sdev)->rcvr->start,
                    rddc_conf(sdev)->rcvr->nr_sectors,
                    tir->req.start_sec, tir->req.num_sec) ) 
            {
                debug_1(1, "conflict with locked area[%Lu,%Lu), req=[%Lu,%Lu)",
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
        r1_read(sdev, tir);
    else if ( rddc_conf(sdev)->defect >= 0 )
        r1_single_write(sdev, tir);
    else 
        r1_double_write(sdev, tir);
    return;
}

struct ops_s raid1 = {
    .create_disks = (create_disks_func) raid_create,
    .destroy_disks = (destroy_disks_func) raid_cleanup,
    .init = (init_func) r1op_init,
    .deinit = (deinit_func) r1op_deinit,
    .query = (query_func) r1op_query,
    .enable = (enable_func) rddc_op_enable,
    .disable = (disable_func) rddc_op_disable,
    .io = (io_func) r1op_io,
    .status = r1op_status,
    .changed = rddc_device_changed,
};

#endif // XPLAT_RAID


