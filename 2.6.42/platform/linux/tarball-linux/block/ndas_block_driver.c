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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#include <linux/version.h>
#include <linux/module.h> 

#include <linux/init.h>
#include <linux/delay.h>

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include <ndas_dev.h>

#include "ndas_cdev.h"
#include "ndas_udev.h"
#include "ndas_regist.h"
#include "ndas_procfs.h"
#include "ndas_bdev.h"


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
MODULE_LICENSE("Dual BSD/GPL");
#endif
MODULE_AUTHOR("Ximeta, Inc.");

#ifndef NDAS_IO_UNIT
#define NDAS_IO_UNIT 64 // KB
#endif

int ndas_io_unit = NDAS_IO_UNIT;
int ndas_max_slot = NDAS_MAX_SLOT;
char* ndas_dev = NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

#include <linux/moduleparam.h>
#define NDAS_MODULE_PARAM_INT(var, perm) module_param(var, int, perm)
#define NDAS_MODULE_PARAM_DESC(var, desc)
#define NDAS_MODULE_PARAM_STRING(var, perm) module_param(var, charp, perm)

#else // 2.4.x or below

#define NDAS_MODULE_PARAM_INT(var, perm) MODULE_PARM(var, "i")
#define NDAS_MODULE_PARAM_DESC(var, desc) MODULE_PARM_DESC(var, desc)
#define NDAS_MODULE_PARAM_STRING(var, perm) MODULE_PARM(var, "s")

#endif

NDAS_MODULE_PARAM_INT( ndas_io_unit, 0 );
NDAS_MODULE_PARAM_DESC( ndas_io_unit, 
						"The maximum size of input/output transfer unit to a NDAS device (default 64)" );

NDAS_MODULE_PARAM_INT( ndas_max_slot, 0 );
NDAS_MODULE_PARAM_DESC( ndas_max_slot, 
						"The maximum number of hard disk can be used by NDAS (default 32)" );

NDAS_MODULE_PARAM_STRING( ndas_dev, 0 );
NDAS_MODULE_PARAM_DESC( ndas_dev, 
						"If set, use only given network interface for NDAS. Set as NULL to use all interfaces." );


int ndas_block_init(void) 
{
    int ret;

	printk( "ndas_block_init\n" );

    udev_set_max_xfer_unit( ndas_io_unit * 1024 );

    ret = registrar_init(ndas_max_slot);

    if (ret) {

        printk( "Failed to initialize NDAS registrator ret = %d\n", ret );
		NDAS_BUG_ON(true);

    	return ret;
    }

    init_ndasproc();

    ret = cdev_init();

    if (ret) {

        printk( "Failed to initialize NDAS control device ret = %d\n", ret );
		NDAS_BUG_ON(true);

    	return ret;
    }

    ret = bdev_init();

    if (ret) {

        printk( "Failed to initialize NDAS block device ret = %d\n", ret );
		NDAS_BUG_ON(true);

    	return ret;
    }

    return ret;
}

void ndas_block_exit(void) 
{
    bdev_cleanup();
    cdev_cleanup();
    cleanup_ndasproc();
    registrar_cleanup();
}

module_init(ndas_block_init);
module_exit(ndas_block_exit);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,0))
#else
EXPORT_NO_SYMBOLS;
#endif


int NDAS_VER_MAJOR=1;
int NDAS_VER_MINOR=1;
int NDAS_VER_BUILD=23;

ndas_error_t ndas_get_version(int* major, int* minor, int* build)
{
/* Temporary: hardcode version number. Version scheme will be changed */
    *major = NDAS_VER_MAJOR;
    *minor = NDAS_VER_MINOR;
    *build = NDAS_VER_BUILD;
    return NDAS_OK;
}

ndas_error_t    
ndas_get_string_error(ndas_error_t code, char* dest, int len)
{
    switch (code) {
    case NDAS_OK: 
        strncpy(dest,"NDAS_OK",len); break;
    case NDAS_ERROR: 
        strncpy(dest,"NDAS_ERROR",len); break;
    case NDAS_ERROR_DRIVER_NOT_LOADED: 
        strncpy(dest,"DRIVER_NOT_LOADED",len); break;
    case NDAS_ERROR_DRIVER_ALREADY_LOADED: 
        strncpy(dest,"DRIVER_ALREADY_LOADED",len); break;
    case NDAS_ERROR_LIBARARY_NOT_INITIALIZED: 
        strncpy(dest,"LIBARARY_NOT_INITIALIZED",len); break;
    case NDAS_ERROR_INVALID_NDAS_ID: 
        strncpy(dest,"INVALID_NDAS_ID",len); break;
    case NDAS_ERROR_INVALID_NDAS_KEY: 
        strncpy(dest,"INVALID_NDAS_KEY",len); break;
    case NDAS_ERROR_NOT_IMPLEMENTED: 
        strncpy(dest,"NOT_IMPLEMENTED",len); break;
    case NDAS_ERROR_INVALID_PARAMETER: 
        strncpy(dest,"INVALID_PARAMETER",len); break;
    case NDAS_ERROR_INVALID_SLOT_NUMBER: 
        strncpy(dest,"INVALID_SLOT_NUMBER",len); break;
    case NDAS_ERROR_INVALID_NAME: 
        strncpy(dest,"invalid name",len); break;
    case NDAS_ERROR_NO_DEVICE: 
        strncpy(dest,"NO_DEVICE",len); break;
    case NDAS_ERROR_ALREADY_REGISTERED_DEVICE: 
        strncpy(dest,"ALREADY_REGISTERED_DEVICE",len); break;
    case NDAS_ERROR_ALREADY_ENABLED_DEVICE:
        strncpy(dest,"ALREADY_ENABLED_DEVICE",len); break;
    case NDAS_ERROR_ALREADY_REGISTERED_NAME:
        strncpy(dest,"ALREADY_REGISTERED_NAME",len); break;
    case NDAS_ERROR_ALREADY_DISABLED_DEVICE:
        strncpy(dest,"ALREADY_DISABLED_DEVICE",len); break;
    case NDAS_ERROR_ALREADY_STARTED:
        strncpy(dest,"ALREADY_STARTED",len); break;
    case NDAS_ERROR_ALREADY_STOPPED:
        strncpy(dest,"ALREADY_STOPPED",len); break;
    case NDAS_ERROR_NOT_ONLINE    :
        strncpy(dest,"the NDAS device is not online",len); break;
    case NDAS_ERROR_NOT_CONNECTED: 
        strncpy(dest,"the NDAS device is not connected",len); break;
    case NDAS_ERROR_INVALID_HANDLE: 
        strncpy(dest,"INVALID_HANDLE",len); break;
    case NDAS_ERROR_NO_WRITE_ACCESS_RIGHT: 
        strncpy(dest,"no access right to write the data",len); break;
    case NDAS_ERROR_WRITE_BUSY: 
        strncpy(dest,"WRITE_BUSY",len); break;        
    case NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION: 
        strncpy(dest,"UNSUPPORTED_HARDWARE_VERSION",len); break;
    case NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION: 
        strncpy(dest,"UNSUPPORTED_SOFTWARE_VERSION",len); break;
    case NDAS_ERROR_UNSUPPORTED_DISK_MODE: 
        strncpy(dest,"UNSUPPORTED_DISK_MODE",len); break;
    case NDAS_ERROR_UNSUPPORTED_FEATURE: 
        strncpy(dest,"UNSUPPORTED_FEATURE",len); break;
    case NDAS_ERROR_BUFFER_OVERFLOW: 
        strncpy(dest,"BUFFER_OVERFLOW",len); break;
    //case NDAS_ERROR_NO_NETWORK_INTERFACE: 
    //    strncpy(dest,"NO_NETWORK_INTERFACE",len); break;
    case NDAS_ERROR_INVALID_OPERATION: 
        strncpy(dest,"INVALID_OPERATION",len); break;
    //case NDAS_ERROR_NETWORK_DOWN: 
    //    strncpy(dest,"NETWORK_DOWN",len); break;
    //case NDAS_ERROR_MEDIA_CHANGED: 
    //    strncpy(dest,"MEDIA_CHANGED",len); break;
    case NDAS_ERROR_TIME_OUT: 
        strncpy(dest,"Timed out",len); break;
    case NDAS_ERROR_READONLY: 
        strncpy(dest,"read-only",len); break;
    case NDAS_ERROR_OUT_OF_MEMORY: 
        strncpy(dest,"out of memory",len); break;
    //case NDAS_ERROR_EXIST: 
    //   strncpy(dest,"EXIST",len); break;
    case NDAS_ERROR_SHUTDOWN: 
        strncpy(dest,"SHUTDOWN",len); break;
   // case NDAS_ERROR_PROTO_REGISTRATION_FAIL:         
   //     strncpy(dest,"PROTO_REGISTRATION_FAIL",len); break;
    case NDAS_ERROR_SHUTDOWN_IN_PROGRESS:
        strncpy(dest,"Shutdown is in progress", len); break;        
   //case NDAS_ERROR_ADDRESS_NOT_AVAIABLE: 
   //     strncpy(dest,"ADDRESS_NOT_AVAIABLE",len); break;
    //case NDAS_ERROR_NOT_BOUND: 
    //    strncpy(dest,"NOT_BOUND",len); break;
    case NDAS_ERROR_NETWORK_FAIL:
        strncpy(dest,"NETWORK_FAIL",len); break;
    case NDAS_ERROR_HDD_DMA2_NOT_SUPPORTED:
        strncpy(dest,"Hard Disk Device does not support DMA 2 mode",len); break;
    case NDAS_ERROR_IDE_REMOTE_INITIATOR_NOT_EXIST:
        strncpy(dest,"Remote Initiator not exists",len); break;
    case NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND:
        strncpy(dest,"Remote Initiator bad command",len); break;
    case NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED:
        strncpy(dest,"Remote Initiator command failed",len); break;
    case NDAS_ERROR_IDE_REMOTE_AUTH_FAILED:
        strncpy(dest,"Remote Authorization failed",len); break;
    case NDAS_ERROR_IDE_TARGET_NOT_EXIST:
        strncpy(dest,"Target not exists",len); break;
    case NDAS_ERROR_HARDWARE_DEFECT:
        strncpy(dest,"Hardware defect",len); break;
    case NDAS_ERROR_BAD_SECTOR:
        strncpy(dest,"Bad sector",len); break;
    case NDAS_ERROR_IDE_TARGET_BROKEN_DATA:
        strncpy(dest,"Target broken data",len); break;
    case NDAS_ERROR_IDE_VENDOR_SPECIFIC:    
        strncpy(dest,"IDE vendor specific error",len); break;
    case NDAS_ERROR_INTERNAL:            
        strncpy(dest,"The error is caused by the internal framework bug",len); break;
    //case NDAS_ERROR_MAX_USER_ERR_NUM: 
    //    strncpy(dest,"MAX_USER_ERR_NUM",len); break;
    case NDAS_ERROR_INVALID_RANGE_REQUEST:
        strncpy(dest,"Invalid range of request",len); break;
    //case NDAS_ERROR_INVALID_METADATA:
    //    strncpy(dest,"Invalid metadata", len); break;
    case NDAS_ERROR_CONNECT_FAILED:
        strncpy(dest,"Failed to connect", len); break;
    default: 
        snprintf(dest,len,"UNKNOWN CODE(%d)",code); break;
    }
    return NDAS_OK;
}
