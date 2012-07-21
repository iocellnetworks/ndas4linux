#include "xplatcfg.h"

#ifdef XPLAT_RAID
#include <netdisk/ndasdib.h>
#include <netdisk/sdev.h>

#include <sal/sync.h>
#include <sal/debug.h>

#include "netdisk/scrc32.h"
#include "netdisk/conn.h"

#include "raid/raid.h"
#include "redundancy.h"

#define RECOVERY_BLOCK_SIZE (4*1024)

#ifdef DEBUG
#define    dbg_rddc(l, x...) \
do { if(l <= DEBUG_LEVEL_RDDC) {\
    sal_debug_print("RD|%d|%s|",l,__FUNCTION__); sal_debug_println(x);    \
} } while(0)     
#define    dbg_rcvr(l, x...) \
do { if(l <= DEBUG_LEVEL_RAID_RECOVER) {\
    sal_debug_print("RR|%d|%s|",l,__FUNCTION__); sal_debug_println(x);    \
} } while(0)        

#else
#define    dbg_rddc(l, x...) do {} while(0)
#define    dbg_rcvr(l, x...) do {} while(0)
#endif

/* forward declarations */
LOCAL
ndas_error_t rd_write_rmd_async(logunit_t *sdev, int defect);

LOCAL
void rr_main(logunit_t *sdev, void *arg);

LOCAL
struct sal_mem_block *
rr_buffer_alloc(int nr_blocks, int block_size) 
{
    int i;
    struct sal_mem_block *ret = sal_malloc(sizeof(struct sal_mem_block) * nr_blocks);
    if ( !ret ) return NULL;
    
    for ( i = 0 ; i < nr_blocks;i++)
    {
        ret[i].ptr = sal_malloc(block_size);
        ret[i].len = block_size;
        if ( !ret[i].ptr ) goto err;
    }
    return ret;
err:
    while( --i >= 0 ) 
    {
        sal_free(ret[i].ptr);
    }
    sal_free(ret);
    return NULL;
}
LOCAL void rr_buffer_free(struct sal_mem_block *blocks, int nr_blocks) 
{
    int i;
    for ( i = 0; i < nr_blocks; i++) 
        sal_free(blocks[i].ptr);
    sal_free(blocks);
}
LOCAL
void rr_unlock_io(logunit_t *sdev) 
{
    struct rddc_recovery *rcvr = rddc_conf(sdev)->rcvr;
    // re-queue pending io
    sal_spinlock_take_softirq(rddc_conf(sdev)->mutex);
    sal_assert(rcvr);
    rcvr->is_locked = 0; // unlock now
    while ( !list_empty(& rcvr->pending) )
    {
        urb_ptr tir = list_entry(rcvr->pending.next,
                                            struct udev_request_block, queue);
        list_del(&tir->queue);
        sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
        dbg_rcvr(3, "re-queue slot=%d pended tir=%p", sdev->info.slot, tir);
        sdev->ops->io(sdev, tir, FALSE);
        sal_spinlock_take_softirq(rddc_conf(sdev)->mutex);
    }
    sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
}


LOCAL 
void rr_end(logunit_t *sdev)
{
    sal_error_print("ndas: the recovery is ended for slot %d\n", sdev->info.slot);
    sal_spinlock_take_softirq(rddc_conf(sdev)->mutex);

    sal_free(rddc_conf(sdev)->rcvr);
    rddc_conf(sdev)->rcvr = NULL;
    sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
}

LOCAL
void rr_complete(logunit_t *sdev)
{
    int target = rddc_conf(sdev)->target;
    ndas_error_t err;
    dbg_rcvr(1, "bitmap is clear slot=%d", sdev->info.slot);
    rddc_conf(sdev)->recovery = 0;
    rddc_conf(sdev)->target = -1;
    //sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
    sdev->nmd->rmd->u.s.state = RMD_STATE_MOUNTED;
    sdev->nmd->rmd->UnitMetaData[target].UnitDeviceStatus &= ~RMD_UNIT_FAULT;
    err = rd_write_rmd_async(sdev, -1);
    if ( !NDAS_SUCCESS(err) ) {
        dbg_rcvr(1, "OUT OF MEMORY to write rmd into the disk.");
    }
    rr_unlock_io(sdev);
    rr_end(sdev);
}

LOCAL
int rr_fail_write(logunit_t *sdev, void * arg)
{
    long index = (long)arg;
    ndas_error_t err;
    dbg_rcvr(1,"slot=%d index=%d", sdev->info.slot, index);

    SDEV_REENTER(sdev, rr_fail_write, sdev, (void*) index);

    err = rddc_member_disable(sdev, index);
    if ( !NDAS_SUCCESS(err) )
        sdev_unlock(sdev);
    rddc_conf(sdev)->recovery  = 0;
    rr_unlock_io(sdev);
    rr_end(sdev); 

	return 0;
}
LOCAL 
void rr_has_written(int slot, ndas_error_t err,void *arg)
{
    logunit_t *sdev = (logunit_t *) arg;
    int ret, idx, index = raid_find_index(sdev, slot);
    struct rddc_recovery *recovery = rddc_conf(sdev)->rcvr;
    
    dbg_rcvr(3, "slot=%d/%d", slot, sdev->info.slot);
    
    if ( !NDAS_SUCCESS(err) ) 
    {
        rr_fail_write(sdev, (void *)index);
        return;
    }
    /* just reset the index - 1*/    
    idx = (recovery->start >> rddc_conf(sdev)->sectors_per_bit_shift);

    bitmap_reset(sdev->nmd->bitmap, idx);
    idx = (recovery->start + recovery->nr_sectors - 1) >> rddc_conf(sdev)->sectors_per_bit_shift;
    bitmap_reset(sdev->nmd->bitmap, idx);

    rr_unlock_io(sdev);
    if ( rddc_conf(sdev)->recovery ) 
    {
        if ( NDAS_SUCCESS(err)) {
            dpc_id = dpc_create(DPC_PRIO_NORMAL, (dpc_func) rr_main, sdev, NULL, NULL, 0);
            if(dpc_id) {
                ret = dpc_invoke(dpc_id);
                sal_assert(ret >= 0);
            }
            return;
        }
        dbg_rcvr(1, "err=%d", err);
    } 
    
    rr_end(sdev); 
}


LOCAL int rr_fail_read(logunit_t *sdev, void *arg)
{
    long index = (long)arg;
    ndas_error_t err;
    struct rddc_recovery *rcvr = rddc_conf(sdev)->rcvr;
    dbg_rcvr(1,"slot=%d index=%d", sdev->info.slot, index);
    SDEV_REENTER(sdev, rr_fail_read, sdev, (void*) index);
    
    err = rddc_member_disable(sdev, index); 
    if ( !NDAS_SUCCESS(err) )
        sdev_unlock(sdev);
    
    if ( sal_atomic_inc(&rcvr->count) < SDEV_RAID_INFO(sdev)->min_disks )
        return 0;
    
    rddc_conf(sdev)->recovery  = 0;
    tir_free(rcvr->tir[rddc_conf(sdev)->target]);
    rr_unlock_io(sdev);
    rr_end(sdev); 
    return 0;
}

LOCAL void rr_has_read(int slot, ndas_error_t err,void *arg)
{
    int i, target;
    logunit_t *sdev = (logunit_t *)arg;
    int index = raid_find_index(sdev, slot);
    struct rddc_recovery *rcvr = rddc_conf(sdev)->rcvr;
    urb_ptr *tir = rcvr->tir;

    sal_assert( index >= 0 );
    dbg_rcvr(3, "slot=%d/%d index=%d count=%d tir=%p", slot, sdev->info.slot, index, 
        sal_atomic_read(&rcvr->count), tir[index]);
    
    if ( !NDAS_SUCCESS(err) )
    {
        dbg_rcvr(1, "err=%d", err);
        rr_fail_read(sdev, (void)index);
        return;
    }

    target = rddc_conf(sdev)->target;    
    
    dbg_rcvr(5, "target=%d", target);
    if ( SDEV_RAID_INFO(sdev)->min_disks > 1 ) 
    {
        int nr_uio = tir[index]->req.nr_uio;
        for ( i = 0 ; i < nr_uio; i++) 
        {
            memxor(tir[target]->req.uio[i].ptr, 
                tir[index]->req.uio[i].ptr,
                tir[target]->req.uio[i].len);
        }
    }
    if ( sal_atomic_inc(&rcvr->count) < SDEV_RAID_INFO(sdev)->min_disks ) {
        return;
    }
    if ( rddc_conf(sdev)->recovery == 0 ) 
    {
        dbg_rcvr(1, "stop the recovery");
        tir_free(rcvr->tir[rddc_conf(sdev)->target]);
        rr_unlock_io(sdev);
        rr_end(sdev);
        return;
    }
    
    dbg_rcvr(5, "writing to slot=%d", sdev->disks[target]->info.slot);
    dbg_rcvr(5, "start_sec=%Lu", tir[target]->req.start_sec);
    dbg_rcvr(5, "tir=%p", tir[target]);
    
    sdev->disks[target]->ops->io(sdev->disks[target], tir[target], FALSE);  // uop_io
}

LOCAL
int rr_main(logunit_t *sdev, void *arg)
{
    int ret, i, target;
    xuint64 sector;
    sal_tick next_schedule = 0;
    struct rddc_recovery *rcvr = rddc_conf(sdev)->rcvr;

    dbg_rcvr(3, "ing slot=%d", sdev->info.slot);
     
    if ( rddc_conf(sdev)->recovery == 0 ) {
        rr_unlock_io(sdev);
        rr_end(sdev);
        return 0;
    }
        
    dbg_rcvr(3, "nr_io=%d", sal_atomic_read(&rddc_conf(sdev)->nr_queued_io));
    if ( sal_atomic_read(&rddc_conf(sdev)->nr_queued_io) > 0 ) {
        dbg_rcvr(3, "busy now try 1 sec later");
        next_schedule = SAL_TICKS_PER_SEC * sal_atomic_read(&rddc_conf(sdev)->nr_queued_io);
        goto out;
    }
    sal_assert(rcvr);
    
    dbg_rcvr(5, "rcvr->is_locked=%d", rcvr->is_locked);
    sal_spinlock_take_softirq(rddc_conf(sdev)->mutex);
    if ( rcvr->is_locked ) {
        sal_assert(!rcvr->is_locked);
        dbg_rcvr(1, "locked slot=%d", sdev->info.slot);
        sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
        next_schedule = SAL_TICKS_PER_SEC * 20;
        goto out;  // wait more sec to be complete. never happens
    }
    sector = bitmap_first_set_index(sdev->nmd->bitmap);
    dbg_rcvr(5, "first dirty index=%Lu", sector);
    if ( sector == 0xffffffffffffffffULL ) {
        dbg_rcvr(1, "bitmap is clear slot=%d", sdev->info.slot);
        sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
        rr_complete(sdev);
        return 0;
    }
    sector <<= rddc_conf(sdev)->sectors_per_bit_shift;
    dbg_rcvr(5, "first dirty sector=%Lu", sector);
    
    rcvr->is_locked = 1;
    rcvr->start = sector;
    rcvr->nr_sectors = sdev->info.max_sectors_per_request;
    sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
    dbg_rcvr(3, "slot=%d sector=%Lu num_sector=%d", sdev->info.slot, sector, rcvr->nr_sectors);

    target = rddc_conf(sdev)->target;

    sal_assert(target >= 0);        
    sal_atomic_set(&rcvr->count,0);
    
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks;i++) 
    {
        urb_ptr tir = tir_alloc(target == i ? ND_CMD_WRITE: ND_CMD_READ, NULL);
        if ( !tir ) goto err;
        
        tir->req->buf = NULL;
        tir->req->start_sec = sector;
        tir->req->num_sec = sdev->info.max_sectors_per_request;
        if ( sdev->info.mode == NDAS_DISK_MODE_RAID4 ) {
            tir->req->nr_uio = rcvr->nr_blocks / SDEV_RAID_INFO(sdev)->disks;
            tir->req->uio = rcvr->blocks + i * tir->req->nr_uio;
        } else {
            sal_assert(sdev->info.mode == NDAS_DISK_MODE_RAID1);
            tir->req->nr_uio = rcvr->nr_blocks;
            tir->req->uio = rcvr->blocks;
        }
        
        tir->req->done_arg = sdev;
        if ( target == i ) {
            tir->req->done =  rr_has_written;
        } else {
            tir->req->done =  rr_has_read;
        }
        rcvr->tir[i] = tir;
        
    }
    if ( sdev->info.mode == NDAS_DISK_MODE_RAID4 )
        for ( i = 0 ; i < rcvr->tir[target]->req.nr_uio; i++) {
            sal_memset(rcvr->tir[target]->req.uio[i].ptr, 0 , rcvr->tir[target]->req.uio[i].len);
        }
        
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks;i++) 
    {
        if ( i != target ) {
            dbg_rcvr(3, "disks[%d] slot=%d queue tir=%p", i, sdev->disks[i]->info.slot, rcvr->tir[i]);
            sdev->disks[i]->ops->io(sdev->disks[i], rcvr->tir[i], FALSE);
        }
        
    }
    return 0;
out:
    if ( rddc_conf(sdev)->recovery ) {
        dpc_id dpcid;
        dpcid = dpc_create(DPC_PRIO_NORMAL, (dpc_func) rr_main, sdev, NULL, NULL, 0);
        if(dpcid) {
            ret = dpc_queue(dpcid, next_schedule);
            if(ret < 0) {
                dpc_destroy(dpcid);
            }
        }
    }
    return 0;
err:
    // out of memory free tirs
    return 0;
}

LOCAL
ndas_error_t rr_start(logunit_t *sdev) 
{
    int ret;
    dpc_id dpcid;
    int sz = sizeof(struct rddc_recovery)
            + sizeof(struct udev_request_block *) * SDEV_RAID_INFO(sdev)->disks;
    struct rddc_recovery *rcvr = sal_malloc(sz);
    dbg_rcvr(2, "ing slot=%d", sdev->info.slot);
    
    if ( !rcvr ) {
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    sal_memset(rcvr, 0 ,sz);

    INIT_LIST_HEAD(&rcvr->pending);

    if ( sdev->info.mode == NDAS_DISK_MODE_RAID4 ) {
        dbg_rcvr(2, "raid4");
        rcvr->nr_blocks = SDEV_RAID_INFO(sdev)->disks * sdev->info.max_sectors_per_request * 512 / RECOVERY_BLOCK_SIZE;
    } else {
        sal_assert(sdev->info.mode == NDAS_DISK_MODE_RAID1);
        rcvr->nr_blocks = sdev->info.max_sectors_per_request * 512 / RECOVERY_BLOCK_SIZE;
    }
    rcvr->blocks = rr_buffer_alloc(rcvr->nr_blocks, RECOVERY_BLOCK_SIZE);
    
    dbg_rcvr(2, "blocks=%p", rcvr->blocks);
    dbg_rcvr(2, "nr_blocks=%d", rcvr->nr_blocks);
    if ( !rcvr->blocks )
        goto out;

    sal_assert(rddc_conf(sdev)->rcvr == NULL);
    rddc_conf(sdev)->rcvr = rcvr;

    dpcid = dpc_create(DPC_PRIO_NORMAL, (dpc_func) rr_main, sdev, NULL, NULL, 0);
    if(dpcid) {
        ret = dpc_invoke(dpcid);
        sal_assert(ret >= 0);
        return NDAS_OK;
    }
    rr_buffer_free(rcvr->blocks, rcvr->nr_blocks);

out:
    sal_free(rcvr);
    rddc_conf(sdev)->rcvr = NULL;
    return NDAS_ERROR_OUT_OF_MEMORY;
}


LOCAL
ndas_error_t
rd_process_rmd(logunit_t *sdev, xbool writable)
{
    int i, defect;
    
    ndas_error_t err;
    NDAS_RAID_META_DATA* rmd = sdev->nmd->rmd;
    dbg_rddc(3,"ing sdev=%p", sdev);
    if ( rmd->u.s.magic != RMD_MAGIC ) 
        return NDAS_ERROR_INVALID_METADATA;
    
    err = nmd_init_bitmap(sdev->nmd);
    if ( !NDAS_SUCCESS(err) ) 
        return err;
    
    defect = rddc_conf(sdev)->defect;
    
    for (i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        if ( rmd->UnitMetaData[i].UnitDeviceStatus == RMD_UNIT_SPARE) 
        {
            nmd_free_bitmap(sdev->nmd);
            return NDAS_ERROR_NOT_IMPLEMENTED;
        }
        if ( rmd->UnitMetaData[i].UnitDeviceStatus == RMD_UNIT_FAULT ) 
        {
            if ( defect >= 0 && defect != i ) 
            {
                dbg_rddc(1, "defect=%d but RMD fault=%d", defect, i);
                nmd_free_bitmap(sdev->nmd);
                return NDAS_ERROR_TOO_MANY_INVALID;
            }
            rddc_conf(sdev)->target = i;
        }
        dbg_rddc(5, "[%d]status=%d", i, rmd->UnitMetaData[i].UnitDeviceStatus);
    }

    dbg_rddc(3,"rmd->state=0x%x", rmd->u.s.state);
    if ( rmd->u.s.state == RMD_STATE_MOUNTED ) 
    {
        bitmap_set_all(sdev->nmd->bitmap);
        dbg_rddc(3,"running=%d", sal_atomic_read(&rddc_conf(sdev)->running));
        dbg_rddc(3,"disks=%d", SDEV_RAID_INFO(sdev)->disks);
        if ( sal_atomic_read(&rddc_conf(sdev)->running) == SDEV_RAID_INFO(sdev)->disks ) {
            if ( rddc_conf(sdev)->target >= 0 ) {
                dbg_rddc(3,"recovery=1");
                rddc_conf(sdev)->recovery = 1;
            } else {
                sal_error_print("ndas: The raid system was crashed last time for slot %d\n", sdev->info.slot);
                sal_error_print("ndas: But it is impossible to decide which one has a more recent data\n");
                sal_error_print("ndas: Execute \'ndasadmin invalidate -s <slot>\' to force to perform the recovery into <slot>\n");
            }
        }
    } else {
        dbg_rddc(3,"target=%d", rddc_conf(sdev)->target);
        dbg_rddc(3,"running=%d", sal_atomic_read(&rddc_conf(sdev)->running) );
        
        rmd->u.s.state = RMD_STATE_MOUNTED;
        if ( sal_atomic_read(&rddc_conf(sdev)->running) == SDEV_RAID_INFO(sdev)->disks
            && rddc_conf(sdev)->target >= 0 ) 
        {
            udev_t *udev;
            i = raid_prev(sdev, rddc_conf(sdev)->target);
            udev = SDEV2UDEV(sdev->disks[i]);
            dbg_rddc(3,"Read bitmap from index=%d", i);
            err = conn_read_bitmap(&udev->conn, udev->info->sectors, sdev->nmd->bitmap);
            if ( !NDAS_SUCCESS(err)) {
                nmd_free_bitmap(sdev->nmd);
                return err;
            }
            dbg_rddc(3,"first_set_index=%Lu", bitmap_first_set_index(sdev->nmd->bitmap));
            if ( bitmap_first_set_index(sdev->nmd->bitmap) != 0xffffffffffffffffULL )
                rddc_conf(sdev)->recovery = 1;
        }
        
    }
    if ( writable ) sdev_queue_write_rmd(sdev);
    return NDAS_OK;

}
/* 
 * True if operatable
 * False if not operatable.
 */
LOCAL
ndas_error_t rd_set_defect_member(logunit_t *sdev, int index) 
{
    dbg_rddc(1, "raid=%d index=%d", sdev->info.slot, index);
    
    sal_spinlock_take_softirq(rddc_conf(sdev)->mutex);
    if ( rddc_conf(sdev)->defect >= 0 && index != rddc_conf(sdev)->defect) 
    {
        /* 2 or more disks fail */
        sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
        return NDAS_ERROR_TOO_MANY_OFFLINE;
    }  

    if ( rddc_conf(sdev)->target >= 0 && index != rddc_conf(sdev)->target )
    {
        /* recovery target and other data disk fail */
        sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
        return NDAS_ERROR_TOO_MANY_INVALID;
    }

    rddc_conf(sdev)->defect = index;
    sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
    return NDAS_OK;
}

LOCAL
void rd_all_enabled(logunit_t *sdev) 
{
    struct raid_struct *rc = SDEV_RAID(sdev);
    dbg_rddc(3, "rc->op_err=%d", rc->op_err);
    if ( NDAS_SUCCESS(rc->op_err) ) {
        rc->op_err = rd_process_rmd(sdev, rc->op_writable);
    }
    if ( !NDAS_SUCCESS(rc->op_err) ) {
        sal_assert(sdev->accept_io == 0);
        
        raid_stop_disks(sdev, rc->op_err, TRUE, raid_disabled_on_failing_to_enable);
        return;
    } 

    sdev->info.writable = rc->op_writable;
    sdev->info.writeshared = rc->op_writeshared;
    
    sdev_notify_enable_result(sdev, rc->op_err);
        
    
    dbg_rddc(3, "rc->op_writable=%d", rc->op_writable);
    if ( rddc_conf(sdev)->recovery && sdev->info.writable ) {
        rr_start(sdev); // check err
    }
}
LOCAL
void rd_all_disabled(logunit_t *raid) 
{
    dbg_rddc(1, "ing raid=%p", raid);

    sal_assert(sdev_is_locked(raid));
    if ( raid->nmd->bitmap ) {
        bitmap_destroy(raid->nmd->bitmap);
        raid->nmd->bitmap = NULL;
    }
        
    sdev_notify_disable_result(raid, SDEV_RAID(raid)->op_err);
}
LOCAL
void rd_all_disabled_on_running(int slot, ndas_error_t err, void* user_arg)
{
    logunit_t *raid = (logunit_t *) user_arg;
    dbg_rddc(1, "ing raid=%d slot=%d err=%d", raid->info.slot, slot, err);

    sal_assert(sdev_is_locked(raid));
    if ( sal_atomic_dec_and_test(&rddc_conf(raid)->running) == 0 )
        return;
    
    rd_all_disabled(raid);
}

LOCAL
int rd_slot_disabled_after_locked(void * arg1, void * arg2)
{
    int slot = (int)arg1;
    int err = (int)arg2;
    int running,index;
    logunit_t *child, *raid;
    
    child = sdev_lookup_byslot(slot);
    if ( !child ) {
        dbg_rddc(1, "slot=%d is unregistered", slot);
        return 0;
    }
    raid = sdev_lookup_byslot(SDEV_UNIT_INFO(child)->raid_slot);
    if ( !raid ) {
        dbg_rddc(1, "slot=%d is not found system shuting down", SDEV_UNIT_INFO(child)->raid_slot);
        return 0;
    }
    SDEV_REENTER(raid, rd_slot_disabled_after_locked, (void*)slot, (void*)err);
    index = raid_find_index(raid, slot);
    
    running = sal_atomic_dec(&rddc_conf(raid)->running);
    
    sdev_set_disabled(child);
    
    dbg_rddc(2, "enabled=%d nr_running=%d", raid->info.enabled, 
        sal_atomic_read(&rddc_conf(raid)->running));

    err = rd_set_defect_member(raid, index);
    if ( !NDAS_SUCCESS(err) ) {
        dbg_rddc(3, "already defected");
        goto false;
    }
    if ( raid->info.writable ) 
    {
        err = rd_write_rmd_async(raid, index);
        if ( !NDAS_SUCCESS(err) )
        {
            dbg_rddc(1, "OUT OF MEMORY");  
            goto false;
        }
    }
    sdev_unlock(raid);
    return;
false:
    sdev_dont_accept_io(raid);
    raid_stop_disks(raid, err, FALSE, rd_all_disabled_on_running);
    return;
}
LOCAL 
void rd_op_disabled(int slot, ndas_error_t err, void* user_arg)
{
    int running;
    logunit_t *raid = (logunit_t *) user_arg;
    int index = raid_find_index(raid, slot);
    dbg_rddc(1, "slot=%d", slot);
    dbg_rddc(2, "enabled=%d nr_running=%d", raid->disks[index]->info.enabled, 
        sal_atomic_read(&rddc_conf(raid)->running));

    sal_assert(raid->disks[index]->info.enabled);
    
    running = !sal_atomic_dec_and_test(&rddc_conf(raid)->running);
    
    sdev_set_disabled(raid->disks[index]);
    
    dbg_rddc(2, "enabled=%d nr_running=%d", raid->info.enabled, 
        sal_atomic_read(&rddc_conf(raid)->running));

    if ( !NDAS_SUCCESS(err) ) {
        SDEV_RAID(raid)->op_err = err;
    }
    if ( running > 0 )
        return;
    
    sdev_notify_disable_result(raid, SDEV_RAID(raid)->op_err);
}
LOCAL
void rd_slot_disabled(int slot, ndas_error_t err, void* user_arg)
{
#ifdef DEBUG
    logunit_t *sdev = (logunit_t *) user_arg;
    int index = raid_find_index(sdev, slot);
    dbg_rddc(1, "slot=%d", slot);
    dbg_rddc(2, "enabled=%d nr_running=%d", sdev->disks[index]->info.enabled, 
        sal_atomic_read(&rddc_conf(sdev)->running));

    sal_assert(sdev->disks[raid_find_index(sdev, slot)]->info.enabled);
#endif    
    rd_slot_disabled_after_locked((void *)slot, (void *)err);
}


LOCAL
void rd_slot_enabled_index(logunit_t *sdev, int index, ndas_error_t err)
{
    struct raid_struct *rc = SDEV_RAID(sdev);
    NDAS_RAID_META_DATA rmd;
    dbg_rddc(3, "index=%d err=%d", index, err);
    dbg_rddc(3, "defect=%d", rddc_conf(sdev)->defect);
    dbg_rddc(4, "raid_struct=%p", rc);

    sal_assert(index >= 0);
    sal_assert(sdev_is_locked(sdev));
    
    if ( !NDAS_SUCCESS(err) ) goto err;

    sal_assert(sdev->disks[index]);
    
    dbg_rddc(3, "slot=%d", sdev->disks[index]->info.slot);

    sal_atomic_inc(&rddc_conf(sdev)->running);
    dbg_rddc(3, "running=%d enabled=%d", sal_atomic_read(&rddc_conf(sdev)->running), 
                                        sdev->disks[index]->info.enabled);
    sal_assert(sdev->disks[index]->info.enabled == 1);
        
    err = udev_read_rmd(SDEV2UDEV(sdev->disks[index]), &rmd);
    if ( err == NDAS_ERROR_INVALID_METADATA ) {
        dbg_rddc(1, "invalid metadata on slot %d", sdev->disks[index]->info.slot);
        // TODO: ignore. it is okay if there is at least one disk avaiable to read rmd, 
        // TODO: but what if no rmd is available ?
        goto out;
    }
    if ( !NDAS_SUCCESS(err) ) { // connection failure or out of memory
        dbg_rddc(1, "fail to read rmd"); 
        // TODO: thread will be killed in rd_all_enabled(), and here. 
        // TODO: could be a race condition 
        sal_assert(sdev->disks[index]->disabled_func == NULL);
        sdev->disks[index]->ops->disable(sdev->disks[index],err);
        goto err;
    }
    
    
    if ( sdev_rmd_is_newer(sdev, &rmd) ) {
        dbg_rddc(2, "using rmd on slot %d", sdev->info.slot);
        sal_memcpy(sdev->nmd->rmd, &rmd, sizeof(NDAS_RAID_META_DATA));
    }
	
    sdev->disks[index]->disabled_func = rd_slot_disabled;
out:
    if ( sal_atomic_dec_and_test(&rc->op_count) == 0) {
        return;
    }
    dbg_rddc(5, "op_count=%d", sal_atomic_read(&rc->op_count));
    
    rd_all_enabled(sdev);
    return;
err:
    err = rd_set_defect_member(sdev, index);
    if ( !NDAS_SUCCESS(err) ) {
        dbg_rddc(3, "already defected");
        rc->op_err = err;
    }
    goto out;
}
/* 
 * This function will be called. When 
 * 1. the member unit disk become enabled, or
 * 2. an error occured after enable request to the unit disk.
 * 
 */
LOCAL
void rd_slot_enabled(int slot, ndas_error_t err,void* arg)
{
    logunit_t *sdev = (logunit_t *) arg;
    int index = raid_find_index(sdev, slot);
    rd_slot_enabled_index(sdev, index, err);
}
LOCAL
void NDAS_CALL rd_write_rmd_done(int slot, ndas_error_t err, void *arg)
{
  //logunit_t *sdev = (logunit_t *) arg;
    
    dbg_rddc(5, "slot=%d err=%d", slot, err);
    if ( !NDAS_SUCCESS(err)) {
        dbg_rddc(1, "err=%d", err);
        sal_assert(NDAS_SUCCESS(err));
        /* We don't care if fail to write rmd at this moment 
           as it is very hard to lock the sdev to call rddc_member_disable here
           (sdev is already locked or not yet locked)
           So let the other I/O operation dicovery if the disk is disabled */
        /* rddc_member_disable(sdev, index); */
    }
}
LOCAL
ndas_error_t rd_write_rmd_async(logunit_t *raid, int defect) 
{
    int i;
    urb_ptr tir;
    logunit_t *child;
    dbg_rddc(3, "raid=%d defect=%d", raid->info.slot, defect);

    
    if ( defect != -1 ) {
        raid->nmd->rmd->UnitMetaData[defect].UnitDeviceStatus
            = RMD_UNIT_FAULT;
        raid->nmd->rmd->crc32_unitdisks = crc32_calc((unsigned char *)raid->nmd->rmd->UnitMetaData, 
                                                sizeof(raid->nmd->rmd->UnitMetaData));
    }
    
    for (i = 0; i < SDEV_RAID_INFO(raid)->disks; i++) 
    {
        child = raid->disks[i];
        if ( defect == i || !child) 
        {
            dbg_rddc(1, "skip index=%d", i);
            continue;
        }
        dbg_rddc(1, "slot[%d]=%d", i, child->info.slot);
        tir = tir_alloc(ND_CMD_WRITE, NULL);
        if ( !tir ) {
            return NDAS_ERROR_OUT_OF_MEMORY;
        }
        tir->req->buf = (char*) raid->nmd->rmd;
        tir->req->start_sec = child->info.sectors + NDAS_BLOCK_SIZE_XAREA + NDAS_BLOCK_LOCATION_1ST_RMD;
        tir->req->num_sec = 1;
        tir->req->done = rd_write_rmd_done;
        tir->req->done_arg = raid;
         
        child->ops->io(child, tir, TRUE); // uop_io
    }
    for (i = 0; i < SDEV_RAID_INFO(raid)->disks; i++) {
        child = raid->disks[i];
        if ( defect == i || !child) {
            dbg_rddc(1, "skip index=%d", i);
            continue;
        }
        dbg_rddc(1, "slot[%d]=%d", i, child->info.slot);
        tir = tir_alloc(ND_CMD_WRITE, NULL);
        if ( !tir ) {
            return NDAS_ERROR_OUT_OF_MEMORY;
        }
        tir->req->buf = (char*) raid->nmd->rmd;
        tir->req->start_sec = child->info.sectors + NDAS_BLOCK_SIZE_XAREA + NDAS_BLOCK_LOCATION_2ND_RMD;
        tir->req->num_sec = 1;
        tir->req->done = rd_write_rmd_done;
        tir->req->done_arg = raid;
        child->ops->io(child, tir, TRUE);
    }
    return NDAS_OK;
}


void rddc_free(struct rddc_s *ret) {
    sal_spinlock_destroy(ret->mutex);
    sal_free(ret);
}
struct rddc_s * rddc_alloc(int disks) 
{
    int i;
    int sz = sizeof(struct rddc_s) + 
                    sizeof(struct rddc_disk_s) * disks;
    struct rddc_s *ret = sal_malloc(sz);

    if ( !ret ) return NULL;
    
    sal_memset(ret, 0, sz);
    sal_atomic_set(&ret->nr_queued_io, 0);
    ret->defect = -1;
    if (!sal_spinlock_create("rddc", &ret->mutex)) {
        sal_free(ret);
        return NULL;
    }
    dbg_rddc(1, "mutex=%p", ret->mutex);
    for ( i = 0; i < disks; i++) {
        ret->disks[i].defective = 0;
        ret->disks[i].spare = 0;
    }
    
    return ret;
}

/* 
 * Mark the bitmap in memory
 * req has the sector number for virtual raid system (not for the physical disk)
 */
void rddc_mark_bitmap(logunit_t *sdev, ndas_io_request_ptr req, int shift)
{
    int s = rddc_conf(sdev)->sectors_per_bit_shift + shift;
    int index_in_bits = req->start_sec >> s;
    dbg_rddc(3, "sdev=%p req=%p ", sdev, req);
    dbg_rddc(6, "start=%Lu",  req->start_sec);

    sal_assert(sdev->nmd->bitmap);
        
    bitmap_set((bitmap_t*)sdev->nmd->bitmap, index_in_bits);
    dbg_rddc(7, "index_in_bits=%d", index_in_bits);
    
    index_in_bits = (req->start_sec + req->num_sec - 1) >> s;
    bitmap_set((bitmap_t*)sdev->nmd->bitmap, index_in_bits);
    dbg_rddc(7, "index_in_bits=%d", index_in_bits);
}

/* 
 * called by enable_internal
 */

void 
rddc_op_enable(logunit_t *sdev, int flags)
{
    xbool writable = ENABLE_FLAG_WRITABLE(flags);
    xbool writeshare = ENABLE_FLAG_WRITESHARE(flags) ;
    int i, count = 0;
    ndas_error_t err;
    struct raid_struct* rc = SDEV_RAID(sdev);
    NDAS_RAID_META_DATA* rmd;
    sal_assert(sdev);
    sal_assert(sdev->nmd);
    sal_assert(sdev->nmd->rmd);
    dbg_rddc(3, "sdev=%p writable=%d disks=%d", sdev, writable, SDEV_RAID_INFO(sdev)->disks);

    sal_assert(sdev_is_locked(sdev));
    rc->op_writable = writable;
    rc->op_writeshared = writeshare;
    
    rddc_conf(sdev)->defect = -1;
    rddc_conf(sdev)->target = -1;
    rddc_conf(sdev)->recovery = 0;
    rc->op_err = NDAS_OK;
    
    rmd = sdev->nmd->rmd;
    
    err = sdev->ops->query(sdev, sdev->nmd); // re-read meta data
    if ( !NDAS_SUCCESS(err) ) 
    {
        sdev_notify_enable_result(sdev, err);
        return;
    }
    for (i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        if ( !sdev->disks[i] ) 
        {
            continue;
        }
        count++;
    }
    if ( count < SDEV_RAID_INFO(sdev)->min_disks ) 
    {
        sdev_notify_enable_result(sdev, NDAS_ERROR_TOO_MANY_OFFLINE);
        return;
    }
    sal_atomic_set(&SDEV_RAID(sdev)->op_count, SDEV_RAID_INFO(sdev)->disks);
    for (i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        if ( !sdev->disks[i] ) 
        {
            rd_slot_enabled_index(sdev, i, NDAS_ERROR_NOT_ONLINE);
            continue;
        }
        sdev->disks[i]->enabled_func = rd_slot_enabled;
        sdev->disks[i]->arg = sdev;        /* uop_enable */
        sdev->disks[i]->ops->enable(sdev->disks[i], ENABLE_FLAG(writable, writeshare, TRUE, FALSE)); 
    }
}

struct writing_bitmap {
    sal_atomic count;
    ndas_error_t err;
    int *wait;
    int nr_io;
    struct sal_mem_block uio[0];
};

LOCAL
NDAS_CALL 
void rd_write_bitmap_done(int slot, ndas_error_t err,void* user_arg)
{
    struct writing_bitmap *arg = (struct writing_bitmap *) user_arg;
    dbg_rddc(1,"count=%d nr_uio=%d err=%d", sal_atomic_read(&arg->count), arg->nr_io, err);
    if ( !NDAS_SUCCESS(err) )
        arg->err = err;
    
    if ( sal_atomic_inc(&arg->count) < arg->nr_io )
        return;
    arg->wait = 0;
}
#define BITMAP_IO_UNIT (32*1024)
LOCAL   
ndas_error_t
rd_write_bitmap(logunit_t *sdev, bitmap_t *bitmap)
{
    int i, j, bitmap_size, wait = 1;
    ndas_error_t err = NDAS_OK;
    urb_ptr tir;
    struct writing_bitmap *arg;

    dbg_rddc(3, "sdev=%p", sdev );
    dbg_rddc(3, "bitmap=%p", bitmap);
    
    arg = sal_malloc(sizeof(struct writing_bitmap) + sizeof(struct sal_mem_block) * (BITMAP_IO_UNIT / bitmap->chunk_size));

    if ( !arg ) {
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    bitmap_size = bitmap->nr_chunks * bitmap->chunk_size;
    arg->nr_io = (BITMAP_IO_UNIT / bitmap->chunk_size);
    sal_atomic_set(&arg->count, 0);
    arg->err = NDAS_OK;
    arg->wait = &wait;
    arg->nr_io = bitmap_size / BITMAP_IO_UNIT;
    for ( j = 0; j < bitmap_size / BITMAP_IO_UNIT; j++) 
    {
        tir = tir_alloc(ND_CMD_WRITE, NULL);
        if ( !tir ) {
            arg->err = NDAS_ERROR_OUT_OF_MEMORY;
            if ( sal_atomic_inc(&arg->count) == arg->nr_io )
                wait = 0;
            continue;
        }
        tir->req->start_sec = sdev->info.sectors 
            + NDAS_BLOCK_SIZE_XAREA 
            + NDAS_BLOCK_LOCATION_BITMAP
            + (BITMAP_IO_UNIT/512 * j);
        tir->req->num_sec = BITMAP_IO_UNIT / 512;
        tir->req->nr_uio = BITMAP_IO_UNIT / bitmap->chunk_size;
        tir->req->uio = arg->uio;
        tir->req->buf = NULL;
        tir->req->done = rd_write_bitmap_done;
        tir->req->done_arg = arg;
        for ( i = 0; i < BITMAP_IO_UNIT / bitmap->chunk_size; i++ ) 
        {
            int idx = j * (BITMAP_IO_UNIT / bitmap->chunk_size) + i;
            if ( idx < bitmap->nr_chunks) 
            {
                tir->req->uio[i].ptr = bitmap->chunks[idx];
                tir->req->uio[i].len = bitmap->chunk_size;
            }
        }

        sdev->ops->io(sdev, tir, FALSE);
    }
    if ( int_wait(&wait, 1000, 20) == -1 ) {
        err = NDAS_ERROR_TIME_OUT;
    } else
        err = arg->err;
    sal_free(arg);
    return err;
}

/*
 *
 * Called by sdev_disable
 */

void rddc_op_disable(logunit_t *sdev, ndas_error_t err)
{
    
    int target = rddc_conf(sdev)->target;
    xbool full_recovery = FALSE;
    
    dbg_rddc(1, "sdev=%p err=%d", sdev, err);

    sal_assert(sdev_is_locked(sdev));
    sal_atomic_set(&SDEV_RAID(sdev)->op_count, SDEV_RAID_INFO(sdev)->disks);
    
    sdev_dont_accept_io(sdev);
    rddc_conf(sdev)->recovery = 0;

    if ( target < 0 ) target = rddc_conf(sdev)->defect;
    
    sdev->nmd->rmd->u.s.state = RMD_STATE_UNMOUNTED;
    
    dbg_rddc(1, "target=%d", target);
    
    if ( sdev->info.writable ) 
    {
        if ( target >= 0 ) 
        {
            sdev->nmd->rmd->UnitMetaData[target].UnitDeviceStatus
                = RMD_UNIT_FAULT;
            target = raid_prev(sdev,target);
            
            if ( sdev->disks[target] ) 
            {
                ndas_error_t ret;
                dbg_rddc(1, "writing bitmap to slot %d", sdev->disks[target]->info.slot);
                ret = rd_write_bitmap(sdev->disks[target], sdev->nmd->bitmap);
                if ( !NDAS_SUCCESS(ret) ) {
                    dbg_rddc(1, "writing bitmap err=%d", ret);
                    full_recovery = TRUE;
                }
            } else 
                full_recovery = TRUE;
        }
        dbg_rddc(1, "full_recovery=%d", full_recovery);
        if ( !full_recovery ) {
            // WIN_FLUSH
            
            sdev_queue_write_rmd(sdev);
        }
    }
    
    raid_stop_disks(sdev, err, FALSE, rd_op_disabled);
}

/* 
 * Called by 
 *   1. v_uop_enabled, 
 *   2. udev_shutdown on running
 *   3. uop_enable, v_uop_enable if out of memory error.
 */
LOCAL
void rddc_slot_changed_by_pnp(int slot, ndas_error_t err,void* arg)
{
    logunit_t *sdev = (logunit_t *) arg;
    int index = raid_find_index(sdev, slot);
    NDAS_RAID_META_DATA rmd;
    dbg_rddc(1, "slot=%d err=%d", slot, err);
    
    sal_assert(sdev_is_locked(sdev));
   
    if ( !NDAS_SUCCESS(err) ) {
        dbg_rddc(1, "nothing is changed");
        sdev_unlock(sdev);
        return;
    }
    err = udev_read_rmd(SDEV2UDEV(sdev->disks[index]), &rmd);
    
    if ( NDAS_SUCCESS(err) ) {
        // sal_spinlock_take_softirq(sdev->nmd->mutex);
        sdev->disks[index]->enabled_func = rd_slot_enabled;
	    if ( sdev_rmd_is_newer(sdev, &rmd)) 
        {
            sal_error_print("ndas: Detected a newer version of raid meta data for slot %d\n"
                            "ndas: Please re-enable slot %d\n", sdev->info.slot, sdev->info.slot);   
#if 0
            /* We need to decide if we stop the raid here or not 
             * Please add more logic to decide if the raid should be re-started
             */

            /* rddc_op_disable */
            sdev->disks[index]->disabled_func = rd_slot_disabled;
            // sal_spinlock_give_softirq(sdev->nmd->mutex);
            sdev->ops->disable(sdev, NDAS_ERROR_SHUTDOWN_IN_PROGRESS); 
            return;
#endif
	    }
        // sal_spinlock_give_softirq(sdev->nmd->mutex);
	}

    sdev->disks[index]->disabled_func = rd_slot_disabled;
    
    sal_spinlock_take_softirq(rddc_conf(sdev)->mutex);
    sal_atomic_inc(&rddc_conf(sdev)->running);

    dbg_rddc(1, "index=%d defect=%d", index, rddc_conf(sdev)->defect);
    
    if ( index == rddc_conf(sdev)->defect ) {
        rddc_conf(sdev)->target = index;
        rddc_conf(sdev)->defect = -1;
        rddc_conf(sdev)->recovery = 1;
    }
    sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
    sdev_unlock(sdev);
    dbg_rddc(1, "writable=%d", sdev->info.writable);
    if ( rddc_conf(sdev)->recovery && sdev->info.writable ) {
        rr_start(sdev); // check err
    }
    
}


/* 
 * Set the unit device is defective 
 * And write RMD info if write-enabled.
 * It release the lock if operational
 * Ohterwhie it queue the request to stop the disks and still hold the sdev lock.
 *
 * return: TRUE if operational 
 */

ndas_error_t rddc_member_disable(logunit_t *sdev, int index) 
{
    ndas_error_t err;
    dbg_rddc(3, "slot=%d disabling", sdev->disks[index]->info.slot);
    rddc_conf(sdev)->recovery = 0;

    sal_assert(sdev_is_locked(sdev));

    
    sal_spinlock_take_softirq(rddc_conf(sdev)->mutex);
    if ( rddc_conf(sdev)->defect >= 0 && index == rddc_conf(sdev)->defect ){
        dbg_rddc(1, "slot %d is already set as defected", sdev->disks[index]->info.slot);  
        sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);    
        sdev_unlock(sdev);
        return NDAS_OK;
    }
    sal_spinlock_give_softirq(rddc_conf(sdev)->mutex);
    err = rd_set_defect_member(sdev, index);
    if ( !NDAS_SUCCESS(err) )
    {
        goto false;
    }
    
    if ( sdev->info.writable ) 
    {
        err = rd_write_rmd_async(sdev, index);
        if ( !NDAS_SUCCESS(err) )
        {
            dbg_rddc(1, "OUT OF MEMORY");  
            goto false;
        }
    }
    sdev_unlock(sdev);
    return NDAS_OK;
false:
    // let them decide
    //raid_stop_disks(sdev, err);
    return err;
}

/* 
 * If an NDAS device becomes online or offline, 
 * Note that sdev is locked
 * called by ndev_pnp_changed 
 */
void rddc_device_changed(logunit_t *raid, int slot, int online) 
{
    int index = raid_find_index(raid, slot);
    int flag;
    dbg_rddc(1, "raid=%d slot=%d online=%d index=%d", raid->info.slot, slot, online, index);

    sal_assert(sdev_is_locked(raid));
    sal_assert(index >= 0);
    if ( index < 0) {
        sdev_unlock(raid);
        return; 
    }
    if ( !online ) {
        dbg_rddc(1, "offline");
        sdev_unlock(raid);
        return;
    }
    if ( !raid->accept_io ) {
        dbg_rddc(1, "not enabled");
        sdev_unlock(raid);
        return;// TODO: race.
    }
    if ( raid->disks[index]->ops->status(raid->disks[index]) != CONN_STATUS_INIT ) {
        dbg_rddc(1, "not enabled status=%x", raid->disks[index]->ops->status(raid->disks[index]));
        sdev_unlock(raid);
        return;
    }
    flag = ENABLE_FLAG(raid->info.writable, raid->info.writeshared, TRUE, TRUE);
    raid->disks[index]->enabled_func = rddc_slot_changed_by_pnp;
    raid->disks[index]->arg = raid;
    raid->disks[index]->ops->enable(raid->disks[index],flag); /* uop_enable */
}

#endif

