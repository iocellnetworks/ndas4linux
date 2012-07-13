/**********************************************************************
 * Copyright ( C ) 2012 IOCELL Networks
 * All rights reserved.
 **********************************************************************/

#include "xplatcfg.h"
#include "lpx/lpx.h"
#include "lpx/lpxutil.h"
#include "sal/sync.h" // sal_spinlock sal_sema
#include "sal/debug.h" // sal_debug_print
#include "sal/types.h"
#include "xlib/dpc.h"
#include "xlib/xbuf.h" // DEBUG_XLIB_XBUF
#include "xlib/gtypes.h" // g_ntohl g_htonl g_ntohs g_htons
#include "sal/net.h"
#include "sal/mem.h"
#include "sal/libc.h"
#include "sal/time.h"
#include "ndasuser/ndasuser.h"
#include "ndasuser/ndaserr.h"

#include "netdisk/list.h"

#ifdef DEBUG
#include "sal/debug.h" // sal_debug_print

#define debug_itf(l, x...) do { \
    if ( l <= DEBUG_LEVEL_ITF ) { \
        sal_debug_print("LX|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x);\
    }\
} while(0) 
#define debug_lpx(l, x...) do { \
    if ( l <= DEBUG_LEVEL_LPX ) { \
        sal_debug_print("LX|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x);\
    }\
} while(0) 
#define debug_rexmit(l, x...) do {\
    if ( l <= DEBUG_LEVEL_LPX_REXMIT ) { \
        sal_debug_print("LXRX|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x);\
    }\
} while(0) 
#define    debug_lpx_packet(l, x...) do {\
    if(l <= DEBUG_LEVEL_LPX_PACKET ) {\
        sal_debug_print("LXP|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x);\
    }\
} while(0)

#define    debug_lpx_datagram(l, x...) do {\
    if(l <= DEBUG_LEVEL_LPX_DATAGRAM ) {\
        sal_debug_print("LXD|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x);\
    }\
} while(0)
#define    debug_lpx_sio(l, x...) do {\
    if(l <= DEBUG_LEVEL_LPX_SIO ) {\
        sal_debug_print("LXS|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x);\
    }\
} while(0)
#define    LPX_INLINE
#else // DEBUG
#define    debug_itf(l, x...) do {} while(0)
#define    debug_lpx(l, x...) do {} while(0)
#define    debug_rexmit(l, x...) do {} while(0)
#define    debug_lpx_packet(l, x...) do {} while(0) 
#define    debug_lpx_datagram(l, x...) do {} while(0)
#define    debug_lpx_sio(l, x...) do {} while(0)
#define    LPX_INLINE   inline
#endif // DEBUG

#ifdef UCOSII
// UCOSII has tight resource restriction. Wait until destroying socket is completed destroyed before creating new.
#define XPLAT_OPT_THROTTLE_SOCK_CREATE 1
#endif

/*
 * sio_notify_to_user should be called in bpc thread.
 * Otherwise, dpc cause a dead lock if dpc call the direct i/o such as conn_connect
 */
#ifdef XPLAT_SIO
#ifndef XPLAT_BPC
#error nxpo-bpc should be enabled if nxpo-sio is enabled
#endif

/*
    SIO control block creation/destroy
 */
 static
inline
lpx_siocb_i *
_siocb_create(
    struct lpx_sock *sk,
    void *data,
    lpx_sio_handler func,
    void *arg,
    int keep_notifying)
{
    lpx_siocb_i *scb;

    scb = sal_malloc(sizeof(lpx_siocb_i));
    if ( !scb ) 
        return NULL;

    INIT_LIST_HEAD(&scb->node);
    scb->func = func;
    scb->user = arg;
    scb->sock = sk;
    scb->cb.filedes = sk->lpxfd;
    scb->cb.buf = data;
    //acb->cb.nbytes = size;
    scb->cb.result = NDAS_OK; // unknown
    scb->notify_bpc = DPC_INVALID_ID;
    scb->timeout_timer = DPC_INVALID_ID;
    scb->keep_notifying = keep_notifying;
    // Creation reference
    sal_atomic_set(&scb->refcnt, 1);

    return scb;
}

inline
void
_siocb_inc_ref(
    lpx_siocb_i *scb
){
    sal_assert(sal_atomic_read(&scb->refcnt) >= 1);

    sal_atomic_inc(&scb->refcnt);
}

static
void
_siocb_do_destroy(
    lpx_siocb_i *scb
){
    debug_lpx_sio(1, "scb=%p notify=%p",scb, scb->notify_bpc);
    // BPC and DPC Must be cancelled before destruction.
    if(scb->notify_bpc != DPC_INVALID_ID) {
#ifdef DEBUG
        sal_assert(bpc_cancel(scb->notify_bpc) != NDAS_OK);
#endif
        bpc_destroy(scb->notify_bpc);
        scb->notify_bpc = DPC_INVALID_ID;
    }
    if(scb->timeout_timer != DPC_INVALID_ID) {
#ifdef DEBUG
        sal_assert(dpc_cancel(scb->timeout_timer) != NDAS_OK);
#endif

        dpc_destroy(scb->timeout_timer);
        scb->timeout_timer = DPC_INVALID_ID;
    }
    sal_free(scb);
}

inline
void
_siocb_destroy(
    lpx_siocb_i *scb
){
    int lastref;
    sal_assert(sal_atomic_read(&scb->refcnt) >= 1);

    lastref = sal_atomic_dec_and_test(&scb->refcnt);
    if(lastref) {
        _siocb_do_destroy(scb);
    }
}

#endif
/* 
    Local variables 
*/
#define LPX_SOCKET_HASH_SIZE    11

LOCAL lpx_interface    *lpx_interfaces    = NULL;
LOCAL struct lpx_portinfo        lpx_port_binder;
LOCAL struct lpx_eth_cache        lpx_dest_cache;
LOCAL sal_spinlock                lpx_socks_lock; /* Note accessed from interrupt context. Use spinlock */
LOCAL struct list_head            lpx_socks[LPX_SOCKET_HASH_SIZE];
LOCAL sal_atomic        lpx_sock_count = sal_atomic_init(0);
LOCAL int                next_lpx_sock_fd = 1;
LOCAL int    lpx_max_transfer_size; /* in bytes */
#ifdef XPLAT_OPT_THROTTLE_SOCK_CREATE
LOCAL int v_destroy_pending_count=0;
#endif

#define lpxitf_mss(sk) ((LPX_OPT(sk)->interface)?LPX_OPT(sk)->interface->mss:1514)

/*
    Forward declarations 
*/    
LOCAL ndas_error_t lpxitf_destroy(lpx_interface     *interface);
LOCAL void    lpx_bind_table_init(void);
LOCAL void    lpx_dest_cache_init(void);
LOCAL void         lpxitf_insert(lpx_interface *interface);
LOCAL LPX_INLINE void lpxitf_demux_sock(lpx_interface *interface, struct xbuf *xb);

LOCAL ndas_error_t dgm_transmit_packet(struct lpx_sock *sk, struct sockaddr_lpx *usaddr, char* data, int len);

//LOCAL void lpx_link_status_change_handler(sal_netdev_desc nd, int link_status);
NDAS_CALL LOCAL void lpx_net_rx_handler(sal_netdev_desc nd, sal_net_buff nbuf, sal_net_buf_seg* bufs);

//LOCAL void lpxitf_up(lpx_interface *interface);
//LOCAL void lpxitf_down(lpx_interface *interface);

LOCAL void lpxitf_remove(lpx_interface *interface);

LOCAL LPX_INLINE void lpxitf_rcv(struct xbuf *xb, sal_netdev_desc nd);
LOCAL void lpx_sock_queue_rcv_xbuf(struct lpx_sock* sk, struct xbuf* xb);

LOCAL void lpx_sock_state_change(struct lpx_sock *sk);
LOCAL void lpx_sock_data_ready(struct lpx_sock* sk, int len);
LOCAL void lpx_sock_destroy(struct lpx_sock* sk);

/* LPX stream functions */
LOCAL LPX_INLINE void stm_hold(struct lpx_sock *sk);
LOCAL LPX_INLINE void stm_release(struct lpx_sock *sk, int creationref);

/* LPX datagram functions */

LOCAL LPX_INLINE void dgm_hold(struct lpx_sock *sk);
LOCAL LPX_INLINE void dgm_release(struct lpx_sock *sk, int creationref);

/* dest cache function */
LOCAL struct lpx_cache *        lpx_find_dest_cache(struct lpx_sock *sk);
LOCAL void                lpx_update_dest_cache(struct lpx_sock * sk);
LOCAL int             lpx_dest_cache_timer(void* p1, void* p2);

/*  port-binding related function */
LOCAL void                lpx_bind_table_init(void);
LOCAL void                lpx_bind_table_down(void);
LOCAL void                lpx_dest_cache_init(void);
LOCAL void                lpx_dest_cache_down(void);
LOCAL void                lpx_dest_cache_disable(unsigned char * node);
LOCAL struct lpx_sock *    lpx_find_port_bind(unsigned short port, int inc_refcnt);
LOCAL unsigned short    lpx_get_free_port_num(void);
LOCAL void                lpx_reg_bind(struct lpx_sock * sk);
LOCAL void                lpx_unreg_bind(struct lpx_sock * sk);
#define lpx_bind_hash(port) ((port)%PORT_HASH)


/* dgram functions */
LOCAL void dgm_rcv(struct lpx_sock *sk, struct xbuf *xb);
LOCAL ndas_error_t dgm_create(struct lpx_sock *sk, int protocol);

/* stream functions */
LOCAL LPX_INLINE void stm_rcv(struct lpx_sock *sk, struct xbuf *xb);
LOCAL ndas_error_t stm_create(struct lpx_sock *sk, int protocol);
LOCAL LPX_INLINE ndas_error_t stm_do_rcv(struct lpx_sock* sk, struct xbuf *xb);
LOCAL void stm_destroy_sock(struct lpx_sock *sk);
LOCAL int lpx_stream_snd_test(struct lpx_sock *sk, struct xbuf* xb);

/* Misc functoins */
LOCAL int                lpx_is_valid_addr(struct lpx_sock * sk,unsigned char * node);

/************************** 
    xbuf related functions 
*************************/

/* 
 * len: length of lpx data (excludes lpx_header & mac header 
 * Assume sk is locked
 */

LOCAL inline struct xbuf* lpx_sock_alloc_xb(struct lpx_sock *sk, int data_len, int noblock)
{
    struct xbuf* xb;

#ifdef USE_XBUF_FROM_NET_BUF_AT_TX
    {
        sal_net_buff nbuf;
        sal_net_buf_seg segbuf;
        nbuf = sal_net_alloc_platform_buf_ex(sk->system_sock, LPX_HEADER_SIZE, data_len, &segbuf, noblock);
        if ( !nbuf ) {
            debug_lpx(1,"sal_net_alloc_platform_buf_ex() data_len=%d noblock=%d failed.", data_len, noblock);
            return NULL;
        }
        xb = xbuf_alloc_from_net_buf(nbuf, &segbuf);
        if ( !xb ) {
            sal_net_free_platform_buf(nbuf);
            debug_lpx(1,"xbuf_alloc_from_net_buf() failed.");
            return NULL;
        }
    }
    sal_assert(xb->sys_buf);
#else
    xb = xbuf_alloc(data_len + LPX_HEADER_SIZE);
    if ( !xb ) {
        debug_lpx(1,"xbuf_alloc() failed.");
        return NULL;
    }
    xbuf_reserve_header(xb, LPX_HEADER_SIZE);
#endif
    xb->owner = (void*)sk;
    sal_assert(xb->owner);
    return xb;    
}

LOCAL inline
ndas_error_t
lpx_sock_rebuild_xb_netbuf(struct lpx_sock *sk, struct xbuf *xb, int noblock)
{

#ifdef USE_XBUF_FROM_NET_BUF_AT_TX
    {
        sal_net_buff nbuf;
        sal_net_buf_seg segbuf;
        int ret;

        sal_assert(xb->sys_buf == NULL);
        ret = xbuf_get_netbuf_seg(xb, &segbuf);
        if(ret < 0) {
            return NDAS_ERROR;
        }
        // Allocate system network buffer with net buf segments.
        // The function will return network header pointers.
        nbuf = sal_net_alloc_buf_bufseg(
            sk->system_sock,
            &segbuf);
        if ( !nbuf ) {
            debug_lpx(1,"sal_net_alloc_buf_bufseg() failed.");
            return NDAS_ERROR;
        }

        // Set network header pointers
        xb->mach = segbuf.mac_header.ptr;
        xb->nh = segbuf.network_header.ptr;
        xb->sys_buf = nbuf;

    }
#endif
    return NDAS_OK;
}

/**************************
    lpx_sock related functions 
***************************/

LOCAL inline
int lpx_hash_sockfd(int sockfd) {
    return (sockfd % LPX_SOCKET_HASH_SIZE);
}


LOCAL struct lpx_sock* lpx_get_sock_from_fd(int fd)
{
    struct list_head *i;

    sal_spinlock_take_softirq(lpx_socks_lock);
    list_for_each(i, &lpx_socks[lpx_hash_sockfd(fd)]) 
    {
        struct lpx_sock *sk = list_entry(i,struct lpx_sock, all_node);
        sal_assert(sk);
        if (sk->lpxfd == fd) {
            sal_spinlock_give_softirq(lpx_socks_lock);
            return sk;
        }
    }
    sal_spinlock_give_softirq(lpx_socks_lock);
    debug_lpx(1, "Invalid lpx fd: %d", fd);

    return NULL;
}


LOCAL inline void lock_sock(struct lpx_sock* sk, unsigned long* flags)
{
    *flags = 0;
    sal_spinlock_take_softirq(sk->lock);
}

LOCAL inline void unlock_sock(struct lpx_sock* sk, unsigned long* flags)
{
    sal_spinlock_give_softirq(sk->lock);
}

LOCAL ndas_error_t lpx_sock_error(struct lpx_sock* sk)
{
    ndas_error_t err;
    err = sk->err; 
    sk->err = NDAS_OK;
    return err;
}

static inline void lpx_sock_inc_ref(struct lpx_sock* sk)
{
    sal_atomic_inc(&sk->refcnt);
}

/* Return 1 if decreased value is 0 */
static inline int lpx_sock_dec_ref(struct lpx_sock* sk)
{
    return sal_atomic_dec_and_test(&sk->refcnt);
}

static inline int lpx_sock_read_ref(struct lpx_sock* sk)
{
    return sal_atomic_read(&sk->refcnt);
}

/************************
    lpx protocol 
**************************/
#ifdef STAT_REXMIT
#define REXMIT_INTERVAL (30 * SAL_TICKS_PER_SEC)
static int rexmit_bad_count = 0;
static int rexmit_good_count = 0;
LOCAL int lpx_rexmit_stat(void *a, void *b) 
{
    dpc_id dpcid;
    int ret;

    sal_error_print("rexmit stat:%d/%d\n", rexmit_bad_count, rexmit_good_count);
    dpcid = dpc_create(DPC_PRIO_NORMAL, lpx_rexmit_stat, NULL, NULL, NULL, 0);
    if(dpcid) {
        ret = dpc_queue(dpcid, REXMIT_INTERVAL);
        if(ret < 0) {
            dpc_destroy(dpcid);
        }

    return 0;
}
#endif

LOCAL ndas_error_t lpx_proto_init(void)
{
#ifdef STAT_REXMIT
    dpc_id dpcid;
    int ret;
#endif
    lpx_bind_table_init();
    lpx_dest_cache_init();
#ifdef STAT_REXMIT
    dpcid = dpc_create(DPC_PRIO_NORMAL, lpx_rexmit_stat, NULL, NULL, NULL, 0);
    if(dpcid) {
        ret = dpc_queue(dpcid, REXMIT_INTERVAL);
        if(ret < 0) {
            dpc_destroy(dpcid);
        }
    }
#endif
    return NDAS_OK;
}

LOCAL void lpx_proto_finito(void)
{
    lpx_interface    *ifc;
    lpx_interface    *next_ifc;
    debug_lpx(3, "ing");
    next_ifc = lpx_interfaces;
    while(next_ifc) {
        ifc = next_ifc;
        next_ifc     = ifc->itf_next;
        lpxitf_destroy(ifc);        
    }

    lpx_dest_cache_down();
    lpx_bind_table_down();
    debug_lpx(3, "ed");
    return;
}

LOCAL inline lpx_interface *lpxitf_find_using_devname(const char* dev_name)
{
    lpx_interface    *i;
    for(i = lpx_interfaces; i && (sal_strcmp(dev_name, i->dev)!=0); i = i->itf_next);
    return i;
}

LOCAL ndas_error_t lpxitf_create(const char *dev_name)
{
    lpx_interface     *itf;
    sal_netdev_desc    nd;
    int ret;
    debug_lpx(3, "ing dev_name=%s", dev_name);

    if(lpxitf_find_using_devname(dev_name) != NULL)
        return NDAS_ERROR_EXIST;

    nd = sal_netdev_open(dev_name);

    if (nd == SAL_INVALID_NET_DESC) {
/*        sal_debug_print("Failed open device %s\r\n", dev_name); */
        return NDAS_ERROR_NO_DEVICE;
    }

    itf = (lpx_interface *)sal_malloc(sizeof(lpx_interface));
    if(itf == NULL) {
        ret =  NDAS_ERROR_OUT_OF_MEMORY;
        goto out1;
    }

    itf->nd    = nd;
    itf->itf_sklist = NULL;
    ret = sal_spinlock_create("itf-sk-lock", &itf->itf_sklock);
    if (!ret) {
        ret = NDAS_ERROR_OUT_OF_MEMORY;
        goto out2;
    }
    
    sal_strcpy(itf->dev, dev_name);
    sal_netdev_get_address(nd, itf->itf_node);
    itf->mss = sal_netdev_get_mtu(nd) - sizeof(struct lpxhdr);
    sal_assert(itf->mss + sizeof(struct lpxhdr)>= MIN_MTU_SIZE); /* We assume MTU is large enough to hold two disk sector */
    itf->hard_header_len = SAL_ETHER_HEADER_SIZE;
        
    lpxitf_insert(itf);

    ret = sal_netdev_register_proto(nd, g_htons(ETH_P_LPX), lpx_net_rx_handler);

    if (ret != 0)  {
        sal_debug_print("sal_netdev_register_proto failed");
        ret = NDAS_ERROR_PROTO_REGISTRATION_FAIL;
        goto out3;
    }
    return NDAS_OK;
out3:    
    lpxitf_remove(itf);
out2: 
    sal_free(itf);
out1:    
    sal_netdev_close(nd);    
    return ret;
}
/**
 * lpxitf_destroy - destroy the interface for lpx protocol.
 */
LOCAL ndas_error_t lpxitf_destroy(lpx_interface     *interface) 
{
    struct lpx_sock *s;
    ndas_error_t ret = NDAS_OK;
    unsigned long flags;
    debug_lpx(1, "ing interface=%p", interface);
    sal_netdev_unregister_proto(interface->nd, g_htons(ETH_P_LPX));// TODO error handling

    lpx_dest_cache_disable(interface->itf_node);
    lpxitf_remove(interface);

    for(s = interface->itf_sklist; s != NULL; s=s->next) {
        lock_sock(s, &flags);
        LPX_OPT(s)->interface = NULL;
        unlock_sock(s, &flags);
    }

    sal_netdev_close(interface->nd);
    sal_spinlock_destroy(interface->itf_sklock);
    sal_free(interface);
    debug_lpx(1, "ed");
    return ret;;
}

LOCAL void    lpx_dest_cache_init(void)
{
    int ret;
    debug_lpx(5, "lpx_dest_cache_init");

    sal_spinlock_create("cache_lock", &lpx_dest_cache.cache_lock);

    lpx_dest_cache.head = NULL;
    lpx_dest_cache.cache_timer = dpc_create(
        DPC_PRIO_NORMAL,
        lpx_dest_cache_timer,
        (void*)&lpx_dest_cache,
        NULL,
        NULL,
        DPCFLAG_DO_NOT_FREE);
    if(lpx_dest_cache.cache_timer == DPC_INVALID_ID) {
        sal_error_print("ndas: failed to create cache timer.\n");
        return;
    }
    ret = dpc_queue(lpx_dest_cache.cache_timer, CACHE_TIMEOUT);
    if(ret < 0) {
        sal_error_print("ndas: failed to queue cache timer.\n");
        return;
    }
}

void
lpx_dest_cache_down(void)
{
    struct lpx_cache *cache, *p_tmp;
    debug_lpx(4, "lpx_dest_cache_down");

    // Stop the periodic timer
    dpc_cancel(lpx_dest_cache.cache_timer);
    dpc_destroy(lpx_dest_cache.cache_timer);
    lpx_dest_cache.cache_timer = DPC_INVALID_ID;

    sal_spinlock_take_softirq(lpx_dest_cache.cache_lock);

    cache = lpx_dest_cache.head;
    while(cache)
    {
        p_tmp = cache;
        cache = cache->next;
        sal_free(p_tmp);
    }
    lpx_dest_cache.head = NULL;

    sal_spinlock_give_softirq(lpx_dest_cache.cache_lock);

    sal_spinlock_destroy(lpx_dest_cache.cache_lock);

}


LOCAL inline lpx_interface *lpxitf_find_using_netdesc(sal_netdev_desc nd)
{
    lpx_interface    *i;
    for(i = lpx_interfaces; i && (i->nd!=nd); i = i->itf_next);
    return i;
}

#ifdef XPLAT_NDASHIX
/* For UUID generation */
sal_netdev_desc lpxitf_find_first() {
    return lpx_interfaces ? lpx_interfaces->nd : (sal_netdev_desc)NULL;
}

int lpxitf_iterate_address(void (*func)(xuchar*,void*), void* arg) 
{
    lpx_interface    *i;
    int count = 0;
    debug_lpx(1, "ing func=%p", func);
    for(i = lpx_interfaces; i ; i = i->itf_next) {
        func(i->itf_node,arg);
        count ++;
    }
    return count;
}
#endif
/**
 * insert the interface into list
 */
LOCAL void lpxitf_insert(lpx_interface *interface)
{
    lpx_interface *i;

    debug_lpx(4, "lpxitf_insert");

    interface->itf_next = NULL;
    if(lpx_interfaces == NULL)
        lpx_interfaces = interface;
    else {
        for(i = lpx_interfaces; i->itf_next != NULL; i = i->itf_next);
        i->itf_next = interface;
    }
    return;
}
/**
 * remove interface from the list
 */
LOCAL void lpxitf_remove(lpx_interface *interface)
{
    lpx_interface *i;
    if(interface == lpx_interfaces)
        lpx_interfaces = interface->itf_next;
    else {
        for(i = lpx_interfaces;
            (i != NULL) && (i->itf_next != interface);
            i = i->itf_next);

        if((i != NULL) && (i->itf_next == interface))
            i->itf_next = interface->itf_next;
    }
}

/*
Can be run in interrupt context. Do not sleep.
*/
NDAS_CALL LOCAL void lpx_net_rx_handler(sal_netdev_desc nd, sal_net_buff nbuf, sal_net_buf_seg* bufs)
{
    struct xbuf* xb;
    struct lpxhdr    *lpxhdr;
    int lpxsize;

    debug_lpx(6, "nd=%p nbuf=%p bufs=%p", nd, (void*) nbuf, bufs);

    if(bufs->data_len < LPX_HEADER_SIZE) {
        sal_net_free_platform_buf(nbuf);
        debug_lpx(1, "too small payload %d", bufs->data_len);
        return;
    }
    sal_assert(bufs->nr_data_segs == 1);
    if (bufs->network_header.len == 0) {
        /* setup network ( LPX ) part */
        bufs->network_header.len = LPX_HEADER_SIZE;
        bufs->network_header.ptr = bufs->data_segs[0].ptr;

        sal_assert(bufs->data_segs[0].len >= LPX_HEADER_SIZE);

        bufs->data_segs[0].ptr = (xpointer)(((char*)bufs->data_segs[0].ptr) + LPX_HEADER_SIZE);
        bufs->data_segs[0].len -= LPX_HEADER_SIZE;
        bufs->data_len -= LPX_HEADER_SIZE;
    }

    // LPX packet length validation
    lpxhdr = (struct lpxhdr *)bufs->network_header.ptr;
    debug_itf(6, "lpxhdr=%p",lpxhdr);
    debug_itf(8, "lpxhdr=%s",DEBUG_LPX_HDR(lpxhdr));
    lpxsize = g_ntohs(lpxhdr->pu.pktsize & ~LPX_TYPE_MASK);
    if(lpxsize < LPX_HEADER_SIZE ||
       lpxsize > LPX_HEADER_SIZE + bufs->data_segs[0].len)
    {
        sal_net_free_platform_buf(nbuf);
        debug_itf(1, "lpx size is incorrect, lpxhdr = %d pkt = %d payload len = %d",
            (int)LPX_HEADER_SIZE, lpxsize, (int)bufs->data_segs[0].len);
        return;
    }

    // cut off payload padding of ethernet drivers.
    bufs->data_segs[0].len = lpxsize - LPX_HEADER_SIZE;

    xb = xbuf_alloc_from_net_buf(nbuf,bufs);
    if (xb) {
        debug_itf(6, "From %s:%4X To: %s:%4X",
                    SAL_DEBUG_HEXDUMP_S(XBUF_MAC_HEADER(xb)->src, LPX_NODE_LEN),
                    g_ntohs(lpxhdr->source_port),
                    SAL_DEBUG_HEXDUMP_S(XBUF_MAC_HEADER(xb)->des, LPX_NODE_LEN),
                    g_ntohs(lpxhdr->dest_port)
            );
        lpxitf_rcv(xb, nd);
    } else {
        sal_net_free_platform_buf(nbuf);
        debug_lpx(1, "Failed to allocate xbuf.");
    }
    debug_lpx(6, "ed");
}

LOCAL lpx_interface *lpxitf_find_using_node(unsigned char *node)
{
    lpx_interface    *i;
    unsigned char    zero_node[LPX_NODE_LEN] = {0, 0, 0, 0, 0, 0};

    for(i = lpx_interfaces; i && sal_memcmp(i->itf_node, node, LPX_NODE_LEN); i = i->itf_next);

    if(i == NULL && sal_memcmp(zero_node, node, LPX_NODE_LEN) == 0) {
        i = lpx_interfaces;
    }
    
    return i;
}

LOCAL void lpxitf_insert_sock(lpx_interface *interface, struct lpx_sock *sk)
{
    struct lpx_sock *s;
    debug_lpx(4, "lpxitf_insert_sock");

    LPX_OPT(sk)->interface = interface;
    sk->next = NULL;

    sal_spinlock_take_softirq(interface->itf_sklock);
    if(interface->itf_sklist == NULL) {
        interface->itf_sklist = sk;
    } else {
        for (s = interface->itf_sklist; s->next != NULL; s = s->next);

        s->next = sk;
    }
    sal_spinlock_give_softirq(interface->itf_sklock);
    
    sk->zapped = 0;
    
    return;
}


LOCAL void lpxitf_remove_sock(struct lpx_sock *sk)
{
    struct lpx_sock     *s;
    lpx_interface     *interface;
    debug_itf(2, "ing sk=%p",sk);

    /* Unbind the socket from the port number */
    lpx_unreg_bind(sk);

    interface = LPX_OPT(sk)->interface;
    LPX_OPT(sk)->interface = NULL;
    sk->zapped = 1;

    if(interface == NULL) {
        debug_itf(4, "ed interface is destroyed");
        return;
    }
    debug_itf(4, "interface=%p", interface);
    sal_spinlock_take_softirq(interface->itf_sklock);
    s = interface->itf_sklist;
    /* If the socket is placed in the head. */
    if(s == sk) {
        interface->itf_sklist = s->next;
        sal_spinlock_give_softirq(interface->itf_sklock);
        return;
    }
    /* Loop until found */
    while(s && s->next) {
        if(s->next == sk) {
            s->next = sk->next;
            debug_itf(4, "ed s=%p",s);
            sal_spinlock_give_softirq(interface->itf_sklock);
            return;
        }
        s = s->next;
    }
    sal_spinlock_give_softirq(interface->itf_sklock);
    debug_itf(2, "ed no match");
    sal_assert(0); // fix me
}


LOCAL int lpx_dest_cache_timer(void* data, void* p2)
{
    struct lpx_cache * cache, *p_cache;
    sal_tick cur_time = sal_get_tick();
    int ret;
    debug_lpx(6,"lpx_dest_cache_timer");

    sal_spinlock_take_softirq(lpx_dest_cache.cache_lock);
    cache = lpx_dest_cache.head ;
    p_cache = NULL;

    while(cache)
    {
        if( sal_tick_sub(cur_time, cache->time) > CACHE_ALIVE )
        {
            if(NULL == p_cache) {
                lpx_dest_cache.head = cache->next;
                sal_free(cache);
                cache = lpx_dest_cache.head;
            }
            else {
                p_cache->next = cache->next;
                sal_free(cache);
                cache = p_cache->next;
            }
            continue;
        }
        p_cache = cache;
        cache = cache->next;

    }
    sal_spinlock_give_softirq(lpx_dest_cache.cache_lock);

    // Queue the DPC again.
    ret = dpc_queue(lpx_dest_cache.cache_timer, CACHE_TIMEOUT);
    if(ret < 0) {
        sal_error_print("ndas: failed to queue cache timer\n");
    }

    return 0;
}


// check validation of portnumber and seach interface
LOCAL int    lpx_is_valid_addr(struct lpx_sock * sk, unsigned char * node)
{
    lpx_interface    * i;
    unsigned char zero_node[LPX_NODE_LEN] = {0,0,0,0,0,0};
    
    debug_lpx(5, "lpx_is_valid_addr");

    if (sal_memcmp(zero_node, node, LPX_NODE_LEN) == 0) {
        //
        // Zero node address means any interface.
        // Check this before iterating interface because some uninitialized interface has zero address
        //
        LPX_OPT(sk)->virtual_mapping = 1;
        LPX_OPT(sk)->interface = NULL;
        sal_memcpy(LPX_OPT(sk)->source_addr.node, zero_node, LPX_NODE_LEN);
        return 1;
    } 

    for(i = lpx_interfaces; i && sal_memcmp(i->itf_node,node,LPX_NODE_LEN) ; i = i->itf_next);

    if(i){
        LPX_OPT(sk)->interface = i;
        sal_memcpy(LPX_OPT(sk)->source_addr.node,i->itf_node, LPX_NODE_LEN);
        return 1;
    } else {
        return 0;
    }
}

/*--------------------------------------------------
    Port number management
  --------------------------------------------------*/
LOCAL void    lpx_bind_table_init(void)
{
    int i;

    debug_lpx(5, "lpx_bind_table_init");

    lpx_port_binder.last_alloc_port = 0;

    sal_spinlock_create("bind_lock", &lpx_port_binder.bind_lock );

    for(i = 0; i < PORT_HASH ; i++)
    {
        lpx_port_binder.port_hashtable[i].bind_next = NULL;
    }
}

LOCAL void    lpx_bind_table_down(void)
{
    sal_spinlock_destroy(lpx_port_binder.bind_lock);
}

/* return socket binded with port */
LOCAL struct lpx_sock * lpx_find_port_bind( unsigned short port, int inc_refcnt)
{
    struct lpx_bindhead * head;
    struct lpx_sock * bind_sk = NULL;
    int found = 0;
    debug_lpx(9, "lpx_find_port_bind:port=%d", port);

    //
    // Assuming that a thread in hardware interrupt context never come to LPX.
    //
    sal_spinlock_take_softirq(lpx_port_binder.bind_lock);

    head = &lpx_port_binder.port_hashtable[lpx_bind_hash(port)];

    for(bind_sk = head->bind_next; bind_sk; bind_sk = bind_sk->bind_next) {
        if(LPX_OPT(bind_sk)->source_addr.port == port) {
            found = 1;
            break;
        }
    }

    // Increase the socket reference count if needed
    if(found)
    {
        if(inc_refcnt) {
            switch(bind_sk->type)
            {
                case LPX_TYPE_STREAM:
                    stm_hold(bind_sk);
                    break;
                case LPX_TYPE_DATAGRAM:
                    dgm_hold(bind_sk);
                    break;
                case LPX_TYPE_RAW:
                    sal_assert(0);
                    break;
                default:
                    sal_assert(0);
                    break;
            }
        }
    } else {
        bind_sk = NULL;
    }

    sal_spinlock_give_softirq(lpx_port_binder.bind_lock);

    debug_lpx(9, "lpx_find_port_bind:bound =%p", bind_sk);
    return bind_sk;
}

// return port number is free
LOCAL unsigned short    lpx_get_free_port_num(void)
{
    unsigned int low = LPX_MIN_EPHEMERAL_SOCKET;
    unsigned int high = LPX_MAX_EPHEMERAL_SOCKET;
    unsigned int remaining = (high - low) +1 ;
    unsigned int port;
    unsigned int port_exists;
    struct lpx_bindhead * head;
    struct lpx_sock * bind_sk;
    int ret = 0;
    
    debug_lpx(5, "lpx_get_free_port_num");
    sal_spinlock_take_softirq(lpx_port_binder.bind_lock);
    port = lpx_port_binder.last_alloc_port;

    // search availabe port number
    do{
        port++;
        if ((port < low) || (port > high)) port = low;
        head = &lpx_port_binder.port_hashtable[lpx_bind_hash(port)];
        port_exists = FALSE;
        for(bind_sk = head->bind_next; bind_sk; bind_sk = bind_sk->bind_next) {
            if(LPX_OPT(bind_sk)->source_addr.port == port) {
                port_exists = TRUE;
                break;
            }
        }
        if (!port_exists) { // proper port found
            lpx_port_binder.last_alloc_port = port;
            ret = port;
            break;
        }
    } while ( --remaining > 0);

    sal_spinlock_give_softirq(lpx_port_binder.bind_lock);
    return ret;
}



//  insert a socket into port bind table
LOCAL void lpx_reg_bind(struct lpx_sock * sk)
{    
    struct lpx_bindhead * head;
    unsigned short port;
    port = LPX_OPT(sk)->source_addr.port;
    debug_lpx(1, "lpx_reg_bind sk=%p, fd=%d, port=%d", sk, sk->lpxfd, port);

    sal_spinlock_take_softirq(lpx_port_binder.bind_lock);

    head = &lpx_port_binder.port_hashtable[lpx_bind_hash(port)];

    sk->bind_next = head->bind_next;
    head->bind_next = sk;

    //
    // Increase the socket's reference count for the binding.
    //
    switch(sk->type)
    {
        case LPX_TYPE_STREAM:
            stm_hold(sk);
            break;
        case LPX_TYPE_DATAGRAM:
            dgm_hold(sk);
            break;
        case LPX_TYPE_RAW:
            sal_assert(0);
            break;
        default:
            sal_assert(0);
            break;
    }

    sal_spinlock_give_softirq(lpx_port_binder.bind_lock);
}

// remove a socket from port binf table
LOCAL void    lpx_unreg_bind(struct lpx_sock * sk)
{
    struct lpx_bindhead * head;
    unsigned short port;
    struct lpx_sock * s;
    int found = 0;

    debug_lpx(3, "lpx_unreg_bind: fd=%d, port=%d", sk->lpxfd, LPX_OPT(sk)->source_addr.port);
    port = LPX_OPT(sk)->source_addr.port;

    sal_spinlock_take_softirq(lpx_port_binder.bind_lock);

    head = &lpx_port_binder.port_hashtable[lpx_bind_hash(port)];

    s = head->bind_next;

    if(s == sk)
    {
            head->bind_next = sk->bind_next;
            sk->bind_next = NULL;
            found = 1;
            goto end;
    }

    while(s && s->bind_next)
    {
        if(s->bind_next == sk){
            s->bind_next = sk->bind_next;
            sk->bind_next = NULL;
            found = 1;
            goto end;
        }
        s = s->bind_next;
    }

    // This is not error case if sock is listening sock 
    debug_lpx(4, "lpx_unreg_bind: sk %p(type=%x) is not found on bind table", sk, sk->type);
//    sal_assert(0);
end:
    if(found) {
        //
        // Decrease the socket's reference count for the binding.
        //
        switch(sk->type)
        {
            case LPX_TYPE_STREAM:
                stm_release(sk, 0);
                break;
            case LPX_TYPE_DATAGRAM:
                dgm_release(sk, 0);
                break;
            case LPX_TYPE_RAW:
                sal_assert(0);
                break;
            default:
                sal_assert(0);
                break;
        }
    }
    sal_spinlock_give_softirq(lpx_port_binder.bind_lock);
}

// dest addr cache mechanism
LOCAL struct lpx_cache *lpx_find_dest_cache(struct lpx_sock *sk)
{

    struct lpx_cache * cache;

    debug_lpx(5, "lpx_find_dest_cache");

    sal_spinlock_take_softirq(lpx_dest_cache.cache_lock);

    for(cache = lpx_dest_cache.head; cache; cache = cache->next) {
        if(!sal_memcmp(cache->dest_addr,LPX_OPT(sk)->dest_addr.node,LPX_NODE_LEN))
        {
            sal_memcpy(LPX_OPT(sk)->source_addr.node,cache->itf_addr,LPX_NODE_LEN);
            break;
        }
    }

    sal_spinlock_give_softirq(lpx_dest_cache.cache_lock);

    return cache;
}



//    cache update function
LOCAL void lpx_update_dest_cache(struct lpx_sock * sk)
{
    struct lpx_cache * cache;
    debug_lpx(4, "lpx_update_dest_cache");
    cache = lpx_find_dest_cache(sk);
    if(!cache){
        cache = sal_malloc(sizeof(struct lpx_cache));
        if(!cache) {
            debug_lpx(1,"Can't allocate memory");
            return;
        }

        cache->time = sal_get_tick();

        sal_spinlock_take_softirq(lpx_dest_cache.cache_lock);

        cache->next = lpx_dest_cache.head;
        lpx_dest_cache.head = cache;

    } else {
        sal_spinlock_take_softirq(lpx_dest_cache.cache_lock);
    }

    sal_memcpy(cache->dest_addr, LPX_OPT(sk)->dest_addr.node, LPX_NODE_LEN);
    sal_memcpy(cache->itf_addr, LPX_OPT(sk)->source_addr.node, LPX_NODE_LEN);

    sal_spinlock_give_softirq(lpx_dest_cache.cache_lock);

    return;
}    


LOCAL void    lpx_dest_cache_disable(unsigned char * node)
{
    struct lpx_cache *cur_cache, *prev_cache;

    debug_lpx(4, "lpx_dest_cache_disable");

    sal_spinlock_take_softirq(lpx_dest_cache.cache_lock);

    cur_cache = lpx_dest_cache.head;
    prev_cache = NULL;
    while(cur_cache)
    {
        if(!sal_memcmp(cur_cache->itf_addr,node,LPX_NODE_LEN))
        {
            // remove the current entry from the link of the previous entry.
            if(prev_cache == NULL) {
                lpx_dest_cache.head = cur_cache->next;
                // Free the memory
                sal_free(cur_cache);
                cur_cache = lpx_dest_cache.head;
            }
            else {
                prev_cache->next = cur_cache->next;
                // Free the memory
                sal_free(cur_cache);
                cur_cache = prev_cache->next;
            }
        } else {
            prev_cache = cur_cache;
            cur_cache = cur_cache->next;
        }
    }

    sal_spinlock_give_softirq(lpx_dest_cache.cache_lock);
}

LOCAL int lpxitf_send(lpx_interface *interface, struct xbuf *xb, xuchar *node)
{
    sal_netdev_desc nd = interface->nd;
    int ret;
    sal_net_buff pbuf;
    int totlen;
#ifndef USE_XBUF_FROM_NET_BUF_AT_TX
    sal_net_buf_seg segbuf;
#endif

    debug_itf(4, "ing xb=%p", xb);
    debug_itf(4, "dest node=%s",SAL_DEBUG_HEXDUMP_S(node,6));
    debug_itf(8, "lpx_sock=%s",DEBUG_LPX_SOCK((struct lpx_sock *)xb->owner));
    sal_assert(xb!=NULL);
    sal_assert(node!=NULL);
    sal_assert(interface!=NULL);
#if 0
    {
        static int packetdrop_test;

        packetdrop_test++;
        if(xbuf_is_sys_netbuf_rebuild(xb) && (packetdrop_test%10) == 0) {
            debug_itf(3, "%d packet drop test. xb=%p", packetdrop_test, xb);
            sal_net_free_platform_buf(xb->sys_buf);
            xb->sys_buf = NULL;
            xb->mach = NULL;
            xb->nh = NULL;
            xbuf_free(xb);
            return NDAS_OK;
        }
    }
#endif

#ifdef USE_XBUF_FROM_NET_BUF_AT_TX
    sal_assert(xb->sys_buf);
    totlen = LPX_HEADER_SIZE + XBUF_NETWORK_DATA_LEN(xb);
    // Detach system network buffer from the xbuffer if the xbuffer has a user buffer.
    // We can rebuild platform buffer from it.
    pbuf = xb->sys_buf;
    // sal_ether_send() will free the system network buffer.
    debug_itf(4, "detaching xb=%p", xb);
    xb->sys_buf = NULL;
    xb->mach = NULL;
    xb->nh = NULL;
#else
    sal_assert(!xb->sys_buf);
    sal_assert(xb->owner);
    sal_assert(((struct lpx_sock*) xb->owner)->system_sock);
    pbuf = sal_net_alloc_platform_buf(
              ((struct lpx_sock*) xb->owner)->system_sock,
          LPX_HEADER_SIZE, XBUF_NETWORK_DATA_LEN(xb), 
          &segbuf
       );
    if (pbuf==NULL) {
        xbuf_free(xb); 
        return -1;
    }
    if (segbuf.size !=1) {
        debug_itf(0, "Currently support single segmented buffer size=%ld", segbuf.size);
    }
    sal_assert(segbuf.network_header.ptr);
    if (XBUF_NETWORK_DATA_LEN(xb)>0) {
        char* data_ptr = ((char*)segbuf.segs[0].ptr) + LPX_HEADER_SIZE;
        sal_assert(XBUF_NETWORK_DATA(xb));
        sal_memcpy(data_ptr, XBUF_NETWORK_DATA(xb), XBUF_NETWORK_DATA_LEN(xb));
        segbuf.segs[0].len += XBUF_NETWORK_DATA_LEN(xb);
    }
    totlen= LPX_HEADER_SIZE + XBUF_NETWORK_DATA_LEN(xb);
#endif

#ifdef DEBUG    
#if DEBUG_LEVEL_LPX >= 7
    {
        struct lpxhdr     *hdr = (struct lpxhdr *) XBUF_NETWORK_HEADER(xb);
        if ( hdr && hdr->pu.p.type == LPX_TYPE_STREAM ) {
            debug_lpx_packet(7, "send %s:%d->%s:%d (Seq=0x%x,AcqSeq=0x%x,LSCTL=0x%x,pktsize=%d,data=%s",
                SAL_DEBUG_HEXDUMP_S(XBUF_MAC_HEADER(xb)->src,6),
                g_ntohs(hdr->source_port),            
                SAL_DEBUG_HEXDUMP_S(XBUF_MAC_HEADER(xb)->des,6),
                g_ntohs(hdr->dest_port),
                g_ntohs(hdr->u.s.sequence),
                g_ntohs(hdr->u.s.ackseq),
                g_ntohs(hdr->u.s.lsctl),
                g_ntohs(hdr->pu.pktsize & ~LPX_TYPE_MASK),
                SAL_DEBUG_HEXDUMP(((char*)segbuf.segs[0].ptr) + LPX_HEADER_SIZE, segbuf.segs[0].len - LPX_HEADER_SIZE)
            );
        }
        //sal_debug_print("->                 %d %d: %ums(data:%d)", g_ntohs(lpxhdr->u.s.sequence), g_ntohs(lpxhdr->u.s.ackseq), sal_time_msec()%1000000, xb->len);
    }
#endif 
    {
        struct lpxhdr     *hdr = (struct lpxhdr *) XBUF_NETWORK_HEADER(xb);
        if ( hdr && hdr->pu.p.type == LPX_TYPE_DATAGRAM ) {
            debug_lpx_datagram(2, "xb=%s", DEBUG_XLIB_XBUF(xb));
            debug_lpx_datagram(2, "send %s:0x%x->%s:0x%x (message_id=0x%x,message_len=0x%x,fragment_id=0x%x,pktsize=%d",
                SAL_DEBUG_HEXDUMP_S(interface->itf_node,6),
                g_ntohs(hdr->source_port),            
                SAL_DEBUG_HEXDUMP_S(node,6),
                g_ntohs(hdr->dest_port),
                g_ntohs(hdr->u.d.message_id),
                g_ntohs(hdr->u.d.message_length),
                g_ntohs(hdr->u.d.fragment_id),
                g_ntohs(hdr->pu.pktsize & ~LPX_TYPE_MASK)
            );
        }
    }
#endif
    ret = sal_ether_send(nd, pbuf, (char*)node, totlen);

    if (ret!= totlen) {
        debug_lpx(1, "sal_ether_send failed(%d)", ret);
    }

    xbuf_free(xb);

    return ret;
}

/*
Can be run in software interrupt context. Cannot sleep.
*/
LOCAL LPX_INLINE void lpxitf_rcv(struct xbuf *xb, sal_netdev_desc nd)
{
    lpx_interface    *interface;

    debug_itf(6, "xb=%s",DEBUG_XLIB_XBUF(xb));
    debug_itf(6, "sal_netdev_desc=%p", nd);

    interface = lpxitf_find_using_netdesc(nd);
    if(!interface) {
        debug_itf(1, "Rcv interface is not found");
        xbuf_free(xb);
        return;
    }

    lpxitf_demux_sock(interface, xb);
}

LOCAL
void _lpxitf_demux_sock_virtual_mapping(
    struct lpx_sock *sk,
    lpx_interface *interface
)
{
    sal_memcpy(LPX_OPT(sk)->source_addr.node, interface->itf_node, LPX_NODE_LEN);
    LPX_OPT(sk)->virtual_mapping = 0;
    LPX_OPT(sk)->interface = interface;
    lpxitf_insert_sock(interface, sk);
    sk->zapped = 0;
    lpx_update_dest_cache(sk);
}

LOCAL LPX_INLINE void lpxitf_demux_sock(lpx_interface *interface, struct xbuf *xb)
{
    struct lpxhdr *lpxhdr;
    struct lpx_sock *sk;

    debug_itf(4, "ing interface=%s", interface->dev);

    lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);
    sk = lpx_find_port_bind(lpxhdr->dest_port, 1);

    if(sk) {
        debug_itf(9, "lpx sock opt=%s",DEBUG_LPX_OPT(sk));
    }

    if(sk)
    {
        if((LPX_OPT(sk)->virtual_mapping)) 
        {
            _lpxitf_demux_sock_virtual_mapping(sk, interface);
        }

        switch(lpxhdr->pu.p.type) 
        {
        case LPX_TYPE_STREAM:
             debug_itf(9, "sk=%p",sk);
            if(sal_memcmp(
                LPX_OPT(sk)->source_addr.node,
                XBUF_MAC_HEADER(xb)->des,
                LPX_NODE_LEN) == 0) {

                 stm_rcv(sk,xb);
            }
            /* Decrease the socket reference count raised by lpx_find_port_bind() */
            stm_release(sk,0);
            break;
        case LPX_TYPE_DATAGRAM:
            dgm_rcv(sk,xb);
            /* Decrease the socket reference count raised by lpx_find_port_bind() */
            dgm_release(sk,0);
            break;
        case LPX_TYPE_RAW:
            lpx_sock_queue_rcv_xbuf(sk, xb);
            xbuf_free(xb);
            sal_assert(0);
            break;
        }
    } else {
        xbuf_free(xb);
    }
    return;
}

/* Queue xbuf to receive queue of lpx_sock */
LOCAL void lpx_sock_queue_rcv_xbuf(struct lpx_sock* sk, struct xbuf* xb)
{
    xb->owner = (void*)sk;
    xbuf_queue_tail(&sk->receive_queue, xb);
    if (!sk->dead) {
        lpx_sock_data_ready(sk, xb->len);
    }
    return;
}

/* Queue data xbuf to receive queue of lpx_sock */
LOCAL void stm_queue_data_rcv(struct lpx_sock* sk, struct xbuf* xb)
{
    unsigned long flags;

    xb->owner = (void*)sk;

    xbuf_qlock(&sk->receive_queue, &flags);
    if(sk->aio == NULL) {
        _xbuf_queue_tail(&sk->receive_queue, xb);
        xbuf_qunlock(&sk->receive_queue, &flags);
        debug_lpx(4, "No Aio. %d", xb->len);
        // Notify to the user process.
        if (!sk->dead) {
            lpx_sock_data_ready(sk, xb->len);
        }
    } else {
        // Copy xbuffer data to mem blocks or simple buffer.
        int to_copy, xb_len;
        int remaining_len;
        int completed;
        int ret;
        lpx_aio *aio = sk->aio;

        debug_lpx(4, "Aio. %d", xb->len);
        xb_len = to_copy = xb->len;
        remaining_len = aio->len - aio->completed_len;
        if(to_copy > remaining_len) {
            to_copy = remaining_len;
        }
        // DO copy.
        if(aio->aio_user_copy) {
            ret = aio->aio_user_copy(aio, XBUF_NETWORK_DATA(xb), to_copy);
        } else if(aio->nr_blocks) {
            ret = xbuf_copy_data_to_blocks(xb, 0, aio->buf.blocks, aio->nr_blocks, to_copy);
        } else {
            ret = xbuf_copy_data_to(xb, 0, ((char *)aio->buf.buf) + aio->completed_len, to_copy);
        }
        if(ret) {
            xbuf_qunlock(&sk->receive_queue, &flags);
            debug_lpx(1, "memcpy_to error");
            xbuf_free(xb);
            return;
        }
        if(xb->len == to_copy) {
            xbuf_free(xb);
        } else {
#ifdef DEBUG
            if ( remaining_len == 60 ) {
                struct lpxhdr * lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);
                debug_lpx(6, "Reading READ reply seq=%d ackseq=%d xb->len=%d", 
                    g_ntohs(lpxhdr->u.s.sequence), g_ntohs(lpxhdr->u.s.ackseq),
                    xb->len);
            }
#endif
            xbuf_pull(xb, to_copy);
            _xbuf_queue_tail(&sk->receive_queue, xb);
        }
        completed = lpx_complete_aio_datalen(aio, to_copy);
        // Notify to the user process.
        if (completed) {
            debug_lpx(4, "Completed aio %p", sk->aio);
            sk->aio = NULL;
	     if (sk->sleep != SAL_INVALID_EVENT) {
          		sal_event_set(sk->sleep);	
	     }
        }
        xbuf_qunlock(&sk->receive_queue, &flags);

        if (!sk->dead) {
            lpx_sock_data_ready(sk, xb_len);
        }
    }

    return;
}

/* see sock_def_readable */
LOCAL void lpx_sock_data_ready(struct lpx_sock* sk, int len)
{
#ifdef USE_DELAYED_RX_NOTIFICATION
    int saved_cnt;
    sal_spinlock_take_softirq(sk->to_recv_count_lock);
    saved_cnt = sk->to_recv_count;
    sk->to_recv_count -= len;
#if 0
    if(sk->type == LPX_TYPE_STREAM && sk->to_recv_count <= 0) {
        sal_error_print("sk %p: %d\n", sk, sk->to_recv_count);
    }
#endif
    if (saved_cnt > 0 && sk->to_recv_count<=0) {
        if (sk->sleep != SAL_INVALID_EVENT)
            sal_event_set(sk->sleep);
    }
    sal_spinlock_give_softirq(sk->to_recv_count_lock);
#else
      if (sk->sleep != SAL_INVALID_EVENT)
          sal_event_set(sk->sleep);
#endif
}

LOCAL struct lpx_sock* lpx_sock_create(int type, int protocol)
{
    struct lpx_sock* sk;
    
    sk = (struct lpx_sock*) sal_malloc(sizeof(struct lpx_sock));
    if (!sk)
        return NULL;

    sal_memset(sk, 0, sizeof(struct lpx_sock));    

    sk->system_sock = sal_sock_create(type, protocol, lpx_max_transfer_size);
    if ( !sk->system_sock ) 
    {
        sal_free(sk);
        return NULL;
    }

    sk->next = NULL;
    sk->next_sibling = NULL;
    sk->type = type;    
    sk->shutdown = 0;
    sk->state = SMP_CLOSE;
    sk->zapped = 1;
    sk->dead = 0;    
    sk->destroy = 0;
    sk->acceptcon = 0;
    sk->rtimeout = SAL_TICKS_PER_SEC * 2;
    sk->wtimeout = SAL_TICKS_PER_SEC * 2;
#ifdef USE_DELAYED_RX_NOTIFICATION
    sk->to_recv_count = 0;
    sal_spinlock_create("recv_cnt", &sk->to_recv_count_lock);
#endif

    xbuf_queue_init(&sk->backlog);
    xbuf_queue_init(&sk->receive_queue);
    xbuf_queue_init(&sk->write_queue);

#ifdef USE_DELAYED_ACK
    sal_spinlock_create("dack_lock", &sk->delayed_ack_lock);
    sk->last_txed_ack = sal_get_tick();
    sk->unsent_ack_count = 0;
#endif

    sal_spinlock_create("sk_lock", &sk->lock);

    if (type==LPX_TYPE_STREAM) {
        sk->sleep = sal_event_create("sk_slp");
    } else {
        sk->sleep = SAL_INVALID_EVENT;
    }

    /* Destruction event */
    /* should be used in lpx_close() */
    sk->destruct_event = SAL_INVALID_EVENT; 

    sk->sock_state = LSS_UNCONNECTED;

    /* Creation reference count */
    sal_atomic_set(&sk->refcnt, 1);
    sk->creationref = 1;

    /* add to socket list */
//    sal_assert(lpx_socks_lock!= SAL_INVALID_MUTEX);

    sal_spinlock_take_softirq(lpx_socks_lock);

    /* Allocate socket file descriptor */
    sk->lpxfd = next_lpx_sock_fd;
    next_lpx_sock_fd++;

    /* Insert to socket list */
    sal_atomic_inc(&lpx_sock_count);

    list_add(&sk->all_node, &lpx_socks[lpx_hash_sockfd(sk->lpxfd)]);

    sal_spinlock_give_softirq(lpx_socks_lock);

    debug_lpx(3, "ed(total #%d)", sal_atomic_read(&lpx_sock_count));
    return sk;
}

LOCAL void lpx_sock_destroy(struct lpx_sock* sk)
{
    sal_sock ssk;

    debug_lpx(3, "ing sk=%p", sk);
//    sal_assert(lpx_socks_lock != SAL_INVALID_MUTEX);
    sal_spinlock_take_softirq(lpx_socks_lock);
    sal_assert(!list_empty(&lpx_socks[lpx_hash_sockfd(sk->lpxfd)]));
    /* remove from all list */
    list_del(&sk->all_node);
    sal_spinlock_give_softirq(lpx_socks_lock);

    if(sk->sleep != SAL_INVALID_EVENT) {
        sal_event_destroy(sk->sleep);
        sk->sleep = SAL_INVALID_EVENT;
    }
    sal_spinlock_destroy(sk->lock);

    /* Set the destruction event
       We must not destroy the destruction event.
       lpx_close() will do */
    if(sk->destruct_event != SAL_INVALID_EVENT)
        sal_event_set(sk->destruct_event);

#ifdef USE_DELAYED_RX_NOTIFICATION
    sal_spinlock_destroy(sk->to_recv_count_lock);
#endif

    xbuf_queue_destroy(&sk->backlog);
    xbuf_queue_destroy(&sk->receive_queue);
    xbuf_queue_destroy(&sk->write_queue);
#ifdef USE_DELAYED_ACK
    sal_spinlock_destroy(sk->delayed_ack_lock);
#endif

    ssk = sk->system_sock;
    debug_lpx(3, "ssk=%p sk=%p", ssk, sk);
    sal_free(sk);

    sal_atomic_dec(&lpx_sock_count);

    sal_sock_destroy(ssk);
    debug_lpx(3, "ed lpx_sock_count=%d)", sal_atomic_read(&lpx_sock_count));
}

/*
    create new lpx_sock 
    return negitive for error. 
*/
LOCAL ndas_error_t lpx_create(struct lpx_sock** newsk, int type, int protocol)
{
    struct lpx_sock *sk;
    debug_lpx(3, "ing type=%d protocol=%d", type, protocol);
    
    if(/*type != LPX_TYPE_RAW && */
        type != LPX_TYPE_DATAGRAM
        && type != LPX_TYPE_STREAM)
    {
        return NDAS_ERROR_INVALID_OPERATION;
    }

    sk = lpx_sock_create(type, protocol);

    if(sk == NULL)
        return NDAS_ERROR_OUT_OF_MEMORY;

    if(type ==  LPX_TYPE_DATAGRAM)     {
        dgm_create(sk, protocol);
    } else if(type ==  LPX_TYPE_STREAM)    {
#ifdef XPLAT_OPT_THROTTLE_SOCK_CREATE
        while(v_destroy_pending_count>2) {
            debug_lpx(1, "%d socks are waiting for destoyed. Delaying new sock creation", v_destroy_pending_count);
            sal_msleep(100);
        }
#endif
        stm_create(sk, protocol);
    }
    *newsk = sk;
    debug_lpx(3, "ed sk=%p", sk);
    return NDAS_OK;
}

/* see sock_def_wakeup */
LOCAL void lpx_sock_state_change(struct lpx_sock *sk)
{
    if (sk->sleep)
        sal_event_set(sk->sleep);
}


LOCAL ndas_error_t dgm_create(struct lpx_sock *sk, int protocol)
{
     debug_lpx_datagram(2, "sk=%p",sk);
    LPX_DGRAM_OPT(sk)->message_id = (unsigned short) sal_get_tick();
#ifdef XPLAT_SIO
    sal_spinlock_create("lpxaio", &(LPX_DGRAM_OPT(sk)->sio_lock));
    INIT_LIST_HEAD(&LPX_DGRAM_OPT(sk)->sio_request_queue);
#endif
    LPX_DGRAM_OPT(sk)->receive_data_size = 0;
    return NDAS_OK;
}

// May be called in interrupt routine.
LOCAL LPX_INLINE void dgm_hold(struct lpx_sock *sk)
{
    lpx_sock_inc_ref(sk);
}

/* Even though reference count is not zero, */
/* mark dead if the creation reference is removed. */
// May be called in interrupt routine.
LOCAL void _dgm_release_creation_ref(struct lpx_sock *sk)
{
    unsigned long socklock_flags;
#ifdef XPLAT_SIO
    lpx_siocb_i *scb;
#endif

    lock_sock(sk, &socklock_flags);
    if(sk->creationref == 0) {
        // fix up reference count decrement.
        lpx_sock_inc_ref(sk);
        unlock_sock(sk, &socklock_flags);
        debug_lpx_datagram(1, "Duplicate close. refcnt=%d", lpx_sock_read_ref(sk));
        return;
    }
    /*NOTE:After shutdown is marked,
        should return errors for user requests. */
    sk->shutdown = 1;
    sk->creationref = 0;
    if(!sk->dead) {
        lpx_sock_state_change(sk);
        sk->dead = 1;
    }
    /* Clean up user request queues. */
    /* deallocate sio_request_queue */
#ifdef XPLAT_SIO
    sal_spinlock_take_softirq(LPX_DGRAM_OPT(sk)->sio_lock);
    while(!list_empty(&LPX_DGRAM_OPT(sk)->sio_request_queue) ) 
    {
        struct list_head *i = LPX_DGRAM_OPT(sk)->sio_request_queue.next;
        scb = list_entry(i,    lpx_siocb_i, node);
        scb->cb.nbytes = 0;
        scb->cb.result = NDAS_ERROR_SHUTDOWN;
        list_del(i);
        sal_spinlock_give_softirq(LPX_DGRAM_OPT(sk)->sio_lock);
        (scb->func)(&scb->cb, 0, scb->user);
        _siocb_destroy(scb);
        sal_spinlock_take_softirq(LPX_DGRAM_OPT(sk)->sio_lock);
    }
    sal_spinlock_give_softirq(LPX_DGRAM_OPT(sk)->sio_lock);
#endif
    unlock_sock(sk, &socklock_flags);
}

// May be called in interrupt routine.
LOCAL
void
_dgm_destroy_sock(
    struct lpx_sock *sk
)
{
    struct xbuf    *xb;

    debug_lpx_datagram(1, "removing dgram sk=%p",sk);
    sal_assert(sk->creationref == 0);
    /* Only one last thread can access the socket after here. */
    /* Unregister the socket from the port number and network interface */
    lpxitf_remove_sock(sk);
    /* deallocate receive_queue */
    while((xb = xbuf_dequeue(&sk->receive_queue)) != NULL) {
        xbuf_free(xb);
    }
#ifdef XPLAT_SIO
    sal_spinlock_destroy(LPX_DGRAM_OPT(sk)->sio_lock);
#endif
    lpx_sock_destroy(sk);
}

// May be called in interrupt routine.
LOCAL
LPX_INLINE
void dgm_release(struct lpx_sock *sk, int creationref)
{
    int last_ref;

    sal_assert(sk);
    debug_lpx_datagram(3, "sk=%p",sk);

    if(creationref) {
        _dgm_release_creation_ref(sk);
    }
    last_ref = lpx_sock_dec_ref(sk);
    if(last_ref) {
        _dgm_destroy_sock(sk);
    }
}

//
// Entered with the socket lock acquired.
//
LOCAL ndas_error_t _dgm_bind(struct lpx_sock *sk, struct sockaddr_lpx *addr, int addr_len)
{
    int valid = 0;
   debug_lpx_datagram(2, "Entered.");

    if(sk->zapped == 0)
        return NDAS_ERROR_INVALID_PARAMETER;

    if(addr_len != sizeof(struct sockaddr_lpx))
        return NDAS_ERROR_INVALID_PARAMETER;

    debug_lpx_datagram(5, "From %02X%02X%02X%02X%02X%02X",
                               addr->slpx_node[0], addr->slpx_node[1],
                               addr->slpx_node[2], addr->slpx_node[3],
                               addr->slpx_node[4], addr->slpx_node[5]);

    // step 1 : validation of address
    //    addr must be either valid ethnet addr or zero addr
    valid = lpx_is_valid_addr(sk, addr->slpx_node);
    if(!valid) {
        debug_lpx(3, "bind: Invalid address");
        return NDAS_ERROR_INVALID_PARAMETER;
    }
    debug_lpx_datagram(3, "From %02X%02X%02X%02X%02X%02X",
                               addr->slpx_node[0], addr->slpx_node[1],
                               addr->slpx_node[2], addr->slpx_node[3],
                               addr->slpx_node[4], addr->slpx_node[5]);

    // step 2 : get port number
    if(addr->slpx_port == 0) {
        addr->slpx_port = g_htons(lpx_get_free_port_num());
        if(addr->slpx_port == 0) {
            debug_lpx_datagram(3, "bind: Address not available");
            return NDAS_ERROR_ADDRESS_NOT_AVAIABLE;
        }
        sk->fixedport = 0;
    } else {
        sk->fixedport = 1;
    }

    // step 3 : check port number validation
    if(lpx_find_port_bind(addr->slpx_port, 0) != NULL) {
        debug_lpx_datagram(3, "bind: Invalid port number %d:already bound", addr->slpx_port);
        return NDAS_ERROR_INVALID_PARAMETER;
    }
    // step 4 : register sock to binder and interfaces
    LPX_OPT(sk)->source_addr.port = addr->slpx_port;
    lpx_reg_bind(sk);
    if(LPX_OPT(sk)->interface) 
        lpxitf_insert_sock(LPX_OPT(sk)->interface, sk);


    debug_lpx_datagram(3, "LPX_OPT(sk)->interface = %p, sk = %s", LPX_OPT(sk)->interface, DEBUG_LPX_SOCK(sk));

    return NDAS_OK;
}

//
// Enter with the socket lock acquired.
//
LOCAL ndas_error_t dgm_connect(struct lpx_sock *sk, struct sockaddr_lpx *addr, int addr_len, int flags)
{
    debug_lpx_datagram(3, "dgm_connect");

    sk->state    = SMP_CLOSE;
    sk->sock_state     = LSS_UNCONNECTED;
    if(addr_len != sizeof(struct sockaddr_lpx)) {
        return NDAS_ERROR_INVALID_PARAMETER;
    }

    if(LPX_OPT(sk)->source_addr.port == 0) {
        struct sockaddr_lpx     uaddr;
        ndas_error_t             ret;
        unsigned char zero_node[LPX_NODE_LEN] = {0,0,0,0,0,0};

        uaddr.slpx_port    = 0;

        if(LPX_OPT(sk)->interface)
            sal_memcpy(uaddr.slpx_node, LPX_OPT(sk)->interface->itf_node,LPX_NODE_LEN);
        else
            sal_memcpy(uaddr.slpx_node, zero_node,LPX_NODE_LEN);
        ret = _dgm_bind(sk, &uaddr,
                sizeof(struct sockaddr_lpx));
        if(!NDAS_SUCCESS(ret)) {
            return ret;
        }
    } else {
     //debug_lpx(1, "source port is given: %d", LPX_OPT(sk)->source_addr.port );
    }

    LPX_OPT(sk)->dest_addr.port = addr->slpx_port;
    sal_memcpy(LPX_OPT(sk)->dest_addr.node, addr->slpx_node,LPX_NODE_LEN);

    if(LPX_OPT(sk)->virtual_mapping) {
        struct lpx_cache *ret;

        ret = lpx_find_dest_cache(sk);
        if(ret) {
            LPX_OPT(sk)->interface = lpxitf_find_using_node(LPX_OPT(sk)->source_addr.node);
            LPX_OPT(sk)->virtual_mapping = 0;
            lpxitf_insert_sock(LPX_OPT(sk)->interface, sk);
        }
    }
    else {
        lpx_update_dest_cache(sk);
    }

    if(sk->type == LPX_TYPE_DATAGRAM) {
        sk->sock_state     = LSS_CONNECTED;
        sk->state     = SMP_ESTABLISHED;
    }

    return NDAS_OK;
}

LOCAL ndas_error_t dgm_getname(struct lpx_sock *sk, struct sockaddr_lpx *uaddr, int *uaddr_len, int peer)
{
    struct sockaddr_lpx saddr;
    lpx_address*     addr;
    debug_lpx_datagram(3, "dgm_getname");

    *uaddr_len = sizeof(struct sockaddr_lpx);

    if(peer) {
        if(sk->state != SMP_ESTABLISHED)
            return NDAS_ERROR_NOT_CONNECTED;
        
        addr = &LPX_OPT(sk)->dest_addr;
        sal_memcpy(saddr.slpx_node, addr->node, LPX_NODE_LEN);
        saddr.slpx_port = addr->port;
    } else {
        if(LPX_OPT(sk)->interface != NULL) {
                sal_memcpy(saddr.slpx_node, LPX_OPT(sk)->interface->itf_node, LPX_NODE_LEN);
        } else {
                sal_memset(saddr.slpx_node, '\0', LPX_NODE_LEN);
        }
        saddr.slpx_port = LPX_OPT(sk)->source_addr.port;
    }
    
    saddr.slpx_family = AF_LPX;
    sal_memcpy(uaddr, &saddr, sizeof(struct sockaddr_lpx));
    return NDAS_OK;
}

LOCAL ndas_error_t dgm_send_user(struct lpx_sock *sk, struct sockaddr_lpx * usaddr, char* data, int len)
{
    struct sockaddr_lpx     local_saddr;
    ndas_error_t             retval;

    debug_lpx_datagram(3, "ing sk=%d",  sk->lpxfd);
    debug_lpx_datagram(8, "ing sk=%s",  DEBUG_LPX_SOCK(sk));

    if(usaddr) {
        if(LPX_OPT(sk)->source_addr.port == 0) {
            struct sockaddr_lpx saddr;
            unsigned long flags;

            saddr.slpx_port = 0;
            if(LPX_OPT(sk)->interface) {
                sal_memcpy(saddr.slpx_node, LPX_OPT(sk)->interface->itf_node,LPX_NODE_LEN);
            } else {
                return NDAS_ERROR_NETWORK_DOWN;
            }
            lock_sock(sk, &flags);
            retval = _dgm_bind(sk, &saddr, sizeof(struct sockaddr_lpx));
            unlock_sock(sk, &flags);
            if(!NDAS_SUCCESS(retval)) {
                return retval;
            }
        }

        if(usaddr->slpx_family != AF_LPX) {
            return NDAS_ERROR_INVALID_PARAMETER;
        }

        if(LPX_OPT(sk)->virtual_mapping) {
            struct lpx_cache *ret;

            sal_memcpy(LPX_OPT(sk)->dest_addr.node, usaddr->slpx_node, LPX_NODE_LEN);
            LPX_OPT(sk)->dest_addr.port = usaddr->slpx_port;
            ret = lpx_find_dest_cache(sk);
            if(ret) {
                LPX_OPT(sk)->interface = lpxitf_find_using_node(LPX_OPT(sk)->source_addr.node);
                LPX_OPT(sk)->virtual_mapping = 0;
                lpxitf_insert_sock(LPX_OPT(sk)->interface, sk);
            }
        }
        else {
            lpx_update_dest_cache(sk);
        }
    } else {
        if(sk->state != SMP_ESTABLISHED) {
            return NDAS_ERROR_NOT_CONNECTED;
        }

        usaddr = &local_saddr;
        usaddr->slpx_family = AF_LPX;
        usaddr->slpx_port = LPX_OPT(sk)->dest_addr.port;
        sal_memcpy(usaddr->slpx_node, LPX_OPT(sk)->dest_addr.node,LPX_NODE_LEN);
    }

    retval = dgm_transmit_packet(sk, usaddr, data, len);
    if(!NDAS_SUCCESS(retval))
        return retval;

    return len;
}

/* Used both dgm_recv_user, sio_dgm_recv_user */
LOCAL
ndas_error_t
makesure_dgm_bound(struct lpx_sock *sk) {
    struct sockaddr_lpx uaddr;
    unsigned long flags;
    ndas_error_t ret;

    uaddr.slpx_port = 0;
    if(LPX_OPT(sk)->interface)
        sal_memcpy(uaddr.slpx_node, LPX_OPT(sk)->interface->itf_node,LPX_NODE_LEN);
    else  {
        return NDAS_ERROR_NETWORK_DOWN;
    }
    lock_sock(sk, &flags);
    ret = _dgm_bind(sk, (struct sockaddr_lpx*)&uaddr, sizeof(struct sockaddr_lpx));
    unlock_sock(sk, &flags);
    if(!NDAS_SUCCESS(ret))
        return ret;

    return NDAS_OK;
}

LOCAL ndas_error_t dgm_recv_user(struct lpx_sock *sk, struct sockaddr_lpx * saddr, char* data, int size)
{
    struct lpxhdr         *lpxhdr = NULL;
    struct xbuf         *xb;
    int             copied;
    ndas_error_t err;
    int            user_data_length;
    int            total_size;
    debug_lpx_datagram(3, "ing sk=%p", sk);
    debug_lpx_datagram(7, "ing sk=%s", DEBUG_LPX_SOCK(sk));

    if ( LPX_OPT(sk)->source_addr.port == 0) {
        err =makesure_dgm_bound(sk);
        if(!NDAS_SUCCESS(err)) {
            return err;
        }
    }

    debug_lpx_datagram(4, "xbuf_dequeue_wait for %d", sk->rtimeout);
    xb = xbuf_dequeue_wait(&sk->receive_queue, sk->rtimeout);
    debug_lpx_datagram(4, "xbuf_dequeue_wait xb=%p", xb);
    if(!xb) {
        debug_lpx_datagram(2, "timed out for %ld", (unsigned long)sk->rtimeout);
        return NDAS_OK; /* timeout */
    }
    total_size = 0;

    lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);

    user_data_length = g_ntohs(lpxhdr->u.d.message_length);
    if(user_data_length > size) {
        user_data_length = size;
        debug_lpx_datagram(1, "lpx_dgram_recv: packet truncated");
    }

    copied = (g_ntohs(lpxhdr->pu.pktsize & ~LPX_TYPE_MASK))- sizeof(struct lpxhdr);

    debug_lpx_datagram(4, "lpx_dgram_recvmsg user_data_length = %d, copied = %d", user_data_length, copied);
    if((total_size + copied) > user_data_length)
    {
        copied = user_data_length - total_size;
    }

    total_size += copied;

    err = xbuf_copy_data_to(xb, 0, data, copied);
    debug_lpx_datagram(4, "lpx_dgram_recvmsg err = %d", err);
    if(err)
        goto out_free;

    if(saddr) {
        saddr->slpx_family     = AF_LPX;
        saddr->slpx_port     = lpxhdr->source_port;
        sal_memcpy(saddr->slpx_node, XBUF_MAC_HEADER(xb)->src, LPX_NODE_LEN);
    }

    err = user_data_length;

out_free:
    debug_lpx_datagram(4, "xbuf_free-ing xb=%p", xb);
    xbuf_free(xb);
    debug_lpx_datagram(4, "xbuf_free-ed");
    return err;
}

LOCAL ndas_error_t dgm_transmit_packet(struct lpx_sock *sk, struct sockaddr_lpx *usaddr, char* data, int len) {
    struct xbuf  *xb;
    lpx_interface   *interface;
    struct lpxhdr   *lpxhdr;

    int        mss_now;
    unsigned short    fragment_id;
    unsigned short    copy;
    unsigned short    user_data_len;
    unsigned int data_offset = 0;
    int ret;
/*    printk("dgm_transmit_packet %d, des: %02x:%02x:%02x:%02x:%02x:%02x\n", len
        , usaddr->slpx_node[0], usaddr->slpx_node[1], usaddr->slpx_node[2]
        , usaddr->slpx_node[3], usaddr->slpx_node[4], usaddr->slpx_node[5]);*/

    debug_lpx_datagram(6, "len=%d", len);

    user_data_len = (unsigned short)len;

    interface = LPX_OPT(sk)->interface;
    
    if (interface)
        mss_now = lpxitf_mss(sk);
    else
        mss_now = MIN_MTU_SIZE - sizeof(struct lpxhdr);                /* to do: adjust MTU for every interface */
    
    fragment_id = 0;
    LPX_DGRAM_OPT(sk)->message_id ++;
    
    while(user_data_len) {
        /* Split large user data into MTU size */
        copy = (unsigned short)mss_now;
        
        if(copy > user_data_len)
            copy = user_data_len;

        user_data_len -= copy;

        // Allocate a packet in blocking context
        xb = lpx_sock_alloc_xb(sk, copy, 0);
        if(!xb) {
            debug_lpx_datagram(1, "sk=%p lpx_sock_alloc_xb() failed.", sk);
            return NDAS_ERROR_OUT_OF_MEMORY;
        }

        lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);
        lpxhdr->pu.pktsize = g_htons(copy + sizeof(struct lpxhdr));
        lpxhdr->pu.p.type = LPX_TYPE_DATAGRAM;
        lpxhdr->source_port = LPX_OPT(sk)->source_addr.port;
        lpxhdr->dest_port = usaddr->slpx_port;

        lpxhdr->u.d.message_id = 0; //LPX_DGRAM_OPT(sk)->message_id;
        lpxhdr->u.d.message_length = g_htons(len);
        lpxhdr->u.d.fragment_id = g_htons(fragment_id);
        lpxhdr->u.d.fragment_length = 0;
        lpxhdr->u.d.reserved_d3 = 0;
        fragment_id ++;

        xbuf_put(xb, copy);
        sal_memcpy(XBUF_NETWORK_DATA(xb), data+data_offset, copy);
        data_offset += copy;

        if(!interface) {
            /* If interface is not determined, send to all registered interface */
            lpx_interface *p_if;
            struct xbuf* xb2;
            for(p_if = lpx_interfaces; p_if; p_if = p_if->itf_next) {
                xb2 = xbuf_copy(xb);
                ret = lpxitf_send(p_if, xb2, usaddr->slpx_node);
                if(ret < 0) {
                    debug_lpx_datagram(1, "lpxitf_send() failed.");
                }
            }
            xbuf_free(xb);
        } else {
            ret = lpxitf_send(interface, xb, usaddr->slpx_node);
            if(ret < 0) {
                debug_lpx_datagram(1, "lpxitf_send() failed.");
            }
        }
    }
    return NDAS_OK;
}


#ifdef XPLAT_SIO

/*
  Notify to the user.
  NOT support complete packet reading
 */
LOCAL int
sio_notify_to_user(lpx_siocb_i *scb, void *arg)
{
    sal_ether_header* machdr;
    struct lpxhdr* lpxhdr;
    struct sockaddr_lpx count_host_addr;
    struct lpx_sock *sk;
    struct xbuf *xb;

    sk = scb->sock;

    while((xb = xbuf_dequeue(&sk->receive_queue)) != NULL) {

        xbuf_copy_data_to(xb, 0, scb->cb.buf, XBUF_NETWORK_DATA_LEN(xb));
        scb->cb.nbytes = XBUF_NETWORK_DATA_LEN(xb);
        scb->cb.result = NDAS_OK;

        count_host_addr.slpx_family = PF_LPX;
        machdr = XBUF_MAC_HEADER(xb);
        lpxhdr = (struct lpxhdr*) XBUF_NETWORK_HEADER(xb);

        sal_memcpy(&count_host_addr.slpx_node, machdr->src, SAL_ETHER_ADDR_LEN);
        count_host_addr.slpx_port = lpxhdr->source_port;

#ifdef DEBUG
        if ( sal_memcmp(machdr->des,LPX_BROADCAST_NODE, SAL_ETHER_ADDR_LEN) != 0 ) {
            debug_lpx_sio(7, "scb=%p xb=%p xb-size=%d",scb,xb,
                XBUF_NETWORK_DATA_LEN(xb));
            debug_lpx_sio(8, "scb=%s", DEBUG_LPX_SIOCB_I(scb)); 
            debug_lpx_sio(9, "xb=%s", DEBUG_XLIB_XBUF(xb));
            debug_lpx_sio(8, "count host addr=%s:0x%x", 
                DEBUG_LPX_NODE(count_host_addr.slpx_node),
                g_ntohs(count_host_addr.slpx_port) );
        }
#endif
        xbuf_free(xb);

        /* handler call */
        (*scb->func)(&scb->cb,&count_host_addr, scb->user);
        // Notify only one packet for the non-keep_notifying SIOCB
        if(!scb->keep_notifying) {
            break;
        }
    }

    // Remove the reference for this function call.
    _siocb_destroy(scb);

    // Decrease the socket reference count for this DPC thread.
    dgm_release(sk, 0);

    return 0;
}

/*
    Signalled IO time out.
 */
LOCAL int 
sio_timedout(void *_acb, void *_sk)
{
    lpx_siocb_i *scb = _acb;
    struct lpx_sock *sk  = _sk;

    sal_spinlock_take_softirq(LPX_DGRAM_OPT(sk)->sio_lock);
    //
    // If the SCB is still in the list, remove it from the list,
    // and remove the creation reference
    //
    if(!list_empty(&scb->node)) {
        list_del_init(&scb->node);
        _siocb_destroy(scb);
    }
    sal_spinlock_give_softirq(LPX_DGRAM_OPT(sk)->sio_lock);

    scb->cb.result = NDAS_ERROR_TIME_OUT;
    scb->cb.nbytes = -1;

    /* handler call */
    scb->func(&scb->cb, NULL, scb->user);
    _siocb_destroy(scb);

    // Decrease the socket reference count for this DPC thread.
    dgm_release(sk, 0);

    return 0;
}

//
// Return the number of packet received.
//
static
int
sio_dgm_recv_queue(
    struct lpx_sock *sk,
    lpx_siocb_i *scb)
{
    int ret = 0;

    if(!xbuf_queue_empty(&sk->receive_queue)) {
        // Socket and SIOCB reference count for the BPC.
        dgm_hold(sk);
         _siocb_inc_ref(scb);

        ret = bpc_invoke(scb->notify_bpc);

        debug_lpx_sio(1, "scb=%p bpc=%p keep_notify=%d",
            scb, scb->notify_bpc, scb->keep_notifying);

        // Decrease reference count if the BPC queueing fails or is redone.
        if (ret == NDAS_OK + 1) {
            _siocb_destroy(scb);
            dgm_release(sk, 0);
        } else if (ret < 0) {
            _siocb_destroy(scb);
            dgm_release(sk, 0);
            goto out;
        }
        // Set one to the return value to indicate that notification occurs.
        ret = 1;
    }

out:
    return ret;
}

/* check more for race condition */
LOCAL ndas_error_t 
sio_dgm_recv_user(struct lpx_sock *sk, 
    lpx_sio_handler func, void* arg, 
    char* data, int size, sal_tick timeout, int keep_notifying)
{
    lpx_siocb_i *scb;
    int ret;

    if ( LPX_OPT(sk)->source_addr.port == 0) {
        ret =makesure_dgm_bound(sk);
        if(!NDAS_SUCCESS(ret)) {
            return ret;
        }
    }

    scb = _siocb_create(sk, data, func, arg, keep_notifying);
    if ( !scb ) 
        return NDAS_ERROR_OUT_OF_MEMORY;

    //
    // Set up notify BPC
    //
    scb->notify_bpc = bpc_create((dpc_func)sio_notify_to_user, scb, NULL, NULL, DPCFLAG_DO_NOT_FREE);
    if ( scb->notify_bpc == DPC_INVALID_ID ) {
        goto out;
    }

    //
    // Receive back-logged packets.
    //
    ret = sio_dgm_recv_queue(sk, scb);
    if(ret < 0) {
        goto out;
    } else {
        // If we receive one packet for the non-keep-notifying SIO,
        // we regard it as a success.
        if(!keep_notifying && ret > 0) {
            // Remove the creation reference so that
            // notifying function will destroy the SIOCB at last.
            _siocb_destroy(scb);
           return NDAS_OK;
        }
    }

    //
    // Set up a timer.
    //
    if ( timeout == 0 ) {
        //
        // No timer.
        // If the SIOCB is keep_notifying, will be freed only at the close of the socket.
        // If not, free at the receive of a packet in dgm_rcv().
        //

        scb->timeout_timer = DPC_INVALID_ID;

    } else {
        debug_lpx_sio(5, "queue-ing timer");

        //
        // If the SIOCB is keep_notifying, will be freed when the time-out occurs.
        // If not, the timer routine or dgm_rcv() will remove the creation reference of the SIOCB.
        //

        scb->timeout_timer = dpc_create(DPC_PRIO_NORMAL, sio_timedout, scb, sk, NULL, DPCFLAG_DO_NOT_FREE);
        if(scb->timeout_timer == DPC_INVALID_ID) {
            goto out;
        }

        // Socket and SIOCB reference count for the DPC.
        dgm_hold(sk);
        _siocb_inc_ref(scb);

        ret = dpc_queue(scb->timeout_timer, timeout);
        debug_lpx_sio(1, "queue-ed timer=%p", scb->timeout_timer);
        if (ret < 0) {
            _siocb_destroy(scb);
            dgm_release(sk, 0);
            goto out;
        }
    }

    //
    // Add to the sio list.
    // Now the SIOCB list acquires the creation reference.
    //
    sal_spinlock_take_softirq(LPX_DGRAM_OPT(sk)->sio_lock);
    list_add_tail(&scb->node, &LPX_DGRAM_OPT(sk)->sio_request_queue);
    sal_spinlock_give_softirq(LPX_DGRAM_OPT(sk)->sio_lock);

    return NDAS_OK;
out:
    debug_lpx_sio(1, "out of memory");
    // Failure. Remove the creation reference.
    _siocb_destroy(scb);
    return NDAS_ERROR_OUT_OF_MEMORY;
}
#endif

LOCAL void dgm_rcv(struct lpx_sock *sk, struct xbuf *xb)
{
    struct lpxhdr *lpxhdr;
    unsigned long flags;
#ifdef XPLAT_SIO
    int ret;
#endif

    debug_lpx_datagram(4, "sk=%p xb=%p", sk, xb);
    debug_lpx_datagram(7, "lpx_sock=%s", DEBUG_LPX_SOCK(sk));
    debug_lpx_datagram(7, "xbuf=%s", DEBUG_XLIB_XBUF(xb));

    lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);

    if(g_ntohs(lpxhdr->u.d.fragment_id) != 0)
    {
        /* Currently fragmented packet is not handled */
        xbuf_free(xb);
        return;
    }

    /* End of remove the duplicatation */
#ifdef XPLAT_SIO
    /* Notify the result */
    sal_spinlock_take_softirq(LPX_DGRAM_OPT(sk)->sio_lock);
    if (!list_empty(&LPX_DGRAM_OPT(sk)->sio_request_queue)) 
    {
        struct list_head *node = LPX_DGRAM_OPT(sk)->sio_request_queue.next;
        lpx_siocb_i *scb = list_entry(node,lpx_siocb_i,node);

        if ( sal_memcmp(XBUF_MAC_HEADER(xb)->des, LPX_BROADCAST_NODE, SAL_ETHER_ADDR_LEN) != 0 ) {
            debug_lpx_sio(8, "scb=%s", DEBUG_LPX_SIOCB_I(scb));

            debug_lpx_sio(5, "notifying the handler");
        }
        if ( !scb->keep_notifying ) {
            list_del_init(node);
            // Timer not needed any more.
            if(scb->timeout_timer != DPC_INVALID_ID) {
                debug_lpx_sio(5, "timeout_timer=%p", scb->timeout_timer);
                ret = dpc_cancel(scb->timeout_timer);
                // If cancelled, undo references.
                if(ret == NDAS_OK) {
                    debug_lpx_sio(1, "scb %p: Timeout cancelled.", scb);
                    _siocb_destroy(scb);
                    dgm_release(sk, 0);
                }
            }
            // No need to increase reference count for the BPC.
            // The BPC function will free the SIOCB.
        } else {
            // Increase the reference count of the SIO control block for the BPC.
            _siocb_inc_ref(scb);
        }
        sal_spinlock_give_softirq(LPX_DGRAM_OPT(sk)->sio_lock);

        // Queue to the packet queue.
        lpx_sock_queue_rcv_xbuf(sk, xb);
        // Reference to the socket for the BPC routine.
        dgm_hold(sk);

        ret = bpc_invoke(scb->notify_bpc);
        // Decrease reference count if the BPC queueing fails or is redone.
        if((ret == NDAS_OK + 1) || ret < 0) {
            debug_lpx_datagram(4, "ret=%d keep_notifying=%d", ret, scb->keep_notifying);
            dgm_release(sk, 0);
            // Reference increased only for keep_notifying SIOCB
            if(scb->keep_notifying)
                _siocb_destroy(scb);
        }
        return;
    }
    sal_spinlock_give_softirq(LPX_DGRAM_OPT(sk)->sio_lock);
#endif
    debug_lpx_datagram(4, "sk=%p xb=%p queued", sk, xb);

    lock_sock(sk, &flags);
    if(sk->dead || sk->shutdown) {
        unlock_sock(sk, &flags);
        debug_lpx_datagram(1, "packet recv after the socket shutdown sock=%p xbuf=%p", sk, xb);
        return;
    }
    lpx_sock_queue_rcv_xbuf(sk, xb);
    unlock_sock(sk, &flags);

    return;
}

/* Functions needed for stream connection start up */

LOCAL ndas_error_t _stm_bind(struct lpx_sock *sk, struct sockaddr_lpx *uaddr, int addr_len);
LOCAL ndas_error_t stm_connect(struct lpx_sock *sk, struct sockaddr_lpx *uaddr, int addr_len, int flags);

LOCAL ndas_error_t stm_accept(struct lpx_sock *sk, int flags);

LOCAL ndas_error_t stm_getname(struct lpx_sock *sk, struct sockaddr_lpx *uaddr, int *usockaddr_len, int peer);
LOCAL ndas_error_t stm_listen(struct lpx_sock *sk, int backlog);
LOCAL ndas_error_t stm_send_user(struct lpx_sock *sk, lpx_aio *aio);
LOCAL ndas_error_t stm_recv_user(struct lpx_sock *sk, lpx_aio *aio);

LOCAL ndas_error_t stm_do_send(struct lpx_sock *sk, lpx_aio * aio);
LOCAL ndas_error_t  stm_transmit(struct lpx_sock *sk, struct xbuf *xb, int type,int len);
LOCAL ndas_error_t stm_route_xb(struct lpx_sock *sk, struct xbuf *xb, int type);
LOCAL unsigned long stm_calc_retransmission_time(struct lpx_sock *sk);
//LOCAL xbool stm_snd_test(struct lpx_sock *sk, struct xbuf *xb);

/* always return NDAS_OK */
LOCAL ndas_error_t stm_retransmit_chk(struct lpx_sock *sk, unsigned short ackseq, int type);
LOCAL void stm_rcv(struct lpx_sock *sk, struct xbuf *xb);
LOCAL int  stm_do_rcv(struct lpx_sock* sk, struct xbuf *xb);

LOCAL int stm_timer(void*, void*);

LOCAL ndas_error_t stm_create(struct lpx_sock *sk, int protocol)
{
    int ret;
     debug_lpx(3, "ing sk=%p protocol=%d", sk, protocol);

    sal_memset(LPX_STREAM_OPT(sk), 0, sizeof(struct lpx_stream_opt));

    LPX_STREAM_OPT(sk)->owner        = (void *)sk;

//    LPX_STREAM_OPT(sk)->interval_time = RETRANSMIT_TIME;

    LPX_STREAM_OPT(sk)->loss_detection_time  = DEFAULT_LOSS_DETECTION_TIME;
    LPX_STREAM_OPT(sk)->alive_timeout = sal_tick_add(sal_get_tick(), ALIVE_INTERVAL);

    LPX_STREAM_OPT(sk)->max_window = SMP_MAX_WINDOW;

    xbuf_queue_init(&LPX_STREAM_OPT(sk)->rexmit_queue);

    LPX_STREAM_OPT(sk)->lpx_stream_timer = dpc_create(
        DPC_PRIO_HIGH,
        (dpc_func) stm_timer,
        (void*)sk,
        NULL,
        NULL,
        DPCFLAG_DO_NOT_FREE);
    if ( LPX_STREAM_OPT(sk)->lpx_stream_timer == DPC_INVALID_ID ) {
        debug_lpx(1, "DPC_INVALID_ID ing sk=%p protocol=%d", sk, protocol);
        xbuf_queue_destroy(&LPX_STREAM_OPT(sk)->rexmit_queue);
        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    // Increase the socket reference count for the DPC thread that will run stm_timer()
    stm_hold(sk);
    ret = dpc_queue(LPX_STREAM_OPT(sk)->lpx_stream_timer, SMP_TIMEOUT);
    if (ret < 0) {
        debug_lpx(1, "DPC_INVALID_ID ing sk=%p protocol=%d", sk, protocol);
        dpc_destroy(LPX_STREAM_OPT(sk)->lpx_stream_timer);
        xbuf_queue_destroy(&LPX_STREAM_OPT(sk)->rexmit_queue);
        stm_release(sk, 0);
        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    return NDAS_OK;
}

/* Hold an SMP socket */
// May be called in interrupt routine.
LOCAL LPX_INLINE void stm_hold(struct lpx_sock *sk) {
    int refcnt;

    lpx_sock_inc_ref(sk);
    refcnt = lpx_sock_read_ref(sk);
    sal_assert(refcnt >= 2); /* This assertion is not safe */

#ifdef DEBUG
    if(refcnt >= 6)
        debug_lpx(1, "sk=%p refcnt=%d",sk, refcnt);
#endif
}

/*
  If creation reference is removed,
    peform final clean up in timer DPC routine
    even though reference count is zero.

    May be called in interrupt routine.
 */
LOCAL
void
_stm_release_creation_ref(struct lpx_sock *sk)
{
    struct lpx_sock *s;
    unsigned long flags;

    lock_sock(sk, &flags);

    if(sk->creationref == 0) {
        // Fix up reference count decrement.
        lpx_sock_inc_ref(sk);
        unlock_sock(sk, &flags);
        debug_lpx(1, "Duplicate close. refcnt=%d",lpx_sock_read_ref(sk));
        return;
    }

    // Continue to close process
    /* mark shutdown flag because the creation reference is removed.
        NOTE:After shutdown is marked,
        should return errors for user requests. */
    sk->creationref = 0;
    sk->shutdown = 1;

    // Clear pending AIO.
    if(sk->aio) {
        unsigned long flags2;
		lpx_aio *aio;
        xbuf_qlock(&sk->receive_queue, &flags2);
		aio = sk->aio;
        sk->aio = NULL;
        xbuf_qunlock(&sk->receive_queue, &flags2);
        lpx_complete_aio_leftover(aio, NDAS_ERROR_SHUTDOWN);
    }

    // Wake up the waitors
    switch(sk->state) {

    case SMP_CLOSE:
    case SMP_LISTEN:
    case SMP_SYN_SENT:
        sk->state = SMP_CLOSE;
        if(!sk->dead)
            lpx_sock_state_change(sk);

        /* Work around to send signal to thread that is blocking at accept */

        for(s = LPX_STREAM_OPT(sk)->child_sklist; s != NULL; s = s->next_sibling) {
            debug_lpx(1, "Signalling child sock %p", s);
            lpx_sock_state_change(s);
        }

        sk->dead = 1;
        unlock_sock(sk, &flags);
        break;

    case SMP_SYN_RECV:
    case SMP_ESTABLISHED:
    case SMP_CLOSE_WAIT:

        if(sk->state == SMP_CLOSE_WAIT)
            sk->state = SMP_LAST_ACK;
        else        
            sk->state = SMP_FIN_WAIT1;

        if(!sk->dead) {
            lpx_sock_state_change(sk);

            LPX_STREAM_OPT(sk)->fin_seq = LPX_STREAM_OPT(sk)->sequence;
            unlock_sock(sk, &flags);

            (void)stm_transmit(sk, NULL, DISCON, 0);
            sk->dead = 1;

        }
//        sk->state = SMP_CLOSE;

        break;

    default:
        unlock_sock(sk, &flags);
        debug_lpx(0, "It's unstable state");
        sal_assert(0);
    }
}

/*
 Release an SMP socket
 May be called in interrupt routine.
*/
LOCAL
LPX_INLINE
void
stm_release(struct lpx_sock *sk, int creationref)
{
    int last_ref;

    debug_lpx(3, "ing fd=%d state=%d", sk->lpxfd, sk->state);
    debug_lpx(3, "ing sk=%s", DEBUG_LPX_SOCK(sk));
    sal_assert(sk);
//    sal_assert(sk->lock != SAL_INVALID_MUTEX);

    if(creationref)
        _stm_release_creation_ref(sk);

    last_ref = lpx_sock_dec_ref(sk);

    if(last_ref) {
        stm_destroy_sock(sk);
    }
}

/*
 * Call with the parent socket lock acqured.
 */

LOCAL
inline
void
stm_remove_from_parent_sk(
    struct lpx_sock *parent_sk,
    struct lpx_sock *sk)
{
    struct lpx_sock *s;

    debug_lpx(4, "Removing child sk %d from sibling list of parent sk %d",
        sk->lpxfd, parent_sk->lpxfd);
    s = LPX_STREAM_OPT(parent_sk)->child_sklist;
    if(s == sk) {
        LPX_STREAM_OPT(parent_sk)->child_sklist = s->next_sibling;
        s = NULL;
    }

    while(s && s->next_sibling) {
        if(s->next_sibling == sk) {
            s->next_sibling = sk->next_sibling;
            break;
        }
        s = s->next_sibling;
    }
}

/*
 * Call with the parent socket lock acqured.
 */

LOCAL
inline
void
stm_insert_to_parent_sk(struct lpx_sock *parent_sk, struct lpx_sock *sk)
{
    struct lpx_sock *s;

    debug_lpx(4, "Adding child sk %d to parent %d", sk->lpxfd, parent_sk->lpxfd);
    if(LPX_STREAM_OPT(parent_sk)->child_sklist == NULL) {
        LPX_STREAM_OPT(parent_sk)->child_sklist = sk;
        return;
    }

    // Traverse to the end of sibling list.
    for(s = LPX_STREAM_OPT(parent_sk)->child_sklist; s->next_sibling != NULL; s = s->next_sibling);
    // Insert new socket to the sibling list.
    s->next_sibling = sk;
}

LOCAL int destroy_count = 0;

LOCAL void stm_destroy_sock(struct lpx_sock *sk)
{
    struct xbuf *xb;
    int back_log = 0, receive_queue = 0, write_queue = 0, retransmit_queue = 0;

    debug_lpx(2, "ing fd=%d", sk->lpxfd);

    sal_assert(sk->creationref == 0);

    /* Cancel timer DPC */
    if (LPX_STREAM_OPT(sk)->lpx_stream_timer != DPC_INVALID_ID) {

        sal_assert(dpc_cancel(LPX_STREAM_OPT(sk)->lpx_stream_timer) != NDAS_OK);

        dpc_destroy(LPX_STREAM_OPT(sk)->lpx_stream_timer);
        LPX_STREAM_OPT(sk)->lpx_stream_timer = DPC_INVALID_ID;
    }
    destroy_count ++;

    debug_lpx(5, "sequence = %x, fin_seq = %x, rmt_seq = %x, rmt_ack = %x", 
        LPX_STREAM_OPT(sk)->sequence, LPX_STREAM_OPT(sk)->fin_seq, LPX_STREAM_OPT(sk)->rmt_seq, LPX_STREAM_OPT(sk)->rmt_ack);
    debug_lpx(5, "last_retransmit_seq=%x, reason = %x", 
        LPX_STREAM_OPT(sk)->last_rexmit_seq, LPX_STREAM_OPT(sk)->timer_reason);

    /*
        Clean up packet queues.
     */
    while((xb = xbuf_dequeue(&sk->backlog))!=NULL) {
        back_log++;
        xbuf_free(xb);
    }

    while((xb = xbuf_dequeue(&sk->receive_queue)) != NULL) {
        receive_queue ++;
        xbuf_free(xb);
    }

    while((xb = xbuf_dequeue(&sk->write_queue)) != NULL) {
        write_queue ++;
        xbuf_free(xb);
    }

    while((xb = xbuf_dequeue(&LPX_STREAM_OPT(sk)->rexmit_queue)) != NULL) {
        retransmit_queue ++;
        xbuf_free(xb);
    }
    xbuf_queue_destroy(&LPX_STREAM_OPT(sk)->rexmit_queue);
#ifdef USE_DELAYED_ACK
    if (sk->delayed_ack_xb) {
        xbuf_free(sk->delayed_ack_xb);
        sk->delayed_ack_xb = NULL;
    }
#endif

    debug_lpx(2, "back_log = %d, receive_queue = %d, write_queue = %d, retransmit_queue = %d", 
            back_log, receive_queue, write_queue, retransmit_queue);

    /* destroy the rest of the socket. */
    lpx_sock_destroy(sk);
#ifdef XPLAT_OPT_THROTTLE_SOCK_CREATE
    v_destroy_pending_count--;
#endif
    debug_lpx(3, "ed");
}


//
// Entered with the socket lock acquired.
//
LOCAL ndas_error_t _stm_bind(struct lpx_sock *sk, struct sockaddr_lpx *uaddr, int addr_len)
{
    ndas_error_t err;

    err = _dgm_bind(sk, uaddr, addr_len);

    return err;
}

LOCAL ndas_error_t stm_connect(struct lpx_sock *sk, struct sockaddr_lpx *uaddr, int addr_len, int flags)
{
    struct sockaddr_lpx saddr;
    int size;
    ndas_error_t err = NDAS_OK;
    unsigned long lock_flags;
    
    debug_lpx(2, "stm_connecting to %s:%d",
            SAL_DEBUG_HEXDUMP_S(uaddr->slpx_node , 6), uaddr->slpx_port);

/*    if(flags & O_NONBLOCK)
        return -1; */

    if(sk->type != LPX_TYPE_STREAM)
        return NDAS_ERROR_INVALID_OPERATION;

    lock_sock(sk, &lock_flags);

    size = sizeof(saddr);
    err  = dgm_getname(sk, (struct sockaddr_lpx *)&saddr, &size, 0);
    if(!NDAS_SUCCESS(err)) 
         goto out;
        
    sal_memcpy(LPX_OPT(sk)->source_addr.node, saddr.slpx_node, LPX_NODE_LEN);
    LPX_OPT(sk)->source_addr.port = saddr.slpx_port;

    err = dgm_connect(sk, uaddr, addr_len, flags);
    if(!NDAS_SUCCESS(err)) 
         goto out;

    sk->state     = SMP_SYN_SENT;
    sk->sock_state = LSS_CONNECTING;
    unlock_sock(sk, &lock_flags);

    /* Send Connection request */
    err = stm_transmit(sk, NULL, CONREQ, 0);
    
    lock_sock(sk, &lock_flags);

    if(!NDAS_SUCCESS(err))
         goto out;

    while(sk->state == SMP_SYN_SENT) {

        if(!NDAS_SUCCESS(sk->err)) {
            err = lpx_sock_error(sk);
            goto out;
        }

        if(sk->dead) {
            err = NDAS_ERROR_NOT_CONNECTED;
            goto out;
        }

        if(sk->state == SMP_SYN_SENT) {
            if(sk->sleep != SAL_INVALID_EVENT) {
                unlock_sock(sk, &lock_flags);
                sal_event_wait(sk->sleep, SAL_SYNC_FOREVER);
                sal_event_reset(sk->sleep);
                lock_sock(sk, &lock_flags);
            } else {
                /* connection is closed. */
                break;
            }
        }
    } 

    if(sk->state == SMP_ESTABLISHED
        || sk->state == SMP_CLOSE_WAIT) 
        sk->sock_state = LSS_CONNECTED;
    else {
        if(!NDAS_SUCCESS(sk->err)) {
            err = lpx_sock_error(sk);
        } else
            err = NDAS_ERROR_NOT_CONNECTED;
    }
out:
    unlock_sock(sk, &lock_flags);
    return err;
}

/* Accept a pending SMP connection 
 * TODO: revise for sal_sock_create and etc
 * Returns
 * positive: the file descriptor of the socket created by accepting.
 * 0 : never happend
 * negative : NDAS_ERROR_CODE
 */
LOCAL ndas_error_t stm_accept(struct lpx_sock *sk, int flags)
{
    ndas_error_t         err = NDAS_OK;
    struct lpx_sock     *newsk;
    unsigned long sk_flags, newsk_flags;

    debug_lpx(4, "ing sk=%p flags=%d", sk, flags);

    if(sk->type != LPX_TYPE_STREAM) {
        sal_assert(0);
        return NDAS_ERROR_INVALID_OPERATION;
    }
    if((sk->sock_state != LSS_UNCONNECTED) || !sk->acceptcon) {
        sal_assert(0);
        return NDAS_ERROR_INTERNAL;
    }
    if(sk->state != SMP_LISTEN) {
        debug_lpx(1, "state is not listen.");
        sal_assert(0);
        return NDAS_ERROR_INTERNAL;
    }

    err = lpx_create(&newsk, LPX_TYPE_STREAM, 0);
    if(!NDAS_SUCCESS(err)) {
        debug_lpx(1, "lpx_create failed");
        return err;
    }

    lock_sock(sk, &sk_flags);

    lock_sock(newsk, &newsk_flags);

//    sal_assert(newsk->lock != SAL_INVALID_MUTEX);

    // interface will be set at the return time.
    LPX_OPT(newsk)->interface = NULL;
    LPX_OPT(newsk)->source_addr.port = LPX_OPT(sk)->source_addr.port;
    LPX_OPT(newsk)->virtual_mapping = 0;

    LPX_STREAM_OPT(newsk)->parent_sk = sk;
    newsk->next_sibling = NULL;

    stm_insert_to_parent_sk(sk, newsk);
    debug_lpx(1, "Added child sk %d", newsk->lpxfd);

    newsk->zapped = 0;
    newsk->state = SMP_LISTEN;

    while(newsk->state == SMP_LISTEN) {

        if(!NDAS_SUCCESS(sk->err)) {
            err = lpx_sock_error(sk);
            unlock_sock(newsk, &newsk_flags);
            unlock_sock(sk, &sk_flags);
            stm_release(newsk, 1);
            return err;
        }

        if(newsk->dead) {
            err = NDAS_ERROR_NOT_CONNECTED;
            unlock_sock(newsk, &newsk_flags);
            unlock_sock(sk, &sk_flags);
            stm_release(newsk, 1);
            debug_lpx(4, "Returing err=%d", err);
            return err;
        }

        unlock_sock(newsk, &newsk_flags);
        unlock_sock(sk, &sk_flags);

        if(newsk->state == SMP_LISTEN) {
            if(newsk->sleep != SAL_INVALID_EVENT) {
                debug_lpx(4, "Waiting event for sock %p", newsk);
                sal_event_wait(newsk->sleep, SAL_SYNC_FOREVER);
                sal_event_reset(newsk->sleep);
            } else {
                sal_assert(0);
            }
        }

        lock_sock(sk, &sk_flags);
        lock_sock(newsk, &newsk_flags);

        /* Stop waiting if parent sock has stopped.
           Work around for stopping accepting process */
        if(sk->state != SMP_LISTEN) {
            unlock_sock(newsk, &newsk_flags);
            unlock_sock(sk, &sk_flags);

            err = NDAS_ERROR_SHUTDOWN;
            stm_release(newsk, 1);
            debug_lpx(1, "Returing err=%d", err);
            return err;
        }
    }

    if(!NDAS_SUCCESS(sk->err) || sk->state != SMP_LISTEN) {
        err = lpx_sock_error(sk);
        if (NDAS_SUCCESS(err)) {
            err = NDAS_ERROR;
        }
        unlock_sock(newsk, &newsk_flags);
        unlock_sock(sk, &sk_flags);
        stm_release(newsk, 1);
        debug_lpx(1, "Returing err=%d", err);
        return err;
    }

    unlock_sock(newsk, &newsk_flags);
    unlock_sock(sk, &sk_flags);

    // Add interface to the new socket.
    lpxitf_insert_sock(LPX_OPT(newsk)->interface, newsk);

//    lpx_update_dest_cache(newsk); // dest cache is not used..
    
    return newsk->lpxfd;
}


LOCAL ndas_error_t stm_getname (struct lpx_sock *sk, struct sockaddr_lpx *uaddr, int *usockaddr_len, int peer)
{
    ndas_error_t err;

    err = dgm_getname(sk, uaddr, usockaddr_len, peer);

    return err;
}


/* Move a socket into listening state. 
*/
LOCAL ndas_error_t stm_listen(struct lpx_sock *sk, int backlog)
{
    unsigned long flags;
    debug_lpx(2, "stm_listen");

    if(sk->type != LPX_TYPE_STREAM)
        return NDAS_ERROR_INVALID_OPERATION;

    if((unsigned) backlog == 0)     /* BSDism */
        backlog = 1;

    if((unsigned) backlog > SOMAXCONN)
        backlog = SOMAXCONN;

//    backlog = 1; // forcely allocating

    lock_sock(sk, &flags);

    if(sk->state != SMP_LISTEN) {
        sk->state = SMP_LISTEN;
    }

    sk->acceptcon = 1;
    unlock_sock(sk, &flags);

    return NDAS_OK;
}

/*
 * return positive for the size of data sent
 * negative for the error code
 * 0 if the request block size is zero or sum of size of blocks is zero
 */
LOCAL ndas_error_t stm_send_user(struct lpx_sock *sk, lpx_aio *aio)
{
    ndas_error_t retval;
#if DEBUG_LEVEL_LPX <= 8 && defined(DEBUG)
    int i = 0;
    for(i = 0 ; i < aio->nr_blocks; i++) {
        debug_lpx(8, "%dth block(%p,%ldbytes)",i, aio->buf.blocks[i].ptr, aio->buf.blocks[i].len);
        debug_lpx(8, "data={%s}",SAL_DEBUG_HEXDUMP(aio->buf.blocks[i].ptr, aio->buf.blocks[i].len));
    }
#endif //DEBUG    
    if(sk == NULL)
        return NDAS_ERROR_INTERNAL;


    debug_lpx(3, "ing blocks=%p",aio->buf.blocks);    

    if(sk->state != SMP_ESTABLISHED) {
        retval = NDAS_ERROR_NOT_CONNECTED;
        goto out;
    } 

    if (sk->shutdown) {
        retval = NDAS_ERROR_NOT_CONNECTED;
        goto out;
    }
    if(!NDAS_SUCCESS(sk->err)) {
        retval = lpx_sock_error(sk);
        goto out;
    }

    retval = stm_do_send(sk, aio);

out:
    
    debug_lpx(4, "returned lpx_stream_sendmsg return=%d",retval);

    return retval;
}

/*
 * Send xbuffer destruct handler
 */
static
void
lpx_send_xbuf_destruct_handler(
    struct xbuf * xbuf,
    void * userdata
){
    sal_assert(userdata);

    // Do not use xbuf->len for completed length.
    // LPX may modify xbuf->len to add padding.
    lpx_complete_aio_datalen((lpx_aio *)userdata, xbuf->userlen);
}

// TODO: flags of MSG_NOSIGNAL, SO_NOSPACE, SOCK_ASYN_NOSPACE, MSG_NOTWAIT
// TODO: check for send buffer overflow
/*
 * return positive for the size of data sent
 * negative for the error code
 * 0 if the request block size is zero or sum of size of blocks is zero
 * Called with sk->lock locked
 */
LOCAL ndas_error_t stm_do_send(struct lpx_sock *sk, lpx_aio *aio)
{
    struct xbuf     *xb;
    int         mss_now;
    ndas_error_t err = NDAS_OK;
    int         copied = 0;
    int         data_offset;
    int         copy, i;
    int         seglen;
    char *      from;
    unsigned long flags;
    int         nr_memblks;
    
    debug_lpx(4, "ing: total_len=%d", aio->len);
//    sal_assert( sal_semaphore_getvalue(sk->lock) == 0);

    lock_sock(sk, &flags);
    
    if(sk->state != SMP_ESTABLISHED) {
        debug_lpx(1, "lpx_stream_do_sendmsg socket is not connected");
        err = NDAS_ERROR_NOT_CONNECTED;
        goto out;
    }
    if (LPX_OPT(sk)->interface == NULL) {
        /* interface that is bound to this socket is closed */
        err = NDAS_ERROR_NETWORK_DOWN;
        goto out;
    }

#if 1
    mss_now = lpxitf_mss(sk);
#else
    // 2.0 small packet bug test code.
    if (total_len >=512) {
        mss_now = 488;
        sal_error_print("Sending %d %d\n", mss_now, aio->len - mss_now);
    } else {
        mss_now = lpxitf_mss(sk);
    }
#endif

    copied = 0;
    data_offset = 0;

    // Process both mem block structure and simple memory pointer.
    nr_memblks = (aio->nr_blocks == 0) ? 1 : aio->nr_blocks;
    for (i = 0;
        i < nr_memblks;
        i++)
    {
        if(aio->nr_blocks) {
            seglen = aio->buf.blocks[i].len;
            from = aio->buf.blocks[i].ptr;
        } else {
            seglen = aio->len;
            from = (char *)aio->buf.buf + aio->offset;
        }

        debug_lpx(8,"%dth block(%p,%dbytes)={%s}",i, from, seglen,
            SAL_DEBUG_HEXDUMP(from,seglen));
        while(seglen > 0) {

            sal_assert(!err);
            /* Stop on errors. */
            if (!NDAS_SUCCESS(sk->err)) {
                debug_lpx(1, "stop on error sk->err=0x%x",sk->err );
                goto do_sock_err;
            }
            /* Make sure that we are established. */
            if (sk->shutdown) {
                debug_lpx(1, "not established");
                goto do_shutdown;
            }
            copy = mss_now;
            if(copy > seglen)
                copy = seglen;

            // Try non-blocking allocation.
            xb = lpx_sock_alloc_xb(sk, copy, 1); // TODO: check for send buffer overflow 
            if (!xb) {
                debug_lpx(1, "lpx_sock_alloc_xb() failed.");
                // Try blocking allocation 
                // lpx_sock_alloc_xb can block if socket buffer is not enough. unlock sock here
                unlock_sock(sk, &flags);

                xb = lpx_sock_alloc_xb(sk, copy, 0);

                lock_sock(sk, &flags);

                if(!xb) {
                    debug_lpx(1, "out of memory.");
                    sk->err = NDAS_ERROR_OUT_OF_MEMORY;
                    goto out;
                }

                /* Check status is not changed while lock is released */
                if(sk->state != SMP_ESTABLISHED) {
                    debug_lpx(1, "lpx_stream_do_sendmsg socket is not connected");
                    err = NDAS_ERROR_NOT_CONNECTED;
                    xbuf_free(xb);
                    goto out;
                }
                if (LPX_OPT(sk)->interface == NULL) {
                    /* interface that is bound to this socket is closed */
                    err = NDAS_ERROR_NETWORK_DOWN;
                    xbuf_free(xb);
                    goto out;
                }
            }
            sal_assert(xb);

            // Set destruct handler if aio is asynchronous.
            if(aio->aio_complete) {
                xbuf_set_destruct_handler(
                    xb,
                    lpx_send_xbuf_destruct_handler,
                    (void *)aio);
            }

            /* Copy data from user buffer to xbuf's data buffer */
            xbuf_copy_data_from(xb, from, copy); 
            seglen -= copy;

            // If the packet is not last packet in the memory block nor
            // the memory block is not last memory block,
            // set continous packet flag.
            if(seglen || nr_memblks < i + 1) {
                xbuf_set_following_packet_exist(xb);
            }
            unlock_sock(sk, &flags);

            err = stm_transmit(sk, xb, DATA, copy);

            lock_sock(sk, &flags);

            from += copy;
            copied += copy;

            if(err) {
                debug_lpx(1, "lpx_stream_tramsit error err=0x%x", err);
                goto out;
            }
        }
    }
    sk->err = NDAS_OK;
    err = copied;
    goto out;

do_sock_err:
    if(copied)
        err = copied;
    else
        err = lpx_sock_error(sk);
    goto out;

do_shutdown:
    if(copied)
        err = copied;
    else
        err = NDAS_ERROR_NOT_CONNECTED;
    goto out;
out:
    unlock_sock(sk, &flags);

    if(err < 0) {
        lpx_complete_aio_leftover(aio, err);
    } else {
        if(copied < aio->len)
            lpx_complete_aio_datalen_error(aio, aio->len - copied, NDAS_ERROR_TRY_AGAIN);
    }
    return err;
}


NDAS_SAL_API ndas_error_t lpx_net_buf_sender(void* context, sal_net_buff nbuff, sal_net_buf_seg* bseg)
{
    lpx_aio * aio = (lpx_aio *)context;
    struct lpx_sock* sk = (struct lpx_sock *)aio->systemuse_sk;
    struct xbuf* xb;
    ndas_error_t err;

    debug_lpx(2, "sending data_len=%d, nr_frags=%d", (int)bseg->data_len, (int)bseg->nr_data_segs);
    sal_assert(aio);
    sal_assert(sk);
    sal_assert(bseg->nr_data_segs);

    xb = xbuf_alloc_from_net_buf(nbuff, bseg);
    if (xb == NULL) {
        debug_lpx(1, "out of memory");
        err = NDAS_ERROR_OUT_OF_MEMORY;
        goto error_out;
    }
    // Send module can detach the system network buffer
    // because the xbuffer uses user-supplied data buffer.
    // We can rebuild for retransmission.
    xbuf_set_sys_netbuf_rebuild(xb);
    if(bseg->packet_flags & SAL_NETBUFSEG_FLAG_FOLLOWING_PACKET_EXIST) {
        xbuf_set_following_packet_exist(xb);
    }

    /* Check status is not changed while lock is released */
    if(sk->state != SMP_ESTABLISHED) {
        debug_lpx(1, "lpx_stream_do_sendmsg socket is not connected");
        err = NDAS_ERROR_NOT_CONNECTED;
        goto error_out;
    }

    if (LPX_OPT(sk)->interface == NULL) {
        /* interface that is bound to this socket is closed */
        debug_lpx(1, "interface that is bound to this socket is closed");
        err = NDAS_ERROR_NETWORK_DOWN;
        goto error_out;
    }

    // Set destruct handler if aio is asynchronous.
    if(aio->aio_complete) {
        xbuf_set_destruct_handler(
            xb,
            lpx_send_xbuf_destruct_handler,
            (void *)aio);
    }

    stm_transmit(sk, xb, DATA, bseg->data_len);

    // Complete a part of data length here if synchronous
    if(!aio->aio_complete) {
        lpx_complete_aio_datalen(aio, bseg->data_len);
    }

    return NDAS_OK;

error_out:

    if(xb) {
        xbuf_reset_destruct_handler(xb);
        xbuf_free(xb);
    }
    // Complete a part of data length here with an error.
    lpx_complete_aio_datalen_error(aio, bseg->data_len, err);

    return err;
}

LOCAL ndas_error_t  stm_do_sendpages(
    struct lpx_sock * sk,
    lpx_aio * aio
) {
    ndas_error_t err = NDAS_OK;
    lpx_interface* itf;
    debug_lpx(2, "fd=%d, pages=%p, offset=%d, size=%d",
        sk->lpxfd, aio->buf.pages, (int)aio->offset, (int)aio->len);

    itf = LPX_OPT(sk)->interface;
    if (itf == NULL) {
        debug_lpx(1, "Currently sendpages is assuming interface is not null");
        /* interface that is bound to this socket is closed */
        return NDAS_ERROR_NETWORK_DOWN;        
    }

    err = sal_process_page_to_net_buf_mapping(
        (void*)aio, sk->system_sock, LPX_HEADER_SIZE, itf->mss, 
        lpx_net_buf_sender, aio->buf.pages, aio->offset, aio->len);

    if(err < 0) {
        debug_lpx(1, "sal error! err=%d", err);
        lpx_complete_aio_leftover(aio, err);
    } else {
        debug_lpx(4, "sal completed %d", err);
        if(err < aio->len) {
            debug_lpx(1, "sal completed %d", err);
            lpx_complete_aio_datalen_error(aio, aio->len - err, NDAS_ERROR_TRY_AGAIN);
        }
    }

    return err;
}


#ifdef DEBUG
static const char* v_packet_type[] = {
    "DATA", "ACK", "2:INVALID", "3:INVALID", "CONREQ", "5:INVALID",
    "DISCON", "7:INVALID", "RETRAN", "ACKREQ" , "DELAYED_ACK"
};
#endif


static
inline
struct lpxhdr *
stm_fill_packet_common_header(
    struct lpx_sock *sk,
    struct xbuf *xb,
    int type,
    int len
)
{
    struct lpxhdr        *lpxhdr;


    lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);

    if(type == DATA)
    {
        lpxhdr->pu.pktsize = g_htons(LPX_HEADER_SIZE+len);
    }
    else
    {
        lpxhdr->pu.pktsize = g_htons(LPX_HEADER_SIZE);
    }

    lpxhdr->pu.p.type       = LPX_TYPE_STREAM;

//    lpxhdr->checksum        = 0;
    lpxhdr->dest_port       = LPX_OPT(sk)->dest_addr.port;
    lpxhdr->source_port     = LPX_OPT(sk)->source_addr.port;
//    lpxhdr->type          = LPX_TYPE_SEQPACKET;
    lpxhdr->u.s.server_tag  = LPX_STREAM_OPT(sk)->server_tag;
    if(xbuf_does_following_packet_exist(xb)) {
        lpxhdr->u.s.option      = LPX_OPTION_PACKETS_CONTINUE_BIT;
    } else {
        lpxhdr->u.s.option      = 0;
    }

    return lpxhdr;
}

//
// Send an LPX stream packet.
// Execution context: Interrupt, Non-interrupt
//

LOCAL ndas_error_t stm_transmit(struct lpx_sock *sk, struct xbuf *xb, int type, int len)
{
    struct lpxhdr        *lpxhdr;
    int lockheld = 0;
    unsigned long flags;

#ifdef USE_DELAYED_ACK
    sal_spinlock_take_softirq(sk->delayed_ack_lock);
    if (type==DELAYED_ACK && sk->delayed_ack_xb) {
        sal_assert(xb==NULL);
        // update existing ack packet
        lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(sk->delayed_ack_xb);
        lpxhdr->u.s.sequence        = g_htons(LPX_STREAM_OPT(sk)->sequence);
        lpxhdr->u.s.ackseq        = g_htons(LPX_STREAM_OPT(sk)->rmt_seq);
        sk->unsent_ack_count++;
        sal_spinlock_give_softirq(sk->delayed_ack_lock);
        return NDAS_OK;
    } else {
        // All packet contains ACK
        sk->unsent_ack_count = 0;
        sal_spinlock_give_softirq(sk->delayed_ack_lock);
    }
#endif

    if(!xb) {
        // Allocate a packet in non-blocking context
        // This function may be called in interrupt context.
        xb = lpx_sock_alloc_xb(sk, len, 1);
        if(!xb) {
            debug_lpx(1, "xb not allocated");
            return NDAS_ERROR_OUT_OF_MEMORY;
        }
    }
    debug_lpx(6, "sk:%s",DEBUG_LPX_SOCK(sk));
    debug_lpx(6, "xb:%s",DEBUG_XLIB_XBUF(xb));
    debug_lpx(4, "type:%s, len:%d",v_packet_type[type],len);
//    sal_assert( sal_semaphore_getvalue(sk->lock) == 0);
    
    lpxhdr = stm_fill_packet_common_header(sk, xb, type, len);

    // Lock the socket to keep the packet sequence order.
    lock_sock(sk, &flags);
    lockheld = 1;

    lpxhdr->u.s.sequence        = g_htons(LPX_STREAM_OPT(sk)->sequence);
    lpxhdr->u.s.ackseq        = g_htons(LPX_STREAM_OPT(sk)->rmt_seq);
//    lpxhdr->u.s.sconn        = LPX_STREAM_OPT(sk)->source_connid;
//    lpxhdr->u.s.dconn        = LPX_STREAM_OPT(sk)->dest_connid;
//    lpxhdr->u.s.allocseq        = g_htons(LPX_STREAM_OPT(sk)->alloc);
    xb->prot_seqnum = LPX_STREAM_OPT(sk)->sequence;

    switch(type) {
    case ACKREQ:    /* ACKREQ */
        lpxhdr->u.s.lsctl     = g_htons(LSCTL_ACKREQ | LSCTL_ACK);
        break;
#ifdef USE_DELAYED_ACK
    case DELAYED_ACK:
        lpxhdr->u.s.lsctl     = g_htons(LSCTL_ACK);

        unlock_sock(sk, &flags);
        lockheld = 0;

        sal_spinlock_take_softirq(sk->delayed_ack_lock);
        sal_assert(sk->delayed_ack_xb == NULL);
        // Keep this packet for delayed ACK
        sk->delayed_ack_xb = xb;
        sk->unsent_ack_count++;
        sal_spinlock_give_softirq(sk->delayed_ack_lock);
        return NDAS_OK;
#endif

    case ACK:    /* ACK */
        lpxhdr->u.s.lsctl     = g_htons(LSCTL_ACK);
        break;
    case CONREQ:    /* Connection Request */
    case DATA:    /* Data */
    case DISCON:    /* Inform Disconnection */

        if(type == CONREQ) {
            lpxhdr->u.s.lsctl     = g_htons(LSCTL_CONNREQ | LSCTL_ACK);
        } else if(type == DATA) {
            lpxhdr->u.s.lsctl     = g_htons(LSCTL_DATA | LSCTL_ACK);
        } else if(type == DISCON) {
            lpxhdr->u.s.sequence    = g_htons(LPX_STREAM_OPT(sk)->fin_seq);
            lpxhdr->u.s.lsctl     = g_htons(LSCTL_DISCONNREQ | LSCTL_ACK);
            LPX_STREAM_OPT(sk)->fin_seq++;
            debug_lpx(2, "fd=%d DISCON sending", sk->lpxfd);
        }

        LPX_STREAM_OPT(sk)->sequence++;

        unlock_sock(sk, &flags);
        lockheld = 0;

        debug_lpx(4, "type = %d, LPX_STREAM_OPT(sk)->rmt_ack = %d, lpxhdr->sequence = %d, LPX_STREAM_OPT(sk)->sequence = %d",
                type, LPX_STREAM_OPT(sk)->rmt_ack, g_ntohs(lpxhdr->u.s.sequence), LPX_STREAM_OPT(sk)->sequence);


        /* patch the NDAS 2.0 chip bug */
        if( xb->len >= 4 &&
            xb->len <= LPX_20PATCH_PACKET_DATA_SIZE-4 && 
            type == DATA )
        {  
            if (xb->sys_buf && (xbuf_is_clone(xb)?xb->clonee->data:xb->data) == NULL) {
                // To do:
                // NDAS 2.0 chip bug work-around is not implemented when using system network buffer
                // We need to add interface to set data to system network buffer like skb_copy_bits
                sal_assert(0);
            } else {
                int oldlen = xb->len;
                char *data = XBUF_NETWORK_DATA(xb);
                xbuf_put(xb,LPX_20PATCH_PACKET_DATA_SIZE - oldlen);
                data[xb->len - 4] = data[oldlen - 4];
                data[xb->len - 3] = data[oldlen - 3];
                data[xb->len - 2] = data[oldlen - 2];
                data[xb->len - 1] = data[oldlen - 1];
                
                debug_lpx(1, "chip2.0fix:%d->%d",
                    oldlen, (int)LPX_20PATCH_PACKET_DATA_SIZE);
            }
        }

        if(!xbuf_queue_empty(&sk->write_queue) || lpx_stream_snd_test(sk, xb) == FALSE) 
        {
            debug_lpx(3, "stm_transmit: queueing to write queue");
            xbuf_queue_tail(&sk->write_queue, xb);
            return 0;
        }
        break;

    default:
        unlock_sock(sk, &flags);
        sal_assert(0);
        return NDAS_ERROR_INVALID_OPERATION;
    }
    if(lockheld)
        unlock_sock(sk, &flags);

    return stm_route_xb(sk, xb, type);
}


LOCAL ndas_error_t stm_retransmit_rebuilt_xb(struct lpx_sock *sk, struct xbuf *xb)
{
    struct lpxhdr        *lpxhdr;

    debug_lpx(4, "sk:%s",DEBUG_LPX_SOCK(sk));
    debug_lpx(4, "xb:%s",DEBUG_XLIB_XBUF(xb));
    debug_lpx(4, "type:%s, len:%d",v_packet_type[DATA], xb->userlen);

    lpxhdr = stm_fill_packet_common_header(sk, xb, DATA, xb->userlen);

    lpxhdr->u.s.sequence        = g_htons(xb->prot_seqnum);
    lpxhdr->u.s.ackseq        = g_htons(LPX_STREAM_OPT(sk)->rmt_seq);
//    lpxhdr->u.s.sconn        = LPX_STREAM_OPT(sk)->source_connid;
//    lpxhdr->u.s.dconn        = LPX_STREAM_OPT(sk)->dest_connid;
//    lpxhdr->u.s.allocseq        = g_htons(LPX_STREAM_OPT(sk)->alloc);
    lpxhdr->u.s.lsctl     = g_htons(LSCTL_DATA | LSCTL_ACK);
    debug_lpx(4, "type = %d, LPX_STREAM_OPT(sk)->rmt_ack = %d, lpxhdr->sequence = %d, LPX_STREAM_OPT(sk)->sequence = %d",
        DATA, LPX_STREAM_OPT(sk)->rmt_ack, g_ntohs(lpxhdr->u.s.sequence), LPX_STREAM_OPT(sk)->sequence);


    /* patch the NDAS 2.0 chip bug */
    if( xb->len >= 4 &&
        xb->len <= LPX_20PATCH_PACKET_DATA_SIZE-4)
    {
        if (xb->sys_buf && (xbuf_is_clone(xb)?xb->clonee->data:xb->data) == NULL) {
            // To do:
            // NDAS 2.0 chip bug work-around is not implemented when using system network buffer
            // We need to add interface to set data to system network buffer like skb_copy_bits
            sal_assert(0);
        } else {
            int oldlen = xb->len;
            char *data = XBUF_NETWORK_DATA(xb);
            xbuf_put(xb,LPX_20PATCH_PACKET_DATA_SIZE - oldlen);
            data[xb->len - 4] = data[oldlen - 4];
            data[xb->len - 3] = data[oldlen - 3];
            data[xb->len - 2] = data[oldlen - 2];
            data[xb->len - 1] = data[oldlen - 1];

            debug_lpx(1, "chip2.0fix:%d->%d\n",
                oldlen, (int)LPX_20PATCH_PACKET_DATA_SIZE);
        }
    }

    return stm_route_xb(sk, xb, RETRAN);
}

//
// Must call this function in interruptible thread context.
// That is, no spin lock must be acqured.
//

LOCAL ndas_error_t stm_route_xb(struct lpx_sock *sk, struct xbuf *xb, int type)
{
    lpx_interface    *interface;
    int ret;
    struct xbuf *xb2;

    sal_assert(sk!=NULL);
    sal_assert(xb!=NULL);

    debug_lpx(4, "xb=%p type=%x",xb, type);

    if (sk->zapped == 1 && (!LPX_OPT(sk)->virtual_mapping)) {
        debug_lpx(1, "stm_route_xb error xb=%p type=%x", xb, type);
        xbuf_free(xb);
        return NDAS_ERROR_INVALID_PARAMETER;
    }

    switch(type) {

    case CONREQ:
    case DATA:
    case DISCON:

        debug_lpx(3, "before clone");

        // If the data buffer's system network buffer is not detachable,
        // LPX cannot rebuild a system network buffer for retransmission.
        // So clone it.
        if(xbuf_is_sys_netbuf_rebuild(xb)) {
            // Increase reference count for retransmission.
            xb2 = xbuf_get(xb);
        }
        else
        {
            xb2 = xbuf_clone(xb);
            if(!xb2) {
                debug_lpx(1, "Clone failed");
                xbuf_queue_tail(&LPX_STREAM_OPT(sk)->rexmit_queue, xb);
                LPX_STREAM_OPT(sk)->rexmit_timeout = sal_tick_add(sal_get_tick(), stm_calc_retransmission_time(sk));
                return NDAS_OK;
            }
            debug_lpx(4, "xb %p Cloned", xb);
        }

        xbuf_queue_tail(&LPX_STREAM_OPT(sk)->rexmit_queue, xb);

        LPX_STREAM_OPT(sk)->rexmit_timeout = sal_tick_add(sal_get_tick(),stm_calc_retransmission_time(sk));

        xb = xb2;
        break;
    case RETRAN:
    case ACK:
    case ACKREQ:
    default:
        break;
    }
#ifdef DEBUG
    if (type == RETRAN) {
        debug_lpx(4, "Sending retransmit packet");
    }
#endif

    interface = lpxitf_find_using_node(LPX_OPT(sk)->source_addr.node);
    if(LPX_OPT(sk)->virtual_mapping) 
    {
        lpx_interface *p_if;
        struct xbuf *sendxb;
        debug_lpx(1,"virtual_mapping, sending to all interface");
        if (lpx_interfaces==NULL) {
            debug_lpx(4,"no interface registered");
        }

        for(p_if = lpx_interfaces; p_if; p_if = p_if->itf_next) {
            debug_lpx(4, "Sending %d itf=%02x:%02x:%02x:%02x:%02x:%02x",
                type,
                p_if->itf_node[0], p_if->itf_node[1],
                p_if->itf_node[2], p_if->itf_node[3],
                p_if->itf_node[4], p_if->itf_node[5]);
            sendxb = xbuf_copy(xb); // Each network interface transmit function may change contents of packet.
                                // So xbuf_clone may not work if network buffer is used for transmit
            if(sendxb) {
                ret = lpxitf_send(p_if, sendxb, LPX_OPT(sk)->dest_addr.node);
                if(ret < 0)
                {
                    debug_lpx(1, "lpxitf_send() failed.");
                }
            } else {
                debug_lpx(1,"stm_route_xb: copy failed");    
            }
        }
        xbuf_free(xb);
        return NDAS_OK;
    }

    if(interface == NULL) {
        debug_lpx(1, "stm_route_xb: interface is NULL, returning");
        xbuf_free(xb);
        return NDAS_ERROR_NO_NETWORK_INTERFACE;
    }
//        sal_error_print("stm_route_xb: Interface found %s\n", interface->dev);
    ret = lpxitf_send(interface, xb, LPX_OPT(sk)->dest_addr.node);
    if(ret < 0)
    {
        debug_lpx(1, "lpxitf_send() failed.");
    }

    return NDAS_OK;
}


LOCAL unsigned long stm_calc_retransmission_time(struct lpx_sock *sk)
{
    unsigned long rtt = RETRANSMIT_TIME;
    int i;
    // Exponential
    for(i = 0; i < LPX_STREAM_OPT(sk)->rexmits; i++) {
        rtt *= 2;
        if(rtt > MAX_RETRANSMIT_DELAY)
            return MAX_RETRANSMIT_DELAY;
    }
    return rtt;
}

/* Return 0 if we cannot send right now */
LOCAL int lpx_stream_snd_test(struct lpx_sock *sk, struct xbuf* xb)
{
    return 1;
#if 0 /* sjcho: under testing */
    xuint16 in_flight;
    struct lpxhdr *lpxhdr;

    lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);
    in_flight = ntohs(lpxhdr->u.s.sequence) - LPX_STREAM_OPT(sk)->rmt_ack;
    if(in_flight >= LPX_STREAM_OPT(sk)->max_flights) {
        debug_lpx(1, "in_flight = %d\n", in_flight);
//        sal_error_print("In flight=%d\n", in_flight);
        return 0;
    }
    return 1;
#endif
}

LOCAL ndas_error_t stm_recv_xbuf(struct lpx_sock* sk, struct xbuf_queue* queue, int total_len)
{
    struct xbuf *xb;
    struct lpxhdr *lpxhdr;
    int copied;
    int index = 0;
    ndas_error_t err = NDAS_OK;
    int ret;
    xuint16    lsctl;
    int diff;
    xbool timed_out;

    sal_tick timeout = sal_tick_add(sal_get_tick(), LPX_SOCK_RECV_WAITTIME);
#ifdef USE_DELAYED_RX_NOTIFICATION
    sal_spinlock_take_softirq(sk->to_recv_count_lock);
    sk->to_recv_count += total_len;
    sal_spinlock_give_softirq(sk->to_recv_count_lock);
#endif

    debug_lpx(3, "ing total_len=%d",total_len);

//    sal_assert(sk->lock != SAL_INVALID_MUTEX);

    sk->opt.stream.last_data_received = 0;

    if(sk->zapped) {
        debug_lpx(3, "sk->zapped called lpx_stream_recvmsg");
        return NDAS_ERROR_NOT_BOUND; /* Socket not bound */
    }
    if ( total_len == 0 )  // return 0 if request 0
        return 0;
restart:
    timed_out = FALSE;

    while(xbuf_queue_empty(&sk->receive_queue))      /* No data */
    {
        /* Socket errors? */
        err = lpx_sock_error(sk);
        if(!NDAS_SUCCESS(err)) {
            debug_lpx(1, "sock error signal_pending err=%d", err);
            goto out;
        }

        /* Socket shut down? */
        if(sk->shutdown) {
            debug_lpx(1, "sock error signal_pending");
            err = NDAS_ERROR_SHUTDOWN;
            goto out;
        }

        diff = sal_tick_sub(timeout, sal_get_tick());
        if (diff>0) {
            ret = sal_event_wait(sk->sleep, diff); 
            if (ret==0) { /* Event signaled */
                sal_event_reset(sk->sleep);
                if (sk->zapped)
                    break; 
            } else {
                timed_out = TRUE;
            }
        } else {
            timed_out = TRUE;
        }
        if (timed_out) {
            break;
        }
    }

    while(total_len && (xb = xbuf_peek(&sk->receive_queue)) != NULL)
    {
        lpxhdr    = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);
        lsctl = g_ntohs(lpxhdr->u.s.lsctl);
        if(lsctl & LSCTL_DISCONNREQ) {
            debug_lpx(1, "Discarding DISCONNREQ packet");
            xbuf_unlink(xb);
            xbuf_free(xb);
            goto out;
        }

        copied     = xb->len;
        if(copied > total_len)
        {   /* 
            this situation never happens as of Jan 10, 2005
            we always pass the total_len the exact size of data we want to read
            */
            struct xbuf* new_xb;
            debug_lpx(1, "Long xbuf len: xb->len=%d, user buf=%d", xb->len, total_len);
            copied = total_len;
            /* Need to split xbuf */
            new_xb = xbuf_alloc(copied);
            if ( !new_xb ) { // drop xb
                debug_lpx(1, "Drop xb as fail to allocate an xbuf to split: readn=%d", total_len);
                xbuf_unlink(xb);
                xbuf_free(xb);
                err = NDAS_ERROR_OUT_OF_MEMORY; /* TODO: or return the data we read so far*/
                goto out;
            }
            new_xb->owner = (void*) sk;
            sal_assert(XBUF_NETWORK_DATA(xb));
            sal_assert(XBUF_NETWORK_DATA(new_xb));
            sal_memcpy(XBUF_NETWORK_DATA(new_xb), XBUF_NETWORK_DATA(xb), copied);
            xbuf_pull(xb, copied);
            // Assign new xb to the current xb.
            xb = new_xb;
        } else {
            xbuf_unlink(xb);
        }
        // No need to lock the queue.
        _xbuf_queue_tail(queue, xb);

        debug_lpx(4, "total_len = %d, copied = %d", total_len, copied);
        index += copied;
        total_len -= copied;
    }

    if(total_len)
        goto restart;

out:
    if(index) {
        err = index;
    } else {
#ifdef USE_DELAYED_RX_NOTIFICATION
        // timeout or error occured. reset to receive count to cancel previous read.
        sal_spinlock_take_softirq(sk->to_recv_count_lock);
        sk->to_recv_count = 0;
        sal_spinlock_give_softirq(sk->to_recv_count_lock);
#endif
    }

    sk->opt.stream.last_data_received    = 0;

    debug_lpx(4, "recv ret=%d", err);
    return err;
}

// Return remaining amount of buffer.
LOCAL
int
stm_look_ahead(struct lpx_sock *sk, lpx_aio *aio)
{
    struct xbuf *xb;
    int request_len_adj = 0;
    int ret;

    xb = xbuf_peek(&sk->receive_queue);
    sal_assert(xb);
    // return value is consumed amount.
    ret = aio->aio_head_lookahead(aio, aio->userparam, XBUF_NETWORK_DATA(xb), xb->len, &request_len_adj);
    aio->aio_head_lookahead = NULL;
    // Consume data of the first xbuffer
    if(ret == xb->len) {
        xbuf_unlink(xb);
        xbuf_free(xb);
    } else {
        xbuf_pull(xb, ret);
    }
    //
    // Add total length increment
    //
    aio->len += request_len_adj;

    return aio->len - ret;
}


LOCAL
int
stm_wait_for_packet(struct lpx_sock *sk, sal_tick timeout)
{
    int ret = NDAS_OK;
    int diff;

    // Wait until data packet arrives.
    do
    {
        /* Socket errors? */
        ret = lpx_sock_error(sk);
        if(!NDAS_SUCCESS(ret)) {
            debug_lpx(1, "sock error signal_pending");
            break;
        }

        /* Socket shut down? */
        if(sk->shutdown) {
            debug_lpx(1, "RCV_SHUTDOWN sock error signal_pending");
            ret = NDAS_ERROR_SHUTDOWN;
            break;
        }

        diff = sal_tick_sub(timeout, sal_get_tick());
        if (diff>0) {
            ret = sal_event_wait(sk->sleep, diff); 
            if (ret==SAL_SYNC_OK) {
                sal_event_reset(sk->sleep);
                if (sk->zapped) {
                    break;
                }
            } else { /* Event signaled or time-out */
                ret = NDAS_ERROR_TIME_OUT;
                break;
            }
        } else {
            ret = NDAS_ERROR_TIME_OUT;
            break;
        }
    } while(xbuf_queue_empty(&sk->receive_queue));


    return ret;
}

/*
call after get xbuf_lock of sk->receive_queue
*/
LOCAL
int
stm_wait_for_completion(struct lpx_sock *sk, sal_tick timeout, lpx_aio *aio)
{
    int ret = NDAS_OK;
    int diff;
    unsigned long flags;
    // Must be synchronous IO.
    sal_assert(aio->aio_complete == NULL);
    // Wait until data packet arrives.
    while(!lpx_is_aio_completed(aio))
    {
        /* Socket errors? */
        ret = lpx_sock_error(sk);
        if(!NDAS_SUCCESS(ret)) {
            debug_lpx(1, "sock error signal_pending");
            break;
        }

        /* Socket shut down? */
        if(sk->shutdown) {
            debug_lpx(1, "RCV_SHUTDOWN sock error signal_pending");
            ret = NDAS_ERROR_SHUTDOWN;
            break;
        }

        diff = sal_tick_sub(timeout, sal_get_tick());
        if (diff>0) {
            debug_lpx(4, "In %p aio=%p f=%x i=%d ofs=%d\n",
                sk->sleep,
                aio,
                aio->aio_flags,
                (int)aio->index,
                (int)aio->offset);
	     xbuf_qunlock(&sk->receive_queue, &flags);		
            ret = sal_event_wait(sk->sleep, diff); 
	     xbuf_qlock(&sk->receive_queue, &flags);		
            debug_lpx(4, "Out %p aio=%p f=%x i=%d ofs=%d\n",
                sk->sleep,
                aio,
                aio->aio_flags,
                (int)aio->index,
                (int)aio->offset);
            if (ret==SAL_SYNC_OK) {
                sal_event_reset(sk->sleep);
                if (sk->zapped) {
                    break;
                }
            } else { /* Event signaled or time-out */
                ret = NDAS_ERROR_TIME_OUT;
                break;
            }
        } else {
            ret = NDAS_ERROR_TIME_OUT;
            break;
        }
#if 0
        if(!lpx_is_aio_completed(aio)) {
            sal_error_print("%p: Wake up when AIO not completed. len=%d comp=%d to_recv=%d\n", sk, aio->len, aio->completed_len, sk->to_recv_count);
        }
#endif
    }

    return ret;
}

/*
 * Description
 * Send message/lpx data to user-land 
 *
 * Returns
 * positive : number of bytes received
 * NDAS_OK : if the size is 0.
 * NDAS_ERROR_NOT_CONNECTED: socket is zapped
 * NDAS_ERROR_SHUTDOWN: socket is shutdown
 * other socket error from sk->err
 */
LOCAL ndas_error_t stm_recv_user(struct lpx_sock *sk, lpx_aio *aio)
{
    struct xbuf *xb;
    int copied;
    char *dest = (char *)aio->buf.buf;
    ndas_error_t err = NDAS_OK;
    int ret;
    int remaining_len = aio->len;
    sal_tick timeout = sal_tick_add(sal_get_tick(), LPX_SOCK_RECV_WAITTIME);
    unsigned long flags;
    xbool syncio;

	if( aio->flags == AIO_FLAG_MEDTERM_IO) {
		timeout = sal_tick_add(sal_get_tick(), LPX_SOCK_RECV_MEDTERM_WAITTIME);
	}else if( aio->flags == AIO_FLAG_LOGTERM_IO ) {
		timeout = sal_tick_add(sal_get_tick(), LPX_SOCK_RECV_LONGTERM_WAITTME);
	}else {
		timeout = sal_tick_add(sal_get_tick(), LPX_SOCK_RECV_WAITTIME);
	}

#ifdef USE_DELAYED_RX_NOTIFICATION
    sal_spinlock_take_softirq(sk->to_recv_count_lock);
    sk->to_recv_count += remaining_len;
    sal_spinlock_give_softirq(sk->to_recv_count_lock);
#endif
    debug_lpx(4, "remaining_len=%d",remaining_len);

    if(sk->zapped) {

        debug_lpx(1, "sk->zapped set");
        lpx_complete_aio_leftover(aio, NDAS_ERROR_NOT_BOUND);
        return NDAS_ERROR_NOT_BOUND; /* Socket not bound */
    }
    if ( remaining_len == 0 ){  // return 0 if request 0

        lpx_complete_aio_leftover(aio, NDAS_OK);
        return 0;
    }

    syncio = (aio->aio_complete == NULL);
    debug_lpx(4, "syncio=%d", (int)syncio);
    sk->opt.stream.last_data_received    = 0;

    // Look ahead the first xbuffer.
    if(aio->aio_head_lookahead) {
        // Wait for a packet.
        if(xbuf_queue_empty(&sk->receive_queue)) {
            err = stm_wait_for_packet(sk, timeout);
            if(err < 0) {
                debug_lpx(1, "stm_wait_for_packet() failed.");
                goto out;
            }
        }

        remaining_len = stm_look_ahead(sk, aio);
        if(remaining_len == 0) {
            goto out;
        }
        dest += aio->len - remaining_len;
    }
#if 0
    if(remaining_len > 1024)
        sal_event_wait(sk->sleep, LPX_SOCK_RECV_WAITTIME);
#endif

    xbuf_qlock(&sk->receive_queue, &flags);
    // Set the AIO to the socket.
    if(sk->aio) {
        debug_lpx(1, "Already aio %p is set.", sk->aio);
        xbuf_qunlock(&sk->receive_queue, &flags);
        goto out;
    }

    // Consume pending packets.
    while(remaining_len && (xb = xbuf_peek(&sk->receive_queue)) != NULL) {

        if(((struct lpxhdr *)XBUF_NETWORK_HEADER(xb))->u.s.lsctl & g_htons(LSCTL_DISCONNREQ)) {
            xbuf_qunlock(&sk->receive_queue, &flags);
            debug_lpx(1, "Recieved DISCONNREQ packet");
            xbuf_unlink(xb);
            xbuf_free(xb);
            goto out;
        }

        copied     = xb->len;
        if(copied > remaining_len)
        {
            /* This can happen if user issue multiple read for one packet */
            debug_lpx(5, "Long xbuf len: xb->len=%d, user buf=%d", xb->len, remaining_len);
            copied = remaining_len;
        }

        // Copy xbuffer data to mem blocks or simple buffer.
        if(aio->aio_user_copy) {
            ret = aio->aio_user_copy(aio, XBUF_NETWORK_DATA(xb), copied);
        } else if(aio->nr_blocks) {
            ret = xbuf_copy_data_to_blocks(xb, 0, aio->buf.blocks, aio->nr_blocks, copied);
        } else {
            ret = xbuf_copy_data_to(xb, 0, dest, copied);
        }
        if(ret) {
            xbuf_qunlock(&sk->receive_queue, &flags);
            debug_lpx(1, "memcpy_to error");
            xbuf_unlink(xb);
            xbuf_free(xb);
            goto out;
        }

        if(xb->len == copied) {
            _xbuf_unlink(xb);
            xbuf_free(xb);
        } else {
#ifdef DEBUG
            if ( remaining_len == 60 ) {
                struct lpxhdr *lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);
                debug_lpx(6, "Reading READ reply seq=%d ackseq=%d xb->len=%d", 
                    g_ntohs(lpxhdr->u.s.sequence), g_ntohs(lpxhdr->u.s.ackseq),
                    xb->len);
            }
#endif
            xbuf_pull(xb, copied);
        }
        xbuf_qunlock(&sk->receive_queue, &flags);

        dest += copied;
        remaining_len -= copied;
        aio->completed_len += copied;
        debug_lpx(4, "remaining_len = %d, copied = %d", remaining_len, copied);

        xbuf_qlock(&sk->receive_queue, &flags);
    }

    //
    // The rest of packets has not arrived.
    // Let the bottom half do the rest job.
    //
    if(remaining_len) {
        sk->aio = aio;
        //xbuf_qunlock(&sk->receive_queue, &flags);
        if(syncio) {
            // For synchronous IO, there is no possiblity to free the AIO.
            // So we can access the AIO safely even if it is completed by the lpx recv handler.
            err = stm_wait_for_completion(sk, timeout, aio);
            if(err >= 0) {
                debug_lpx(4, "ret=%d\n", aio->len);
#if 0
                sal_error_print("rmn=%d ret=%d\n", remaining_len, aio->len);
#endif
                sk->opt.stream.last_data_received    = 0;
		  xbuf_qunlock(&sk->receive_queue, &flags);
                return aio->len;
            } else {
                debug_lpx(1, "stm_wait_for_completion() failed.");
                // Take away the AIO from the socket.
                //xbuf_qlock(&sk->receive_queue, &flags);
                sk->aio = NULL;
                xbuf_qunlock(&sk->receive_queue, &flags);
                remaining_len = aio->len - aio->completed_len;
                // Fall through.
            }
        } else {
            // Just return for asynchronous IO request.
#if 0
            sal_error_print("Asynch recv %d\n", remaining_len);
#endif
            return 0;
        }
    } else {
        // Receive completes.
        sal_event_reset(sk->sleep);
        xbuf_qunlock(&sk->receive_queue, &flags);
#if 0
        sal_error_print("Copied by caller. ret=%d\n", aio->len);
#endif
    }

out:
    if(aio->completed_len) {
        // Override the error code with copied length.
        err = aio->completed_len;
    } else {
#ifdef USE_DELAYED_RX_NOTIFICATION
        // timeout or error occured. reset to receive count to cancel previous read.
        sal_spinlock_take_softirq(sk->to_recv_count_lock);
        sk->to_recv_count = 0;
        sal_spinlock_give_softirq(sk->to_recv_count_lock);
#endif
    }
    sk->opt.stream.last_data_received    = 0;

    /*
     * Complete aio.
     * Carefully touch the aio because it might be completed in the copy loop.
     */
    if(err < 0) {
        lpx_complete_aio_leftover(aio, err);
    } else {
        if(remaining_len)
            lpx_complete_aio_datalen_error(aio, remaining_len, NDAS_ERROR_TRY_AGAIN);
        else
            lpx_complete_aio_datalen(aio, 0);
    }

    debug_lpx(4, "recv ret=%d", err);
    return err;
}

/*
 */
 
 LOCAL
 struct lpx_sock *lookup_childsk(
    struct lpx_sock *sk,
    sal_ether_header* ethhdr,
    struct lpxhdr    *lpxhdr,
    struct xbuf *xb)
 {
    struct lpx_sock     *receive_sk;
    struct lpx_sock     *child_sk;
    unsigned long flags;

    receive_sk = NULL;

    lock_sock(sk, &flags);
    child_sk = LPX_STREAM_OPT(sk)->child_sklist;

    /*
     * Match a pair of a packet's source address and source port number to
     * child socket's dest address and dest port number
     */
    while(child_sk != NULL) {
        if(
        (sal_memcmp(LPX_OPT(child_sk)->dest_addr.node, ethhdr->src, LPX_NODE_LEN) == 0) &&
        (LPX_OPT(child_sk)->dest_addr.port == lpxhdr->source_port))
        {
            debug_lpx(3, "Found child sk = %p, fd=%d", child_sk, child_sk->lpxfd);
            receive_sk = child_sk;
            break;
        }
        child_sk = child_sk->next_sibling;
    }

    debug_lpx(4, "child, child_sk = %p", sk);
    // This is new connection if the packet has CONNREQ control.
    if(receive_sk == NULL &&
        (g_ntohs(lpxhdr->u.s.lsctl) & LSCTL_CONNREQ)) {
        child_sk = LPX_STREAM_OPT(sk)->child_sklist;
        while(child_sk != NULL) {

            debug_lpx(4, "child_sk = %p", child_sk);
            if(child_sk->state == SMP_LISTEN)
            {
                receive_sk = child_sk;

                if(!LPX_OPT(receive_sk)->interface) {
                    // remember interface that packet is coming.
                    // to do: need to handle when network interface is changed..
                    LPX_OPT(receive_sk)->interface = lpxitf_find_using_node(ethhdr->des);
                    sal_memcpy(LPX_OPT(receive_sk)->source_addr.node, 
                            ethhdr->des, LPX_NODE_LEN);
                }
                debug_lpx(3, "from listen sk: %d", receive_sk->lpxfd);
                break;
            }
            child_sk = child_sk->next_sibling;
        }
    }

    debug_lpx(4, "child2, child_sk = %p", sk);
    if(receive_sk == NULL) {
         unlock_sock(sk, &flags);

        // Someone is listening but not called accept...
        if (g_ntohs(lpxhdr->u.s.lsctl) & LSCTL_CONNREQ) {
            debug_lpx(1, "Backlogging CONNREQ to %p", sk);
            xbuf_queue_tail(&sk->backlog, xb);
        } else {
            xbuf_free(xb);
            debug_lpx(1, "ed receive_sk == NULL");
        }
        return NULL;
    } else {
        // Increase reference count for child socket.
        stm_hold(receive_sk);
    }

    unlock_sock(sk, &flags);

    return receive_sk;
 }

/* 
    Can be called from softirq context. Cannot sleep.
*/
LOCAL LPX_INLINE void stm_rcv(struct lpx_sock *sk, struct xbuf *xb)
{
    struct lpxhdr    *lpxhdr;
    struct lpx_sock     *receive_sk;
    int pktsize;
    sal_ether_header* ethhdr;
    int recvsk_refheld = 0;

    debug_lpx(4, "ing sk=%p xb=%p", sk, xb);
    debug_lpx(8, "sk=%s", DEBUG_LPX_SOCK(sk));
    debug_lpx(8, "xb=%s", DEBUG_XLIB_XBUF(xb));
#if 0
{
        struct lpxhdr     *lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);
        static int prevseq=0;    
        sal_debug_print("<- %d %d: %ums", g_ntohs(lpxhdr->u.s.sequence), g_ntohs(lpxhdr->u.s.ackseq), sal_time_msec()%1000000);
        if (prevseq+1 < g_ntohs(lpxhdr->u.s.sequence)) 
            rexmit_bad_count++;
        else 
            rexmit_good_count++;
        prevseq = g_ntohs(lpxhdr->u.s.sequence);
    }
#endif
    ethhdr = XBUF_MAC_HEADER(xb);
    lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);
    pktsize = g_ntohs(lpxhdr->pu.pktsize & ~LPX_TYPE_MASK) - LPX_HEADER_SIZE;

    if(xb->len != pktsize) {
        if(xb->len < pktsize) {
            debug_lpx(1, "Invalid packet size value at header=%d, real value=%d",
                            pktsize, xb->len);
            xbuf_free(xb);
            return;
        }

        /* This is not an error. Ethernet packet size can be larger than actual packet size */
        debug_lpx(6, "Trimming packet size to %d from %d", pktsize, xb->len);
        xbuf_trim(xb, xb->len - pktsize);
    }

    //
    // Determine the destination socket.
    //
    if(LPX_STREAM_OPT(sk)->child_sklist == NULL) {
        // this is not listening socket.
        receive_sk = sk;
    } else {
        receive_sk = lookup_childsk(sk, ethhdr, lpxhdr, xb);
        if(receive_sk) {
            recvsk_refheld = 1;
        } else {
            return;
        }
    }

    //
    // Process backlogged and the current packets 
    //
    {
        struct xbuf* xb2;
        while((xb2=xbuf_dequeue(&receive_sk->backlog))!=NULL) {
            debug_lpx(1,"Handling backlog");
            stm_do_rcv(receive_sk, xb2);
        }
        stm_do_rcv(receive_sk, xb);
    }

    if(recvsk_refheld)
        stm_release(receive_sk, 0);

    debug_lpx(4, "ed");
    return;
}

struct _stm_packet_info {
    xuint16    lsctl;
    xuint16    seq;
    xuint16    ackseq;
    xuint16    packetsize;
};

LOCAL
int
_stm_state_syn_recv(struct lpx_sock* sk, struct _stm_packet_info *pktinfo)
{
    debug_lpx(1, "SMP_SYN_RECV");
    if(!(pktinfo->lsctl & LSCTL_ACK)) {
        debug_lpx(1, "SMP_SYN_RECV not ack");
        return -1;
    }

    if(pktinfo->ackseq < 1) {
        debug_lpx(1, "SMP_SYN_RECV ackseq =%d", 
            pktinfo->ackseq);
        return -1;
    }

    // Dequeue packets from retransmission queue including CONN request.
    stm_retransmit_chk(sk, pktinfo->ackseq, ACK);

    //
    // Move to the ESTABLISHED state.
    //
    sk->sock_state = LSS_CONNECTED;
    sk->state = SMP_ESTABLISHED;
    debug_lpx(1, "SMP_SYN_RECV wake");

    if(!sk->dead)
        lpx_sock_state_change(sk);

    return 0;
}

LOCAL
LPX_INLINE
void
_stm_state_established(
    struct lpx_sock *sk,
    struct xbuf *xb,
    struct _stm_packet_info *pktinfo,
    unsigned long flags
)
{

    debug_lpx(4, "SMP_ESTABLISHED");

    //
    // Dequeue packets from the retransmission if ack flag is set.
    //
    if(pktinfo->lsctl & LSCTL_ACK) {
        LPX_STREAM_OPT(sk)->rmt_ack = pktinfo->ackseq;
        debug_lpx(4, "ackseq = %d", pktinfo->ackseq);

        stm_retransmit_chk(sk, pktinfo->ackseq, ACK);
        pktinfo->lsctl &= ~LSCTL_ACK;
        if(pktinfo->lsctl == 0) {
            unlock_sock(sk, &flags);
            xbuf_free(xb);
            return;
        }
    }

    switch(pktinfo->lsctl) {
    case LSCTL_DATA:
        debug_lpx(4, "LSCTL_DATA");

        // Timeout based packet loss detection
        if (sk->opt.stream.last_data_received != 0) {
            xint32 tick_diff;
            tick_diff = sal_tick_sub(sal_get_tick(), sk->opt.stream.last_data_received);
            if (tick_diff > 0 &&  tick_diff > sk->opt.stream.loss_detection_time ) {
                sal_atomic_inc(&sk->opt.stream.retransmit_count.rx_packet_loss);
#if 1
                sal_error_print("ndas: packet loss detected: time diff=%dms\n", tick_diff * 1000 / SAL_TICKS_PER_SEC);
#endif
            }
        }
        sk->opt.stream.last_data_received = sal_get_tick();
    case LSCTL_DISCONNREQ:    // Fall through
        if (pktinfo->lsctl & LSCTL_DISCONNREQ) {
            debug_lpx(1, "LSTCTL_DISCONNREQ");
        }

        if(pktinfo->seq != LPX_STREAM_OPT(sk)->rmt_seq) {
            #ifdef STAT_REXMIT
            rexmit_bad_count++;
            #endif
            #if DEBUG_LEVEL_LPX_REXMIT >= 4 
                    sal_debug_print("LXRX:%d/%d %d/%d",
                    pktinfo->seq , LPX_STREAM_OPT(sk)->rmt_seq,
                    pktinfo->ackseq, LPX_STREAM_OPT(sk)->rmt_ack);
            #endif
            unlock_sock(sk, &flags);
            xbuf_free(xb);

            stm_transmit(sk, NULL, ACK, 0); // sjcho: why we need this??

            break;
        }

        LPX_STREAM_OPT(sk)->rmt_seq ++;

        unlock_sock(sk, &flags);

#ifdef USE_DELAYED_ACK
        // Use delayed ack only to full packet 
        // And send ACK for every MAX_DELAYED_ACK_COUNT
        // To do:
        //  - Set timeout short when delayed ack is exists. 
        //  - Do not delay ack for last received packet.
        if ((pktinfo->lsctl & LSCTL_DATA) && 
            (pktinfo->packetsize == 1040 || pktinfo->packetsize >= 1484) &&
            sk->unsent_ack_count<MAX_DELAYED_ACK_COUNT)
        {
            stm_transmit(sk, NULL, DELAYED_ACK, 0);
        }
        else
        {
            stm_transmit(sk, NULL, ACK, 0);
        }
#else
        if (pktinfo->lsctl & LSCTL_DISCONNREQ) {
            debug_lpx(1, "Sending ack for LSCTL_DISCONNREQ");
        }

        stm_transmit(sk, NULL, ACK, 0);
#endif

        if(pktinfo->lsctl & LSCTL_DATA) {
            stm_queue_data_rcv(sk, xb);
        } else {
            lpx_sock_queue_rcv_xbuf(sk, xb);
        }

        if(pktinfo->lsctl & LSCTL_DISCONNREQ) {
            sk->state = SMP_CLOSE_WAIT;
            sk->shutdown = 1;
        }

        break;

    case LSCTL_ACKREQ:

        debug_lpx(4, "LSCTL_ACKREQ");

        unlock_sock(sk, &flags);
        xbuf_free(xb);

        stm_transmit(sk, NULL, ACK, 0);
        break;

    default:
        unlock_sock(sk, &flags);
        xbuf_free(xb);

        debug_lpx(1, "lsctl=0x%x",pktinfo->lsctl);
        break;
    }
}

LOCAL
void
_stm_state_listen(
    struct lpx_sock *sk,
    struct xbuf *xb,
    struct _stm_packet_info *pktinfo,
    unsigned long flags
)
{
    struct lpxhdr     *lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);

    if(LPX_STREAM_OPT(sk)->parent_sk == NULL) {
        return;
    }

    switch(pktinfo->lsctl) 
    {
    case LSCTL_CONNREQ:
    case LSCTL_CONNREQ | LSCTL_ACK:

        debug_lpx(4, "SMP_LISTEN");

        sk->sock_state = LSS_CONNECTING;
        sk->state     = SMP_SYN_RECV;

        ((struct lpx_stream_opt *)(LPX_STREAM_OPT(sk)))->rmt_seq ++;
        sal_memcpy(LPX_OPT(sk)->dest_addr.node, XBUF_MAC_HEADER(xb)->src, 6);
        LPX_OPT(sk)->dest_addr.port = lpxhdr->source_port;
        LPX_STREAM_OPT(sk)->server_tag = lpxhdr->u.s.server_tag;
        debug_lpx(4, "sk %p: reply to CONREQ to %02x", sk, XBUF_MAC_HEADER(xb)->src[5]);

        unlock_sock(sk, &flags);

        stm_transmit(sk, NULL, CONREQ, 0);   /* Connection REQUEST */
        break;
    default:
        unlock_sock(sk, &flags);
    }

    xbuf_free(xb);
}

LOCAL
void
_stm_state_sync_sent(
    struct lpx_sock *sk,
    struct xbuf *xb,
    struct _stm_packet_info *pktinfo,
    unsigned long flags
)
{
    struct lpxhdr     *lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);

    switch(pktinfo->lsctl) {

    case LSCTL_CONNREQ:
    case LSCTL_CONNREQ | LSCTL_ACK:
        debug_lpx(4, "SMP_SYN_SENT");
        LPX_STREAM_OPT(sk)->rmt_seq ++;
        LPX_STREAM_OPT(sk)->server_tag = lpxhdr->u.s.server_tag;

        sk->state = SMP_ESTABLISHED;

        if(pktinfo->lsctl & LSCTL_ACK) { 
            LPX_STREAM_OPT(sk)->rmt_ack = pktinfo->ackseq; // must be 1
            stm_retransmit_chk(sk, LPX_STREAM_OPT(sk)->rmt_ack, ACK);
        }

        unlock_sock(sk, &flags);

        stm_transmit(sk, NULL, ACK, 0);

        if(!sk->dead)
            lpx_sock_state_change(sk);

        break;

    default:
        unlock_sock(sk, &flags);
        break;
    }
    xbuf_free(xb);
}

LOCAL
void
_stm_state_last_ack(
    struct lpx_sock *sk,
    struct xbuf *xb,
    struct _stm_packet_info *pktinfo,
    unsigned long flags
)
{
    switch(pktinfo->lsctl) {

    case LSCTL_ACK:

        LPX_STREAM_OPT(sk)->rmt_ack = pktinfo->ackseq;
        stm_retransmit_chk(sk, LPX_STREAM_OPT(sk)->rmt_ack, ACK);

        if(LPX_STREAM_OPT(sk)->rmt_ack == LPX_STREAM_OPT(sk)->fin_seq) {
            sk->state = SMP_CLOSE;
        } else {
            debug_lpx(1, "fin_seq mismatch");
        }
        break;

    default:
        break;

    }
    unlock_sock(sk, &flags);
    xbuf_free(xb);
}


LOCAL
void
_stm_state_fin_wait1(
    struct lpx_sock *sk,
    struct xbuf *xb,
    struct _stm_packet_info *pktinfo,
    unsigned long flags
)
{
    debug_lpx(4, "SMP_FIN_WAIT1 lsctl=0x%x", pktinfo->lsctl);
    switch(pktinfo->lsctl) {

    case LSCTL_DATA:
    case LSCTL_DATA | LSCTL_ACK:

        LPX_STREAM_OPT(sk)->rmt_seq ++;

        if(!(pktinfo->lsctl & LSCTL_ACK)) {
            unlock_sock(sk, &flags);
            break;
        }

    case LSCTL_ACK: // Fall through

        LPX_STREAM_OPT(sk)->rmt_ack = pktinfo->ackseq;
        stm_retransmit_chk(sk, LPX_STREAM_OPT(sk)->rmt_ack, ACK);

        if(LPX_STREAM_OPT(sk)->rmt_ack == LPX_STREAM_OPT(sk)->fin_seq) {
            sk->state = SMP_FIN_WAIT2;
        } else {
            debug_lpx(1, "fin_seq mismatch: rmt_ack=%d, fin_seq=%d", LPX_STREAM_OPT(sk)->rmt_ack, LPX_STREAM_OPT(sk)->fin_seq);
        }
        LPX_STREAM_OPT(sk)->time_wait_timeout = sal_tick_add(sal_get_tick() ,TIME_WAIT_INTERVAL); //sjcho: temp fix for disconnect seq number mismatch
        unlock_sock(sk, &flags);
        break;

    case LSCTL_DISCONNREQ:
    case LSCTL_DISCONNREQ | LSCTL_ACK:
            debug_lpx(1, "DISCONN AT SMP_FIN_WAIT1");
        LPX_STREAM_OPT(sk)->rmt_seq ++;

        unlock_sock(sk, &flags);

        stm_transmit(sk, NULL, ACK, 0);

        lock_sock(sk, &flags);
        sk->state = SMP_CLOSING;
        LPX_STREAM_OPT(sk)->time_wait_timeout = sal_tick_add(sal_get_tick() ,TIME_WAIT_INTERVAL); //sjcho: temp fix for disconnect seq number mismatch

        if(pktinfo->lsctl & LSCTL_ACK) {
            LPX_STREAM_OPT(sk)->rmt_ack = pktinfo->ackseq;
            stm_retransmit_chk(sk, LPX_STREAM_OPT(sk)->rmt_ack, ACK);

            if(LPX_STREAM_OPT(sk)->rmt_ack == LPX_STREAM_OPT(sk)->fin_seq) {
                sk->state = SMP_TIME_WAIT;
            } else {
                debug_lpx(1, "fin_seq mismatch");
            }
        }
        unlock_sock(sk, &flags);
        break;
    default:
        unlock_sock(sk, &flags);
    }
    xbuf_free(xb);
}

LOCAL
void
_stm_state_fin_wait2(
    struct lpx_sock *sk,
    struct xbuf *xb,
    struct _stm_packet_info *pktinfo,
    unsigned long flags
)
{
    debug_lpx(4, "SMP_FIN_WAIT2 lsctl=0x%x", pktinfo->lsctl);
    switch(pktinfo->lsctl) {

    case LSCTL_DATA:
    case LSCTL_DATA | LSCTL_ACK:

        LPX_STREAM_OPT(sk)->rmt_seq ++;

        break;

    case LSCTL_DISCONNREQ:
    case LSCTL_DISCONNREQ | LSCTL_ACK:
        debug_lpx(4, "DISCONN AT SMP_FIN_WAIT2");
        LPX_STREAM_OPT(sk)->rmt_seq ++;

        if(LPX_STREAM_OPT(sk)->rmt_ack != LPX_STREAM_OPT(sk)->fin_seq) {// impossible
            debug_lpx(1, "fin_seq mismatch");
            break;
        }

        debug_lpx(4, "SMP_FIN_WAIT2");
        sk->state = SMP_TIME_WAIT;
        LPX_STREAM_OPT(sk)->time_wait_timeout = sal_tick_add(sal_get_tick() ,TIME_WAIT_INTERVAL);

        unlock_sock(sk, &flags);

        stm_transmit(sk, NULL, ACK, 0);

        debug_lpx(4, "SMP_FIN_WAIT2 end");
        break;

    default:
        unlock_sock(sk, &flags);
        break;
    }

    xbuf_free(xb);
}

LOCAL
void
_stm_state_fin_closing(
    struct lpx_sock *sk,
    struct xbuf *xb,
    struct _stm_packet_info *pktinfo,
    unsigned long flags
)
{
    debug_lpx(1, "SMP_CLOSING lsctl=0x%x", pktinfo->lsctl);
    switch(pktinfo->lsctl) {

    case LSCTL_ACK:
    
        LPX_STREAM_OPT(sk)->rmt_ack = pktinfo->ackseq;
        stm_retransmit_chk(sk, LPX_STREAM_OPT(sk)->rmt_ack, ACK);

        if(LPX_STREAM_OPT(sk)->rmt_ack != LPX_STREAM_OPT(sk)->fin_seq) {
            debug_lpx(1, "fin_seq mismatch");
            break;
        }
        sk->state = SMP_TIME_WAIT;
        LPX_STREAM_OPT(sk)->time_wait_timeout = sal_tick_add(sal_get_tick(), TIME_WAIT_INTERVAL);

        break;

    default:

        break;
    }

    unlock_sock(sk, &flags);
    xbuf_free(xb);
}

LOCAL
LPX_INLINE
ndas_error_t
_stm_do_vaild_rcv(
    struct lpx_sock* sk,
    struct xbuf *xb,
    struct _stm_packet_info *pktinfo,
    unsigned long flags)
{
    //
    // Valid packet received. Reset alive timeout.
    //
    LPX_STREAM_OPT(sk)->alive_retries = 0;
    LPX_STREAM_OPT(sk)->alive_timeout = sal_tick_add(sal_get_tick() , ALIVE_INTERVAL);

    switch(sk->state) {
    case SMP_ESTABLISHED: // Fall through
        _stm_state_established(sk, xb, pktinfo, flags);
        break;

    case SMP_SYN_RECV:
        if(_stm_state_syn_recv(sk, pktinfo) < 0) {
            unlock_sock(sk, &flags);
            xbuf_free(xb);
            break;
        }

    case SMP_LISTEN:
        _stm_state_listen(sk, xb, pktinfo, flags);
        break;

    case SMP_SYN_SENT:
        _stm_state_sync_sent(sk, xb, pktinfo, flags);
        break;

    case SMP_LAST_ACK:
        _stm_state_last_ack(sk, xb, pktinfo, flags);
        break;

    case SMP_FIN_WAIT1:
        _stm_state_fin_wait1(sk, xb, pktinfo, flags);
        break;

    case SMP_FIN_WAIT2:
        _stm_state_fin_wait2(sk, xb, pktinfo, flags);
        break;

    case SMP_CLOSING:
        _stm_state_fin_closing(sk, xb, pktinfo, flags);
        break;

    case SMP_CLOSE:
        unlock_sock(sk, &flags);
        xbuf_free(xb);

         if (pktinfo->lsctl & LSCTL_CONNREQ) {
                debug_lpx(1, "DISCONN AT SMP_CLOSE");
         }
        debug_lpx(2, "SMP_CLOSE");
        break;

    default:
        unlock_sock(sk, &flags);
        xbuf_free(xb);
        break;
    }

    debug_lpx(4, "ed");
    return NDAS_OK;
}


LOCAL
LPX_INLINE
int
stm_vaildate_packet(struct lpx_sock *sk, struct xbuf *xb, struct _stm_packet_info *pktinfo)
{
    struct lpxhdr *lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);

    pktinfo->packetsize = g_ntohs(lpxhdr->pu.pktsize & ~LPX_TYPE_MASK);
    pktinfo->lsctl = g_ntohs(lpxhdr->u.s.lsctl);
    pktinfo->seq = g_ntohs(lpxhdr->u.s.sequence);
    pktinfo->ackseq = g_ntohs(lpxhdr->u.s.ackseq);

#if 0 //#ifdef DEBUG // check packet loss at lower driver level
    {
    
//        sal_debug_print("<- %d %d: %ums(data:%d)", g_ntohs(lpxhdr->u.s.sequence), g_ntohs(lpxhdr->u.s.ackseq), sal_time_msec()%1000000, xb->len);
        if (LPX_STREAM_OPT(sk)->prevseq+1 < g_ntohs(lpxhdr->u.s.sequence)) {
            sal_debug_print("%d packet missing\n", g_ntohs(lpxhdr->u.s.sequence) - LPX_STREAM_OPT(sk)->prevseq-1);
            printk("Missed %d packet. jif=%ld\n", g_ntohs(lpxhdr->u.s.sequence) - LPX_STREAM_OPT(sk)->prevseq-1, jiffies);
        }
#if 0
        // retransmit log
        if (LPX_STREAM_OPT(sk)->prevseq> g_ntohs(lpxhdr->u.s.sequence)) {
            sal_debug_print("%d packet missing\n", g_ntohs(lpxhdr->u.s.sequence) - LPX_STREAM_OPT(sk)->prevseq-1);
        }
#endif
        LPX_STREAM_OPT(sk)->prevseq = g_ntohs(lpxhdr->u.s.sequence);
    }
#endif
    if(((xint16)(pktinfo->ackseq - LPX_STREAM_OPT(sk)->sequence)) > 0) {
        debug_lpx(1, "ed ackseq invalid: ack %d, seq %d(fd=%d)",
            pktinfo->ackseq,
            LPX_STREAM_OPT(sk)->sequence, sk->lpxfd);

        sal_atomic_inc(&sk->opt.stream.retransmit_count.rx_packet_loss);
        return 0;
    }

    if ( pktinfo->seq == LPX_STREAM_OPT(sk)->rmt_seq )
    {
        return 1;
    }
    else if ( pktinfo->lsctl == LSCTL_ACK &&
            ((xint16)(pktinfo->seq - LPX_STREAM_OPT(sk)->rmt_seq ) > 0) )
    {
        return 1;
    }
    else if ( pktinfo->lsctl & LSCTL_DATA )
    {
        return 1;
    }

    /*if(g_ntohs(lpxhdr->u.s.sequence) != LPX_STREAM_OPT(sk)->rmt_seq 
        && !(g_ntohs(lpxhdr->u.s.lsctl) ==  LSCTL_ACK 
            && ((xint16)(g_ntohs(lpxhdr->u.s.sequence) - LPX_STREAM_OPT(sk)->rmt_seq) > 0)) 
        && !(g_ntohs(lpxhdr->u.s.lsctl) & LSCTL_DATA)) 
    {
        debug_lpx(2, "seqnum invalid");
        return 0;
    }*/
    debug_lpx(4, "xb-lsctl=0x%x",pktinfo->lsctl);
    debug_lpx(2, "ed seqnum invalid");
    /* Already received packet is arrived again.
       Can be caused by retransmit and also caused by buggy NIC driver??
       So don't increase packet loss. 
    */
#if 0
    sal_atomic_inc(&sk->opt.stream.retransmit_count.rx_packet_loss);
#endif
    debug_lpx(1, "Duplicate packet detected from invalid seq: seq=%d, rmt_seq=%d",
        pktinfo->seq, LPX_STREAM_OPT(sk)->rmt_seq);
    return 0;
}

/* 
    Can be called in bottom half context. should not sleep.
    SMP lpx receive engine 
    Always return NDAS_OK.
 */
LOCAL
LPX_INLINE
ndas_error_t
stm_do_rcv(struct lpx_sock* sk, struct xbuf *xb)
{
    unsigned long flags;
    struct _stm_packet_info pktinfo;

    debug_lpx(4, "ing sk=%p xb=%p", sk, xb);
    //sal_assert( sal_semaphore_getvalue(sk->lock) == 0);

    lock_sock(sk, &flags);

    debug_lpx(5, "(%lu tick), port = %x, sk= %s", (unsigned long)sal_get_tick(),
                    g_ntohs(LPX_OPT(sk)->source_addr.port),
                    DEBUG_LPX_SOCK(sk)
                 );

    debug_lpx(8, "xb=%s",DEBUG_XLIB_XBUF(xb));
    if(stm_vaildate_packet(sk, xb, &pktinfo)) {
#ifdef STAT_REXMIT
        rexmit_good_count++;
#endif
        return _stm_do_vaild_rcv(sk, xb, &pktinfo, flags);
    }
    unlock_sock(sk, &flags);

    xbuf_free(xb);
#ifdef STAT_REXMIT
    rexmit_bad_count++;
#endif
    return NDAS_OK;
}


/* Check lpx for retransmission, ConReqAck aware */
LOCAL ndas_error_t 
stm_retransmit_chk(struct lpx_sock *sk, unsigned short ackseq, int type)
{
    struct xbuf      *xb;
    unsigned long flags;
//    unsigned short        success_packets;
    debug_lpx(4, "ing ackseq=%d type=%d",ackseq,type);
    sal_assert(sk);
    //sal_assert( sal_semaphore_getvalue(sk->lock) == 0);

    xbuf_qlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
    xb = xbuf_peek(&LPX_STREAM_OPT(sk)->rexmit_queue);
    if(!xb) {
        xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
        return NDAS_OK;
    }

    if(((xint16)(ackseq - xb->prot_seqnum)) <= 0) {
#ifdef DEBUG
        if ( ackseq < 100 && xb->prot_seqnum > (0xffff - 100) )
            debug_lpx(1, "rounded sk-ackseq=%d xb-sequence=%d", ackseq, (int)xb->prot_seqnum);
#endif
        xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
        return NDAS_OK;
    }
//    success_packets = ackseq - g_ntohs(lpxhdr->u.s.sequence);
//    flight_adaption(sk, success_packets);

    while(xb != NULL) {
        /*
         * If a packet is acked, free it.
         */
        if(((xint16)(ackseq - xb->prot_seqnum)) > 0) {
            xb = _xbuf_dequeue(&LPX_STREAM_OPT(sk)->rexmit_queue);
            xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
            xbuf_free(xb);
            xbuf_qlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
        } else {
            break;
        }

        xb = xbuf_peek(&LPX_STREAM_OPT(sk)->rexmit_queue);
    }

#if 0    /* no effect */
    if(!xbuf_queue_empty(&LPX_STREAM_OPT(sk)->rexmit_queue)) {
//        LPX_STREAM_OPT(sk)->retransmit_timeout = sal_tick_add(sal_get_tick() , stm_calc_retransmission_time(sk));
    } else {
        LPX_STREAM_OPT(sk)->interval_time = (LPX_STREAM_OPT(sk)->interval_time*99) + sal_tick_sub(sal_get_tick(), LPX_STREAM_OPT(sk)->latest_sendtime);
        if(LPX_STREAM_OPT(sk)->interval_time > 100)
            LPX_STREAM_OPT(sk)->interval_time /= 100;
        else
            LPX_STREAM_OPT(sk)->interval_time = 1;
        LPX_STREAM_OPT(sk)->latest_sendtime = 0;
    }                    
#endif

    if(LPX_STREAM_OPT(sk)->rexmits) {
        debug_rexmit(3, "ing ackseq=%d xb-sequence=%d type=%d",
            ackseq, xb->prot_seqnum, type);
        LPX_STREAM_OPT(sk)->rexmits = 0;
        LPX_STREAM_OPT(sk)->timer_reason |= SMP_RETRANSMIT_ACKED;
    } else if (!(LPX_STREAM_OPT(sk)->timer_reason & SMP_RETRANSMIT_ACKED) 
        && !(LPX_STREAM_OPT(sk)->timer_reason & SMP_SENDIBLE) 
        && ((xb = xbuf_peek(&sk->write_queue)) != NULL) 
        && lpx_stream_snd_test(sk, xb))
    {
        LPX_STREAM_OPT(sk)->timer_reason |= SMP_SENDIBLE;
    }    
    xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
    return NDAS_OK;
}

LOCAL ndas_error_t stm_rexmit(struct lpx_sock *sk) 
{
    struct xbuf_queue xq;
    struct xbuf *xb, *xb2;
    unsigned long flags;
    int ret;

    xbuf_qlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);    
    if (xbuf_queue_empty(&LPX_STREAM_OPT(sk)->rexmit_queue)) {
        xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
        return NDAS_OK;
    } else {
        xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
    }
    
    xbuf_queue_init(&xq);

    xbuf_qlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
    xb = xbuf_peek(&LPX_STREAM_OPT(sk)->rexmit_queue);
    if ( xb ) {
        // Increase retransmit count by one if retransmit queue is not empty
        sal_atomic_inc(&sk->opt.stream.retransmit_count.tx_retransmit);

        while(xb != (struct xbuf*)&(LPX_STREAM_OPT(sk)->rexmit_queue)) 
        {
            sal_assert(xb);

            debug_rexmit(1, "rexmit 1. xb=%p", xb);

            // If the xbuf is shared, clone, or rebuild-able,
            // Copy the xbuf.
            if(xbuf_is_shared(xb) || xbuf_is_clone(xb) || xbuf_is_sys_netbuf_rebuild(xb)) {
                xb2 = xbuf_copy(xb);
                debug_rexmit(1, "Copied xb=%p to xb2=%p", xb, xb2);
            } else {
                xb2 = xbuf_clone(xb);
            }
            if ( xb2 == NULL ) {
                xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
                goto out;
            }
            xbuf_queue_tail(&xq, xb2);
            xb = xb->next;
        }
    }
    debug_rexmit(1, "rexmit 2");
    xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
    while( (xb = xbuf_dequeue(&xq)) != NULL ) 
    {
        LPX_STREAM_OPT(sk)->rexmit_timeout = sal_tick_add(sal_get_tick() ,stm_calc_retransmission_time(sk));

        sal_assert(xb->destruct_handler == NULL);
        if(xbuf_is_sys_netbuf_rebuild(xb)) {
            debug_rexmit(1, "rexmit_queue xb=%p xb->next=%p xb->clonee=%p",
                xb, xb->next, xb->clonee);

            // Allocate new system network buffer to the retransmission xbuffer.
            ret = lpx_sock_rebuild_xb_netbuf(sk, xb, 0);
            if(ret < 0) {
                debug_rexmit(1, "lpx_sock_rebuild_xb_netbuf() failed.");
                break;
            }
            // Prevent this retransmission xbuffer to be rebuilt.
            xbuf_reset_sys_netbuf_rebuild(xb);
            // Fill the LPX header and send.
            stm_retransmit_rebuilt_xb(sk, xb);
        } else {
            struct lpxhdr * lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);

            if(lpxhdr) {
                debug_rexmit(1, "rexmit_queue xb=%p xb->next=%p xb->clonee=%p"
                    "lpxhdr->u.s.ackseq=%d rmt_seq=%d ",
                    xb, xb->next, xb->clonee,
                    lpxhdr->u.s.ackseq, g_htons(LPX_STREAM_OPT(sk)->rmt_seq));
                lpxhdr->u.s.ackseq = g_htons(LPX_STREAM_OPT(sk)->rmt_seq);
            }

            stm_route_xb(sk, xb, RETRAN);
        }
    }
    LPX_STREAM_OPT(sk)->timer_reason &= ~SMP_RETRANSMIT_ACKED;
out:
    xbuf_queue_destroy(&xq);

    debug_rexmit(1, "rexmit ed");
    return NDAS_OK;
}

static
inline
void
stm_timer_dpc_timeout(
    int *cur_dpc_timeout,
    int request_timeout
)
{
#if 0
#if 0
    if(request_timeout>*cur_dpc_timeout)
        *cur_dpc_timeout = request_timeout;
#else
    *cur_dpc_timeout = SMP_TIMEOUT;
#endif
#endif
}
LOCAL int stm_timer_prepare_to_destroy(struct lpx_sock *sk)
{
    int ret;

    debug_lpx(4,"before destroy");
    // Schedule the destroy job when the queues are empty or 
    // retransmission reaches max alive count.
    if(( xbuf_queue_empty(&sk->write_queue) &&
        xbuf_queue_empty(&LPX_STREAM_OPT(sk)->rexmit_queue)) ||
        LPX_STREAM_OPT(sk)->alive_retries > MAX_ALIVE_COUNT
    ) {
        sk->destroy = 1;
#ifdef XPLAT_OPT_THROTTLE_SOCK_CREATE
        v_destroy_pending_count++;
#endif
        // Keep current status for SMP_DESTROY_TIMEOUT
//        sk->state = SMP_CLOSE;

        // Increase the reference count for the DPC thread that will run stm_timer()
        stm_hold(sk);
        ret = dpc_queue(LPX_STREAM_OPT(sk)->lpx_stream_timer, SMP_DESTROY_TIMEOUT);
        if ((ret == NDAS_OK +1) || ret < 0) {
            stm_release(sk, 0);
            debug_lpx(1, "stm_timer is not queued or re-queued");
        }
        // Decrease the reference count for this DPC thread.
        stm_release(sk, 0);
        return 0;
    } else {
        debug_lpx(1,"Packet queue is not empty. Try to destroy later.");
    }
    return 1;
}

LOCAL
void
stm_timer_destroy(struct lpx_sock *sk)
{
    unsigned long skflags;
    int ret;

    lock_sock(sk, &skflags);
    sk->state = SMP_CLOSE;
    unlock_sock(sk, &skflags);

    if(LPX_STREAM_OPT(sk)->child_sklist) {
        /* Don't destroy socket struct until all childrens are destroyed */
        debug_lpx(1, "Child exits. Delaying destroying, sk=%p, sal_tick_get() = %ld", sk, (unsigned long)sal_get_tick());

        stm_hold(sk); // Increase the reference count for the DPC thread that will run the stm_timer().
        ret = dpc_queue(LPX_STREAM_OPT(sk)->lpx_stream_timer, SMP_DESTROY_TIMEOUT);
        if ((ret == NDAS_OK + 1) || ret < 0) {
            stm_release(sk, 0);
            debug_lpx(1, "stm_timer is not queued or re-queued.");
        }

        // Decrease the reference count for this DPC thread.
        stm_release(sk, 0);

    } else {
        struct lpx_sock *parent_sk = LPX_STREAM_OPT(sk)->parent_sk;
        unsigned long sockflags;
        debug_lpx(2, "destroying");

        /* Unbind the socket from the socket's lpx address,
            and get out from interfaces' socket list.
            No more packet will be received to the socket afterward. */

        lpxitf_remove_sock(sk);

        /* Get out from the parent's socket list. */
        if(parent_sk != NULL) 
        {
            unsigned long parent_flags;

            lock_sock(parent_sk, &parent_flags);

            lock_sock(sk, &sockflags);
            LPX_STREAM_OPT(sk)->parent_sk = NULL;
            unlock_sock(sk, &sockflags);

            stm_remove_from_parent_sk(parent_sk, sk);
            unlock_sock(parent_sk, &parent_flags);
            debug_lpx(1, "Removed child sk %d from the parent sk %d", sk->lpxfd, parent_sk->lpxfd);
        }

        // Decrease the reference count for this DPC thread.
        // This release should make reference count zero so that
        // the socket can be destroy.
        stm_release(sk, 0);

        debug_lpx(2, "ed destroyed");
    }
}

//
// Require the socket lock held.
//
LOCAL void stm_timer_retransmit(struct lpx_sock *sk, int *dpc_expire_time)
{
    struct lpxhdr *lpxhdr;
    unsigned long flags, skflags;
    int ret;
	struct xbuf *xb;
	struct xbuf* xb2;
            
    xbuf_qlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
    xb = xbuf_peek(&LPX_STREAM_OPT(sk)->rexmit_queue);
    if(xb != NULL) {
        debug_rexmit(1, "rexmiting from timer: sk=%p state=%d rmt_seq=%d",
            sk, sk->state, LPX_STREAM_OPT(sk)->rmt_seq);

        // Need to leave xb on the queue, aye the fear

        // Retransmission packet must not have following packet flag.
        xbuf_reset_following_packet_exist(xb);

        xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);

        // If the xbuf is shared, clone, or rebuild-able,
        // Copy the xbuf.
        if(xbuf_is_shared(xb) || xbuf_is_clone(xb) || xbuf_is_sys_netbuf_rebuild(xb)) {
            xb2 = xbuf_copy(xb);
            debug_rexmit(1, "Copied xb=%p to xb2=%p", xb, xb2);
        } else {
            xb2 = xbuf_clone(xb);
        }

        if ( xb2 ) {
            LPX_STREAM_OPT(sk)->rexmits++;
            LPX_STREAM_OPT(sk)->rexmit_timeout = sal_tick_add(sal_get_tick() , stm_calc_retransmission_time(sk));
            LPX_STREAM_OPT(sk)->last_rexmit_seq = xb->prot_seqnum;

            debug_rexmit(5, "current tick=%ld", (unsigned long)sal_get_tick());
            debug_rexmit(5, "Rexmit, remote ackseq=%d", LPX_STREAM_OPT(sk)->rmt_ack);
            debug_rexmit(1, "LPX_STREAM_OPT(sk)->last_rexmit_seq=%d, LPX_STREAM_OPT(sk)->rexmits=%d", 
                LPX_STREAM_OPT(sk)->last_rexmit_seq, LPX_STREAM_OPT(sk)->rexmits);

            unlock_sock(sk, &skflags);

            if(xbuf_is_sys_netbuf_rebuild(xb2)) {
                // Allocate new system network buffer to the retransmission xbuffer.
                ret = lpx_sock_rebuild_xb_netbuf(sk, xb2, 0);
                if(ret >= 0) {
                    // Prevent this retransmission xbuffer to be rebuilt.
                    xbuf_reset_sys_netbuf_rebuild(xb2);
                    // Fill the LPX header and send.
                    stm_retransmit_rebuilt_xb(sk, xb2);
                } else {
                    debug_rexmit(1, "lpx_sock_rebuild_xb_netbuf() failed.");
                }
            } else {
                //
                // Update LPX header for retransmission.
                //
                lpxhdr = (struct lpxhdr *) XBUF_NETWORK_HEADER(xb);
                if(lpxhdr) {
                    lpxhdr->u.s.ackseq = g_htons(LPX_STREAM_OPT(sk)->rmt_seq); /* check when this value updated */
    
                    // Clear continuing flag in lpx header.
                    lpxhdr->u.s.option &= ~LPX_OPTION_PACKETS_CONTINUE_BIT;
    
                    // 1.0, 1.1 conreq packet loss bug fix:
                    //   Netdisk ignore multiple connection request from same host.
                    //   So if host lose first connection ack, host cannot connect the disk.
                    //   Work around: connect with different source port number.
                    if (lpxhdr->u.s.lsctl & g_htons(LSCTL_CONNREQ)) {
                        //
                        // If the user specify the fixed port number, do not change it.
                        //
                        if(!sk->fixedport) {
                            unsigned short port;
                            // find another port number
                            port = lpx_get_free_port_num();
                            // Change port
                            lpx_unreg_bind(sk);
                            LPX_OPT(sk)->source_addr.port = g_htons(port);
                            lpx_reg_bind(sk);
                            lpxhdr->source_port = LPX_OPT(sk)->source_addr.port;
                            debug_lpx(1, "Conreq fix: changing port number to %d", port);
                        }
                    }
                }
                //
                // Send!
                //
                stm_route_xb(sk, xb2, RETRAN);
            }
            lock_sock(sk, &skflags);

            debug_rexmit(5, "rexmit-ing done");
        }
        // Queue another DPC to check retransmission timeout again.
        stm_timer_dpc_timeout(dpc_expire_time, SMP_TIMEOUT);
    } else {
        xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
    }
}


//
// Require the socket lock held.
//
LOCAL
void
stm_timer_send_writeq_packets(struct lpx_sock *sk, int *dpc_expire_time)
{
    struct lpxhdr *lpxhdr;
    struct xbuf *xb;
    unsigned long skflags;

    debug_rexmit(2, "fd=%d retransmit send packet len=%d", sk->lpxfd, sk->write_queue.qlen);
    while((xb = xbuf_peek(&sk->write_queue)) != NULL) {
        if(lpx_stream_snd_test(sk, xb) ) {
            xb = xbuf_dequeue(&sk->write_queue);
            if (xb==NULL)
                break;
            //
            // We can modify the lpx header without xbuf sharing check
            // because the packets in the write queue has not been sent to the lower layers.
            //
            lpxhdr = (struct lpxhdr *)XBUF_NETWORK_HEADER(xb);
            lpxhdr->u.s.ackseq = g_htons(LPX_STREAM_OPT(sk)->rmt_seq);
            debug_rexmit(1, "retransmit: rmt_seq=%d", LPX_STREAM_OPT(sk)->rmt_seq);
            
            unlock_sock(sk, &skflags);
            stm_route_xb((struct lpx_sock*)xb->owner, xb, DATA);
            lock_sock(sk, &skflags);
        } else
            break;
    }
    // Clear sendible timer reason.
    LPX_STREAM_OPT(sk)->timer_reason &= ~SMP_SENDIBLE;
    debug_rexmit(2, "transmit send packet done");

    // Queue another DPC to check retransmission timeout again.
    stm_timer_dpc_timeout(dpc_expire_time, SMP_TIMEOUT * 2);
}

//
// Require the socket lock held.
//
LOCAL void
stm_timer_retransmit_acked(struct lpx_sock *sk, int *dpc_expire_time)
{
    struct xbuf *xb;
    unsigned long flags, skflags;

    xbuf_qlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
    xb = xbuf_peek(&LPX_STREAM_OPT(sk)->rexmit_queue);
#ifdef DEBUG
    if(xb) {
        struct lpxhdr *lpxhdr;
        lpxhdr = (struct lpxhdr *) XBUF_NETWORK_HEADER(xb);
        if(lpxhdr) {
            sal_assert(lpxhdr != NULL);
            debug_rexmit(2, "rexmit acked seq=%d  ackseq=%d ", 
                lpxhdr->u.s.sequence, lpxhdr->u.s.ackseq);
        } else {
            debug_rexmit(2, "rexmit acked seq=%d", 
                xb->prot_seqnum);
        }
    }
#endif
    xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
    /*
        * if retransmission is finished, send packet in the write queue.
        */
    if(xb == NULL) {
        LPX_STREAM_OPT(sk)->timer_reason &= ~SMP_RETRANSMIT_ACKED;
        stm_timer_send_writeq_packets(sk, dpc_expire_time);
        return;
    }
    unlock_sock(sk, &skflags);

    stm_rexmit(sk);

    lock_sock(sk, &skflags);

    // Queue another DPC to check retransmission timeout again.
    stm_timer_dpc_timeout(dpc_expire_time, SMP_TIMEOUT * 2);
}


//
// Require the socket lock held.
//
LOCAL int
stm_timer_alive_check(struct lpx_sock *sk, int *dpc_expire_time) {
    struct xbuf *xb;
    int ret = 0;
    unsigned long flags;

    debug_lpx(4, "sk->dead, current=%ld, alive_retries = %d", 
        (unsigned long)sal_get_tick(), LPX_STREAM_OPT(sk)->alive_retries);
    if(LPX_STREAM_OPT(sk)->alive_retries > MAX_ALIVE_COUNT) {
        debug_lpx(1, "Exceeded max alive count error");
        sk->err = NDAS_ERROR_TIME_OUT;
        if (sk->sleep)
            sal_event_set(sk->sleep);

        sk->shutdown = 1;
        sk->state = SMP_CLOSE;

        if(!sk->dead)
            lpx_sock_state_change(sk);
        // Queue another DPC to proceed to destroy routine.
        stm_timer_dpc_timeout(dpc_expire_time, SMP_TIMEOUT);
        return 1;
    }
    LPX_STREAM_OPT(sk)->alive_timeout = sal_tick_add(sal_get_tick() , ALIVE_INTERVAL);
    if (sk->sock_state == LSS_CONNECTED ) { // send only if connected state
        xbuf_qlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
        xb = xbuf_peek(&LPX_STREAM_OPT(sk)->rexmit_queue);
        xbuf_qunlock(&LPX_STREAM_OPT(sk)->rexmit_queue, &flags);
        if(!xb) {
            ret = 2;
        } else {
            debug_lpx(1, "In retransmit, skip ACKREQ");
            // Don't send ackreq if there exists packet to retransmit
        }
    }
    LPX_STREAM_OPT(sk)->alive_retries ++;
    // Queue another DPC to check alive timeout again.
    stm_timer_dpc_timeout(dpc_expire_time, SMP_TIMEOUT * 4);

    return ret;
}


/* 
 * called every 50 ms for stream socket.
 * 
 */
LOCAL int stm_timer(void* p1, void* p2)
{
    struct lpx_sock *sk = p1;
    unsigned long skflags;
    int lockheld = 0;
    int ret;
    int dpc_expire_time = SMP_TIMEOUT;


#ifdef DEBUG
    debug_lpx(4, "sk = %p", sk);
    debug_lpx(8, "sk = %s", DEBUG_LPX_SOCK(sk)); 
    sal_assert(sk);
    if ( LPX_STREAM_OPT(sk)->rexmit_queue.qlen > 0 ) {
        debug_rexmit(4,"time=%ld, timeout=%d ", 
            (unsigned long)sal_get_tick(), LPX_STREAM_OPT(sk)->rexmit_timeout);
    }
#endif

    //
    // Destroy a socket
    //
    if(sk->destroy) {
        stm_timer_destroy(sk);
        return 0;
    }

    if((sk->state == SMP_CLOSE && sk->dead) ||
           ((sk->state == SMP_TIME_WAIT ||
             sk->state == SMP_LAST_ACK ||
             sk->state == SMP_FIN_WAIT1 || /* Work-around for no passive discon from Windows LPX */
             sk->state == SMP_FIN_WAIT2 || /* Work-around for no passive discon from Windows LPX */
             sk->state == SMP_CLOSING) &&  /* Work-around for no passive discon from Windows LPX */
            sal_tick_sub(LPX_STREAM_OPT(sk)->time_wait_timeout , sal_get_tick())<=0)
    )
    {
        if(stm_timer_prepare_to_destroy(sk) == 0) {
            return 0;
        }
    }


    if(sk->state == SMP_CLOSE || sk->state == SMP_LISTEN)
        goto out_return;

    lock_sock(sk, &skflags);
    lockheld = 1;

    /*-------------------------------------------------------
        Retransmission
      -------------------------------------------------------*/

    /* check if timed out to receive the acks of the packets we sent
       Retransmit a packet if retransmission time-outed.
       closed the door we do not accept ack any more from now on. */
    if(sal_tick_sub(LPX_STREAM_OPT(sk)->rexmit_timeout, sal_get_tick())  <= 0) 
    {
        stm_timer_retransmit(sk, &dpc_expire_time);

    /* First retransmitted packet is acked. Send the rest of the packets */
    } else if (LPX_STREAM_OPT(sk)->timer_reason & SMP_RETRANSMIT_ACKED)  {
        stm_timer_retransmit_acked(sk, &dpc_expire_time);
    }

    /*-------------------------------------------------------
        Delayed sending packets and alive packets
      -------------------------------------------------------*/

    /*
        Send packets in the write queue.
     */
    if(LPX_STREAM_OPT(sk)->timer_reason & SMP_SENDIBLE) 
    {
        stm_timer_send_writeq_packets(sk, &dpc_expire_time);
    /*
      LPX alive check
     */
    } else if (sal_tick_sub(LPX_STREAM_OPT(sk)->alive_timeout,sal_get_tick()) <= 0) // alive message
    {
        ret = stm_timer_alive_check(sk, &dpc_expire_time);
        if(ret == 1) {
            goto out_return;
        } else if(ret == 2) {
            unlock_sock(sk, &skflags);
            lockheld = 0;
            stm_transmit(sk, NULL, ACKREQ, 0);
        }
    }
#ifdef USE_DELAYED_ACK
    if(lockheld) {
        unlock_sock(sk, &skflags);
        lockheld = 0;
    }

    /*-------------------------------------------------------
        Delayed ack
      -------------------------------------------------------*/
    /*
      Delayed ack
    */
    sal_spinlock_take_softirq(sk->delayed_ack_lock);
    if (sk->delayed_ack_xb && sal_tick_sub(sal_get_tick(), sk->last_txed_ack) > MAX_ACK_DELAY ) {
        struct xbuf* xb2;
        xb2 = sk->delayed_ack_xb;
        sk->delayed_ack_xb = NULL;
        sal_spinlock_give_softirq(sk->delayed_ack_lock);
        stm_route_xb(sk, xb2, ACK);
    } else {
        sal_spinlock_give_softirq(sk->delayed_ack_lock);
    }
#endif
out_return:
    if(lockheld)
        unlock_sock(sk, &skflags);

    /*-------------------------------------------------------
        Queue another DPC if needed.
      -------------------------------------------------------*/
    // Increase the reference count for the DPC thread that will run stm_timer().
    stm_hold(sk);
    ret = dpc_queue(LPX_STREAM_OPT(sk)->lpx_stream_timer, dpc_expire_time);
    // LPX_STREAM_OPT(sk)->lpx_stream_timer may be initialized to DPC_INVALID_ID if DPC is stopped.
    if ((ret == NDAS_OK +1) || ret < 0) {
        stm_release(sk, 0);
        debug_lpx(1, "stm_timer is not queued or re-queued.");
    }

    // Decrease the reference count for this DPC thread.
    stm_release(sk, 0);
    return 0;
}

ndas_error_t lpx_get_retransmit_count(int lpxfd, struct lpx_retransmits_count* cnt)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (sk == NULL) {
    debug_lpx(1, "fd=%d does not exist", lpxfd);
        return NDAS_ERROR_INVALID_HANDLE;
    }
    if ( sk->type != LPX_TYPE_STREAM ) 
        return NDAS_ERROR_INVALID_OPERATION;
    cnt->rx_packet_loss = sal_atomic_read(&sk->opt.stream.retransmit_count.rx_packet_loss);
    cnt->tx_retransmit = sal_atomic_read(&sk->opt.stream.retransmit_count.tx_retransmit);
#ifdef DEBUG
    if (cnt->rx_packet_loss !=0  || cnt->tx_retransmit !=0) {
        debug_lpx(5, "Packet loss=%d, Retransmits=%d",
            cnt->rx_packet_loss, cnt->tx_retransmit);
    }
#endif

    return NDAS_OK;
}

ndas_error_t lpx_reset_retransmit_count(int lpxfd)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (sk == NULL) {
        return NDAS_ERROR_INVALID_OPERATION;
    }
    if ( sk->type != LPX_TYPE_STREAM )  {
        return NDAS_ERROR_INVALID_OPERATION;
    }
    sal_atomic_set(&sk->opt.stream.retransmit_count.rx_packet_loss, 0);
    sal_atomic_set(&sk->opt.stream.retransmit_count.tx_retransmit, 0);
    return NDAS_OK;
}

ndas_error_t lpx_set_packet_loss_detection_time(int lpxfd, sal_msec msec)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if ( sk->type != LPX_TYPE_STREAM ) 
        return NDAS_ERROR_INVALID_OPERATION;
    sk->opt.stream.loss_detection_time = SAL_TICKS_PER_SEC * msec / 1000;
    return NDAS_OK;
}

/********************************
    LPX socket-like interface
********************************/

/* 
    type: LPX_TYPE_RAW, LPX_TYPE_DATAGRAM, LPX_TYPE_STREAM 
    protocol: Ignored 
    returns lpxfd or 
            -1 for error
*/

ndas_error_t lpx_socket( int type, int protocol)
{
    int ret;
    struct lpx_sock *sk;
    ret = lpx_create(&sk, type, protocol);
    if (!NDAS_SUCCESS(ret)) {
        debug_lpx(2, "type=%d, err=%d", type, ret);
        return ret;
    } else {
        debug_lpx(2, "type %d, fd=%d", type, sk->lpxfd);
        return sk->lpxfd;
    }
}
ndas_error_t lpx_is_connected(int fd) {
    struct lpx_sock *sk = lpx_get_sock_from_fd(fd);
    return ( !sk ) ? NDAS_ERROR_INVALID_HANDLE
        : ( sk->type != LPX_TYPE_STREAM ) ? NDAS_ERROR_INVALID_OPERATION
        : ( sk->state != SMP_ESTABLISHED ) ? NDAS_ERROR_NOT_CONNECTED
        : NDAS_OK;
}

ndas_error_t lpx_is_recv_data(int fd) {
    struct lpx_sock *sk = lpx_get_sock_from_fd(fd);
    if(!sk)
        return NDAS_ERROR_INVALID_HANDLE;
    return !xbuf_queue_empty(&sk->receive_queue);
}

ndas_error_t lpx_accept(int parent_fd, struct sockaddr_lpx* addr, unsigned int *addrlen)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(parent_fd);
    ndas_error_t ret;
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;
    
    if (sk->type == LPX_TYPE_STREAM) {

        ret = stm_accept(sk, 0);
        if (ret>=0) {
            struct lpx_sock* new_sock = lpx_get_sock_from_fd(ret);
            sal_assert(new_sock);

            if ((*addrlen) >= sizeof(struct sockaddr_lpx)) {
                addr->slpx_family = AF_LPX;
                addr->slpx_port = new_sock->lpx_opt.source_addr.port;
                sal_memcpy(addr->slpx_node, new_sock->lpx_opt.dest_addr.node, LPX_NODE_LEN);
            }
            *addrlen = sizeof(struct sockaddr_lpx);
        }
        return ret;
    }
    return NDAS_ERROR_INVALID_OPERATION;
}


ndas_error_t lpx_close(int lpxfd)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    int ret = NDAS_ERROR_INVALID_OPERATION;
    unsigned long sockflags;

    debug_lpx(2, "ing lpxfd=%d sk=%p", lpxfd, sk);
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;

    if (sk->type == LPX_TYPE_DATAGRAM) {
        /* Get out from the interface and port binding list */
        lpxitf_remove_sock(sk);
        dgm_release(sk, 1);
        return NDAS_OK;
    } else if (sk->type == LPX_TYPE_STREAM) {
        sal_event destruct_event;

        /* Workaround for kernel crash.
           User buffer might be free before send packets are in-flight.
           So, we wait until the socket is actually destroyed by timer DPC thread */

        destruct_event = sal_event_create("sk_dstr");
        if(destruct_event != SAL_INVALID_EVENT) {
            // Hand in to the socket's timer DPC.
            lock_sock(sk, &sockflags);
            if(sk->destruct_event != SAL_INVALID_EVENT) {
                debug_lpx(1, "Duplicate close. fd=%d sk=%p", lpxfd, sk);
                unlock_sock(sk, &sockflags);

                sal_event_destroy(destruct_event);
                return NDAS_ERROR_INVALID_OPERATION;
            }
            sk->destruct_event = destruct_event;
            unlock_sock(sk, &sockflags);
        } else {
            debug_lpx(1, "sal_event_create() failed. sk=%p", sk);
        }
#if 0
        {
            struct lpx_sock *parent_sk = LPX_STREAM_OPT(sk)->parent_sk;

            /* Get out from the interface and port binding list */
            lpxitf_remove_sock(sk);
            /* Get out from the parent's socket list. */
            if(parent_sk != NULL) 
            {
                unsigned long parent_flags;

                lock_sock(parent_sk, &parent_flags);
                lock_sock(sk, &sockflags);
                LPX_STREAM_OPT(sk)->parent_sk = NULL;
                unlock_sock(sk, &sockflags);

                stm_remove_from_parent_sk(parent_sk, sk);
                unlock_sock(parent_sk, &parent_flags);
                debug_lpx(1, "Removed child sk %d from the parent sk %d", sk->lpxfd, parent_sk->lpxfd);
            }
        }
        debug_lpx(1, "sk=%p parent=%p", sk, LPX_STREAM_OPT(sk)->parent_sk);
#endif
        stm_release(sk, 1);
        (void)sal_event_wait(destruct_event, SAL_SYNC_FOREVER);
        if(destruct_event != SAL_INVALID_EVENT)
            sal_event_destroy(destruct_event);
    }
    return ret;
}

ndas_error_t lpx_bind(int lpxfd, struct sockaddr_lpx* addr, int addrlen)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    unsigned long flags;
    ndas_error_t ret;
    if (!sk) {
        return NDAS_ERROR_INVALID_HANDLE;
    }
    /* to do: check sk validity */
    lock_sock(sk, &flags);
    if (sk->type == LPX_TYPE_DATAGRAM) {
        ret = _dgm_bind(sk, addr, addrlen);
    } else if (sk->type == LPX_TYPE_STREAM) {
        ret = _stm_bind(sk, addr, addrlen);
    } else {
       ret = NDAS_ERROR_INVALID_OPERATION;
    }
    unlock_sock(sk, &flags);
    
    return ret;
}

ndas_error_t lpx_recvfrom(int lpxfd, void* buf, int len, int flags, struct sockaddr_lpx *from, int *fromlen)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;

    if (sk->type == LPX_TYPE_DATAGRAM) {
        return dgm_recv_user(sk, from, buf, len);
    } 
    return NDAS_ERROR_INVALID_OPERATION;
}

#define TURN_ON_DISCONN 0

ndas_error_t lpx_recv_aio(lpx_aio * aio)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(aio->sockfd);
#if TURN_ON_DISCONN
    static int discon=0;
    discon++;
    if (discon>=200) {
        printf("TEST: lpx closing connection while recv\n");
        discon=0;
        lpx_close(aio->sockfd);
        return NDAS_ERROR_INVALID_HANDLE;
    }
#endif
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;

    debug_lpx(4, "lpx_recv_aio: %d", aio->len);
    if (sk->type == LPX_TYPE_STREAM) {
        _lpx_set_system_sk(sk, aio);
        return stm_recv_user(sk, aio);
    }
    return NDAS_ERROR_INVALID_OPERATION;
}

ndas_error_t lpx_recv(int lpxfd, void* buf, int len, int flags)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    lpx_aio aio;

#if TURN_ON_DISCONN
    static int discon=0;
    discon++;
    if (discon>=200) {
        printf("TEST: lpx closing connection while recv\n");
        discon=0;
        lpx_close(aio->sockfd);
        return NDAS_ERROR_INVALID_HANDLE;
    }
#endif
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;

    lpx_prepare_aio(
        lpxfd,
        0,
        buf,
        0,
        len,
        flags,
        &aio);

    debug_lpx(4, "lpx_recv_aio: %d", aio.len);
    if (sk->type == LPX_TYPE_STREAM) {
        _lpx_set_system_sk(sk, &aio);
        return stm_recv_user(sk, &aio);
    }

    return NDAS_ERROR_INVALID_OPERATION;
}
#ifdef XPLAT_SIO
ndas_error_t 
lpx_sio_recvfrom(int lpxfd, void* buf, int len, int flags, 
                    lpx_sio_handler func, void* arg, sal_tick timeout)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;
    if(sk->shutdown)
        return NDAS_ERROR_INVALID_HANDLE;

    if (sk->type == LPX_TYPE_DATAGRAM) {
        return sio_dgm_recv_user(sk,func, arg, buf, len, timeout, flags);
    } 
    return NDAS_ERROR_INVALID_OPERATION;
}
#endif

/**
 * lpx_recv_xbuf - Internal lpx receive function. Caller should free xbufs from queue manually. 
 */

ndas_error_t lpx_recv_xbuf(int lpxfd, struct xbuf_queue* queue, int len)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
#if TURN_ON_DISCONN
    static int discon=0;
    discon++;
    if (discon>=200) {
        printf("TEST: lpx closing connection while recv xbuf\n");
    discon=0;
    lpx_close(lpxfd);
        return NDAS_ERROR_INVALID_HANDLE;
    }
#endif
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;
    debug_lpx(4, "lpx_recv: %d", len);
    if (sk->type == LPX_TYPE_STREAM) {

        return stm_recv_xbuf(sk, queue, len);
    } 
    return NDAS_ERROR_INVALID_OPERATION;
}

ndas_error_t lpx_recvmsg(int lpxfd, struct sal_mem_block* blocks, int nr_blocks, int total_len, int flags)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;

    debug_lpx(4, "lpx_recvmg: %d", total_len);
    if (sk->type == LPX_TYPE_STREAM) {
        lpx_aio aio;

        lpx_prepare_aio(
            lpxfd,
            nr_blocks,
            blocks,
            0,
            total_len,
            flags,
            &aio);
        _lpx_set_system_sk(sk, &aio);
        return stm_recv_user(sk, &aio);
    }
    return NDAS_ERROR_INVALID_OPERATION;
}
/*
 * return positive for the size of data sent
 * negative for the error code
 * 0 if the request block size is zero or sum of size of blocks is zero
 */
ndas_error_t lpx_send_aio(lpx_aio *aio)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(aio->sockfd);
#if TURN_ON_DISCONN
    static int discon=0;
    discon++;
    if (discon>=200) {
        printf("TEST: lpx closing connection while send\n");
        discon=0;
        lpx_close(aio->lpxfd);
        return NDAS_ERROR_INVALID_HANDLE;
    }
#endif 
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;

    debug_lpx(4, "len=%d", aio->len);
    debug_lpx(8, "data(%p,%dbytes)={%s}", aio->buf.buf, aio->len,SAL_DEBUG_HEXDUMP((unsigned char*)aio->buf.buf,aio->len));

    // Set socket to the AIO structure.
    _lpx_set_system_sk(sk, aio);

    if (sk->type == LPX_TYPE_STREAM) {
        return stm_send_user(sk, aio);
    }
    return NDAS_ERROR_INVALID_OPERATION;
}

ndas_error_t lpx_send(int lpxfd, const void* buf, int len, int flags)
{
    lpx_aio aio;

    lpx_prepare_aio(
        lpxfd,
        0,
        (void *)buf,
        0,
        len,
        flags,
        &aio);

    return lpx_send_aio(&aio);
}

ndas_error_t lpx_sendto(int lpxfd, const void* buf, int len, int flags, const struct sockaddr_lpx *to, int tolen)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;

    if (sk->type == LPX_TYPE_DATAGRAM) {
        return dgm_send_user(sk, (struct sockaddr_lpx*)to, (char*)buf, len);
    }     
    return NDAS_ERROR_INVALID_OPERATION;
}

ndas_error_t lpx_sendmsg(int lpxfd ,struct sal_mem_block* blocks, int nr_blocks, int total_length, int flags)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    lpx_aio aio;
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;

    lpx_prepare_aio(
        lpxfd,
        nr_blocks,
        blocks,
        0,
        total_length,
        flags,
        &aio);
    _lpx_set_system_sk(sk, &aio);

    return stm_send_user(sk, &aio);
}

ndas_error_t lpx_sendpages_aio(lpx_aio *aio)
{
    ndas_error_t retval;
    struct lpx_sock *sk = lpx_get_sock_from_fd(aio->sockfd);
    debug_lpx(4, "pages=%p, offset=%d, size=%d", aio->buf.pages, (int)aio->offset, (int)aio->len);

    if (!sk) {
        lpx_complete_aio_leftover(aio, NDAS_ERROR_INVALID_HANDLE);
        debug_lpx(1, "sockfd=%d. Invalid handle", aio->sockfd);
        return NDAS_ERROR_INVALID_HANDLE;
    }

    if(sk->state != SMP_ESTABLISHED) {
        lpx_complete_aio_leftover(aio, NDAS_ERROR_NOT_CONNECTED);
        retval = NDAS_ERROR_NOT_CONNECTED;
        debug_lpx(1, "sockfd=%d. not connected", aio->sockfd);
        goto out;
    } 

    if (sk->shutdown) {
        lpx_complete_aio_leftover(aio, NDAS_ERROR_NOT_CONNECTED);
        retval = NDAS_ERROR_NOT_CONNECTED;
        debug_lpx(1, "sockfd=%d. shutdown", aio->sockfd);
        goto out;
    }
    if(!NDAS_SUCCESS(sk->err)) {
        retval = lpx_sock_error(sk);
        lpx_complete_aio_leftover(aio, retval);
        debug_lpx(1, "sockfd=%d. socket error", aio->sockfd);
        goto out;
    }

    // Set socket to the AIO structure
    _lpx_set_system_sk(sk, aio);
    retval = stm_do_sendpages(sk, aio);
out:
    return retval;
}

ndas_error_t lpx_sendpages(
    int lpxfd , sal_page** pages, unsigned long offset, unsigned long size, int flags)
{
    lpx_aio aio;
    
    lpx_prepare_aio(
        lpxfd,
        0, // Indicate no mem block used.
        pages,
        offset,
        size,
        flags,
        &aio);

    return lpx_sendpages_aio(&aio);
}

ndas_error_t lpx_connect(int lpxfd, const struct sockaddr_lpx *serv_addr, int addrlen)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;

    if (sk->type == LPX_TYPE_STREAM) {
        return stm_connect(sk, (struct sockaddr_lpx*)serv_addr, addrlen, 0);    
    }
    return NDAS_ERROR_INVALID_OPERATION;
}

ndas_error_t lpx_listen(int lpxfd, int backlog)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;

    if (sk->type == LPX_TYPE_STREAM) {
        return stm_listen(sk, backlog);
    }
    return NDAS_ERROR_INVALID_OPERATION;
}
ndas_error_t lpx_set_rtimeout(int lpxfd, sal_msec timeout) {
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;
    sk->rtimeout = timeout * SAL_TICKS_PER_SEC / 1000;
    return NDAS_OK;
}
ndas_error_t lpx_set_wtimeout(int lpxfd, sal_msec timeout) {
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;
    sk->wtimeout = timeout * SAL_TICKS_PER_SEC / 1000;
    return NDAS_OK;
}

ndas_error_t lpx_register_dev(const char* devname)
{
    return lpxitf_create(devname);
}
ndas_error_t lpx_unregister_dev(const char* devname)
{
    lpx_interface     *interface;
    if(( interface = lpxitf_find_using_devname(devname)) == NULL) {
        debug_lpx(1, "can't find %s", devname);
        return NDAS_ERROR_EXIST;
    }
    return lpxitf_destroy(interface);
}

ndas_error_t lpx_getname(int lpxfd, struct sockaddr_lpx *uaddr, int *usockaddr_len, int peer)
{
    struct lpx_sock *sk = lpx_get_sock_from_fd(lpxfd);
    if (!sk)
        return NDAS_ERROR_INVALID_HANDLE;
    if (sk->type == LPX_TYPE_DATAGRAM) {
        return dgm_getname(sk, uaddr, usockaddr_len, peer);
    } else if (sk->type == LPX_TYPE_STREAM) {
        return stm_getname(sk, uaddr, usockaddr_len, peer);
    }        
    return NDAS_ERROR_INVALID_OPERATION;
}

ndas_error_t lpx_init(int transfer_unit)
{    
    ndas_error_t err;
    int ret = 0;
    int idx;
    err = xbuf_pool_init(transfer_unit);
    if ( !NDAS_SUCCESS(err) ) {
        return err;
    }
    lpx_max_transfer_size = transfer_unit * 1024;
    for(idx = 0; idx < LPX_SOCKET_HASH_SIZE; idx++) {
        INIT_LIST_HEAD(&lpx_socks[idx]);
    }
    ret = sal_spinlock_create("lock_sock", &lpx_socks_lock);
    if (!ret)
        return NDAS_ERROR_OUT_OF_MEMORY;
    debug_lpx(1,"lpxitf_rcv=%p",lpxitf_rcv);
    return lpx_proto_init();
}

void lpx_cleanup(void)
{
#ifdef DEBUG
    int idx;
    struct list_head *head;
#endif
    debug_lpx(2,"ing");
    while(sal_atomic_read(&lpx_sock_count) > 0) {
        debug_lpx(1, "lpx_cleanup: wait until all sockets are closed"
            " %d remained", sal_atomic_read(&lpx_sock_count));
#ifdef DEBUG
        for(idx = 0; idx < LPX_SOCKET_HASH_SIZE; idx++) {
            list_for_each(head, &lpx_socks[idx])
            {
                struct lpx_sock *sk = list_entry(head,struct lpx_sock, all_node);
                debug_lpx(1, "lpx_cleanup: fd %d, type-%d, state-%d, %d ",
                    sk->lpxfd, sk->type, sk->sock_state, sk->state);
            }
        }
#endif
        sal_msleep(200);
    }
    lpx_proto_finito();
//    sal_assert(lpx_socks_lock != SAL_INVALID_MUTEX);
    sal_spinlock_destroy(lpx_socks_lock);

    xbuf_pool_shutdown();
    debug_lpx(2,"ed");
}

#ifdef XPLAT_XIXFS_EVENT

int lpxitf_Is_local_node(unsigned char *node)
{
    lpx_interface    *i;
    unsigned char    zero_node[LPX_NODE_LEN] = {0, 0, 0, 0, 0, 0};
    unsigned char    bc_node[LPX_NODE_LEN] = {0, 0, 0, 0, 0, 0};


    if(!sal_memcmp(zero_node, node, LPX_NODE_LEN)) {
        return 1;
    }

    if(!sal_memcmp(bc_node, node, LPX_NODE_LEN)) {
        return 1;
    }

        for(i = lpx_interfaces; i ; i = i->itf_next) {
        if(!sal_memcmp(i->itf_node, node, LPX_NODE_LEN) ) {
            return 1;
        }
        }

     return 0;
    }
#endif //#ifdef XPLAT_XIXFS_EVENT


/*
 * LPX AIO implementation
 */
 
 
 /* 
 * Better be non-inline function to exclude error path for performace.
 */
void
lpx_complete_aio_datalen_error(
    lpx_aio * aio,
    int datalen,
    ndas_error_t returncode
){
    _lpx_complete_aio_datalen(aio, datalen, returncode);
}

/* 
 * Better be non-inline function to exclude error path for performace.
 */
void
lpx_complete_aio_leftover(
    lpx_aio *aio,
    ndas_error_t returncode
){
    int datalen;

    datalen = aio->len - (aio->completed_len + aio->error_len);

    _lpx_complete_aio_datalen(aio, datalen, returncode);
}

