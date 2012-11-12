#include "xplatcfg.h"

#ifdef XPLAT_RAID
#include <sal/mem.h>
#include <ndasuser/ndaserr.h>
#include <ndasuser/def.h>
#include <ndasuser/info.h>
#include "raid/raid.h"
#include "raid0.h"
#include "jbod.h"

#ifdef DEBUG
#define    dbg0(l, x...)    do {\
    if(l <= DEBUG_LEVEL_RAID0) {     \
        sal_debug_print("R0|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)        
#define    dbg_jbod(l, x...)    do {\
    if(l <= DEBUG_LEVEL_RAID0) {     \
        sal_debug_print("R0|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)        
#else
#define    dbg0(l, x...)    do {} while(0)
#define    dbg_jbod(l, x...)    do {} while(0)
#endif

LOCAL
void r0op_deinit(logunit_t *sdev) 
{
    raid_struct_free(sdev->private);
    sdev->private = NULL;
}

LOCAL
ndas_error_t r0op_query(logunit_t *sdev, ndas_metadata_t *nmd);
/* 
 * called by sdev_create
 */
LOCAL
ndas_error_t r0op_init(logunit_t *sdev)
{
    ndas_error_t err;
    dbg0(1, "sdev=%p", sdev);
    
    SDEV_RAID_INFO(sdev)->min_disks = SDEV_RAID_INFO(sdev)->disks;
    
    sdev->private = raid_struct_alloc();
    if ( !sdev->private ) 
        return NDAS_ERROR_OUT_OF_MEMORY;
    
    err = r0op_query(sdev, sdev->nmd);    
    if ( !NDAS_SUCCESS(err) ) {
        goto out;
    }
    return NDAS_OK;
out:
    r0op_deinit(sdev);
    return err;
}

LOCAL
ndas_error_t r0op_query(logunit_t *sdev, ndas_metadata_t *nmd) // uop_query
{
    int i = 0, count =0;
    ndas_error_t err;
    xbool writable = TRUE;
    unsigned int min_mspr = -1;
    ndas_metadata_t *nmd_local;
    ndas_metadata_t *n = NULL;
    nmd_local = nmd_create();
    if ( !nmd_local ) {
        dbg0(1, "out of memory");
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    
    for ( i = 0 ; i < SDEV_RAID_INFO(sdev)->disks; i++ )
    {
        logunit_t *child = sdev->disks[i];
        if ( !child ) continue;
        n = ( count == 0 ) ? nmd : nmd_local;
        
        err = child->ops->query(child, n);
        if ( !NDAS_SUCCESS(err) ) {
            goto out;
        }
        if ( !dib_compare(nmd->dib, n->dib) ) {
            err = NDAS_ERROR_INVALID_METADATA;
            goto out;
        }
        dbg0(1, "sectors=%Lu", child->info.sectors);
        if ( min_mspr > sdev->disks[i]->info.max_sectors_per_request )
            min_mspr = sdev->disks[i]->info.max_sectors_per_request;
        writable &= sdev->disks[i]->has_key;
        count++;
    }
    if ( count == 0 ) {
        err = NDAS_ERROR_TOO_MANY_OFFLINE; // should not be called if no device available.
        goto out;
    }
    sdev->has_key = writable;
    sdev->info.sectors = nmd->dib->u.s.sizeUserSpace * nmd->dib->u.s.nDiskCount;
    sdev->info.capacity = sdev->info.sectors << 9;
    sdev->info.sectors_per_disk = nmd->dib->u.s.sizeUserSpace;
    sdev->info.io_splits = nmd->dib->u.s.nDiskCount;
    sdev->info.mode = NDAS_DISK_MODE_RAID0;
    sdev->info.sector_size = (1 << 9) * nmd->dib->u.s.nDiskCount;
    sdev->info.max_sectors_per_request = min_mspr * nmd->dib->u.s.nDiskCount;
    dbg0(1, "capacity=%Lu", sdev->info.capacity);
    dbg0(1, "sector_size=%u", sdev->info.sector_size);
    err = NDAS_OK;
out:
    nmd_destroy(nmd_local);
    return err;
}
LOCAL
void r0op_io(logunit_t *sdev, urb_ptr orgtir, xbool urgent)
{
    int i;
    urb_ptr subtir;
    struct jbod_io_s *jio;
    dbg0(3, "sdev=%p orgtir=%p ", sdev, orgtir);
    dbg0(1, "req->uio=%p req->nr_uio=%d req->start_sec=%Lu num_sec=%u", 
        orgtir->req.uio, orgtir->req.nr_uio, orgtir->req.start_sec, orgtir->req.num_sec);

    jio = jbod_io_alloc();
    if ( !jio ) 
    {
        orgtir->req.done(sdev->info.slot, NDAS_ERROR_OUT_OF_MEMORY, orgtir->req.done_arg);
        tir_free(orgtir);
        return;
    }
    
    jio->nr_io = SDEV_RAID_INFO(sdev)->disks;
    jio->sdev = sdev;
    jio->orgtir = orgtir;
    for (i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        subtir = tir_alloc(orgtir->cmd, &orgtir->req);
        if ( !subtir ) {
            jio->err = NDAS_ERROR_OUT_OF_MEMORY;
            jbod_end_io(jio);
            continue;
        }
        subtir->arg = jio;
        subtir->req.nr_uio = orgtir->req.nr_uio / SDEV_RAID_INFO(sdev)->disks;
        subtir->req.uio = orgtir->req.uio + i*(subtir->req.nr_uio);
        subtir->req.done = jbod_has_ioed;
        subtir->req.done_arg = subtir;
        subtir->req.num_sec = orgtir->req.num_sec / SDEV_RAID_INFO(sdev)->disks;
        subtir->req.start_sec = orgtir->req.start_sec >> bitmap_ffb(SDEV_RAID_INFO(sdev)->disks);
        dbg0(1, "[%d]req->uio=%p req->nr_uio=%d req->start_sec=%Lu num_sec=%u", 
            i,
            subtir->req.uio, 
            subtir->req.nr_uio, 
            subtir->req.start_sec, 
            subtir->req.num_sec
        );
        sdev->disks[i]->ops->io(sdev->disks[i], subtir, urgent); // uop_io
        
    }
    return;
}

struct ops_s raid0 = {
    .create_disks = (create_disks_func) raid_create,
    .destroy_disks = (destroy_disks_func) raid_cleanup,
    .init = (init_func) r0op_init,
    .deinit = (deinit_func) r0op_deinit,
    .query = (query_func) r0op_query,
    .enable = (enable_func) jbod_op_enable,
    .disable = (disable_func) jbod_op_disable,
    .io = (io_func) r0op_io,
    .status = jbod_op_status,
    .changed = NULL,
};

#endif // XPLAT_RAID

