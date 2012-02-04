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
#ifndef _NDASUSER_H__INCLUDED_
#define _NDASUSER_H__INCLUDED_

#include "ndasuser/def.h"
#include "ndasuser/info.h"
#include "ndasuser/ndaserr.h"
#include "ndasuser/io.h"
#include "sal/time.h"  /* sal_msec */

/* Description
 *    Initialize NDAS user libarary.
 *    Should be called by application that uses NDAS user library. 
 *    Called by each process that uses NDAS user libary.
 *
 * Parameters
 * io_unit: transfer unit for one I/O request to the NDAS device in kbytes. 64 is used if 0 is given.
 * sock_buffer_size: hint for socket buffer allocation.
 * nr_dpc: size of pre-allocated delay-procedure-call. 150 is default value (when non-positive value given)
 * max_slot: the maximum number of the hard disk, the NDAS system can maintain.
 */    
NDASUSER_API ndas_error_t ndas_init(int io_unit, int sock_buffer_size, int reserved, int max_slot);

/* Description
 *  Start NDAS services like detecting the NDAS device on network
 *   or HIX feature.
 *  This funtion must be called after ndas_init is called. 
 */

NDASUSER_API ndas_error_t ndas_start(void);

/* Description
 *  Stop NDAS services
 * This function must be called before calling ndas_cleanup
 */
NDASUSER_API ndas_error_t ndas_stop(void);

/* Description
 * Restart NDAS services like detecting the NDAS device on network
 *   or HIX feature.
 *  This funtion must be called after a network interface is restarted.
 */

NDASUSER_API ndas_error_t ndas_restart(void);

/*
 * Description
 *    Clean up NDAS user library
 */    
NDASUSER_API ndas_error_t ndas_cleanup(void);

/**
 * Description
 * Register an NDAS device (ie. Ximeta NetDisk, Ximeta NetDisk Office) with the given name.
 * the name can be changed by ndas_set_registration_name.
 * the name should be maintained by the GUI program so that it can be un-registered later.
 * 
 * Parameters
 * name: The name to identify the NDAS device in the GUI program.
 * id: The NDAS id or NDAS serial id that is the unique id to identify the NDAS product.
 * ndas_key: The NDAS key to acquire the write permission. 
 *     set NULL if you want to access the device with read-only permission.
 * 
 * 
 * 
 */
NDASUSER_API 
ndas_error_t 
ndas_register_device(const char* name, const char *id, const char *ndas_key);

/**
 * Description
 * Register an NDAS device with the given name and serial.
 * Basically device enabled with this option can access device only in read-only mode.
 * 
 * Parameters
 * name: The name to identify the NDAS device in the GUI program.
 * serial: 8 or 16 bytes string. Can be obtained through probe function.
 */
NDASUSER_API 
ndas_error_t 
ndas_register_device_by_serial(const char* name, const char *serial);


/**
 * Description
 * Unregister NDAS device (ie. Ximeta NetDisk, Ximeta NetDisk Office) by the registered name
 * 
 * Parameters
 * name: The registered name set by ndas_register_device or ndas_set_registration_name
 */
 
NDASUSER_API ndas_error_t ndas_unregister_device(const char* name);

typedef NDAS_CALL ndas_error_t (*ndas_permission_request_handler) (int slot);

/** 
 * Description
 * Enable the NDAS unit device(ie. Hard Disk, DVD) identified by slot number
 *  with read-only mode.
 * 
 * When disk is disconnected, disk is in "disconnected" state
 * and disconnect-handler is called, if user has registered the
 * handler. You can reconnect to disk by calling
 * ndas_enable_slot again.
 * 
 * Parameters
 * slot:            NDAS unit device slot number, allocated when ndas_register_device is called.
                     can be obtainbed by ndas_query_slot, 
 *
 * Returns negative if an error occurs.
 *         NDAS_OK if succussfully enabled
 *         Positive if the disk is a member of the NDAS raid. the value is the slot number for the raid
 * 
 * Examples
 * ndas_device_info dev;
 * ndas_register_device(name, id, key);
 * ndas_get_ndas_dev_info(name, &dev);
 * for (int i = 0; i < dev.nr_unit;i++) 
 *     ndas_enable_slot(dev.unit[i]->slot);
 *
 */
NDASUSER_API ndas_error_t ndas_enable_slot(int slot);

/** 
 * Description
 * Enable the NDAS unit device(ie. Hard Disk, DVD) identified by slot number
 *  with read-write mode.
 * The slot will be only writable from this host once this function is called.
 *
 * When disk is disconnected, disk is in "disconnected" state
 * and disconnect-handler is called, if user has registered the
 * handler. You can reconnect to disk by calling
 * ndas_enable_slot again.
 * 
 * Parameters
 * slot:            NDAS unit device slot number, allocated when ndas_register_device is called.
                     can be obtainbed by ndas_query_slot, 
 *
 * Returns negative if an error occurs.
 *         NDAS_OK if succussfully enabled
 *         Positive if the disk is a member of the raid. the value is the slot number for the raid
 * 
 * Examples
 * ndas_device_info dev;
 * ndas_register_device(name, id,key);
 * ndas_detect_device(name);
 * ndas_get_ndas_dev_info(name, &dev);
 * for (int i = 0; i < dev.nr_unit;i++) 
 *     ndas_enable_exclusive_writable(dev.unit[i]->slot);
 *
 */
NDASUSER_API ndas_error_t ndas_enable_exclusive_writable(int slot);

/** 
 * Description
 * Write-sharing version of ndas_enable_slot.
 * 
 * Write-sharing is only available some version of NDAS devices.
 * In write-share mode, writing to NDAS devices requires coordination with other hosts
 *               so all NDAS device needs to connect in write-share mode.
 * This mode can be used for the symetric shared file system such a GFS, OCFS, OCFS2 and so on.
 * 
 * Parameters
 * slot:            NDAS unit device slot number, allocated when ndas_register_device is called.
                      can be obtainbed by ndas_query_slot, 
 *
 */
NDASUSER_API ndas_error_t ndas_enable_writeshare(int slot);


/* Description
 * Send the request to gain the exclusive write permission.
 * This request will be sent to the NDAS device which has the current exclusive request.
* Request cannot be guaranteed to be accepted. 
 */
NDASUSER_API ndas_error_t   ndas_request_permission(int slot);

/** 
 * Description
 * Request to disable the NDAS unit device enabled by ndas_enable_slot function.
 * The result will be signalled by the disconnected handler
 * 
 * Parameters
 * slot:            NDAS device slot number, allocated when ndas_register_device is called
 */
NDASUSER_API ndas_error_t ndas_disable_slot(int slot);

/**
 * Description
 * Lookup the device on the network.
 * This function update internal status of NDAS device. 
 * If PNP is enabled, this function will be called internally but if not, user need to call this function manually.
 * 
 * Parameters
 * name: The registered name to identify NDAS device. set by ndas_register_device, ndas_set_registration_name
 * info : ndas_device_info to get.
 *
 * Returns
 * NDAS_OK: if success. 
 * NDAS_ERROR_INVALID_NAME: No NDAS device is registered with 'name'
 * NDAS_ERROR: If failed to discover in some reason, such as non-on network. Use ndas_get_ndas_dev_info for more information.
 * To do: add more error code that can be returned during login process
 */
NDASUSER_API ndas_error_t ndas_detect_device(const char* name);


/**
 * Description
 * Get the registration data(NDAS ID, NDAS key, NDAS device name) of registered NDAS device.
 * You can get slot number assigned to the device. 
 * 
 * Limitations
 * Currenly the driver supports one disk per NDAS device. 
 * version, nr_unit and status in ndas_device_info may not updated in some driver build configuration. 
 * 
 * Parameters
 * name: The registered name to identify NDAS device. set by ndas_register_device, ndas_set_registration_name
 * info : ndas_device_info to get.
 *
 * Returns
 * NDAS_OK: if success. 
 * NDAS_ERROR_INVALID_NAME: No NDAS device is registered with 'name'
 * Example
 * 
 * <CODE>    
 *     ndas_error_t status;
 *     ndas_device_info info;
 *     status = ndas_get_ndas_dev_info(name, &info);
 *     printf("NDAS device name:%s\\n", info.name);
 *     printf("NDAS ID:%s\\n", info.id);
 * </CODE>                                                    
 */
NDASUSER_API ndas_error_t ndas_get_ndas_dev_info(const char* name, ndas_device_info* info);

/**
 * Description
 * Set encryption option of NDAS device. If encryption is disabled, the CPU load is reduced
 * This function should be called before enabling.
 * 
 * Parameters
 * slot : slot for the NDAS hard disk
 * headenc : true if the header information should be encrypted
 * dataenc : true if the data should be encrypted
 */
NDASUSER_API ndas_error_t ndas_set_encryption_mode(int slot,  xbool headenc, xbool dataenc);


/**
 * Description
 * Set the NDAS device name of registered NDAS device.
 * 
 * Parameters
 * oldname : the old name of the registered NDAS device. set by ndas_register_device, ndas_set_registration_name
 * newname : the new name of the NDAS device.
 * 
 * Returns
 * NDAS_OK: if success. 
 * NDAS_ERROR_INVALID_NAME: No NDAS device is registered with 'oldname'
 * 
 * Example
 * 
 * <CODE>
 *     int slot; // slot number obtained by ndas_register_device 
 *     ndas_error_t status;
 *     status = ndas_set_registration_name("old name","New disk name");
 * </CODE>                                                        
 */
NDASUSER_API ndas_error_t ndas_set_registration_name(const char* oldname, const char *newname);

/** 
 * Description
 * ndas_query_slot - get information about the slot.
 * The information is obtained when the device can be connected. 
 * info.sector_count is this slot's total sector count and 
 *     info.max_sectors_per_request is the maximum request size that caller can request  to this slot at one time.
 * Slot is a logical view of the NDAS device. Multiple physical disks can be a single slot.
 * 
 * Parameters 
 * slot :  Slot number allocated when ndas_register_device is called
 * info :  Pointer to ndas_device_info to get information. 
 *
 * Returns
 * NDAS_OK: successfully obtained the information
 * NDAS_ERROR_INVALID_SLOT_NUMBER: the slot number is invalid.
 */
NDASUSER_API ndas_error_t ndas_query_slot(int slot, ndas_slot_info_t* info);

NDASUSER_API ndas_error_t 
    ndas_query_unit(int slot, ndas_unit_info_t* info);
    
struct ndas_registered {
    char name[NDAS_MAX_NAME_LENGTH];
};

/* Description
 * Get the number of the registered NDAS device.
 * 
 * Returns:
 * Non-negative: the number of the registered NDAS device.
 * NDAS_ERROR_NOT_IMPLEMENTED: if feature is not support in the system
 */
NDASUSER_API
ndas_error_t
ndas_registered_size(void);

/* Description
 * Get the list of the registered NDAS device.
 * 
 * Parameters
 * data: the first item of array to read the data
 * size: the number of item in the array.
 *
 * Returns
 * Non-negative: the number of data stored into the array.
 * NDAS_ERROR_NOT_IMPLEMENTED: if feature is not support in the system
 */
NDASUSER_API 
ndas_error_t 
ndas_registered_list(struct ndas_registered *data, int size);

struct ndas_probed {
    char serial[NDAS_SERIAL_LENGTH+1];
    NDAS_DEV_STATUS status;
};

/* Description
 * Get the number of the NDAS devices in the network.
 * 
 * Returns:
 * Non-negative: the number of the NDAS devices in the network.
 * NDAS_ERROR_NOT_IMPLEMENTED: if feature is not support in the system
 */
NDASUSER_API 
ndas_error_t 
ndas_probed_size(void);

/**
 * Get the list of the NDAS devices in the network.
 * 
 * Parameters
 * data: the first item of array to read the data
 * size: the number of item in the array.
 *
 * Returns
 * Non-negative: the number of data stored into the array.
 * NDAS_ERROR_NOT_IMPLEMENTED: if feature is not support in the system
 */
NDASUSER_API 
ndas_error_t 
ndas_probed_list(struct ndas_probed* data, int devicen);

/**
 * Description
 * Function that is called when online status of netdisk is changed. This functions should not block.
 * Parameters
 *  network_id: Serial number of NDAS device. Serial number is 8 or 16 character long string of numbers.
 *  name: Registered name of device. Null if not registered.
 *  status: Online status.
 */
typedef NDAS_CALL void (*ndas_device_handler)(const char* serial, const char* name, NDAS_DEV_STATUS status);

/**
 * Description
 *
 * Set handler to be called when NDAS device's online status is changed. 
 * This function is available only when PNP is enabled.
 *
 * Parameters  
 * func: handler function
 */
NDASUSER_API void ndas_set_device_change_handler(ndas_device_handler func);

/**
 * Description
 *
 * Set handler to be called when connection to the enabled
 * the connection of the device identified by the slot is lost. 
 * In disconnecthandler, application can notify the
 * user of the event, or unmount a file system and try to
 * reconnect.
 *
 * Parameters  
 * slot: the slot of the device
 * enabled_func: handler function to be called when the slot is enabled.
 * disabled_func: handler function to be called when the slot is disabled/disconnected.
 * handwrite_permission_request_handlerler:       handler function for read-write enabled mode.
 *                This function will be called when peer host requested for 
 *                  the write permission (which is exclusive). 
 *                According to your application, exit current connection and reconnect as read-only mode
 *                           or just ignore the message.
 * user_arg: user argments
 */
NDASUSER_API ndas_error_t ndas_set_slot_handlers(int slot, 
    ndas_io_done enabled_func, 
    ndas_io_done disabled_func, 
    ndas_permission_request_handler write_permission_request_handler,
    void* user_arg);

/**
 * Description
 * Register the network interface. The NDAS driver will talk to registered interface only.
 * 
 * Parameter
 * devname: the name of network interface
 * 
 * Returns
 * NDAS_OK: The interface registered sucessfully.
 */
NDASUSER_API ndas_error_t ndas_register_network_interface(const char* devname);
/**
 * Description
 * Un-register the network interface
 * 
 * Parameter
 * devname: the name of network interface
 */
NDASUSER_API ndas_error_t ndas_unregister_network_interface(const char* devname);

/* Description
   Return string describing NDAS error code.
   
   Parameters
   code :  NDAS error code.
   buf : Buffer to hold the error message.
   buflen : Length of buf

   Returns
   NDAS_OK: buf is filled with valid error message.
   
   NDAS_ERROR_INVALID_PARAMETER: Unknown error code
   
   NDAS_ERROR_BUFFER_OVERFLOW : buflen is not enough to hold the error message
                                                                 */
NDASUSER_API ndas_error_t    ndas_get_string_error(ndas_error_t code, char* buf, int buflen); 

/* Description
   Schedule a function to be executed in 'delay' mili-sec.
   The function will be called from the 'NDAS thread' to gurantee the MT-safe-ness
   of calling NDAS APIs.
   Note that the func should have NDAS_CALL modifier. Otherwise,
   the call might carry the wrong parameters.
 */
NDASUSER_API ndas_error_t   ndas_queue_task(sal_tick delay,NDAS_CALL void (*func)(void*), void* arg);

#define NDAS_GET_STRING_ERROR(code) ({\
        static char buf[1024]; \
        ndas_get_string_error(code,buf,1024); \
        (const char*) buf; \
    })

typedef enum _NDAS_LOCK_OP {
    NDAS_LOCK_TAKE = 1,    // Take device lock
    NDAS_LOCK_GIVE,        // Give device lock
    NDAS_LOCK_GET_COUNT,    // Get the number that the lock is freed. Used for statistics
    NDAS_LOCK_GET_OWNER,    // Get the current lock owner or last owner.
    NDAS_LOCK_BREAK,    // If lock is taken by other, try to break the lock and take it.     
#if 0
    NDAS_LOCK_BREAK_TAKE,// If lock is taken by other, try to break the lock and take it. 
                            //This can take a several second and  may fail in some cases
#endif
} NDAS_LOCK_OP;

/*     
    Description
    Access NDAS device locks. Only 1.1 and 2.0 chip supports lock and those chip has locks numbered as 0,1,2 and 3.
    Lock is only used for shared-write function and lock 0 is reserved for internal use.
    
    Return value depends on 'op'. 
        NDAS_OK if operation is successfully operated device and requested operation is 
        NDAS_ERROR if NDAS device got the request but cannot accept the request such as other host has the lock.
        NDAS_ERROR_INVALID_SLOT_NUMBER             : slot is invalid
        NDAS_ERROR_NETWORK_FAIL                    : Access to NDAS device is failed.
        NDAS_ERROR_UNSUPPORTED_FEATURE            : HW does not support device lock.
        NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION    : This software does not support HW's lock mechanism.
    Opt depends on 'op.
        NDAS_LOCK_TAKE, NDAS_LOCK_GIVE,NDAS_LOCK_BREAK_TAKE does not require opt.
        NDAS_LOCK_GET_COUNT requires 'opt' to be pointer to 32 bit integer and this function will set it to the count of lock unlock.
        NDAS_LOCK_GET_OWNER requires 'opt' to be pointer to 6 bytes data and this function will set it to the lock owner or last owner. 
*/
NDASUSER_API ndas_error_t ndas_lock_operation(int slot, NDAS_LOCK_OP op, int lockid, void* lockdata, void* opt);

/*
    Get NDAS ID/key from device by serial number. 
    This function is available only for internal application.
    serial: input. 8 or 16 byte serial number string.
    id: output. Should be buffer at least NDAS_ID_LENGTH+1 bytes.
    key: output. Should be buffer at least NDAS_KEY_LENGTH+1 bytes.
    Return value: 
        NDAS_OK: When succeeded.
        NDAS_ERROR_UNSUPPORTED_FEATURE: This function is not supported on provided NDAS libary.
        NDAS_ERROR_INVALID_PARAMETER
*/
NDASUSER_API ndas_error_t ndas_query_ndas_id_by_serial(const char* serial, char* id, char* key);

/* 
    Get version number string.
*/
NDASUSER_API ndas_error_t ndas_get_version(int* major, int* minor, int *build);


/*     
    Description
	Start NDAS emulator.
	version: NDAS version to emulate.
	max_transfer_size is in kbyte unit.
*/
NDASUSER_API ndas_error_t ndas_emu_start(char* devfile, int version, int max_transfer_size);
/*
    Description
	Stop NDAS emulator.
*/
NDASUSER_API ndas_error_t ndas_emu_stop(void);



#endif /* _NDASUSER_H__INCLUDED_ */
