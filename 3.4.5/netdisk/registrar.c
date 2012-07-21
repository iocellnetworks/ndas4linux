#ifndef NDAS_NO_LANSCSI
#include "xplatcfg.h"
#include <sal/types.h>
#include <sal/libc.h>
#include <sal/debug.h>
#include <sal/time.h>
#include <xlib/xhash.h>

#include "netdisk/ndev.h"
#include "registrar.h"
#include "ndpnp.h"
#include "des.h"
#include "netdisk/sdev.h"
#include "netdisk/serial.h"
#include "netdisk/netdiskid.h"

#ifdef DEBUG
#define    debug_registrar(l, x...)    \
do {\
    if(l <= DEBUG_LEVEL_REGISTRAR ) {     \
        sal_debug_print("REG|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x); \
    } \
} while(0) 
#define    debug_pnp(l, x...)    \
do {\
    if(l <= DEBUG_LEVEL_LPX_PNP ) {     \
        sal_debug_print("PNP|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x); \
    } \
} while(0) 
#else
#define debug_registrar(l,x...) do {} while(0); 
#define debug_pnp(l,x...) do {} while(0);
#endif

struct reg_arg { 
    int count;
    struct ndas_registered *data;
    int size;
};
#ifdef XPLAT_PNP
struct probe_arg {
    int count;
    struct ndas_probed *data;
    int size;
};
#endif

struct m2d {
    struct list_head node;
    unsigned char *mac;
    ndev_t* ndev;
};

struct _slot {
    sal_spinlock lock;
    XLIB_HASH_TABLE*  slot_2_sdev; // mapping ( logical_slot -> logunit_t * )
    /* only registered devices are listed */
    XLIB_HASH_TABLE*  name_2_device; // mapping (name -> ndas_device_info_ptr)
    /* probed devices are listed */
    XLIB_HASH_TABLE*  serial_2_device; // mapping (serial -> ndas_device_info_ptr)
    /* probed devices are listed */
    struct list_head mac_2_device;
    xuint32           max_slot;
};
LOCAL struct _slot v_slot;

/*
 * called in bpc
 */
LOCAL ndas_error_t nosync_disappear(const char *ndasid, void *noarg1);

/* note: the name is allocated when registered and maintained in the structure.
        will be de-allocated when unregistered. */


LOCAL
xbool m2d_remove(unsigned char *network_id, int sync)
{
    struct list_head *pos;
    if ( sync ) sal_spinlock_take(v_slot.lock);
    list_for_each(pos, &v_slot.mac_2_device) {
        struct m2d *n = list_entry(pos,struct m2d, node);
        if ( sal_memcmp(n->mac , network_id, SAL_ETHER_ADDR_LEN) == 0 ) {
            list_del(&n->node);
            sal_free(n);
            if ( sync ) sal_spinlock_give(v_slot.lock);        
            return TRUE;
        }
    }
    if ( sync ) sal_spinlock_give(v_slot.lock);    
    return FALSE;
}

LOCAL xuint32 v_name_hash(char* name) 
{
    xuint32 hash = 0;
    int i;
    for (i = 0; i<NDAS_MAX_NAME_LENGTH && name[i];i++)
        hash ^= name[i];
    return hash;
}
LOCAL xbool v_name_equals(char* a, char* b) {
    if ( !a ) return FALSE;
    if ( !b ) return FALSE;
    return sal_strncmp(a,b,NDAS_MAX_NAME_LENGTH) == 0;
}

LOCAL xuint32 v_6bytes_hash(void* key) 
{    
    char* six = (char*) key;
    return six[0] ^ 
        six[1] ^ 
        six[2] ^ 
        six[3] ^ 
        six[4] ^
        six[5];
}
LOCAL xuint32 v_slot_hash(void* slot) 
{
    long _slot = (long)slot;
    return (int) _slot;
}
LOCAL xbool v_slot_equals(void *a, void *b) 
{
    return ((long) a) == ((long) b);
}

LOCAL xbool v_slot_serial_equals(void *a, void *b) 
{
    char *aid = (char*) a;
    char *bid = (char*) b;
    int i = 0;
    
    for (i = 0; i < NDAS_SERIAL_LENGTH; i++ ) {
        if ( aid[i] != bid[i] ) return FALSE;
    }
    return TRUE;
}

#if 0 /* not used */
LOCAL xbool v_mac_equals(void *a, void *b) 
{
    char *aid = (char*) a;
    char *bid = (char*) b;
    int i = 0;
    
    for (i = 0; i < LPX_NODE_LEN; i++ ) {
        if ( aid[i] != bid[i] ) return FALSE;
    }
    return TRUE;
}
#endif

LOCAL void v_enumerator(void* name, void* _dev, struct reg_arg* arg) 
{
    debug_registrar(3, "name=%s", (char*) name);
    debug_registrar(3, "info->ndas_serial=%s", ((ndev_t*)_dev)->info.ndas_serial);
    if ( arg->count >= arg->size ) {
        return;
    }
    sal_strncpy(arg->data[arg->count++].name,name,NDAS_MAX_NAME_LENGTH);
}

/* extern APIs */
ndas_error_t registrar_init(int max_slot) 
{
    int ret;
    ret = sal_spinlock_create("r", &v_slot.lock);
    if (!ret) {
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    v_slot.max_slot = max_slot;
    v_slot.slot_2_sdev = xlib_hash_table_new(v_slot_hash, v_slot_equals);
    v_slot.name_2_device = xlib_hash_table_new((XlibHashFunc)v_name_hash,(XlibEqualFunc)v_name_equals);
    v_slot.serial_2_device = xlib_hash_table_new(v_6bytes_hash, v_slot_serial_equals);
    INIT_LIST_HEAD(&v_slot.mac_2_device);
    return NDAS_OK;
}
LOCAL
xbool return_true(void* key, void* value, void* user_data) 
{
    return TRUE;
}
/* 
 * Free those ndev_t* objects created by pnp-discovery facility.
 * TODO: invent the better way for free these object.
 *  also there could be a race condition for creating object
 *  make sure the pnp-discovery is disabled when this function is called.
 * called by : ndas_stop (user thread of 'ndasadmin stop')
 */
void registrar_stop() 
{
    dpc_id bid;
    sal_event comp_event;
    int ret;

    debug_registrar(1, "ing");
    comp_event = sal_event_create("registrar-stop");
    if(comp_event == SAL_INVALID_EVENT) {
        sal_error_print("ndas: fail to alloc an event.\n");
        return;
    }

    sal_spinlock_take(v_slot.lock);

    while(!list_empty(&v_slot.mac_2_device))
    {
        struct m2d *n = list_entry(v_slot.mac_2_device.next, struct m2d, node);
        ndev_t* ndev = n->ndev;

        list_del(&n->node);
        sal_free(n);
        sal_spinlock_give(v_slot.lock);

        // Unregister the ndev
        ndev_unregister(ndev);

        // call nosync_disappear() in the BPC thread.
        bid = bpc_create((dpc_func)nosync_disappear, ndev->info.ndas_id, NULL, comp_event, 0);
        if(!bid) {
            sal_error_print("ndas: fail to alloc a BPC.\n");
            break;
        }
        ret = bpc_queue(bid, 0);
        if (ret < 0) {
            bpc_destroy(bid);
            sal_error_print("ndas: failed to queue a BPC.\n");
            break;
        }
        // Wait on the completion of the BPC
        ret = sal_event_wait(comp_event, 30*SAL_TICKS_PER_SEC);
        if(ret != NDAS_OK) {
            sal_error_print("ndas: timed out to remove object from \'ndas_stop\'\n");
            break;
        }
        debug_registrar(4, "reset event %p", comp_event);
        sal_event_reset(comp_event);

        // Clean up the ndev
        ndev_cleanup(ndev);

        sal_spinlock_take(v_slot.lock);
    }
    xlib_hash_table_foreach_remove(v_slot.name_2_device,return_true ,NULL);
    xlib_hash_table_foreach_remove(v_slot.serial_2_device,return_true ,NULL);
    xlib_hash_table_foreach_remove(v_slot.slot_2_sdev,return_true ,NULL);
    sal_spinlock_give(v_slot.lock);
    debug_registrar(1, "ed");
}
/* 
 * called by ndas_cleanup (user thread of 'rmmod ndas_core')
 */
void registrar_cleanup(void) 
{
    debug_registrar(1, "ing");
    sal_spinlock_take(v_slot.lock);

    sal_assert(xlib_hash_table_size(v_slot.name_2_device) == 0);
    xlib_hash_table_destroy(v_slot.name_2_device); 
    v_slot.name_2_device = NULL;

    sal_assert(xlib_hash_table_size(v_slot.serial_2_device) == 0);
    xlib_hash_table_destroy(v_slot.serial_2_device); 
    v_slot.serial_2_device = NULL;

    sal_assert(list_empty(&v_slot.mac_2_device));

    sal_assert(xlib_hash_table_size(v_slot.slot_2_sdev) == 0);
    xlib_hash_table_destroy(v_slot.slot_2_sdev); 
    v_slot.slot_2_sdev = NULL;

    sal_spinlock_give(v_slot.lock);
    sal_spinlock_destroy(v_slot.lock);

    debug_registrar(1, "ed");
}

ndev_t* ndev_lookup_byname(const char* name) {
    ndev_t* ret;

    if (name == NULL || name[0]==0) {
        return NULL;
    }
    sal_assert(v_slot.name_2_device);

    ret = (ndev_t*) xlib_hash_table_lookup(v_slot.name_2_device, name);

    if (ret == NULL) {
        debug_registrar(4, "Failed to find %s, name_2_device size=%d", 
            name, xlib_hash_table_size(v_slot.name_2_device));
    }
    return ret;
}

ndev_t* ndev_lookup_byserial(const char* serial) {
    sal_assert(v_slot.serial_2_device);
    return (ndev_t*) xlib_hash_table_lookup(v_slot.serial_2_device, serial);
}

inline static 
ndev_t* ndev_lookup_bynetworkid_nosync(xuchar* network_id) {
    struct list_head *pos;
    list_for_each(pos, &v_slot.mac_2_device) 
    {
        struct m2d *n = list_entry(pos,struct m2d, node);
        if ( sal_memcmp(n->mac , network_id, SAL_ETHER_ADDR_LEN) == 0 ) {
            ndev_t* ret = n->ndev;
            return ret;
        }
    }
    return NULL;
}
ndev_t* ndev_lookup_bynetworkid(xuchar* network_id) {
    ndev_t* np;
    sal_spinlock_take(v_slot.lock);
    np = ndev_lookup_bynetworkid_nosync(network_id);
    sal_spinlock_give(v_slot.lock);        
    return np;
}

ndas_error_t ndev_register(ndev_t* ndev) 
{
    debug_registrar(3, "ing ndev=%p", ndev);
    debug_registrar(5, "ing name=%s", ndev->info.name);
    debug_registrar(5, "ing id=%s, MAC=%02x:%02x:%02x:%02x:%02x:%02x", 
    	ndev->info.ndas_id, 
    	ndev->network_id[0], ndev->network_id[1], ndev->network_id[2],
    	ndev->network_id[3], ndev->network_id[4], ndev->network_id[5]
    );
    if ( ndev->info.name[0] == '\0' ) {
        return NDAS_ERROR_INVALID_PARAMETER;
    }
    sal_spinlock_take(v_slot.lock);
    if ( !v_slot.name_2_device ) {
        sal_spinlock_give(v_slot.lock);
        return NDAS_ERROR_SHUTDOWN;
    }
    debug_registrar(3, "ndev->info.name=%s ndev=%p", ndev->info.name, ndev);
    xlib_hash_table_insert(v_slot.name_2_device, ndev->info.name, ndev);

out:
    sal_spinlock_give(v_slot.lock);
    return NDAS_OK;
}
/*
 * Remove the entry in ndas_2_device registarar and reset the name.
 * called by 
 *   1. ndas_unregister_device (user thread 'ndasadmin unregister' )
 *   2. ndas_register_device (user thread 'ndasadamin register' ) on fail to register
 *   3. registrar_stop (user thread 'ndasadmin stop') for each registered NDAS device
 *   4. registrar_cleanup (user thread 'rmmod ndas_core') for each registered NDAS device
 */
ndas_error_t ndev_unregister(ndev_t* dev) {
    debug_registrar(5, "ing dev=%p name=%s", dev, dev->info.name);
    sal_spinlock_take(v_slot.lock);
    if ( !v_slot.name_2_device ) {
        sal_spinlock_give(v_slot.lock);
        return NDAS_ERROR_SHUTDOWN;
    }

    xlib_hash_table_remove(v_slot.name_2_device, dev->info.name);
    /* should be removed. if not set 0, will not be removed by nosync_disappear */
    dev->info.name[0] = 0;
    dev->info.status = NDAS_DEV_STATUS_UNKNOWN;
    dev->online_notified = 0;
    
//    dev->last_tick = 0;
//    e = nosync_disappear(dev->info.ndas_idserial);
    sal_spinlock_give(v_slot.lock);
    return NDAS_OK;
}

/*
 * Convert the NDAS network id (mac address) into NDAS serial
 * 
 */
LOCAL void get_ndasid(xuchar* network_id,char* ndas_id) {
    ndas_id_info nid;
    sal_assert(network_id);
    debug_registrar(6, "ing network_id=%p ndas_id=%p", network_id,ndas_id);
    sal_memcpy(nid.ndas_network_id, network_id, 6);
    sal_memcpy(nid.key1, NDIDV1Key1, 8);
    sal_memcpy(nid.key2, NDIDV1Key2, 8);    
    sal_memcpy(nid.reserved, NDIDV1Rsv, 2);    
#ifdef NDAS_CRYPTO
    nid.vid = NDIDV1VID_NETCAM; 
#else
    nid.vid = NDIDV1VID_DEFAULT; 
#endif
    nid.random = NDIDV1Rnd;

    EncryptNdasID(&nid);

    sal_memcpy(ndas_id, nid.ndas_id,NDAS_ID_LENGTH);
    ndas_id[NDAS_ID_LENGTH] = 0;
    debug_registrar(6, "TRUE");
}

/*
 * Look up the ndev of the given network_id.
 * Insert ndev into ndasid_2_device , mac_2_device hashtable
 * Called by 
 *   ndas_register_device
 *   v_update_alive_info
 * To do: split this function to ndev_lookup and ndev_create
 */
ndas_error_t ndev_lookup(xuchar *network_id, ndev_t* *ret) 
{
    struct m2d *m2d_node;
    debug_registrar(3, "network_id=%s", SAL_DEBUG_HEXDUMP_S(network_id, SAL_ETHER_ADDR_LEN));
    
    sal_assert(network_id);
    sal_spinlock_take(v_slot.lock);
    if ( !v_slot.serial_2_device ) {
        sal_spinlock_give(v_slot.lock);
        return NDAS_ERROR_SHUTDOWN;
    }

    (*ret) = ndev_lookup_bynetworkid_nosync(network_id);
    
    if ( (*ret) ) {
        sal_spinlock_give(v_slot.lock);
        debug_registrar(9, "exists devid=%s", (*ret)->info.ndas_serial);
        return NDAS_OK;
    }

    sal_spinlock_give(v_slot.lock);
    
    (*ret) = sal_malloc(sizeof(ndev_t));
    if ( !(*ret) ) {
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    sal_memset((*ret), 0, sizeof(ndev_t));
    
#if 0
    (*ret)->break_lock = sal_semaphore_create("nb", 1, 1);
    if ( (*ret)->break_lock == SAL_INVALID_SEMAPHORE ) 
    {
        sal_free((*ret));
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
#endif

    m2d_node = sal_malloc(sizeof(struct m2d));
    if ( !m2d_node ) 
    {
#if 0
        sal_semaphore_destroy((*ret)->break_lock);
#endif
        sal_free((*ret));
        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    sal_spinlock_take(v_slot.lock);
    get_serial_from_networkid((*ret)->info.ndas_serial, network_id);
    get_ndasid(network_id,(*ret)->info.ndas_id);

    sal_memcpy((*ret)->network_id, network_id, SAL_ETHER_ADDR_LEN);    

    (*ret)->info.version = NDAS_VERSION_UNKNOWN;
    (*ret)->info.status = NDAS_DEV_STATUS_UNKNOWN;

    debug_registrar(3, "ndev=%p serial=%s mac=%s",(*ret), (*ret)->info.ndas_serial,
            SAL_DEBUG_HEXDUMP_S((*ret)->network_id, SAL_ETHER_ADDR_LEN));

    
    xlib_hash_table_insert(v_slot.serial_2_device, 
                (void*) (*ret)->info.ndas_serial, (*ret));

    {
        m2d_node->mac = (*ret)->network_id;
        m2d_node->ndev = (*ret);
        list_add(&m2d_node->node, &v_slot.mac_2_device);
    }
    sal_spinlock_give(v_slot.lock);
    return NDAS_OK;
}

/*
 * The NDAS device is vanished in the local network
 * 
 * called by
 *   1. ndev_vanished (bpc)
 *   2. registrar_cleanup (user thread of 'rmmod ndas_core' but via bpc_queue
 *   3. registrar_stop (user thread of 'ndasadmin stop' but via bpc_queue)
 * To do: change name and parameter of this function
 */
LOCAL ndas_error_t nosync_disappear(const char *serial, void *noarg1)
{
    ndev_t* ndev;
    ndas_error_t ret;
    
    sal_spinlock_take(v_slot.lock);
    ndev = ndev_lookup_byserial(serial);
    debug_pnp(1, "serial=%s",serial);
    debug_pnp(5, "dev=%p",ndev);
    if ( !ndev )
    {
        ret = NDAS_ERROR_INTERNAL;
        debug_pnp(2, "Not found");
        goto out;
    }

    if ( !v_slot.serial_2_device ) {
        ret = NDAS_ERROR_SHUTDOWN;
        goto out;
    }
#ifdef XPLAT_PNP
    ndev->info.status = NDAS_DEV_STATUS_OFFLINE;
#else
    ndev->info.status = NDAS_DEV_STATUS_UNKNOWN;
#endif
    ndev_notify_status_changed(ndev, ndev->info.status);

    if ( ndev->info.name[0] ) {
        /* Keep registered device in the list */
        ret = NDAS_ERROR_ALREADY_REGISTERED_DEVICE; // registered device should be remain
    } else {
        /* Remove from table */
        xlib_hash_table_remove(v_slot.serial_2_device, ndev->info.ndas_serial);
        m2d_remove(ndev->network_id, FALSE);
        ret = NDAS_OK;
    }

out:
    sal_spinlock_give(v_slot.lock);
    return ret;
}

/*
 * The NDAS device gone offline.
 * only called by v_netdisk_alive_info_test_dead_disk
 * THREAD : bpc 
 */
ndas_error_t ndev_vanished(const char *serial) {
    ndas_error_t e;
    e = nosync_disappear(serial, NULL);
    return e;
}

ndas_error_t sdev_register(logunit_t *sdev)
{
    int allocated_slot = NDAS_INVALID_SLOT;
    int i = 0;

    sal_spinlock_take(v_slot.lock);
    
    for (i = NDAS_FIRST_SLOT_NR; i < v_slot.max_slot; i++)
    {
    	if (sdev_lookup_byslot(i) == NULL)
        {
    		allocated_slot = i;
    		break;
    	}
    }

    if ( i == NDAS_INVALID_SLOT )
    {
    	sal_spinlock_give(v_slot.lock);
        return NDAS_ERROR_SHUTDOWN;
    }

    sdev->info.slot = allocated_slot;

    debug_registrar(3,"slot=%d", sdev->info.slot);
    // TODO: check the validity of slot number.

    if ( !v_slot.slot_2_sdev ) {
        sal_spinlock_give(v_slot.lock);
        return NDAS_ERROR_SHUTDOWN;
    }
    
    xlib_hash_table_insert(v_slot.slot_2_sdev, (void*)(long)sdev->info.slot, sdev);
    sal_spinlock_give(v_slot.lock);
    return NDAS_OK;
}

ndas_error_t sdev_unregister(logunit_t *sdev) 
{
    debug_registrar(1, "ing slot=%d", sdev->info.slot);
    if ( !v_slot.slot_2_sdev ) {
        return NDAS_ERROR_SHUTDOWN;
    }
    xlib_hash_table_remove(v_slot.slot_2_sdev, (void *)(long)sdev->info.slot);
    return NDAS_OK;
}
logunit_t * sdev_lookup_byslot(int slot) 
{
#ifdef DEBUG
    logunit_t *ret = xlib_hash_table_lookup(v_slot.slot_2_sdev, (void *)(long)slot);
    debug_registrar(5, "ing slot=%d", slot);
    if ( ret ) sal_assert(ret->magic == SDEV_MAGIC);
    return ret;
#else
    debug_registrar(5, "ing slot=%d", slot);
    return (logunit_t *) xlib_hash_table_lookup(v_slot.slot_2_sdev, (void *)(long)slot);
#endif
}

#ifdef XPLAT_PNP

ndas_error_t ndev_probe_size() 
{
    return xlib_hash_table_size(v_slot.serial_2_device);
}
LOCAL void v_probie_serial(void* key, void* value, void* user_data) 
{
    ndev_t* dev = (ndev_t*) value;
    struct probe_arg *arg = (struct probe_arg *) user_data;
    debug_pnp(3, "ing dev=%p key=%p count=%d", dev, key, arg->count);
    debug_pnp(3, "ing key=%s", (char*) key);
    if ( arg->count >= arg->size ) {
        return;
    }

    arg->data[arg->count].status = dev->info.status;
    sal_memcpy(arg->data[arg->count].serial, key, NDAS_SERIAL_LENGTH);
    arg->data[arg->count].serial[NDAS_SERIAL_LENGTH] =0;
    arg->count++;
}

ndas_error_t ndev_probe_serial(struct ndas_probed *data, int size) 
{
    struct probe_arg arg = { 
        .data = data, 
        .count = 0, 
    };
    
    debug_pnp(3, "ing size=%d", size);
    if ( !data ) return NDAS_ERROR_INVALID_PARAMETER;
    sal_spinlock_take(v_slot.lock);

    arg.size = xlib_hash_table_size(v_slot.serial_2_device);
    xlib_hash_table_foreach(v_slot.serial_2_device, v_probie_serial, &arg);
    
    sal_spinlock_give(v_slot.lock);
    debug_pnp(3, "ed count=%d", arg.count);
    return arg.count;
}


#endif

ndas_error_t
ndev_registered_size()
{
    return xlib_hash_table_size(v_slot.name_2_device);
}    
ndas_error_t ndev_registered_list(struct ndas_registered *data, int size ) 
{
    struct reg_arg arg = { .data = data, .count = 0 };
    sal_spinlock_take(v_slot.lock);
    if ( !v_slot.name_2_device ) {
        sal_spinlock_give(v_slot.lock);
        return NDAS_ERROR_SHUTDOWN;
    }
    arg.size = xlib_hash_table_size(v_slot.name_2_device);
    
    xlib_hash_table_foreach(v_slot.name_2_device, (XlibHashIteFunc) v_enumerator, &arg);
    sal_spinlock_give(v_slot.lock);
    return arg.count;
}

ndas_error_t ndev_rename(ndev_t* ndev, const char* newname) 
{
    if ( sal_strlen(newname) > NDAS_MAX_NAME_LENGTH ) {
        return NDAS_ERROR_INVALID_NAME;
    }
    sal_spinlock_take(v_slot.lock);
    if ( !v_slot.name_2_device ) {
        sal_spinlock_give(v_slot.lock);
        return NDAS_ERROR_SHUTDOWN;
    }
    xlib_hash_table_remove(v_slot.name_2_device, ndev->info.name);
    sal_strncpy(ndev->info.name, newname, NDAS_MAX_NAME_LENGTH);
    xlib_hash_table_insert(v_slot.name_2_device, ndev->info.name, ndev);
    sal_spinlock_give(v_slot.lock);
    return NDAS_OK;
}

#ifdef XPLAT_RESTORE

#define NDAS_RESTORE_ENTRY_SIZE (32*5)
#if NDAS_RESTORE_ENTRY_SIZE <(NDAS_MAX_NAME_LENGTH + NDAS_ID_LENGTH + 1 + NDAS_KEY_LENGTH + 1 )
#error NDAS_RESTORE_ENTRY_SIZE is not small
#endif
#define DES_UNIT 8u
struct _reg {
    char name[NDAS_MAX_NAME_LENGTH];
    char ndas_id[NDAS_ID_LENGTH + 1];
    char ndas_key[NDAS_KEY_LENGTH + 1];
    xuint16 enabled_bits;   /* Initial enabling status */
    xuint16 reserved;
};

LOCAL char key[] = {0x27, 0xE2, 0x55, 0x3A, 0xA6, 0xE2, 0xD9, 0x7A};

struct enc_arg {
    char* data;
    int count;
};

LOCAL void v_encryptor(char* name, ndev_t* ndev, struct enc_arg* arg)
{
    xuint32 des_key[32];
    char* ob; 
    char ik[32];
    unsigned int i;
    char *ib;
    xuint16 enabled_bitmap = 0;
    xuint16 reserved = 0;
    
    debug_registrar(3,"ing ndev=%p", ndev);
    debug_registrar(3,"ing data=%p",arg->data);
    debug_registrar(8,"ing name=%s(%p)", ndev->info.name, ndev->info.name);
    debug_registrar(5,"ing ndas_idserial=%s(%p)", ndev->info.ndas_id, ndev->info.ndas_id);
    des_ky(key, des_key);

    ib = (char*) ndev->info.name;
    debug_registrar(5,"ing name=%s(%p)",ndev->info.name, ndev->info.name);
    ob = arg->data + (arg->count++ * NDAS_RESTORE_ENTRY_SIZE); 
    for (i = 0; i < NDAS_MAX_NAME_LENGTH/DES_UNIT; i++) 
    {
        des_ec(ib, ob, des_key);
        ib += DES_UNIT;
        ob += DES_UNIT;
    }

    ib = ik;
debug_registrar(5,"ing ndas_idserial=%s(%p)", ndev->info.ndas_id, ndev->info.ndas_id);
    sal_memcpy(ib, ndev->info.ndas_id, sizeof(ndev->info.ndas_id));
    ib+= sizeof(ndev->info.ndas_id);
    
    debug_registrar(5,"ing name=%s(%p)",ndev->info.name, ndev->info.name);
    debug_registrar(5,"ing ndas_idserial=%s(%p)", ndev->info.ndas_id, ndev->info.ndas_id);
    debug_registrar(5,"ing ndas_key=%s", ndev->info.ndas_key);
    if ( ndev->info.ndas_key == NULL || ndev->info.ndas_key[0] == '\0' ) {
        sal_memset(ib, 0, sizeof(ndev->info.ndas_key));
    } else 
        sal_memcpy(ib , ndev->info.ndas_key, sizeof(ndev->info.ndas_key));
    ib+= sizeof(ndev->info.ndas_key);
    
    /* no consideration for endian */
    debug_registrar(5,"ing nr_unit=%d", ndev->info.nr_unit);
    for (i = 0; i < ndev->info.nr_unit; i++) 
    {
        debug_registrar(5,"ing ndev->unit[i]=%p", ndev->unit[i]);
        if ( ndev->unit[i] && ndev->unit[i]->info.enabled ) 
        {
            int bit = 0;
            if ( ndev->unit[i]->info.writeshared ) 
                bit = REGDATA_BIT_SHARE_ENABLED;
            else if ( ndev->unit[i]->info.writable ) 
                bit = REGDATA_BIT_WRITE_ENABLED;
            else 
                bit = REGDATA_BIT_READ_ENABLED;
            
            enabled_bitmap |= bit << (i*2);   
            debug_registrar(5,"ing bit=0x%x", bit);
        }
    }
    debug_registrar(5,"ing enabled_bitmap=0x%x", enabled_bitmap);
    sal_memcpy(ib, &enabled_bitmap, sizeof(enabled_bitmap));
    ib+=sizeof(enabled_bitmap);
    sal_memcpy(ib, &reserved, sizeof(reserved));

    ib = ik;
    for (i = 0; i < 32/DES_UNIT; i++) {
        des_ec(ib, ob , des_key);
        ib += DES_UNIT;
        ob += DES_UNIT;
    }
    return;
}

LOCAL ndas_error_t v_decryptor(int c, char* data, struct _reg* reg)
{
    xuint32 des_key[32];
    unsigned int i;
    char ik[32];
    char* ob;
    char* ib;
    
    des_ky(key,des_key);

    ib = reg->name;
    ob = data + (c * NDAS_RESTORE_ENTRY_SIZE); 
    for (i = 0; i < NDAS_MAX_NAME_LENGTH/DES_UNIT; i++) {
        des_dc(ob, ib, des_key);
        ib += DES_UNIT;
        ob += DES_UNIT;
    }
    ib = ik;
    for (i = 0; i < sizeof(ik)/DES_UNIT; i++) {
        des_dc(ob, ib, des_key);
        ib += DES_UNIT; 
        ob += DES_UNIT;
    }

    sal_assert( NDAS_ID_LENGTH + 1 + NDAS_KEY_LENGTH + 1 + 
                sizeof(reg->enabled_bits) + sizeof(reg->reserved) <= sizeof(ik));
    ib = ik;
    sal_memcpy(reg->ndas_id, ib, NDAS_ID_LENGTH + 1); 
    ib += NDAS_ID_LENGTH + 1;
    sal_memcpy(reg->ndas_key, ib, NDAS_KEY_LENGTH + 1); 
    ib += NDAS_KEY_LENGTH + 1;
    /* no consideration for the endianness */
    sal_memcpy(&reg->enabled_bits, ib, sizeof(reg->enabled_bits));

    debug_registrar(5,"name=%s", reg->name);
    debug_registrar(5,"ndas_idserial=%s", reg->ndas_id);
    debug_registrar(5,"ndas_key=%s", reg->ndas_key);
    debug_registrar(5,"enabled_bitmap=0x%x", reg->enabled_bits);
        
    return  NDAS_OK;
}

ndas_error_t ndev_get_registration_data(char **data,int *size) 
{
    struct enc_arg arg;
    sal_spinlock_take(v_slot.lock);
    (*size) = xlib_hash_table_size(v_slot.name_2_device) * NDAS_RESTORE_ENTRY_SIZE  + 5;
    (*data) = sal_malloc(*size);
    
    debug_registrar(3, "data=%p size=%d",*data,(*size));
    if ( (*data) == NULL ) {
        sal_spinlock_give(v_slot.lock);
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    sal_memcpy((*data), "NDAS", 4);
    (*data)[4] = 0x1; // version

    arg.count = 0;
    arg.data = (*data) + 5;
    
    xlib_hash_table_foreach(v_slot.name_2_device, 
        (XlibHashIteFunc) v_encryptor, &arg);
    debug_registrar(3, "ed count=%d", arg.count);
    sal_spinlock_give(v_slot.lock);
    return NDAS_OK;
}
ndas_error_t ndev_set_registration_data(char *data, int size)
{
    int i;
    int num_disk;
    struct _reg* to_register;
    ndas_error_t e;
    debug_registrar(3,"ing data=%p size=%d", data,size);
    
    if ( sal_memcmp(data, "NDAS", 4) != 0 ) 
        return NDAS_ERROR_INVALID_NDAS_ID;
    
    if ( data[4] != 0x1 ) 
        return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;

    num_disk = (size - 5) / NDAS_RESTORE_ENTRY_SIZE;
    if ( num_disk * NDAS_RESTORE_ENTRY_SIZE != size - 5 ) 
        return NDAS_ERROR_INVALID_NDAS_ID;

    debug_registrar(5,"num_disk=%d", num_disk);
    to_register = sal_malloc(num_disk * sizeof(struct _reg ));
    for ( i = 0 ; i < num_disk ; i++) {
        debug_registrar(5,"data=%p to_register=%p", data + 5,to_register + i);
        v_decryptor(i, data + 5,  to_register + i);
    }
    
    for ( i = 0; i < num_disk; i++) 
    {
        ndev_t *ndev;
        debug_registrar(5,"name=%s", to_register[i].name);
        debug_registrar(5,"ndas_idserial=%s", to_register[i].ndas_id);
        debug_registrar(5,"ndas_key=%s", to_register[i].ndas_key);
        debug_registrar(5,"ndas_key=0x%x", to_register[i].enabled_bits);
        e = register_internal(to_register[i].name, to_register[i].ndas_id, 
            to_register[i].ndas_key, to_register[i].enabled_bits, FALSE);
        if ( !NDAS_SUCCESS(e)) {
            to_register[i].ndas_id[15] = '*';
            to_register[i].ndas_id[16] = '*';
            to_register[i].ndas_id[17] = '*';
            to_register[i].ndas_id[18] = '*';
            to_register[i].ndas_id[19] = '*';
            debug_registrar(1,"ndas: fail to register the ndas device %s\n", to_register[i].ndas_id);
            
            debug_registrar(1,"e=%d",e);
        }
        ndev = ndev_lookup_byname(to_register[i].name);
        if ( !ndev ) {
            debug_registrar(1,"Fail to get the just-registered ndev object %s",to_register[i].name);
            continue;
        }
    }
    sal_free(to_register);
    return NDAS_OK;
}
#endif

#endif /* #ifndef NDAS_NO_LANSCSI */

