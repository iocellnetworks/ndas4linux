#include "sal/net.h"
#include "sal/debug.h"
#include "sal/sync.h"
#include "sal/types.h"
#include "sal/mem.h"
#include "sal/thread.h"
#include "sal/time.h"

#include "mbuf.h"
#include "iface.h"

#define DEBUG_LEVEL 0

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
    struct iface * iface;

     sal_net_rx_proc             rx_handler;
    int destroy;
    xuint32    stat_rx_count;
    xuint32    stat_tx_count;    
} SAL_NET;

static SAL_NET* v_sal_net_list = NULL;
static sal_mutex v_sal_net_list_lock = SAL_INVALID_MUTEX;

static void* _sal_net_recv_thread(void* data);

static SAL_NET* _sal_find_sal_net(sal_netdev_desc nd)
{
    return (SAL_NET*) nd;
}

static void _sal_free_sal_net(SAL_NET* sn)
{
    unsigned long flags;
    SAL_NET* ite;
    sal_mutex_take(v_sal_net_list_lock, &flags);
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
    sal_mutex_give(v_sal_net_list_lock, &flags);
    sal_free(sn);
}

/* return 0 for OK, negative value for error */
int sal_netdev_get_address(sal_netdev_desc nd, unsigned char* mac)
{
    memcpy(mac, ((SAL_NET*)nd)->iface->hwaddr, 6);
    sal_debug_print("sal_netdev_get_address %s - %02x:%02x:%02x:%02x:%02x:%02x\r\n",
        ((SAL_NET*)nd)->devname,
        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    return 0;
}

sal_netdev_desc sal_netdev_open(const char* devname)
{
    unsigned long flags;
    struct iface* iface; /* From syabas/tcpip/include/iface.h */
    SAL_NET* net;
    iface = if_lookup(devname);
    if (iface==NULL) {
        return SAL_INVALID_NET_DESC;    
    }
    net = (SAL_NET*) sal_malloc(sizeof(SAL_NET));
    memset(net, 0, sizeof(SAL_NET));
    strcpy(net->devname, devname);
    net->iface = iface;
    sal_debug_print("sal_netdev_open: %s, %x\r\n", devname, iface);
    sal_netdev_get_address((sal_netdev_desc)net, net->mac);
    
    /* Link to net list */
    sal_mutex_take(v_sal_net_list_lock, &flags);
    net->next = v_sal_net_list;
    v_sal_net_list = net;
    
    sal_mutex_give(v_sal_net_list_lock, &flags);

    return (sal_netdev_desc)net;
}

int sal_netdev_close(sal_netdev_desc nd)
{
    SAL_NET* sn;
    sal_debug_print("sal_netdev_close starting\r\n");
    sn = _sal_find_sal_net(nd);
    if (!sn) {
        return -1;    
    }
    sn->destroy = TRUE;
    sal_debug_print("sal_netdev_close: Rx packet=%d, Tx packet=%d\r\n", sn->stat_rx_count, sn->stat_tx_count);
    
    _sal_free_sal_net(sn);
    sal_debug_print("sal_netdev_close returning\r\n");
    return 0;
}

int sal_net_init(void)
{
    v_sal_net_list_lock = sal_mutex_create("sal_net_list");
    sal_assert(v_sal_net_list_lock!=SAL_INVALID_MUTEX);
    return 0;
}

void sal_net_cleanup(void)
{
    sal_mutex_destroy(v_sal_net_list_lock);
}

int sal_netdev_get_mtu(sal_netdev_desc nd)
{
    return ((SAL_NET*)nd)->iface->mtu;
}

/**
 * register protocol
 */
int sal_netdev_register_proto(sal_netdev_desc nd, xuint16 protocolid, sal_net_rx_proc rxhandler)
{
    SAL_NET* sn;    
    sn = _sal_find_sal_net(nd);
    if (!sn) {
        return -1;    
    }
    sn->rx_handler = rxhandler;
    return 0;
}

int sal_netdev_unregister_proto(sal_netdev_desc nd, xuint16 protocolid)
{
    return 0;
}


sal_net_buff sal_net_alloc_platform_buf(sal_sock nd, xuint32 len, sal_buf_seg* bufs)
{
    struct mbuf* m;
//    sal_debug_print("sal_net_alloc_platform_buf:%d\r\n", len); 
      m = alloc_mbuf(len);

    if (m==NULL) {
        sal_debug_print("mbuf alloc(%d) failed. \r\n",    len);
        sal_assert(0); /* out of memory */
        return NULL;
    }

    /* currently lpx support single segmented buffer only */
    sal_assert(m->next==NULL);
    bufs->size = 1;
    bufs->total_len = len;
    bufs->mac_header.ptr = m->data;
    bufs->mac_header.len = SAL_ETHER_HEADER_SIZE;
    bufs->segs[0].ptr = m->data + SAL_ETHER_HEADER_SIZE;
    bufs->segs[0].len = len - SAL_ETHER_HEADER_SIZE;
    return (sal_net_buff)m;
}

int sal_net_free_platform_buf(sal_net_buff buf)
{
    struct mbuf* mb = (struct mbuf*)buf;
/*    sal_debug_print("sal_net_free_platform_buf:%d\r\n", len_p(mb)); */
    free_p(&mb);
    if (mb!=NULL) {
        sal_debug_print("Trying to free chained mbuf\r\n");
        /* Assume single mbuf chain */
        sal_assert(0);
    }
    return 0;
}

// This function is used only when NDAS writing function is enabled
NDAS_SAL_API sal_net_buff sal_net_clone_platform_buf(sal_net_buff buf)
{
    struct mbuf* mb = (struct mbuf*) buf;
    mb->refcnt++; 
    return (sal_net_buff) mb;
}

NDAS_SAL_API sal_net_buff sal_net_copy_platform_buf(sal_net_buff buf, xuint32 len, sal_buf_seg* bufs)
{
	//
	// Not yet implemented.
	//
	sal_assert(0);
	return NDAS_ERROR_NOT_IMPLEMENTED;
}
	
int sal_ether_send(sal_netdev_desc nd, sal_net_buff buf, xuint32 len)
{
    struct mbuf* mb = (struct mbuf*)buf;
    SAL_NET* net = (SAL_NET*) nd;

    mb->cnt = len;
/*    sal_debug_print("sal_ether_send %d\r\n", len);
    DEBUG_HEXDUMP(0, mb->data, 64); */
    net->stat_tx_count++;
    
    (*net->iface->raw) (net->iface, &mb);
    return len;
}

sal_sock sal_sock_create(int type, int protocol, int mts)
{
    /* Used only by linux kernel mode */
    return (sal_sock)1;
}

void sal_sock_destroy(sal_sock sock) 
{
    /* Used only by linux kernel mode */
}

        
xuint32 lpx_input(struct iface* i_iface, struct mbuf** bpp)
{
    SAL_NET* ite;
    struct mbuf* mb= *bpp;
    int next_tail;
    sal_buf_seg segbuf;    
    ite = v_sal_net_list;

    while(ite) {
/*        sal_debug_print("%x %x %x\n", ite->iface ,i_iface , ite->rx_handler); */
        if (ite->iface == i_iface && (!ite->destroy) && ite->rx_handler) {
            ite->stat_rx_count++;

/*            sal_debug_print("lpx_input %d\r\n", len_p(mb));
            DEBUG_HEXDUMP(0, mb->data, len_p(mb));     */
            sal_assert(mb->next == NULL);
            segbuf.total_len = len_p(mb);
            segbuf.size = 1;
            segbuf.mac_header.ptr = mb->data;
            segbuf.mac_header.len = SAL_ETHER_HEADER_SIZE;
            segbuf.segs[0].ptr = mb->data + SAL_ETHER_HEADER_SIZE;
            segbuf.segs[0].len = len_p(mb) - SAL_ETHER_HEADER_SIZE;
            ite->rx_handler((sal_netdev_desc)ite, mb, &segbuf);
            return 0;
        }
        ite= ite->next;
    }
    free_p(bpp);
    return 0;
}

