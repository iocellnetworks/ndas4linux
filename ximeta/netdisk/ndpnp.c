/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/

#include "xplatcfg.h"
#include "sal/libc.h"
#include "sal/mem.h"
#include "sal/types.h"
#include "sal/time.h"
#include "sal/debug.h"
#include "lpx/lpx.h"
#include "xlib/xhash.h"
#include "xlib/dpc.h"
#include "xlib/gtypes.h" // g_ntohl g_htonl g_ntohs g_htons
#include "netdisk/list.h"
#include "netdisk/ndev.h"
#include "netdisk/serial.h"
#include "netdisk/netdiskid.h" // NDIDV1Key1, NDIDV1Key2
#include "ndpnp.h"
#include "registrar.h"

#ifdef XPLAT_PNP
#ifdef DEBUG
#define    debug_pnp(l, x...)    \
do {\
    if(l <= DEBUG_LEVEL_LPX_PNP ) {     \
        sal_debug_print("PNP|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x); \
    } \
} while(0) 
#else
#define debug_pnp(l,x...) do {} while(0)
#endif

#define RX_PACKET_BUFFER_SIZE 1600
#define ALIVE_CHECK_INTERVAL  (10 * SAL_TICKS_PER_SEC) 

LOCAL void v_update_alive_info(xuchar* mac, int type, int ver);

struct _ndpnp {
    xint16 running;
    xint16 buf_sz;
    dpc_id alive_checker;
    char* buf;
    int fd;
    XLIB_HASH_TABLE *changed_inner_handlers;
    ndas_device_handler changed_ext_listener;
    struct sockaddr_lpx slpx;
};

struct subscribe {
    char serial[NDAS_SERIAL_LENGTH+1];
    ndas_device_handler func;
};

LOCAL struct _ndpnp  v_pnp = { 
    .running = FALSE,
    .buf_sz =RX_PACKET_BUFFER_SIZE,
    .fd = -1
};


LOCAL
xuint32    sub_hash(void* key)
{
    xuchar* ck = (xuchar*) key;
    xuint32 val = ck[0] ^ (ck[1] << 8) ^ (ck[2] << 16) ^ (ck[3] << 24) 
        ^ ck[4] ^ (ck[5] << 16) ^ (ck[6] << 16) ^ (ck[7] << 24);
    if (ck[8]) {
        /* Long form serial */
        val ^= ck[8] ^ (ck[9] << 16) ^ (ck[10] << 16) ^ (ck[11] << 24)
        ^ ck[12] ^ (ck[13] << 16) ^ (ck[14] << 16) ^ (ck[15] << 24);       
    }
    return val;
}

LOCAL
xbool sub_equals(void* a, void* b)
{
    return sal_strncmp(a,b,NDAS_SERIAL_LENGTH) == 0;
}

NDASUSER_API void ndas_set_device_change_handler(ndas_device_handler change_handler)
{
    debug_pnp(1,"Setting device change handler:%p", change_handler);
    v_pnp.changed_ext_listener = change_handler;
}

/* 
 * Fire the internal/extern listener that want to be notified on going offline/online
 * Called by
 *   1. ndev_register_device (user thead, 'ndasadmin register ...', but via bpc_queue)
 *   2. v_update_alive_info :bpc 
 *          on receiving PNP package 
 *   3. nosync_disappear bpc 
 */
void ndev_notify_status_changed(ndev_t* ndev,NDAS_DEV_STATUS status)
{
    struct subscribe *s;
    ndev_t* regdev;

    if (!v_pnp.running) {
        return;
    }
    debug_pnp(4,"ndev=%p ndasid=%s status=%d lasttick=%u", ndev, ndev->info.ndas_id, status, ndev->last_tick);
    s = xlib_hash_table_lookup(v_pnp.changed_inner_handlers, ndev->info.ndas_serial);
    if ( s ) {
        regdev = ndev_lookup_byname(ndev->info.name);
        if (regdev == NULL) {
            debug_pnp(1,"ndasid=%s is pnp subscriber but not yet registered", ndev->info.ndas_id);
            return;
        }
        debug_pnp(1,"calling internal dev change handler");
        /* ndev_pnp_changed */
        s->func(ndev->info.ndas_serial, ndev->info.name, status);
        /* ndev->info.status can be changed by ndev_pnp_changed.
            To do: 
                Changing in status in notification function is confusing.
                Use seperate struct member for pnp packet status and current status..
        */
        
        if (v_pnp.changed_ext_listener) {
            debug_pnp(1,"calling external dev change handler");
            /* ndcmd_dev_changed */
            v_pnp.changed_ext_listener(ndev->info.ndas_serial, ndev->info.name, ndev->info.status);
        } else {
            debug_pnp(1,"No external dev change handler");
        }
        ndev->online_notified = (ndev->info.status == NDAS_DEV_STATUS_HEALTHY)? 1 : 0;

        if (ndev->info.status == NDAS_DEV_STATUS_HEALTHY && ndev->enable_flags) {
            int i;
            for(i=0;i<ndev->info.nr_unit;i++) {
                if (ndev->unit[i]) {
                    int enable_flags;
                    enable_flags = (ndev->enable_flags >> i*2) & REGDATA_BIT_MASK;                    
                    switch ( enable_flags ) {
                    case REGDATA_BIT_SHARE_ENABLED:
                        ndas_enable_writeshare(ndev->unit[i]->info.slot);
                        break;
                    case REGDATA_BIT_WRITE_ENABLED:
                        ndas_enable_exclusive_writable(ndev->unit[i]->info.slot);
                        break;
                    case REGDATA_BIT_READ_ENABLED:
                        ndas_enable_slot(ndev->unit[i]->info.slot);
                        break;
                    }
                }
            }
        } else {
            // Device is not enablable status.
        }
    } else {
        debug_pnp(3,"ndev=%p ndasid=%s status=%d: not PNP subscriber", ndev, ndev->info.ndas_id, status);
    }
}

/*
 * Update the last_tick info
 *
 * THREAD : bpc thread from datagram sio facility
 */
LOCAL void v_update_alive_info(xuchar* network_id, int type, int version)
{
    ndas_error_t err;
    ndev_t* ndev;
    err = ndev_lookup(network_id, &ndev);

    if ( !NDAS_SUCCESS(err) ) {
        sal_error_print("ndas: fail to identify the packet from the NDAS device: %s\n", NDAS_GET_STRING_ERROR(err));
        return;
    }
    
    debug_pnp(6, "ndev-mac=%s ndev=%p",
            SAL_DEBUG_HEXDUMP_S(network_id, SAL_ETHER_ADDR_LEN),ndev);
    
    ndev->info.version = version;
    ndev->last_tick = sal_get_tick();
    
#ifdef XPLAT_PNP
    if ( ndev->info.status == NDAS_DEV_STATUS_OFFLINE ||
         ndev->info.status == NDAS_DEV_STATUS_CONNECT_FAILURE ||
         ndev->info.status == NDAS_DEV_STATUS_UNKNOWN) {
        /* ndev status will be changed in ndas_discover_device */
        ndev_notify_status_changed(ndev, NDAS_DEV_STATUS_HEALTHY);
    }
#endif

    debug_pnp(6, "ed");
}

LOCAL void v_netdisk_alive_info_test_dead_disk(const char *serial)
{
    ndev_t* ndev;
    ndas_error_t err;
    debug_pnp(6, "ing");

    ndev = ndev_lookup_byserial(serial);
    sal_assert(ndev);
    if (ndev->info.status != NDAS_DEV_STATUS_OFFLINE &&
        sal_tick_sub(sal_get_tick() , ndev->last_tick + PNP_ALIVE_TIMEOUT) > 0) {
        debug_pnp(1, "Device %s has gone offline", serial);
        err = ndev_vanished(serial);
        if ( err != NDAS_ERROR_ALREADY_REGISTERED_DEVICE ) {
            ndev_cleanup(ndev);
        }
    }
    return;
}

static struct ndas_probed* v_probed_data = NULL; 
static int v_probed_data_size=0;

/* Assume non-reentrance */
LOCAL void v_prune_dead_disks(void)
{
    int i;
    int size;
    ndas_error_t err;

    debug_pnp(4, "ing");
    size = ndev_probe_size();

    if ( !NDAS_SUCCESS(size) || size == 0) {
        return;
    }
    if (v_probed_data_size<size) {
        if (v_probed_data) 
            sal_free(v_probed_data);
        v_probed_data_size = size;    
        v_probed_data = sal_malloc(sizeof(struct ndas_probed) * size);
    }

    if ( !v_probed_data) {
        debug_pnp(1, "out of memory");
        return;
    }

    err = ndev_probe_serial(v_probed_data, size);

    if ( !NDAS_SUCCESS(err) ) {
        goto out;
    }

    for ( i = 0; i < size; i++) {
        /* Check device is offline */        
        v_netdisk_alive_info_test_dead_disk(v_probed_data[i].serial);
    }
out:    
    debug_pnp(6, "ed");
    return;
}

LOCAL int v_prune_dead_disks_timer(void* param1, void* param2)
{
    int ret;
//    static sal_tick last_checked_time;
    if (!v_pnp.running) {
        if(v_pnp.alive_checker != NULL) {
            bpc_cancel(v_pnp.alive_checker);
            bpc_destroy(v_pnp.alive_checker);
        }
        v_pnp.alive_checker = NULL;
        return 0;
    }

    debug_pnp(3, "ing");

    v_prune_dead_disks();

    ret = bpc_queue(v_pnp.alive_checker, ALIVE_CHECK_INTERVAL);
    if (ret < 0) {
        ndpnp_cleanup();
        // TODO: this will cause memory leak. kill pnp or force schedule
        //sal_assert(id != DPC_INVALID_ID);
        sal_error_print("ndas: pnp thread is killed\n");
    }
    debug_pnp(3, "ed");

    return 0;
}

static
ndas_error_t v_ndpnp_reinit(void) 
{
    ndas_error_t result;
    v_pnp.fd = lpx_socket(LPX_TYPE_DATAGRAM, 0);
    debug_pnp(2, "LPX: socket: %d",v_pnp.fd);
    if (!NDAS_SUCCESS(v_pnp.fd))
    {
        return v_pnp.fd;
    }
    result = lpx_set_rtimeout(v_pnp.fd, 2000);
    if (!NDAS_SUCCESS(result)) {
        
        lpx_close(v_pnp.fd);
        v_pnp.fd = -1;
        return result;
    }
    sal_memset(&v_pnp.slpx, 0, sizeof(v_pnp.slpx));
    v_pnp.slpx.slpx_family = AF_LPX;
    v_pnp.slpx.slpx_port = g_htons(NDDEV_LPX_PORT_NUMBER_PNP);

    result = lpx_bind(v_pnp.fd, &v_pnp.slpx, sizeof(v_pnp.slpx));
    if (!NDAS_SUCCESS(result)) {
        lpx_close(v_pnp.fd);
        v_pnp.fd = -1;
    }
    debug_pnp(2, "LPX: bind: %d", result);   
    return result;
}

static 
ndas_error_t v_ndpnp_init(void) 
{
    int result;
    
    v_pnp.changed_inner_handlers = xlib_hash_table_new(sub_hash, sub_equals);
    if ( !v_pnp.changed_inner_handlers ) {
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    debug_pnp(1, "v_pnp.changed_inner_handlers = %p", v_pnp.changed_inner_handlers);
    
    v_pnp.buf = sal_malloc(v_pnp.buf_sz);
    if ( !v_pnp.buf ) {
        xlib_hash_table_destroy(v_pnp.changed_inner_handlers);
        sal_assert(v_pnp.buf);
        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    result = v_ndpnp_reinit();
    if (!NDAS_SUCCESS(result))
    {
        lpx_close(v_pnp.fd);
        v_pnp.fd = -1;
        sal_free(v_pnp.buf);
        xlib_hash_table_destroy(v_pnp.changed_inner_handlers);
        return result;
    }

    // Use non-auto-free DPC.
    v_pnp.alive_checker = bpc_create(v_prune_dead_disks_timer, NULL,NULL, NULL, DPCFLAG_DO_NOT_FREE);
    if ( v_pnp.alive_checker == DPC_INVALID_ID ) {
        lpx_close(v_pnp.fd);
        sal_free(v_pnp.buf);
        xlib_hash_table_destroy(v_pnp.changed_inner_handlers);
        v_pnp.fd = -1;
        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    result = bpc_queue(v_pnp.alive_checker, ALIVE_CHECK_INTERVAL);
    if ( result < 0 ) {
        lpx_close(v_pnp.fd);
        sal_free(v_pnp.buf);
        xlib_hash_table_destroy(v_pnp.changed_inner_handlers);
        v_pnp.fd = -1;
        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    return NDAS_OK;
}
LOCAL
void pnp_unsubscribe_all(void);

LOCAL void ndpnp_svr(lpx_siocb* cb, struct sockaddr_lpx *from, void* user_arg)
{
//    ndas_error_t result;
    if (!NDAS_SUCCESS(cb->result)) {
        debug_pnp(1, "result=%d", cb->result);
        return;
    }
    if (v_pnp.running==FALSE || v_pnp.buf==NULL) {
        debug_pnp(1, "pnp thread exited");
        return; 
    }
    debug_pnp(4, "PNP message from %s - type %d, ver %d",
            SAL_DEBUG_HEXDUMP(from->slpx_node,6),
            (int)((unsigned char *)cb->buf)[0],
            (int)((unsigned char *)cb->buf)[1]);
    v_update_alive_info(
        from->slpx_node,
        (int)((unsigned char *)cb->buf)[0],
        (int)((unsigned char *)cb->buf)[1]);
}
ndas_error_t ndpnp_reinit(void)
{
    ndas_error_t err;
    if (v_pnp.fd > 0) {
        lpx_close(v_pnp.fd);
        v_pnp.fd = -1;
    }
    err = v_ndpnp_reinit();
    if ( !NDAS_SUCCESS(err) ) {
        sal_assert(0);
        // TODO: retry
        return err;
    }
    err = lpx_sio_recvfrom(v_pnp.fd, v_pnp.buf, v_pnp.buf_sz, 1, ndpnp_svr, NULL, 0);
    if (!NDAS_SUCCESS(err)) {
        sal_assert(0);
        // TODO: retry
    }
    return err;
}

ndas_error_t ndpnp_init(void)
{
    int result;
    debug_pnp(1, "ing");

    result = v_ndpnp_init();
    
    if ( !NDAS_SUCCESS(result)) {
        debug_pnp(1, "ndpnp_init failed");
        goto error_out;
    }

    result = lpx_sio_recvfrom(v_pnp.fd, v_pnp.buf, v_pnp.buf_sz, 1, ndpnp_svr, NULL, 0);
    if (!NDAS_SUCCESS(result)) {
        goto error_out; 
     }
    v_pnp.running = TRUE;
    debug_pnp(3, "ed");
    return NDAS_OK;        
error_out:
    ndpnp_cleanup();
    debug_pnp(3, "ed");
    return result;
}

void ndpnp_cleanup(void) 
{
    dpc_id checker;
    debug_pnp(1, "ing");
    checker= v_pnp.alive_checker;
    if ( checker != DPC_INVALID_ID ) {
        bpc_cancel(checker);
        bpc_destroy(checker);
        v_pnp.alive_checker = DPC_INVALID_ID;
    }
    v_pnp.running = FALSE;
    if (v_pnp.fd > -1) {
        lpx_close(v_pnp.fd);
        v_pnp.fd = -1;
    }
    if (v_pnp.buf) {
        sal_free(v_pnp.buf); 
        v_pnp.buf = NULL;
    }
    if (v_probed_data)  {
        sal_free(v_probed_data);
        v_probed_data = NULL;
        v_probed_data_size = 0;
    }
    pnp_unsubscribe_all();
    xlib_hash_table_destroy(v_pnp.changed_inner_handlers);
    v_pnp.changed_inner_handlers = NULL;
}

xbool pnp_subscribe(const char *serial, ndas_device_handler func) 
{
    struct subscribe *i = sal_malloc(sizeof(struct subscribe ));
    if ( !i ) 
        return FALSE;
    debug_pnp(1, "ing serial=%s", serial);
    sal_strncpy(i->serial, serial, NDAS_SERIAL_LENGTH+1);    
    i->func = func;

    xlib_hash_table_insert(v_pnp.changed_inner_handlers, i->serial, i);
    return TRUE;
}

void pnp_unsubscribe(const char *serial, xbool remove_from_list)
{
    struct subscribe *value = xlib_hash_table_lookup(v_pnp.changed_inner_handlers, serial);
    if ( !value ) {
        debug_pnp(1, "ing failed find serial=%s", serial);
        return;
    }

    if (remove_from_list) {
        xlib_hash_table_remove(v_pnp.changed_inner_handlers, serial);
    }
    sal_free(value);
}

static xbool
_pnp_unsubscribe(void* key, void* value, void* user_data)
{
    pnp_unsubscribe((const char*)key, FALSE);
    return TRUE;
}

LOCAL
void pnp_unsubscribe_all(void) {
    xlib_hash_table_foreach_remove(v_pnp.changed_inner_handlers, _pnp_unsubscribe, NULL);
}

#else 
NDASUSER_API void ndas_set_device_change_handler(ndas_device_handler change_handler)
{
}
#endif /* XPLAT_NDPNP */


