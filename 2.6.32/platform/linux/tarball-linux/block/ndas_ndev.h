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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#ifndef _NDAS_NDEV_H_
#define _NDAS_NDEV_H_

#include <ndas_lpx.h>
#include "ndas/def.h"

#include "ndas_udev.h"

#ifdef NDAS_CRYPTO
#define PNP_ALIVE_TIMEOUT 		msecs_to_jiffies(5*MSEC_PER_SEC)	// 5 second
#else
#define PNP_ALIVE_TIMEOUT 		msecs_to_jiffies(30*MSEC_PER_SEC)	// 30 second
#endif

typedef enum _NDAS_DEV_STATUS {

    NDAS_DEV_STATUS_OFFLINE = 0,
    NDAS_DEV_STATUS_HEALTHY,    		 // Device is online and working correctly
    NDAS_DEV_STATUS_UNKNOWN,    		 // Without PNP, device status is not known until it is actually connected
    NDAS_DEV_STATUS_CONNECT_FAILURE,     // Device is on the network, but failed to connect
    NDAS_DEV_STATUS_LOGIN_FAILURE, 		 // Failed to login. Maybe password problem
    NDAS_DEV_STATUS_TARGET_LIST_FAILURE, // Failed to login. Maybe password problem
    NDAS_DEV_STATUS_IO_FAILURE 			 // Failed to identify or IO. No disk or error disk

} NDAS_DEV_STATUS;

typedef struct _ndas_device_info {

    __s16 version;			// NDAS ASIC version. Currently used values are
                           	// NDAS_VERSION_1_0, NDAS_VERSION_1_1, NDAS_VERSION_2_0

    __s16 nr_unit;        	// Number of devices attached to this NDAS chip

    NDAS_DEV_STATUS status;

    __u8 slot[NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE]; 	// Unique slot numbers assigned to each disks that attached the NDAS chip
    __u8 mode[NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE];  // NDAS_DISK_MODE_****

    char name[NDAS_MAX_NAME_LENGTH];				// The name assigned by user

    char ndas_sid[NDAS_SERIAL_LENGTH + 1]; 			// NDAS ndas_sid to identify the device

	char ndas_id[NDAS_ID_LENGTH + 1];				// NDAS ndas id
	char ndas_key[NDAS_KEY_LENGTH + 1];				// NDAS ndas_key 

	char ndas_default_id[NDAS_ID_LENGTH + 1];		// NDAS canonical ndas_id Used only by ndas_regists.c

} ndas_device_info;

/* 
 * Per NDAS device information.
 */
typedef struct ndev_s {

    struct semaphore ndev_mutex;
    ndas_device_info info;

    __u16 enabled_bitmap;
    __u8  network_id[LPX_NODE_LEN];
 
    struct udev_s *udev[NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE];

	unsigned long 	online_jiffies;

} ndev_t;


#define NDEV_IS_REGISTERED(ndev) ((ndev)->info.name[0] != '\0')

/* 
 * Regiser the ndas device with given name, ndas id, and the ndas key.
 * You can specify if the unit device should be enabled.
 */

ndev_t* ndev_create(void);
void ndev_cleanup(ndev_t *ndev);

ndas_error_t ndev_register_device(const char* name, const char *ndas_id_or_serial, const char *ndas_key,
								  __u16 enabled_bitmap, bool use_serial, __u8 *slot, unsigned char slot_num);

ndas_error_t ndev_unregister_device(const char *name);

void ndev_pnp_changed(const char *ndas_id, const char *name, NDAS_DEV_STATUS status);

ndas_error_t ndev_detect_device(const char* name);
ndas_error_t ndev_get_ndas_dev_info(const char* name, ndas_device_info* info);
ndas_error_t ndev_set_registration_name(const char* oldname, const char *newname);
ndas_error_t ndev_request_permission(int slot);

/* 
    To do: clean debugs
*/
#define ENABLE_FLAG(writable, writeshare, raid, pnp) \
    (((writable) ? 0x1 : 0) | ((writeshare) ? 0x2 : 0) | ((raid) ? 0x4 : 0) | ((pnp) ? 0x8 : 0))
#define ENABLE_FLAG_WRITABLE(flag) (((flag)&0x1)? TRUE:FALSE)
#define ENABLE_FLAG_WRITESHARE(flag) (((flag)&0x2)? TRUE:FALSE)
#define ENABLE_FLAG_RAID(flag) (((flag)&0x4)? TRUE:FALSE)
#define ENABLE_FLAG_PNP(flag) (((flag)&0x8)? TRUE:FALSE)
#define ENABLE_FLAG_DELAYED(flag) (((flag)&0x10)? TRUE:FALSE)

#endif /* _NDAS_NDEV_H_ */
