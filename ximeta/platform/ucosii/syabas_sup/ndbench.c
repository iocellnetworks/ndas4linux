/***************************************************
* Module name:
*
* Copyright 2005 Syabas Technology.
* All Rights Reserved.
*
*  The information contained herein is confidential
* property of Syabas Technology. The use, copying, transfer or
* disclosure of such information is prohibited except
* by express written agreement with Syabas Technology.
*
* First written on 4/20/2005 11:52PM by OngEK.
* $Id: ndbench.c,v 1.3 2005/05/23 06:24:14 EKOng Exp $
*
* Module Description:
*
***************************************************/

/*  Include section
* Add all #includes here
*
***************************************************/
#include "ucos_ii.h"
#include "fileio.h"
#include "upnp.h"
#include <ndasuser/io.h>
#include <ndasuser/ndasuser.h>

/*  Defines section
* Add all #defines here
*
***************************************************/
#if 0
#define NDAS_DEBUG    debug_outs
#else
#define NDAS_DEBUG(x,...)
#endif

#define MAX_NDAS_UNIT_COUNT        16
#define NDAS_OFFLINE            0x80000000

/*  Variable Section
* Add all local variables here.
*
***************************************************/
struct ndas_reg_struct {
    UCHAR ndas_reg_id[NDAS_ID_LENGTH + 1];
    INT32U ndas_reg_slot;
};

struct ndas_reg_global {
    INT32U total;
    INT32U enabled;
    struct ndas_reg_struct dev[MAX_NDAS_UNIT_COUNT];
};

struct ndas_reg_global ndas_global;
INT32U ndas_activity_count = 0;
INT32U ndas_changed_count = 0;

/*  Function Prototype Section
* Add prototypes for all functions called by this
* module, with the exception of runtime routines.
*
***************************************************/

/***************************************************
* Function name    : ndas_datain
*    returns    : 0 = read success, 1 = read fail
*    arg1        : NDAS device (0~15)
*    arg2        : LBA address
*    arg3        : number of sectors to be read
*    arg4        : buffer to store read data
* Created by    : OngEK
* Date created    : 4/21/2005 12:09AM
* Description    : read sectors from NDAS to buf
* Notes            :
***************************************************/
INT32U ndas_datain(INT8U dev, INT32U startsector, INT32U numsectors, void *buf)
{
    ndas_error_t    status;
    ndas_io_request req = {0};
    INT32U retry_count = 5;
    INT32U slot = ndas_global.dev[dev & 0xf].ndas_reg_slot;

    if(slot == 0 || (slot & NDAS_OFFLINE))
        return 1;

    if(ndas_global.enabled != slot)
    {
        ndas_disable_slot(ndas_global.enabled);
/*        ndas_set_encryption_mode(slot, FALSE, FALSE); */
        status = ndas_enable_slot(slot);
        if (!NDAS_SUCCESS(status)) {
            return 1;
        }
        ndas_global.enabled = slot;
    }

    req.start_sec = startsector;
    req.num_sec = numsectors;
    req.buf = buf;
    NDAS_DEBUG("~1o1~ slot=%d, start=0x%x size=%d\r\n", slot, startsector, numsectors);

    while(retry_count) {
        status = ndas_read(slot, &req);
        if(NDAS_SUCCESS(status))
            return 0;
        retry_count--;
        while(retry_count) {
            OSTimeDly(1000);
            ndas_disable_slot(slot);
            status = ndas_enable_slot(slot);
            if (NDAS_SUCCESS(status)) {
                break;
            } else {
                retry_count--;
            }
        }
    }
    return 1;
}

/***************************************************
* Function name    : NDAS_connection_changed_handler
*    arg1        : slot number
*    arg2        :
* Created by    : OngEK
* Date created    : 4/20/2005 11:26PM
* Description    :
* Notes            :
***************************************************/
static void NDAS_connection_changed_handler(int slot, ndas_error_t err, void* arg)
{
}

/***************************************************
* Function name    : NDAS_register
*    returns    : 0 = fail, 1 = register success
*    arg1        : structure holding information for the NDAS
*    arg2        : id of the NDAS
*    arg3        : write access key (not supported yet)
* Created by    : OngEK
* Date created    : 4/20/2005 11:56PM
* Description    : register NDAS get the slot info
* Notes            : assume only info.slot[0] available
***************************************************/
static INT32U NDAS_register(struct ndas_reg_struct *ndasdev, UCHAR* ndas_id, UCHAR* ndas_key)
{
    ndas_error_t     status;
    ndas_device_info info;

    status = ndas_register_device(ndas_id, ndas_id, ndas_key);
    if (!NDAS_SUCCESS(status)) {
        return 0;
    }
    status = ndas_query_device(ndas_id, &info);
    if (!NDAS_SUCCESS(status)) {
        return 0;
    }

    ndasdev->ndas_reg_slot = info.slot[0];
    ndas_set_slot_handlers(ndasdev->ndas_reg_slot, 
        NDAS_connection_changed_handler, 
        NDAS_connection_changed_handler, 
        NULL,
        NULL);
#if 0
    status = ndas_enable_slot(ndasdev->ndas_reg_slot);
    if (!NDAS_SUCCESS(status)) {
        return 0;
    }
#endif
    return 1;
}

/***************************************************
* Function name    : add_ndas_reg_dev
*    returns    : 1 = success, 0 = fail
*    arg1        : id of the NDAS
* Created by    : OngEK
* Date created    : 4/20/2005 11:43PM
* Description    : add Netdisk as registered device
* Notes            :
***************************************************/
static INT32U add_ndas_reg_dev(UCHAR* ndas_id)
{
    INT32U i;

    if(ndas_global.total >= MAX_NDAS_UNIT_COUNT)
        return 0;

    for(i = 0; i < MAX_NDAS_UNIT_COUNT; i++) {
        if(ndas_global.dev[i].ndas_reg_slot == 0) {
            if(NDAS_register(&ndas_global.dev[i], ndas_id, NULL))    {
                ndas_global.total++;
                strcpy(ndas_global.dev[i].ndas_reg_id, ndas_id);
                init_NetDisk(i);
                return 1;
            }
        }
    }
    return 0;
}

/***************************************************
* Function name    : search_for_reg_ndas
*    returns    : 0~15 = registered, 16 = not registered
*    arg1        : dev id of NDAS
* Created by    : OngEK
* Date created    : 4/20/2005 11:40PM
* Description    : Check whether NDAS with particular dev id is already registered
* Notes            : if registered, return value indicate the dev number in the global
***************************************************/
static INT32U search_for_reg_ndas(UCHAR* ndas_id)
{
    INT32U i;

    for(i = 0; i < MAX_NDAS_UNIT_COUNT; i++)
        if(ndas_global.dev[i].ndas_reg_slot != 0
                && !strcmp(ndas_global.dev[i].ndas_reg_id, ndas_id))
            break;
    return i;
}

/***************************************************
* Function name    : add_netdisk
*    arg1        : information of online NDAS
*    arg2        : total available NDAS online
* Created by    : OngEK
* Date created    : 4/20/2005 8:35PM
* Description    : if device is not yet registered, register it
* Notes            :
***************************************************/
static void add_netdisk(struct ndas_probed *devlist, INT32U proped_total)
{
    INT32U i;

    for(i = 0; i < proped_total; i++) {
        if(search_for_reg_ndas(devlist[i].id) == MAX_NDAS_UNIT_COUNT)    {
            add_ndas_reg_dev(devlist[i].id);
        }
    }
}

/***************************************************
* Function name    : ndas_scan
* Created by    : OngEK
* Date created    : 4/20/2005 12:46PM
* Description    : scan for available NDAS on the network
* Notes            :
***************************************************/
void ndas_scan(void)
{
    INT32U i;
    INT32U probed_count;
    INT32U ret;
    struct ndas_probed *devlist;

    if(ndas_changed_count != ndas_activity_count) {
        ndas_changed_count = ndas_activity_count;

        for(i = 0; i < MAX_NDAS_UNIT_COUNT; i++)
            if(ndas_global.dev[i].ndas_reg_slot & NDAS_OFFLINE) {
                ndas_unregister_device(ndas_global.dev[i].ndas_reg_id);
                remove_fat_any(i | DISK_NDAS);
                ndas_global.dev[i].ndas_reg_slot = 0;
                ndas_global.total--;
            }

        probed_count = ndas_probed_size();
        if(probed_count) {
            devlist = OS_malloc(sizeof(struct ndas_probed) * probed_count);
            if(devlist)    {
                probed_count = ndas_probed_list(devlist, probed_count);
                add_netdisk(devlist, probed_count);
            }
        }
        else if(ndas_global.total != 0) {
        }
    }
}

/***************************************************
* Function name    : NDAS_status_change_handler
*    arg1        : device id of changed drive
*    arg2        : latest status. 1 = online, 0 = offline
* Created by    : OngEK
* Date created    : 4/20/2005 3:56PM
* Description    : inidicate to OS that status of some NDAS has changed
* Notes            : handler should not sleep. Just set flag for another task handle the change
***************************************************/
static void NDAS_status_change_handler(const char* devid, const char *name,int online)
{
    INT32U i;

    if(online == 0) {
        i = search_for_reg_ndas(devid);
        if(i != MAX_NDAS_UNIT_COUNT)
        {
            if(ndas_global.enabled == ndas_global.dev[i].ndas_reg_slot)
                ndas_global.enabled = 0;
            ndas_global.dev[i].ndas_reg_slot |= NDAS_OFFLINE;
        }
    }

    ndas_activity_count++;
#ifdef UPNP
    if(f_upnp_advertise)
        myihome_refresh_skip = glb_sec - REFRESH_SKIP_DELAY;
#endif
}

/***************************************************
* Function name    : init_reg_dev
* Created by    : OngEK
* Date created    : 4/20/2005 8:16PM
* Description    : initialise global variable
* Notes            :
***************************************************/
static void init_reg_dev(void)
{
    INT32U i;

    ndas_global.total = 0;
    ndas_global.enabled = 0;

    for(i = 0; i < MAX_NDAS_UNIT_COUNT; i++)
    {
        ndas_global.dev[i].ndas_reg_slot = 0;
    }
}

/***************************************************
* Function name    : ndas_begin
* Created by    : OngEK
* Date created    : 4/20/2005 12:45PM
* Description    : initialisation, create NDAS task
* Notes            :
***************************************************/
void ndas_begin(void)
{
    int ret;
    int retry_count = 0;

    init_reg_dev();
    sal_init();
    ndas_init(0, 0);
    while(1) {
        ret = ndas_register_network_interface("eth0");
        if (ret == 0) {
            break;
        } else {
            NDAS_DEBUG("Deferring ndas_register_network_interface(%d)\r\n", retry_count);
            OSTimeDly(2000);
        }
        retry_count++;
        if (retry_count > 10) {
            NDAS_DEBUG("Failed to register interface %s\r\n");
            return;
        }
    }
    ndas_set_device_handler(NDAS_status_change_handler);
    ndas_start();
}

