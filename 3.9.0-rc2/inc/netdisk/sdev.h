#ifndef _NETDISK_SDEV_H_
#define _NETDISK_SDEV_H_

#include <sal/types.h>
#include <sal/sync.h>
#include <ndasuser/ndasuser.h>
#include <ndasuser/ndaserr.h>
#include <ndasuser/def.h>
#include <ndasuser/info.h>
#include "../netdisk/udev.h" /* to do: move raid code to 
netdisk and move all netdisk header to source dir */
#include "netdisk/ndev.h"
#include "netdisk/ndasdib.h"
#ifdef XPLAT_RAID
#include "raid/bitmap.h"
#endif
#ifdef DEBUG
#define SDEV2UDEV(sdev) ({ \
    sal_assert(sdev); \
    sal_assert((sdev)->disks); \
    sal_assert(((udev_t *)(sdev)->disks)->magic == UDEV_MAGIC); \
    ((udev_t *)(sdev)->disks); \
})
#else
#define SDEV2UDEV(sdev) ((udev_t *)(sdev)->disks)
#endif
#define SDEV_STATUS(sdev) (sdev)->ops->status(sdev)

#define SDEV_PARENT(sdev) ((logunit_t *)(sdev)->private)



struct ops_s;

typedef struct ndas_metadata_s {
#ifdef XPLAT_RAID
    /* only meaningful(non-null) for raid0 raid4 */
    bitmap_t* bitmap; 

    NDAS_RAID_META_DATA* rmd;

    int rmd_usn_match;
#endif    
    /* nmd is allocated at sdev_create, could be freed at some point. */

    NDAS_DIB_V2 *dib;
} ndas_metadata_t;

ndas_metadata_t *nmd_create(void);
void nmd_destroy(ndas_metadata_t * nmd);
#ifdef XPLAT_RAID
ndas_error_t nmd_init_rmd(ndas_metadata_t *nmd);
void nmd_free_bitmap(ndas_metadata_t *nmd);
ndas_error_t nmd_init_bitmap(ndas_metadata_t *nmd);
#endif

#ifdef DEBUG
#define SDEV_MAGIC 0x77fe5531
#endif
#define REGDATA_BIT_DISABLED 00
#define REGDATA_BIT_READ_ENABLED 01
#define REGDATA_BIT_WRITE_ENABLED 02
#define REGDATA_BIT_SHARE_ENABLED 03
#define REGDATA_BIT_MASK 03

    
/**
 * Description
 * logunit_t represents a device refered by the slot number.
 * To do:
 *  In NDAS windows source, this is called logical unit. Need to change naming.
 */
typedef struct _logunit {
#ifdef DEBUG
    int magic;
#endif 
    /* if set, accept io, if not est, don't accept io */
    int accept_io;

    ndas_slot_info_t info; 


    /* ndas_raid_info if raid or ndas_unit_info_t if udev */
    void* info2; 
    
    struct ops_s *ops;

    /* array of pointers to the sdev or 
       the pointer to udev_s 
        to do: this member need to be changed to array of udevs
       */
    struct _logunit **disks;
    
    /* the call back function will be called if the non-enabled slot is enabled */
    ndas_io_done            enabled_func;
    /* the call back function will be called if the enabled slot is disabled */
    ndas_io_done            disabled_func;
    /* argument will be passed to enabled_func, disabled_func */
    void                    *arg;

    ndas_metadata_t           *nmd;

    /* whether user has the NDAS key for this device */
    xbool     has_key;    
    xbool       notify_enable_result_on_disabled;
    /* 
     * lock is locked when sdev is initialized.
     */
    xbool                   lock;
    /* lock_mutex is used to protect enable/disable status, 
       should not be used for ndas_io */
    sal_spinlock               lock_mutex;
#ifdef XPLAT_NDASHIX
    /* */
    ndas_permission_request_handler surrender_request_handler;
#endif
#ifdef XPLAT_RAID
    /* raid private conf or parent sdev */
    void* private; 
#endif    
}logunit_t;

typedef struct _sdev_create_param {
    int mode;
    union {
        struct _sdev_single {
            ndev_t* ndev;
            int unit;
        } single;
        struct _sdev_raid {
            int disk_count;
            int slots[8];   /* Member slots. Need to change to array of udev */
        } raid;
    } u;
} sdev_create_param_t;


typedef ndas_error_t (*create_disks_func)(logunit_t *sdev, sdev_create_param_t* param);
typedef void (*destroy_disks_func)(logunit_t *sdev);
typedef ndas_error_t (*init_func)(logunit_t *sdev);
typedef void (*deinit_func)(logunit_t *sdev);

typedef void (*enable_func) (logunit_t *sdev, int flag);

typedef void (*disable_func)(logunit_t *sdev, ndas_error_t err);
typedef void (*io_func)(logunit_t *sdev, urb_ptr tir, xbool urgent);
typedef ndas_error_t (*query_func)(logunit_t *sdev, ndas_metadata_t *nmd);
typedef int (*status_func)(logunit_t *sdev);
typedef void (*changed_func)(logunit_t *sdev, int slot, int online);

struct ops_s{
    /* 
     * single: create udev 
     * raid: find udev of the slots
     * call by sdev_create
     */
    create_disks_func create_disks;
    /* 
     * single: cleanup udev
     * raid: deallocate 
     */
    destroy_disks_func destroy_disks;
    /* 
     * initialize the data field of sdev, 
     * raid: create raid specific data 
     */
    init_func init;
    /* raid: cleanup raid specific data */
    deinit_func deinit;
    enable_func enable;
    disable_func disable;
#if 0
    query_func query; 
#endif
//    writable_func writable;
    io_func io;
    status_func status;
    changed_func changed;
};


#ifndef DEBUG
static inline void sdev_dont_accept_io(logunit_t *sdev) { sdev->accept_io = 0; }
static inline void sdev_accept_io(logunit_t *sdev) { sdev->accept_io = 1; }
static inline void sdev_set_enabled(logunit_t *sdev) { sdev->info.enabled = 1; }
static inline void sdev_set_disabled(logunit_t *sdev) { sdev->info.enabled = 0; }

#endif

#ifdef DEBUG
#define sdev_dont_accept_io(sdev) \
    do { \
        sdev->accept_io = 0; \
        if(2 <= DEBUG_LEVEL_SDEV_LOCK) { \
            sal_debug_print("SDL|2|%s|",__FUNCTION__); \
            sal_debug_println("sdev_dont_accept_io slot=%d", sdev->info.slot); \
        } \
    } while(0)  

/* 
 * only be called by sdev_notify_enable_result
 */
#define sdev_accept_io(sdev) \
    do { \
        sdev->accept_io = 1; \
        if(2 <= DEBUG_LEVEL_SDEV_LOCK) { \
            sal_debug_print("SD|2|%s|",__FUNCTION__); \
            sal_debug_println("sdev_accept_io slot=%d", sdev->info.slot); \
        } \
    } while(0)  
/* 
 * only be called by sdev_notify_enable_result
 */
#define sdev_set_enabled(sdev) \
    do { \
        sal_assert(sdev->info.enabled == 0); \
        sdev->info.enabled = 1; \
        if(2 <= DEBUG_LEVEL_SDEV_LOCK) { \
            sal_debug_print("SDL|2|%s|",__FUNCTION__); \
            sal_debug_println("sdev_set_enabled slot=%d", sdev->info.slot); \
        } \
    } while(0)  
#define sdev_set_disabled(sdev) \
    do { \
        sal_assert(sdev->info.enabled == 1); \
        sdev->info.enabled = 0; \
        if(2 <= DEBUG_LEVEL_SDEV_LOCK) { \
            sal_debug_print("SDL|2|%s|",__FUNCTION__); \
        } \
    } while(0)  
#endif

/*
 * Create slot dev object with the give unit device object for single disk 
 * ldev: udev or nbdev object for the slot
 */
logunit_t *sdev_create(sdev_create_param_t* opt);

/*
 * Clean up the unlocked sdev
 */
int sdev_cleanup(logunit_t *sdev, void *arg);

/*
 * Clean up the locked sdev
 */
void sdev_do_cleanup(logunit_t *sdev);

int sdev_disable(logunit_t *sdev, void * reserved);

/* 
 * Lock the sdev to guarantee that only one operation over the sdev is performed
 * Muti-thread safe.
 * 
 * Return: True if successfully locked, FALSE if other thread locked sdev.
 */
xbool sdev_lock(logunit_t *sdev);

#ifdef DEBUG
#define sdev_unlock(sdev) \
    do { \
        sal_assert(sdev_is_locked(sdev)); \
        sdev->lock = FALSE; \
        if(2 <= DEBUG_LEVEL_SDEV_LOCK) { \
            sal_debug_print("SDL|2|%s|",__FUNCTION__); \
            sal_debug_println("sdev_unlocked slot=%d", sdev->info.slot); \
        } \
    } while(0)  
#else
static inline void sdev_unlock(logunit_t *sdev) { sdev->lock = FALSE; }
#endif

static inline
xbool sdev_is_locked(logunit_t *sdev)
{
    return sdev->lock;
}
logunit_t * sdev_lookup_byslot(int slot);

inline static 
xbool slot_is_the_raid(ndas_slot_info_t *info) 
{ 
    switch (info->mode) {
    case NDAS_DISK_MODE_RAID0:
    case NDAS_DISK_MODE_RAID1:
    case NDAS_DISK_MODE_RAID4:        
    case NDAS_DISK_MODE_AGGREGATION:
    case NDAS_DISK_MODE_MIRROR:        
        return 1;
    default:
        return 0;
    }
}

void sdev_notify_disable_result(logunit_t *sdev, ndas_error_t reason);
/* For raid sdev */
void sdev_raid_notify_disable_result(logunit_t *sdev, ndas_error_t reason);

void sdev_notify_enable_result(logunit_t *sdev, ndas_error_t reason);


#ifdef XPLAT_RAID
void sdev_queue_write_rmd(logunit_t *sdev);

#endif


inline static 
ndas_unit_info_t* SDEV_UNIT_INFO(logunit_t *sdev)
{
    return (ndas_unit_info_t*) (sdev->info2);
}

#ifdef XPLAT_RAID
extern struct ops_s raidl;
extern struct ops_s raid0;
extern struct ops_s raid1;
extern struct ops_s raid5;
#endif

/* 
 * Try to lock the sdev, reschedule to retry 0.2 sec later if fail to gain lock, 
 */
#ifdef XPLAT_BPC
#define SDEV_REENTER(sdev, this_func, arg1, arg2) \
    if ( !sdev_lock(sdev) ) \
    {   \
        int ret; \
        dpc_id dpcid; \
        dpcid = bpc_create((dpc_func) this_func, arg1, arg2, NULL, 0); \
        if(dpcid) { \
            ret = bpc_queue(dpcid, SAL_TICKS_PER_SEC/5); \
            if(ret < 0) { \
                bpc_destroy(dpcid); \
            } \
        } \
        return 0; \
    }
#else
#ifdef XPLAT_RAID
#error nxpo-bpc should be 'y' to use raid
#endif
#define SDEV_REENTER(sdev, this_func, arg1, arg2) \
    if ( !sdev_lock(sdev) ) \
    { \
        int ret\
        dpc_id dpcid; \
        dpcid = dpc_create(DPC_PRIO_NORMAL, (dpc_func) this_func, arg1, arg2, NULL, 0); \
        if(dpcid)  { \
            ret = dpc_queue(dpcid, SAL_TICKS_PER_SEC/5); \
            if(ret < 0) { \
                dpc_destroy(dpcid); \
            } \
        } \
        return 0; \
    }
#endif
#endif // _NETDISK_SDEV_H_

