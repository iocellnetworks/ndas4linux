/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/


#include "xplatcfg.h" 
#include <sal/sal.h>
#include <sal/libc.h>
#include <sal/sync.h>
#include <sal/thread.h>
#include <sal/debug.h>
#include <sal/mem.h>
#include <sal/types.h>
#include <xlib/dpc.h>
#include <xlib/xhash.h>
#include <xlib/xbuf.h>
#include <xlib/gtypes.h> /* g_ntohl g_htonl g_ntohs g_htons */
#include <ndasuser/info.h>
#include <ndasuser/ndasuser.h>
#include <ndasuser/write.h>

#include "lpx/lpx.h"
#include "lpx/lpxutil.h"
#include "netdisk/serial.h"
#include "netdisk/netdisk.h"
#include "netdisk/netdiskid.h"
#include "netdisk/list.h"
#include "netdisk/ndasdib.h"

#include "netdisk/ndev.h"
#include "ndiod.h"
#include "ndpnp.h"
#include "registrar.h"

#include "netdisk/conn.h"
#include "netdisk/sdev.h"
#include "udev.h"
#include "ndpnp.h"

#define NDAS_PROBING_TIMEOUT 15*1000

#define NDAS_PNP_DEFAULT_TIMEOUT 10*1000
#define NDAS_REGISTER_DEFAULT_TIMEOUT 30*1000

#ifndef NDAS_NO_LANSCSI

#ifdef XPLAT_RAID
#include "raid/raid.h"
#ifndef XPLAT_ASYNC_IO
#error Asynchronous I/O should be enabled to support NDAS BIND 
#endif
#endif

#define STAT_NDIOD_INTERVAL (10 * SAL_TICKS_PER_SEC)
#define TH(i) ((i) == 1?"st":(i) == 2?"nd":(i) == 3?"rd":"th")

#ifdef DEBUG
#define    debug_nddev(l, x...)    do {\
    if(l <= DEBUG_LEVEL_NDDEV) {     \
        sal_debug_print("ND|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)        
#else
#define    debug_nddev(l, x...)    do {} while(0)
#endif

void * Urb_pool = NULL;

ndas_error_t 
ndas_block_core_init(void)
{
    Urb_pool = sal_create_mem_pool("ndas_urb", sizeof(struct udev_request_block));
    if(Urb_pool == NULL)
        return NDAS_ERROR_OUT_OF_MEMORY;

    return NDAS_OK;
}

ndas_error_t 
ndas_block_core_cleanup(void)
{
    if(Urb_pool)
        sal_destroy_mem_pool(Urb_pool);

    return NDAS_OK;
}

static ndas_error_t ndas_discover_device(const char* name);

/* return TRUE for valid key 
 * Parameters
 * ndas_idserial: the NDAS id to read the NDAS device(aka read key)
 * ndas_key: the NDAS key to write the NDAS device (aka write key)
 * ndas_device_id: [out] the NDAS device id (currently mac address)
 * writable: [out] whether the device can be written.
 */
LOCAL ndas_error_t 
get_network_id(const char* ndas_id, const char* ndas_key, xuchar* network_id, int* writable) 
{
    ndas_id_info        info;

    debug_nddev(3, "ing: %s %s", ndas_id, ndas_key);
    //
    //    inputs for decryption
    //
    // NDAS ID / NDAS Key(Write key)

    sal_memcpy(info.ndas_id[0], &ndas_id[0], 5);
    sal_memcpy(info.ndas_id[1], &ndas_id[5], 5);
    sal_memcpy(info.ndas_id[2], &ndas_id[10], 5);
    sal_memcpy(info.ndas_id[3], &ndas_id[15], 5);

    if (ndas_key) {
        sal_memcpy(info.ndas_key, ndas_key, 5);
    } else {
        sal_memset(info.ndas_key, 0, 5);
    }
    //
    //    two keys for decryption
    //
    sal_memcpy(info.key1, NDIDV1Key1, 8) ;
    sal_memcpy(info.key2, NDIDV1Key2, 8) ;
    
    if(DecryptNdasID(&info) == TRUE) {
#ifdef NDAS_CRYPTO
	 if (info.vid == NDIDV1VID_NETCAM &&
            info.reserved[0] == NDIDV1Rsv[0] &&
            info.reserved[1] == NDIDV1Rsv[1] &&
            info.random == NDIDV1Rnd) 
        {
            dbg_calln(9, "valid id&key(NetCam");
            /* Key is correct. */
            if ( network_id ) 
                memcpy(network_id, info.ndas_network_id, 6);
            if ( writable ) 
                *writable = info.bIsReadWrite ;
            return NDAS_OK;
	 }
#else
        if (info.vid == NDIDV1VID_DEFAULT &&
            info.reserved[0] == NDIDV1Rsv[0] &&
            info.reserved[1] == NDIDV1Rsv[1] &&
            info.random == NDIDV1Rnd) 
        {
            debug_nddev(9, "valid id&key");
            /* Key is correct. */
            if ( network_id ) 
                sal_memcpy(network_id, info.ndas_network_id, 6);
            if ( writable ) 
                *writable = info.bIsReadWrite ;
            return NDAS_OK;        
         } else if (info.vid == NDIDV1VID_SEAGATE &&
            info.reserved[0] == NDIDV1Rsv[0] &&
            info.reserved[1] == NDIDV1Rsv[1] &&
            info.random == NDIDV1Rnd) 
         {
            debug_nddev(9, "valid id&key(seagate");
            /* Key is correct. */
            if ( network_id ) 
                sal_memcpy(network_id, info.ndas_network_id, 6);
            if ( writable ) 
                *writable = info.bIsReadWrite ;
            return NDAS_OK;
       }
#endif
    }
    return NDAS_ERROR_INVALID_NDAS_ID;
}
#ifdef XPLAT_RAID
LOCAL void raid_set_slot_number(logunit_t *raid, int *slots)
{
    int i;
    for (i = 0; i < SDEV_RAID_INFO(raid)->disks; i++) 
    {
        SDEV_RAID_INFO(raid)->slots[i] = slots[i];
    }
}
LOCAL int raid_update(logunit_t *raid, void *arg)
{
    int try_lock = (int)arg;
    int i;
    xbool has_key = TRUE;
    debug_nddev(4, "raid=%p try_lock=%d", raid, try_lock);
    if ( try_lock )
        SDEV_REENTER(raid, raid_update, (void *)raid, (void *)FALSE);
    /* Update informations */
    
    debug_nddev(4, "disks=%d", SDEV_RAID_INFO(raid)->disks);
     
    for (i = 0; i < SDEV_RAID_INFO(raid)->disks; i++) 
    {
        logunit_t *single = raid->disks[i]; 
        if ( !single && SDEV_RAID_INFO(raid)->slots[i] >= NDAS_FIRST_SLOT_NR ) 
        {
            debug_nddev(1, "Discovered slot=%d index=%d", SDEV_RAID_INFO(raid)->slots[i], i);
            single = sdev_lookup_byslot(SDEV_RAID_INFO(raid)->slots[i]);
            raid->disks[i] = single;
        }
        debug_nddev(4, "disks[%d]=%p parent_slot=%d", i,single, raid->info.slot); 
        
        if ( !single ) 
            continue;
        
        debug_nddev(4, "disks[%d]-info2=%p parent_slot=%d", i,
            SDEV_UNIT_INFO(single), raid->info.slot); 
        SDEV_UNIT_INFO(single)->raid_slot = raid->info.slot;
        has_key &= single->has_key;
    }
    raid->has_key = has_key;

    return 0;
}

#if 0
LOCAL ndas_error_t raid_detect(logunit_t *single, xbool pnp) 
{
    unsigned int i;
    int slots[8];
    ndas_error_t err;
    logunit_t *raid = NULL;
    ndas_metadata_t *nmd;
    NDAS_DIB_V2 *dib;
    udev_t* udev = SDEV2UDEV(single);
    xbool raid_is_locked = FALSE;
    
    debug_nddev(3, "single=%p", single);
    
    nmd = nmd_create();
    if ( !nmd ) return NDAS_ERROR_OUT_OF_MEMORY;
    
    err = udev_query_unit(udev, nmd); 
    if ( !NDAS_SUCCESS(err) ) {
        debug_nddev(1, "ed out=%d", err);
        if ( err == NDAS_ERROR_TIME_OUT ) 
            err = NDAS_ERROR_NO_DEVICE;
        goto free_nmd;
    }
    
    dib = nmd->dib;
    sal_assert(dib);
    debug_nddev(5, "mode=%d", single->info.mode);
    if ( single->info.mode == NDAS_DISK_MODE_SINGLE ) {
        err = NDAS_OK;
        goto free_nmd;
    }
    sal_assert(sal_memcmp(dib->u.s.Signature, NDAS_DIB_V2_SIGNATURE,8) == 0);
    
    debug_nddev(5, "nDiskCount=%d", dib->u.s.nDiskCount);    
    for ( i = 0; i < dib->u.s.nDiskCount; i++) 
    {
        ndev_t* ndev = ndev_lookup_bynetworkid(dib->UnitDisks[i].MACAddr);
        if ( !ndev || ndev->info.nr_unit <= dib->UnitDisks[i].UnitNumber) 
        {
            debug_nddev(1, "not found %s", DBG_DIB_UNITDISK(dib->UnitDisks + i));
            slots[i] = NDAS_INVALID_SLOT;
            continue;
        }
        slots[i] = ndev->unit[dib->UnitDisks[i].UnitNumber]->info.slot;
        single = sdev_lookup_byslot(slots[i]);

        if ( !single ) {
            debug_nddev(2,"child slot(%d) not found", slots[i]);
            continue;
        }

        sal_assert(slots[i] == single->info.slot);
        
        if ( single->info.mode != NDAS_DISK_MODE_RAID0 
            && single->info.mode != NDAS_DISK_MODE_RAID1 
            && single->info.mode != NDAS_DISK_MODE_RAID4 
            && single->info.mode != NDAS_DISK_MODE_MIRROR
            && single->info.mode != NDAS_DISK_MODE_AGGREGATION )
        {
            debug_nddev(2,"slot mode is not raid but %d", single->info.mode);
            err = NDAS_ERROR_INVALID_METADATA;
            goto free_nmd;
        }

        if ( single->info.type != NDAS_UNIT_TYPE_HARD_DISK )
        {
            debug_nddev(2,"slot type is not HDD but %d", single->info.type);
            err = NDAS_ERROR_INTERNAL;
            goto free_nmd;
        }
        debug_nddev(4, "slot[%d]=%d{%s}", i, slots[i],
                        DBG_DIB_UNITDISK(dib->UnitDisks + i));

        if ( SDEV_UNIT_INFO(single)->raid_slot <= NDAS_INVALID_SLOT) {
            debug_nddev(2,"parent slot for %d not exists", slots[i]);
            continue;
        }
        debug_nddev(2,"parent slot (%d) exist", SDEV_UNIT_INFO(single)->raid_slot);
        raid = sdev_lookup_byslot(SDEV_UNIT_INFO(single)->raid_slot);
    }
    if ( !raid ) 
    {
        sdev_create_param_t param;
        param.mode = single->info.mode;
        param.u.raid.disk_count = dib->u.s.nDiskCount;
        sal_memcpy(param.u.raid.slots, slots, sizeof(slots));
        raid = sdev_create(&param);
        if ( !raid ) {
            err = NDAS_ERROR_OUT_OF_MEMORY;
            goto free_nmd;
        }
        if ( !pnp ) 
            sdev_unlock(raid);
        else
            raid_is_locked = TRUE;
    } else 
        raid_set_slot_number(raid, slots);
    
    if ( raid_is_locked ) 
        raid_update((void *)raid, (void *)FALSE);
    else
        if ( pnp ) 
            raid_update((void *)raid, (void *)TRUE);
        else
            raid_update((void *)raid, (void *)FALSE);
        
    debug_nddev(4, "mode=%d", single->info.mode);
    debug_nddev(4, "nr_disks=%d", dib->u.s.nDiskCount);
    err = NDAS_OK;
free_nmd:
    nmd_destroy(nmd);
    return err;
}
#endif

#endif

#ifdef XPLAT_RAID
int ndev_free_raid_(logunit_t *raid, logunit_t *sdev)
{
    int i, count = 0;
    debug_nddev(1,"raid slot=%d sdev slot=%d", raid->info.slot, sdev->info.slot);
    SDEV_REENTER(raid,  ndev_free_raid_, (void *)raid, (void *)sdev);

    for ( i = 0; i < SDEV_RAID_INFO(raid)->disks; i++) {
        if ( raid->disks[i] == sdev ) {
            debug_nddev(1, "removed slot=%d from raid slot %d",sdev->info.slot, raid->info.slot);
            raid->disks[i] = NULL;
            SDEV_RAID_INFO(raid)->slots[i] = NDAS_INVALID_SLOT;
        }
        if ( raid->disks[i] != NULL ) 
            count++;
    }
    if ( count == 0 ) {
        debug_nddev(1, "All slot is remove from raid %d, cleaning", raid->info.slot);
        sdev_do_cleanup(raid);
    } else {
        sdev_unlock(sdev);
    }

    return 0;
}

void ndev_free_raid(logunit_t *sdev)
{
    logunit_t *raid = sdev_lookup_byslot(SDEV_UNIT_INFO(sdev)->raid_slot);
    if ( !raid ) {
        debug_nddev(1, "No raid sdev is found for slot %d", SDEV_UNIT_INFO(sdev)->raid_slot);
        return;
    }
    ndev_free_raid_(raid, sdev);
}

#endif
/*
 * Destroy the unit disk's sdev object.
 * Called if 
 *  1. ndev is unregistered (but still in the registrar for probe list)
 *  2. ndev_cleanup
 */
void ndev_unit_cleanup(ndev_t *ndev) 
{
    int unit;
    debug_nddev(3, "ndev=%p",ndev);
    for (unit = 0; unit < ndev->info.nr_unit; unit++) 
    {
        logunit_t * sdev = ndev->unit[unit];
        if ( sdev == NULL ) {
            debug_nddev(1, "sdev=NULL:fix me");
            continue;
        }
        debug_nddev(1, "sdev slot=%d",ndev->unit[unit]->info.slot);
#ifdef XPLAT_RAID
        if ( slot_is_the_raid(&sdev->info) ) {
            ndev_free_raid(sdev);
        }
#else

#endif
        sdev_cleanup(sdev, NULL);
        ndev->unit[unit] = NULL;
    }
}

void ndev_cleanup(ndev_t *ndev)
{
    ndev_unit_cleanup(ndev);
#if 0
    debug_nddev(5, "ndev->break_lock=%p",ndev->break_lock);
    
    if ( ndev->break_lock != SAL_INVALID_SEMAPHORE ) {
        sal_semaphore_destroy(ndev->break_lock);
    } else {
        // There should be ndev->break_lock
        sal_assert(0);
    }
#endif    
//    xlib_hash_table_remove(v_slot.ndasid_2_device, ndev->info.ndas_idserial);
//    m2d_remove(ndev->network_id, FALSE);    
    sal_free(ndev);
}

LOCAL int ndev_reenter_offline_notify(logunit_t *sdev, void * arg)
{
    long slot = (long)arg;
    SDEV_REENTER(sdev, ndev_reenter_offline_notify, sdev, (void*) slot);
    if ( sdev->ops->changed )
        sdev->ops->changed(sdev, (int)slot, 0);
    else 
        sdev_unlock(sdev);

    return 0;
}
LOCAL int ndev_reenter_online_notify(logunit_t *sdev, void * arg)
{
    long slot = (long)arg;
    SDEV_REENTER(sdev, ndev_reenter_online_notify, sdev, (void*) slot);
    if ( sdev->ops->changed )
        sdev->ops->changed(sdev, (int)slot, 1);
    else 
        sdev_unlock(sdev);

    return 0;
}
#ifdef XPLAT_PNP
LOCAL void ndev_pnp_offline(ndev_t* ndev)
{
    int i;
    logunit_t *raid_sdev = NULL;
    for ( i = 0; i < ndev->info.nr_unit; i++) 
    {
        logunit_t *sdev;
        debug_nddev(1, "slot=%d", ndev->info.slot[i]);
        sdev = sdev_lookup_byslot(ndev->info.slot[i]);
        if ( !sdev ) continue;
        debug_nddev(1, "mode=%d", sdev->info.mode);

        if ( !slot_is_the_raid(&sdev->info) )
            continue;

        raid_sdev = sdev_lookup_byslot(SDEV_UNIT_INFO(sdev)->raid_slot);
        if ( !raid_sdev) continue;
        if ( raid_sdev->ops->changed ) {
            ndev_reenter_offline_notify(raid_sdev, (void *)(long)ndev->info.slot[i]);
            return;
        } 

        // rddc_device_changed
        if ( raid_sdev->ops->changed )
            raid_sdev->ops->changed(raid_sdev, ndev->info.slot[i], 0); 
    }
    
}

LOCAL void ndev_pnp_changed(const char *serial, const char *name, NDAS_DEV_STATUS online) 
{
    ndas_error_t err;
    ndev_t* ndev = ndev_lookup_byserial(serial);
    debug_nddev(3, "serial=%s online=%d", serial, online);
    if ( !ndev ) {
        debug_nddev(1, "not registered");
        return;
    }

    if ( name == NULL ) {
        /* Currently auto-registration is not implemented */
        return;
    }
    debug_nddev(1,  "ndas: registered device \"%s\" is status %d",name, online);
    
    if ( online == NDAS_DEV_STATUS_OFFLINE) {
        debug_nddev(1,  "ndas: device became offline");        
        ndev_pnp_offline(ndev);
        return;
    }

    err = ndas_discover_device(name);
    if (!NDAS_SUCCESS(err)) {
        debug_nddev(1,  "Failed to discover device %s", name);
        return;
    }    
}
#endif
static ndas_error_t 
ndev_register_device(ndev_t* ndev, xuint16 enable_bits)
{
    ndas_error_t err;
    debug_nddev(1, "ndev=%p network_id=%s",ndev, ndev->network_id);

    ndev->enable_flags = enable_bits;
    err = ndev_register(ndev);
    if ( !NDAS_SUCCESS(err) ) {
        debug_nddev(1, "clean up ndev");
        ndev_cleanup(ndev);
        return err;
    }

#ifdef XPLAT_PNP
    pnp_subscribe(ndev->info.ndas_serial, ndev_pnp_changed);
#endif

    /* 
        To do: 
            This is temp fix for start-up enabling. Fix to handle start-up enabling in user-side.
    */
    if (enable_bits) {
        //
        // Make status change handler is called and device is discovered.
        //
        ndev_notify_status_changed(ndev, NDAS_DEV_STATUS_UNKNOWN);
    }
    return NDAS_OK;
}

ndas_error_t register_internal(const char* name, const char *idserial, const char *ndas_key, xuint32 enable_bits, xbool use_serial)
{    
    xuchar network_id[SAL_ETHER_ADDR_LEN];
    ndas_error_t err;
    int writable = 0;
    ndev_t* ndev;
    
    if ( !name || !name[0] || sal_strnlen(name, NDAS_MAX_NAME_LENGTH) > NDAS_MAX_NAME_LENGTH ) 
        return NDAS_ERROR_INVALID_NAME;
    
    if ( !idserial ) 
        return NDAS_ERROR_INVALID_NDAS_ID;
    
    if ( ndev_lookup_byname(name) ) {
        return NDAS_ERROR_ALREADY_REGISTERED_NAME;
    }

    if (use_serial) {
        err = get_networkid_from_serial(network_id, (const char*)idserial);
        /* Do not allow writing when registerd with serial */
        writable = FALSE;
    } else {
        err = get_network_id(idserial, ndas_key, network_id, &writable);
    }
    if ( !NDAS_SUCCESS(err) ) {
        return err;
    }

    err = ndev_lookup(network_id, &ndev);
    if ( !NDAS_SUCCESS(err) ) {
        return err;
    }

    sal_assert(ndev);
    if ( ndev->info.name[0] ) {
        return NDAS_ERROR_ALREADY_REGISTERED_DEVICE;
    }

    sal_strncpy(ndev->info.name, name, NDAS_MAX_NAME_LENGTH);

    if ( writable ) {
        sal_strncpy(ndev->info.ndas_key, ndas_key, NDAS_KEY_LENGTH);
    } else {
        sal_memset(ndev->info.ndas_key,0,NDAS_KEY_LENGTH);
    }

    err = ndev_register_device(ndev, enable_bits);

    return err;
}

NDASUSER_API ndas_error_t 
ndas_register_device(const char* name, const char *id, const char *ndas_key)
{
    return register_internal(name, id, ndas_key, 0, FALSE);
}

NDASUSER_API 
ndas_error_t 
ndas_register_device_by_serial(const char* name, const char *serial)
{
    return register_internal(name, serial, NULL, 0, TRUE);
}

NDASUSER_API ndas_error_t 
    ndas_unregister_device(const char *name)
{
    int i = 0;
    ndev_t* ndev = ndev_lookup_byname(name);
    if ( !ndev ) 
        return NDAS_ERROR_INVALID_NAME;
    
    for (i = 0; i < ndev->info.nr_unit; i++) 
    {
        if ( ndev->unit[i] && ndev->unit[i]->info.enabled ) {
            return NDAS_ERROR_ALREADY_ENABLED_DEVICE;
        }
    }
#ifdef XPLAT_PNP
    pnp_unsubscribe(ndev->info.ndas_serial, TRUE);
#endif
    ndev_unregister(ndev);
    ndev_unit_cleanup(ndev);

    return NDAS_OK;    
}

struct enable_arg {
    int *wait;
    ndas_error_t err;
};

LOCAL NDAS_CALL void
enable_internal_done(int slot, ndas_error_t err, void *arg)
{
    struct enable_arg *earg = (struct enable_arg *)arg;
    logunit_t * lunit;
    debug_nddev(1,"slot=%d err=%d", slot, err);
    earg->err = err;
    (*earg->wait) = 0;
    lunit = sdev_lookup_byslot(slot);
//    sdev_unlock(lunit); 
}

/* 
 * Hold the thread until the flag is reset
 */

int int_wait(int *flag, sal_msec interval, int count)
{
    int c = 0;
    while((*flag) == 1) { // use polling to reduce semaphore usage count
        sal_msleep(interval);
        if ( c++ > count ) {        
            return -1;
        }
    }
    return 0;
}

/*
 * Enable the slot with given parameters.
 * 
 * Called by ndas_enable_slot funtions  (user thread)
 */
LOCAL
ndas_error_t 
enable_internal(int slot,
    xbool exclusive_writable, 
    xbool writeshare)
{
    int wait = 0;
    struct enable_arg earg = {&wait,NDAS_OK};
    logunit_t * sdev = (logunit_t *) sdev_lookup_byslot(slot);
    debug_nddev(3, "sdev=%p", sdev);
    if ( sdev == NULL ) {
        debug_nddev(1, "invalid slot");
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }
    if ( slot_is_the_raid(&sdev->info) ) {
	debug_nddev(1, "Disk is raid member!!!");
        return NDAS_ERROR_UNSUPPORTED_DISK_MODE;
    }
    /* If device is locked, it is being enabled/disabled */
    if ( !sdev_lock(sdev)) {
        /* Device may be in initializing or not yet discovered. */
        debug_nddev(1, "sdev is locked!!!");        
        return NDAS_ERROR_TRY_AGAIN;
    }
    debug_nddev(1, "sdev=%p slot=%d", sdev, slot);
    if ( sdev->info.enabled ) {
        debug_nddev(1, "already enabled");
        sdev_unlock(sdev);
        return NDAS_ERROR_ALREADY_ENABLED_DEVICE;
    }
    if ((exclusive_writable || writeshare) && sdev->has_key==FALSE) {
        debug_nddev(1, "No NDAS key:writemode=%d writeshare=%d has_key=%d"
            , exclusive_writable, writeshare, sdev->has_key);
        sdev_unlock(sdev);
        return NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
    }    
    /* Don't fire disable listener as the slot has not yet been enabled */
    sdev->notify_enable_result_on_disabled = FALSE;

    if ( !sdev->enabled_func ) {
        debug_nddev(1, "No enabled_func set. Using enable_internal_done instead");
        sdev->enabled_func = enable_internal_done;
        sdev->arg = &earg;
        wait = 1;
    }
    
    /* uop_enable, jbod_op_enable, rddc_op_enable */
    sdev->ops->enable(sdev, ENABLE_FLAG(exclusive_writable, writeshare, FALSE , FALSE));     

    /* sdev will be unlocked by enable notifying function or in error cases */
    
    if ( int_wait(&wait, 1000, 20) == -1 ) {
        sdev_unlock(sdev);
        return NDAS_ERROR_TIME_OUT;
    }
    
    return earg.err;
}

/* 
    Write-share version of ndas_enable_slot. In this mode, writing can be executed in read-only connection.
    And writing is executed only when NDAS device lock is held.(not implemented)
*/
NDASUSER_API ndas_error_t ndas_enable_writeshare(int slot)
{
    return enable_internal(slot, FALSE, TRUE);
}

NDASUSER_API ndas_error_t ndas_enable_slot(int slot)
{
    return enable_internal(slot, FALSE, FALSE);
}


NDASUSER_API ndas_error_t ndas_enable_exclusive_writable(int slot)
{
    return enable_internal(slot, TRUE, FALSE);
}

/** 
 * Description
 * Request to disable the NDAS unit device enabled by ndas_enable_slot function.
 * The result will be signalled by the disconnected handler
 * 
 * Parameters
 * slot:            NDAS device slot number, allocated when ndas_register_device is called
 */
NDASUSER_API ndas_error_t ndas_disable_slot(int slot)
{
    logunit_t * sdev = sdev_lookup_byslot(slot);
    if ( sdev == NULL ) {
        debug_nddev(1, "Invalid slot number %d\n", slot);
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }
    debug_nddev(3, "slot=%d",slot);

    if ( slot_is_the_raid(&sdev->info) ) {
        return NDAS_ERROR_UNSUPPORTED_DISK_MODE;
    }
    if ( !sdev->info.enabled ) {
        debug_nddev(1, "Trying to disable disabled slot");
        return NDAS_ERROR_ALREADY_DISABLED_DEVICE; // todo:
    }
    
    sdev_disable(sdev, 0);

    return NDAS_OK;
}

NDASUSER_API ndas_error_t 
    ndas_query_unit(int slot, ndas_unit_info_t* info)
{
    logunit_t *sdev = sdev_lookup_byslot(slot);

    debug_nddev(3, "slot=%d sdev=%p", slot, sdev);
    if ( sdev == NULL ) {
        debug_nddev(1, "invalid slot=%d", slot);
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

    sal_memcpy(info, SDEV_UNIT_INFO(sdev), sizeof(ndas_unit_info_t));
    return NDAS_OK;
}
/*
 * Obtain the information about the ndas device registered.
 * Parameters
 * slot: NDAS device slot number for the NDAS device in the NDAS device registrar
 * info: [out] information about the ndas device identified by physical_slot 
 */
NDASUSER_API ndas_error_t 
    ndas_query_slot(int slot, ndas_slot_info_t* info)
{
//    ndas_error_t err;
    logunit_t *sdev = sdev_lookup_byslot(slot);

    debug_nddev(3, "slot=%d sdev=%p", slot, sdev);
    if ( sdev == NULL ) {
        debug_nddev(1, "invalid slot=%d", slot);
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

    /* 
        To do: Actual informatin retrieval from device should be done another function.
        For timebeing, device/slot/unit information is already retreived in ndas_discover_device.
    */
#if 0
    /* uop_query, r0op_query, r1op_query, r5op_query*/
    err = sdev->ops->query(sdev, sdev->nmd);
    if ( !NDAS_SUCCESS(err) )
        return err;
#endif
    
    sal_assert(info!=&sdev->info);

    sal_memcpy(info, &sdev->info, sizeof(ndas_slot_info_t));
    if (sdev->info.mode == NDAS_DISK_MODE_SINGLE)  {
        /* Adjust user accessible area */
        /* To do: seperate internal ndas_slot_info and user ndas_slot_info */
        info->sectors = sdev->info.sectors - NDAS_BLOCK_SIZE_XAREA;
        info->capacity = info->sectors * sdev->info.sector_size;
    }
    debug_nddev(3, "ed");
    return NDAS_OK;
}

NDASUSER_API ndas_error_t 
ndas_read(int slot, ndas_io_request_ptr req)
{
    return ndas_io(slot, ND_CMD_READ, req, TRUE);
}

NDASUSER_API ndas_error_t 
ndas_write(int slot, ndas_io_request_ptr ioreq)
{
    return ndas_io(slot, ND_CMD_WRITE, ioreq, TRUE);
}

NDASUSER_API ndas_error_t 
ndas_flush(int slot, ndas_io_request_ptr req)
{
    return ndas_io(slot, ND_CMD_FLUSH, req, TRUE);
}

NDASUSER_API ndas_error_t 
ndas_packetcmd(int slot, ndas_io_request_ptr req)
{
    return ndas_io_pc(slot,  req, TRUE);
}



NDASUSER_API 
ndas_error_t 
ndas_set_slot_handlers(int slot, 
    ndas_io_done enabled_func,
    ndas_io_done disabled_func,
    ndas_permission_request_handler write_func, 
    void* arg)
{
    logunit_t * sdev = sdev_lookup_byslot(slot);
    if ( sdev == NULL ) {
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }
    sdev->enabled_func = enabled_func;
    sdev->disabled_func = disabled_func;
#ifdef XPLAT_NDASHIX
    sdev->surrender_request_handler = write_func;    
#endif
    
    sdev->arg = arg;

    if ( slot_is_the_raid(&sdev->info) )
        return NDAS_OK;
    debug_nddev(1,"mode=0x%x", sdev->info.mode);

#if 0
    if ( !slot_is_the_raid(&sdev->info) ) {
        enabled_bits = ( SDEV2UDEV(sdev)->ndev->enable_flags >> (sdev->info.unit*2)) & REGDATA_BIT_MASK;
    } else {
        // TODO: think about enabling the raid on boot time.
    }
    debug_nddev(1,"enabled_bits=0x%x", enabled_bits);
    if ( enabled_func ) {
        switch ( enabled_bits ) {
        case REGDATA_BIT_SHARE_ENABLED:
            ndas_enable_writeshare(slot);
            break;
        case REGDATA_BIT_WRITE_ENABLED:
            ndas_enable_exclusive_writable(slot);
            break;
        case REGDATA_BIT_READ_ENABLED:
            ndas_enable_slot(slot);
            break;
        }
    }
#endif
    return NDAS_OK;
}

NDASUSER_API
ndas_error_t 
ndas_detect_device(const char* name) 
{
    ndev_t* ndev;
    ndev = ndev_lookup_byname(name);
    if (ndev) {
        ndev_notify_status_changed(ndev, NDAS_DEV_STATUS_UNKNOWN);
        return NDAS_OK;
    } else {
        return NDAS_ERROR_INVALID_NAME;
    }
}

/*
    Get device information such as number of unit, DIB 
*/
static
ndas_error_t 
ndas_discover_device(const char* name) 
{
    ndas_error_t err = NDAS_OK;
    int i = 0;
    ndev_t* ndev;
    uconn_t conn[2] = {{0}};  // We need seperate connection to get each inform from each disk.
    udev_t * udev;
    logunit_t* ldev;
    ndas_slot_info_t* sinfo;
    const lsp_ata_handshake_data_t* hsdata;
    sdev_create_param_t sparam;
    lsp_text_target_list_t targetList;

    debug_nddev(1, "Discovering %s..", name);
    ndev = ndev_lookup_byname(name);

    if ( ndev == NULL ) 
    {
        debug_nddev(1, "NDAS_ERROR_INVALID_NAME name=%s", name);
        return NDAS_ERROR_INVALID_NAME;
    }
    
    debug_nddev(5, "ndev=%p",ndev);
    /* 
        Connect to device and 
        update device info and slot info
    */
    err = conn_init(&conn[0], ndev->network_id, 0, NDAS_CONN_MODE_READ_ONLY, NULL, NULL);
    if ( !NDAS_SUCCESS(err)) {
        /* This does not happen */
        return err;
    }
    err = conn_init(&conn[1], ndev->network_id, 1, NDAS_CONN_MODE_READ_ONLY, NULL, NULL);
    if ( !NDAS_SUCCESS(err)) {
        conn_clean(&conn[0]);
        /* This does not happen */
        return err;
    }
    err = conn_connect(&conn[0], NDAS_PROBING_TIMEOUT);
    if ( !NDAS_SUCCESS(err)) {
        debug_nddev(1, "Connect failed");        
        /* Connect failed */
        ndev->info.version = NDAS_VERSION_UNKNOWN;
        ndev->info.nr_unit = 0;
        /* 
          * Determine proper status
          */
#ifdef XPLAT_PNP        
        if (sal_tick_sub(sal_get_tick() , ndev->last_tick + PNP_ALIVE_TIMEOUT) <= 0) {
            //
            // PNP packet is received recently.
            //
            if (NDAS_ERROR_CONNECT_FAILED == err) {
                ndev->info.status = NDAS_DEV_STATUS_CONNECT_FAILURE;
            } else {
                ndev->info.status = NDAS_DEV_STATUS_LOGIN_FAILURE;
            }
        } else {
            ndev->info.status = NDAS_DEV_STATUS_OFFLINE;
        }
#else
        if (NDAS_ERROR_CONNECT_FAILED == err) {
            ndev->info.status = NDAS_DEV_STATUS_OFFLINE;
        } else {
            ndev->info.status = NDAS_DEV_STATUS_LOGIN_FAILURE;
        }
#endif
        conn_clean(&conn[0]);
        conn_clean(&conn[1]);
        return NDAS_ERROR;
    } 

    /* Connection is successful */
#ifdef XPLAT_PNP
    ndev->last_tick = sal_get_tick();
#endif
    ndev->info.version = conn[0].hwdata->hardware_version;
    ndev->info.status = NDAS_DEV_STATUS_HEALTHY;
    err = conn_text_target_list(&conn[0], &targetList, sizeof(lsp_text_target_list_t));
    if ( !NDAS_SUCCESS(err)) {
        debug_nddev(1, "Text target list failed");
        ndev->info.status = NDAS_DEV_STATUS_TARGET_LIST_FAILURE;
        conn_disconnect(&conn[0]);
        conn_clean(&conn[0]);
        conn_clean(&conn[1]);
        return NDAS_ERROR;        
    }
    ndev->info.nr_unit = targetList.number_of_elements;

    debug_nddev(1, "ndas: discovered %d unit device from %s", ndev->info.nr_unit, name);

    if (ndev->info.nr_unit <1 || ndev->info.nr_unit >2) {
        ndev->info.nr_unit = 0;
        sal_assert(FALSE);
    }
    
    if (ndev->info.nr_unit == 2) {
        err = conn_connect(&conn[1], NDAS_PROBING_TIMEOUT);
        if ( !NDAS_SUCCESS(err)) {
            debug_nddev(1, "Connecting to second device failed");
            ndev->info.status = NDAS_DEV_STATUS_CONNECT_FAILURE;
            conn_logout(&conn[0]);
            conn_disconnect(&conn[0]);
            conn_clean(&conn[0]);
            conn_clean(&conn[1]);
            return NDAS_ERROR;
        }
    }

    for (i = 0; i < ndev->info.nr_unit; i++)
    {
        if ( ndev->unit[i] ) {
            debug_nddev(1, "Device %d is already created ",i);
            /* Device is already created. */
            /* To do: check device information has changed */
            continue;
        }
        /* To do: 
            Don't create sdev here.
            Detect first and create sdev only for single type device 
            Keep old code during refactoring.
        */
        sparam.mode = NDAS_DISK_MODE_SINGLE;
        sparam.u.single.ndev = ndev;
        sparam.u.single.unit =i;
        ndev->unit[i] = sdev_create(&sparam);
        
        /* sdev is locked when created. Unlock after discovering */
        
        if ( !ndev->unit[i] ) {
            int j=i;
            debug_nddev(1, "out of memory on %dth init",i);
            while ( --j >= 0 ) {
                if ( ndev->unit[j] ) {
                    sdev_cleanup(ndev->unit[j], NULL);
                    ndev->unit[j] = NULL;
                }
            }
            break;
        }

        ldev = ndev->unit[i];
        sinfo = &ldev->info;
        err  = conn_handshake(&conn[i]);
        if ( !NDAS_SUCCESS(err) ) {
            sinfo->mode = NDAS_DISK_MODE_UNKNOWN;
            ndev->info.status = NDAS_DEV_STATUS_IO_FAILURE;
            sdev_unlock(ndev->unit[i]);
            err = NDAS_ERROR;
            goto out;
        }

        udev = SDEV2UDEV(ldev);
        sal_assert(udev);
        /* 
         * Fill ndas_slot info and ndas_unit_info_t 
         */
        hsdata = lsp_get_ata_handshake_data(conn[i].lsp_handle);

        udev->uinfo->sectors = hsdata->lba_capacity.quad;
        udev->uinfo->capacity = hsdata->lba_capacity.quad * hsdata->logical_block_size;
        
        udev->uinfo->headers_per_disk = hsdata->num_heads;
        udev->uinfo->cylinders_per_header = hsdata->num_cylinders;
        udev->uinfo->sectors_per_cylinder = hsdata->num_sectors_per_track;
        
        sal_memcpy(udev->uinfo->serialno, hsdata->serial_number, sizeof(udev->uinfo->serialno));
        sal_memcpy(udev->uinfo->model, hsdata->model_number, sizeof(udev->uinfo->model));
        sal_memcpy(udev->uinfo->fwrev, hsdata->firmware_revision, sizeof(udev->uinfo->fwrev));

        sinfo->sector_size = 1 << 9;
        sinfo->capacity = udev->uinfo->sectors * sinfo->sector_size;
        sinfo->sectors = udev->uinfo->sectors;
        sinfo->max_sectors_per_request = 16 * 1024 * 1024 / 512;

        if (hsdata->device_type == 0) {
            PNDAS_DIB_V2 dib;
            xuint32 size =sizeof(NDAS_DIB_V2);
            dib = sal_malloc(size);
            if(dib == NULL) {
                sinfo->mode = NDAS_DISK_MODE_UNKNOWN;
                ndev->info.status = NDAS_DEV_STATUS_IO_FAILURE;
                debug_nddev(1, "Error allocate DIB memory");
                goto out;
            }
            /* ATA */
            udev->uinfo->type = NDAS_UNIT_TYPE_HARD_DISK;
            err = conn_read_dib(&conn[i], sinfo->sectors, dib, &size);
            if ( !NDAS_SUCCESS(err) ) {
                sal_free(dib);
                sinfo->mode = NDAS_DISK_MODE_UNKNOWN;
                ndev->info.status = NDAS_DEV_STATUS_IO_FAILURE;
                debug_nddev(1, "Error reading DIB %d", err);
                goto out;
            }
            
            debug_nddev(5, "mediaType=%u", dib->u.s.iMediaType);
            if ( dib->u.s.iMediaType == NMT_RAID0 ) 
            {
                sinfo->mode = NDAS_DISK_MODE_RAID0;
                sinfo->mode_role = dib->u.s.iSequence;
            } else if ( dib->u.s.iMediaType == NMT_RAID1 ) {
                sinfo->mode = NDAS_DISK_MODE_RAID1;
                sinfo->mode_role = dib->u.s.iSequence;
            } else if ( dib->u.s.iMediaType == NMT_RAID4 ) {
                sinfo->mode = NDAS_DISK_MODE_RAID4;
                sinfo->mode_role = dib->u.s.iSequence;
            } else if ( dib->u.s.iMediaType == NMT_AGGREGATE ) {
                sinfo->mode = NDAS_DISK_MODE_AGGREGATION;
                sinfo->mode_role = dib->u.s.iSequence;
            } else if ( dib->u.s.iMediaType == NMT_MIRROR ) {
                sinfo->mode = NDAS_DISK_MODE_MIRROR; // TODO convert into raid1
                sinfo->mode_role = dib->u.s.iSequence;
            } else if ( dib->u.s.iMediaType == NMT_MEDIAJUKE) {
                sal_debug_println("NDAS_DISK_MODE_MEDIAJUKE");
                sinfo->mode = NDAS_DISK_MODE_MEDIAJUKE; 
            } else {
                sinfo->mode = NDAS_DISK_MODE_SINGLE;
            }
            sal_free(dib);
            err = NDAS_OK;
        } else {
            debug_nddev(0, "ATAPI detected");
            /* ATAPI */
            udev->uinfo->type = NDAS_UNIT_TYPE_ATAPI;
            sinfo->mode = NDAS_DISK_MODE_ATAPI;
            err = NDAS_OK;
        }
        sdev_unlock(ndev->unit[i]);
    }
/* To do: Detect changes in RAID configuration */
#if 0
#ifdef XPLAT_RAID
            err = raid_detect(ndev->unit[i], pnp);
            debug_nddev(5, "err=%d i=%d", err, i);
            if ( !NDAS_SUCCESS(err) )
                goto out;
            sdev_unlock(ndev->unit[i]);
    for ( i = 0; i < ndev->info.nr_unit; i++)
    {
        logunit_t *sdev;
        if ( newborn ) {
            debug_nddev(1, "ndas: slot %d is assigned for %d%s unit disk of \"%s\"\n"
                ,ndev->unit[i]->info.slot, i+1, TH(i+1), name);
        }
        debug_nddev(1, "slot=%d", ndev->info.slot[i]);
        sdev = sdev_lookup_byslot(ndev->info.slot[i]);
        if ( !sdev ) continue;
        debug_nddev(1, "mode=%d", sdev->info.mode);

        if ( !slot_is_the_raid(&sdev->info) )
            continue;

        raid_sdev = sdev_lookup_byslot(SDEV_UNIT_INFO(sdev)->raid_slot);
        if ( !raid_sdev) continue;
        if ( !newborn ) {
            sal_assert(!sdev_is_locked(raid_sdev));
            if ( raid_sdev->ops->changed ) {
                ndev_reenter_online_notify(raid_sdev, (void *)(long)ndev->info.slot[i]);
                return;
            }
        }
        if ( raid_sdev->ops->changed )
            raid_sdev->ops->changed(raid_sdev, ndev->info.slot[i], online); // rddc_device_changed
    }
#endif
#endif
out:
    for (i = 0; i < ndev->info.nr_unit; i++) {
        conn_logout(&conn[i]);
        conn_disconnect(&conn[i]);
        conn_clean(&conn[i]);
    }
    debug_nddev(3, "nr_unit=%d",ndev->info.nr_unit);
    return err;
}

NDASUSER_API
ndas_error_t 
ndas_get_ndas_dev_info(const char* name, ndas_device_info* info) 
{
    ndev_t* ndev = ndev_lookup_byname(name);
    if ( ndev == NULL ) 
    {
        debug_nddev(1, "NDAS_ERROR_INVALID_NAME name=%s", name);
        return NDAS_ERROR_INVALID_NAME;
    }
    sal_memcpy(info,&ndev->info, sizeof(ndas_device_info));
    /* Hide last five char of ID */
    sal_strcpy(&info->ndas_id[15], "*****");
    debug_nddev(3, "nr_unit=%d",ndev->info.nr_unit);
    return NDAS_OK;
}

NDASUSER_API 
ndas_error_t 
ndas_set_registration_name(const char* name, const char *newname) 
{
    ndev_t* dev = ndev_lookup_byname(name);
    if ( dev == NULL ) return NDAS_ERROR_INVALID_NAME;
    return ndev_rename(dev,newname);
}

#ifdef XPLAT_NDASHIX
#include "nhix.h"
NDASUSER_API
ndas_error_t
ndas_request_permission(int slot) 
{
    logunit_t * sdev = sdev_lookup_byslot(slot);
    if ( sdev == NULL ) return NDAS_ERROR_INVALID_SLOT_NUMBER;
    if ( sdev->info.mode == NDAS_DISK_MODE_SINGLE )
        return nhix_send_request(SDEV2UDEV(sdev));
    return NDAS_ERROR_NOT_IMPLEMENTED;
    // TODO: check if the proper status.
}
#else
NDASUSER_API
ndas_error_t
ndas_request_permission(int slot) 
{
    return NDAS_OK;
}
#endif

#else /* #ifndef NDAS_NO_LANSCSI */

NDASUSER_API ndas_error_t 
    ndas_unregister_device(const char *name)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API 
ndas_error_t 
ndas_set_registration_name(const char* name, const char *newname) 
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API 
ndas_error_t 
ndas_set_slot_handlers(int slot, 
    ndas_io_done enabled_func,
    ndas_io_done disabled_func, 
    ndas_permission_request_handler write_func, 
    void* arg)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API ndas_error_t 
    ndas_query_unit(int slot, ndas_unit_info_t* info)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}
NDASUSER_API ndas_error_t 
ndas_register_device(const char* name, const char *id, const char *ndas_key)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API 
ndas_error_t 
ndas_register_device_by_serial(const char* name, const char *serial)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}
NDASUSER_API ndas_error_t ndas_enable_exclusive_writable(int slot)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API
ndas_error_t
ndas_request_permission(int slot) 
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API
ndas_error_t
    ndas_read(int slot, ndas_io_request_ptr req)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API ndas_error_t 
    ndas_write(int slot, ndas_io_request_ptr ioreq)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API ndas_error_t ndas_enable_slot(int slot)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API ndas_error_t ndas_disable_slot(int slot)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API ndas_error_t 
    ndas_query_slot(int slot, ndas_slot_info_t* info)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API ndas_error_t 
ndas_detect_device(const char* name) 
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API
ndas_error_t 
ndas_get_ndas_dev_info(const char* name, ndas_device_info* info) 
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

NDASUSER_API ndas_error_t ndas_enable_writeshare(int slot)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

#endif /* #ifndef NDAS_NO_LANSCSI */ 
