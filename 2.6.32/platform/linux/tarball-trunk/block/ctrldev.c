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
#include <asm/uaccess.h> // VERIFY_WRITE

#include <sal/debug.h>
#include <sal/types.h>
#include <sal/sync.h>
#include <sal/libc.h>
#include <ndasuser/ndasuser.h>
#include <ndasuser/write.h>

#include "block.h"
#include "ops.h"
#include "ndasdev.h"
#include "procfs.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20))
#include "linux/compiler.h"
#endif

#ifdef NDAS_MSHARE 
#include <ndasuser/mediaop.h>
#endif

#ifdef NDAS_DEVFS
#ifndef CONFIG_DEVFS_FS
#error This kernel config does not support devfs.
#else
#include <linux/devfs_fs_kernel.h>
static devfs_handle_t devfs_control_handle = NULL;
#endif
#endif

#ifdef DEBUG
#undef LOCAL
#define LOCAL
#else
#undef LOCAL
#define LOCAL static
#endif

extern int ndas_io_unit;

#ifndef __user
#define __user
#endif

#if LINUX_VERSION_25_ABOVE

#else
static inline unsigned iminor(const struct inode *inode)
{
    return MINOR(inode->i_rdev);
}
#endif

static ssize_t
ndas_ctrldev_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offp)
{
        printk("ndas: can't write to NDAS control device.\n");
        return -EINVAL;
}

static int
ndas_ctrldev_open(struct inode *inode, struct file *filp)
{
    if (iminor(inode) == NDAS_CHR_DEV_MINOR) {
        return 0;
    } else {
        return -EINVAL;
    }
}

static int
ndas_ctrldev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t
ndas_ctrldev_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
        printk("ndas: can't read NDAS control device.\n");
        return -EINVAL;
}

#ifndef NDAS_QUEUE_SCHEDULER
/* add NDAS_EXTRA_CFALGS 
 * -DNDAS_SCHEDULER=\"noop\" 
 * or one of "noop" "anticipatory" "deadline" "cfq" 
 */
#define NDAS_QUEUE_SCHEDULER "noop"
#endif

NDAS_CALL void ndcmd_dev_changed(const char* serial, const char *name, NDAS_DEV_STATUS online);

#if LINUX_VERSION_25_ABOVE
extern void nblk_request_proc(struct request_queue *q);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
extern void nblk_prepare_flush(request_queue_t *q, struct request *rq);
#endif
#endif

LOCAL int slot_create(int s) 
{
    struct ndas_slot *slot;
    dbgl_blk(1, "ing s=%d",s);
    
    slot = sal_malloc(sizeof(struct ndas_slot));

    if ( slot == NULL ) 
        return -1;

    memset(slot, 0, sizeof(struct ndas_slot));
    slot->slot = s;
    sema_init(&slot->mutex, 1);
    slot->enabled = 0;
    slot->devname[0] = 0;

    NDAS_SET_SLOT_DEV(s, slot);
    
    dbgl_blk(3, "ed s=%d OK",s);
    nproc_add_slot(slot);

    return 0;
}

/*
    Create system device file for the enabled slot.
*/
ndas_error_t slot_enable(int s)
{
    ndas_error_t ret = NDAS_ERROR_INTERNAL;
    int got;
    struct ndas_slot* slot = NDAS_GET_SLOT_DEV(s); 
    dbgl_blk(3, "ing s#=%d slot=%p",s, slot);
    got = try_module_get(THIS_MODULE);
    MOD_INC_USE_COUNT;
    
    if ( slot == NULL)
        goto out1;
    
    if ( slot->enabled ) {
        dbgl_blk(1, "already enabled");
        ret = NDAS_OK;
        goto out2;
    }
    ret = ndas_query_slot(s, &slot->info);
    if ( !NDAS_SUCCESS(ret) ) {
        dbgl_blk(1, "fail ndas_query_slot");
        goto out2;
    }
    dbgl_blk(1, "mode=%d", slot->info.mode);
    
    slot->enabled = 1;
    
#if LINUX_VERSION_25_ABOVE

    slot->disk = NULL;
    spin_lock_init(&slot->lock);
    slot->queue = blk_init_queue(
        nblk_request_proc, 
        &slot->lock
    );
    
    blk_queue_max_phys_segments(slot->queue, ND_BLK_MAX_REQ_SEGMENT);
    blk_queue_max_hw_segments(slot->queue, ND_BLK_MAX_REQ_SEGMENT);
    blk_queue_hardsect_size(slot->queue, slot->info.sector_size);
    blk_queue_max_sectors(slot->queue, DEFAULT_ND_MAX_SECTORS);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
    // Set ordered queue property.
#if 0
    blk_queue_ordered(slot->queue, QUEUE_ORDERED_TAG_FLUSH, nblk_prepare_flush);
#endif
#endif

    slot->disk = alloc_disk(NR_PARTITION);
    if ( slot->disk == NULL ) {
        slot->enabled = 0;
        dbgl_blk(1, "fail alloc disk");
        goto out2;
    }

    slot->disk->major = NDAS_BLK_MAJOR;
    slot->disk->first_minor = (s - NDAS_FIRST_SLOT_NR) << PARTN_BITS;
    slot->disk->fops = &ndas_fops;
    slot->disk->queue = slot->queue;
    slot->disk->private_data = (void*) (long)s;
    slot->queue_flags = 0;

    dbgl_blk(1, "mode=%d", slot->info.mode);
    if ( slot->info.mode == NDAS_DISK_MODE_SINGLE || 
        slot->info.mode == NDAS_DISK_MODE_ATAPI ||
        slot->info.mode == NDAS_DISK_MODE_MEDIAJUKE) 
    {
        char short_serial[NDAS_SERIAL_SHORT_LENGTH + 1];
        if (strlen(slot->info.ndas_serial) > 8) {
            /* Extended serial number is too long as sysfs object name. Use last 8 digit only */
            strncpy(
                short_serial,
                slot->info.ndas_serial + ( NDAS_SERIAL_EXTEND_LENGTH - NDAS_SERIAL_SHORT_LENGTH),
                8);
        } else {
            strncpy(short_serial, slot->info.ndas_serial, 8);
        }
        short_serial[8] =0;
        snprintf(slot->devname,
            sizeof(slot->devname)-1, 
            "ndas-%s-%d", short_serial, slot->info.unit
        );
        strcpy(slot->disk->disk_name, slot->devname);

#if !LINUX_VERSION_DEVFS_REMOVED_COMPLETELY
        strcpy(slot->disk->devfs_name, slot->devname);
#endif
        set_capacity(slot->disk, slot->info.sectors);
    } else {
        /* Other mode is not implemented */

    }
    
    if (slot->info.mode == NDAS_DISK_MODE_ATAPI) {
        slot->disk->flags = GENHD_FL_CD | GENHD_FL_REMOVABLE;
#if 0
        kref_init(&slot->ndascd.kref);
#endif
    }

    dbgl_blk(4, "adding disk: slot=%d, first_minor=%d, capacity=%llu", s, slot->disk->first_minor, slot->info.sectors);
    add_disk(slot->disk);
#ifndef NDAS_DONT_CARE_SCHEDULER
#if LINUX_VERSION_AVOID_CFQ_SCHEDULER
#if CONFIG_SYSFS
    sal_assert(slot->queue->kobj.ktype);
    sal_assert(slot->queue->kobj.ktype->default_attrs);
    {
        struct queue_sysfs_entry {
        	struct attribute attr;
        	ssize_t (*show)(struct request_queue *, char *);
        	ssize_t (*store)(struct request_queue *, const char *, size_t);
        };
        struct attribute *attr = slot->queue->kobj.ktype->default_attrs[4];
        struct queue_sysfs_entry *entry = container_of(attr , struct queue_sysfs_entry, attr);
        entry->store(slot->queue,NDAS_QUEUE_SCHEDULER,strlen(NDAS_QUEUE_SCHEDULER));
        
    }
#else
#error "NDAS driver doesn't work well with CFQ scheduler of 2.6.13 or above kernel." \
       "if you forcely want to use it, please specify compiler flags by " \
       "export NDAS_EXTRA_CFLAGS=\"-DNDAS_DONT_CARE_SCHEDULER\" "\
       "then compile the source again."
#endif
#endif
#endif        
    printk("ndas: /dev/%s enabled\n" , 
            slot->devname);
#else 
    /* < LINUX_VERSION_25_ABOVE */
    dbgl_blk(4, "blksize=%d", DEFAULT_ND_BLKSIZE);
    dbgl_blk(4, "size=%lld", slot->info.sectors);
    dbgl_blk(1, "hardsectsize=%d", slot->info.sector_size);
    ndas_ops_set_blk_size(
        s, 
        DEFAULT_ND_BLKSIZE, 
        slot->info.sectors,
        slot->info.sector_size, 
        DEFAULT_ND_MAX_SECTORS
    );
#ifdef NDAS_DEVFS    
    printk("ndas: /dev/nd/disc%d enabled\n" , 
            s - NDAS_FIRST_SLOT_NR);
#else
    printk("ndas: /dev/nd%c enabled\n" , 
            s + 'a' - NDAS_FIRST_SLOT_NR);
#endif

#endif
    
    //up(&slot->mutex);
 #ifdef NDAS_MSHARE 
    if(NDAS_GET_SLOT_DEV(s)->info.mode == NDAS_DISK_MODE_MEDIAJUKE)
    {
  	    ndas_CheckFormat(s);
    }
 #endif
#if !LINUX_VERSION_25_ABOVE
    ndas_ops_read_partition(s);
#endif
    dbgl_blk(3, "ed");
    return NDAS_OK;
out2:    
    //up(&slot->mutex);
out1:    
    if ( got ) module_put(THIS_MODULE);
    MOD_DEC_USE_COUNT;
    return ret;
}

LOCAL void slot_disable(int slot)
{
    struct ndas_slot* sd = NDAS_GET_SLOT_DEV(slot); 
    dbgl_blk(2, "ing slot=%d",slot);

    if ( sd == NULL ) {
        dbgl_blk(0, "ndas: fix me at slot_disable!!");
        goto out;
    }
    down(&sd->mutex);
    if ( !sd->enabled ) {
        dbgl_blk(1, "ed slot=%d is already disabled",slot);    
        up(&sd->mutex);
        goto out;
    }
#if LINUX_VERSION_25_ABOVE
    del_gendisk(sd->disk);
    put_disk(sd->disk);
    sd->disk = NULL;
    blk_cleanup_queue(sd->queue);
    sd->queue = NULL;   
#else    

    ndas_ops_set_blk_size(slot, 0, 0, 0, 0);
    ndas_ops_invalidate_slot(slot);
#endif   

#ifdef NDAS_DEVFS
//	devfs_unregister(sd->devfs_handle);
#endif

    sd->enabled = 0;
    up(&sd->mutex);
    module_put(THIS_MODULE);
    MOD_DEC_USE_COUNT;
    if ( sd ) {
#if LINUX_VERSION_25_ABOVE
        printk("ndas: /dev/%s is disabled\n", sd->devname);
#else
#ifdef NDAS_DEVFS
       printk("ndas: /dev/nd/disc%d is disabled\n", slot - NDAS_FIRST_SLOT_NR);
#else
	   printk("ndas: /dev/nd%c is disabled\n", slot + 'a' - NDAS_FIRST_SLOT_NR);
#endif	   
#endif
    }
    else
        printk("ndas: slot %d is disabled\n", slot);
out:    
    dbgl_blk(3, "ed");
}


LOCAL void slot_destroy(int slot)
{
    struct ndas_slot* sd = NDAS_GET_SLOT_DEV(slot); 
    if (sd == NULL) {
    	printk("ndas: slot object for %d doesn't exist: bug caused in race condition\n", slot);
    	return;
    }
    nproc_remove_slot(sd);

    while ( sd->enabled ) {
        up(&sd->mutex);
        slot_disable(slot);
        down(&sd->mutex);    
    }
    sal_free(sd);
    NDAS_SET_SLOT_DEV(slot, NULL);
}


LOCAL ndas_error_t ndcmd_start(void) 
{
    ndas_error_t ret;
    
    ndas_set_device_change_handler(ndcmd_dev_changed);

    ret = ndas_start();
    if( NDAS_SUCCESS(ret) ) {
        return NDAS_OK;
    }
    ndas_set_device_change_handler(NULL);
    return ret;
}

LOCAL ndas_error_t ndcmd_stop(void) 
{
    int i,size, slot;
    ndas_error_t ret;
    ndas_device_info info;
    struct ndas_registered *data;
    
    size = ndas_registered_size();
    if ( !NDAS_SUCCESS(size) ) {
        dbgl_blk(1,"fail to get ndas_registered_size");    
        goto out;
    }
    dbgl_blk(1,"ndas_registered_size=%d", size);
    data = sal_malloc(sizeof(struct ndas_registered)* size);
    if ( !data ) {
        goto out;
    }

    for ( i = NDAS_FIRST_SLOT_NR; i < NDAS_MAX_SLOT_NR; i++) 
    {
        struct ndas_slot* sd = NDAS_GET_SLOT_DEV(i); 

        if ( !sd ) {
            continue;
        }
        slot = sd->slot;
        slot_destroy(slot);
        ndas_disable_slot(slot);
    }
    ret = ndas_registered_list(data, size);
    if ( !NDAS_SUCCESS(ret) ) {
        goto out_free;
    }
    
    if ( ret != size ) {
        goto out_free;
    }

    for ( i = 0; i < size; i ++ ) 
    {
        dbgl_blk(1,"name[%d]=%s", i , data[i].name);
        ret = ndas_get_ndas_dev_info(data[i].name, &info);
        if ( !NDAS_SUCCESS(ret) )
            continue;
        nproc_remove_ndev(data[i].name);
        ndas_unregister_device(data[i].name);
    }
out_free:
    sal_free(data);
out:
    return ndas_stop();
}

LOCAL 
ndas_error_t 
ndcmd_register(char *name, char *id, char *ndas_key) 
{
    ndas_error_t err;
    dbgl_blk(3,"name=%s", name);

    err = ndas_register_device(name, id, ndas_key);
    
    if ( !NDAS_SUCCESS(err) ) 
        return err;
        
    nproc_add_ndev(name);
    return NDAS_OK;
}

LOCAL 
ndas_error_t 
ndcmd_register_by_serial(char *name, char *serial) 
{
    ndas_error_t err;
    dbgl_blk(3,"name=%s", name);

    err = ndas_register_device_by_serial(name, serial);
    
    if ( !NDAS_SUCCESS(err) ) 
        return err;
        
    nproc_add_ndev(name);
    return NDAS_OK;
}

LOCAL
ndas_error_t ndcmd_unregister(char *name) 
{
    int i;
    ndas_error_t err;
    ndas_device_info info;
    err = ndas_get_ndas_dev_info(name, &info); 

    if ( !NDAS_SUCCESS(err)) {
        return err;
    }
    err = ndas_unregister_device(name);
    if ( !NDAS_SUCCESS(err)) {
        return err;
    }
    
    for ( i = 0; i < info.nr_unit; i++) {
        slot_destroy(info.slot[i]);
    }
    nproc_remove_ndev(name);

    return err;
}
LOCAL NDAS_CALL ndas_error_t notify_permission_requested(int slot) 
{
    char buf[1024];
    snprintf(buf,sizeof(buf),
        "ndas: %s wants you to surrender the exclusive write access for slot %d\n", 
        "somebody",
        slot
    );
    printk(buf);
    return NDAS_OK;
}

NDAS_CALL LOCAL void ndcmd_enabled_handler(int slot, ndas_error_t err, void* arg)
{
    struct ndas_slot *sd = (struct ndas_slot *)arg;
    dbgl_blk(1, "slot=%d err=%d", slot, err);

    if ( !NDAS_SUCCESS(err) ) {
        sd->enable_result = err;
        up(&sd->mutex);
        return;
    }
    
    err = slot_enable(slot);
    if ( !NDAS_SUCCESS(err) ) 
    {
        dbgl_blk(1, "err=%d", err);
        ndas_disable_slot(slot);
        sd->enable_result = err;
    } else {
        sd->enable_result = NDAS_OK;
    }
    up(&sd->mutex);
}
/*
 * Disconnected handler.
 * This function will be called when the driver is disconnected.
 */
NDAS_CALL LOCAL void ndcmd_disabled_handler(int s, ndas_error_t err, void* arg)
{
    slot_disable((int)s);
}
/* 
 * Called when the NDAS device becomes online or offline
 */
NDAS_CALL void ndcmd_dev_changed(const char* serial, const char *name, NDAS_DEV_STATUS online)
{
    int i;
    ndas_error_t err;
    ndas_device_info dinfo;
    struct ndas_slot* sd;
    dbgl_blk(1,"ing serial=%s name=%s online=%d", serial, name, online);
    if ( !name ) {
        /* Ignore unregistered device */
        /* Add code here if you want to use all device on the network */
        return;
    }
    
    if (online != NDAS_DEV_STATUS_HEALTHY) {
        /* Device is offline. */        
        /* Don't disable device yet. ndascore may be able to connect to the device again */
        /* Disable when IO failed */
        dbgl_blk(1,"Device %s is not healthy", name);        
        return;
    }

    err = ndas_get_ndas_dev_info(name, &dinfo);
    if ( !NDAS_SUCCESS(err) ) {
        dbgl_blk(1, "err=%d", err);
        return;
    }
    dbgl_blk(1,"name=%s nr_unit=%d status=%d, slot=%d, %d", 
        dinfo.name, dinfo.nr_unit, dinfo.status, dinfo.slot[0], dinfo.slot[1]);

    /* 
     * 1. register connect/disconnect handler for the NDAS device
     * 2. Check if the slot composes a raid slot, then create the slot for raid
     */
    for ( i = 0 ; i < dinfo.nr_unit; i++) 
    {
        ndas_slot_info_t sinfo;
        ndas_unit_info_t uinfo;
        dbgl_blk(1, "slot[%d]=%d", i, dinfo.slot[i]);
        if ( NDAS_GET_SLOT_DEV(dinfo.slot[i]) == NULL ) {
            if ( unlikely(slot_create(dinfo.slot[i]) != 0) ) 
            {
                printk("ndas: out of memery to assign the slot %d\n", dinfo.slot[i]);
                continue;
            }
        }
        sd = NDAS_GET_SLOT_DEV(dinfo.slot[i]);
           
        /* to do:
            Imlement without ndcmd_enabled_handler, ndcmd_disabled_handler 
        */
    
        ndas_set_slot_handlers(dinfo.slot[i], ndcmd_enabled_handler, ndcmd_disabled_handler, notify_permission_requested, sd);
        
        err = ndas_query_slot(dinfo.slot[i], &sinfo);
        if ( !NDAS_SUCCESS(err) )
            continue;
        
        if ( sinfo.mode == NDAS_DISK_MODE_SINGLE )
            continue;
        if ( sinfo.mode == NDAS_DISK_MODE_ATAPI)
            continue;
        if ( sinfo.mode == NDAS_DISK_MODE_MEDIAJUKE )
            continue;
        if ( sinfo.mode == NDAS_DISK_MODE_UNKNOWN )
            continue;
        
        /* for XIMETA Raid */
        err = ndas_query_unit(dinfo.slot[i], &uinfo);
        if ( !NDAS_SUCCESS(err) ) continue;

        if ( uinfo.raid_slot == NDAS_INVALID_SLOT )
            continue;
        if ( NDAS_GET_SLOT_DEV(uinfo.raid_slot) == NULL ) 
        {
            slot_create(uinfo.raid_slot);
            sd = NDAS_GET_SLOT_DEV(uinfo.raid_slot);
            printk("ndas: slot %d discovered\n", uinfo.raid_slot);
        }
    }
}

LOCAL 
ndas_error_t 
ndcmd_enable(int slot, int write_mode)
{
    ndas_error_t err = NDAS_OK;
    struct ndas_slot *sd = NDAS_GET_SLOT_DEV(slot);
    
    dbgl_blk(1, "ing slot=%d %s",slot, write_mode ? "read-write": "read-only");
    /* Just disable the encryption for mips devices: temporal */
#if defined(_MIPSEL) || defined(_MIPS) 
        err = ndas_set_encryption_mode(slot, 0, 0);
        if (!NDAS_SUCCESS(err)) {
            dbgl_blk(1, "failed to set encryption mode");
        }
#endif
    if ( sd == NULL ) {
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }    
    
    down(&sd->mutex);
    
    switch(write_mode) {
    case 2:
        printk("ndas: Enabling slot %d in write-share mode\n", slot);
        err = ndas_enable_writeshare(slot);
        break;
    case 1:
        err = ndas_enable_exclusive_writable(slot);
        break;
    case 0:
        err = ndas_enable_slot(slot);
        break;
    default:
        dbgl_blk(1, "Unknown write_mode: %d, enabling with read-only", write_mode);
        err = ndas_enable_slot(slot);
        break;
    }
    if ( !NDAS_SUCCESS(err) ) {
        dbgl_blk(1, "err=%d", err);
        up(&sd->mutex);
        return err;
    }
    
    down(&sd->mutex);
    err = sd->enable_result;
    up(&sd->mutex);
    
    if ( !NDAS_SUCCESS(err) ) {
        dbgl_blk(1, "err=%d", err);
        return err;
    }
    dbgl_blk(1, "ed");
    return NDAS_OK;    
}

LOCAL ndas_error_t ndcmd_disable(int slot)
{
    ndas_error_t err;
    err = ndas_disable_slot(slot);
    // To do: Remove use of ndas_set_slot_handlers. 
    // These handlers are not changed. Don't need to clear these handler
//    ndas_set_slot_handlers(slot, NULL, NULL, NULL, NULL);
    return err;
}

LOCAL ndas_error_t ndcmd_request_permission(int slot) 
{
    return ndas_request_permission(slot);
}

extern int juke_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

LOCAL inline ndas_error_t ndcmd_probe(char *user_array, int sz, int *count, int convert_to_id)
{
    ndas_error_t ret = NDAS_OK;
    struct ndas_probed *data;
    char id[NDAS_ID_LENGTH+1];
    char key[NDAS_KEY_LENGTH+1];
    int retry = 0;
    
    dbgl_blk(3,"ing sz=%d",sz);
    do {
        int i, c;
        c = ndas_probed_size();
        if ( !NDAS_SUCCESS(c) ) {
            return c;
        }
        if ( user_array == NULL ) {/* just return the count */
            (*count) = c;
            return NDAS_OK;
        }
        if ( sz != c )
            return NDAS_ERROR_INVALID_PARAMETER;
        
        dbgl_blk(3,"c=%d", c);
        
        data = sal_malloc(sizeof(struct ndas_probed) * c);
        if ( !data ) {
            ret = NDAS_ERROR_OUT_OF_MEMORY;
            break;
        }
        // ndas_probed_list() returns the count.
        ret = ndas_probed_list(data, c);
        if ( ret == NDAS_ERROR_INVALID_PARAMETER ) {
            if ( retry++ > 2 ) {
                break;
            }
            sal_free(data);
            continue;
        }
        if ( ret < 0 ) {
            break;
        }
        // Reset the return value.
        ret = NDAS_OK;
        if (convert_to_id) {
	        if ( !access_ok(VERIFY_WRITE, user_array, (NDAS_ID_LENGTH+1) * c) ) {
	            ret = NDAS_ERROR_INTERNAL;
	            break;
	        }
        } else { 
	        if ( !access_ok(VERIFY_WRITE, user_array, (NDAS_SERIAL_LENGTH+1) * c) ) {
	            ret = NDAS_ERROR_INTERNAL;
	            break;
	        }
	    }
        for ( i = 0; i < c; i++ ) {
            int result;
            dbgl_blk(5, "serial=%s", data[i].serial);
            if (convert_to_id) {
                ret = ndas_query_ndas_id_by_serial(data[i].serial, id, key);
                if (ret == NDAS_OK) {
                    result = copy_to_user(user_array + (NDAS_ID_LENGTH+1)*i, id, NDAS_ID_LENGTH+1);
                    if ( result != 0 ) {
                        ret = NDAS_ERROR_INTERNAL; 
                        (*count) = 0;
                        break;
                    }
                } else {
                    // ndas_query_ndas_id_by_serial is not supported
                    (*count) = 0;
                    break;
                }
            } else {
                result = copy_to_user(user_array + (NDAS_SERIAL_LENGTH+1)*i, data[i].serial, NDAS_SERIAL_LENGTH+1);
                if ( result != 0 ) { 
                    ret = NDAS_ERROR_INTERNAL; 
                    (*count) = 0;
                    break;
                }
            }
        }
        if (NDAS_SUCCESS(ret)) {
            (*count)=c;
        }
        break;
    } while(1);
    sal_free(data);
    dbgl_blk(3,"ed");
    return ret;
}

int ndas_ctrldev_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    unsigned int        result = 0;

    dbgl_blk(3,"ing inode=%p, filp=%p, cmd=0x%x, arg=%p", inode, filp, cmd, (void*) arg);

    if(!capable(CAP_SYS_ADMIN))
        return -EPERM;

    if(!inode) {
        printk("ndas: open called with inode = NULL\n");
        return -EIO;
    }
    if (iminor(inode) != NDAS_CHR_DEV_MINOR)  {
        return -EINVAL;
    }
                
    switch(cmd) {
    case IOCTL_NDCMD_START:
    {
        result = ndcmd_start();
    }
    break;
    case IOCTL_NDCMD_STOP:
    {
        result = ndcmd_stop();
    }
    break;
    case IOCTL_NDCMD_REGISTER: 
    {
        ndas_error_t err;
        ndas_ioctl_register_ptr rarg = (ndas_ioctl_register_ptr) arg;
        
        char name[NDAS_MAX_NAME_LENGTH];
        char idserial[NDAS_ID_LENGTH+1];
        char key[NDAS_KEY_LENGTH+1];
        char use_serial;
        char discover;
        dbgl_blk(3,"IOCTL_NDCMD_REGISTER)");

        result = copy_from_user(name,rarg->name,NDAS_MAX_NAME_LENGTH);
        if ( result != 0) { return -EINVAL; }
        result = copy_from_user(idserial, rarg->ndas_idserial, NDAS_ID_LENGTH + 1);
        if ( result != 0) { return -EINVAL; }
        result = copy_from_user(key, rarg->ndas_key, NDAS_KEY_LENGTH + 1);
        if ( result != 0) { return -EINVAL; }
        result = copy_from_user(&use_serial, &rarg->use_serial, sizeof(rarg->use_serial));
        if ( result != 0) { return -EINVAL; }
        result = copy_from_user(&discover, &rarg->discover, sizeof(rarg->discover));
        if ( result != 0) { return -EINVAL; }
        
        if (use_serial) {
            dbgl_blk(1,"Registering by serial:%s", idserial);
            err = ndcmd_register_by_serial(name, idserial);
        } else {
            dbgl_blk(1,"Registering by id:%s, key: %s", idserial, key);
            err = ndcmd_register(name,idserial,key);
        }
        dbgl_blk(1,"idserial=%s, err=%d", idserial, err);
        put_user(err, &rarg->error);
        if ( !NDAS_SUCCESS(err) ) {
            result = -EINVAL;
            break;
        } else {
            if (discover) {
                ndas_detect_device(name);
            }
        }
        result = 0;
    }
    break;
    case IOCTL_NDCMD_UNREGISTER: // unregister lanscsi controller
    {
        char name[NDAS_MAX_NAME_LENGTH];
        ndas_error_t err;
        ndas_ioctl_unregister_ptr rarg = (ndas_ioctl_unregister_ptr) arg;
        dbgl_blk(3,"IOCTL_NDCMD_UNREGISTER");

        result = copy_from_user(name,rarg->name,NDAS_MAX_NAME_LENGTH);
        if ( result != 0 ) { return -EINVAL; }
        err = ndcmd_unregister(name);
        put_user(err,&rarg->error);
        dbgl_blk(3,"IOCTL_NDCMD_UNREGISTER name=%s err=%d", name, err);
        result = (err == NDAS_OK) ? 0: -EINVAL;
    }
    break;
    case IOCTL_NDCMD_ENABLE:
    {
        int slot = 0;
        int write_mode = 0;
        ndas_error_t err;
        ndas_ioctl_enable_ptr earg = (ndas_ioctl_enable_ptr) arg;
        get_user(slot,&earg->slot);
        get_user(write_mode,&earg->write_mode);

        err = ndcmd_enable(slot,write_mode);
        
        put_user(err,&earg->error);
        if ( err == NDAS_ERROR_INVALID_SLOT_NUMBER ) {
            dbgl_blk(3, "ed NDAS_ERROR_INVALID_SLOT_NUMBER");
            result = -ENODEV;
        } else if ( err == NDAS_ERROR_ALREADY_ENABLED_DEVICE ) {
            dbgl_blk(3, "ed NDAS_ERROR_ALREADY_ENABLED_DEVICE");
            result = -EINVAL; 
        } else if ( err == NDAS_ERROR_OUT_OF_MEMORY ) {
            dbgl_blk(3, "ed NDAS_ERROR_OUT_OF_MEMORY");
            result = -ENOMEM;
        } else if ( err == NDAS_ERROR_TRY_AGAIN ) {
            dbgl_blk(3, "ed NDAS_ERROR_TRY_AGAIN");
            result = -EAGAIN;
        } else if ( !NDAS_SUCCESS(err) ) {
            dbgl_blk(3, "ed unknown error %d",err);    
            result = -EINVAL;
        }     
    }
    break;
    case IOCTL_NDCMD_DISABLE:
    {
        int slot = 0;
        ndas_error_t err;
        ndas_ioctl_arg_disable_ptr darg = (ndas_ioctl_arg_disable_ptr) arg;

        get_user(slot,&darg->slot);
        err = ndcmd_disable(slot);
        put_user(err,&darg->error);

        if ( err == NDAS_ERROR_INVALID_SLOT_NUMBER ) {
            result = -ENODEV;
        } else if ( err != NDAS_OK ) {
            result = -EINVAL;
        } else 
            result = 0;
    }
    break;
    case IOCTL_NDCMD_PROBE:
    {    /* no consideration for race condition. */
        ndas_error_t err;
        ndas_ioctl_probe_ptr parg = (ndas_ioctl_probe_ptr) arg;
        char *user_array = NULL;
        int user_array_sz = 0;
        int probe_count = 0;
        get_user(user_array, &parg->ndasids);
        get_user(user_array_sz, &parg->sz_array);
        err = ndcmd_probe(user_array, user_array_sz, &probe_count, FALSE);
        put_user(err, &parg->error);
        put_user(probe_count, &parg->nr_ndasid);

        result = NDAS_SUCCESS(err) ? 0 : -EINVAL;
        
    }
    break;

    case IOCTL_NDCMD_PROBE_BY_ID:
    {    /* no consideration for race condition. */
        ndas_error_t err;
        ndas_ioctl_probe_ptr parg = (ndas_ioctl_probe_ptr) arg;
        char *user_array = NULL;
        int user_array_sz = 0;
        int probe_count = 0;
        get_user(user_array, &parg->ndasids);
        get_user(user_array_sz, &parg->sz_array);
        err = ndcmd_probe(user_array, user_array_sz, &probe_count, TRUE);

        put_user(err, &parg->error);
        put_user(probe_count, &parg->nr_ndasid);

        result = NDAS_SUCCESS(err) ? 0 : -EINVAL;

    }
    break;

    case IOCTL_NDCMD_QUERY:
    {
        ndas_error_t err;
        ndas_device_info info;
        char name[NDAS_MAX_NAME_LENGTH];
        ndas_ioctl_query_ptr parg = (ndas_ioctl_query_ptr) arg;

        result = copy_from_user(name, parg->name, NDAS_MAX_NAME_LENGTH);
        if ( result != 0 ) {
            result = -EINVAL;
            break;
        }
        err = ndas_get_ndas_dev_info(name, &info);
        if ( !NDAS_SUCCESS(err) )
        {
            result = -EINVAL;
            break;
        } 
        result = 0;
        if ( copy_to_user(parg->slots, info.slot, 
                NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE * sizeof(int)) != 0 ) {
            result = -EINVAL;
            break;
        }
        put_user(info.nr_unit, &parg->nr_unit);
    }
    break;
    case IOCTL_NDCMD_EDIT:
    {
        ndas_error_t err;
        char name[NDAS_MAX_NAME_LENGTH];
        char newname[NDAS_MAX_NAME_LENGTH];
        ndas_ioctl_edit_ptr parg = (ndas_ioctl_edit_ptr) arg;

        if ( copy_from_user(name, parg->name, NDAS_MAX_NAME_LENGTH) != 0 ) {
            result = -EINVAL;
            break;
        }
        if ( copy_from_user(newname, parg->newname, NDAS_MAX_NAME_LENGTH) ) {
            result = -EINVAL;
            break;
        }
        err = ndas_set_registration_name(name, newname);
        put_user(err, &parg->error);
        if ( !NDAS_SUCCESS(err) ) {
            result = -EINVAL;
            break;
        } 
        result = 0;
    }
    
    break;
    case IOCTL_NDCMD_REQUEST:
    {
        int slot = 0;
        ndas_error_t err;
        ndas_ioctl_request_ptr rarg = (ndas_ioctl_request_ptr) arg; 

        get_user(slot, &rarg->slot);
        err = ndcmd_request_permission(slot);
        put_user(err, &rarg->error);

        if ( err != NDAS_OK ) {
            result = -EINVAL;
        } else
            result = 0;
    }
    break;

    default:
#ifdef NDAS_MSHARE
		// juke_ioctl may handle unhandled ioctls
		result = juke_ioctl(inode, filp, cmd, arg);
#else
		result = -EINVAL;
#endif //#ifdef NDAS_MSHARE

    }

    dbgl_blk(3,"ed cmd=%x result=%d", cmd, result);
    return result;
}


static struct file_operations ndasctrl_fops = {
	.write = ndas_ctrldev_write,
	.read = ndas_ctrldev_read,
	.open = ndas_ctrldev_open,
	.release = ndas_ctrldev_release,
	.ioctl = ndas_ctrldev_ioctl, 
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
    .owner =            THIS_MODULE
#endif
};


#if LINUX_VERSION_25_ABOVE 
#if LINUX_VERSION_HAS_CLASS_CREATE 
static struct class *ndas_ctrldev_class;
#else
static struct class_simple *ndas_ctrldev_class;
#endif
#endif

int ndas_ctrldev_init(void)
{
	int ret = 0;

#ifdef NDAS_DEVFS
        // Create control device file
	devfs_control_handle = devfs_register(NULL, "ndas",
		DEVFS_FL_DEFAULT, NDAS_CHR_DEV_MAJOR, 0,
		S_IFCHR | S_IRUGO | S_IWUGO,
		&ndasctrl_fops, NULL);

	if (!devfs_control_handle) {
		printk(KERN_ERR "Failed to register control device file\n");
		return -EBUSY;
	}
#else
    ret = register_chrdev(NDAS_CHR_DEV_MAJOR, "ndas", &ndasctrl_fops);
    if (ret < 0) { 
        printk(KERN_ERR "ndas: can't register char device\n");
        return ret;
    }
#endif

#if LINUX_VERSION_25_ABOVE 
#if LINUX_VERSION_HAS_CLASS_CREATE 
    ndas_ctrldev_class = class_create(THIS_MODULE, "ndas");
    if (IS_ERR(ndas_ctrldev_class)) {
#ifdef NDAS_DEVFS
		devfs_unregister(devfs_control_handle);
#else
		unregister_chrdev(NDAS_CHR_DEV_MAJOR, "ndas");
#endif
        return PTR_ERR(ndas_ctrldev_class);
    }

#if LINUX_VERSION_HAS_DEVICE_CREATE
    device_create_drvdata(ndas_ctrldev_class, NULL, 	
        MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR),
        NULL, "ndas");
#else
    class_device_create(ndas_ctrldev_class, NULL,
        MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR),
        NULL, "ndas");
#endif //#if LINUX_VERSION_HAS_DEVICE_CREATE

#else 
    ndas_ctrldev_class = class_simple_create(THIS_MODULE, "ndas");
    if (IS_ERR(ndas_ctrldev_class)) {
#ifdef NDAS_DEVFS
		devfs_unregister(devfs_control_handle);
#else
		unregister_chrdev(NDAS_CHR_DEV_MAJOR, "ndas");
#endif
        return PTR_ERR(ndas_ctrldev_class);
    }
    class_simple_device_add(ndas_ctrldev_class,
        MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR),
        NULL, "ndas");
#endif  /* LINUX_VERSION_HAS_CLASS_CREATE */
#endif /* LINUX_VERSION_25_ABOVE  */

    return ret;
}

int ndas_ctrldev_cleanup(void)
{
#if LINUX_VERSION_25_ABOVE 
#if LINUX_VERSION_HAS_CLASS_CREATE 

#if LINUX_VERSION_HAS_DEVICE_CREATE
    device_destroy(ndas_ctrldev_class, MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR));
#else
    class_device_destroy(ndas_ctrldev_class, MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR));
#endif //#if LINUX_VERSION_HAS_DEVICE_CREATE

    class_destroy(ndas_ctrldev_class);
#else
    class_simple_device_remove(MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR));
    class_simple_destroy(ndas_ctrldev_class);
#endif  /* LINUX_VERSION_HAS_CLASS_CREATE */
#endif /* LINUX_VERSION_25_ABOVE  */

#ifdef NDAS_DEVFS
		devfs_unregister(devfs_control_handle);
#else
		unregister_chrdev(NDAS_CHR_DEV_MAJOR, "ndas");
#endif

    return 0;
}

