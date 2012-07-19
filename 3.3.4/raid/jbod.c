#include "xplatcfg.h"

#ifdef XPLAT_RAID
#include <sal/mem.h>
#include "raid/raid.h"
#include "jbod.h"

#ifdef DEBUG
#define    dbg_jbod(l, x...)    do {\
    if(l <= DEBUG_LEVEL_JBOD) {     \
        sal_debug_print("JB|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)        
#else
#define    dbg_jbod(l, x...)    do {} while(0)
#endif

void jbod_io_free(struct jbod_io_s * jio)
{
    sal_free(jio);
}

struct jbod_io_s * jbod_io_alloc()
{
    struct jbod_io_s * ret = sal_malloc(sizeof(struct jbod_io_s));
    if ( !ret ) 
        return NULL;
    sal_memset(ret, 0, sizeof(struct jbod_io_s));
    sal_atomic_set(&ret->count, 0);
    return ret;
}

int jbod_op_status(logunit_t *sdev)
{
    int i = 0; 
    int status = 0;
    for ( i = 0 ; i < SDEV_RAID_INFO(sdev)->disks; i ++ )
    {
        if ( sdev->disks[i] )
            status |= sdev->disks[i]->ops->status(sdev->disks[i]);
    }
    if ( status & CONN_STATUS_INIT) 
        return CONN_STATUS_INIT;
    
    if ( status & CONN_STATUS_SHUTING_DOWN )
        return CONN_STATUS_SHUTING_DOWN;
    
    if ( status & CONN_STATUS_CONNECTING )
        return CONN_STATUS_CONNECTING;
    
    return CONN_STATUS_CONNECTED;
}
LOCAL
void jb_all_disabled_on_running(int slot, ndas_error_t err, void *arg)
{
    logunit_t *sdev = (logunit_t *) arg;
    struct raid_struct *rc = SDEV_RAID(sdev);
    int index = raid_find_index(sdev, slot);
    dbg_jbod(1, "slot=%d err=%d", slot,err);
    
    sal_assert(sdev_is_locked(sdev));

    sdev_set_disabled(sdev->disks[index]);
    sdev_dont_accept_io(sdev->disks[index]);
    sdev->disks[index]->disabled_func = NULL;
    
    if ( sal_atomic_dec_and_test(&SDEV_RAID(sdev)->op_count) == 0 )
        return;       

    sal_assert(!NDAS_SUCCESS(rc->op_err));
    sdev_notify_disable_result(sdev, rc->op_err); 
}
LOCAL
void jb_slot_disabled_on_running(int slot, logunit_t *sdev)
{
    struct raid_struct *rc = SDEV_RAID(sdev);
    int index = raid_find_index(sdev, slot);
    dbg_jbod(1, "slot=%d count=%d", slot, sal_atomic_read(&rc->op_count));

    sal_assert(sdev_is_locked(sdev));
    
    sdev_set_disabled(sdev->disks[index]);

    sdev_dont_accept_io(sdev);
    raid_stop_disks(sdev,rc->op_err, FALSE, jb_all_disabled_on_running);
}
/* 
 * called by 
 *  udev_shutdown when the disk thread is dead
 */
LOCAL
void jb_slot_disabled(int slot, ndas_error_t err,void* arg)
{
    logunit_t *sdev = (logunit_t *)arg;
    int index = raid_find_index(sdev, slot);
    struct raid_struct *rc = SDEV_RAID(sdev);
    dbg_jbod(1, "slot=%d err=%d count=%d", slot, err, sal_atomic_read(&rc->op_count));

    if ( !NDAS_SUCCESS(err) ) {
        rc->op_err = err;
    }
    if ( err != NDAS_ERROR_SHUTDOWN_IN_PROGRESS && !sdev_is_locked(sdev) ) {
        if ( sdev_lock(sdev) ) {
            jb_slot_disabled_on_running(slot, sdev);
            return;    
        }
    }
    sal_assert(sdev_is_locked(sdev));
    
    sdev_set_disabled(sdev->disks[index]);
    
    
    if ( sal_atomic_dec_and_test(&rc->op_count) == 0) {
        return;
    }
    sdev_notify_disable_result(sdev, rc->op_err);
}

LOCAL 
void jb_all_enabled(logunit_t *sdev)
{
    struct raid_struct *rc = SDEV_RAID(sdev);

    if ( NDAS_SUCCESS(rc->op_err) ) {
        sdev->info.writable = rc->op_writable;
        sdev->info.writeshared = rc->op_writeshared;
        sdev_notify_enable_result(sdev, rc->op_err);
        return;
    }
    sal_assert(sdev->accept_io == 0);
    raid_stop_disks(sdev, rc->op_err, TRUE, raid_disabled_on_failing_to_enable);
}

/*
 * jbod_op_enable if the disk is not online/registered
 */
LOCAL
void jb_slot_enabled(int slot, ndas_error_t err,void* arg)
{
    logunit_t *sdev = (logunit_t *) arg;
    struct raid_struct *rc = SDEV_RAID(sdev);
    int i = raid_find_index(sdev, slot);
    
    if ( !NDAS_SUCCESS(err) ) {
        rc->op_err = err;
    } else {
        sdev->disks[i]->disabled_func = jb_slot_disabled;
    }

    if ( sal_atomic_dec_and_test(&rc->op_count) == 0 ) {
        return;
    }
    jb_all_enabled(sdev);
}

void jbod_op_enable(logunit_t *sdev, int flags)
{
    int i;
    xbool writable = ENABLE_FLAG_WRITABLE(flags);
    xbool writeshare = ENABLE_FLAG_WRITESHARE(flags);
    dbg_jbod(1, "sdev=%p disks=%d", sdev, SDEV_RAID_INFO(sdev)->disks);

    sal_assert(sdev_is_locked(sdev));
    
    SDEV_RAID(sdev)->op_writable = writable;
    SDEV_RAID(sdev)->op_writeshared = writeshare;
    SDEV_RAID(sdev)->op_err = NDAS_OK;
    
    for (i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        if ( !sdev->disks[i] ) {
            goto out;
        }
    }
    sal_atomic_set(&SDEV_RAID(sdev)->op_count, SDEV_RAID_INFO(sdev)->disks);
    for (i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {           
        sdev->disks[i]->enabled_func = jb_slot_enabled;
        sdev->disks[i]->arg = sdev;
        /* uop_enable */
        sdev->disks[i]->ops->enable(sdev->disks[i], ENABLE_FLAG(writable, writeshare, TRUE, FALSE)); 
    }
    return;
out:
    sdev_notify_enable_result(sdev, NDAS_ERROR_TOO_MANY_OFFLINE);
    return;
}
/* 
 *
 * called by sdev_disable
 */
void jbod_op_disable(logunit_t *sdev, ndas_error_t err)
{
    raid_stop_disks(sdev, err, FALSE, jb_slot_disabled);
}
void jbod_end_io(struct jbod_io_s *jio) 
{
    urb_ptr orgtir = jio->orgtir;
    logunit_t *sdev = jio->sdev;   
    
    if ( sal_atomic_inc(&jio->count) < jio->nr_io) 
        return;
    
    orgtir->req.done(sdev->info.slot, jio->err, orgtir->req.done_arg);
    tir_free(orgtir);
    jbod_io_free(jio);
}

NDAS_CALL void jbod_has_ioed(int slot, ndas_error_t err,void* user_arg) 
{
    urb_ptr subtir = (urb_ptr) user_arg;
    struct jbod_io_s *jio = (struct jbod_io_s *) subtir->arg;
    dbg_jbod(5, "err=%d subtir=%p jio=%p", err, subtir, jio);
    if ( !NDAS_SUCCESS(err)) 
    {
        jio->err = err;
    }
    jbod_end_io(jio);
}

#endif /* XPLAT_RAID*/

