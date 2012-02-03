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
#include <linux/version.h>
#include <linux/module.h> 

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include <ndas_dev.h>

#include "ndas_sdev.h"
#include "ndas_procfs.h"
#include "ndas_bdev.h"
#include "ndas_regist.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

#ifdef MOD_INC_USE_COUNT
#undef MOD_INC_USE_COUNT
#endif
#ifdef MOD_DEC_USE_COUNT
#undef MOD_DEC_USE_COUNT
#endif
#define MOD_INC_USE_COUNT do {} while(0)
#define MOD_DEC_USE_COUNT do {} while(0)

#endif

#ifdef DEBUG

int	dbg_level_nproc = DBG_LEVEL_NPROC;

#define dbg_call(l,x...) do { 								\
    if (l <= dbg_level_nproc) {								\
        printk("PF|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif

#ifndef __user // for 2.4
#define __user
#endif

struct list_head slots_node;
struct list_head devs_node;

static struct proc_dir_entry *ndas_dir;
static struct proc_dir_entry *net_recv_file;
static struct proc_dir_entry *net_drop_file;
static struct proc_dir_entry *regdata_file;
static struct proc_dir_entry *registered_file;
static struct proc_dir_entry *probed_file;
static struct proc_dir_entry *ndas_devs_file;
static struct proc_dir_entry *ndas_devs_dir;
static struct proc_dir_entry *ndas_version;
static struct proc_dir_entry *ndas_max_slot;
static struct proc_dir_entry *ndas_slots_list;
static struct proc_dir_entry *ndas_slots_dir;
static struct proc_dir_entry *ndas_nr_part;

static void nproc_ndev_remove_all(void);

unsigned long sal_net_recv_count=0;
unsigned long sal_net_drop_count=0;

static int proc_net_recv_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    char* pos = page;
    pos+= sprintf(pos, "%lu\n", sal_net_recv_count);
    return pos-page;
}

static int proc_net_drop_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    char* pos = page;
    pos+= sprintf(pos, "%lu\n", sal_net_drop_count);
    return pos-page;
}

/*
 * Update the registration data every changes
 */
static ndas_error_t update_regdata(void) 
{
    if ( regdata_file->data != NULL )  {
        ndas_kfree(regdata_file->data);
        regdata_file->data = NULL;
        regdata_file->size = 0;
    }
        
    return ndev_get_registration_data((char**)&regdata_file->data, (int*)&regdata_file->size);
}

static int proc_ndas_devs_file_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    int i, j,size;
    char* pos = page;
    struct ndas_registered *reg;
    ndas_error_t err;    
    ndas_device_info info;
    MOD_INC_USE_COUNT;
    size = ndev_registered_size();

    dbg_call(4,"st size=%d\n", size);

    if ( !NDAS_SUCCESS(size) ) {
        MOD_DEC_USE_COUNT;
        return -EINVAL;
    }
    reg = ndas_kmalloc(size * sizeof(struct ndas_registered));
    if ( !reg ) {

		NDAS_BUG_ON(true);
        MOD_DEC_USE_COUNT;
        return -ENOMEM;
    }
    size = ndev_registered_list(reg, size);
    if ( !NDAS_SUCCESS(size) ) {
        ndas_kfree(reg);
        MOD_DEC_USE_COUNT;
        return -EINVAL;
    }

    pos+=sprintf(pos, "%-10s\t%-22s %-3s %-17s%-4s%-15s%s\n",
        "Name", "ID", "Key", "Serial", "Ver ", "Status", "Slots");
    for ( i = 0; i < size; i++ ) {
        char* onlinestatus;
        int parent_slot = NDAS_INVALID_SLOT;
        pos += sprintf(pos, "%-10s\t", reg[i].name);
        err = ndev_get_ndas_dev_info(reg[i].name, &info);
        if (!NDAS_SUCCESS(err)) {
            char buf[65];
            ndas_get_string_error(err, buf, 64);
            buf[64]=0;
            pos+= sprintf(pos, "%s\n", buf);
            continue;
        }            
        memcpy(&info.ndas_id[15], "*****", 5);
        pos += sprintf(pos, "%-22s ", info.ndas_id);

        pos += sprintf(pos, "%-3s ", (info.ndas_key[0])?"Yes":"No");
        pos += sprintf(pos, "%-16s ", info.ndas_sid);

        if (info.version>=0)
            pos += sprintf(pos, "%-4d", info.version);    /* Version of NDAS device */
        else
            pos += sprintf(pos, "%-4s", "N/A");
        switch(info.status) {
        case NDAS_DEV_STATUS_HEALTHY:
            if ( info.nr_unit > 0) {
                onlinestatus = "Online";
            } else {
                onlinestatus = "Connecting";
            }
            break;
        case NDAS_DEV_STATUS_OFFLINE: 
            onlinestatus = "Offline";
            break;
        case NDAS_DEV_STATUS_CONNECT_FAILURE: 
            onlinestatus = "Connect Error";
            break;
        case NDAS_DEV_STATUS_LOGIN_FAILURE:
            onlinestatus = "Login Error";
            break;
        case NDAS_DEV_STATUS_IO_FAILURE:
            onlinestatus = "IO error";
            break;
        case NDAS_DEV_STATUS_TARGET_LIST_FAILURE:
            onlinestatus = "Info retrieval failure";
            break;            
        case NDAS_DEV_STATUS_UNKNOWN:
        default: 
            onlinestatus = "N/A"; 
            break; /* Status is unknown yet or NDAS device detection function is disabled in the ndas_core module */
        }
        pos += sprintf(pos, "%-15s", onlinestatus);
        for (j=0;j<info.nr_unit;j++) 
        {
            ndas_slot_info_t sinfo;
            ndas_unit_info_t uinfo;
            pos += sprintf(pos, "%d ", info.slot[j]);

            if ( info.slot[j] < NDAS_FIRST_SLOT_NR ) 
                continue;
            err = sdev_query_slot_info(info.slot[j], &sinfo);
            if ( !NDAS_SUCCESS(err) ) 
                continue;

            if ( sinfo.mode == NDAS_DISK_MODE_SINGLE ) 
                continue;
            if ( sinfo.mode == NDAS_DISK_MODE_MEDIAJUKE) 
                continue;
            err = sdev_query_unit(info.slot[j], &uinfo);

            if ( !NDAS_SUCCESS(err) ) 
                continue;
            if ( uinfo.raid_slot != parent_slot ) {
                pos += sprintf(pos, "%d(raid) ", uinfo.raid_slot);
                parent_slot = uinfo.raid_slot;
            }
        }
        pos+= sprintf(pos,"\n");
    }

    ndas_kfree(reg);
    dbg_call(4,"ed size=%ld\n", (long)(pos-page));
    MOD_DEC_USE_COUNT;
    return pos-page;
}
static int proc_ndas_max_slot_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    char* pos = page;
    pos+= sprintf(pos, "%d\n", NDAS_MAX_SLOT);
    return pos-page;
}
#define __xstr__(x) __str__(x)
#define __str__(x) #x
static int proc_ndas_version_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    char* pos = page;
    int major, minor, build;
    ndas_get_version(&major, &minor, &build);
    pos+= sprintf(pos, "%d.%d-%d\n", major, minor, build);
    return pos-page;
}
#undef __str__
#undef __xstr__
static int proc_ndas_nr_part_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    char* pos = page;
    pos+= sprintf( pos, "%d\n", NDAS_NR_PARTITION );
    return pos-page;
}

static int proc_ndas_slots_file_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    int i, j,size;
    char* pos = page;
    struct ndas_registered *reg;
    ndas_error_t err;    
    ndas_device_info dev_info;
    ndas_slot_info_t slot_info;
    MOD_INC_USE_COUNT;
    size = ndev_registered_size();
    if ( !NDAS_SUCCESS(size) ) {
        MOD_DEC_USE_COUNT;
        return -EINVAL;
    }
    reg = ndas_kmalloc(size * sizeof(struct ndas_registered));
    if ( !reg ) {
        MOD_DEC_USE_COUNT;
        return -ENOMEM;
    }
    size = ndev_registered_list(reg, size);
    if ( !NDAS_SUCCESS(size) ) {
        ndas_kfree(reg);
        MOD_DEC_USE_COUNT;
        return -EINVAL;
    }

    for ( i = 0; i < size; i++ ) 
    {
        err = ndev_get_ndas_dev_info(reg[i].name, &dev_info);
        if (!NDAS_SUCCESS(err)) {
            dbg_call(1, "device query failed=%d\n", err);
            continue;
        }
        for(j=0;j<dev_info.nr_unit;j++) {
            if (dev_info.status!=NDAS_DEV_STATUS_HEALTHY) {
                pos+=sprintf(pos,     "%d\n", dev_info.slot[j]); 
                continue;
            }
            err = sdev_query_slot_info(dev_info.slot[j], &slot_info);
            if (NDAS_SUCCESS(err)) {
                pos+=sprintf(pos, "%d\n", dev_info.slot[j]);
            } else {
                dbg_call(1, "slot query failed=%d\n", err);
                continue;
            }
        }
    }
    ndas_kfree(reg);
    dbg_call(5,"ed size=%ld\n", (long)(pos-page));
    MOD_DEC_USE_COUNT;
    return pos-page;

}

/*
 * Read the registration data from the NDAS Core by User application
 */
static int regdata_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    ndas_error_t err;
    MOD_INC_USE_COUNT;
    err = update_regdata();
    if ( !NDAS_SUCCESS(err) ) {
        dbg_call(1, "err=%d\n", err);
        strcpy(page, "ERROR:");
        ndas_get_string_error(err, page+strlen(page), 64);
        page[64] = 0;
        MOD_DEC_USE_COUNT;
        return -1;
    }
        
    memcpy(page, regdata_file->data, regdata_file->size);
    MOD_DEC_USE_COUNT;
    dbg_call(5,"ed size=%ld\n", (long int)regdata_file->size);
    dbg_call(5,"ed data=%p\n", regdata_file->data);
    dbg_call(8,"ed data=%s\n", NDAS_DEBUG_HEXDUMP(regdata_file->data, regdata_file->size));
    return regdata_file->size;
}

/* 
 * Write the registration data from user application to the NDAS CORE
 */
static int regdata_write(struct file *file, const char __user *buffer,
               unsigned long count, void *data)
{
    struct ndas_registered *reg;
    ndas_error_t err;    
    ndas_error_t size;
    int i;
    
    dbg_call(2, "ing count=%ld\n", count);
    MOD_INC_USE_COUNT;
    if ( regdata_file->data && regdata_file->size ) {
        kfree(regdata_file->data);
        regdata_file->data = NULL;
        regdata_file->size = 0;
    }
    regdata_file->data = ndas_kmalloc(count);
    regdata_file->size = count;

    if(copy_from_user(regdata_file->data, buffer, count)) {
        MOD_DEC_USE_COUNT;
        return -EFAULT;
    }
    /*
      * To do: Use readable configuration file and parse in user-level
      */
    err= ndev_set_registration_data(regdata_file->data, regdata_file->size);
    if ( !NDAS_SUCCESS(err)) {
        kfree(regdata_file->data);
        regdata_file->data = NULL;
        regdata_file->size = 0;
        MOD_DEC_USE_COUNT;
        dbg_call(2, "ed -1\n");
        return -1;
    }

    /* 
        Register /proc/ndas/devices entry 
        Temp fix: Enable them if neccessary. This should be done when processing config file
    */
    size = ndev_registered_size();
    if ( !NDAS_SUCCESS(size) ) {
        MOD_DEC_USE_COUNT;
        return -EINVAL;
    }

    reg = ndas_kmalloc(size * sizeof(struct ndas_registered));
    if ( !reg ) {
        MOD_DEC_USE_COUNT;
        return -ENOMEM;
    }
    size = ndev_registered_list(reg, size);
    if ( !NDAS_SUCCESS(size) ) {
        ndas_kfree(reg);
        MOD_DEC_USE_COUNT;
        return -EINVAL;
    }

    for ( i = 0; i < size; i++ ) {
        nproc_add_ndev(reg[i].name);
    }

    
    MOD_DEC_USE_COUNT;
    dbg_call(2, "ed\n");
    return regdata_file->size;
}

/*
 * Read the registration data from the NDAS Core by User application
 */
static int registered_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    int i, size, len = 0;
    struct ndas_registered *reg;
    dbg_call(5, "ing count=%d\n", count);
    
    MOD_INC_USE_COUNT;
    size = ndev_registered_size();

    if ( !NDAS_SUCCESS(size) ) {
        MOD_DEC_USE_COUNT;
        return -EINVAL;
    }
    dbg_call(5, "ing size=%d\n", size);
    
    reg = kmalloc(size * sizeof(struct ndas_registered),GFP_ATOMIC);
    if ( !reg ) {
        MOD_DEC_USE_COUNT;
        return -ENOMEM;
    }
    size = ndev_registered_list(reg, size);
    
    if ( !NDAS_SUCCESS(size) ) {
        kfree(reg);
        MOD_DEC_USE_COUNT;
        return -EINVAL;
    }
    
    for ( i = 0; i < size; i++ ) {
        int n = sprintf(page, "%s\n", reg[i].name);
        page += n;
        len += n;
    }
    
    MOD_DEC_USE_COUNT;
    dbg_call(5,"ed size=%d\n", len);
    kfree(reg);
    return len;
}


 /*
 * Read the registration data from the NDAS Core by User application
 */
static int probed_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    int i, size, len = 0;
    struct ndas_probed *reg;
    dbg_call(5, "ing count=%d\n", count);
    
    MOD_INC_USE_COUNT;
    size = ndev_probe_size();

    if ( !NDAS_SUCCESS(size) ) {
        MOD_DEC_USE_COUNT;
        return -EINVAL;
    }
    dbg_call(5, "ing size=%d\n", size);
    
    reg = kmalloc(size * sizeof(struct ndas_probed),GFP_ATOMIC);
    if ( !reg ) {
        MOD_DEC_USE_COUNT;
        return -ENOMEM;
    }
    size = ndev_probe_ndas_id(reg, size);
    
    if ( !NDAS_SUCCESS(size) ) {
        kfree(reg);
        MOD_DEC_USE_COUNT;
        return -EINVAL;
    }
    
    for ( i = 0; i < size; i++ ) {
        int n = sprintf(page, "%s\n", reg[i].ndas_id);
        page += n;
        len += n;
    }
    
    MOD_DEC_USE_COUNT;
    dbg_call(5,"ed size=%d\n", len);
    kfree(reg);
    return len;
}   
ndas_error_t init_ndasproc(void) 
{
    ndas_error_t ret = NDAS_OK;
    dbg_call(1, "ing\n");
    ndas_dir = proc_mkdir("ndas", NULL);
    if (ndas_dir == NULL) {
        return NDAS_ERROR_OUT_OF_MEMORY; // out of memory
    }
    ndas_dir->owner = THIS_MODULE;

    net_recv_file= create_proc_entry("netrecv", 0644, ndas_dir);
    if ( net_recv_file == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out_1;
    }
    net_recv_file->data = NULL;
    net_recv_file->read_proc = proc_net_recv_read;
    net_recv_file->write_proc = NULL;
    net_recv_file->owner = THIS_MODULE;

    net_drop_file= create_proc_entry("netdrop", 0644, ndas_dir);
    if ( net_drop_file == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out0;
    }
    net_drop_file->data = NULL;
    net_drop_file->read_proc = proc_net_drop_read;
    net_drop_file->write_proc = NULL;
    net_drop_file->owner = THIS_MODULE;
    
    regdata_file = create_proc_entry("regdata", 0644, ndas_dir);
    if ( regdata_file == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out1;
    }
    regdata_file->data = NULL;
    regdata_file->read_proc = regdata_read;
    regdata_file->write_proc = regdata_write;
    regdata_file->owner = THIS_MODULE;

    registered_file = create_proc_entry("registered", 0444, ndas_dir);
    if ( registered_file == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out2;
    }
    registered_file->data = NULL;
    registered_file->read_proc = registered_read;
    registered_file->write_proc = NULL;
    registered_file->owner = THIS_MODULE;

    if (ndev_probe_size() != NDAS_ERROR_UNSUPPORTED_FEATURE) {
        probed_file = create_proc_entry("probed", 0444, ndas_dir);
        if ( probed_file == NULL ) {
            ret = NDAS_ERROR_OUT_OF_MEMORY;
            goto out3;
        }
        probed_file->data = NULL;
        probed_file->read_proc = probed_read;
        probed_file->write_proc = NULL;
        probed_file->owner = THIS_MODULE;
    } else {
        probed_file = NULL;
    }
    
    ndas_devs_file = create_proc_entry("devs", 0444, ndas_dir);
    if ( ndas_devs_file == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out4;
    }
    ndas_devs_file->data = NULL;
    ndas_devs_file->read_proc = proc_ndas_devs_file_read;
    ndas_devs_file->write_proc = NULL;
    ndas_devs_file->owner = THIS_MODULE;

    ndas_slots_list = create_proc_entry("slot_list", 0444, ndas_dir);
    if ( ndas_slots_list == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out5;
    }
    ndas_slots_list->data = NULL;
    ndas_slots_list->read_proc = proc_ndas_slots_file_read;
    ndas_slots_list->write_proc = NULL;
    ndas_slots_list->owner = THIS_MODULE;

    ndas_slots_dir = proc_mkdir("slots", ndas_dir);
    if ( ndas_slots_dir == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out6;
    }
    ndas_slots_dir->owner = THIS_MODULE;

    ndas_devs_dir = proc_mkdir("devices", ndas_dir);
    if ( ndas_devs_dir == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out7;
    }
    ndas_devs_dir->owner = THIS_MODULE;

    ndas_version = create_proc_entry("version", 0444, ndas_dir);
    if ( ndas_version == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out8;
    }
    ndas_version->data = NULL;
    ndas_version->read_proc = proc_ndas_version_read;
    ndas_version->write_proc = NULL;
    ndas_version->owner = THIS_MODULE;

    ndas_max_slot = create_proc_entry("max_slot", 0444, ndas_dir);
    if ( ndas_max_slot == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out9;
    }
    ndas_max_slot->data = NULL;
    ndas_max_slot->read_proc = proc_ndas_max_slot_read;
    ndas_max_slot->write_proc = NULL;
    ndas_max_slot->owner = THIS_MODULE;

    ndas_nr_part = create_proc_entry("partitions_per_device", 0444, ndas_dir);
    if ( ndas_nr_part == NULL ) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out10;
    }
    ndas_nr_part->data = NULL;
    ndas_nr_part->read_proc = proc_ndas_nr_part_read;
    ndas_nr_part->write_proc = NULL;
    ndas_nr_part->owner = THIS_MODULE;
    
    INIT_LIST_HEAD(&slots_node);
    INIT_LIST_HEAD(&devs_node);

    dbg_call(1, "exit\n");

    return NDAS_OK;
out10:    
    remove_proc_entry("max_slot", ndas_dir);
    ndas_max_slot = NULL;
out9:    
    remove_proc_entry("version", ndas_dir);
    ndas_version = NULL;
out8:
    remove_proc_entry("devices", ndas_dir);
    ndas_devs_dir = NULL;
out7:    
    remove_proc_entry("slots", ndas_dir);
    ndas_slots_dir = NULL;
out6:
    remove_proc_entry("slot_list", ndas_dir);
    ndas_slots_list = NULL;
out5:
    remove_proc_entry("devs", ndas_dir);
    ndas_devs_file = NULL;
out4:
    remove_proc_entry("registered", ndas_dir);
    registered_file = NULL;
out3: 
    if (probed_file) {
        remove_proc_entry("probed", ndas_dir);
        probed_file = NULL;
    }
out2:
    remove_proc_entry("regdata", ndas_dir);
    regdata_file = NULL;
out1:    
    remove_proc_entry("netdrop", ndas_dir);
    net_drop_file = NULL;
out0:
    remove_proc_entry("netrecv", ndas_dir);
    net_recv_file= NULL;
out_1:    
    remove_proc_entry("ndas", NULL);
    ndas_dir = NULL;
    INIT_LIST_HEAD(&slots_node);
    
    return ret;
}

void cleanup_ndasproc(void) 
{
    dbg_call(1, "ing\n");

    remove_proc_entry("partitions_per_device", ndas_dir);
    ndas_nr_part = NULL;
    remove_proc_entry("max_slot", ndas_dir);
    ndas_max_slot = NULL;
    remove_proc_entry("version", ndas_dir);
    ndas_version = NULL;
    nproc_ndev_remove_all();
    remove_proc_entry("devices", ndas_dir);
    ndas_devs_dir = NULL;
    remove_proc_entry("slots", ndas_dir);
    ndas_slots_dir = NULL;
    remove_proc_entry("slot_list", ndas_dir);
    ndas_slots_list = NULL;
    remove_proc_entry("devs", ndas_dir);
    ndas_devs_file = NULL;
    if ( regdata_file ) {
        dbg_call(5,"ed data=%p\n", regdata_file->data);

        if ( regdata_file->data ) ndas_kfree(regdata_file->data);
        regdata_file->data = NULL;
        regdata_file->size = 0;
        remove_proc_entry("regdata", ndas_dir);
        regdata_file = NULL;
    }
    if (probed_file) {
        remove_proc_entry("probed", ndas_dir);
        probed_file = NULL;
    }
    remove_proc_entry("registered", ndas_dir);
    registered_file = NULL;
    remove_proc_entry("netdrop", ndas_dir);
    net_drop_file = NULL;
    remove_proc_entry("netrecv", ndas_dir);
    net_recv_file= NULL;    
    remove_proc_entry("ndas", NULL);
    ndas_dir = NULL;
}

static int proc_ndas_slot_ndas_id_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    char* pos = page;
    sdev_t *sd = data;
    MOD_INC_USE_COUNT;
    pos+=sprintf(pos, "%s", sd->info.ndas_id);
    MOD_DEC_USE_COUNT;
    return pos-page;    
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
static int proc_ndas_slot_devname_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    char* pos = page;
    sdev_t *sd = data;
    MOD_INC_USE_COUNT;
    pos+=sprintf(pos, "%s", sd->devname);
    MOD_DEC_USE_COUNT;
    return pos-page;    
}
#endif

static int proc_dev_nr_unit_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    ndas_error_t err;
    char* pos = page;
    struct ndev_node *node = data;

    MOD_INC_USE_COUNT;

    err = ndev_get_ndas_dev_info(node->info.name, &node->info);
    if ( NDAS_SUCCESS(err) ) {
        pos+=sprintf(pos, "%d", node->info.nr_unit);
    }
    MOD_DEC_USE_COUNT;
    return pos-page;    
}

static int proc_dev_serial_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    char* pos = page;
    struct ndev_node *node = data;

    MOD_INC_USE_COUNT;

    pos+=sprintf(pos, "%s\n", node->info.ndas_sid);
  
    MOD_DEC_USE_COUNT;
    return pos-page;    
}

static int proc_dev_id_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    char* pos = page;
    struct ndev_node *node = data;

    MOD_INC_USE_COUNT;

    pos+=sprintf(pos, "%s\n", node->info.ndas_id);
  
    MOD_DEC_USE_COUNT;
    return pos-page;    
}
    
static int proc_dev_slots_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    ndas_error_t err;
    char* pos = page;
    struct ndev_node *node = data;

    MOD_INC_USE_COUNT;

    err = ndev_get_ndas_dev_info(node->info.name, &node->info);
    if ( NDAS_SUCCESS(err) ) {
        int i;
        for ( i = 0; i < node->info.nr_unit; i++) {
            pos+=sprintf(pos, "%d\n", node->info.slot[i]);
        }
    }
    MOD_DEC_USE_COUNT;
    return pos-page;    
}

static int proc_ndas_slot_enabled_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    ndas_error_t err;
    char* pos = page;
    sdev_t *sd = data;
    ndas_slot_info_t sinfo;
    
    MOD_INC_USE_COUNT;

    err = sdev_query_slot_info(sd->info.slot, &sinfo);
    switch(err) {
    case NDAS_OK:
        break;
    default:
        goto out;
    }    
    pos+=sprintf(pos, "%d\n", sinfo.enabled);
out:
    MOD_DEC_USE_COUNT;
    return pos-page;    
}

static int proc_ndas_slot_unit_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    ndas_error_t err;
    char* pos = page;
    sdev_t *sd = data;
    ndas_slot_info_t sinfo;

    MOD_INC_USE_COUNT;

    err = sdev_query_slot_info(sd->info.slot, &sinfo);
    switch(err) {
    case NDAS_OK:
        break;
    default:
        goto out;
    }    
    pos+=sprintf(pos, "%d\n", sinfo.unit[0]);
out:    
    MOD_DEC_USE_COUNT;
    return pos-page;    
}

static int proc_sdev_timeout_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    ndas_error_t err;
    char* pos = page;
    sdev_t *sd = data;
    ndas_slot_info_t sinfo;

    MOD_INC_USE_COUNT;

    err = sdev_query_slot_info(sd->info.slot, &sinfo);
    switch(err) {
    case NDAS_OK:
        break;
    default:
        goto out;
    }    
    pos+=sprintf(pos, "%ld\n", sinfo.conn_timeout);
out:    
    MOD_DEC_USE_COUNT;
    return pos-page;    
}
    
static int proc_ndas_slot_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    ndas_error_t err;
    char* pos = page;
    sdev_t *sd = data;
    ndas_slot_info_t sinfo;
    
    MOD_INC_USE_COUNT;

    err = sdev_query_slot_info(sd->info.slot, &sinfo);
    switch(err) {
    case NDAS_OK:
        break;
    default:
        pos += sprintf(pos, "sdev_query_slot_info error %s(%d)\n",
            NDAS_GET_STRING_ERROR(err), err);
        goto out;
    }
    // handle error

    pos+=sprintf(pos, "%-10s%-9s%-8s%s\n",
         "Status", "Capacity", "Mode", "Major/Minor");
    
    pos+=sprintf(pos, "%-10s%6dGB %-7s %d/%d\n",  
        sinfo.enabled?"Enabled":"Disabled",
        (int)(sinfo.logical_blocks/2/1024/1024),    
        sinfo.mode == NDAS_DISK_MODE_SINGLE ? "Single"
            : sinfo.mode == NDAS_DISK_MODE_ATAPI ? "ATAPI"
            : sinfo.mode == NDAS_DISK_MODE_RAID0 ? "Raid-0"
            : sinfo.mode == NDAS_DISK_MODE_RAID1 ? "Raid-1"
            : sinfo.mode == NDAS_DISK_MODE_RAID4 ? "Raid-4" 
            : sinfo.mode == NDAS_DISK_MODE_AGGREGATION ? "Raid-Linear" 
            : sinfo.mode == NDAS_DISK_MODE_MIRROR? "Duplicate" 
            : "Unknown",
        NDAS_BLK_MAJOR, 
        (sd->info.slot - NDAS_FIRST_SLOT_NR)<<NDAS_PARTN_BITS);
out:    
    MOD_DEC_USE_COUNT;
    return pos-page;
}

#ifdef XPLAT_RAID

static int proc_ndas_raid_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    int i, sz;
    char* pos = page;
    ndas_error_t err;
    sdev_t *sd = (sdev_t *) data;
    ndas_raid_info info, *pinfo;
    
    MOD_INC_USE_COUNT;

    dbg_call(1, "sd->info.slot=%d\n", sd->info.slot);
    
    err = ndas_query_raid(sd->info.slot, &info, sizeof(info));
    
    if ( !NDAS_SUCCESS(err) ) {
        pos += sprintf(pos, "ndas_query_raid error %s(%d)\n",
            NDAS_GET_STRING_ERROR(err), err);
        goto out;
    }

    sz = sizeof(ndas_raid_info) + err * sizeof(pinfo->slots[0]);
    
    pinfo = kmalloc(sz, GFP_ATOMIC);

    if ( !pinfo ) {
        MOD_DEC_USE_COUNT;
        return -ENOMEM;
    }
    err = ndas_query_raid(sd->info.slot, pinfo, sz);
    
    if ( !NDAS_SUCCESS(err) ) {
        pos += sprintf(pos, "ndas_query_raid error %s(%d)\n",
            NDAS_GET_STRING_ERROR(err), err);
        goto out2;
    }
    pos+=sprintf(pos, "%-12s%-5s Slots\n",
          "Mode", "Disks");
    
    pos+=sprintf(pos, "%-12s", pinfo->mode == NDAS_DISK_MODE_RAID0 ? "Raid-0" 
        : pinfo->mode == NDAS_DISK_MODE_RAID1 ? "Raid-1" 
            : pinfo->mode == NDAS_DISK_MODE_RAID4 ? "Raid-4" 
                : pinfo->mode == NDAS_DISK_MODE_AGGREGATION ? "Raid-Linear" 
                    : pinfo->mode == NDAS_DISK_MODE_MIRROR? "Duplicate" 
                        : "Unknown"
        );
    pos+=sprintf(pos, "%-5d ", pinfo->disks);
    for ( i = 0; i < pinfo->disks; i++) {
        pos+=sprintf(pos, "%-4d", pinfo->slots[i]);
    }
out2:
    kfree(pinfo);
out:
    MOD_DEC_USE_COUNT;
    return pos-page;
}

#endif

static int proc_ndas_unit_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    ndas_error_t err;
    char* pos = page;
    sdev_t *sd = data;
    ndas_unit_info_t info;
    
    MOD_INC_USE_COUNT;

    err = sdev_query_unit(sd->info.slot, &info);
    if ( !NDAS_SUCCESS(err) ) {
        pos += sprintf(pos, "ndas_query_unit error %s(%d)\n",
            NDAS_GET_STRING_ERROR(err), err);
        goto out;
    }
    // handle error
    
    pos+=sprintf(pos, "%-21s%-14s%-21s%-6s%-7s%-9s\n",
          "Model","Firmware Rev.","Serial Number","Read", "Write",
          info.raid_slot > NDAS_INVALID_SLOT ? "Raid slot" : "");
    
    pos+=sprintf(pos, "%-21s", info.model);
    pos+=sprintf(pos, "%-14s", info.fwrev);
    pos+=sprintf(pos, "%-21s", info.serialno);
    pos+=sprintf(pos, "%-6d", info.rohost);
    pos+=sprintf(pos, "%-7d", info.rwhost);
    if ( info.raid_slot > NDAS_INVALID_SLOT) {
        pos+=sprintf(pos, "%-9d\n", info.raid_slot);
    } else {
        pos+=sprintf(pos, "\n");
    }
out:    
    MOD_DEC_USE_COUNT;
    return pos-page;

}

static int proc_ndas_info2_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    ndas_error_t err;
    char* pos = page;
    sdev_t *sd = data;
    ndas_slot_info_t sinfo;
    
    MOD_INC_USE_COUNT;

    err = sdev_query_slot_info(sd->info.slot, &sinfo);
    if ( !NDAS_SUCCESS(err) ) {
        pos += sprintf(pos, "sdev_query_slot_info error %s(%d)\n",
            NDAS_GET_STRING_ERROR(err), err);
        goto out;
    }

    if ( sinfo.mode == NDAS_DISK_MODE_MEDIAJUKE) {
        return 0;
    }

    if ( sinfo.mode == NDAS_DISK_MODE_SINGLE ||
         sinfo.mode == NDAS_DISK_MODE_ATAPI)  {
        return proc_ndas_unit_read(page, start, off,count,eof,data);
    }

	NDAS_BUG_ON(1);

#ifdef XPLAT_RAID
    return proc_ndas_raid_read(page, start, off,count,eof,data);
#endif

out:
    MOD_DEC_USE_COUNT;
    return pos-page;
}

static int proc_ndas_slot_sectors_read(char *page, char **start, off_t off,
                            int count, int *eof, void *data)
{
    ndas_error_t err;
    char* pos = page;
    sdev_t *sd = data;
    ndas_slot_info_t sinfo;
    
    MOD_INC_USE_COUNT;

    err = sdev_query_slot_info(sd->info.slot, &sinfo);
    if (NDAS_OK == err) {
        pos += sprintf(pos, "%lld", (long long)sinfo.logical_blocks);
    } else {
        pos += sprintf(pos, "0");
    }
    MOD_DEC_USE_COUNT;
    return pos-page;
}

ndas_error_t nproc_add_ndev(const char *name)
{
    struct ndev_node * node;
    struct list_head *pos;
    
    list_for_each(pos, &devs_node) {
        struct ndev_node * n = list_entry(pos, struct ndev_node, proc_node);
        if ( strncmp(n->name, name, NDAS_MAX_NAME_LENGTH) == 0 ) {
            return NDAS_ERROR_ALREADY_REGISTERED_DEVICE;
        }
    }
    
    node = kmalloc(sizeof(struct ndev_node), GFP_ATOMIC);
    if ( !node ) return NDAS_ERROR_OUT_OF_MEMORY;

    memset(node, 0, sizeof(struct ndev_node));
    memcpy(node->name, name, sizeof(node->name));
    list_add(&node->proc_node, &devs_node);

    ndev_get_ndas_dev_info(name, &node->info);
    
    node->proc_dir = proc_mkdir(name, ndas_devs_dir);
    if ( node->proc_dir == NULL ) 
        return NDAS_ERROR;
    node->proc_dir->owner = THIS_MODULE;

    node->proc_nr_unit = create_proc_entry("nr_unit", 0644, node->proc_dir);
    if ( node->proc_nr_unit == NULL ) 
        return NDAS_ERROR;
    node->proc_nr_unit->data = node;
    node->proc_nr_unit->read_proc = proc_dev_nr_unit_read;
    node->proc_nr_unit->write_proc = NULL;
    node->proc_nr_unit->owner = THIS_MODULE;

    node->proc_slots= create_proc_entry("slots", 0644, node->proc_dir);
    if ( node->proc_slots == NULL ) 
        return NDAS_ERROR;
    node->proc_slots->data = node;
    node->proc_slots->read_proc = proc_dev_slots_read;
    node->proc_slots->write_proc = NULL;
    node->proc_slots->owner = THIS_MODULE;

    node->proc_name = create_proc_entry("serial", 0644, node->proc_dir);
    if ( node->proc_name == NULL ) 
        return NDAS_ERROR;
    node->proc_name->data = node;
    node->proc_name->read_proc = proc_dev_serial_read;
    node->proc_name->write_proc = NULL;
    node->proc_name->owner = THIS_MODULE;

    node->proc_id = create_proc_entry("id", 0644, node->proc_dir);
    if ( node->proc_name == NULL ) 
        return NDAS_ERROR;
    node->proc_id->data = node;
    node->proc_id->read_proc = proc_dev_id_read;
    node->proc_id->write_proc = NULL;
    node->proc_id->owner = THIS_MODULE;
    
    return NDAS_OK;
}
void nproc_remove_ndev(const char *name)
{
    struct list_head *pos, *n;
    struct ndev_node *node = NULL;
    // take
    list_for_each_safe(pos, n, &devs_node) 
    {
        struct ndev_node *n = list_entry(pos, struct ndev_node, proc_node);
        if (  strncmp(n->name, name, sizeof(n->name)) == 0 ) {
            list_del(pos);
            node = n;
            break;
        }
    }
    // give
    if ( node == NULL ) {
        return ;
    }

    if ( node->proc_name ) {
        remove_proc_entry("serial", node->proc_dir);
        node->proc_name= NULL;
    }
    if ( node->proc_slots ) {
        remove_proc_entry("slots", node->proc_dir);
        node->proc_slots= NULL;
    }
    if ( node->proc_nr_unit ) {
        remove_proc_entry("nr_unit", node->proc_dir);
        node->proc_nr_unit= NULL;
    }

    if ( node->proc_id) {
        remove_proc_entry("id", node->proc_dir);
        node->proc_nr_unit= NULL;
    }
    
    remove_proc_entry(name, ndas_devs_dir);
    node->proc_dir= NULL;
    kfree(node);
}
static void nproc_ndev_remove_all()
{
    struct list_head *pos, *n;
    struct ndev_node *node = NULL;
    // take
    list_for_each_safe(pos, n, &devs_node) 
    {
        node = list_entry(pos, struct ndev_node, proc_node);
        if ( node->proc_name ) {
            remove_proc_entry("name", node->proc_dir);
            node->proc_name= NULL;
        }
        if ( node->proc_slots ) {
            remove_proc_entry("slots", node->proc_dir);
            node->proc_slots= NULL;
        }
        if ( node->proc_nr_unit ) {
            remove_proc_entry("nr_unit", node->proc_dir);
            node->proc_nr_unit= NULL;
        }
        
        remove_proc_entry(node->info.ndas_sid, ndas_devs_dir);
        node->proc_dir= NULL;
        kfree(node);
    }
    // give

}

void nproc_rename_ndev(char *oldname, char *newname)
{
}
ndas_error_t nproc_add_slot(sdev_t *sd)
{
    char buffer[16];
    list_add(&sd->proc_node, &slots_node);
    snprintf(buffer, 16, "%d", sd->info.slot);

    sd->proc_dir = proc_mkdir(buffer, ndas_slots_dir);
    if ( sd->proc_dir == NULL ) 
        return NDAS_ERROR;
    sd->proc_dir->owner = THIS_MODULE;
    
    sd->proc_info = create_proc_entry("info", 0644, sd->proc_dir);
    if ( sd->proc_info == NULL ) 
        return NDAS_ERROR;
    sd->proc_info->data = sd;
    sd->proc_info->read_proc = proc_ndas_slot_read;
    sd->proc_info->write_proc = NULL;
    sd->proc_info->owner = THIS_MODULE;

    sd->proc_unit = create_proc_entry("unit", 0644, sd->proc_dir);
    if ( sd->proc_unit == NULL ) 
        return NDAS_ERROR;
    sd->proc_unit->data = sd;
    sd->proc_unit->read_proc = proc_ndas_slot_unit_read;
    sd->proc_unit->write_proc = NULL;
    sd->proc_unit->owner = THIS_MODULE;

    sd->proc_timeout = create_proc_entry("timeout", 0644, sd->proc_dir);
    if ( sd->proc_timeout == NULL ) 
        return NDAS_ERROR;
    sd->proc_timeout->data = sd;
    sd->proc_timeout->read_proc = proc_sdev_timeout_read;
    sd->proc_timeout->write_proc = NULL;
    sd->proc_timeout->owner = THIS_MODULE;

    sd->proc_enabled = create_proc_entry("enabled", 0644, sd->proc_dir);
    if ( sd->proc_unit == NULL ) 
        return NDAS_ERROR;
    sd->proc_enabled->data = sd;
    sd->proc_enabled->read_proc = proc_ndas_slot_enabled_read;
    sd->proc_enabled->write_proc = NULL; // TODO ndas_enabled command
    sd->proc_enabled->owner = THIS_MODULE;
    
    sd->proc_ndas_serial = create_proc_entry("ndas_id", 0644, sd->proc_dir);
    if ( sd->proc_ndas_serial == NULL ) 
        return NDAS_ERROR;
    sd->proc_ndas_serial->data = sd;
    sd->proc_ndas_serial->read_proc = proc_ndas_slot_ndas_id_read;
    sd->proc_ndas_serial->write_proc = NULL;
    sd->proc_ndas_serial->owner = THIS_MODULE;


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    sd->proc_devname = create_proc_entry("devname", 0644, sd->proc_dir);
    if ( sd->proc_devname == NULL ) 
        return NDAS_ERROR;
    sd->proc_devname->data = sd;
    sd->proc_devname->read_proc = proc_ndas_slot_devname_read;
    sd->proc_devname->write_proc = NULL;
    sd->proc_devname->owner = THIS_MODULE;
#endif

    sd->proc_info2 = create_proc_entry("info2", 0644, sd->proc_dir);
    if ( sd->proc_info2 == NULL ) 
        return NDAS_ERROR;
    sd->proc_info2->data = sd;
    sd->proc_info2->read_proc = proc_ndas_info2_read;
    sd->proc_info2->write_proc = NULL;
    sd->proc_info2->owner = THIS_MODULE;

    sd->proc_sectors = create_proc_entry("sectors", 0644, sd->proc_dir);
    if ( sd->proc_sectors == NULL ) 
        return NDAS_ERROR;
    sd->proc_sectors->data = sd;
    sd->proc_sectors->read_proc = proc_ndas_slot_sectors_read;
    sd->proc_sectors->write_proc = NULL;
    sd->proc_sectors->owner = THIS_MODULE;

    
    return NDAS_OK;
}

void nproc_remove_slot(sdev_t *sd)
{
    char buffer[16];

    list_del(&sd->proc_node);
    
    remove_proc_entry("info2", sd->proc_dir);
    sd->proc_info2 = NULL;
    remove_proc_entry("info", sd->proc_dir);
    sd->proc_info = NULL;
    remove_proc_entry("timeout", sd->proc_dir);
    sd->proc_timeout = NULL;
    remove_proc_entry("unit", sd->proc_dir);
    sd->proc_unit = NULL;
    remove_proc_entry("enabled", sd->proc_dir);
    sd->proc_enabled = NULL;
    remove_proc_entry("ndas_id", sd->proc_dir);
    sd->proc_ndas_serial = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    remove_proc_entry("devname", sd->proc_dir);
    sd->proc_devname = NULL;
#endif    
    remove_proc_entry("sectors", sd->proc_dir);
    sd->proc_sectors = NULL;
        
    snprintf(buffer, 16, "%d", sd->info.slot);
    remove_proc_entry(buffer, ndas_slots_dir);
    sd->proc_dir = NULL;
}

