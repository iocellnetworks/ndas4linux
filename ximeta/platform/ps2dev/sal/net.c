/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
#include <types.h>
#include <tcpip.h>
#include <ps2ip.h>
#include <thsemap.h>
#include <intrman.h>
#include "sal/types.h"
#include "sal/libc.h"
#include "sal/net.h"
#include "sal/debug.h"
#include "sal/sync.h"
#include "sal/types.h"
#include "sal/mem.h"
#include "sal/thread.h"
#include "../smapx/smapx.h"

#define DEBUG_LEVEL 1

#define    DEBUG_CALL(l, x)    \
    if(l <= DEBUG_LEVEL)      \
        sal_debug_print x
#define DEBUG_HEXDUMP(l, ptr, len) \
    if (l<= DEBUG_LEVEL)    \
        sal_debug_hexdump(ptr, len)
 
#define MAX_PACKET_LENGTH 1600

#define MAX_RX_BUF_COUNT 32

#ifdef wrap_around
#undef wrap_around
#endif
#define wrap_around(x, r)    ((x)%(r))

typedef struct _SAL_NET {
    struct _SAL_NET* next;
    char    devname[SAL_NET_MAX_DEV_NAME_LENGTH];
    char    mac[6]; /* MAC address of this interface */
    struct netif* netif;
    int is_bound;    /* TRUE if interface is bound */
    unsigned short proto; /* Protocol ID in Network order */
    sal_net_rx_proc             rx_handler;
    sal_thread_id    rxpollthread; /* RX polling thread */
    int destroy;
    struct pbuf*    rxbufs[MAX_RX_BUF_COUNT]; /* static ring buffer */
    int                rxbuf_head;
    int                rxbuf_tail;
    int                rxsem;    
} SAL_NET;

static SAL_NET* v_sal_net_list = NULL;
static sal_semaphore v_sal_net_list_lock = SAL_INVALID_SEMAPHORE;

static SAL_NET* _sal_find_net_desc(sal_netdev_desc nd)
{
    SAL_NET* ite = v_sal_net_list;
    sal_semaphore_take(v_sal_net_list_lock, SAL_SYNC_FOREVER);
    while(ite!=NULL) {
        if (ite == (SAL_NET*) nd)
            break;
        ite= ite->next;    
    }
    sal_semaphore_give(v_sal_net_list_lock);
    return ite;
}

static void _sal_free_sal_net(SAL_NET* sn)
{
    SAL_NET* ite;
    sal_semaphore_take(v_sal_net_list_lock, SAL_SYNC_FOREVER);
    /* Unlink link */
    if (sn==v_sal_net_list) {
        v_sal_net_list = sn->next;    
    } else {
        for(ite=v_sal_net_list; ite!=NULL; ite=ite->next) {
            if (ite->next == sn) {
                ite->next = sn->next;
                break;    
            }
        }
    }
    sal_semaphore_give(v_sal_net_list_lock);
    sal_free(sn);
}

/* This function runs in interrupt context */
static int _sal_net_input_hook(struct pbuf* pb, struct netif* netif)
{
    SAL_NET* sn = v_sal_net_list;
    sal_ether_header* hdr;
    xuint32 flags;
    sal_assert(pb);
    sal_assert(netif);
    
    /* Check interface */
    while(sn!=NULL) {
        if (sn->netif == netif)
            break;
        sn= sn->next;    
    }
    if (sn==NULL || !sn->is_bound)
        return 0; /* not handled */
    if (sn->rx_handler == NULL)
        return 0; /* rx handler is not registered */
    /* Check ptorocol type */
    hdr = pb->payload;
    if (hdr->proto != sn->proto) {
        return 0;
    }

    /* Queue to receive buffer */
    if (wrap_around(sn->rxbuf_tail+1, MAX_RX_BUF_COUNT) == sn->rxbuf_head) {
        /* buffer full. Discard */
        sal_debug_print("net rx buffer full\n");
        pbuf_free(pb);
        return 1;    
    }

    CpuSuspendIntr(&flags);
    sal_assert(sn->rxbufs[sn->rxbuf_tail] == 0);
    sn->rxbufs[sn->rxbuf_tail] = pb;
    sn->rxbuf_tail = wrap_around(sn->rxbuf_tail+1, MAX_RX_BUF_COUNT);
    CpuResumeIntr(flags);
    
    iSignalSema(sn->rxsem);
    return 1;
}

static void* _sal_net_recv_thread(void* data)
{
    SAL_NET *sn = (SAL_NET*) data;
    int ret;
    sal_buf_seg segbuf;
    struct pbuf* p;
    xuint32 flags;
    
    sal_assert(data);
    debug_pnp(4, ("_sal_net_recv_thread starting for device: %s\n", sn->devname));
    while(1) {
        ret = WaitSema(sn->rxsem);
        if (sn->destroy) {
            break;
        }
        if (ret<0) {
            break;
        }
        /* while receive ring buffer is not no_pending_dpc */
        while(sn->rxbuf_tail != sn->rxbuf_head) {
            CpuSuspendIntr(&flags);
            /* dequeue from buffer */
            p = sn->rxbufs[sn->rxbuf_head];
            sn->rxbufs[sn->rxbuf_head] = 0;
            sn->rxbuf_head = wrap_around(sn->rxbuf_head + 1, MAX_RX_BUF_COUNT);
            CpuResumeIntr(flags);
            sal_assert(p!=NULL);
            if (sn->rx_handler) {
                /* to do: remove 802.2 and SNAP header if packet contains them */

                /* currently lpx implementation support single segmented platform buffer */
                sal_assert(p->next == NULL); 
                segbuf.total_len = p->tot_len;
                segbuf.size = 1;
                segbuf.mac_header.ptr = p->payload;
                segbuf.mac_header.len = SAL_ETHER_HEADER_SIZE;
                segbuf.segs[0].ptr = p->payload + SAL_ETHER_HEADER_SIZE;
                segbuf.segs[0].len = p->tot_len - SAL_ETHER_HEADER_SIZE;
                debug_pnp(4, ("packet rxed:%d\n", p->len));
                DEBUG_HEXDUMP(4, p->payload+SAL_ETHER_HEADER_SIZE, 32);            
                sn->rx_handler((sal_netdev_desc)sn, p, &segbuf);
            } else {
                pbuf_free(p);
            }
        }
    }
    sn->rxpollthread = SAL_INVALID_THREAD_ID;
    debug_pnp(4, ("_sal_net_recv_thread exiting for device: %s\n", sn->devname));
    sal_thread_exit(0);
    return 0;
}

/* return 0 for OK, negative value for error */
int sal_netdev_get_address(sal_netdev_desc nd, unsigned char* mac)
{
    SAL_NET* sn;

    sn = _sal_find_net_desc(nd);
    if (sn) {
        memcpy(mac, sn->netif->hwaddr,6);
        return 0;
    } else {
        return -1;
    }
}

/* Not supported, and not used */
int sal_netdev_set_address(sal_netdev_desc nd, unsigned char* mac)
{
    return -1;
}

sal_netdev_desc sal_netdev_open(const char* devname)
{
    char mac[6];
    SAL_NET* psn;
    iop_sema_t sema;
    struct netif* netif;
    
    debug_pnp(4, ("sal_netdev_open: %s\n", devname));
    netif = netif_find(devname);
    if (netif == NULL) {
        return SAL_INVALID_NET_DESC;
    }
    
    psn = (SAL_NET*) sal_malloc(sizeof(SAL_NET));
    memset(psn, 0, sizeof(SAL_NET));
    strcpy(psn->devname, devname);
    memcpy(psn->mac, mac, 6);
    psn->netif = netif;
    psn->is_bound = FALSE;
    
    memset(psn->rxbufs, 0, sizeof(psn->rxbufs));
    psn->rxbuf_head = psn->rxbuf_tail = 0;
    
    sema.attr = 0;
    sema.option = 0;
    sema.initial = 0;
    sema.max = 1;    
    psn->rxsem = CreateSema(&sema);    
    
    /* Link to net list */
    sal_semaphore_take(v_sal_net_list_lock, SAL_SYNC_FOREVER);

    psn->next = v_sal_net_list;
    v_sal_net_list = psn;
    
    sal_semaphore_give(v_sal_net_list_lock);
    return (sal_netdev_desc)psn;
}

int sal_netdev_close(sal_netdev_desc nd)
{
    SAL_NET* sn;
    debug_pnp(3, ("sal_netdev_close starting\n"));

    SMapRegisterInputHook(NULL);
    sn = _sal_find_net_desc(nd);
    if (!sn) {
        return -1;    
    }
    sn->destroy = TRUE;
    SignalSema(sn->rxsem);
    
    while(sn->rxpollthread != SAL_INVALID_THREAD_ID) {
        sal_msleep(100);
    }
    DeleteSema(sn->rxsem);

    _sal_free_sal_net(sn);
    debug_pnp(3, ("sal_netdev_close succeeded\n"));
    return 0;
}

int sal_netdev_register_proto(sal_netdev_desc nd, xuint16 protocolid, sal_net_rx_proc rxhandler)
{
    SAL_NET* sn;
    SMAP_LOW_LEVEL_INPUT_HOOK prev_hook;
    debug_pnp(3, ("sal_ether_bind: protocol id 0x%x\n", ntohs(protocolid)));
    sn = _sal_find_net_desc(nd);
    if (!sn) {
        return -1;    
    }
    
    sn->proto = protocolid;
    sn->is_bound = TRUE;
    sn->rx_handler = rxhandler;
    prev_hook = SMapRegisterInputHook(_sal_net_input_hook);
    if (prev_hook) {
        sal_debug_print("Currently smapx supports only single hook\n");
        sal_assert(0);
    }
    sal_thread_create(&sn->rxpollthread, "netrecvthread", 3, 0, _sal_net_recv_thread, (void*)sn);
    return 0;
}

int sal_netdev_unregister_proto(sal_netdev_desc nd, xuint16 protocolid) 
{
    /* to do: */
    
    return 0;    
}
int sal_ether_send(sal_netdev_desc nd, sal_net_buff buf, xuint32 len)
{
    struct pbuf* p =     (struct pbuf*) buf;
    int ret;
    SAL_NET* sn;
    sn = _sal_find_net_desc(nd);
    if (!sn) {
        sal_debug_print("invalid net disc\n");
        pbuf_free(p);
        return -1;    
    }    
    ret = sn->netif->linkoutput(sn->netif, p);
    pbuf_free(p);
    if (ret==0) {
        return len;
    } else {
        debug_pnp(1,("linkoutput  returned %d\n", ret));
        return -1;
    }
}

int sal_net_init(void)
{
    debug_pnp(4, ("sal_net_init\n"));
    v_sal_net_list_lock = sal_semaphore_create("sal_net_list", 1, 1);
    sal_assert(v_sal_net_list_lock!=SAL_INVALID_SEMAPHORE);
    return 0;
}

void sal_net_cleanup(void)
{
    sal_semaphore_destroy(v_sal_net_list_lock);
}

int sal_netdev_get_mtu(sal_netdev_desc nd)
{
    SAL_NET* sn;    
    sn = _sal_find_net_desc(nd);
    if (!sn) {
        return -1;
    }
    return sn->netif->mtu;
}

sal_net_buff sal_net_alloc_platform_buf(sal_sock nd, xuint32 len, sal_buf_seg* bufs)
{
    struct pbuf* p;
      p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!p) {
        sal_debug_print("pbuf alloc(%d) failed\n", len);
        sal_assert(0); /* out of memory */
        return NULL;
    }
    /* currently lpx support single segmented buffer only */
    sal_assert(p->next==NULL);
    bufs->size = 1;
    bufs->total_len = p->len;
    bufs->mac_header.ptr = p->payload;
    bufs->mac_header.len = SAL_ETHER_HEADER_SIZE;
    bufs->segs[0].ptr = p->payload + SAL_ETHER_HEADER_SIZE;
    bufs->segs[0].len = p->len - SAL_ETHER_HEADER_SIZE;
    return (sal_net_buff)p;
}

int sal_net_free_platform_buf(sal_net_buff buf)
{
//    sal_debug_print("free pbuf:%d\n", ((struct pbuf*)buf)->len);
    pbuf_free((struct pbuf*)buf);
    return 0;
}

sal_sock sal_sock_create(int type, int protocol, int mts)
{
    return 1;    
}

void sal_sock_destroy(sal_sock sock) {}

