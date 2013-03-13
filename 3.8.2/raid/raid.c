#include "xplatcfg.h"
#include <sal/mem.h>
#include <ndasuser/bind.h>
#include <ndasuser/ndaserr.h>
#include <ndasuser/ndasuser.h>
#include "raid/raid.h"
#include "netdisk/sdev.h"
#include "../netdisk/udev.h"
#include "raid0.h"

#ifdef DEBUG
#define    dbg_raid(l, x...)    do {\
    if(l <= DEBUG_LEVEL_RAID) {     \
        sal_debug_print("RD|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)
#else
#define dbg_raid(l, x...) do {} while(0)
#endif

#ifdef XPLAT_RAID

LOCAL
ndas_error_t nb_pr_handler(int slot) {
    // do nothing, we are busy with creating NDAS bind.
    return NDAS_OK;
}
#define MAX_U64 18446744073709551615ULL

void raid_cleanup(logunit_t *sdev)
{
    sal_free(sdev->disks);
    sdev->disks = NULL;
}
/* 
 * called by sdev_create
 */
ndas_error_t raid_create(logunit_t *sdev, sdev_create_param_t* param) 
{
    int i;
    ndas_error_t err = NDAS_ERROR_OUT_OF_MEMORY;
    logunit_t **ret = sal_malloc(sizeof(logunit_t*) * param.u.raid.disk_count);

    dbg_raid(3, "ing sdev=%p, disks=%d", sdev, disks);
    if ( !ret) return NDAS_ERROR_OUT_OF_MEMORY;

    sdev->info2 = sal_malloc(sizeof(ndas_raid_info) + sizeof(int) * param.u.raid.disk_count);
    if ( !sdev->info2 ) {
        goto out1;
    }

    SDEV_RAID_INFO(sdev)->mode = sdev->info.mode;
    SDEV_RAID_INFO(sdev)->disks = param.u.raid.disk_count;

    dbg_raid(5, "SDEV_RAID_INFO(sdev)->disks=%d", SDEV_RAID_INFO(sdev)->disks);
    
    for ( i = 0; i < param.u.raid.disk_count; i++) 
    {
        logunit_t *child = sdev_lookup_byslot(param.u.raid.slots[i]);
        dbg_raid(5, "%d:slot=%d child=%p", i, param.u.raid.slots[i], child);
        if ( !child ) {
            ret[i] = NULL;
            SDEV_RAID_INFO(sdev)->slots[i] = 0;
            continue;
        }

        if ( child->info.mode != sdev->info.mode ) {
            dbg_raid(1, "mode=%d", child->info.mode);
            err = NDAS_ERROR_INVALID_METADATA;
            goto out2;
        }
        
        ret[i] = child;
        SDEV_RAID_INFO(sdev)->slots[i] = param.u.raid.slots[i];
        
        child->private = sdev; 
    }
    sdev->disks = ret;
    
    return NDAS_OK;
out2:
    sal_free(sdev->info2);
out1:
    dbg_raid(1, "ed err=%d", err);
    sal_free(ret);
    return err;
}

#define CONVERT_DISKMODE_2_NMT(mode) ({ \
    int ret = NMT_SINGLE; \
    if ( mode == NDAS_DISK_MODE_RAID0 ) \
        ret = NMT_RAID0; \
    if ( mode == NDAS_DISK_MODE_RAID1 ) \
        ret = NMT_RAID1; \
    if ( mode == NDAS_DISK_MODE_RAID4 ) \
        ret = NMT_RAID4; \
    ret; \
})

ndas_error_t nbd_write_dib(logunit_t *sdev) 
{
    int i;
    ndas_error_t err;
    for (i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        sdev->nmd->dib->u.s.iSequence = i;
        err = udev_write_dib(SDEV2UDEV(sdev->disks[i]), sdev->nmd->dib);
        if ( !NDAS_SUCCESS(err) ) 
            goto out;
    }
    return NDAS_OK;
out:
    sdev->nmd->dib->u.s.Signature[0] = 0; // invalidate
    while(i--) {
        udev_write_dib(SDEV2UDEV(sdev->disks[i]), sdev->nmd->dib);
    }
    return err;
}

/* 
          0 sectors    0 Gbytes -  1073741824 sectors   512 Gbytes :  128
 1073741825 sectors  512 Gbytes -  2147483648 sectors  1024 Gbytes :  256
 2147483649 sectors 1024 Gbytes -  4294967296 sectors  2048 Gbytes :  512
 4294967297 sectors 2048 Gbytes -  8589934592 sectors  4096 Gbytes : 1024
 8589934593 sectors 4096 Gbytes - 17179869184 sectors  8192 Gbytes : 2048
17179869185 sectors 8192 Gbytes - 34359738368 sectors 16384 Gbytes : 4096
*/
#define MAX_SECTORS_PER_BIT 6

#define SECTORS_PER_BIT(sectors) ({ \
    int ret = 1 << 7; \
    int i = 0; \
    while(i < MAX_SECTORS_PER_BIT) \
    { \
        if(sectors <= 1024ULL * 1024ULL * 1024ULL * ( 1ULL << i )) \
        { \
            ret = 1 << (i + 7); \
            break; \
        } \
        i++; \
    } \
    ret; \
})

ndas_error_t dib2_init(NDAS_DIB_V2 *dib, logunit_t *sdev) 
{
    int i;
    sal_memcpy(dib->u.s.Signature, NDAS_DIB_V2_SIGNATURE, 8);
    dib->u.s.MajorVersion = 1;
    dib->u.s.MinorVersion = 2;
    dib->u.s.sizeXArea = 2 * 1024 * 2;
    dib->u.s.sizeUserSpace = sdev->info.sectors;
    dib->u.s.iSectorsPerBit = SECTORS_PER_BIT(sdev->info.sectors_per_disk);  
    dib->u.s.nDiskCount = SDEV_RAID_INFO(sdev)->disks;
    dib->u.s.iMediaType = CONVERT_DISKMODE_2_NMT(sdev->info.mode);
    //dib->u.s.iSequence;

    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++) 
    {
        sal_memcpy(
            dib->UnitDisks[i].MACAddr, 
            SDEV2UDEV(sdev->disks[i])->ndev->network_id,
            SAL_ETHER_ADDR_LEN
        );
        dib->UnitDisks[i].UnitNumber = sdev->disks[i]->info.unit;
    }
    return NDAS_OK;
}

xbool dib_compare(NDAS_DIB_V2 *dib1, NDAS_DIB_V2 *dib2) 
{
    int first_size = sizeof(dib1->u.s.Signature)
                    + sizeof(dib1->u.s.MajorVersion)
                    + sizeof(dib1->u.s.MinorVersion)
                    + sizeof(dib1->u.s.sizeXArea)
                    + sizeof(dib1->u.s.sizeUserSpace)
                    + sizeof(dib1->u.s.iSectorsPerBit)
                    + sizeof(dib1->u.s.iMediaType)
                    + sizeof(dib1->u.s.nDiskCount);
    int second_size = sizeof(UNIT_DISK_LOCATION) * dib1->u.s.nDiskCount;
    if ( dib1 == NULL ) return FALSE;
    if ( dib2 == NULL ) return FALSE;
    if ( dib1 == dib2 )
        return TRUE;
    if ( sal_memcmp(dib1->u.s.Signature, dib1->u.s.Signature, first_size) != 0 )
        return FALSE;
    return sal_memcmp(dib1->UnitDisks, dib2->UnitDisks, second_size) == 0 ? TRUE : FALSE;
}
xbool sdev_rmd_is_newer(logunit_t *sdev, NDAS_RAID_META_DATA *rmd2)
{
    sal_assert(sdev->nmd);
    sal_assert(sdev->nmd->rmd);
    
    if ( rmd2->u.s.magic != RMD_MAGIC ) 
        return FALSE;
    
    if ( sdev->nmd->rmd->u.s.magic != RMD_MAGIC )
        return TRUE;
    
    if ( sdev->nmd->rmd->u.s.usn < rmd2->u.s.usn ) 
    {
        return TRUE;
    }
    return FALSE;
}
LOCAL
NDAS_CALL void bind_enable_done(int slot, ndas_error_t err, void *arg)
{
    
}
NDASUSER_API        
ndas_error_t ndas_bind(int type, int nr_disks, int *slots) 
{
    ndas_error_t err;
    logunit_t *sdev;
    sdev_create_param_t sparam = {0};
    sparam.mode = type;
    sparam.u.raid.disk_count = nr_disks;
    sal_memcpy(sparam.u.raid.slots, slots, sizeof(int)*nr_disks);
   
    sdev = sdev_create(&sparam);

    if ( !sdev )
        goto out1;

    sdev->enabled_func = bind_enable_done;
    sdev->disabled_func = bind_enable_done;
    sdev->arg = sdev;
        // jbod_op_enable, r1op_connect, rddc_op_enable
    sdev->ops->enable(sdev, ENABLE_FLAG(nb_pr_handler != NULL, FALSE, TRUE, FALSE)); 

    dib2_init(sdev->nmd->dib, sdev);

    err = nbd_write_dib(sdev);
    if ( !NDAS_SUCCESS(err) )
        goto out2;

     sdev->ops->disable(sdev, NDAS_ERROR_SHUTDOWN_IN_PROGRESS);
    // TODO: notify it through hix
    
    return NDAS_OK;
out2:
    sdev_cleanup(sdev, NULL);
out1:
    return NDAS_ERROR_OUT_OF_MEMORY;
}

void raid_struct_free(struct raid_struct* rs)
{
    sal_spinlock_destroy(rs->op_mutex);
    sal_free(rs); 
}

struct raid_struct* raid_struct_alloc()
{
    struct raid_struct* rs = (struct raid_struct*) sal_malloc(sizeof(struct raid_struct));
    int ret;
    if ( !rs ) {
        return NULL;
    }
    sal_memset(rs, 0, sizeof(struct raid_struct));
    sal_atomic_set(&rs->op_count, 0);
    ret = sal_spinlock_create("rs", &rs->op_mutex);
    dbg_raid(1,"op_mutex=%p",rs->op_mutex); 
    if (!ret) 
    {
        sal_free(rs);
        return NULL;
    }
        
    return rs;
}

void raid_disabled_on_failing_to_enable(int slot, ndas_error_t err, void *arg)
{
    logunit_t *sdev = (logunit_t *) arg;
    int index = raid_find_index(sdev, slot);
    dbg_raid(1, "slot=%d err=%d", slot,err);
    
    sal_assert(sdev_is_locked(sdev));

    sdev_set_disabled(sdev->disks[index]);
    sdev->disks[index]->disabled_func = NULL;
    
    if ( sal_atomic_dec_and_test(&SDEV_RAID(sdev)->op_count) == 0 )
        return;       
    sal_assert(!NDAS_SUCCESS(SDEV_RAID(sdev)->op_err));
    sdev_notify_enable_result(sdev, SDEV_RAID(sdev)->op_err);
}

/*
 * Called when it fails to enable the part of disks and the raid is not operatable.
 * So 1. set the sdev->disabled_func of the already enabled disk
 *    2. call the uop_disable of which disks are successfully enabled.
 * Note that we still hold the sdev-lock
 * Called by jb_all_enabled, rd_all_enabled, 
 */
void raid_stop_disks(logunit_t *raid, ndas_error_t err, xbool notify_enable, ndas_io_done func)
{
    int i, count = 0;
    dbg_raid(1, "ing slot=%d err=%d", raid->info.slot, err);

    sal_assert(sdev_is_locked(raid));
    
    for (i = 0; i < SDEV_RAID_INFO(raid)->disks; i++) 
    {
        if ( !raid->disks[i] || !raid->disks[i]->info.enabled) {
            dbg_raid(1,"raid->disks[%d]=%p",i,raid->disks[i]); 
            if ( raid->disks[i] ) 
                dbg_raid(1,"[%d]info.enabled=%d",i, raid->disks[i]->info.enabled); 
            continue;
        }
        dbg_raid(1,"[%d slot=%d]info.enabled=%d",i,raid->disks[i]->info.slot, raid->disks[i]->info.enabled); 
        count++;
    }
    if ( count == 0 ) {
        dbg_raid(1,"all disks are already disabled");
        if ( notify_enable ) {
            sdev_notify_enable_result(raid, err);
        } else {
            sdev_notify_disable_result(raid, err);
        }
        return;
    }
    sal_atomic_set(&SDEV_RAID(raid)->op_count, count);
    for (i = 0; i < SDEV_RAID_INFO(raid)->disks; i++) 
    {
        if ( !raid->disks[i] || !raid->disks[i]->info.enabled) {
            dbg_raid(1,"raid->disks[%d]=%p",i,raid->disks[i]); 
            if ( raid->disks[i] ) 
                dbg_raid(1,"[%d]info.enabled=%d",i, raid->disks[i]->info.enabled); 
            continue;
        }
        sal_assert(raid->disks[i]->disabled_func != NULL);
        raid->disks[i]->disabled_func = func;
        raid->disks[i]->ops->disable(raid->disks[i], err); // uop_disable
    }
}
#endif // XPLAT_RAID

NDASUSER_API
ndas_error_t 
ndas_query_raid(int bind_slot, ndas_raid_ptr info, int size)
{
#ifdef XPLAT_RAID
    logunit_t *sdev = sdev_lookup_byslot(bind_slot);
    int sz;

    if ( !sdev )
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    
    sz = sizeof(ndas_raid_info) 
        + SDEV_RAID_INFO(sdev)->disks 
        * sizeof(SDEV_RAID_INFO(sdev)->slots[0]);
    
    if ( size < sz ) {
        sal_memcpy(info, SDEV_RAID_INFO(sdev), size);    
        return SDEV_RAID_INFO(sdev)->disks;
    }
    
    sal_memcpy(info, SDEV_RAID_INFO(sdev), size);
    return NDAS_OK;
#else
    return NDAS_ERROR_UNSUPPORTED_DISK_MODE;
#endif
}

