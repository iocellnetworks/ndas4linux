#include "xplatcfg.h"
#include <sal/libc.h>
#include <sal/debug.h>
#include "xlib/dpc.h"
#include "lpx/lpxutil.h"
#include "netdisk/netdisk.h"
#include <ndasuser/ndasuser.h>
#include <ndasuser/persist.h>
#include "netdisk/serial.h"
#include "netdisk/netdiskid.h"
#include "ndiod.h"
#include "registrar.h"
#include "ndiod.h"
#include "ndpnp.h"
#include "nhix.h"
#include "registrar.h"
#include "udev.h"

#ifdef DEBUG
#define debug_ndasuser(l,x...) do { \
    if ( l <= DEBUG_LEVEL_LDASUSER ) \
        sal_debug_print("NU|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x);\
} while(0) 
#else
#define debug_ndasuser(l,fmt...) 
#endif

#define    XLIB_DPC_COUNT        150
#define DEFAULT_XBUF_NUM  64
#define DEFAULT_MAX_SLOT 16

NDASUSER_API ndas_error_t ndas_get_version(int* major, int* minor, int* build)
{
/* Temporary: hardcode version number. Version scheme will be changed */
    *major = NDAS_VER_MAJOR;
    *minor = NDAS_VER_MINOR;
    *build = NDAS_VER_BUILD;
    return NDAS_OK;
}

NDASUSER_API ndas_error_t 
ndas_init(
    int io_unit,  // KB
    int sock_buffer_size, // KB
    int reserved,
    int max_slot) 
{
    ndas_error_t ret;
    if ( io_unit <= 0 ) io_unit = DEFAULT_XBUF_NUM;
    if ( sock_buffer_size <= 0 ) sock_buffer_size = DEFAULT_XBUF_NUM;
    if ( max_slot <= 0) max_slot = DEFAULT_MAX_SLOT;

    sal_error_print("\nndas: Initializing NDAS driver version %d.%d.%d\n", 
        NDAS_VER_MAJOR, NDAS_VER_MINOR, NDAS_VER_BUILD);
    ret = dpc_start();
#ifdef XPLAT_BPC
    ret = bpc_start();
#endif
    ret = lpx_init(sock_buffer_size);
    if ( !NDAS_SUCCESS(ret) ) {
        goto out1;
    }
    sal_error_print("ndas: Setting max request size to %dkbytes\n", io_unit);
#ifndef NDAS_NO_LANSCSI
    ret = ndas_block_core_init();
    if ( !NDAS_SUCCESS(ret) ) {
        goto out1;
    }
    udev_set_max_xfer_unit(io_unit*1024);
    ret = registrar_init(max_slot);
#endif
    return NDAS_OK;
out1:
    dpc_stop();
    return ret;
}

NDASUSER_API ndas_error_t 
ndas_cleanup(void) 
{
    debug_ndasuser(1, "ing");
#ifndef NDAS_NO_LANSCSI
    ndas_block_core_cleanup();
    registrar_cleanup();
#endif    
    lpx_cleanup();
#ifdef XPLAT_BPC
    bpc_stop();
#endif

    dpc_stop();
    debug_ndasuser(1, "ed");
    return NDAS_OK;
}

/**
 * internal static functions 
 */
NDASUSER_API ndas_error_t 
ndas_register_network_interface(const char* devname)
{
    return lpx_register_dev(devname);
}

NDASUSER_API ndas_error_t 
ndas_unregister_network_interface(const char* devname) 
{
    return lpx_unregister_dev(devname);
}

static int v_dpc_func(void* a, void* b) {
    ((NDAS_CALL void(*)(void*)) a) (b);
    return 0;
}

NDASUSER_API ndas_error_t 
ndas_queue_task(sal_tick delay, NDAS_CALL void (*func)(void*), void* arg) 
{
    int ret;
    dpc_id id;
    id = dpc_create(DPC_PRIO_NORMAL, v_dpc_func,  func, arg, NULL, 0);
    if (id) {
        ret = dpc_queue(id, delay);
        if(ret < 0) {
            dpc_destroy(id);
        }
    }
    return id == DPC_INVALID_ID ? NDAS_ERROR_OUT_OF_MEMORY : NDAS_OK;
}

#ifndef NDAS_NO_LANSCSI
static int started;

NDASUSER_API ndas_error_t 
ndas_start() 
{
    ndas_error_t err;
    if ( started ) { /* no consideration for race condition */
        return NDAS_ERROR_ALREADY_STARTED;
    }
    err = ndpnp_init();
    if ( !NDAS_SUCCESS(err) )
        return err;
    err = nhix_init();
    if ( !NDAS_SUCCESS(err) ) 
    {
        ndpnp_cleanup();
        return err;
    }
    started = 1;
    return NDAS_OK;
}    

NDASUSER_API ndas_error_t 
ndas_restart() 
{
    ndas_error_t err;
    if ( !started ) { /* no consideration for race condition */
        debug_ndasuser(1, "ndas not stared yet");
        return NDAS_ERROR_ALREADY_STOPPED;
    }
    err = ndpnp_reinit();
    if ( !NDAS_SUCCESS(err) ) {
        nhix_cleanup();
        started = 0;
        debug_ndasuser(1, "Failed to ndpnp_reinit");
        return err;
    }
    err = nhix_reinit();
    if ( !NDAS_SUCCESS(err) ) 
    {
        ndpnp_cleanup();
        started = 0;
        debug_ndasuser(1, "Failed to nhix_reinit");
        return err;
    }
    started = 1;
    return NDAS_OK;
}   

NDASUSER_API ndas_error_t 
ndas_stop() 
{
    if ( !started ) { /* no consideration for race condition */
        debug_ndasuser(1, "Already stopped");
        return NDAS_ERROR_ALREADY_STOPPED;
    }
    nhix_cleanup();
    ndpnp_cleanup();
    
#ifndef NDAS_NO_LANSCSI
    registrar_stop();
#endif
    started = 0;
    return NDAS_OK;
}

#ifdef NDAS_PROBE
NDASUSER_API 
ndas_error_t 
ndas_probed_size() {
    return ndev_probe_size();
}

NDASUSER_API 
ndas_error_t 
ndas_probed_list(struct ndas_probed* data, int devicen)
{
    return ndev_probe_serial(data, devicen);
}
#else
NDASUSER_API 
ndas_error_t 
ndas_probed_size() {
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
}

NDASUSER_API 
ndas_error_t 
ndas_probed_list(struct ndas_probed* data, int devicen)
{
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
}

#endif

NDASUSER_API ndas_error_t 
ndas_get_registration_data(char** data, int* size) 
{
#ifdef XPLAT_RESTORE
    return ndev_get_registration_data(data,size);
#else
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
#endif
}

NDASUSER_API ndas_error_t 
ndas_set_registration_data(char *data, int size) 
{
#ifdef XPLAT_RESTORE
    return ndev_set_registration_data(data, size);
#else
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
#endif
}
NDASUSER_API
ndas_error_t
ndas_registered_size()
{
#ifndef NDAS_NO_LANSCSI
    return ndev_registered_size();
#else
	return 0;
#endif
}

NDASUSER_API
ndas_error_t ndas_registered_list(struct ndas_registered *data, int size) 
{
#ifndef NDAS_NO_LANSCSI
    return ndev_registered_list(data, size);
#else
	return 0;
#endif
}


NDASUSER_API 
ndas_error_t ndas_query_ndas_id_by_serial(const char* serial, char* id, char* key)
{
    ndas_id_info nid;
    ndas_error_t  ret;    
    if (!serial || !id || !key) {
        return NDAS_ERROR_INVALID_PARAMETER;
    }

    ret = get_networkid_from_serial(nid.ndas_network_id, serial);
    if (ret !=NDAS_OK) {
        return NDAS_ERROR_INVALID_PARAMETER;
    }
    
    sal_memcpy(nid.key1, NDIDV1Key1, 8);
    sal_memcpy(nid.key2, NDIDV1Key2, 8);    
    sal_memcpy(nid.reserved, NDIDV1Rsv, 2);    
    /* To do: For seagate HW this should be detected from device */
#ifdef NDAS_CRYPTO
    nid.vid = NDIDV1VID_NETCAM; 	
#else
    nid.vid = NDIDV1VID_DEFAULT; 
#endif
    nid.random = NDIDV1Rnd;
    if ( EncryptNdasID(&nid) )
    {
#ifdef XPLAT_SERIAL2ID
	sal_memcpy(id, nid.ndas_id[0], NDAS_KEY_LENGTH);
	sal_memcpy(id+5, nid.ndas_id[1], NDAS_KEY_LENGTH);
	sal_memcpy(id+10, nid.ndas_id[2], NDAS_KEY_LENGTH);
	sal_memcpy(id+15, nid.ndas_id[3], NDAS_KEY_LENGTH);

        id[NDAS_ID_LENGTH] = 0;
        sal_memcpy(key, nid.ndas_key, NDAS_KEY_LENGTH);
        key[NDAS_KEY_LENGTH] = 0;
#else
	sal_memcpy(id, nid.ndas_id[0], NDAS_KEY_LENGTH);
	sal_memcpy(id+5, nid.ndas_id[1], NDAS_KEY_LENGTH);
	sal_memcpy(id+10, nid.ndas_id[2], NDAS_KEY_LENGTH);

        /* Hide last 5 digit and key */
        sal_memset(id+15, '*', NDAS_ID_LENGTH);
        id[NDAS_ID_LENGTH] = 0;
        sal_memset(key, '*', NDAS_KEY_LENGTH);
        key[NDAS_KEY_LENGTH] = 0;
#endif
        return NDAS_OK;
    } else {
        return NDAS_ERROR_INVALID_PARAMETER;
    }
}
#else 
NDASUSER_API ndas_error_t 
ndas_start(void) 
{
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
}    

NDASUSER_API ndas_error_t 
ndas_restart() 
{
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
}

NDASUSER_API ndas_error_t 
ndas_stop() 
{
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
}

#ifdef NDAS_PROBE
NDASUSER_API 
ndas_error_t 
ndas_probed_size() {
    return ndev_probe_size();
}

NDASUSER_API 
ndas_error_t 
ndas_probed_list(struct ndas_probed* data, int devicen)
{
    return ndev_probe_serial(data, devicen);
}
#else
NDASUSER_API 
ndas_error_t 
ndas_probed_size() {
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
}

NDASUSER_API 
ndas_error_t 
ndas_probed_list(struct ndas_probed* data, int devicen)
{
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
}

#endif

NDASUSER_API ndas_error_t 
ndas_get_registration_data(char** data, int* size) 
{
#ifdef XPLAT_RESTORE
    return ndev_get_registration_data(data,size);
#else
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
#endif
}

NDASUSER_API ndas_error_t 
ndas_set_registration_data(char *data, int size) 
{
#ifdef XPLAT_RESTORE
    return ndev_set_registration_data(data, size);
#else
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
#endif
}
NDASUSER_API
ndas_error_t
ndas_registered_size()
{
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
}

NDASUSER_API
ndas_error_t ndas_registered_list(struct ndas_registered *data, int size) 
{
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
}


NDASUSER_API 
ndas_error_t ndas_query_ndas_id_by_serial(const char* serial, char* id, char* key)
{
    return NDAS_ERROR_UNSUPPORTED_FEATURE;
}
#endif /* NDAS_NO_LANSCSI*/

NDASUSER_API ndas_error_t    
ndas_get_string_error(ndas_error_t code, char* dest, int len)
{
    switch (code) {
    case NDAS_OK: 
        sal_strncpy(dest,"NDAS_OK",len); break;
    case NDAS_ERROR: 
        sal_strncpy(dest,"NDAS_ERROR",len); break;
    case NDAS_ERROR_DRIVER_NOT_LOADED: 
        sal_strncpy(dest,"DRIVER_NOT_LOADED",len); break;
    case NDAS_ERROR_DRIVER_ALREADY_LOADED: 
        sal_strncpy(dest,"DRIVER_ALREADY_LOADED",len); break;
    case NDAS_ERROR_LIBARARY_NOT_INITIALIZED: 
        sal_strncpy(dest,"LIBARARY_NOT_INITIALIZED",len); break;
    case NDAS_ERROR_INVALID_NDAS_ID: 
        sal_strncpy(dest,"INVALID_NDAS_ID",len); break;
    case NDAS_ERROR_INVALID_NDAS_KEY: 
        sal_strncpy(dest,"INVALID_NDAS_KEY",len); break;
    case NDAS_ERROR_NOT_IMPLEMENTED: 
        sal_strncpy(dest,"NOT_IMPLEMENTED",len); break;
    case NDAS_ERROR_INVALID_PARAMETER: 
        sal_strncpy(dest,"INVALID_PARAMETER",len); break;
    case NDAS_ERROR_INVALID_SLOT_NUMBER: 
        sal_strncpy(dest,"INVALID_SLOT_NUMBER",len); break;
    case NDAS_ERROR_INVALID_NAME: 
        sal_strncpy(dest,"invalid name",len); break;
    case NDAS_ERROR_NO_DEVICE: 
        sal_strncpy(dest,"NO_DEVICE",len); break;
    case NDAS_ERROR_ALREADY_REGISTERED_DEVICE: 
        sal_strncpy(dest,"ALREADY_REGISTERED_DEVICE",len); break;
    case NDAS_ERROR_ALREADY_ENABLED_DEVICE:
        sal_strncpy(dest,"ALREADY_ENABLED_DEVICE",len); break;
    case NDAS_ERROR_ALREADY_REGISTERED_NAME:
        sal_strncpy(dest,"ALREADY_REGISTERED_NAME",len); break;
    case NDAS_ERROR_ALREADY_DISABLED_DEVICE:
        sal_strncpy(dest,"ALREADY_DISABLED_DEVICE",len); break;
    case NDAS_ERROR_ALREADY_STARTED:
        sal_strncpy(dest,"ALREADY_STARTED",len); break;
    case NDAS_ERROR_ALREADY_STOPPED:
        sal_strncpy(dest,"ALREADY_STOPPED",len); break;
    case NDAS_ERROR_NOT_ONLINE    :
        sal_strncpy(dest,"the NDAS device is not online",len); break;
    case NDAS_ERROR_NOT_CONNECTED: 
        sal_strncpy(dest,"the NDAS device is not connected",len); break;
    case NDAS_ERROR_INVALID_HANDLE: 
        sal_strncpy(dest,"INVALID_HANDLE",len); break;
    case NDAS_ERROR_NO_WRITE_ACCESS_RIGHT: 
        sal_strncpy(dest,"no access right to write the data",len); break;
    case NDAS_ERROR_WRITE_BUSY: 
        sal_strncpy(dest,"WRITE_BUSY",len); break;        
    case NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION: 
        sal_strncpy(dest,"UNSUPPORTED_HARDWARE_VERSION",len); break;
    case NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION: 
        sal_strncpy(dest,"UNSUPPORTED_SOFTWARE_VERSION",len); break;
    case NDAS_ERROR_UNSUPPORTED_DISK_MODE: 
        sal_strncpy(dest,"UNSUPPORTED_DISK_MODE",len); break;
    case NDAS_ERROR_UNSUPPORTED_FEATURE: 
        sal_strncpy(dest,"UNSUPPORTED_FEATURE",len); break;
    case NDAS_ERROR_BUFFER_OVERFLOW: 
        sal_strncpy(dest,"BUFFER_OVERFLOW",len); break;
    case NDAS_ERROR_NO_NETWORK_INTERFACE: 
        sal_strncpy(dest,"NO_NETWORK_INTERFACE",len); break;
    case NDAS_ERROR_INVALID_OPERATION: 
        sal_strncpy(dest,"INVALID_OPERATION",len); break;
    case NDAS_ERROR_NETWORK_DOWN: 
        sal_strncpy(dest,"NETWORK_DOWN",len); break;
    case NDAS_ERROR_MEDIA_CHANGED: 
        sal_strncpy(dest,"MEDIA_CHANGED",len); break;
    case NDAS_ERROR_TIME_OUT: 
        sal_strncpy(dest,"Timed out",len); break;
    case NDAS_ERROR_READONLY: 
        sal_strncpy(dest,"read-only",len); break;
    case NDAS_ERROR_OUT_OF_MEMORY: 
        sal_strncpy(dest,"out of memory",len); break;
    case NDAS_ERROR_EXIST: 
        sal_strncpy(dest,"EXIST",len); break;
    case NDAS_ERROR_SHUTDOWN: 
        sal_strncpy(dest,"SHUTDOWN",len); break;
    case NDAS_ERROR_PROTO_REGISTRATION_FAIL:         
        sal_strncpy(dest,"PROTO_REGISTRATION_FAIL",len); break;
    case NDAS_ERROR_SHUTDOWN_IN_PROGRESS:
        sal_strncpy(dest,"Shutdown is in progress", len); break;        
    case NDAS_ERROR_ADDRESS_NOT_AVAIABLE: 
        sal_strncpy(dest,"ADDRESS_NOT_AVAIABLE",len); break;
    case NDAS_ERROR_NOT_BOUND: 
        sal_strncpy(dest,"NOT_BOUND",len); break;
    case NDAS_ERROR_NETWORK_FAIL:
        sal_strncpy(dest,"NETWORK_FAIL",len); break;
    case NDAS_ERROR_HDD_DMA2_NOT_SUPPORTED:
        sal_strncpy(dest,"Hard Disk Device does not support DMA 2 mode",len); break;
    case NDAS_ERROR_IDE_REMOTE_INITIATOR_NOT_EXIST:
        sal_strncpy(dest,"Remote Initiator not exists",len); break;
    case NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND:
        sal_strncpy(dest,"Remote Initiator bad command",len); break;
    case NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED:
        sal_strncpy(dest,"Remote Initiator command failed",len); break;
    case NDAS_ERROR_IDE_REMOTE_AUTH_FAILED:
        sal_strncpy(dest,"Remote Authorization failed",len); break;
    case NDAS_ERROR_IDE_TARGET_NOT_EXIST:
        sal_strncpy(dest,"Target not exists",len); break;
    case NDAS_ERROR_HARDWARE_DEFECT:
        sal_strncpy(dest,"Hardware defect",len); break;
    case NDAS_ERROR_BAD_SECTOR:
        sal_strncpy(dest,"Bad sector",len); break;
    case NDAS_ERROR_IDE_TARGET_BROKEN_DATA:
        sal_strncpy(dest,"Target broken data",len); break;
    case NDAS_ERROR_IDE_VENDOR_SPECIFIC:    
        sal_strncpy(dest,"IDE vendor specific error",len); break;
    case NDAS_ERROR_INTERNAL:            
        sal_strncpy(dest,"The error is caused by the internal framework bug",len); break;
    case NDAS_ERROR_MAX_USER_ERR_NUM: 
        sal_strncpy(dest,"MAX_USER_ERR_NUM",len); break;
    case NDAS_ERROR_INVALID_RANGE_REQUEST:
        sal_strncpy(dest,"Invalid range of request",len); break;
    case NDAS_ERROR_INVALID_METADATA:
        sal_strncpy(dest,"Invalid metadata", len); break;
    case NDAS_ERROR_CONNECT_FAILED:
        sal_strncpy(dest,"Failed to connect", len); break;
    default: 
        sal_snprintf(dest,len,"UNKNOWN CODE(%d)",code); break;
    }
    return NDAS_OK;
}

