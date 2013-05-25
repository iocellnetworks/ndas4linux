#ifndef NDAS_NO_LANSCSI

#include "xplatcfg.h"
#include <sal/debug.h>
#include <ndasuser/ndasuser.h>
#include "registrar.h"
#include "netdisk/sdev.h"
#include "netdisk/conn.h"
#include "raid/bitmap.h"
#include "raid/raid.h"
#include "udev.h"

#define NDAS_IO_DEFAULT_TIMEOUT (60*1000)

#ifdef DEBUG
#define    debug_sdev(l, x...)    do {\
    if(l <= DEBUG_LEVEL_SDEV) {     \
        sal_debug_print("SD|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)    
#define    debug_sdev_lock(l, x...)    do {\
    if(l <= DEBUG_LEVEL_SDEV_LOCK) {     \
        sal_debug_print("SDL|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0) 
#else
#define debug_sdev(l, x...) do {} while(0)
#define debug_sdev_lock(l, x...) do {} while(0)
#endif

#define MEMORY_FREE(ptr) if ( ptr ) { sal_free(ptr); ptr = NULL; }


struct ops_s *choose_op(int type) 
{
#ifdef XPLAT_RAID
    if ( type == NDAS_DISK_MODE_RAID0 ) {
        return &raid0;
    } 
    if ( type == NDAS_DISK_MODE_RAID1 ) {
        return &raid1;
    }
    if ( type == NDAS_DISK_MODE_RAID4 ) {
        return &raid5;
    }
#endif
    if ( type == NDAS_DISK_MODE_MEDIAJUKE) {
        return &udev_ops;
    }
    sal_assert(type == NDAS_DISK_MODE_SINGLE || type == NDAS_DISK_MODE_ATAPI);
    return &udev_ops;
}
/** 
 * Initialize the logunit_t * data structure.
 *
 * old : driver_init, udev_create
 * called by : 
 * 
 * To do: Create unit devices in discover and link them in here
 *           Use sperate function for creating logical device with single disk and multiple disk
 */
logunit_t *sdev_create(sdev_create_param_t* param) 
{
    ndas_error_t err;
    int ret;
    logunit_t *sdev = (logunit_t *) sal_malloc(sizeof(logunit_t));
    if ( !sdev ) 
        return NULL;

    debug_sdev(1, "ing mode=%d",  param->mode);
    sal_memset(sdev, 0, sizeof(logunit_t));
#ifdef DEBUG
    sdev->magic = SDEV_MAGIC;
#endif
    sdev->nmd = nmd_create();
    if ( !sdev->nmd) {
        goto out;
    }
    ret = sal_spinlock_create("s", &sdev->lock_mutex);
    if (!ret) {
        goto out0;
    }
    sdev->ops = choose_op(param->mode);
    sdev->info.mode = param->mode;
    sdev->info.timeout = NDAS_IO_DEFAULT_TIMEOUT;
    err = sdev->ops->create_disks(sdev, param); // uop_create, raid_create

    if ( !NDAS_SUCCESS(err) ) 
        goto out1;
    
    sdev->info.enabled = 0; // not called with ndas_enabled_device
    sdev->has_key = 0; /* unknown */
    sdev->lock = TRUE; // locked from the beginning
    
    err = sdev_register(sdev);
    if ( !NDAS_SUCCESS(err) ) {
        goto out2;
    }
    err = sdev->ops->init(sdev); // uop_init, r0op_init, r1op_init, r5op_init
    if ( !NDAS_SUCCESS(err) ) {
        goto out3;
    }
    debug_sdev(2, "ed sdev=%p",sdev);
    return sdev;    
out3:
    sdev_unregister(sdev);
out2:
    sdev->ops->destroy_disks(sdev); // uop_cleanup, raid_cleanup
out1:
    sal_spinlock_destroy(sdev->lock_mutex);
out0:
    nmd_destroy(sdev->nmd);
out:    
    MEMORY_FREE(sdev);
    debug_sdev(1, "err = %d",err);
    return NULL;
}

struct cleanup_arg {
    ndas_io_done disabled_func;
    void *arg;
    int *wait;
};

int sdev_cleanup(logunit_t *sdev, void *arg) 
{
    debug_sdev(1, "slot=%d",sdev->info.slot);
//    SDEV_REENTER(sdev, sdev_cleanup, sdev, NULL);
    while(!sdev_lock(sdev) )  {
        debug_sdev(1, "slot=%d is locked", sdev->info.slot);
        sal_msleep(200); // fix me!!
    }
    sdev_do_cleanup(sdev);

    return 0;
}

static NDAS_CALL void sdev_cleaning_up(int slot, ndas_error_t err, void *arg)
{
    logunit_t *sdev = sdev_lookup_byslot(slot);
    struct cleanup_arg *carg = (struct cleanup_arg *)arg;
    ndas_io_done disabled_func = carg->disabled_func;
    debug_sdev(1, "slot=%d",slot);
    sal_assert(sdev_is_locked(sdev));
    
    arg = carg->arg;
    
    sal_assert(sdev);
    sdev->ops->deinit(sdev); // uop_deinit, r0op_deinit
    sdev->ops->destroy_disks(sdev); //uop_cleanup, raid_cleanup
    sdev_unregister(sdev);
    if ( sdev->nmd ) nmd_destroy(sdev->nmd);
    if (sdev->info2) sal_free(sdev->info2);
    sdev->disabled_func = disabled_func;
    sdev->arg = arg;
    sdev_unlock(sdev);
    if ( disabled_func ) {
        debug_sdev(1, "slot=%d calling disabled_func=%p",slot,disabled_func );
        disabled_func(slot, err, arg);
    }
    sal_spinlock_destroy(sdev->lock_mutex);
    sal_free(sdev);
    (*carg->wait) = 0;
}

/* 
 * Destroy logunit_t * data structure.
 * Only called by ndas_unregister_device.
 * 1. disable the disks
 * 2. de allocate the memory allocated to sdev->disks
 * 3. unregister from the registrar
 */
void sdev_do_cleanup(logunit_t *sdev) 
{
    debug_sdev(2, "slot=%d",sdev->info.slot);
    sal_assert(sdev_is_locked(sdev));

    if ( sdev->info.enabled ) {
        struct cleanup_arg *carg;
        int wait = 1;
        carg = sal_malloc(sizeof(struct cleanup_arg));
        if ( !carg ) {
            debug_sdev(1, "OUT OF MEMORY TO clean up");
            sdev_unlock(sdev);
            return;// NDAS_ERROR_OUT_OF_MEMORY;
        }
        carg->arg = sdev->arg;
        carg->disabled_func = sdev->disabled_func;
        carg->wait = &wait;
        sdev->arg = carg;
        debug_sdev(2, " disabled_func=%p",sdev->disabled_func );
        sdev->disabled_func = sdev_cleaning_up;// TODO : change and wait till notify
    
        sdev->ops->disable(sdev, NDAS_ERROR_SHUTDOWN_IN_PROGRESS);     
        if ( int_wait(&wait, 1000, 0) == -1 ) {
            debug_sdev(1, "fail to disable cleanly");
            sdev_unlock(sdev);
            sal_free(carg);
            return;// NDAS_ERROR_TIME_OUT;
        }
        sal_free(carg);
        return;
    } else {
        ndas_io_done disabled_func = sdev->disabled_func;
        void *arg = sdev->arg;
            
        sdev->ops->deinit(sdev); // uop_deinit, r0op_deinit
        sdev->ops->destroy_disks(sdev); //uop_cleanup, raid_cleanup
        sdev_unregister(sdev);
        if ( sdev->nmd ) nmd_destroy(sdev->nmd);
        if (sdev->info2) sal_free(sdev->info2);
        sdev_unlock(sdev);
        if ( disabled_func ) {
            debug_sdev(2, "slot=%d calling disabled_func=%p",sdev->info.slot,disabled_func );
            disabled_func(sdev->info.slot, NDAS_ERROR_SHUTDOWN_IN_PROGRESS, arg);
        }
        sal_spinlock_destroy(sdev->lock_mutex);
        sal_free(sdev);
    }
}

static
void sdev_do_disable(logunit_t *sdev) 
{
    debug_sdev(2, "slot=%d", sdev->info.slot);
    
    if ( !sdev->info.enabled ) {
        ndas_io_done disabled_func = sdev->disabled_func;

        sdev_unlock(sdev);
        
        if ( disabled_func )
            disabled_func(sdev->info.slot, NDAS_ERROR_ALREADY_DISABLED_DEVICE, sdev->arg);
        return;
    }
    //uop_disable , jbod_op_disable, rddc_op_disable
    sdev->ops->disable(sdev, NDAS_ERROR_SHUTDOWN_IN_PROGRESS); 
}

int sdev_disable(logunit_t *sdev, void * reserved)
{
    debug_sdev(2, "slot=%d", sdev->info.slot);
    SDEV_REENTER(sdev, sdev_disable, sdev, NULL);
    sdev_do_disable(sdev);

    return 0;
}
#ifdef XPLAT_RAID
void sdev_queue_write_rmd(logunit_t *sdev) 
{
    int i = 0;
    debug_sdev(2, "sdev=%p",sdev);
    sal_assert(sdev);
    sal_assert(sdev->nmd);
    sal_assert(sdev->nmd->rmd);
    debug_sdev(2, "sdev->nmd->rmd=%p",sdev->nmd->rmd);
    sdev->nmd->rmd->u.s.usn++;
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) {
        if ( !sdev->disks[i] || !sdev->disks[i]->accept_io ) 
            continue;
        udev_queue_write_rmd(SDEV2UDEV(sdev->disks[i]), 0, sdev->nmd->rmd);
        debug_sdev(2, "1st [%d]child=%p {status=%x}",i,sdev->disks[i], 
            sdev->nmd->rmd->UnitMetaData[i].UnitDeviceStatus);
        
    }
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        if ( !sdev->disks[i] || !sdev->disks[i]->accept_io ) 
            continue;
        udev_queue_write_rmd(SDEV2UDEV(sdev->disks[i]), 1, sdev->nmd->rmd);
        debug_sdev(2, "2nd [%d]child=%p",i,sdev->disks[i]);
    }
}
#endif
ndas_metadata_t *nmd_create()
{
    ndas_metadata_t *ret = sal_malloc(sizeof(ndas_metadata_t));
    if ( !ret ) return NULL;
#ifdef XPLAT_RAID
    ret->rmd = NULL;
    ret->bitmap = NULL;
    ret->rmd_usn_match = 0;
#endif    
    ret->dib = sal_malloc(sizeof(NDAS_DIB_V2));
    if ( !ret->dib ) goto out;

    return ret;

out:
    sal_free(ret);
    return NULL;
}

#ifdef XPLAT_RAID    
ndas_error_t nmd_init_rmd(ndas_metadata_t *nmd)
{
    debug_sdev(3, "ing nmd=%p", nmd);
    sal_assert(nmd->dib);
    
    sal_assert(nmd->rmd == NULL);
    
    nmd->rmd = sal_malloc(sizeof(NDAS_RAID_META_DATA));
    if ( !nmd->rmd ) {
        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    debug_sdev(3, "nmd->rmd=%p", nmd->rmd);

    return NDAS_OK;
}
void nmd_free_bitmap(ndas_metadata_t *nmd)
{
    if ( nmd->bitmap ) bitmap_destroy(nmd->bitmap);
}
ndas_error_t nmd_init_bitmap(ndas_metadata_t *nmd)
{
    xuint32 size;
    debug_sdev(3, "ing nmd=%p", nmd);
    sal_assert(nmd->dib);
    sal_assert(nmd->bitmap == NULL);

    if ( sal_memcmp(nmd->dib->u.s.Signature,NDAS_DIB_V2_SIGNATURE,8) != 0 )
        return NDAS_ERROR_INVALID_METADATA;
    
    size = nmd->dib->u.s.sizeUserSpace >> ( bitmap_ffb(nmd->dib->u.s.iSectorsPerBit) + 3); 

    nmd->bitmap = bitmap_create(size);

    debug_sdev(3, "nmd->bitmap=%p", nmd->bitmap);
    
    if ( !nmd->bitmap ) 
        return NDAS_ERROR_OUT_OF_MEMORY;

    debug_sdev(3, "nmd->rmd=%p", nmd->rmd);

    return NDAS_OK;
}
#endif
void nmd_destroy(ndas_metadata_t *nmd)
{
    if ( !nmd ) return;
#ifdef XPLAT_RAID    
    debug_sdev(3, "nmd->rmd=%p", nmd->rmd);
    if ( nmd->rmd ) sal_free(nmd->rmd);
    debug_sdev(3, "nmd->bitmap=%p", nmd->bitmap);
    if ( nmd->bitmap ) bitmap_destroy(nmd->bitmap);
#endif    
    debug_sdev(3, "nmd->dib=%p", nmd->dib);
    if ( nmd->dib ) sal_free(nmd->dib);
    debug_sdev(3, "nmd=%p", nmd);
    sal_free(nmd);
}

xbool sdev_lock(logunit_t *sdev)
{
    sal_spinlock_take_softirq(sdev->lock_mutex);
    if ( sdev->lock ) {
        debug_sdev_lock(1,"slot=%d conflict", sdev->info.slot);
        sal_spinlock_give_softirq(sdev->lock_mutex);
        return FALSE;
    }
    sdev->lock = TRUE;
    debug_sdev_lock(2,"ed slot=%d", sdev->info.slot);
    sal_spinlock_give_softirq(sdev->lock_mutex);
    return TRUE;
}

void sdev_notify_disable_result(logunit_t *sdev, ndas_error_t err)
{
    ndas_io_done disabled_func = sdev->disabled_func;
    /* Bug
       1. enable
       2. meet bad sector
       3. enable again
       3. meet bad sector 
         no disable_func set detected*/
    //sdev->disabled_func = NULL;
    if ( !slot_is_the_raid(&sdev->info)) {
        sdev_set_disabled(sdev);
        sdev_unlock(sdev);
    }
    
    if ( disabled_func ) {
        /* ndcmd_disabled_handler, jb_slot_disabled, rd_slot_disabled */
        disabled_func(sdev->info.slot, err, sdev->arg);
    } else {
        debug_sdev(1, "No disabled_func for slot %d (%p)", sdev->info.slot, sdev);
    }
}

void sdev_raid_notify_disable_result(logunit_t *sdev, ndas_error_t err)
{
    sdev_notify_disable_result(sdev, err);
}


struct notify_arg {
    ndas_io_done noti_func;
    void *arg;
    logunit_t *sdev;
};

int notify_func(void *a, void *b)
{
    struct notify_arg *arg = a;
    long _b = (long)b;
    ndas_error_t err = (ndas_error_t)_b;

    arg->noti_func(arg->sdev->info.slot, err, arg->arg);
    sal_free(arg);
    return 0;
}
void sdev_notify_enable_result(logunit_t *sdev, ndas_error_t err)
{
    ndas_io_done enabled_func = sdev->enabled_func; 
    void *enabled_arg = sdev->arg;
    dpc_id dpcid;
    int ret;

#ifdef XPLAT_RAID
    if ( NDAS_SUCCESS(err) ) 
    {
        sdev_set_enabled(sdev);
        sdev_accept_io(sdev);
        if ( !slot_is_the_raid(&sdev->info) ) {
            //sal_error_print("ndas: slot %d is ready to use\n", sdev->info.slot);
        }
    }
    
    if ( !slot_is_the_raid(&sdev->info) ) {
        sdev_unlock(sdev);
    }
#else
    if ( NDAS_SUCCESS(err) ) {
        sdev_set_enabled(sdev);
        sdev_accept_io(sdev);
        //sal_error_print("ndas: slot %d is ready to use\n", sdev->info.slot);
    } else {
        sal_error_print("ndas: slot %d can't be enabled: %s\n", sdev->info.slot,
            NDAS_GET_STRING_ERROR(err));
    }
    sdev_unlock(sdev);
#endif    

    if (enabled_func) {
        struct notify_arg *arg = sal_malloc(sizeof(struct notify_arg));
        if ( arg == NULL ) {
            /* ndcmd_enabled_handler in Linux */
            enabled_func(sdev->info.slot, NDAS_ERROR_OUT_OF_MEMORY, enabled_arg);
            return;
        }
        arg->noti_func = enabled_func;
        arg->arg = enabled_arg;
        arg->sdev = sdev;

        dpcid = bpc_create(notify_func, arg, (void*)(long)err, NULL, 0);
        sal_assert(dpcid);
        if(dpcid) {
            ret = bpc_invoke(dpcid);
            sal_assert(ret >= 0);
        }
    }
    else {
        debug_sdev(1, "No enabled_func for slot %d (%p)", sdev->info.slot, sdev);
    }
    return;    
}

#endif /* #ifndef NDAS_NO_LANSCSI */

