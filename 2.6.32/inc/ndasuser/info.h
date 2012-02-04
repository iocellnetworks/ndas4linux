/*
 -------------------------------------------------------------------------
 Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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
#ifndef _NDASUSER_INFO_H_
#define _NDASUSER_INFO_H_

#include <sal/types.h>
#include <sal/time.h>
#include <ndasuser/def.h>
#include <ndasuser/ndaserr.h>

/* Description
   This structure contains NDAS host's information.
 */
typedef struct _host_info {
    int permission;
    char hostname[NDAS_MAX_HOSTNAME_LENGTH + 1];
    /* number of network address node */
    int nr_node;
} ndas_host_info, *ndas_host_ptr;

typedef struct _unit_info {
    xuint64 sectors;    // Disk capacity in sector count
    xuint64 capacity;        // Disk Capacity in byte

    unsigned short headers_per_disk;
    unsigned short cylinders_per_header;
    unsigned short sectors_per_cylinder;

    char serialno[NDAS_MAX_DEVICE_SERIAL_LENGTH+1];
    char model[NDAS_DISK_MODEL_NAME_LENGTH+1];    /* Model name of disk. */
    char fwrev[NDAS_DISK_FMREV_LENGTH+1]; /* firmware revision */

    int type;        /* One of NDAS_UNIT_DISK_TYPE_xxx( NDAS_UNIT_TYPE_UNKNOWN, 
                            NDAS_UNIT_DISK_TYPE_HARD_DISK, 
                            NDAS_UNIT_DISK_TYPE_DVD_ROM,
                            NDAS_UNIT_DISK_TYPE_DVD_RW */    

#if 1 // to do: move this to seperate structure
    int rwhost;      // Number of read-write hosts that are connected to this unit disk
    int rohost;     // Number of read-only hosts that are connected to this unit 
#endif

    /* Following members are meaningful only in RAID */
    int raid_slot;    /* Not NDAS_INVALID_SLOT if this unit is member of RAID */
    int defective;  /* Set to non 0 value if this disk is defective in RAID */
} ndas_unit_info_t;

/* Per logical disk information */
typedef struct _ndas_slot_info {
    /* TRUE if current connection is enabled in write share mode */    
    xuint8 writeshared:1;
    /* set if write connection is established */
    xuint8 writable:1;
    /* set if enabled TODO refactor */
    xuint8 enabled:1;
    xuint8 reserved:5;
    xuint8 mode;            // NDAS_DISK_MODE_SINGLE, NDAS_DISK_MODE_RAID0, NDAS_DISK_MODE_RAID1 NDAS_DISK_MODE_MEDIAJUKE
    /* 
     * How many chunks of the memory of an I/O request should be split into.
     * 1 : single disk and raid1 
     * 2,4,8 : raid0, raid4
     */
    xuint16 io_splits;

    int mode_role;        // index number

    /* slot number for the NDAS unit device. */
    int slot;

    /* connection timeout */
    sal_msec timeout;
    // 16 bytes

    xuint64 sectors;    // Disk capacity in sector count
    xuint64 capacity;        // Disk Capacity in byte
    // 32 bytes

    /* the maximum possible sectors per a request */
    unsigned int max_sectors_per_request;  /* This value is not limited by HW anymore because of IO splitting. */

    /* raid related parameters */
    /* usually 512 bytes for unit disk. raid has different size */
    unsigned int sector_size; 
    /* number of physical disk of which the slot consists */
    unsigned int disks; 

    int  unit;
    // 48 bytes

    /*
            ndas_serial and unit is provided only as hint for device name choice.
            In RAID, this may be first member's serial.
            To do: change this to slotname. For single device <serial>-<unit>, for raid <first serial>-R0.
    */
    char ndas_serial[NDAS_SERIAL_LENGTH+1];
    // 64 bytes

    /* Disk capacity of each disk's user space. Used only in RAID */
    xuint64 sectors_per_disk; 

} ndas_slot_info_t;

typedef enum _NDAS_DEV_STATUS {
    NDAS_DEV_STATUS_OFFLINE = 0,
    NDAS_DEV_STATUS_HEALTHY,    /* Device is online and working correctly */
    NDAS_DEV_STATUS_UNKNOWN,    /* Without PNP, device status is not known until it is actually connected */
    NDAS_DEV_STATUS_CONNECT_FAILURE,    /* Device is on the network, but failed to connect. */
    NDAS_DEV_STATUS_LOGIN_FAILURE, /* Failed to login. Maybe password problem */
    NDAS_DEV_STATUS_TARGET_LIST_FAILURE, /* Failed to login. Maybe password problem */
    NDAS_DEV_STATUS_IO_FAILURE /* Failed to identify or IO. No disk or error disk */
} NDAS_DEV_STATUS;

/* Description
   This structure contains NDAS device's 
   information.(Not device that attached to NDAS device, aka chip)
 */
typedef struct _ndas_device_info {
    xint16 version;         /* NDAS ASIC version. Currently used values are
                            NDAS_VERSION_1_0, NDAS_VERSION_1_1, NDAS_VERSION_2_0*/
    xint16 nr_unit;        /* Number of devices attached to this NDAS chip */
    NDAS_DEV_STATUS status;
    int slot[NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE]; /* Unique slot numbers assigned to each disks that attached the NDAS chip */
    // 16 bytes

    /* The name assigned by user */
    char name[NDAS_MAX_NAME_LENGTH];
    /* NDAS serial to identify the device */
    char ndas_serial[NDAS_SERIAL_LENGTH + 1];
    /* NDAS dev id ; NDAS id or NDAS serial */
    char ndas_id[NDAS_ID_LENGTH + 1];
    /* NDAS ndas_key */ 
    char ndas_key[NDAS_KEY_LENGTH + 1];

} ndas_device_info;

#endif //_NDASUSER_INFO_H_

