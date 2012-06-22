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
#include <linux/version.h> // LINUX_VERSION_CODE,KERNEL_VERSION
#include <linux/kernel.h> // container_of
#include <linux/module.h> // module_put for 2.6, MOD_INC/DEC for 2.4, EXPORT_SYMBOL
#include <linux/list.h> // list_node
#include <linux/notifier.h> // struct notifier_block
#include <linux/netdevice.h> // struct netdevice, register_netdevice_notifier, unregister_netdevice_notifier
#include <linux/spinlock.h> // spinlock
#include <linux/net.h> // sock_create
#include <linux/string.h> // memset
#include <linux/time.h>
#ifdef NDAS_SNAP
#include <net/psnap.h> // register_snap_client
#include <net/p8022.h> // register_8022_client/unregister_8022_client
#endif
#include <net/datalink.h> // struct datalink_proto
#include <net/sock.h> // sock_no_socketpair, sock_no_setsockopt, sock_no_getsockopt, struct sock

#include <sal/net.h>
#include <sal/debug.h>
#include <sal/sync.h>
#include <sal/types.h>
#include <sal/mem.h>
#include <sal/thread.h>
#include "linux_ver.h"

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20))
#include "linux/compiler.h"
#endif

/*
 * R8169 works faster when time stamping is enabled. Don't know why.	
#define ENABLE_TIME_STAMPING	1  
*/	
/*
#define ENABLE_PACKET_DROP_TEST 1 
*/
#ifdef ENABLE_PACKET_DROP_TEST
static int v_rx_packet_count;
static int v_tx_packet_count;
#endif

#define PF_LPX 29
#define ETH_P_LPX    0x88ad

#define BROADCAST_NODE    "\xFF\xFF\xFF\xFF\xFF\xFF"

#define SET_LPX_FD(sock,fd) (sock->SK_USER_DATA = (void*) fd)

#ifdef DEBUG
    #ifndef DEBUG_LEVEL_SAL_NET
        #ifdef DEBUG_LEVEL
            #define DEBUG_LEVEL_SAL_NET DEBUG_LEVEL
        #else
            #define DEBUG_LEVEL_SAL_NET 1
        #endif
    #endif
    #ifndef DEBUG_LEVEL_SAL_NET_PACKET
        #ifdef DEBUG_LEVEL
            #define DEBUG_LEVEL_SAL_NET_PACKET DEBUG_LEVEL
        #else
            #define DEBUG_LEVEL_SAL_NET_PACKET 1
        #endif
    #endif    
#endif

#ifdef DEBUG
#define dbgl_salnet(l, x...) do { \
    if ( l <= DEBUG_LEVEL_SAL_NET ) {\
        sal_debug_print("SN|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x); \
    }\
} while(0)
#define debug_sal_net_packet(l, x...) do { \
    if ( l <= DEBUG_LEVEL_SAL_NET_PACKET ) {\
        sal_debug_print("SNP|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x); \
    }\
} while(0)
#else
#define dbgl_salnet(l, x...) do {} while(0)
#define debug_sal_net_packet(l, x...) do {} while(0)
#endif

#define DEBUG_SK_BUFF(skb) \
({\
    static char __buf__[1024];\
    if (!skb)\
        snprintf(__buf__,sizeof(__buf__),"NULL");\
    else\
        snprintf(__buf__,sizeof(__buf__),\
            "struct sk_buff(%p)={mac(%p)=%s,"\
            "data(%dbytes,%p)=%s,tail=%p,"\
            "sock(%p),data_len=%d,head=%p,end=%p}",\
            skb,\
            (void*) _sal_get_mac_hdr(skb),\
            SAL_DEBUG_HEXDUMP_S((void*) _sal_get_mac_hdr(skb), IFHWADDRLEN),\
            skb->len,\
            skb->data,\
            SAL_DEBUG_HEXDUMP(skb->data,skb->len),\
            skb->tail,\
            skb->sk,\
            skb->data_len,\
            skb->head,\
            skb->end\
        );\
    (const char*) __buf__;\
})

    
#define DEBUG_LINUX_SOCK(sk) \
({\
    static char __buf__[1024];\
    if (!sk)\
        snprintf(__buf__,sizeof(__buf__),"NULL");\
    else\
        snprintf(__buf__,sizeof(__buf__),\
            "struct sock(%p)={socket=%p,"\
            "allocation=0x%x}",\
            sk, sk->SK_SOCKET, \
            sk->SK_ALLOCATION\
        );\
    (const char*) __buf__;\
})
    
static inline struct ethhdr* _sal_get_mac_hdr(struct sk_buff* skb) {
#if LINUX_VERSION_USE_SK_BUFF_DATA_T
    return (struct ethhdr *)skb_mac_header(skb); // >= 2.6.22
#else
    return ((struct ethhdr *)skb->mac.raw); // < 2.6.22
#endif    
}

static inline unsigned char * _sal_get_network_hdr(struct sk_buff* skb) {
#if LINUX_VERSION_USE_SK_BUFF_DATA_T
    return (void*)skb_network_header(skb); // >= 2.6.22
#else
    return (skb->nh.raw); // < 2.6.22
#endif
}


#ifdef NDAS_SNAP
static unsigned char _8022_type = 0xD0;
static unsigned char _snap_id[5] = { 0x0, 0x0, 0x0, 0x88, 0xad };
#endif

static struct proto_ops ndas_proto_ops;

unsigned long sal_net_recv_count=0;
unsigned long sal_net_drop_count=0;

static struct net_proto_family lpx_family;

/**
 * the network facility for the driver
 */
static struct {
    struct list_head net_dev_list; // list of _network_device_t
    spinlock_t    lock;
    struct datalink_proto *p8022;
    struct datalink_proto *psnap;
    struct notifier_block *notifier;
    struct packet_type ptype;
    sal_net_change_handler_func net_change_handler;
} v_sal_net_global;

/**
 * _network_device_t
 * @node - circle-linked list node.
 * @devname - the name of network device
 * @mac - the 6 bytes mac address
 * @rx_handler - forward call when ptype.rx_handler is called.
 */
typedef struct _network_device {
    struct list_head link;
    struct net_device	*if_dev;
    sal_net_rx_proc rx_handler;
    int use_virtual_mac; /* Used to change my MAC address to fake MAC. Used for netdisk emulation.*/
    char    devname[IFNAMSIZ];
    char    mac[IFHWADDRLEN]; /* MAC address of this interface */
    char virtual_mac[IFHWADDRLEN];
} _network_device_t;

static _network_device_t* GET_NETWORK_DEVICE_T(sal_netdev_desc nd)
{
    return (_network_device_t*) nd;
}


EXPORT_SYMBOL(sal_net_recv_count);
EXPORT_SYMBOL(sal_net_drop_count);
    
/* wrapped packet receiver */
static inline int sal_ether_recv(struct sk_buff *skb, _network_device_t* pnd)
{
    sal_net_buf_seg bufs;

#ifdef ENABLE_PACKET_DROP_TEST
    v_rx_packet_count++;
    if(unlikely(v_rx_packet_count%10000==0)) {
        printk("(RXD)");
        goto drop;
    }
#endif


    if(unlikely(!pnd->rx_handler))
        goto drop;
    if(unlikely(skb_is_nonlinear(skb))){
        printk("Nonlinear skb not supported. drop.");
        goto drop;
    }

    
    dbgl_salnet(9, "skb=%s",DEBUG_SK_BUFF(skb));
    dbgl_salnet(3, "ing");

    // MAC header
    bufs.mac_header.ptr = (xpointer) _sal_get_mac_hdr(skb);
#if LINUX_VERSION_25_ABOVE
    bufs.mac_header.len = skb->mac_len;
#else
    bufs.mac_header.len = ETH_HLEN;
#endif

    // Network protocol header
    bufs.network_header.ptr = NULL;
    bufs.network_header.len = 0;

    // Payload buffer
    bufs.data_len = skb->len;
    bufs.nr_data_segs = 1;
    bufs.data_segs[0].ptr = skb->data;
    bufs.data_segs[0].len = skb->len;

#ifdef DEBUG
    if(_sal_get_mac_hdr(skb)->h_dest[0] != (unsigned char)0xff) {
        dbgl_salnet(2,"skb->len=%d, data_len=%d", skb->len, skb->data_len);
    }
#endif

//    dbgl_salnet(4, "Recv skb=%p, len=%d data_len=%d h.raw=%p nh.raw=%p mac.raw=%p destr=%p head=%p data=%p tail=%p end=%p",
//     skb, skb->len, skb->data_len, skb->h.raw, skb->nh.raw, skb->mac.raw, skb->destructor, skb->head, skb->data, skb->tail, skb->end );
     dbgl_salnet(4, "nr_frags=%d frag_list=%p",
     skb_shinfo(skb)->nr_frags, skb_shinfo(skb)->frag_list);
    dbgl_salnet(8,"bufs->data_segs[0].ptr=%p",bufs.data_segs[0].ptr);

    (*pnd->rx_handler)((sal_netdev_desc)pnd, (sal_net_buff)skb, &bufs);
    return 0;
drop:
    sal_net_drop_count++;
    kfree_skb(skb);
    return -1;
}

static
inline
int _sal_rx_handler(struct sk_buff* buf, struct net_device* dev, struct packet_type *type)
{
    _network_device_t* pnd;
    struct list_head* i;
    dbgl_salnet(9,"ing");

    sal_net_recv_count++;

    //
    // Acquire spinlock assuming the thread context is softirq ( bottom-half )
    //
    spin_lock_bh(&v_sal_net_global.lock);

    list_for_each(i, &v_sal_net_global.net_dev_list)
    {
        pnd = list_entry(i, _network_device_t, link);
        if (pnd->if_dev == dev) {
            if (type->type == __constant_htons(ETH_P_LPX)) {
                spin_unlock_bh(&v_sal_net_global.lock);
                return sal_ether_recv(buf,pnd);
            } else {
                dbgl_salnet(1, "Protocol type mismatch");
                break;
            }
        }
    }
    spin_unlock_bh(&v_sal_net_global.lock);
    dbgl_salnet(3, "No matching device is registered");
    kfree_skb(buf);

    return NET_RX_SUCCESS;
}

#ifdef NDAS_SNAP
/*
 * If the perfomance on snap/802.2 is important, optimize this. 
 * (hashtable or something) or (Accept the packet for any network device)
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)) 
static int _802_rx_handler(struct sk_buff *buf, struct net_device *dev, struct packet_type *type) 
#else
static int _802_rx_handler(struct sk_buff *buf, struct net_device *dev, struct packet_type *type, struct net_device * pdev) 
#endif
{
    return   _sal_rx_handler(buf, dev, type);
}
#endif /* NDAS_SNAP */

/* lpx packet receiver */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14))
static int sal_rx_handler(struct sk_buff *buf, struct net_device *dev, struct packet_type *type)
#else
static int sal_rx_handler(struct sk_buff *buf, struct net_device *dev, struct packet_type *type, struct net_device *pdev)
#endif
{
    return _sal_rx_handler(buf, dev, type);
}

#if LINUX_VERSION_SK_ALLOC_NEED_PROTO
static struct proto sal_linux_proto = {
	.name	  = "NDAS",
	.owner	  = THIS_MODULE,
	.obj_size = sizeof(struct sock),
};
#endif

void
sal_sock_destruct(struct sock *sk)
{
    dbgl_salnet(4, "%p",sk);
}

static int    _sock_create(struct net *net, struct socket *sock, int protocol, int kern) 
{
    struct sock* sk;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
    sk = sk_alloc(net, PF_LPX, GFP_ATOMIC, &sal_linux_proto);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,5))
#if LINUX_VERSION_SK_ALLOC_NEED_PROTO 
    sk = sk_alloc(PF_LPX, GFP_ATOMIC, &sal_linux_proto, 1);
#else
    sk = sk_alloc(PF_LPX, GFP_ATOMIC, 1, NULL);
#endif
#else // LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,5)
    sk = sk_alloc(PF_LPX, GFP_ATOMIC, 1);
#endif
    
    if(unlikely(!sk)) {
        dbgl_salnet(1,"fail to create sock");
        return -1;
    }

    sock->ops = &ndas_proto_ops;
    sock_init_data(sock, sk);
    sk->SK_DESTRUCT    = sal_sock_destruct;
    sk->SK_ALLOCATION = GFP_ATOMIC;
    return 0;
}

static int sal_sock_prot_release(struct socket *sock) {
    struct sock *sk = sock->sk;

    dbgl_salnet(4,"BSDsock=%p file=%p inode=%p i_count=%d",
        sock, sock->file, SOCK_INODE(sock), SOCK_INODE(sock)->i_count.counter);

    sal_assert(SOCK_INODE(sock)->i_count.counter == 1);

    // Remove creation reference count
    sal_assert(sk);
    if(likely(sk)) {
        sock_put(sk);
        sock->sk = NULL;
    }
    return 0;
}

NDAS_SAL_API void sal_net_set_change_handler(sal_net_change_handler_func handler) 
{
    unsigned long flags;
    spin_lock_irqsave(&v_sal_net_global.lock, flags);    
    v_sal_net_global.net_change_handler = handler;
    spin_unlock_irqrestore(&v_sal_net_global.lock, flags);
}

EXPORT_SYMBOL(sal_net_set_change_handler);

static int v_notifiee(struct notifier_block *self, unsigned long event, void *ptr)
{
    struct net_device *dev = (struct net_device *) ptr;
    unsigned long flags;

    if (unlikely(!dev)) {
        dbgl_salnet(1, "ing dev=%p event=%ld", dev, event);
        return NOTIFY_DONE;
    }
    dbgl_salnet(1, "ing dev=%p(%s) event=%ld", dev, dev->name, event);
    if( event == NETDEV_UP ) {
/*        printk("ndas: NETDEV_UP %s\n", dev->name);*/
        spin_lock_irqsave(&v_sal_net_global.lock, flags);
        if(v_sal_net_global.net_change_handler) {
            spin_unlock_irqrestore(&v_sal_net_global.lock, flags);
            /* Currently only forward to ndas_change_handler_func */
            v_sal_net_global.net_change_handler(SAL_NET_EVENT_UP, dev->name);
        } else {
            spin_unlock_irqrestore(&v_sal_net_global.lock, flags);        
            dbgl_salnet(1, "net change handler is not set");
        }
        return NOTIFY_DONE;
    } else if(event == NETDEV_DOWN) {
/*        printk("ndas: NETDEV_DOWN %s\n", dev->name);*/
        spin_lock_irqsave(&v_sal_net_global.lock, flags);
        if(v_sal_net_global.net_change_handler) {
            spin_unlock_irqrestore(&v_sal_net_global.lock, flags);
            v_sal_net_global.net_change_handler(SAL_NET_EVENT_DOWN, dev->name);
        } else {
            spin_unlock_irqrestore(&v_sal_net_global.lock, flags);
            dbgl_salnet(1, "net change handler is not set");
        }
        return NOTIFY_DONE;
    } else {
        return NOTIFY_DONE;
    }
}

/**
 * sal_net_init - initialize the SAL network layer
 * register rx handler to ether packet, 8022, snap and 
 * register net device event notifier. 
 * 
 */
NDAS_SAL_API int sal_net_init(void)
{
//    int err;
    dbgl_salnet(1,"ing");

    spin_lock_init(&v_sal_net_global.lock);
    
    v_sal_net_global.notifier = NULL;
    INIT_LIST_HEAD(&v_sal_net_global.net_dev_list);
#ifdef NDAS_SNAP
    v_sal_net_global.p8022 = NULL;
    v_sal_net_global.psnap = NULL;

//    request_module("psnap");
//    request_module("p8022");
    // TODO check the drivers(llc, 8022, snap) loaded 
    if (!v_sal_net_global.p8022)
        v_sal_net_global.p8022 = register_8022_client(_8022_type, _802_rx_handler);
    
    if (!v_sal_net_global.p8022)
        sal_error_print("ndas: Unable to register with 802.2\n");
                                                                            
    if (!v_sal_net_global.psnap)
        v_sal_net_global.psnap = register_snap_client(_snap_id, _802_rx_handler);
    
    if (!v_sal_net_global.psnap)
        sal_error_print("ndas: Unable to register with SNAP\n");
#endif /* NDAS_SNAP */

    /* register lpx protocol family */
    lpx_family.family = PF_LPX;
    lpx_family.create = _sock_create;

    /* Initialize lpx protocol BSD operation functions */
    ndas_proto_ops.family = PF_LPX;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    ndas_proto_ops.owner = THIS_MODULE;
#endif
    ndas_proto_ops.release = sal_sock_prot_release;
    ndas_proto_ops.bind = sock_no_bind;
    ndas_proto_ops.connect = sock_no_connect;
    ndas_proto_ops.socketpair = sock_no_socketpair;
    ndas_proto_ops.accept = sock_no_accept;
    ndas_proto_ops.getname = sock_no_getname;
    ndas_proto_ops.poll = datagram_poll;  /* this does seqlpx too */
    ndas_proto_ops.ioctl = sock_no_ioctl;
    ndas_proto_ops.listen = sock_no_listen;
    ndas_proto_ops.shutdown = sock_no_shutdown;
    ndas_proto_ops.setsockopt = sock_no_setsockopt;
    ndas_proto_ops.getsockopt = sock_no_getsockopt;
    ndas_proto_ops.sendmsg = sock_no_sendmsg;
    ndas_proto_ops.recvmsg = sock_no_recvmsg;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0))
    ndas_proto_ops.mmap = sock_no_mmap;
    ndas_proto_ops.sendpage = sock_no_sendpage;
#else
    ndas_proto_ops.dup = sock_no_dup;
    ndas_proto_ops.fcntl = sock_no_fcntl;
#endif

    if ( sock_register(&lpx_family) !=0 ) {
        printk("ndas: fail to register ndas network protocol family\n"); 
        sal_net_cleanup();
        return -1;
    }

    /* Register netdevice notifiee */
    v_sal_net_global.notifier = kmalloc(sizeof(struct notifier_block), GFP_ATOMIC);
    memset(v_sal_net_global.notifier, 0, sizeof(struct notifier_block));
    v_sal_net_global.notifier->notifier_call = v_notifiee;
    v_sal_net_global.notifier->next = NULL;
    v_sal_net_global.notifier->priority = 0;

    register_netdevice_notifier(v_sal_net_global.notifier);

    /* Register the packet type */
    v_sal_net_global.ptype.type = __constant_htons(ETH_P_LPX);
    v_sal_net_global.ptype.func = sal_rx_handler;

    dev_add_pack(&v_sal_net_global.ptype);

#ifdef ENABLE_TIME_STAMPING
    net_enable_timestamp();
#endif
    dbgl_salnet(1,"ed");
    return 0;
}
EXPORT_SYMBOL(sal_net_init);

NDAS_SAL_API void sal_net_cleanup(void)
{
#ifdef ENABLE_TIME_STAMPING
    net_disable_timestamp();
#endif
    dev_remove_pack(&v_sal_net_global.ptype);

    if ( v_sal_net_global.notifier ) { 
        unregister_netdevice_notifier(v_sal_net_global.notifier);
        kfree(v_sal_net_global.notifier);
        v_sal_net_global.notifier = NULL;
    }
    sock_unregister(PF_LPX);
#ifdef NDAS_SNAP
    if (v_sal_net_global.p8022) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
        unregister_8022_client(v_sal_net_global.p8022);
#else
        unregister_8022_client(_8022_type);
#endif 
    }
    v_sal_net_global.p8022 = NULL;
    if (v_sal_net_global.psnap) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
        unregister_snap_client(v_sal_net_global.psnap);
#else
        unregister_snap_client(_snap_id);
#endif
    }
    v_sal_net_global.psnap = NULL;
#endif /* NDAS_SNAP */
}
EXPORT_SYMBOL(sal_net_cleanup);

/**
 * sal_netdev_open - open the network device by the given name
 * @devname - the name of the network device to open
 */
NDAS_SAL_API sal_netdev_desc sal_netdev_open(const char* devname)
{
    unsigned long flags;
    int ret;
    _network_device_t* psn = NULL;
    struct net_device     *dev = NULL;
    struct list_head* i;

    dbgl_salnet(1, "ing name=%s", devname);
    if ( strcmp(devname, "lo") == 0 ) {
        goto errout;
    }
    /* Check device is exists */
    dev = DEV_GET_BY_NAME(devname);

    if (unlikely(dev == NULL)) {
        dbgl_salnet(1, "no such device %s:",devname);
        goto errout;
    }

    spin_lock_irqsave(&v_sal_net_global.lock, flags);
    
    list_for_each(i, &v_sal_net_global.net_dev_list)
    {
        psn = list_entry(i, _network_device_t, link);
        sal_assert(psn != NULL);
        if ( !strncmp(psn->devname,devname,IFNAMSIZ) ) 
        {
            /* Already opened device */
            spin_unlock_irqrestore(&v_sal_net_global.lock, flags);
            psn = NULL;
            goto errout;
        }
    }
    spin_unlock_irqrestore(&v_sal_net_global.lock, flags);    

    /* Now everything OK, 
        create place holder for new net interface */

    psn = (_network_device_t*) sal_malloc(sizeof(_network_device_t));
    if ( !psn ) {
        goto errout;        
    }
    memset(psn, 0, sizeof(_network_device_t));

    strncpy(psn->devname, devname, IFNAMSIZ); // TODO: strncpy 
    psn->if_dev = dev;

    ret = sal_netdev_get_address((sal_netdev_desc)psn, psn->mac);
    if (ret!=0) {
        goto errout;
    }

    /* Link to net list */
    spin_lock_irqsave(&v_sal_net_global.lock, flags);    
    list_add(&psn->link, &v_sal_net_global.net_dev_list); 
    spin_unlock_irqrestore(&v_sal_net_global.lock, flags);    

    dev_put(dev);
    dbgl_salnet(5, "sal_netdev_opened : %s", devname);
    return (sal_netdev_desc)psn;

errout:
    dbgl_salnet(1, "sal_netdev_open : can\'t open %s", devname);
    if (dev) {
        dev_put(dev);
    }
    if (psn) {
        sal_free(psn); // psn is not NULL
    }
    return SAL_INVALID_NET_DESC;
}
EXPORT_SYMBOL(sal_netdev_open);

NDAS_SAL_API int sal_netdev_close(sal_netdev_desc nd)
{
    _network_device_t* dev = GET_NETWORK_DEVICE_T(nd);
    unsigned long flags;
    dbgl_salnet(1, "sal_netdev_close starting");
    if(unlikely(!dev)) {
        return -1;    
    }
    spin_lock_irqsave(&v_sal_net_global.lock, flags);    
    list_del(&dev->link);
    spin_unlock_irqrestore(&v_sal_net_global.lock, flags);    
    sal_free(dev);

    dbgl_salnet(1, "sal_netdev_close succeeded");
    return 0;
}
EXPORT_SYMBOL(sal_netdev_close);

NDAS_SAL_API int sal_netdev_get_mtu(sal_netdev_desc nd)
{
    _network_device_t* dev = GET_NETWORK_DEVICE_T(nd);
    struct net_device *ndev = DEV_GET_BY_NAME(dev->devname);
    if (likely(ndev)) {
        int mtu = ndev->mtu;
#ifdef NDAS_ESN7108_MTU_FIX
        /* ESN7108 does not work with 1500 payload packet. Reduced packet size fixes the problem */
        mtu -= 20;
#endif
        dbgl_salnet(1, "MTU=%d\n", mtu);
        dev_put(ndev);
        return mtu;
    } else {
        dbgl_salnet(1, "Can't find interface %s", dev->devname);
        return 0;
    }
}
EXPORT_SYMBOL(sal_netdev_get_mtu);

/* 
 allocate sturct sk_buff
 */
NDAS_SAL_API sal_net_buff sal_net_alloc_platform_buf_ex(
    sal_sock nd, xuint32 nethdr_len, xuint32 data_len, sal_net_buf_seg* bufs, int noblock)
{
    struct sk_buff *skb;
    struct sock* sk = (struct sock*) nd;
    int skb_size;
    
//  unsigned long flags;
//  spinlock_t lock = SPIN_LOCK_UNLOCKED;
    int non_blocking_call = noblock ? 1 : 0;
    int err;
    dbgl_salnet(3, "nd=%p len=%d bufs=%p", nd, data_len, bufs);
    dbgl_salnet(5, "sal_net_alloc_platform_buf: sk=%s, len=%d, bufs=%p", 
                    DEBUG_LINUX_SOCK(sk), data_len, bufs);

    /* sock_alloc_send_skb will alloc single linear data buffer with length len */

    // add MAX_HEADER bytes for the header of underlying protocol such as Ethernet.
    skb_size = MAX_HEADER + nethdr_len + data_len;

#ifdef NDAS_ESN7108_MTU_FIX
    {
        int pklen = ETH_HLEN + nethdr_len + data_len;
    // Some ethernet drivers must access more than 2 + 64 + 2 bytes
    // due to NIC's DMA bugs or limitation.
	//
    // Eg) GO700x/GO800x on-board Ethernet embbeded in UDP Tech IP camera.

        if(pklen < 68)
            skb_size += 68 - pklen;
    }

#endif

    skb = sock_alloc_send_skb(sk, skb_size, non_blocking_call, &err); 

    if(unlikely(!skb) ) {
			if(!noblock) {
				printk("Failed to allocate skb len=%d ( %d )\n",
                    skb_size, ETH_HLEN + nethdr_len + data_len);
			}
        return NULL;
    }

    /* Reserve space for headers */
    skb_reserve(skb, MAX_HEADER + nethdr_len);

    /* Mac header.  */
    /* mac header is not touched by LPX for send purpose
       sal_ether_send() will fill the mac header */
    bufs->mac_header.ptr = NULL;
    bufs->mac_header.len = 0;

    /* network protocol ( LPX ) header */
    bufs->network_header.ptr = skb_push(skb, nethdr_len);
    bufs->network_header.len = nethdr_len;
#if LINUX_VERSION_USE_SK_BUFF_DATA_T
    skb_reset_network_header(skb);
#else
    skb->nh.raw = skb->data;
#endif

    /* payload of network protocol ( LPX ) */
    bufs->nr_data_segs = 1;

    bufs->data_segs[0].ptr = skb_put(skb, data_len);
    bufs->data_len = bufs->data_segs[0].len = data_len;

    dbgl_salnet(9, "ed skb=%s", DEBUG_SK_BUFF(skb));

//    dbgl_salnet(1, "Alloc skb=%p, len=%d", skb, skb->len);
    return (sal_net_buff) skb;
}
EXPORT_SYMBOL(sal_net_alloc_platform_buf_ex);

NDAS_SAL_API sal_net_buff sal_net_alloc_platform_buf(
    sal_sock nd, xuint32 nethdr_len, xuint32 data_len, sal_net_buf_seg* bufs) {

    return sal_net_alloc_platform_buf_ex(nd, nethdr_len, data_len, bufs, 0); // Blocking allocation
}
EXPORT_SYMBOL(sal_net_alloc_platform_buf);

#if 0
NDAS_SAL_API sal_net_buff sal_net_get_platform_buf(sal_net_buff buf)
{
    return (sal_net_buff)skb_get((struct sk_buff*)buf);
}
EXPORT_SYMBOL(sal_net_get_platform_buf);
#endif

NDAS_SAL_API int sal_net_free_platform_buf(sal_net_buff buf)
{
//    dbgl_salnet(1, "Free skb=%p, len=%d", buf, ((struct sk_buff *) buf)->len);    
    dbgl_salnet(9, "ing"); 
    kfree_skb((struct sk_buff *) buf);
    dbgl_salnet(9, "ed");
    return 0;
}
EXPORT_SYMBOL(sal_net_free_platform_buf);

static
inline
int
default_kmalloc_flags(void)
{
    int kmalloc_flags = 0;

#if LINUX_VERSION_25_ABOVE
    if(in_atomic() || in_interrupt() || irqs_disabled())
#else
    if(in_interrupt())
#endif
    {
        kmalloc_flags |= GFP_ATOMIC;
    }
    else
    {
        kmalloc_flags |= GFP_KERNEL;
    }

    return kmalloc_flags;
}

NDAS_SAL_API sal_net_buff sal_net_clone_platform_buf(sal_net_buff buf)
{
    struct sk_buff* skb;
    skb = skb_clone((struct sk_buff*)buf, default_kmalloc_flags());
    if(unlikely(skb==NULL)) {
        printk("Failed to clone skb\n");
    } else {
//        dbgl_salnet(1, "Clone skb=%p, len=%d(src=%p)", skb, skb->len, buf);
/*	printk("skb_clone: skb=%p, mac=%p, data=%p -> skb=%p, mac=%p, data=%p\n",
		(char*) buf, ((struct sk_buff*)buf)->mac.raw, ((struct sk_buff*)buf)->data, 
		(char*) skb, skb->mac.raw, skb->data);*/
    }
    return (sal_net_buff) skb;
}
EXPORT_SYMBOL(sal_net_clone_platform_buf);

NDAS_SAL_API sal_net_buff sal_net_copy_platform_buf(sal_net_buff buf, xuint32 neth_len, xuint32 data_len, sal_net_buf_seg* bufs)
{
    struct sk_buff* skb;
    skb = skb_copy((struct sk_buff*)buf, default_kmalloc_flags());
    if (unlikely(skb==NULL)) {
        printk("Failed to copy skb\n");
	   return NULL;
    }

    bufs->network_header.ptr = (xpointer) _sal_get_network_hdr(skb);
    bufs->network_header.len = neth_len;

    /* skb_copy returns linear skb*/
    bufs->data_segs[0].ptr = _sal_get_network_hdr(skb) + neth_len;
    bufs->data_segs[0].len = data_len;
    bufs->nr_data_segs = 1;
    bufs->data_len = data_len;

//    dbgl_salnet(1, "Copy skb=%p, len=%d(src=%p)", skb, skb->len, buf);
    return (sal_net_buff) skb;
}
EXPORT_SYMBOL(sal_net_copy_platform_buf);


NDAS_SAL_API int sal_net_is_platform_buf_shared(sal_net_buff buf)
{
    return skb_shared((struct sk_buff*)buf);
}
EXPORT_SYMBOL(sal_net_is_platform_buf_shared);


/**
 * sal_netdev_register_proto
 * Protocol is already registered by sal_net_register_proto.
 * Just add rxhandler.
 */
NDAS_SAL_API int sal_netdev_register_proto(sal_netdev_desc nd, xuint16 protocolid, sal_net_rx_proc rxhandler)
{
    _network_device_t* pnd = GET_NETWORK_DEVICE_T(nd);
    dbgl_salnet(5, "ing");

    if (unlikely(!pnd)) {
        return -1;    
    }
    pnd->rx_handler = rxhandler;
    dbgl_salnet(5, "ed");
    return 0;
}
EXPORT_SYMBOL(sal_netdev_register_proto);

NDAS_SAL_API int sal_netdev_unregister_proto(sal_netdev_desc nd, xuint16 protocolid) 
{
    _network_device_t* pnd = GET_NETWORK_DEVICE_T(nd);
    dbgl_salnet(1, "ing");
    if (unlikely(!pnd)) {
        return -1;    
    }
    pnd->rx_handler = NULL;
    dbgl_salnet(1, "ed");
    return 0;
}
EXPORT_SYMBOL(sal_netdev_unregister_proto);
/**
 * sal_netdev_set_address 
 * Set the virtual mac address of the network device
 * @param nd the network descriptor
 * @param mac the virtual mac address to be set
 */
NDAS_SAL_API int sal_netdev_set_address(sal_netdev_desc nd, unsigned char* mac)
{
    _network_device_t* sn = GET_NETWORK_DEVICE_T(nd);
    if (unlikely(!sn)) {
        return -1;    
    }
    
    sn->use_virtual_mac = 1;
    memcpy(sn->virtual_mac, mac, IFHWADDRLEN);
    return 0;
}
EXPORT_SYMBOL(sal_netdev_set_address);

/**
 * sal_netdev_get_address -
 * Get the mac address of the network device
 * @param nd the net descriptor
 * @param 6 bytes mac address found (out parameter)
 * @return 0 for OK, negative value for error 
 */
NDAS_SAL_API int sal_netdev_get_address(sal_netdev_desc nd, unsigned char* mac)
{
    _network_device_t* pnd = GET_NETWORK_DEVICE_T(nd);
    struct net_device* dev;
    if (unlikely(!pnd)) {
        return -1;    
    }
    
    if (pnd->use_virtual_mac) {
        memcpy(mac, pnd->virtual_mac, IFHWADDRLEN);
        return 0;
    }
    
    dev =  DEV_GET_BY_NAME(pnd->devname);
    if (likely(dev)) {
        if (unlikely(dev->addr_len != IFHWADDRLEN )) {
            dbgl_salnet(1, "the size of mac address is %d bytes:", dev->addr_len);
            dev_put(dev);
            return -1;
        } else {
            memcpy(mac, dev->dev_addr, dev->addr_len);
            dev_put(dev);
            return 0;
        }
    } else {
        return -1;
    }
}
EXPORT_SYMBOL(sal_netdev_get_address);

void                    (*destructor)(struct sk_buff *skb);
/**
 * sal_ether_send
 */
NDAS_SAL_API int sal_ether_send(sal_netdev_desc nd, sal_net_buff buf, char* dest, xuint32 len)
{
    struct sk_buff* skb = (struct sk_buff*)buf;
    _network_device_t* pnd;
    struct net_device     *dev;
    int ret;

#if LINUX_VERSION_25_ABOVE  
    if(unlikely(in_irq() || irqs_disabled())) {
        printk(KERN_ERR "IRQs disabled. %lu %d\n", in_irq(), irqs_disabled());
#ifdef DEBUG
        *(char *)0 = 0;
#endif
        return -1;
    }
#endif

#ifdef ENABLE_PACKET_DROP_TEST
    v_tx_packet_count++;
    if (unlikely(v_tx_packet_count%10000==0)) {
        printk("TXD");
        sal_net_free_platform_buf(buf);
        return -1;
    }
#endif

    dbgl_salnet(4, "len=%d", len);
    pnd = GET_NETWORK_DEVICE_T(nd);
    if (unlikely(!pnd)) {
        sal_net_free_platform_buf(buf);
        dbgl_salnet(1, "len=%d", len);
        return -1;
    }
    dbgl_salnet(4, "sal_ether_send to %s, %d bytes", pnd->devname, len);

    dev = DEV_GET_BY_NAME(pnd->devname);

    if (unlikely(dev == NULL)) {
        dbgl_salnet(1, "Device %s is down.", pnd->devname);
         sal_net_free_platform_buf(buf);
         return -1;
    }

    if(unlikely(dev->flags & IFF_LOOPBACK)) {
        debug_sal_net_packet(1, "send to local loop is not supported yet");
    }
    skb->dev = dev;
    skb->protocol = __constant_htons(ETH_P_LPX);

    
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
    ret = dev_hard_header(skb, skb->dev, ETH_P_LPX, dest, NULL, skb->len);
    if (ret == 0) {
        sal_assert(FALSE);
        kfree_skb(skb);
        debug_sal_net_packet(0, "dev_hard_header failed. Discarding packet");        
        return 0;
    }
#else
    if(likely(skb->dev->hard_header)) {
        ret = skb->dev->hard_header(skb, skb->dev, ETH_P_LPX, dest, NULL, skb->len);
        if(ret < 0) {
            sal_assert(FALSE);
            kfree_skb(skb);
            debug_sal_net_packet(0, "hard_header failed. Discarding packet");        
            return 0;
        }
    } else {
        sal_assert(FALSE);
        kfree_skb(skb);
        debug_sal_net_packet(0, "No hard_header. Discarding packet");        
        return 0;
    }
#endif

    debug_sal_net_packet(4, "data=%p size=%d", skb->data,skb->len);
    debug_sal_net_packet(4, "%s", SAL_DEBUG_HEXDUMP(skb->data,skb->len));

//    dbgl_salnet(1, "Send skb=%p, len=%d", skb, skb->len);
    /* Note: dev_queue_xmit handles SG buffers. */
#ifdef DEBUG
    if((unsigned long)skb->data & 0x3) {
        dbgl_salnet(4, "Send skb=%p, skb->data=%p len=%d", skb, skb->data, skb->len);
    }
#endif
    ret = dev_queue_xmit(skb);
    if(unlikely(ret < 0)) {
        debug_sal_net_packet(1, "dev_queue_xmit() failed.");
        len = -1;
    }
    dev_put(dev);
    debug_sal_net_packet(6, "ed");

    return len;
}
EXPORT_SYMBOL(sal_ether_send);

/**
 * create the system socket for internal usage of LPX socket.
 * system socket 'struct sock' should be create for allocating sk_buff.
 * even the lpx socket is created directly inside the driver.
 */
NDAS_SAL_API sal_sock sal_sock_create(int type, int protocol, int mts)
{
    struct socket* sock;
    int err;
    //MOD_INC_USE_COUNT;  TODO: the owner of this count should be ndas_core

    dbgl_salnet(5, "type=%d",type);

    err = SOCK_CREATE(PF_LPX, type, 0,&sock);

    if (unlikely(err)) {
        dbgl_salnet(1,"fail to create socket");
        //MOD_DEC_USE_COUNT;
        return NULL;
    }
    sal_assert(sock->sk);

#if LINUX_VERSION_25_ABOVE  
    sock->sk->sk_sndbuf = mts + mts/2;
#else
    sock->sk->sndbuf = mts + mts/2; // Varies on lpx configuration. Increase this if sock_alloc_send_skb fails
#endif
    dbgl_salnet(5, "created socket=%p",sock);
    return (sal_sock) sock->sk;
}
EXPORT_SYMBOL(sal_sock_create);

NDAS_SAL_API void sal_sock_destroy(sal_sock sk0) {
    struct sock* sk = (struct sock*) sk0;   // Linux kernel socket
    struct socket* sock;                    // BSD socket
    dbgl_salnet(5, "sal_sock=%s",DEBUG_LINUX_SOCK(sk));

    sal_assert(sk0 != NULL); // fix me

#if LINUX_VERSION_25_ABOVE
    dbgl_salnet(5, "sal_sock=%s",DEBUG_LINUX_SOCK(sk));
    /* sk_wmem_alloc should be zero.
       sk_wmem_alloc is accumulated skb->truesize ( ~= sizeof(struct sk_buff) + skb->len )
       it increases when skbuff is allocated.
       it decreases when skbuff is deallocated. */
    if(unlikely(sk->sk_wmem_alloc.counter)) {
        printk("sk %p wmem=%d\n", sk, sk->sk_wmem_alloc.counter);
    }
    if(unlikely(sk->sk_rmem_alloc.counter)) {
        printk("sk %p rmem=%d\n",sk, sk->sk_rmem_alloc.counter);
    }
    if(unlikely(sk->sk_wmem_queued)) {
        printk("sk %p rmem=%d\n",sk, sk->sk_wmem_queued);
    }
    if(unlikely(sk->sk_forward_alloc)) {
        printk("sk %p rmem=%d\n",sk, sk->sk_forward_alloc);
    }
#endif

    sock = sk->SK_SOCKET;
    if(likely(sock)) sock_release(sock);

    //MOD_DEC_USE_COUNT; 
}
EXPORT_SYMBOL(sal_sock_destroy);


static inline void call_send_handler(
    sal_net_buf_sender_func sendhandler,
    void* context,
    struct sk_buff *skb,
    int nh_len, // Length of protocol's packet header
    sal_net_buf_seg* bseg
){
    bseg->network_header.ptr = skb_push(skb, nh_len);
    bseg->network_header.len = nh_len;
#if LINUX_VERSION_USE_SK_BUFF_DATA_T
    skb_reset_network_header(skb);
#else
    skb->nh.raw = skb->data;
#endif
    sendhandler(context, (sal_net_buff) skb, bseg);
}

#if !LINUX_VERSION_25_ABOVE
/* Copied from Linux-2.4.22\net\ipv4\tcp.c */
static inline int
skb_can_coalesce(struct sk_buff *skb, int i, struct page *page, int off)
{
	if(unlikely(i)) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i-1];
		return page == frag->page &&
			off == frag->page_offset+frag->size;
	}
	return 0;
}

static inline void
skb_fill_page_desc(struct sk_buff *skb, int i, struct page *page, int off, int size)
{
	skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
	frag->page = page;
	frag->page_offset = off;
	frag->size = size;
	skb_shinfo(skb)->nr_frags = i+1;
}
#endif /* !LINUX_VERSION_25_ABOVE*/

/*
    Create sk_buff from page and call handler for each sk_buff.
    Copied from linux kernel's net\ipv4\tcp.c:do_tcp_sendpages
*/
NDAS_SAL_API int sal_process_page_to_net_buf_mapping(
    void* context, sal_sock system_sock, int nh_len, int mss,
    sal_net_buf_sender_func shandler, 
    sal_page** pages, unsigned long poffset, unsigned long psize
) {
    struct sk_buff *skb = NULL;
    ssize_t copied = 0;
    int size_goal = mss;
    sal_net_buf_seg bseg;
#if LINUX_VERSION_25_ABOVE
    struct sock* sk = (struct sock*) system_sock;
    long timeo = HZ * 30;
#endif

#if LINUX_VERSION_25_ABOVE  
    if(unlikely(irqs_disabled())) {
        dbgl_salnet(1, "WARN: IRQs disabled.");
    }
#endif
   dbgl_salnet(4, "context=%p system_sock=%p nh_len=%d, mss=%d, poffset=%ld psize=%ld",
		context, system_sock, nh_len, mss, poffset, psize);
    while (psize > 0) {
        struct page *page = (struct page *)pages[poffset / PAGE_SIZE];
        int copy, i, can_coalesce;
        int offset = poffset % PAGE_SIZE;
        int size = min_t(size_t, psize, PAGE_SIZE - offset);

        dbgl_salnet(2, "Current page: page=%p offset=%d, size=%d, skb=%p", page, offset, size, skb);

        if (skb == NULL || (copy = size_goal - skb->len) <= 0) {
new_segment:
            dbgl_salnet(2, "new segment: size_goal=%d", size_goal);
#if 0            
            if (!sk_stream_memory_free(sk)) {
                dbgl_salnet(0, "sk_stream_memory_free failed");
                goto wait_for_sndbuf;
            }
#endif

#if LINUX_VERSION_25_ABOVE  
            skb = alloc_skb(MAX_HEADER + nh_len, sk->sk_allocation);
#else
            skb = alloc_skb(MAX_HEADER + nh_len, GFP_KERNEL);
#endif
/*
            skb = sk_stream_alloc_pskb(sk, 0, 0,
            			   sk->sk_allocation);
*/            			   
            if(unlikely(!skb)) {
                dbgl_salnet(0, "alloc_skb failed");
                goto wait_for_memory;
            }
            
            /* Set up skb for lpx */
            skb_reserve(skb, nh_len + ETH_HLEN);
            
            dbgl_salnet(2, "skb->nh=%p", _sal_get_network_hdr(skb));

            /* To do: 
                Fill other fields of bseg in case that ndas core changes contents of packets.
                Right now data part is not touched by lpx. So we don't need to fill it.
            */
            bseg.packet_flags = 0;
            bseg.nr_data_segs = 0;
            bseg.data_len = 0;
            copy = size_goal;
        }

        if (copy > size)
            copy = size;

        i = skb_shinfo(skb)->nr_frags;
        can_coalesce = skb_can_coalesce(skb, i, page, offset);

        if (!can_coalesce && i >= MAX_SKB_FRAGS && i >= SAL_NET_MAX_BUF_SEG_COUNT) {

            // We can assume this packet is not last packet.
            bseg.packet_flags |= SAL_NETBUFSEG_FLAG_FOLLOWING_PACKET_EXIST;
            call_send_handler(shandler, context, skb, nh_len, &bseg);
            skb = NULL;
            copied += copy;

            goto new_segment;
        }

/*
        dbgl_salnet(2, "copy=%d, sk->sk_forward_alloc=%d", copy, sk->sk_forward_alloc);

        if (!sk_stream_wmem_schedule(sk, copy)) {
            dbgl_salnet(0, "sk_stream_wmem_schedule failed");
            goto wait_for_memory;
        }
*/
        if (unlikely(can_coalesce)) {
        	dbgl_salnet(1, "Coalescing. skb=%p shinfo=%p i=%d, copy=%d", skb, skb_shinfo(skb), i, copy);
            skb_shinfo(skb)->frags[i - 1].size += copy;
            // Increase length in SAL segment entry.
            bseg.data_segs[i - 1].len += copy;
        } else {
        	dbgl_salnet(4, "Fregmenting. skb=%p shinfo=%p i=%d, copy=%d buf=%p", skb, skb_shinfo(skb), i, copy, page_address(page) + offset);
            get_page(page);
            skb_fill_page_desc(skb, i, page, offset, copy);
            // Fill SAL segment entry.
            bseg.data_segs[i].ptr = page_address(page) + offset;
            bseg.data_segs[i].len = copy;
            bseg.nr_data_segs = i + 1;
        }

        bseg.data_len += copy;

        skb->len += copy;
        skb->data_len += copy;
#if 0
        skb->truesize += copy;
        sk->sk_wmem_queued += copy;
        sk->sk_forward_alloc -= copy;
#endif

        copied += copy;
        poffset += copy;
        if (!(psize -= copy))
            goto out;

        if (skb->len < mss)
            continue;

        // This is not last packet in the data buffer.
        // Last packet will send out of the loop.
        // Set packet exist flag.
        bseg.packet_flags |= SAL_NETBUFSEG_FLAG_FOLLOWING_PACKET_EXIST;
        call_send_handler(shandler, context, skb, nh_len, &bseg);
        skb = NULL;
        continue;
#if 0
wait_for_sndbuf:
        set_bit(SOCK_NOSPACE, &sk->sk_socket->flags);
#endif
wait_for_memory:
        if (copied && skb)  {
            // We can assume this is the last packet.
            call_send_handler(shandler, context, skb, nh_len, &bseg);
            skb = NULL;
        }
#if LINUX_VERSION_25_ABOVE
        if (unlikely(sk_stream_wait_memory(sk, &timeo) != 0))
            goto do_error;
#else
        /* to do: copy code from wait_for_tcp_memory */
        goto do_error;
#endif
    }
out:
    if (copied && skb) {
        // We can assume this is the last packet.
        call_send_handler(shandler, context, skb, nh_len, &bseg);
        skb = NULL;
    }
    return copied;

do_error:
    if (copied) {
        return copied;
    }
    dbgl_salnet(1, "Error! copy=%d", copied);
    return -1;
}
EXPORT_SYMBOL(sal_process_page_to_net_buf_mapping);



/*
    Create sk_buff from buffer segment structure
*/
NDAS_SAL_API
struct sk_buff *
sal_net_alloc_buf_bufseg(
    sal_sock system_sock,
    sal_net_buf_seg *bufseg
) {
    struct sk_buff *skb = NULL;
    int i;
    struct page *page;
    int offset;
    struct sal_mem_block *memblk;
    int nh_len = bufseg->network_header.len;
#if LINUX_VERSION_25_ABOVE
    struct sock* sk = (struct sock*) system_sock;
    long timeo = HZ * 30;
#endif

#if LINUX_VERSION_25_ABOVE  
    if(unlikely(irqs_disabled())) {
        dbgl_salnet(1, "WARN: IRQs disabled.");
    }
#endif
   dbgl_salnet(4, "system_sock=%p bufseg=%p nh_len=%d",
        system_sock, bufseg, nh_len);

    for(;;) {
#if LINUX_VERSION_25_ABOVE  
        skb = alloc_skb(nh_len + ETH_HLEN, sk->sk_allocation);
#else
        skb = alloc_skb(nh_len + ETH_HLEN, GFP_KERNEL);
#endif
        if(unlikely(skb == NULL)) {
#if LINUX_VERSION_25_ABOVE
            if (unlikely(sk_stream_wait_memory(sk, &timeo) != 0))
                goto do_error;
#else
            /* to do: copy code from wait_for_tcp_memory */
            goto do_error;
#endif
        }
        break;
    }

    skb_reserve(skb, nh_len + ETH_HLEN);

    /* Return network and mac header pointers */
    bufseg->network_header.ptr = skb_push(skb, nh_len);
#if LINUX_VERSION_USE_SK_BUFF_DATA_T
    skb_reset_network_header(skb);
#else
    skb->nh.raw = skb->data;
#endif

    /* mac header is not touched for send purpose */
    bufseg->mac_header.ptr = NULL;

    dbgl_salnet(2, "skb->nh=%p", _sal_get_network_hdr(skb));
    dbgl_salnet(4, "nr_data_segs %d", bufseg->nr_data_segs);
    for(i = 0; i< bufseg->nr_data_segs; i++) {
        memblk = bufseg->data_segs + i;
        dbgl_salnet(4, "%d: len=%lu buf=%p", i, memblk->len, memblk->ptr);
        page = virt_to_page(memblk->ptr);
        offset = (xsize_t)(memblk->ptr) % PAGE_SIZE;
        get_page(page);
        skb_fill_page_desc(skb, i, page, offset, memblk->len);
        skb->len += memblk->len;
        skb->data_len += memblk->len;
#if 0
        skb->truesize += memblk->len;
        sk->sk_wmem_queued += memblk->len;
        sk->sk_forward_alloc -= memblk->len;
#endif
    }

    return skb;

do_error:
    dbgl_salnet(1, "Error!");
    return NULL;
}
EXPORT_SYMBOL(sal_net_alloc_buf_bufseg);
