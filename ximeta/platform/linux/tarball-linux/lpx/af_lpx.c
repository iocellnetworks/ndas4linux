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
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
 revised by William Kim 06/10/2008
 -------------------------------------------------------------------------
*/

#include <linux/version.h>
#include <linux/module.h> 

#include <linux/delay.h>

#define __LPX__		1

#define __AF_LPX__

#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/init.h>
#if !__LPX__
#include <linux/lpx.h>
#endif
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/net.h>
#include <linux/netdevice.h>
#include <linux/uio.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/sockios.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/termios.h>

#if !__LPX__
#include <net/lpx.h>
#endif
#include <net/p8022.h>
#include <net/psnap.h>
#include <net/sock.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#include <net/tcp_states.h>
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#include <net/tcp.h>
#endif

#include <asm/uaccess.h>

#if __LPX__
#include <ndas_lpx.h>
#include "netlpx.h"
#endif

#ifdef DEBUG

int	dbg_level_lpx = DBG_LEVEL_LPX;

#define dbg_call(l,x...) do { 								\
    if ( l <= dbg_level_lpx ) {								\
        printk("LP|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif

#ifdef CONFIG_SYSCTL
extern void lpx_register_sysctl(void);
extern void lpx_unregister_sysctl(void);
#else
#define lpx_register_sysctl()
#define lpx_unregister_sysctl()
#endif

/* Configuration Variables */
static char lpxcfg_auto_create_interfaces = 0;
int sysctl_lpx_pprop_broadcasting = 0;

/* Global Variables */
static struct datalink_proto *pEII_datalink = NULL;
static struct datalink_proto *pSNAP_datalink = NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
static const struct proto_ops lpx_dgram_ops;
#else
static struct proto_ops lpx_dgram_ops;
#endif

LIST_HEAD(lpx_interfaces);
DEFINE_SPINLOCK(lpx_interfaces_lock);

struct lpx_interface *lpx_lo_interface = NULL;

extern int lpxrtr_add_route(__be32 network, struct lpx_interface *intrfc,
			    unsigned char *node);
extern void lpxrtr_del_routes(struct lpx_interface *intrfc);
extern int lpxrtr_route_packet(struct sock *sk, struct sockaddr_lpx *uslpx,
			       struct iovec *iov, int len, int noblock);
extern int lpxrtr_route_skb(struct sk_buff *skb);
extern struct lpx_route *lpxrtr_lookup(__be32 net);
extern int lpxrtr_ioctl(unsigned int cmd, void __user *arg);

static int smp_create(struct socket *sock, int protocol);
static void smp_rcv(struct sock *sk, struct sk_buff *skb);
void smp_destroy_sock(struct sock *sk);

#undef LPX_REFCNT_DEBUG
#ifdef LPX_REFCNT_DEBUG
atomic_t lpx_sock_nr;
#endif

struct lpx_interface *lpx_interfaces_head(void)
{
	struct lpx_interface *rc = NULL;

	if (!list_empty(&lpx_interfaces))
		rc = list_entry(lpx_interfaces.next,
				struct lpx_interface, node);
	return rc;
}

static int lpxcfg_get_config_data(struct lpx_config_data __user *arg)
{
	struct lpx_config_data vals;

	vals.lpxcfg_auto_create_interfaces = lpxcfg_auto_create_interfaces;

	return copy_to_user(arg, &vals, sizeof(vals)) ? -EFAULT : 0;
}

/*
 * Note: Sockets may not be removed _during_ an interrupt or inet_bh
 * handler using this technique. They can be added although we do not
 * use this facility.
 */

static void lpx_remove_socket(struct sock *sk)
{
	/* Determine interface with which socket is associated */
	struct lpx_interface *intrfc = lpx_sk(sk)->intrfc;

	if (!intrfc)
		goto out;

	lpxitf_hold(intrfc);
	spin_lock_bh(&intrfc->if_sklist_lock);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	sk_del_node_init(sk);
#else
	{
		struct sock *s;

		s = intrfc->if_sklist;
	
		if (s == sk) {
	
			intrfc->if_sklist = s->next;
			goto out_unlock;
		}

		while (s && s->next) {
	
			if (s->next == sk) {
	
				s->next = sk->next;
				goto out_unlock;
			}
			s = s->next;
		}
	}

out_unlock:
	sk->next = NULL;
	sock_put(sk);

#endif

	spin_unlock_bh(&intrfc->if_sklist_lock);
	lpxitf_put(intrfc);
out:
	return;
}

static void lpx_destroy_socket(struct sock *sk)
{
	lpx_remove_socket(sk);
	skb_queue_purge(&sk->sk_receive_queue);
#ifdef LPX_REFCNT_DEBUG
	atomic_dec(&lpx_sock_nr);
	printk(KERN_DEBUG "LPX socket %p released, %d are still alive\n", sk,
			atomic_read(&lpx_sock_nr));
	if (atomic_read(&sk->sk_refcnt) != 1)
		printk(KERN_DEBUG "Destruction sock lpx %p delayed, cnt=%d\n",
				sk, atomic_read(&sk->sk_refcnt));
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	kfree(lpx_sk(sk));
	lpx_sk(sk) = NULL;
#endif

	dbg_call( 2, "lpx_destroy_socket sk->refcnt = %d\n", atomic_read(&sk->sk_refcnt) );	

	sock_put(sk);
}

/*
 * The following code is used to support LPX Interfaces (LPXITF).  An
 * LPX interface is defined by a physical device and a frame type.
 */


static struct lpx_interface *__lpxitf_find_using_phys(struct net_device *dev,
						      __be16 datalink)
{
	struct lpx_interface *i;

	list_for_each_entry(i, &lpx_interfaces, node) {
		
		dbg_call( 6, "__lpxitf_find_using_phys i->if_dev = %p, dev = %p, i->if_dlink_type = %d, datalink = %d\n", 
						i->if_dev, dev, i->if_dlink_type, datalink );
		
		if (i->if_dev == dev && i->if_dlink_type == datalink)
			goto out;
	}
	i = NULL;
out:
	return i;
}

static struct lpx_interface *lpxitf_find_using_phys(struct net_device *dev,
						    __be16 datalink)
{
	struct lpx_interface *i;

	spin_lock_bh(&lpx_interfaces_lock);
	i = __lpxitf_find_using_phys(dev, datalink);
	if (i)
		lpxitf_hold(i);
	spin_unlock_bh(&lpx_interfaces_lock);
	return i;
}

#if !__LPX__
struct lpx_interface *lpxitf_find_using_net(__be32 net)
{
	struct lpx_interface *i;

	spin_lock_bh(&lpx_interfaces_lock);
	if (net) {
		list_for_each_entry(i, &lpx_interfaces, node)
			if (i->if_netnum == net)
				goto hold;
		i = NULL;
		goto unlock;
	}

	i = lpx_primary_net;
	if (i)
hold:
		lpxitf_hold(i);
unlock:
	spin_unlock_bh(&lpx_interfaces_lock);
	return i;
}
#else
struct lpx_interface *lpxitf_find_using_node(unsigned char *node)
{
	struct lpx_interface	*i = NULL;

	dbg_call( 4, "lpxitf_find_using_node\n" );

	spin_lock_bh(&lpx_interfaces_lock);

	if (list_empty(&lpx_interfaces)) {
		
		i = NULL;
		goto unlock;
	}

	if (node) {

		list_for_each_entry(i, &lpx_interfaces, node)
			if (!memcmp(i->if_node, node, LPX_NODE_LEN))
				goto hold;

		i = NULL;
		goto unlock;
	}

hold:
	lpxitf_hold(i);
unlock:
	spin_unlock_bh(&lpx_interfaces_lock);

	return i;
}
#endif

/* Sockets are bound to a particular LPX interface. */
static void lpxitf_insert_socket(struct lpx_interface *intrfc, struct sock *sk)
{
	lpxitf_hold(intrfc);
	spin_lock_bh(&intrfc->if_sklist_lock);

	lpx_sk(sk)->intrfc = intrfc;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	sk_add_node(sk, &intrfc->if_sklist);
#else

	sock_hold(sk);
	sk->next = NULL;
	
	if (intrfc->if_sklist == NULL) {

		intrfc->if_sklist = sk;
	
	} else {
	
		struct sock *s = intrfc->if_sklist;
	
		while (s->next)
			s = s->next;
		s->next = sk;
	}

#endif

	spin_unlock_bh(&intrfc->if_sklist_lock);
	lpxitf_put(intrfc);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

/* caller must hold intrfc->if_sklist_lock */
static struct sock *__lpxitf_find_socket(struct lpx_interface *intrfc,
					 __be16 port)
{
	struct sock *s;
	struct hlist_node *node;

	dbg_call( 6, "__lpxitf_find_socket intrfc = %p\n", intrfc );

	NDAS_BUG_ON( intrfc == NULL );

	sk_for_each(s, node, &intrfc->if_sklist) {

		dbg_call( 6, "__lpxitf_find_socket, intrfc = %p lpx_sk(s)->port = %x, lpx_sk(s)->port = %x\n",
						intrfc, lpx_sk(s)->port, port );

		if (lpx_sk(s)->port == port)
			goto found;
	}

	s = NULL;
found:
	return s;
}

#else

static struct sock *__lpxitf_find_socket(struct lpx_interface *intrfc,
					 __be16 port)
{
	struct sock *s = intrfc->if_sklist;

	while (s && lpx_sk(s)->port != port)
	     s = s->next;

	return s;
}

#endif

/* caller must hold a reference to intrfc */
static struct sock *lpxitf_find_socket(struct lpx_interface *intrfc,
					__be16 port)
{
	struct sock *s;

	spin_lock_bh(&intrfc->if_sklist_lock);
	s = __lpxitf_find_socket(intrfc, port);
	if (s)
		sock_hold(s);
	spin_unlock_bh(&intrfc->if_sklist_lock);

	return s;
}

#ifdef CONFIG_LPX_INTERN
static struct sock *lpxitf_find_internal_socket(struct lpx_interface *intrfc,
						unsigned char *lpx_node,
						__be16 port)
{
	struct sock *s;
	struct hlist_node *node;

	lpxitf_hold(intrfc);
	spin_lock_bh(&intrfc->if_sklist_lock);

	sk_for_each(s, node, &intrfc->if_sklist) {
		struct lpx_sock *lpxs = lpx_sk(s);

		if (lpxs->port == port &&
		    !memcmp(lpx_node, lpxs->node, LPX_NODE_LEN))
			goto found;
	}
	s = NULL;
found:
	spin_unlock_bh(&intrfc->if_sklist_lock);
	lpxitf_put(intrfc);
	return s;
}
#endif

static void __lpxitf_down(struct lpx_interface *intrfc)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	struct sock *s;
	struct hlist_node *node, *t;
#else
	struct sock *s, *t;
#endif

	//NDAS_BUG_ON(true);

#if !__LPX__
	/* Delete all routes associated with this interface */
	lpxrtr_del_routes(intrfc);
#endif

	spin_lock_bh(&intrfc->if_sklist_lock);
	/* error sockets */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

	sk_for_each_safe(s, node, t, &intrfc->if_sklist) {
		struct lpx_sock *lpxs = lpx_sk(s);

		s->sk_err = ENOLINK;
		s->sk_error_report(s);
		lpxs->intrfc = NULL;
		lpxs->port   = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
		sock_set_flag(s, SOCK_ZAPPED); /* Indicates it is no longer bound */
#else
		s->sk_zapped = 1;	/* Indicates it is no longer bound */
#endif
		sk_del_node_init(s);
	}
	INIT_HLIST_HEAD(&intrfc->if_sklist);

#else

	for (s = intrfc->if_sklist; s;) {
		s->err = ENOLINK;
		s->error_report(s);
		lpx_sk(s)->intrfc = NULL;
		lpx_sk(s)->port   = 0;
		s->zapped = 1;	/* Indicates it is no longer bound */
		t = s;
		s = s->next;
		t->next = NULL;
	}
	intrfc->if_sklist = NULL;

#endif

	spin_unlock_bh(&intrfc->if_sklist_lock);

	/* remove this interface from list */
	list_del(&intrfc->node);

	if (intrfc->if_dev)
		dev_put(intrfc->if_dev);
	kfree(intrfc);
}

void lpxitf_down(struct lpx_interface *intrfc)
{
	spin_lock_bh(&lpx_interfaces_lock);
	__lpxitf_down(intrfc);
	spin_unlock_bh(&lpx_interfaces_lock);
}

static __inline__ void __lpxitf_put(struct lpx_interface *intrfc)
{
	if (atomic_dec_and_test(&intrfc->refcnt))
		__lpxitf_down(intrfc);
}

static int lpxitf_device_event(struct notifier_block *notifier,
				unsigned long event, void *ptr)
{
	struct net_device *dev = ptr;
	struct lpx_interface *i, *tmp;

	if (event != NETDEV_DOWN && event != NETDEV_UP)
		goto out;

	spin_lock_bh(&lpx_interfaces_lock);
	list_for_each_entry_safe(i, tmp, &lpx_interfaces, node)
		if (i->if_dev == dev) {
			if (event == NETDEV_UP)
				lpxitf_hold(i);
			else
				__lpxitf_put(i);
		}
	spin_unlock_bh(&lpx_interfaces_lock);
out:
	return NOTIFY_DONE;
}


static __exit void lpxitf_cleanup(void)
{
	struct lpx_interface *i, *tmp;

	spin_lock_bh(&lpx_interfaces_lock);
	list_for_each_entry_safe(i, tmp, &lpx_interfaces, node)
		__lpxitf_put(i);
	spin_unlock_bh(&lpx_interfaces_lock);
}

static void lpxitf_def_skb_handler(struct sock *sk, struct sk_buff *skb)
{
	switch (lpx_hdr(skb)->pu.p.lpx_type) {

	case LPX_TYPE_RAW:
	case LPX_TYPE_DATAGRAM:

	if (sock_queue_rcv_skb(sk, skb) < 0)
		kfree_skb(skb);

	break;

	case LPX_TYPE_STREAM:

		smp_rcv(sk, skb);
		break;
			
	default:

		kfree_skb(skb);
	}

	return;
}

static void lpxitf_demux_socket(struct lpx_interface *intrfc, struct sk_buff *skb)
{
	struct lpxhdr *lpx = lpx_hdr(skb);
	struct sock   *sock;

	dbg_call( 5, "lpxitf_demux_sock\n" );

#ifdef DEBUG
	if (memcmp(lpx_broadcast_node, eth_hdr(skb)->h_dest, LPX_NODE_LEN)) {

		dbg_call( 3, "lpxitf_demux_sock lpx->destination_port = %x\n", lpx->destination_port );
	}
#endif

	sock = lpxitf_find_socket(intrfc, lpx->destination_port);
	
	if (unlikely(sock == NULL)) {

		if (lpx_lo_interface) {
		
			sock = lpxitf_find_socket( lpx_lo_interface, lpx->destination_port );
		}
	}

	if (unlikely(!sock)) {

		kfree_skb(skb);
		return;
	}

	lpxitf_def_skb_handler( sock, skb );

	sock_put(sock);

	return;
}

static struct sk_buff *lpxitf_adjust_skbuff(struct lpx_interface *intrfc,
					    struct sk_buff *skb)
{
	struct sk_buff *skb2;
	int in_offset = (unsigned char *)lpx_hdr(skb) - skb->head;
	int out_offset = intrfc->if_lpx_offset;
	int len;

	dbg_call( 3, "lpxitf_adjust_skbuff in_offset = %d, out_offset = %d\n", in_offset, out_offset );

	/* Hopefully, most cases */
	if (likely(in_offset >= out_offset))
		return skb;

	dbg_call( 1, "lpxitf_adjust_skbuff in_offset = %d, out_offset = %d\n", in_offset, out_offset );

	/* Need new SKB */
	len  = skb->len + out_offset;
	skb2 = alloc_skb(len, GFP_ATOMIC);
	if (skb2) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
		skb_reset_mac_header(skb2);
		skb_reserve(skb2, out_offset);
		skb_reset_network_header(skb2);
		skb_reset_transport_header(skb2);
		skb_put(skb2, skb->len);
#else
		skb2->mac.raw = skb2->data;
		skb_reserve(skb2, out_offset);
		skb2->nh.raw = skb2->h.raw = skb_put(skb2, skb->len);
#endif
		memcpy(lpx_hdr(skb2), lpx_hdr(skb), skb->len);
		memcpy(skb2->cb, skb->cb, sizeof(skb->cb));
	}
	kfree_skb(skb);
	return skb2;
}

/* caller must hold a reference to intrfc and the skb has to be unshared */
int lpxitf_send2(struct lpx_interface *intrfc, struct sk_buff *skb, char *node)
{
#if !__LPX__
	struct lpxhdr *lpx = lpx_hdr(skb);
#endif
	struct net_device *dev = intrfc->if_dev;
	struct datalink_proto *dl = intrfc->if_dlink;
	unsigned char dest_node[LPX_NODE_LEN];
	int send_to_wire = 1;
	int addr_len;

	dbg_call( 3, "called lpxitf_send2\n" );

	NDAS_BUG_ON( intrfc == NULL );

	/*
	 * We need to know how many skbuffs it will take to send out this
	 * packet to avoid unnecessary copies.
	 */

	if (unlikely(!dl || !dev || dev->flags & IFF_LOOPBACK))
		send_to_wire = 0;	/* No non looped */

	/*
	 * See if this should be demuxed to sockets on this interface
	 *
	 * We want to ensure the original was eaten or that we only use
	 * up clones.
	 */

	if (memcmp(node, intrfc->if_node, LPX_NODE_LEN) == 0) {

		/* Don't charge sender */
		skb_orphan(skb);

		/* Will charge receiver */
		lpxitf_demux_socket( intrfc, skb );
		return 0;
	}

	if (unlikely(!send_to_wire)) {

		kfree_skb(skb);
		goto out;
	}

	/* Determine the appropriate hardware address */
	
	addr_len = dev->addr_len;
	
	if (!memcmp(lpx_broadcast_node, node, LPX_NODE_LEN))
		memcpy(dest_node, dev->broadcast, addr_len);
	else
		memcpy(dest_node, &(node[LPX_NODE_LEN-addr_len]), addr_len);

	/* Make any compensation for differing physical/data link size */
	skb = lpxitf_adjust_skbuff(intrfc, skb);

	if (!skb) {

		NDAS_BUG_ON(true);
		goto out;
	}

	/* set up data link and physical headers */
	skb->dev		= dev;
	skb->protocol	= htons(ETH_P_LPX);

	dbg_call( 3, "From %02X%02X%02X%02X%02X%02X:%4X\n",
                     intrfc->if_node[0], intrfc->if_node[1],
                     intrfc->if_node[2], intrfc->if_node[3],
                     intrfc->if_node[4], intrfc->if_node[5],
					 lpx_hdr(skb)->source_port );

	dbg_call( 3, "To %02X%02X%02X%02X%02X%02X:%4X\n",
                     dest_node[0], dest_node[1],
                     dest_node[2], dest_node[3],
                     dest_node[4], dest_node[5],
					 lpx_hdr(skb)->destination_port );

	dbg_call( 3, "skb->protocol = %X dl = %p\n", skb->protocol, dl );

	/* Send it out */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	dl->request(dl, skb, dest_node);
#else
	dl->datalink_header(dl, skb, dest_node);
	dev_queue_xmit(skb);
#endif

out:
	return 0;
}

int lpxitf_send(struct lpx_interface *intrfc, struct sk_buff *skb, char *lpxnode)
{

	dbg_call( 6, "lpxitf_send intrfc = %p, lpx_lo_interface = %p\n", intrfc, lpx_lo_interface );

	if (unlikely(intrfc == lpx_lo_interface)) {

		struct lpx_interface *i;

		//spin_lock_bh(&lpx_interfaces_lock);

		list_for_each_entry(i, &lpx_interfaces, node) {
		
			struct sk_buff *skb2;

			if (i == lpx_lo_interface) {

				continue;
			}

			dbg_call( 6, "lpxitf_send i = %p, i->if_dlink_type = %d\n", 
							i, i->if_dlink_type );

			skb2 = skb_clone(skb, GFP_ATOMIC);
			lpxitf_send2( i, skb2, lpxnode );
			//lpxitf_send2( i, skb, lpxnode );
		}

		//spin_unlock_bh(&lpx_interfaces_lock);

		kfree_skb(skb);

	} else {

		return lpxitf_send2( intrfc, skb, lpxnode );
	}

	return 0;
}

#if !__LPX__
static int lpxitf_add_local_route(struct lpx_interface *intrfc)
{
	return lpxrtr_add_route(intrfc->if_netnum, intrfc, NULL);
}

static void lpxitf_discover_netnum(struct lpx_interface *intrfc,
				   struct sk_buff *skb);
static int lpxitf_pprop(struct lpx_interface *intrfc, struct sk_buff *skb);
#endif

static int lpxitf_rcv(struct lpx_interface *intrfc, struct sk_buff *skb)
{
	int rc = 0;

	dbg_call( 6, "called lpxitf_rcv\n" );

	lpxitf_hold(intrfc);

	if (eth_hdr(skb)->h_source[5] == 0xE5) {

		dbg_call( 6, "%02X%02X%02X%02X%02X%02X:%4X\n",
    	                 intrfc->if_node[0], intrfc->if_node[1],
        	             intrfc->if_node[2], intrfc->if_node[3],
            	         intrfc->if_node[4], intrfc->if_node[5],
						 lpx_hdr(skb)->source_port );

		dbg_call( 6, "%02X%02X%02X%02X%02X%02X:%4X\n",
    	                 eth_hdr(skb)->h_dest[0], eth_hdr(skb)->h_dest[1],
        	             eth_hdr(skb)->h_dest[2], eth_hdr(skb)->h_dest[3],
            	         eth_hdr(skb)->h_dest[4], eth_hdr(skb)->h_dest[5],
						 lpx_hdr(skb)->destination_port );
	}

#if 0
	if (!memcmp(lpx_broadcast_node, eth_hdr(skb)->h_dest, LPX_NODE_LEN)) {
	//if (lpx_hdr(skb)->destination_port == 0xee00 || lpx_hdr(skb)->destination_port == 0xee) {

		dbg_call( 4, "%02X%02X%02X%02X%02X%02X:%4X\n",
    	                 intrfc->if_node[0], intrfc->if_node[1],
        	             intrfc->if_node[2], intrfc->if_node[3],
            	         intrfc->if_node[4], intrfc->if_node[5],
						 lpx_hdr(skb)->source_port );

		dbg_call( 4, "%02X%02X%02X%02X%02X%02X:%4X\n",
    	                 eth_hdr(skb)->h_dest[0], eth_hdr(skb)->h_dest[1],
        	             eth_hdr(skb)->h_dest[2], eth_hdr(skb)->h_dest[3],
            	         eth_hdr(skb)->h_dest[4], eth_hdr(skb)->h_dest[5],
						 lpx_hdr(skb)->destination_port );

		if (!memcmp(lpx_broadcast_node, eth_hdr(skb)->h_dest, LPX_NODE_LEN) ||
		    !memcmp(intrfc->if_node, eth_hdr(skb)->h_dest, LPX_NODE_LEN)) {
			rc = lpxitf_demux_socket(intrfc, skb, 0);
			goto out_intrfc;
		}
	
	} else {
#endif
		if (!memcmp(lpx_broadcast_node, eth_hdr(skb)->h_dest, LPX_NODE_LEN) ||
		    !memcmp(intrfc->if_node, eth_hdr(skb)->h_dest, LPX_NODE_LEN)) {
			rc = 0;
			lpxitf_demux_socket( intrfc, skb );
			goto out_intrfc;
		}
#if 0
	}
#endif

	kfree_skb(skb);
out_intrfc:
	lpxitf_put(intrfc);
	return rc;
}

#if !__LPX__
static void lpxitf_discover_netnum(struct lpx_interface *intrfc,
				   struct sk_buff *skb)
{
	const struct lpx_cb *cb = LPX_SKB_CB(skb);

	/* see if this is an intra packet: source_net == dest_net */
	if (cb->lpx_source_net == cb->lpx_dest_net && cb->lpx_source_net) {
		struct lpx_interface *i =
				lpxitf_find_using_net(cb->lpx_source_net);
		/* NB: NetWare servers lie about their hop count so we
		 * dropped the test based on it. This is the best way
		 * to determine this is a 0 hop count packet. */
		if (!i) {
			intrfc->if_netnum = cb->lpx_source_net;
			lpxitf_add_local_route(intrfc);
		} else {
			printk(KERN_WARNING "LPX: Network number collision "
				"%lx\n        %s %s and %s %s\n",
				(unsigned long) ntohl(cb->lpx_source_net),
				lpx_device_name(i),
				lpx_frame_name(i->if_dlink_type),
				lpx_device_name(intrfc),
				lpx_frame_name(intrfc->if_dlink_type));
			lpxitf_put(i);
		}
	}
}

/**
 * lpxitf_pprop - Process packet propagation LPX packet type 0x14, used for
 * 		  NetBIOS broadcasts
 * @intrfc: LPX interface receiving this packet
 * @skb: Received packet
 *
 * Checks if packet is valid: if its more than %LPX_MAX_PPROP_HOPS hops or if it
 * is smaller than a LPX header + the room for %LPX_MAX_PPROP_HOPS hops we drop
 * it, not even processing it locally, if it has exact %LPX_MAX_PPROP_HOPS we
 * don't broadcast it, but process it locally. See chapter 5 of Novell's "LPX
 * RIP and SAP Router Specification", Part Number 107-000029-001.
 *
 * If it is valid, check if we have pprop broadcasting enabled by the user,
 * if not, just return zero for local processing.
 *
 * If it is enabled check the packet and don't broadcast it if we have already
 * seen this packet.
 *
 * Broadcast: send it to the interfaces that aren't on the packet visited nets
 * array, just after the LPX header.
 *
 * Returns -EINVAL for invalid packets, so that the calling function drops
 * the packet without local processing. 0 if packet is to be locally processed.
 */
static int lpxitf_pprop(struct lpx_interface *intrfc, struct sk_buff *skb)
{
	struct lpxhdr *lpx = lpx_hdr(skb);
	int i, rc = -EINVAL;
	struct lpx_interface *ifcs;
	char *c;
	__be32 *l;

	/* Illegal packet - too many hops or too short */
	/* We decide to throw it away: no broadcasting, no local processing.
	 * NetBIOS unaware implementations route them as normal packets -
	 * tctrl <= 15, any data payload... */
	if (LPX_SKB_CB(skb)->lpx_tctrl > LPX_MAX_PPROP_HOPS ||
	    ntohs(lpx->lpx_pktsize) < sizeof(struct lpxhdr) +
					LPX_MAX_PPROP_HOPS * sizeof(u32))
		goto out;
	/* are we broadcasting this damn thing? */
	rc = 0;
	if (!sysctl_lpx_pprop_broadcasting)
		goto out;
	/* We do broadcast packet on the LPX_MAX_PPROP_HOPS hop, but we
	 * process it locally. All previous hops broadcasted it, and process it
	 * locally. */
	if (LPX_SKB_CB(skb)->lpx_tctrl == LPX_MAX_PPROP_HOPS)
		goto out;

	c = ((u8 *) lpx) + sizeof(struct lpxhdr);
	l = (__be32 *) c;

	/* Don't broadcast packet if already seen this net */
	for (i = 0; i < LPX_SKB_CB(skb)->lpx_tctrl; i++)
		if (*l++ == intrfc->if_netnum)
			goto out;

	/* < LPX_MAX_PPROP_HOPS hops && input interface not in list. Save the
	 * position where we will insert recvd netnum into list, later on,
	 * in lpxitf_send */
	LPX_SKB_CB(skb)->last_hop.index = i;
	LPX_SKB_CB(skb)->last_hop.netnum = intrfc->if_netnum;
	/* xmit on all other interfaces... */
	spin_lock_bh(&lpx_interfaces_lock);
	list_for_each_entry(ifcs, &lpx_interfaces, node) {
		/* Except unconfigured interfaces */
		if (!ifcs->if_netnum)
			continue;

		/* That aren't in the list */
		if (ifcs == intrfc)
			continue;
		l = (__be32 *) c;
		/* don't consider the last entry in the packet list,
		 * it is our netnum, and it is not there yet */
		for (i = 0; i < LPX_SKB_CB(skb)->lpx_tctrl; i++)
			if (ifcs->if_netnum == *l++)
				break;
		if (i == LPX_SKB_CB(skb)->lpx_tctrl) {
			struct sk_buff *s = skb_copy(skb, GFP_ATOMIC);

			if (s) {
				LPX_SKB_CB(s)->lpx_dest_net = ifcs->if_netnum;
				lpxrtr_route_skb(s);
			}
		}
	}
	spin_unlock_bh(&lpx_interfaces_lock);
out:
	return rc;
}
#endif

static void lpxitf_insert(struct lpx_interface *intrfc)
{
	spin_lock_bh(&lpx_interfaces_lock);
	list_add_tail(&intrfc->node, &lpx_interfaces);
	spin_unlock_bh(&lpx_interfaces_lock);
}

static struct lpx_interface *lpxitf_alloc(struct net_device *dev, __be32 netnum,
					  __be16 dlink_type,
					  struct datalink_proto *dlink,
					  unsigned char internal,
					  int lpx_offset)
{
	struct lpx_interface *intrfc = kmalloc(sizeof(*intrfc), GFP_ATOMIC);

	if (intrfc) {
		intrfc->if_dev		= dev;
#if !__LPX__
		intrfc->if_netnum	= netnum;
#endif
		intrfc->if_dlink_type 	= dlink_type;
		intrfc->if_dlink 	= dlink;
		intrfc->if_internal 	= internal;
		intrfc->if_lpx_offset 	= lpx_offset;
		intrfc->if_sknum 	= LPX_MIN_EPHEMERAL_SOCKET;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		INIT_HLIST_HEAD(&intrfc->if_sklist);
#else
		intrfc->if_sklist = NULL;
#endif
		atomic_set(&intrfc->refcnt, 1);
		spin_lock_init(&intrfc->if_sklist_lock);
	}

	return intrfc;
}

#if !__LPX__
static int lpxitf_create_internal(struct lpx_interface_definition *idef)
{
	struct lpx_interface *intrfc;
	int rc = -EEXIST;

	/* Only one primary network allowed */
	if (lpx_primary_net)
		goto out;

	/* Must have a valid network number */
	rc = -EADDRNOTAVAIL;
	if (!idef->lpx_network)
		goto out;
	intrfc = lpxitf_find_using_net(idef->lpx_network);
	rc = -EADDRINUSE;
	if (intrfc) {
		lpxitf_put(intrfc);
		goto out;
	}
	intrfc = lpxitf_alloc(NULL, idef->lpx_network, 0, NULL, 1, 0);
	rc = -EAGAIN;
	if (!intrfc)
		goto out;
	memcpy((char *)&(intrfc->if_node), idef->lpx_node, LPX_NODE_LEN);
	lpx_internal_net = lpx_primary_net = intrfc;
	lpxitf_hold(intrfc);
	lpxitf_insert(intrfc);

	rc = lpxitf_add_local_route(intrfc);
	lpxitf_put(intrfc);
out:
	return rc;
}
#endif

static __be16 lpx_map_frame_type(unsigned char type)
{
	__be16 rc = 0;

	switch (type) {
	case LPX_FRAME_ETHERII:	rc = htons(ETH_P_LPX);		break;
	case LPX_FRAME_8022:	rc = htons(ETH_P_802_2);	break;
	case LPX_FRAME_SNAP:	rc = htons(ETH_P_SNAP);		break;
	case LPX_FRAME_8023:	rc = htons(ETH_P_802_3);	break;
	}

	return rc;
}

static int lpxitf_create(struct lpx_interface_definition *idef)
{
	struct net_device *dev;
	__be16 dlink_type = 0;
	struct datalink_proto *datalink = NULL;
	struct lpx_interface *intrfc;
	int rc;

	dbg_call( 1, "lpxitf_create %s\n", idef->lpx_device );

#if !__LPX__
	if (idef->lpx_special == LPX_INTERNAL) {
		rc = lpxitf_create_internal(idef);
		goto out;
	}

	rc = -EEXIST;
	if (idef->lpx_special == LPX_PRIMARY && lpx_primary_net)
		goto out;

	intrfc = lpxitf_find_using_net(idef->lpx_network);
	rc = -EADDRINUSE;
	if (idef->lpx_network && intrfc) {
		lpxitf_put(intrfc);
		goto out;
	}

	if (intrfc)
		lpxitf_put(intrfc);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	dev = dev_get_by_name(&init_net, idef->lpx_device);
#else
	dev = dev_get_by_name(idef->lpx_device);
#endif
	rc = -ENODEV;
	if (!dev)
		goto out;

	switch (idef->lpx_dlink_type) {

	case LPX_FRAME_ETHERII:

		if (dev->type != ARPHRD_IEEE802) {

			dlink_type 	= htons(ETH_P_LPX);
			datalink 	= pEII_datalink;
			break;

		} else

			printk(KERN_WARNING "LPX frame type EtherII over "
					"token-ring is obsolete. Use SNAP "
					"instead.\n");
		/* fall through */

	case LPX_FRAME_SNAP:

		NDAS_BUG_ON(true);
		dlink_type 	= htons(ETH_P_SNAP);
		datalink 	= pSNAP_datalink;
		break;

	case LPX_FRAME_NONE: {

		dlink_type 	= htons(ETH_P_LOOP);
		datalink 	= pEII_datalink;

		break;
	}

	default:

		rc = -EPROTONOSUPPORT;
		goto out_dev;
	}

	rc = -ENETDOWN;
	if (!(dev->flags & IFF_UP))
		goto out_dev;

	/* Check addresses are suitable */
	rc = -EINVAL;
	if (dev->addr_len > LPX_NODE_LEN)
		goto out_dev;

	intrfc = lpxitf_find_using_phys(dev, dlink_type);

	if (!intrfc) {

		/* Ok now create */
		intrfc = lpxitf_alloc(dev, idef->lpx_network, dlink_type,
				      		  datalink, 0, 
							  dev->hard_header_len + datalink->header_length);

		rc = -EAGAIN;

		if (!intrfc)
			goto out_dev;
#if !__LPX__
		/* Setup primary if necessary */
		if (idef->lpx_special == LPX_PRIMARY)
			lpx_primary_net = intrfc;

		if (lpx_primary_net == NULL) {

			lpx_primary_net = intrfc;
		}
#endif

		if (!memcmp(idef->lpx_node, "\000\000\000\000\000\000", LPX_NODE_LEN)) {

			memset(intrfc->if_node, 0, LPX_NODE_LEN);
			memcpy(intrfc->if_node + LPX_NODE_LEN - dev->addr_len,
				dev->dev_addr, dev->addr_len);

		} else {

			memcpy(intrfc->if_node, idef->lpx_node, LPX_NODE_LEN);
		}

		dbg_call( 1, "lpxitf_create intrfc = %p intrfc->if_dlink_type = %x, ETH_P_LOOP = %x, htons(ETH_P_LOOP) = %x, "
					 "intrfc->if_dev = %p, intrfc->if_dev->hard_header_len = %d, if_node = %02X:%02X:%02X:%02X:%02X:%02X\n",
        	         intrfc, intrfc->if_dlink_type, ETH_P_LOOP, htons(ETH_P_LOOP), intrfc->if_dev,			
					 intrfc->if_dev->hard_header_len,						
					 intrfc->if_node[0], intrfc->if_node[1],
                	 intrfc->if_node[2], intrfc->if_node[3],
                     intrfc->if_node[4], intrfc->if_node[5] );

		lpxitf_hold(intrfc);
		lpxitf_insert(intrfc);
	}

	if (intrfc->if_dlink_type  == htons(ETH_P_LOOP)) {

		lpx_lo_interface = intrfc;
	}

	dbg_call( 1, "lpxitf_create success %s\n", idef->lpx_device );

#if !__LPX__
	/* If the network number is known, add a route */
	rc = 0;
	if (!intrfc->if_netnum)
		goto out_intrfc;
	rc = lpxitf_add_local_route(intrfc);
out_intrfc:
#endif
	lpxitf_put(intrfc);
	goto out;
out_dev:
	dev_put(dev);
out:
	return rc;
}

#if !__LPX__
static int lpxitf_delete(struct lpx_interface_definition *idef)
{
	struct net_device *dev = NULL;
	__be16 dlink_type = 0;
	struct lpx_interface *intrfc;
	int rc = 0;

	spin_lock_bh(&lpx_interfaces_lock);
	if (idef->lpx_special == LPX_INTERNAL) {
		if (lpx_internal_net) {
			__lpxitf_put(lpx_internal_net);
			goto out;
		}
		rc = -ENOENT;
		goto out;
	}

	dlink_type = lpx_map_frame_type(idef->lpx_dlink_type);
	rc = -EPROTONOSUPPORT;
	if (!dlink_type)
		goto out;

	dev = __dev_get_by_name(idef->lpx_device);
	rc = -ENODEV;
	if (!dev)
		goto out;

	intrfc = __lpxitf_find_using_phys(dev, dlink_type);
	rc = -EINVAL;
	if (!intrfc)
		goto out;

	if (intrfc->if_dlink_type  == htons(ETH_P_LOOP)) {

		lpx_lo_interface = NULL;
	}

	__lpxitf_put(intrfc);

	rc = 0;
out:
	spin_unlock_bh(&lpx_interfaces_lock);
	return rc;
}

#endif

static struct lpx_interface *lpxitf_auto_create(struct net_device *dev,
						__be16 dlink_type)
{
	struct lpx_interface *intrfc = NULL;
	struct datalink_proto *datalink;

	if (!dev)
		goto out;

	/* Check addresses are suitable */
	if (dev->addr_len > LPX_NODE_LEN)
		goto out;

	switch (ntohs(dlink_type)) {
	case ETH_P_LPX:		datalink = pEII_datalink;	break;
	case ETH_P_SNAP:	datalink = pSNAP_datalink;	break;
	default:		goto out;
	}

	intrfc = lpxitf_alloc(dev, 0, dlink_type, datalink, 0,
				dev->hard_header_len + datalink->header_length);

	if (intrfc) {
		memset(intrfc->if_node, 0, LPX_NODE_LEN);
		memcpy((char *)&(intrfc->if_node[LPX_NODE_LEN-dev->addr_len]),
			dev->dev_addr, dev->addr_len);
		spin_lock_init(&intrfc->if_sklist_lock);
		atomic_set(&intrfc->refcnt, 1);
		lpxitf_insert(intrfc);
		dev_hold(dev);
	}

out:
	return intrfc;
}

static int lpxitf_ioctl(unsigned int cmd, void __user *arg)
{
	int rc = -EINVAL;
	struct ifreq ifr;
	int val;

	switch (cmd) {
#if !__LPX__
	case SIOCSIFADDR: {
		struct sockaddr_lpx *slpx;
		struct lpx_interface_definition f;

		rc = -EFAULT;
		if (copy_from_user(&ifr, arg, sizeof(ifr)))
			break;
		slpx = (struct sockaddr_lpx *)&ifr.ifr_addr;
		rc = -EINVAL;
		if (slpx->slpx_family != AF_LPX)
			break;
		f.lpx_network = slpx->slpx_network;
		memcpy(f.lpx_device, ifr.ifr_name,
			sizeof(f.lpx_device));
		memcpy(f.lpx_node, slpx->slpx_node, LPX_NODE_LEN);
		f.lpx_dlink_type = slpx->slpx_type;
		f.lpx_special = slpx->slpx_special;

		if (slpx->slpx_action == LPX_DLTITF)
			rc = lpxitf_delete(&f);
		else
			rc = lpxitf_create(&f);
		break;
	}
#endif
	case SIOCGIFADDR: {
		struct sockaddr_lpx *slpx;
		struct lpx_interface *lpxif;
		struct net_device *dev;

		rc = -EFAULT;
		if (copy_from_user(&ifr, arg, sizeof(ifr)))
			break;
		slpx = (struct sockaddr_lpx *)&ifr.ifr_addr;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
		dev  = __dev_get_by_name(&init_net, ifr.ifr_name);
#else
		dev  = __dev_get_by_name(ifr.ifr_name);
#endif
		rc   = -ENODEV;
		if (!dev)
			break;
		lpxif = lpxitf_find_using_phys(dev,
					   lpx_map_frame_type(slpx->slpx_type));
		rc = -EADDRNOTAVAIL;
		if (!lpxif)
			break;

		slpx->slpx_family	= AF_LPX;
#if !__LPX__
		slpx->slpx_network	= lpxif->if_netnum;
#endif
		memcpy(slpx->slpx_node, lpxif->if_node,
			sizeof(slpx->slpx_node));
		rc = -EFAULT;
		if (copy_to_user(arg, &ifr, sizeof(ifr)))
			break;
		lpxitf_put(lpxif);
		rc = 0;
		break;
	}
	case SIOCALPXITFCRT:
		rc = -EFAULT;
		if (get_user(val, (unsigned char __user *) arg))
			break;
		rc = 0;
		lpxcfg_auto_create_interfaces = val;
		break;
	}

	return rc;
}

#if !__LPX__
/*
 *	Checksum routine for LPX
 */

/* Note: We assume lpx_tctrl==0 and htons(length)==lpx_pktsize */
/* This functions should *not* mess with packet contents */

__be16 lpx_cksum(struct lpxhdr *packet, int length)
{
	/*
	 *	NOTE: sum is a net byte order quantity, which optimizes the
	 *	loop. This only works on big and little endian machines. (I
	 *	don't know of a machine that isn't.)
	 */
	/* handle the first 3 words separately; checksum should be skipped
	 * and lpx_tctrl masked out */
	__u16 *p = (__u16 *)packet;
	__u32 sum = p[1] + (p[2] & (__force u16)htons(0x00ff));
	__u32 i = (length >> 1) - 3; /* Number of remaining complete words */

	/* Loop through them */
	p += 3;
	while (i--)
		sum += *p++;

	/* Add on the last part word if it exists */
	if (packet->lpx_pktsize & htons(1))
		sum += (__force u16)htons(0xff00) & *p;

	/* Do final fixup */
	sum = (sum & 0xffff) + (sum >> 16);

	/* It's a pity there's no concept of carry in C */
	if (sum >= 0x10000)
		sum++;

	/*
	 * Leave 0 alone; we don't want 0xffff here.  Note that we can't get
	 * here with 0x10000, so this check is the same as ((__u16)sum)
	 */
	if (sum)
		sum = ~sum;

	return (__force __be16)sum;
}
#endif

const char *lpx_frame_name(__be16 frame)
{
	char* rc = "None";

	switch (ntohs(frame)) {
	case ETH_P_LPX:		rc = "EtherII";	break;
	case ETH_P_802_2:	rc = "802.2";	break;
	case ETH_P_SNAP:	rc = "SNAP";	break;
	case ETH_P_802_3:	rc = "802.3";	break;
	case ETH_P_TR_802_2:	rc = "802.2TR";	break;
	}

	return rc;
}

const char *lpx_device_name(struct lpx_interface *intrfc)
{
	return intrfc->if_internal ? "Internal" :
		intrfc->if_dev ? intrfc->if_dev->name : "Unknown";
}

#if !__LPX__
/* Handling for system calls applied via the various interfaces to an LPX
 * socket object. */

static int lpx_setsockopt(struct socket *sock, int level, int optname,
			  char __user *optval, int optlen)
{
	struct sock *sk = sock->sk;
	int opt;
	int rc = -EINVAL;

	if (optlen != sizeof(int))
		goto out;

	rc = -EFAULT;
	if (get_user(opt, (unsigned int __user *)optval))
		goto out;

	rc = -ENOPROTOOPT;
	if (!(level == SOL_LPX && optname == LPX_TYPE))
		goto out;

	lpx_sk(sk)->type = opt;
	rc = 0;
out:
	return rc;
}

static int lpx_getsockopt(struct socket *sock, int level, int optname,
	char __user *optval, int __user *optlen)
{
	struct sock *sk = sock->sk;
	int val = 0;
	int len;
	int rc = -ENOPROTOOPT;

	if (!(level == SOL_LPX && optname == LPX_TYPE))
		goto out;

	val = lpx_sk(sk)->type;

	rc = -EFAULT;
	if (get_user(len, optlen))
		goto out;

	len = min_t(unsigned int, len, sizeof(int));
	rc = -EINVAL;
	if(len < 0)
		goto out;

	rc = -EFAULT;
	if (put_user(len, optlen) || copy_to_user(optval, &val, len))
		goto out;

	rc = 0;
out:
	return rc;
}

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

struct proto lpx_proto = {
	.name	  = "LPX",
	.owner	  = THIS_MODULE,
	.obj_size = sizeof(struct sock) + sizeof(struct lpx_sock),
};

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
static int lpx_create(struct net *net, struct socket *sock, int protocol)
#else
static int lpx_create(struct socket *sock, int protocol)
#endif
{
	int rc = -ESOCKTNOSUPPORT;
	struct sock *sk;

	dbg_call( 4, "lpx_create module_refcount(THIS_MODULE) = %d\n", module_refcount(THIS_MODULE) );

	/*
	 * SPX support is not anymore in the kernel sources. If you want to
	 * ressurrect it, completing it and making it understand shared skbs,
	 * be fully multithreaded, etc, grab the sources in an early 2.5 kernel
	 * tree.
	 */

	if (sock->type != SOCK_DGRAM && sock->type != SOCK_STREAM) {
		
		NDAS_BUG_ON(true);
		goto out;
	}

	rc = -ENOMEM;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	sk = sk_alloc(net, PF_LPX, GFP_KERNEL, &lpx_proto);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	sk = sk_alloc(PF_LPX, GFP_KERNEL, &lpx_proto, 1);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	sk = sk_alloc(PF_LPX, GFP_KERNEL, 1, NULL);
	lpx_sk(sk) = kmalloc(sizeof(struct lpx_sock), GFP_KERNEL);
#else
	sk = sk_alloc(PF_LPX, GFP_KERNEL, 1);
#endif
	if (!sk)
		goto out;
#ifdef LPX_REFCNT_DEBUG
	atomic_inc(&lpx_sock_nr);
	printk(KERN_DEBUG "LPX socket %p created, now we have %d alive\n", sk,
			atomic_read(&lpx_sock_nr));
#endif
	
	memset( lpx_sk(sk), 0, sizeof(struct lpx_sock) );

	sock_init_data(sock, sk);
	sk->sk_no_check = 1;		/* Checksum off by default */

	if (sock->sk == NULL) {

		NDAS_BUG_ON( true );
		return -1;
	}

	if (sock->type == SOCK_STREAM) {

		return smp_create( sock, protocol );
	}

	sock->ops = &lpx_dgram_ops;
	rc = 0;
out:

	dbg_call( 4, "lpx_create return module_refcount(THIS_MODULE) = %d\n", module_refcount(THIS_MODULE) );

	return rc;
}

static int lpx_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	dbg_call( 2, "lpx_release sock = %p\n", lpx_release );

	if (!sk)
		goto out;

	if (!sock_flag(sk, SOCK_DEAD))
		sk->sk_state_change(sk);

	sk->sk_shutdown |= RCV_SHUTDOWN;
	sk->sk_shutdown |= SEND_SHUTDOWN;

	sock_set_flag(sk, SOCK_DEAD);
	sock->sk = NULL;
	lpx_destroy_socket(sk);

	dbg_call( 2, "lpx_release return\n" );

out:
	return 0;
}

/* caller must hold a reference to intrfc */

static __be16 lpx_first_free_socketnum(struct lpx_interface *intrfc)
{
	unsigned short socketNum = intrfc->if_sknum;

	spin_lock_bh(&intrfc->if_sklist_lock);

	if (socketNum < LPX_MIN_EPHEMERAL_SOCKET) {

		socketNum = LPX_MIN_EPHEMERAL_SOCKET;
	}

	while (__lpxitf_find_socket(intrfc, htons(socketNum))) {

		socketNum++;

		if (socketNum > LPX_MAX_EPHEMERAL_SOCKET) {

			socketNum = LPX_MIN_EPHEMERAL_SOCKET;
		}
	}

	spin_unlock_bh(&intrfc->if_sklist_lock);

	intrfc->if_sknum = socketNum + 1;

	return htons(socketNum);
}

static int lpx_bind2(struct socket *sock, struct sockaddr *uaddr, int addr_len, struct net_device *dev)
{
	struct sock *sk = sock->sk;
	struct lpx_sock *lpxs = lpx_sk(sk);
	struct lpx_interface *intrfc;
	struct sockaddr_lpx *addr = (struct sockaddr_lpx *)uaddr;
	int rc = -EINVAL;

	dbg_call( 2, "lpx_bind ntohs(addr->slpx_port) = %x sock = %p\n", ntohs(addr->slpx_port), sock );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	if (!sock_flag(sk, SOCK_ZAPPED) || addr_len != sizeof(struct sockaddr_lpx)) {
#else
	if (!sk->sk_zapped || addr_len != sizeof(struct sockaddr_lpx)) {
#endif

		NDAS_BUG_ON(true);
		goto out;
	}

#if !__LPX__
	intrfc = lpxitf_find_using_net(addr->slpx_network);
#else
	
	if (dev) {

		intrfc = lpxitf_find_using_phys(dev, htons(ETH_P_LPX));

	} else {

		intrfc = lpxitf_find_using_node(addr->slpx_node);
	}

#endif

	dbg_call( 2, "lpx_bind intrfc = %p\n", intrfc );

	rc = -EADDRNOTAVAIL;

	if (!intrfc)
		goto out;

	if (!addr->slpx_port) {

		addr->slpx_port = lpx_first_free_socketnum(intrfc);
		rc = -EINVAL;

		if (!addr->slpx_port)
			goto out_put;
	}

	/* protect LPX system stuff like routing/sap */
	
	rc = -EACCES;
	
	if (ntohs(addr->slpx_port) < LPX_MIN_EPHEMERAL_SOCKET && !capable(CAP_NET_ADMIN))
		goto out_put;

	lpxs->port = addr->slpx_port;

#ifdef CONFIG_LPX_INTERN
	if (intrfc == lpx_internal_net) {
		/* The source address is to be set explicitly if the
		 * socket is to be bound on the internal network. If a
		 * node number 0 was specified, the default is used.
		 */

		rc = -EINVAL;
		if (!memcmp(addr->slpx_node, lpx_broadcast_node, LPX_NODE_LEN))
			goto out_put;
		if (!memcmp(addr->slpx_node, lpx_this_node, LPX_NODE_LEN))
			memcpy(lpxs->node, intrfc->if_node, LPX_NODE_LEN);
		else
			memcpy(lpxs->node, addr->slpx_node, LPX_NODE_LEN);

		rc = -EADDRINUSE;
		if (lpxitf_find_internal_socket(intrfc, lpxs->node,
						lpxs->port)) {
			SOCK_DEBUG(sk,
				"LPX: bind failed because port %X in use.\n",
				ntohs(addr->slpx_port));
			goto out_put;
		}
	} else {
		/* Source addresses are easy. It must be our
		 * network:node pair for an interface routed to LPX
		 * with the lpx routing ioctl()
		 */

		memcpy(lpxs->node, intrfc->if_node, LPX_NODE_LEN);

		rc = -EADDRINUSE;

		if (lpxitf_find_socket(intrfc, addr->slpx_port)) {

			SOCK_DEBUG(sk,
				"LPX: bind failed because port %X in use.\n",
				ntohs(addr->slpx_port));
			goto out_put;
		}
	}

	if (intrfc == lpx_lo_interface) {

		struct lpx_interface *i;

		spin_lock_bh(&lpx_interfaces_lock);

		list_for_each_entry(i, &lpx_interfaces, node) {
		
			struct sk_buff *skb2;

			if (i == lpx_lo_interface) {

				continue;
			}

			dbg_call( 5, "lpxitf_send i->if_dev = %p, i->if_dlink_type = %d\n", 
							i->if_dev, i->if_dlink_type );

			if (lpxitf_find_socket(i, addr->slpx_port)) {

				dbg_call( 1,
							"LPX: bind failed because port %X in use by interface %p\n",
							 ntohs(addr->slpx_port, i );
		
				NDAS_BUG_ON(true); // do more
		
				spin_unlock_bh(&lpx_interfaces_lock);
				goto out_put;
			}
		}

		spin_unlock_bh(&lpx_interfaces_lock);
	
	} else {

		if (lpxitf_find_socket(lpx_lo_interface, addr->slpx_port)) {

			dbg_call( 1,
						"LPX: bind failed because port %X in use by interface %p\n",
						 ntohs(addr->slpx_port), lpx_lo_interface );
		
			NDAS_BUG_ON(true); // do more
		
			goto out_put;
		}
	}

#else	/* !def CONFIG_LPX_INTERN */

	/* Source addresses are easy. It must be our network:node pair for
	   an interface routed to LPX with the lpx routing ioctl() */

	rc = -EADDRINUSE;
	if (lpxitf_find_socket(intrfc, addr->slpx_port)) {
		SOCK_DEBUG(sk, "LPX: bind failed because port %X in use.\n",
				ntohs((int)addr->slpx_port));
		goto out_put;
	}

#endif	/* CONFIG_LPX_INTERN */

	lpxitf_insert_socket(intrfc, sk);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	sock_reset_flag(sk, SOCK_ZAPPED);
#else
	sk->sk_zapped = 0;
#endif

	rc = 0;
out_put:
	lpxitf_put(intrfc);
out:
	return rc;
}

static int lpx_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	return lpx_bind2(sock, uaddr, addr_len, NULL);
}

static int lpx_connect(struct socket *sock, struct sockaddr *uaddr,
	int addr_len, int flags)
{
	struct sock *sk = sock->sk;
	struct lpx_sock *lpxs = lpx_sk(sk);
	struct sockaddr_lpx *addr;
	int rc = -EINVAL;
#if !__LPX__
	struct lpx_route *rt;
#endif

	sk->sk_state	= TCP_CLOSE;
	sock->state 	= SS_UNCONNECTED;

	if (addr_len != sizeof(*addr))
		goto out;
	addr = (struct sockaddr_lpx *)uaddr;

	/* put the autobinding in */
	if (!lpxs->port) {
		struct sockaddr_lpx uaddr;

		memset( &uaddr, 0, sizeof(uaddr) );

		uaddr.slpx_port		= 0;
#if !__LPX__
		uaddr.slpx_network 	= 0;
#endif

#ifdef CONFIG_LPX_INTERN
		rc = -ENETDOWN;
		if (!lpxs->intrfc)
			goto out; /* Someone zonked the iface */
		memcpy(uaddr.slpx_node, lpxs->intrfc->if_node,
			LPX_NODE_LEN);
#endif	/* CONFIG_LPX_INTERN */

		rc = lpx_bind(sock, (struct sockaddr *)&uaddr,
			      sizeof(struct sockaddr_lpx));
		if (rc)
			goto out;
	}

#if !__LPX__
	/* We can either connect to primary network or somewhere
	 * we can route to */
	rt = lpxrtr_lookup(addr->slpx_network);
	rc = -ENETUNREACH;
	if (!rt && !(!addr->slpx_network && lpx_primary_net))
		goto out;
#endif

#if !__LPX__
	lpxs->dest_addr.net  = addr->slpx_network;
#endif
	lpxs->dest_addr.port = addr->slpx_port;
	memcpy(lpxs->dest_addr.node, addr->slpx_node, LPX_NODE_LEN);
	lpxs->type = addr->slpx_type;

	if (sock->type == SOCK_DGRAM) {
		sock->state 	= SS_CONNECTED;
		sk->sk_state 	= TCP_ESTABLISHED;
	}

#if !__LPX__
	if (rt)
		lpxrtr_put(rt);
#endif
	rc = 0;
out:
	return rc;
}


static int lpx_getname(struct socket *sock, struct sockaddr *uaddr,
			int *uaddr_len, int peer)
{
	struct lpx_address *addr;
	struct sockaddr_lpx slpx;
	struct sock *sk = sock->sk;
	struct lpx_sock *lpxs = lpx_sk(sk);
	int rc;

	*uaddr_len = sizeof(struct sockaddr_lpx);

	if (peer) {
		rc = -ENOTCONN;
		if (sk->sk_state != TCP_ESTABLISHED)
			goto out;

		addr = &lpxs->dest_addr;
#if !__LPX__
		slpx.slpx_network	= addr->net;
#endif
		slpx.slpx_port		= addr->port;
		memcpy(slpx.slpx_node, addr->node, LPX_NODE_LEN);
	} else {
		if (lpxs->intrfc) {
#if !__LPX__
			slpx.slpx_network = lpxs->intrfc->if_netnum;
#endif
#ifdef CONFIG_LPX_INTERN
			memcpy(slpx.slpx_node, lpxs->node, LPX_NODE_LEN);
#else
			memcpy(slpx.slpx_node, lpxs->intrfc->if_node,
				LPX_NODE_LEN);
#endif	/* CONFIG_LPX_INTERN */

		} else {
#if !__LPX__
			slpx.slpx_network = 0;
#endif
			memset(slpx.slpx_node, '\0', LPX_NODE_LEN);
		}

		slpx.slpx_port = lpxs->port;
	}

	slpx.slpx_family = AF_LPX;
	slpx.slpx_type	 = lpxs->type;
	slpx.slpx_zero	 = 0;
	memcpy(uaddr, &slpx, sizeof(slpx));

	rc = 0;
out:
	return rc;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static int lpx_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt, struct net_device *orig_dev)
#else
static int lpx_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt)
#endif
{
	/* NULL here for pt means the packet was looped back */
	struct lpx_interface *intrfc;

	u16 lpx_pktsize;
	int rc = 0;

	dbg_call( 5, "lpx_rcv\n" );

	/* Not ours */
	if (unlikely(skb->pkt_type == PACKET_OTHERHOST)) {

		goto drop;
	}

	if (unlikely((skb = skb_share_check(skb, GFP_ATOMIC)) == NULL)) {

		goto out;
	}

	if (unlikely(!pskb_may_pull(skb, sizeof(struct lpxhdr)))) {

		goto drop;
	}

	lpx_pktsize = ntohs(lpx_hdr(skb)->pu.packet_size & ~LPX_TYPE_MASK);

	/* Too small or invalid header? */
	if (unlikely(lpx_pktsize < sizeof(struct lpxhdr) || !pskb_may_pull(skb, lpx_pktsize))) {

		goto drop;
	}

	/* Determine what local lpx endpoint this is */
	intrfc = lpxitf_find_using_phys(dev, pt->type);

	if (unlikely(!intrfc)) {

		if (lpxcfg_auto_create_interfaces) {

			intrfc = lpxitf_auto_create(dev, pt->type);

			if (intrfc) {

				lpxitf_hold(intrfc);
			}
		}

		if (!intrfc)
			goto drop;
	}

	rc = lpxitf_rcv(intrfc, skb);
	lpxitf_put(intrfc);
	goto out;
drop:
	kfree_skb(skb);
out:
	return rc;
}

static int lpx_send_packet(struct sock *sk, struct sockaddr_lpx *uslpx,
						   struct iovec *iov, size_t len, int noblock)
{
	struct sk_buff *skb;
	struct lpx_sock *lpxs = lpx_sk(sk);
	struct lpx_interface *intrfc;
	struct lpxhdr *lpx;
	size_t size;
	int lpx_offset;
	int rc;

	intrfc = lpx_sk(sk)->intrfc;

	lpxitf_hold(intrfc);
	lpx_offset = intrfc->if_lpx_offset;
	size = sizeof(struct lpxhdr) + len + lpx_offset;

	skb = sock_alloc_send_skb(sk, size, noblock, &rc);
	if (!skb)
		goto out_put;

	skb->sk = sk;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
	skb_reset_mac_header(skb);
	skb_reserve(skb, lpx_offset);
	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);
	skb_put(skb, sizeof(struct lpxhdr));
#else
	skb->mac.raw = skb->data;
	skb_reserve(skb, lpx_offset);
	skb->h.raw = skb->nh.raw = skb_put(skb, sizeof(struct lpxhdr));
#endif

	lpx = lpx_hdr(skb);
	lpx->pu.packet_size = htons(len + sizeof(struct lpxhdr));

	lpx->source_port 		= lpxs->port;
	lpx->destination_port 	= uslpx->slpx_port;

	rc = memcpy_fromiovec(skb_put(skb, len), iov, len);

	if (rc) {

		kfree_skb(skb);
		goto out_put;
	}

	rc = lpxitf_send(intrfc, skb, uslpx->slpx_node);

out_put:
	lpxitf_put(intrfc);

	return rc;
}
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static int lpx_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
static int lpx_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, int len)
#else
static int lpx_sendmsg(struct socket *sock, struct msghdr *msg, int len, struct scm_cookie *scm)
#endif
{
	struct sock *sk = sock->sk;
	struct lpx_sock *lpxs = lpx_sk(sk);
	struct sockaddr_lpx *uslpx = (struct sockaddr_lpx *)msg->msg_name;
	struct sockaddr_lpx local_slpx;
	int rc = -EINVAL;
	int flags = msg->msg_flags;

	/* Socket gets bound below anyway */
/*	if (sk->sk_zapped)
		return -EIO; */	/* Socket not bound */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	if (flags & ~(MSG_DONTWAIT|MSG_CMSG_COMPAT|MSG_NOSIGNAL)) {
#else
	if (flags & ~(MSG_DONTWAIT|MSG_NOSIGNAL)) {
#endif

		NDAS_BUG_ON(true);
		goto out;
	}

	/* Max possible packet size limited by 16 bit pktsize in header */
	if (len >= 65535 - sizeof(struct lpxhdr)) {

		NDAS_BUG_ON(true);
		goto out;
	}

	if (uslpx) {
		if (!lpxs->port) {
			struct sockaddr_lpx uaddr;

			uaddr.slpx_port		= 0;
#if !__LPX__
			uaddr.slpx_network	= 0;
#endif
#ifdef CONFIG_LPX_INTERN
			rc = -ENETDOWN;
			if (!lpxs->intrfc)
				goto out; /* Someone zonked the iface */
			memcpy(uaddr.slpx_node, lpxs->intrfc->if_node,
				LPX_NODE_LEN);
#endif
			rc = lpx_bind(sock, (struct sockaddr *)&uaddr,
					sizeof(struct sockaddr_lpx));
			if (rc)
				goto out;
		}

		rc = -EINVAL;
		if (msg->msg_namelen < sizeof(*uslpx) ||
		    uslpx->slpx_family != AF_LPX)
			goto out;
	} else {
		rc = -ENOTCONN;
		if (sk->sk_state != TCP_ESTABLISHED)
			goto out;

		uslpx = &local_slpx;
		uslpx->slpx_family 	= AF_LPX;
		uslpx->slpx_type 	= lpxs->type;
		uslpx->slpx_port 	= lpxs->dest_addr.port;

		memcpy(uslpx->slpx_node, lpxs->dest_addr.node, LPX_NODE_LEN);
	}

	rc = lpx_send_packet(sk, uslpx, msg->msg_iov, len, flags & MSG_DONTWAIT);
	if (rc >= 0)
		rc = len;
out:
	return rc;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static int lpx_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t size, int flags)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
static int lpx_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, int size, int flags)
#else
static int lpx_recvmsg(struct socket *sock, struct msghdr *msg, int size, int flags, struct scm_cookie *scm)
#endif
{
	struct sock *sk = sock->sk;
	struct lpx_sock *lpxs = lpx_sk(sk);
	struct sockaddr_lpx *slpx = (struct sockaddr_lpx *)msg->msg_name;
	struct lpxhdr *lpx = NULL;
	struct sk_buff *skb;
	int copied, rc;

	dbg_call( 4, "lpx_recvmsg sock = %p lpxs = %p, lpxs->port = %d, sk = %p, lpxs->intrfc = %p\n", 
					sock, lpxs, lpxs->port, sk, lpxs->intrfc );
 
	/* put the autobinding in */
	if (!lpxs->port) {
		struct sockaddr_lpx uaddr;

		uaddr.slpx_port		= 0;
#if !__LPX__
		uaddr.slpx_network 	= 0;
#endif

#ifdef CONFIG_LPX_INTERN
		rc = -ENETDOWN;
		if (!lpxs->intrfc)
			goto out; /* Someone zonked the iface */
		memcpy(uaddr.slpx_node, lpxs->intrfc->if_node, LPX_NODE_LEN);
#endif	/* CONFIG_LPX_INTERN */

		rc = lpx_bind(sock, (struct sockaddr *)&uaddr,
			      sizeof(struct sockaddr_lpx));
		if (rc)
			goto out;
	}

	rc = -ENOTCONN;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	if (sock_flag(sk, SOCK_ZAPPED))
#else
	if (sk->sk_zapped == 1)
#endif
		goto out;

	skb = skb_recv_datagram(sk, flags & ~MSG_DONTWAIT,
				flags & MSG_DONTWAIT, &rc);
	if (!skb) {

		if (sock_flag(sk, SOCK_DEAD)) {

			rc = -ENOTCONN;

		} else {

			dbg_call( 1, "no skb\n" );
			rc = 0;
		}
	
		goto out;
	}

	lpx 	= lpx_hdr(skb);
#if !__LPX__
	copied 	= ntohs(lpx->lpx_pktsize) - sizeof(struct lpxhdr);
#else
	copied 	= ntohs(lpx->pu.packet_size &~LPX_TYPE_MASK) - sizeof(struct lpxhdr);
#endif
	if (copied > size) {
		copied = size;
		msg->msg_flags |= MSG_TRUNC;
	}

	rc = skb_copy_datagram_iovec(skb, sizeof(struct lpxhdr), msg->msg_iov,
				     copied);
	if (rc)
		goto out_free;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))

	if (skb->tstamp.tv64)
		sk->sk_stamp = skb->tstamp;

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

	if (skb->tstamp.off_sec)
		skb_get_timestamp(skb, &sk->sk_stamp);

#else
	sk->sk_stamp = skb->stamp;
#endif

	msg->msg_namelen = sizeof(*slpx);

	if (slpx) {
		slpx->slpx_family	= AF_LPX;
#if !__LPX__
		slpx->slpx_port		= lpx->lpx_source.sock;
		memcpy(slpx->slpx_node, lpx->lpx_source.node, LPX_NODE_LEN);
		slpx->slpx_network	= LPX_SKB_CB(skb)->lpx_source_net;
		slpx->slpx_type 	= lpx->lpx_type;
#else
		slpx->slpx_port		= lpx->source_port;
		memcpy(slpx->slpx_node, eth_hdr(skb)->h_source, LPX_NODE_LEN);
#endif
		slpx->slpx_zero		= 0;
	}
	rc = copied;

out_free:
	skb_free_datagram(sk, skb);
out:
	return rc;
}


static int lpx_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	int rc = 0;
	long amount = 0;
	struct sock *sk = sock->sk;
	void __user *argp = (void __user *)arg;

	switch (cmd) {
	case TIOCOUTQ:
		amount = sk->sk_sndbuf - atomic_read(&sk->sk_wmem_alloc);
		if (amount < 0)
			amount = 0;
		rc = put_user(amount, (int __user *)argp);
		break;
	case TIOCINQ: {
		struct sk_buff *skb = skb_peek(&sk->sk_receive_queue);
		/* These two are safe on a single CPU system as only
		 * user tasks fiddle here */
		if (skb)
			amount = skb->len - sizeof(struct lpxhdr);
		rc = put_user(amount, (int __user *)argp);
		break;
	}
#if !__LPX__
	case SIOCADDRT:
	case SIOCDELRT:
		rc = -EPERM;
		if (capable(CAP_NET_ADMIN))
			rc = lpxrtr_ioctl(cmd, argp);
		break;
#endif
	case SIOCSIFADDR:
	case SIOCALPXITFCRT:
	case SIOCALPXPRISLT:
		rc = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			break;
	case SIOCGIFADDR:
		rc = lpxitf_ioctl(cmd, argp);
		break;
	case SIOCLPXCFGDATA:
		rc = lpxcfg_get_config_data(argp);
		break;
#if !__LPX__
	case SIOCLPXNCPCONN:
		/*
		 * This socket wants to take care of the NCP connection
		 * handed to us in arg.
		 */
		rc = -EPERM;
		if (!capable(CAP_NET_ADMIN))
			break;
		rc = get_user(lpx_sk(sk)->lpx_ncp_conn,
			      (const unsigned short __user *)argp);
		break;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	case SIOCGSTAMP:
		rc = -EINVAL;
		if (sk)
			rc = sock_get_timestamp(sk, argp);
		break;

#else
	case SIOCGSTAMP: {
		int ret = -EINVAL;
		if (sk) {
			if (!sk->sk_stamp.tv_sec)
				return -ENOENT;
			ret = -EFAULT;
			if (!copy_to_user((void *)arg, &sk->sk_stamp,
					sizeof(struct timeval)))
				ret = 0;
		}

		return ret;
	}
#endif

	case SIOCGIFDSTADDR:
	case SIOCSIFDSTADDR:
	case SIOCGIFBRDADDR:
	case SIOCSIFBRDADDR:
	case SIOCGIFNETMASK:
	case SIOCSIFNETMASK:
		rc = -EINVAL;
		break;
	default:
		rc = -ENOIOCTLCMD;
		break;
	}

	return rc;
}


#ifdef CONFIG_COMPAT
static int lpx_compat_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	/*
	 * These 4 commands use same structure on 32bit and 64bit.  Rest of LPX
	 * commands is handled by generic ioctl code.  As these commands are
	 * SIOCPROTOPRIVATE..SIOCPROTOPRIVATE+3, they cannot be handled by generic
	 * code.
	 */
	switch (cmd) {
	case SIOCALPXITFCRT:
	case SIOCALPXPRISLT:
	case SIOCLPXCFGDATA:
	case SIOCLPXNCPCONN:
		return lpx_ioctl(sock, cmd, arg);
	default:
		return -ENOIOCTLCMD;
	}
}
#endif


/*
 * Socket family declarations
 */

static struct net_proto_family lpx_family_ops = {
	.family		= PF_LPX,
	.create		= lpx_create,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	.owner		= THIS_MODULE,
#endif
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
static const struct proto_ops SOCKOPS_WRAPPED(lpx_dgram_ops) = {
#else
static struct proto_ops SOCKOPS_WRAPPED(lpx_dgram_ops) = {
#endif

	.family		= PF_LPX,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	.owner		= THIS_MODULE,
#endif
	.release	= lpx_release,
	.bind		= lpx_bind,
	.connect	= lpx_connect,
	.socketpair	= sock_no_socketpair,
	.accept		= sock_no_accept,
	.getname	= lpx_getname,
	.poll		= datagram_poll,
	.ioctl		= lpx_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= lpx_compat_ioctl,
#endif
	.listen		= sock_no_listen,
	.shutdown	= sock_no_shutdown, /* FIXME: support shutdown */
#if !__LPX__
	.setsockopt	= lpx_setsockopt,
	.getsockopt	= lpx_getsockopt,
#else
	.setsockopt	= sock_no_setsockopt,
	.getsockopt	= sock_no_getsockopt,
#endif
	.sendmsg	= lpx_sendmsg,
	.recvmsg	= lpx_recvmsg,
	.mmap		= sock_no_mmap,
	.sendpage	= sock_no_sendpage,
};

#include <linux/smp_lock.h>
SOCKOPS_WRAP(lpx_dgram, PF_LPX);

#if !__LPX__
static struct packet_type lpx_8023_packet_type = {
	.type		= __constant_htons(ETH_P_802_3),
	.func		= lpx_rcv,
};
#endif

static struct packet_type lpx_dix_packet_type = {
	.type		= __constant_htons(ETH_P_LPX),
	.func		= lpx_rcv,
};

static struct notifier_block lpx_dev_notifier = {
	.notifier_call	= lpxitf_device_event,
};

#if !__LPX__
static unsigned char lpx_8022_type = 0xE0;
static unsigned char lpx_snap_id[5] = { 0x0, 0x0, 0x0, 0x88, 0xAD };
#endif

static char lpx_EII_err_msg[] __initdata =
	KERN_CRIT "LPX: Unable to register with Ethernet II\n";
#if !__LPX__
static char lpx_8023_err_msg[] __initdata =
	KERN_CRIT "LPX: Unable to register with 802.3\n";
static char lpx_llc_err_msg[] __initdata =
	KERN_CRIT "LPX: Unable to register with 802.2\n";
static char lpx_snap_err_msg[] __initdata =
	KERN_CRIT "LPX: Unable to register with SNAP\n";
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

static int lpx_pEII_request(struct datalink_proto *dl,
			struct sk_buff *skb, unsigned char *dest_node)
{
	struct net_device *dev = skb->dev;

	skb->protocol = htons(ETH_P_LPX);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	dev_hard_header(skb, dev, ETH_P_LPX, dest_node, NULL, skb->len);
#else
	if (dev->hard_header)
		dev->hard_header(skb, dev, ETH_P_LPX,
				 dest_node, NULL, skb->len);
#endif
	return dev_queue_xmit(skb);
}

struct datalink_proto *lpx_make_EII_client(void)
{
	struct datalink_proto *proto = kmalloc(sizeof(*proto), GFP_ATOMIC);

	if (proto) {
		proto->header_length = 0;
		proto->request = lpx_pEII_request;
	}

	return proto;
}

#else

static void
pEII_datalink_header(struct datalink_proto *dl, 
		struct sk_buff *skb, unsigned char *dest_node)
{
	struct net_device	*dev = skb->dev;

	skb->protocol = htons (ETH_P_LPX);
	if(dev->hard_header)
		dev->hard_header(skb, dev, ETH_P_LPX, dest_node, NULL, skb->len);
}

struct datalink_proto *lpx_make_EII_client(void)
{
	struct datalink_proto *proto = kmalloc(sizeof(*proto), GFP_ATOMIC);

	if (proto) {
		proto->header_length = 0;
		proto->datalink_header = pEII_datalink_header;
	}

	return proto;
}

#endif

void lpx_destroy_EII_client(struct datalink_proto *dl)
{
	kfree(dl);
}

static int __init lpx_init(void)
{
	int rc = 0;
	struct lpx_interface_definition ldef;

    struct net_device *dev;


	NDAS_BUG_ON( typecheck(unsigned long, jiffies) == false );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	rc = proto_register(&lpx_proto, 1);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else
	dbg_call( 1, "sizeof(struct lpx_sock) = %d, sizeof(((struct sock *)NULL)->tp_pinfo)= %d\n", 
				 sizeof(struct lpx_sock), sizeof(((struct sock *)NULL)->tp_pinfo) );
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else

	dbg_call( 1, "sizeof(struct lpx_sock) = %d, sizeof(((struct sock *)NULL)->tp_pinfo = %d, "
					"sizeof(struct tcp_opt) = %d \n", 
					sizeof(struct lpx_sock), sizeof(((struct sock *)NULL)->tp_pinfo), sizeof(struct tcp_opt) );

#endif

	dbg_call( 1, "msecs_to_jiffies(1000) = %lu, HZ = %d\n", msecs_to_jiffies(1000), HZ );
	dbg_call( 1, "sizeof(jiffies) = %lu, sizeof(struct kvec) = %lu\n",  
					(long unsigned int)sizeof(jiffies), (long unsigned int)sizeof(struct iovec) );

	if (rc != 0)
		goto out;

	sock_register(&lpx_family_ops);

	pEII_datalink = lpx_make_EII_client();

	if (pEII_datalink) {

		dev_add_pack(&lpx_dix_packet_type);

	} else {

		printk(lpx_EII_err_msg);
	}

	dbg_call( 1, "pEII_datalink = %p lpx_dix_packet_type.type = %X\n", 
					pEII_datalink, lpx_dix_packet_type.type );

#if !__LPX__
	p8023_datalink = make_8023_client();
	if (p8023_datalink)
		dev_add_pack(&lpx_8023_packet_type);
	else
		printk(lpx_8023_err_msg);

	p8022_datalink = register_8022_client(lpx_8022_type, lpx_rcv);
	if (!p8022_datalink)
		printk(lpx_llc_err_msg);

	pSNAP_datalink = register_snap_client(lpx_snap_id, lpx_rcv);
	if (!pSNAP_datalink)
		printk(lpx_snap_err_msg);
#endif

	register_netdevice_notifier(&lpx_dev_notifier);
	lpx_register_sysctl();
	lpx_proc_init();

    read_lock(&dev_base_lock);

#ifdef for_each_netdev
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
    for_each_netdev(&init_net, dev) {
#else
    for_each_netdev(dev) {
#endif
#else
    for (dev=dev_base; dev; dev=dev->next) { 
#endif

        dbg_call( 1, "ndas: registering network interface %s\n", dev->name );

		memset( &ldef, 0, sizeof(ldef) );

		strcpy( ldef.lpx_device, dev->name );
		
		if (strcmp(ldef.lpx_device, "lo") == 0) {
			
			ldef.lpx_dlink_type = LPX_FRAME_NONE;
		
		} else {
		
			ldef.lpx_dlink_type = LPX_FRAME_ETHERII;
		}

		lpxitf_create( &ldef );
    }

    read_unlock(&dev_base_lock);
	
out:
	return rc;
}

static void __exit lpx_proto_finito(void)
{
	dbg_call( 1, "lpx_proto_finito\n" );

	lpx_proc_exit();
	lpx_unregister_sysctl();

	unregister_netdevice_notifier(&lpx_dev_notifier);

	lpxitf_cleanup();

#if !__LPX__
	if (pSNAP_datalink) {
		unregister_snap_client(pSNAP_datalink);
		pSNAP_datalink = NULL;
	}

	if (p8022_datalink) {
		unregister_8022_client(p8022_datalink);
		p8022_datalink = NULL;
	}

	dev_remove_pack(&lpx_8023_packet_type);
	if (p8023_datalink) {
		destroy_8023_client(p8023_datalink);
		p8023_datalink = NULL;
	}
#endif

	dev_remove_pack(&lpx_dix_packet_type);
	if (pEII_datalink) {
		lpx_destroy_EII_client(pEII_datalink);
		pEII_datalink = NULL;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	proto_unregister(&lpx_proto);
#endif
	sock_unregister(lpx_family_ops.family);

	dbg_call( 1, "lpx_proto_finito return\n" );

}

module_init(lpx_init);
module_exit(lpx_proto_finito);
MODULE_LICENSE("GPL");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
MODULE_ALIAS_NETPROTO(PF_LPX);
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
static const struct proto_ops lpxsmp_ops;
#else
static struct proto_ops lpxsmp_ops;
#endif

static int lpxitf_mtu(struct sock *sk);

static void smp_timer(unsigned long data);

static struct sk_buff *smp_prefare_non_data_skb(struct sock *sk, int type);

void smp_send_data_skbs(struct sock *sk);
static int smp_route_skb(struct sock *sk, struct sk_buff *skb);

static int 	smp_do_rcv(struct sock* sk, struct sk_buff *skb);
static void smp_retransmit_chk(struct sock *sk, unsigned short ackseq);

static inline unsigned long smp_calc_rtt(struct sock *sk)
{
	return smp_sk(sk)->retransmit_interval;

#if 0
	if(smp_sk(sk)->retransmit_count == 0)
		return smp_sk(sk)->interval_time * 10;
	if(smp_sk(sk)->retransmit_count < 10)
		return smp_sk(sk)->interval_time * smp_sk(sk)->retransmit_count * 1000;
	return (MAX_RETRANSMIT_DELAY);
#endif
}

static int smp_send_test(struct sock *sk, struct sk_buff *skb)
{
	u16 	in_flight;

	if (smp_sk(sk)->retransmit_count) {
		
		return 0;
	}

	in_flight = ntohs(lpx_hdr(skb)->u.s.sequence) - smp_sk(sk)->remote_ack_sequence;
	
	if (in_flight >= smp_sk(sk)->max_flights) {
		
		dbg_call( 1, "in_flight = %d\n", in_flight );
		return 0;
	}

	return 1;
}

static void smp_data_ready(struct sock *sk, int len)
{
	smp_sk(sk)->received_data_length += len;

	if (smp_sk(sk)->received_data_length >= smp_sk(sk)->remained_user_data_length) {

		smp_sk(sk)->sk_data_ready(sk, len);
	}
}

static int smp_create(struct socket *sock, int protocol)
{
	struct sock *sk = sock->sk;

 	dbg_call( 2, "called smp_create\n" );

	NDAS_BUG_ON(sk == NULL);

	sk->sk_sndbuf = (sk->sk_sndbuf > (256*1024)) ? sk->sk_sndbuf : (256 * 1024);
	sk->sk_rcvbuf = (sk->sk_rcvbuf > (256*1024)) ? sk->sk_rcvbuf : (256 * 1024);

	sk->sk_backlog_rcv = smp_do_rcv;

	memset(smp_sk(sk), 0, sizeof(struct smp_sock));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	INIT_HLIST_HEAD( &smp_sk(sk)->child_sklist );
#else
	smp_sk(sk)->child_sklist = NULL;
#endif
	spin_lock_init( &smp_sk(sk)->child_sklist_lock );

	smp_sk(sk)->pcb_window = DEFAULT_PCB_WINDOW;

	smp_sk(sk)->sequence			= 0;
	smp_sk(sk)->remote_sequence		= 0;
	smp_sk(sk)->remote_ack_sequence	= 0;
	smp_sk(sk)->server_tag			= 0;
	
	smp_sk(sk)->max_flights = SMP_MAX_FLIGHT / 2;

	setup_timer( &smp_sk(sk)->smp_timer, &smp_timer, (unsigned long)sk ); 

	smp_sk(sk)->smp_timer_interval	= LPX_DEFAULT_SMP_TIMER_INTERVAL;

	smp_sk(sk)->max_stall_interval			= LPX_DEFAULT_MAX_STALL_INTERVAL;
	smp_sk(sk)->max_connect_wait_interval	= LPX_DEFAULT_MAX_CONNECT_WAIT_INTERVAL;

	smp_sk(sk)->retransmit_interval	= LPX_DEFAULT_RETRANSMIT_INTERVAL;

	smp_sk(sk)->alive_interval		= LPX_DEFAULT_ALIVE_INTERVAL;

	smp_sk(sk)->time_wait_interval	= LPX_DEFAULT_TIME_WAIT_INTERVAL;
	smp_sk(sk)->last_ack_interval	= LPX_DEFAULT_LAST_ACK_INTERVAL;

	sock->ops 	= &lpxsmp_ops;

	smp_sk(sk)->sk_data_ready = sk->sk_data_ready;
	sk->sk_data_ready = smp_data_ready;

	return 0;
}

static int smp_release(struct socket *sock)
{
 	struct sock 	*sk = sock->sk;
	int 			err = 0;

	if (sk == NULL) {

		dbg_call( 2, "smp_release sk == NULL\n" );
		return 0;
	}

	dbg_call( 2, "smp_release sk->sk_state = %d\n", sk->sk_state );

	lock_sock(sk);

	sk->sk_shutdown	|= SEND_SHUTDOWN;

	switch (sk->sk_state) {

	case SMP_STATE_CLOSE: 
	case SMP_STATE_LISTEN:
	case SMP_STATE_SYN_SENT: {

		sk->sk_state = SMP_STATE_CLOSE;

		sock_set_flag( sk, SOCK_DESTROY );
		mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->time_wait_interval );
	
		break;
	}

	case SMP_STATE_SYN_RECV:
	case SMP_STATE_ESTABLISHED:
	case SMP_STATE_CLOSE_WAIT: {

		struct sk_buff *skb;

		if (sk->sk_state == SMP_STATE_CLOSE_WAIT) {

			sk->sk_state = SMP_STATE_LAST_ACK;
			smp_sk(sk)->last_ack_timeout = jiffies + smp_sk(sk)->last_ack_interval;

		} else {
		
			sk->sk_state = SMP_STATE_FIN_WAIT1;
		}
			
		smp_sk(sk)->fin_sequence = smp_sk(sk)->sequence;

		skb = smp_prefare_non_data_skb( sk, SMP_TYPE_DISCON );

		if (skb == NULL) {

			err = -ENOMEM;
			sk->sk_err = -err;
			sk->sk_error_report(sk);

			break;
		}

		skb_queue_tail( &sk->sk_write_queue, skb );
		smp_send_data_skbs( sk );

		break;
	}

	default:

		NDAS_BUG_ON(true);

		sk->sk_state = SMP_STATE_CLOSE;

		sock_set_flag( sk, SOCK_DESTROY );
		mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->time_wait_interval );
	}

	if (!sock_flag(sk, SOCK_DEAD)) {

		sk->sk_state_change(sk);
	}

	sock_set_flag( sk, SOCK_DEAD );
	sock->sk = NULL;

	dbg_call( 2, "smp_release sk->refcnt = %d\n", atomic_read(&sk->sk_refcnt) );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

	release_sock(sk);

#else

	del_timer( &smp_sk(sk)->smp_timer );
	lpx_sk(sk)->port = 0;

	release_sock(sk);

	smp_destroy_sock(sk);

#endif

	return err;
}

static int smp_bind(struct socket *sock, struct sockaddr *uaddr, int addr_len)
{
	int			 err;
	struct sock	*sk = sock->sk;

	lock_sock( sk );

	err = lpx_bind( sock, uaddr, addr_len );

	release_sock( sk );
	return err;
}


/* Build a connection to an SMP socket */
static int smp_connect(struct socket *sock, struct sockaddr *uaddr, int addr_len, int flags)
{
 	int 		err = 0;
	struct sock *sk = sock->sk;

	int					size;
	struct sockaddr_lpx	saddr;

	struct sk_buff	*skb;


	dbg_call( 2, "smp_connect sk->sk_state = %d\n", sk->sk_state );

	if (sk == NULL) {

		NDAS_BUG_ON( true );
		return 0;
	}

	lock_sock(sk);

	switch (sk->sk_state) {

	case SMP_STATE_CLOSE: {

		size = sizeof(saddr);
		err  = lpx_getname( sock, (struct sockaddr *)&saddr, &size, 0 );

		if (err != 0) {
 
 			break;
		}
		
		lpx_sk(sk)->port = saddr.slpx_port;

		err = lpx_connect( sock, uaddr, addr_len, flags );

		if (err != 0) {
 
			break;
		}

		sock->state	 = SS_CONNECTING;
		sk->sk_state = SMP_STATE_SYN_SENT;

		skb = smp_prefare_non_data_skb( sk, SMP_TYPE_CONREQ );

		if (skb == NULL) {

			err = -ENOMEM;
			break;
		}

		smp_sk(sk)->max_stall_timeout 	= jiffies + smp_sk(sk)->max_connect_wait_interval;
		smp_sk(sk)->retransmit_timeout 	= jiffies + smp_calc_rtt( sk );

		dbg_call( 1, "smp_connect smp_sk(sk)->max_stall_timeout = %lx, smp_sk(sk)->retransmit_timeout = %lx, jiffies = %lx\n",
					  smp_sk(sk)->max_stall_timeout, smp_sk(sk)->retransmit_timeout, jiffies );
	
		mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->smp_timer_interval );

		skb_queue_tail( &sk->sk_write_queue, skb );

		smp_send_data_skbs(sk);

		release_sock(sk);
	
		err = wait_event_interruptible( *sk->sk_sleep, 
								(sk->sk_state != SMP_STATE_SYN_SENT && sk->sk_state != SMP_STATE_SYN_RECV)	||
							  			sk->sk_err 															||
							  			sock_flag(sk, SOCK_DEAD) );

		lock_sock(sk);

		if (err != 0) {

			break;
		}


		if (sk->sk_err) {
				
			err = -sk->sk_err;
			break;
		}

		if (sock_flag(sk, SOCK_DEAD)) {
			
			err = -EPIPE;
			break;
		}

		if (sk->sk_state != SMP_STATE_ESTABLISHED && sk->sk_state != SMP_STATE_CLOSE_WAIT) {
 
			err = -EPIPE;
			break;
		}

		break;
	}

	default:

		err = -EINVAL;
		break;
	}

	if (err == 0) {

		if (lpx_sk(sk)->intrfc == lpx_lo_interface) {

			lpx_remove_socket(sk);
			lpx_sk(sk)->intrfc = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
			sock_set_flag(sk, SOCK_ZAPPED);
#else
			sk->sk_zapped = 1;
#endif

			size = sizeof(saddr);
			err  = lpx_getname( sock, (struct sockaddr *)&saddr, &size, 0 );

			if (err != 0) {

				NDAS_BUG_ON(true);
 				goto out;
			}

			memcpy(saddr.slpx_node, lpx_sk(sk)->source_node, LPX_NODE_LEN);

			err = lpx_bind2(sock, (struct sockaddr *)&saddr, size, lpx_sk(sk)->source_dev);
			dbg_call( 1, "lpx_bind2 smp_sk(sk)->max_stall_timeout = %lx, jiffies = %lx err = %d\n",
							 smp_sk(sk)->max_stall_timeout, jiffies, err );

			if (err != 0) {
 
				NDAS_BUG_ON(true);
 				goto out;
			}	
		}		
	}

out:

	release_sock(sk);
	return err;
}

/* Move a socket into listening state. */

static int smp_listen(struct socket *sock, int backlog)
{
	int 		err = 0;
	struct sock *sk = sock->sk;

	dbg_call( 2, "smp_listen\n" );

	if (sock->type != SOCK_STREAM) {

		NDAS_BUG_ON( true );
		return -EOPNOTSUPP;
	}

	if ((unsigned) backlog == 0) {

		backlog = 1;
	}

	if ((unsigned) backlog > SOMAXCONN) {

		backlog = SOMAXCONN;
	}

	lock_sock(sk);

	switch (sk->sk_state) {

	case SMP_STATE_CLOSE:

		sk->sk_max_ack_backlog = backlog;
		sk->sk_ack_backlog  = 0;

		sk->sk_state = SMP_STATE_LISTEN;

		sk->sk_socket->flags |= __SO_ACCEPTCON;
	
		break;
	
	default:

		err = -EINVAL;
		break;
	}

	release_sock(sk);
	return err;
}

/* Accept a pending SMP connection */

static int smp_accept(struct socket *sock, struct socket *newsock, int flags)
{
	int 		err = 0;
	struct sock	*hostsk, *newsk;
	bool		sock_locked = false;

	dbg_call( 2, "smp_accept\n" );

	if (sock->sk == NULL || newsock->sk) {

		dbg_call( 1, "sock->sk = %p, newsock->sk = %p\n", sock->sk, newsock->sk );
		NDAS_BUG_ON( true ); 
		return -EINVAL;
	}

	if (flags & O_NONBLOCK) {

		return -EOPNOTSUPP;
	}

	if (sock->state != SS_UNCONNECTED || !(sock->flags & __SO_ACCEPTCON)) {

		return -EINVAL;
	}

	hostsk = sock->sk;
	lock_sock(hostsk);
	sock_locked = true;

	switch (hostsk->sk_state) {

	case SMP_STATE_LISTEN: {

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
		err = lpx_create( hostsk->sk_net, newsock, SOCK_STREAM );
#else
		err = lpx_create( newsock, SOCK_STREAM );
#endif
		if (err) {

			break;
		}

		newsk = newsock->sk;

		lock_sock(newsk);

		lpx_sk(newsk)->intrfc = lpx_sk(hostsk)->intrfc;
		lpx_sk(newsk)->port = lpx_sk(hostsk)->port;
		
		spin_lock_bh( &smp_sk(hostsk)->child_sklist_lock );
		smp_sk(newsk)->parent_sk = hostsk;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		sk_add_node(newsk, &smp_sk(hostsk)->child_sklist);
#else

		sock_hold(newsk);
		newsk->next = NULL;

		if (smp_sk(hostsk)->child_sklist == NULL) {

			smp_sk(hostsk)->child_sklist = newsk;
	
		} else {
	
			struct sock *s = smp_sk(hostsk)->child_sklist;
			while (s->next)
				s = s->next;
			s->next = newsk;
		}

#endif

		spin_unlock_bh( &smp_sk(hostsk)->child_sklist_lock );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
		sock_reset_flag( newsk, SOCK_ZAPPED );
#else
		newsk->sk_zapped = 0;
#endif

		newsk->sk_state = SMP_STATE_LISTEN;

		release_sock(newsk);

		release_sock(hostsk);
		sock_locked = false;

		dbg_call( 0, "wait_event_interruptible before, newsk->err = %d\n", newsk->sk_err );
		err = wait_event_interruptible( *newsk->sk_sleep, 
										(newsk->sk_state != SMP_STATE_LISTEN 	&& 
										 newsk->sk_state != SMP_STATE_SYN_SENT	&& 
										 newsk->sk_state != SMP_STATE_SYN_RECV)		||
							  			newsk->sk_err 								||
							  			sock_flag(newsk, SOCK_DEAD) );


		dbg_call( 0, "wait_event_interruptible after\n" );
		lock_sock(newsk);

		if (err != 0) {

			dbg_call( 0, "interruppted\n" );
			release_sock(newsk);
			break;
		}

		if (newsk->sk_err) {
				
			dbg_call( 0, "newsk->sk_err\n" );
			err = -newsk->sk_err;
			release_sock(newsk);

			break;
		}

		if (sock_flag(newsk, SOCK_DEAD)) {
			
			dbg_call( 0, "SOCK_DEAD\n" );
			err = -EPIPE;
			release_sock(newsk);

			break;
		}

		if (newsk->sk_state != SMP_STATE_ESTABLISHED && newsk->sk_state != SMP_STATE_CLOSE_WAIT) {
 
			dbg_call( 0, "newsk->sk_state != SMP_STATE_ESTABLISHED\n" );
			err = -EPIPE;
			release_sock(newsk);

			break;
		}

		release_sock(newsk);
		break;
	}

	default:

		err = -EINVAL;
	}

	if (sock_locked) {

		release_sock(hostsk);
	}

#if 0
	if (err != 0) {

		if (newsock->sk) {

			newsk = newsock->sk;

			lock_sock(newsk);
		
			newsk->sk_state = SMP_STATE_CLOSE;

			sock_set_flag( newsk, SOCK_DESTROY );
			mod_timer( &smp_sk(newsk)->smp_timer, jiffies + smp_sk(newsk)->time_wait_interval );

			sock_set_flag( newsk, SOCK_DEAD );
			newsock->sk = NULL;

			release_sock(newsk);
		}
	}
#endif

	dbg_call( 0, "accept returned err = %d\n", err );

	return err; 
}

static int smp_getname (struct socket *sock, struct sockaddr *uaddr, int *usockaddr_len, int peer)
{
	int err;

	err = lpx_getname(sock, uaddr, usockaddr_len, peer);

	return err;
}

static int smp_ioctl(struct socket *sock, unsigned int cmd, unsigned long arg)
{
	struct sock *sk = sock->sk;

	switch (cmd) {

		case LPX_IOCTL_DEL_TIMER:

			del_timer(&smp_sk(sk)->smp_timer);
			break;

		default:
			break;
	}

	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static int smp_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t len)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
static int smp_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, int len)
#else
static int smp_sendmsg(struct socket *sock, struct msghdr *msg, int len, struct scm_cookie *scm)
#endif
{
	int 		ret;
	struct sock *sk = sock->sk;

	if (unlikely(sk == NULL)) {

		NDAS_BUG_ON(true);
		return (-EINVAL);
	}

	dbg_call( 3, "called smp_sendmsg, sk->sk_prot = %p, sk->sk_backlog_rcv = %p\n", 
					sk->sk_prot, sk->sk_backlog_rcv );

	lock_sock(sk);

	if (unlikely(sk->sk_shutdown & SEND_SHUTDOWN)) {

		NDAS_BUG_ON(true);
		ret = -EPIPE;
		goto out;
	}

	if (unlikely(sk->sk_err)) {

		dbg_call( 1, "smp_sendmsg, sk->sk_err = %d\n", sk->sk_err );
		ret = -sk->sk_err;
		goto out;
	}

	switch (sk->sk_state) {

	case SMP_STATE_ESTABLISHED:
	case SMP_STATE_CLOSE_WAIT: {

		struct sk_buff_head temp_queue;
		struct sk_buff		*skb;

		int 	remained_user_data_length;
		bool	do_more = true;


		clear_bit( SOCK_ASYNC_NOSPACE, &sk->sk_socket->flags );
		skb_queue_head_init( &temp_queue );	

		ret = 0;
		remained_user_data_length = len;

		while (remained_user_data_length) {

			u16	copied;

			copied = (u16)(lpxitf_mtu(sk) - sizeof(struct lpxhdr));
			copied = (u16)((copied < remained_user_data_length) ? copied : remained_user_data_length);

			while (1) {

				NDAS_BUG_ON( lpx_sk(sk)->intrfc->if_lpx_offset != 14 );

				skb = sock_wmalloc( sk, 
									lpx_sk(sk)->intrfc->if_lpx_offset + sizeof(struct lpxhdr) + copied,
									0, 
									GFP_ATOMIC );

				if (likely(skb != NULL)) {
	
					break;
				}

				NDAS_BUG_ON(true);

				dbg_call( 1, "skb is not allocated\n" );
				
				if (len != remained_user_data_length) {

					do_more = false;						
					break;
				}

				if (msg->msg_flags & MSG_DONTWAIT) {

					do_more = false;						
					break;
				}

				release_sock(sk);

				ret = wait_event_interruptible( *sk->sk_sleep,
						  	((atomic_read(&sk->sk_wmem_alloc) + 
							  lpx_sk(sk)->intrfc->if_lpx_offset + sizeof(struct lpxhdr) + copied) <
							 					sk->sk_sndbuf) ||
												(sk->sk_state != SMP_STATE_ESTABLISHED && 
												 sk->sk_state != SMP_STATE_CLOSE_WAIT)	||
						  						sk->sk_err 								||
						  						sock_flag(sk, SOCK_DEAD) );

				lock_sock(sk);

				if (ret != 0) {

					dbg_call( 1, "smp_sendmsg, ret != 0\n" );
					NDAS_BUG_ON( ret > 0 );
					do_more = false;
					break;
				}

				if ((sk->sk_state != SMP_STATE_ESTABLISHED && sk->sk_state != SMP_STATE_CLOSE_WAIT)	||
					sk->sk_err 																		||
					sock_flag(sk, SOCK_DEAD)) {;

					ret = -EPIPE;
					do_more = false;
					break;
				}	 	 
			}

			if (unlikely(do_more == false)) {

				break;
			}

			skb->sk = sk;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
			skb_reset_mac_header(skb);
			skb_reserve(skb, lpx_sk(sk)->intrfc->if_lpx_offset);
			skb_reset_network_header(skb);
			skb_reset_transport_header(skb);
			skb_put(skb, sizeof(struct lpxhdr));
#else
			skb->mac.raw = skb->data;
			skb_reserve(skb, lpx_sk(sk)->intrfc->if_lpx_offset);
			skb->h.raw = skb->nh.raw = skb_put(skb, sizeof(struct lpxhdr));
#endif

			lpx_hdr(skb)->pu.packet_size = htons(sizeof(struct lpxhdr) + copied);

			if (memcpy_fromiovec(skb_put(skb, copied), msg->msg_iov, copied)) {

				NDAS_BUG_ON(true);			
				ret = -EFAULT;
				sk->sk_err = -ret;
				sk->sk_error_report(sk);

				while ((skb = skb_dequeue(&temp_queue)) != NULL) {

					kfree_skb(skb);
				}

				goto out;
			}

			skb_queue_tail( &temp_queue, skb );
	
			remained_user_data_length -= copied;
		}
		
		NDAS_BUG_ON( ret != 0 );

		ret = len - remained_user_data_length;
	
		if (unlikely(ret == 0)) {

			NDAS_BUG_ON( !skb_queue_empty(&temp_queue) );
			goto out;
		}

		while ((skb = skb_dequeue(&temp_queue)) != NULL) {

			lpx_hdr(skb)->pu.p.lpx_type 	= LPX_TYPE_STREAM;

			lpx_hdr(skb)->destination_port	= lpx_sk(sk)->dest_addr.port;
			lpx_hdr(skb)->source_port		= lpx_sk(sk)->port;

			lpx_hdr(skb)->u.s.lsctl 	= htons(LSCTL_DATA | LSCTL_ACK);
			lpx_hdr(skb)->u.s.sequence	= htons(smp_sk(sk)->sequence);

			smp_sk(sk)->sequence ++;

			lpx_hdr(skb)->u.s.ack_sequence	= htons(smp_sk(sk)->remote_sequence);
			lpx_hdr(skb)->u.s.server_tag	= smp_sk(sk)->server_tag;

			lpx_hdr(skb)->u.s.reserved_s1[0] = 0;
			lpx_hdr(skb)->u.s.reserved_s1[1] = 0;

			lpx_hdr(skb)->u.s.o.option1 = 0;

			skb_queue_tail( &sk->sk_write_queue, skb );
		}
	
		smp_send_data_skbs(sk);

		break;
	}

	default:

		ret = -EPIPE;
		break;
	}
	
out:
	release_sock(sk);

	if (ret < 0) {
	
		dbg_call( 1, "returned smp_sendmsg ret = %d\n", ret );
	}

	return ret;
}

/* Send message/lpx data to user-land */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
static int smp_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, size_t size, int flags)
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
static int smp_recvmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg, int size, int flags)
#else
static int smp_recvmsg(struct socket *sock, struct msghdr *msg, int size, int flags, struct scm_cookie *scm)
#endif
{
	int 		ret;
	struct sock *sk = sock->sk;
	

	if (unlikely(sk == NULL)) {

		NDAS_BUG_ON( true );
		return (-EINVAL);
	}

	dbg_call( 3, "called smp_recvmsg size = %lu\n", (unsigned long)size );

	lock_sock(sk);

	switch (sk->sk_state) {

	case SMP_STATE_ESTABLISHED:
	case SMP_STATE_CLOSE_WAIT: {

		struct sk_buff		*skb;

		bool	do_more = true;

		if (size == 0) {

			ret = 0;
			break;
		}

		ret = 0;
		smp_sk(sk)->remained_user_data_length = size;

		while (smp_sk(sk)->remained_user_data_length) {

			u16	copied;

			while (1) {

				skb = skb_peek(&sk->sk_receive_queue);

				if (skb) {
	
					break;
				}

				if (ret) {

					do_more = false;						
					break;
				}

				if (flags & MSG_DONTWAIT) {

					dbg_call( 1, "smp_recvmsg : MSG_DONTWAIT\n" );

					do_more = false;						
					break;
				}

				release_sock(sk);

				dbg_call( 5, "smp_recvmsg : wait_event_interruptible\n" );

				ret = wait_event_interruptible( *sk->sk_sleep,
						  						(!skb_queue_empty(&sk->sk_receive_queue)) 	||
												(sk->sk_state != SMP_STATE_ESTABLISHED && 
												 sk->sk_state != SMP_STATE_CLOSE_WAIT)		||
						  						sk->sk_err 									||
						  						sock_flag(sk, SOCK_DEAD) );

				dbg_call( 5, "smp_recvmsg : wait_event_interruptible ret = %d\n", ret );

				lock_sock(sk);

				if (unlikely(ret != 0)) {

					dbg_call( 1, "smp_recvmsg : interrupted ret = %x\n", ret );

					NDAS_BUG_ON( ret > 0 );
					do_more = false;
					break;
				}

				if (unlikely((sk->sk_state != SMP_STATE_ESTABLISHED && sk->sk_state != SMP_STATE_CLOSE_WAIT)	||
					sk->sk_err 																		||
					sock_flag(sk, SOCK_DEAD))) {;

					dbg_call( 1, "smp_recvmsg : error\n" );

					ret = -EPIPE;
					do_more = false;
					break;
				}	 	 
			}

			if (unlikely(do_more == false)) {

				dbg_call( 1, "smp_recvmsg : do_more = %d\n", do_more );
				break;
			}

			if (unlikely(ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_DISCONNREQ)) {

				dbg_call( 1, "smp_recvmsg : LSCTL_DISCONNREQ\n" );

				sk->sk_shutdown |= RCV_SHUTDOWN;
				
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
				skb_unlink(skb, &sk->sk_receive_queue);
#else
				skb_unlink(skb);
#endif
				kfree_skb(skb);
				
				ret = ret ? ret : -EPIPE; 
				goto out;
			}

			copied 	= skb->len;

			if (unlikely(copied > smp_sk(sk)->remained_user_data_length)) {

				dbg_call( 6, "It's long skbuff\n" );
				copied = smp_sk(sk)->remained_user_data_length;
			}

			dbg_call( 3, "smp_recvmsg memcpy_toiovec before\n" );

			if (memcpy_toiovec(msg->msg_iov, skb->data, copied)) {
	
				dbg_call( 1, "smp_recvmsg : memcpy_toiovec error\n" );
				NDAS_BUG_ON(true);
		
				ret = -EFAULT;
				sk->sk_err = -ret;
				sk->sk_error_report(sk);

				goto out;
			}

			dbg_call( 3, "smp_recvmsg : memcpy_toiovec success\n" );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
			dbg_call( 4, "skb->len = %d, copied = %d, remained_user_data_length %d "
						   "skb->data = %p, skb->nh.raw = %p\n", 
							skb->len, copied, smp_sk(sk)->remained_user_data_length, skb->data, skb_transport_header(skb) );
#else
			dbg_call( 4, "skb->len = %d, copied = %d, remained_user_data_length %d "
						   "skb->data = %p, skb->nh.raw = %p\n", 
							skb->len, copied, smp_sk(sk)->remained_user_data_length, skb->data, skb->nh.raw );
#endif
 
			skb_pull( skb, copied );

			if (likely(skb->len == 0)) {

				dbg_call( 5, "smp_recvmsg : free skb\n" );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
				skb_unlink(skb, &sk->sk_receive_queue);
#else
				skb_unlink(skb);
#endif
				kfree_skb(skb);
			} 
	
			smp_sk(sk)->remained_user_data_length -= copied;
			smp_sk(sk)->received_data_length -= copied;
		}

		if (ret == 0) {

			ret = size - smp_sk(sk)->remained_user_data_length;
		}
			
		break;
	}

	default:

		ret = -EPIPE;
		break;
	}
	
out:

	if (ret > 0) {

		if (smp_sk(sk)->smp_timer_flag & SMP_TIMER_DELAYED_ACK) {

			struct sk_buff *skb;

			skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

			if (skb) {

				smp_route_skb( sk, skb );
				smp_sk(sk)->smp_timer_flag &= ~SMP_TIMER_DELAYED_ACK;
			}

			mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->smp_timer_interval );
		}
	}

	release_sock(sk);

	dbg_call( 3, "returned smp_recvmsg ret = %d\n", ret );

	return ret;
}

void smp_destroy_sock(struct sock *sk)
{
	struct sk_buff *skb;

	int back_log = 0, receive_queue = 0, write_queue = 0;
	
	del_timer( &smp_sk(sk)->smp_timer );

	dbg_call( 2, "sk->sk_wmem_alloc = %x, sk->sk_rmem_alloc = %x\n", 
					atomic_read(&sk->sk_wmem_alloc), atomic_read(&sk->sk_rmem_alloc) );
	
	dbg_call( 2, "sequence = %x, fin_sequence = %x, remote_sequence = %x, remote_ack_sequence = %x\n", 
					smp_sk(sk)->sequence, smp_sk(sk)->fin_sequence, smp_sk(sk)->remote_sequence,
					 smp_sk(sk)->remote_ack_sequence );
	
	dbg_call( 2, "smp_sk(sk)->remained_user_data_length = %d, reason = %x\n", 
					smp_sk(sk)->remained_user_data_length, smp_sk(sk)->smp_timer_flag );

	sk->sk_data_ready = smp_sk(sk)->sk_data_ready;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

	if (smp_sk(sk)->parent_sk) {

		spin_lock_bh( &smp_sk(sk)->child_sklist_lock );
		sk_del_node_init(sk);
		spin_unlock_bh( &smp_sk(sk)->child_sklist_lock );
	}

#else
	
	if (smp_sk(sk)->parent_sk) {

		struct sock *s;
	
		spin_lock_bh( &smp_sk(sk)->child_sklist_lock );
		
		s = smp_sk(sk)->child_sklist;
	
		if (s == sk) {
	
			smp_sk(sk)->child_sklist = s->next;
			goto out_unlock;
		}

		while (s && s->next) {
	
			if (s->next == sk) {
	
				s->next = sk->next;
				goto out_unlock;
			}
			s = s->next;
		}

out_unlock:
		sock_put(sk);
		spin_unlock_bh(&smp_sk(sk)->child_sklist_lock);
	}

#endif

	while ((skb=sk->sk_backlog.tail) != NULL) {

		sk->sk_backlog.tail = skb->next;
		back_log++;
		kfree_skb(skb);
	}
	
	while ((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {

		receive_queue ++;
		kfree_skb(skb);
	}

	while ((skb = skb_dequeue(&sk->sk_write_queue)) != NULL) {

		write_queue ++;
		kfree_skb(skb);
	}

	dbg_call(2, "back_log = %d, receive_queue = %d, write_queue = %d\n", 
				 	back_log, receive_queue, write_queue );

	lpx_destroy_socket(sk);
}


static int lpxitf_mtu(struct sock *sk)
{
	struct net_device *dev;

	if (lpx_sk(sk)->intrfc == lpx_lo_interface) {

		struct lpx_interface *i;
		int	mtu = sizeof(struct lpxhdr) + 1024 + 256;
		
		spin_lock_bh(&lpx_interfaces_lock);

		list_for_each_entry(i, &lpx_interfaces, node) {
		
			if (i == lpx_lo_interface) {

				continue;
			}

			dbg_call( 5, "lpxitf_send i->if_dev = %p, i->if_dlink_type = %d\n", 
							i->if_dev, i->if_dlink_type );

	    	dev = i->if_dev;

   	 		if (dev == NULL) {

        		continue;
			}

			if (((dev->mtu << 2) >> 2) < mtu) {
	
				mtu = ((dev->mtu << 2) >> 2);
			}
		}

		spin_unlock_bh(&lpx_interfaces_lock);
		
		return mtu;

	} else {

	    dev = lpx_sk(sk)->intrfc->if_dev;

    	if (dev == NULL) {

        	   return -ENXIO;
		}

		return ((dev->mtu >> 2) << 2);
	}
}

static void smp_timer(unsigned long data)
{
	struct sock *sk = (struct sock*)data;

	dbg_call( 5, "called smp_timer, sk = %p, sk->sk_state = %d, sk->sk_dead = %d\n", 
					 sk, sk->sk_state, sock_flag(sk, SOCK_DEAD) );

	bh_lock_sock(sk);

	if (sock_owned_by_user(sk)) {

		dbg_call( 2, "called smp_timer, sk->sk_sock_readers\n" );

		mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->smp_timer_interval );		
		bh_unlock_sock(sk);

		return;
	}

	if (sock_flag(sk, SOCK_DESTROY)) {
		
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		if (sk->sk_user_data != NULL || !hlist_empty(&smp_sk(sk)->child_sklist)) {
#else
		if (sk->sk_user_data != NULL || smp_sk(sk)->child_sklist) {
#endif			
			dbg_call( 2, "called smp_timer, before destroy, sk=%p, jiffies = %lx\n", sk, jiffies );
			goto out;
		
		} else {

			struct sock *parent_sk = smp_sk(sk)->parent_sk;

			//NDAS_BUG_ON( parent_sk != NULL );
			dbg_call( 2, "called smp_timer, before destroy, parent_sk = %p, sk=%p, jiffies = %lx \n", 
							parent_sk, sk, jiffies );

			if (parent_sk) {

				bh_lock_sock(parent_sk);

				if (sock_owned_by_user(parent_sk)) {
				
					bh_unlock_sock(parent_sk);
					goto out;
				}
			}

			bh_unlock_sock(sk);
			smp_destroy_sock(sk);

			if (parent_sk) {

				bh_unlock_sock(parent_sk);
			}			
		}
	
		return;
	}

	if (time_after(jiffies, smp_sk(sk)->max_stall_timeout)) {

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
		if (sock_flag(sk, SOCK_DEAD) || sock_flag(sk, SOCK_ZAPPED)) {
#else
		if (sock_flag(sk, SOCK_DEAD) || sk->sk_zapped) {
#endif

			dbg_call( 1, "smp_timer sock_flag(sk, SOCK_DEAD) sk = %p, timed out smp_sk(sk)->max_stall_timeout = %lx, jiffies = %lx\n",
							 sk, smp_sk(sk)->max_stall_timeout, jiffies );

			sock_set_flag( sk, SOCK_DESTROY );
			mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->time_wait_interval );
		
			bh_unlock_sock(sk);
			return;
		
		} else {

			dbg_call( 4, "smp_timer sk = %p, timed out smp_sk(sk)->max_stall_timeout = %lx, jiffies = %lx\n",
							 sk, smp_sk(sk)->max_stall_timeout, jiffies );

			sk->sk_err = ETIMEDOUT;
			sk->sk_error_report(sk);

			sk->sk_state = SMP_STATE_CLOSE;
			sk->sk_state_change(sk);

			mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->max_stall_interval );
			bh_unlock_sock(sk);

			return;
		}
	}			

	switch (sk->sk_state) {

	case SMP_STATE_TIME_WAIT:

		if (time_after(jiffies, smp_sk(sk)->time_wait_timeout)) {
		
			sock_set_flag( sk, SOCK_DESTROY );
			mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->time_wait_interval );
		
			bh_unlock_sock(sk);
			return;
		}

		goto out;

	case SMP_STATE_LISTEN:

		goto out;

	case SMP_STATE_CLOSING:
	case SMP_STATE_LAST_ACK:

		if (time_after(jiffies, smp_sk(sk)->last_ack_timeout)) {
		
			sock_set_flag( sk, SOCK_DESTROY );
			mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->time_wait_interval );
		
			bh_unlock_sock(sk);
			return;
		}
		
		goto out;

	case SMP_STATE_ESTABLISHED:
	case SMP_STATE_SYN_SENT:
	case SMP_STATE_SYN_RECV:
	case SMP_STATE_FIN_WAIT1:
	case SMP_STATE_FIN_WAIT2:
	case SMP_STATE_CLOSE_WAIT:

		break;

	default:

		NDAS_BUG_ON( false );		
		break;
	}

	if (time_after(jiffies, smp_sk(sk)->alive_timeout)) {

		struct sk_buff *skb;

		skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACKREQ );

		if (skb) {

			smp_route_skb( sk, skb );
			smp_sk(sk)->alive_timeout = jiffies + smp_sk(sk)->alive_interval;
		}	
	}
	
	if (smp_sk(sk)->smp_timer_flag & SMP_TIMER_DELAYED_ACK) {

		struct sk_buff *skb;

		skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

		if (skb) {

			smp_route_skb( sk, skb );
		}

		smp_sk(sk)->smp_timer_flag &= ~SMP_TIMER_DELAYED_ACK;
	}

	smp_send_data_skbs( sk );
		
out:

	mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->smp_timer_interval );
	bh_unlock_sock(sk);

	return;
}

static struct sk_buff *smp_prefare_non_data_skb(struct sock *sk, int type)
{
	int offset;
	int size;

	struct sk_buff	*skb;

	if (lpx_sk(sk)->intrfc == NULL) {

		return NULL;
	}

	offset = lpx_sk(sk)->intrfc->if_lpx_offset;
	size    = offset + sizeof(struct lpxhdr);

	dbg_call( 3, "called smp_prefare_non_data_skb\n" );

	if (offset < 0) {

		return NULL;
	}

	skb = sock_wmalloc( sk, size, 1, GFP_ATOMIC );

	if (!skb) {

		dbg_call( 0, "skb not allocated\n" );
		return NULL;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
	skb_reset_mac_header(skb);
	skb_reserve(skb, offset);
	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);
	skb_put(skb, sizeof(struct lpxhdr));
#else
	skb->mac.raw = skb->data;
	skb_reserve(skb, offset);
	skb->h.raw = skb->nh.raw = skb_put(skb, sizeof(struct lpxhdr));
#endif

	lpx_hdr(skb)->pu.packet_size	= htons(sizeof(struct lpxhdr));
	lpx_hdr(skb)->pu.p.lpx_type 	= LPX_TYPE_STREAM;

	lpx_hdr(skb)->destination_port	= lpx_sk(sk)->dest_addr.port;
	lpx_hdr(skb)->source_port		= lpx_sk(sk)->port;

	switch(type) {

	case SMP_TYPE_CONREQ:	/* Connection Request */
	case SMP_TYPE_DISCON:	/* Inform Disconnection */

		if (type == SMP_TYPE_CONREQ) {

			lpx_hdr(skb)->u.s.lsctl 	= htons(LSCTL_CONNREQ | LSCTL_ACK);
			lpx_hdr(skb)->u.s.sequence	= htons(smp_sk(sk)->sequence);

			smp_sk(sk)->sequence ++;

		} else if (SMP_TYPE_DISCON) {

			lpx_hdr(skb)->u.s.lsctl 	= htons(LSCTL_DISCONNREQ | LSCTL_ACK);
			lpx_hdr(skb)->u.s.sequence	= htons(smp_sk(sk)->fin_sequence);

			smp_sk(sk)->fin_sequence ++;
		}

		break;

	case SMP_TYPE_ACK:	/* ACK */

		lpx_hdr(skb)->u.s.lsctl 	= htons(LSCTL_ACK);
		lpx_hdr(skb)->u.s.sequence	= htons(smp_sk(sk)->sequence);

		break;

	case SMP_TYPE_ACKREQ:	/* ACKREQ */

		lpx_hdr(skb)->u.s.lsctl 	= htons(LSCTL_ACKREQ | LSCTL_ACK);
		lpx_hdr(skb)->u.s.sequence	= htons(smp_sk(sk)->sequence);

		break;


	default:

		NDAS_BUG_ON( true );
		kfree_skb(skb);
		return NULL;
	}

	lpx_hdr(skb)->u.s.ack_sequence	= htons(smp_sk(sk)->remote_sequence);
	lpx_hdr(skb)->u.s.server_tag	= smp_sk(sk)->server_tag;

	lpx_hdr(skb)->u.s.reserved_s1[0] = 0;
	lpx_hdr(skb)->u.s.reserved_s1[1] = 0;

	lpx_hdr(skb)->u.s.o.option1 = 0;

	return skb;
}

void smp_send_data_skbs(struct sock *sk)
{
	if (unlikely(time_after(jiffies, smp_sk(sk)->retransmit_timeout))) {

		dbg_call( 4, "time_after(jiffies, smp_sk(sk)->retransmit_timeout)\n" );

		if (unlikely(!skb_queue_empty(&sk->sk_write_queue))) {

			struct sk_buff	*skb, *skb2;

			skb = skb_peek(&sk->sk_write_queue);

			if ((__s16)(ntohs(lpx_hdr(skb)->u.s.sequence) - smp_sk(sk)->next_transmit_sequence) < 0) {

				dbg_call( 1, "smp_send_data_skbs retransmit smp_sk(sk)->retransmit_count = %d\n",
							  smp_sk(sk)->retransmit_count );

				lpx_hdr(skb)->u.s.ack_sequence = htons(smp_sk(sk)->remote_sequence);
				smp_sk(sk)->retransmit_count ++;
				smp_sk(sk)->retransmit_timeout = jiffies + smp_calc_rtt(sk);
				smp_sk(sk)->next_transmit_sequence = ntohs(lpx_hdr(skb)->u.s.sequence) + 1;
			}

			if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_CONNREQ) {

				lpx_hdr(skb)->source_port = lpx_sk(sk)->port;
			}
			
			skb2 = skb_clone( skb, GFP_ATOMIC );
			smp_route_skb( sk, skb2 );
		}
	} 

	if (likely(smp_sk(sk)->retransmit_count == 0)) {

		struct sk_buff_head temp_queue;
		struct sk_buff		*skb, *skb2;

		skb_queue_head_init( &temp_queue );

		while(!skb_queue_empty(&sk->sk_write_queue)) {

			skb = skb_dequeue(&sk->sk_write_queue);
			skb_queue_head( &temp_queue, skb );

			if ((s16)(ntohs(lpx_hdr(skb)->u.s.sequence) - smp_sk(sk)->next_transmit_sequence) < 0) {

				continue;
			}

			if (unlikely(!smp_send_test(sk, skb))) {

				break;
			}

			lpx_hdr(skb)->u.s.ack_sequence = htons(smp_sk(sk)->remote_sequence);

			smp_sk(sk)->retransmit_timeout = jiffies + smp_calc_rtt(sk);
			smp_sk(sk)->next_transmit_sequence = ntohs(lpx_hdr(skb)->u.s.sequence) + 1;

			skb2 = skb_clone(skb, GFP_ATOMIC);
			smp_route_skb( sk, skb2 );
		}

		while ((skb = skb_dequeue(&temp_queue)) != NULL) {
			
			skb_queue_head( &sk->sk_write_queue, skb );
		}
	}								
}

static int smp_route_skb(struct sock *sk, struct sk_buff *skb)
{
	struct lpx_interface	*interface;

	dbg_call( 3, "called smp_route_skb\n" );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	if (unlikely(sock_flag(sk, SOCK_ZAPPED))) {
#else
	if (unlikely(sk->sk_zapped == 1)) {
#endif

		dbg_call( 0, "lpx_send_skb error\n" );
		kfree_skb(skb);

		return -1;
	}

	interface = lpx_sk(sk)->intrfc;

	if (unlikely(interface == NULL)) {

		NDAS_BUG_ON( true );
		kfree_skb(skb);
		return -1;
	}

	return lpxitf_send( interface, skb, lpx_sk(sk)->dest_addr.node );
}

static void smp_rcv(struct sock *sk, struct sk_buff *skb)
{
	struct sock *child_sk;
	struct sock *receive_sk;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	struct hlist_node	*node;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
	dbg_call( 3, "called smp_rcv, sk = %p, skb->nh.raw = %p, skb->data = %p, skb->len = %d\n", 
					sk, skb_transport_header(skb), skb->data, skb->len );
#else
	dbg_call( 3, "called smp_rcv, sk = %p, skb->nh.raw = %p, skb->data = %p, skb->len = %d\n", 
					sk, skb->nh.raw, skb->data, skb->len );
#endif

#if 0
	{	
		u16 *temp = (u16 *) lpxhdr;
		int i;

		for(i=0;i<40;i++) {
			dbg_call( 2, "%04x ", htons(temp[i]) );
			if((i+1)%8 == 0) dbg_call(2, "\n" );
		}
	}	
#endif

	if ((ntohs(lpx_hdr(skb)->pu.packet_size & ~LPX_TYPE_MASK)) < sizeof(struct lpxhdr)) {

		kfree_skb(skb);
		return;
	}

	if (skb->len < (ntohs(lpx_hdr(skb)->pu.packet_size & ~LPX_TYPE_MASK))) {
		
		kfree_skb(skb);
		return;
	}

	if (skb->len != (ntohs(lpx_hdr(skb)->pu.packet_size & ~LPX_TYPE_MASK))) {

		skb_trim( skb, (ntohs(lpx_hdr(skb)->pu.packet_size & ~LPX_TYPE_MASK)) );
	}

	receive_sk = NULL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	if (hlist_empty(&smp_sk(sk)->child_sklist)) {
#else
	if (smp_sk(sk)->child_sklist == NULL) {
#endif			

		receive_sk = sk;
		skb_pull( skb, sizeof(struct lpxhdr) );

		goto no_child;
	}

	spin_lock_bh( &smp_sk(sk)->child_sklist_lock );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	sk_for_each(child_sk, node, &smp_sk(sk)->child_sklist) {

		dbg_call( 3, "lpx_sk(child_sk)->dest_addr.port = %x, lpx_hdr(skb)->source_port = %x\n", 
						lpx_sk(child_sk)->dest_addr.port, lpx_hdr(skb)->source_port );
		
		if (!memcmp(lpx_sk(child_sk)->dest_addr.node, eth_hdr(skb)->h_source, 6) &&
			lpx_sk(child_sk)->dest_addr.port == lpx_hdr(skb)->source_port) {

			dbg_call( 3, "find called smp_rcv, child_sk = %p\n", child_sk );
			receive_sk = child_sk;
			break;
		}
	}
#else
	for (child_sk = smp_sk(sk)->child_sklist; child_sk;) {

		if (!memcmp(lpx_sk(child_sk)->dest_addr.node, eth_hdr(skb)->h_source, 6) &&
			lpx_sk(child_sk)->dest_addr.port == lpx_hdr(skb)->source_port) {

			dbg_call( 3, "find called smp_rcv, child_sk = %p\n", child_sk );
			receive_sk = child_sk;
			break;
		}

		child_sk = child_sk->next;
	}
#endif			

	spin_unlock_bh( &smp_sk(sk)->child_sklist_lock );

	dbg_call( 4, "child called smp_rcv, child_sk = %p\n", sk );

	if (receive_sk == NULL) {

		spin_lock_bh( &smp_sk(sk)->child_sklist_lock );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		sk_for_each(child_sk, node, &smp_sk(sk)->child_sklist) {

			dbg_call( 1, "called smp_rcv, child_sk = %p\n", child_sk );
			
			if (child_sk->sk_state == SMP_STATE_LISTEN) {

				receive_sk = child_sk;
				
				if (!lpx_sk(receive_sk)->intrfc) {
				
					lpx_sk(receive_sk)->intrfc = lpx_sk(sk)->intrfc;
				}

				break;
			}
		}
#else
		for (child_sk = smp_sk(sk)->child_sklist; child_sk;) {

			dbg_call( 1, "called smp_rcv, child_sk = %p\n", child_sk );
			
			if (child_sk->sk_state == SMP_STATE_LISTEN) {

				receive_sk = child_sk;
				
				if (!lpx_sk(receive_sk)->intrfc) {
				
					lpx_sk(receive_sk)->intrfc = lpx_sk(sk)->intrfc;
				}

				break;
			}

		child_sk = child_sk->next;
	}
#endif			

		spin_unlock_bh( &smp_sk(sk)->child_sklist_lock );
	}
	
	dbg_call( 3, "child called smp_rcv2, child_sk = %p\n", sk );

	if (receive_sk == NULL) {

		kfree_skb(skb);
		return;
	}

	skb_pull( skb, sizeof(struct lpxhdr) );
			
no_child:

	dbg_call( 3, "child called smp_rcv3, child_sk = %p\n", sk );

	if (receive_sk->sk_state == SMP_STATE_CLOSE) {
		
		dbg_call( 1, "SMP_STATE_CLOSED lpxhdr->sequence = %x\n", lpx_hdr(skb)->u.s.sequence );
		kfree_skb(skb);
		return;
	}

	if (receive_sk->sk_state == SMP_STATE_LISTEN) {

		if (smp_sk(receive_sk)->parent_sk == NULL) {
		
			dbg_call( 1, "SMP_STATE_LISTEN lpxhdr->sequence = %x\n", lpx_hdr(skb)->u.s.sequence );
			kfree_skb(skb);
			return;
		}
	}

	bh_lock_sock( receive_sk );

	if (!sock_owned_by_user(receive_sk)) {

		smp_do_rcv( receive_sk, skb );

	} else {

		sk_add_backlog( receive_sk, skb );
	}

	bh_unlock_sock( receive_sk );

	return;
}

static int smp_state_do_receive_when_listen(struct sock* sk, struct sk_buff *skb)
{
	struct sk_buff *reply_skb;

	if (sock_flag(sk, SOCK_DEAD)) {

		goto out;
	}

	if (smp_sk(sk)->parent_sk == NULL) {

		NDAS_BUG_ON( false );		
		goto out;
	}

	switch (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_MASK) {

	case LSCTL_CONNREQ:
	case LSCTL_CONNREQ | LSCTL_ACK: {

		dbg_call( 2, "SMP_STATE_LISTEN\n" );

		if (lpx_hdr(skb)->u.r.option & LPX_OPTION_NONE_DELAYED_ACK) {

			smp_sk(sk)->destination_non_delayed_ack = true;
			smp_sk(sk)->pcb_window = DEFAULT_PCB_WINDOW;
		}

		memcpy(lpx_sk(sk)->dest_addr.node, eth_hdr(skb)->h_source, 6);
		lpx_sk(sk)->dest_addr.port = lpx_hdr(skb)->source_port;

		smp_sk(sk)->remote_sequence ++;
		smp_sk(sk)->server_tag = lpx_hdr(skb)->u.s.server_tag;

		sk->sk_socket->state = SS_CONNECTING;
		sk->sk_state = SMP_STATE_SYN_RECV;
		sk->sk_state_change(sk);

		smp_sk(sk)->max_stall_timeout 	= jiffies + smp_sk(sk)->max_connect_wait_interval;
		mod_timer( &smp_sk(sk)->smp_timer, jiffies + smp_sk(sk)->smp_timer_interval );

		reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_CONREQ );

		if (reply_skb) {

			struct sk_buff *skb2;

			skb2 = skb_clone( reply_skb, GFP_ATOMIC );

			skb_queue_tail( &sk->sk_write_queue, reply_skb );
			smp_sk(sk)->next_transmit_sequence = ntohs(lpx_hdr(reply_skb)->u.s.sequence);
	
			smp_route_skb( sk, skb2 );
		}

		break;
	}

	default:

		break;
	}

out:
	if (skb) {

		kfree_skb(skb);
	}

	return 0;
}

static int smp_state_do_receive_when_syn_sent(struct sock* sk, struct sk_buff *skb)
{
	struct sk_buff *reply_skb;

	if (sock_flag(sk, SOCK_DEAD)) {

		goto out;
	}

	if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACK) {

		if (((s16)(ntohs(lpx_hdr(skb)->u.s.ack_sequence) - smp_sk(sk)->remote_ack_sequence)) > 0) {
	
			smp_sk(sk)->remote_ack_sequence = ntohs(lpx_hdr(skb)->u.s.ack_sequence);
			smp_retransmit_chk(sk, smp_sk(sk)->remote_ack_sequence);
		}
	}

	if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_CONNREQ) {

		dbg_call( 2, "SMP_STATE_SYN_SENT\n" );

		smp_sk(sk)->remote_sequence ++;
		smp_sk(sk)->server_tag = lpx_hdr(skb)->u.s.server_tag;

		sk->sk_state = SMP_STATE_SYN_RECV;
		sk->sk_state_change(sk);
	
		if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACK) {

			sk->sk_socket->state = SS_CONNECTED;
			sk->sk_state = SMP_STATE_ESTABLISHED;
			sk->sk_state_change(sk);
		}

		memcpy( lpx_sk(sk)->source_node, eth_hdr(skb)->h_dest, LPX_NODE_LEN );
		lpx_sk(sk)->source_dev = skb->dev;

		dbg_call( 2, "SMP_STATE_SYN_SENT skb->dev = %p\n", skb->dev );
		
		reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

		if (reply_skb) {

			smp_route_skb( sk, reply_skb );
		}
	}

out:
	if (skb) {

		kfree_skb(skb);
	}

	return 0;
}

static int smp_state_do_receive_when_syn_recv(struct sock* sk, struct sk_buff *skb)
{
	if (sock_flag(sk, SOCK_DEAD)) {

		goto out;
	}

	if (!(ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACK)) {

		goto out;
	}

	dbg_call( 2, "SMP_STATE_SYN_RECV\n" );

	if (ntohs(lpx_hdr(skb)->u.s.ack_sequence) < 1) {

		goto out;
	}

	if (lpx_hdr(skb)->u.s.server_tag != smp_sk(sk)->server_tag) {

		goto out;
	}

	if (((s16)(ntohs(lpx_hdr(skb)->u.s.ack_sequence) - smp_sk(sk)->remote_ack_sequence)) > 0) {

		smp_sk(sk)->remote_ack_sequence = ntohs(lpx_hdr(skb)->u.s.ack_sequence);
		smp_retransmit_chk(sk, smp_sk(sk)->remote_ack_sequence);
	}

	sk->sk_socket->state = SS_CONNECTED;
	sk->sk_state = SMP_STATE_ESTABLISHED;
	sk->sk_state_change(sk);

out:
	if (skb) {

		kfree_skb(skb);
	}

	return 0;
}

static int smp_state_do_receive_when_established(struct sock* sk, struct sk_buff *skb)
{
	bool 			send_ack = false;
	struct sk_buff 	*reply_skb;
	int 			err;

	if (sock_flag(sk, SOCK_DEAD)) {

		goto out;
	}

	if (unlikely(lpx_hdr(skb)->u.s.server_tag != smp_sk(sk)->server_tag)) {

		NDAS_BUG_ON(true);
		goto out;
	}

	if (likely(ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACK)) {

		if (((s16)(ntohs(lpx_hdr(skb)->u.s.ack_sequence) - smp_sk(sk)->remote_ack_sequence)) > 0) {

			smp_sk(sk)->remote_ack_sequence = ntohs(lpx_hdr(skb)->u.s.ack_sequence); 
			smp_retransmit_chk(sk, smp_sk(sk)->remote_ack_sequence);
		}
	}

	switch (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_MASK) {

	case LSCTL_ACKREQ:
	case LSCTL_ACKREQ | LSCTL_ACK: {

		reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

		if (reply_skb) {

			smp_route_skb( sk, reply_skb );
		}

		break;
	}

	case LSCTL_DATA:
	case LSCTL_DATA | LSCTL_ACK: {

		if (unlikely(ntohs(lpx_hdr(skb)->pu.packet_size & ~LPX_TYPE_MASK) <= sizeof(struct lpxhdr))) {

			NDAS_BUG_ON(true);
			dbg_call( 1, "no data length !!!!!!!!!!!!!!!!!!\n" );
		}

		if (unlikely(((s16)(ntohs(lpx_hdr(skb)->u.s.sequence) - smp_sk(sk)->remote_sequence)) > 0)) {

			dbg_call( 1, "packet dropped\n" );

			// insert receive_reorder_queue;

			reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

			if (reply_skb) {

				smp_route_skb( sk, reply_skb );
			}

			break;
		}

		if (unlikely(((s16)(ntohs(lpx_hdr(skb)->u.s.sequence) - smp_sk(sk)->remote_sequence)) < 0)) {

			dbg_call( 2, "packet duplicated\n" );

			reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

			if (reply_skb) {

				smp_route_skb( sk, reply_skb );
			}

			break;
		}
	
		NDAS_BUG_ON( ((s16)(ntohs(lpx_hdr(skb)->u.s.sequence) != smp_sk(sk)->remote_sequence)) );

		smp_sk(sk)->remote_sequence ++;

		if (!(lpx_hdr(skb)->u.r.option & LPX_OPTION_PACKETS_CONTINUE_BIT)) {

			smp_sk(sk)->smp_timer_flag &= ~SMP_TIMER_DELAYED_ACK;
			send_ack = true;
			
		} else {

			if (smp_sk(sk)->remote_sequence % smp_sk(sk)->pcb_window == 0) {

				smp_sk(sk)->smp_timer_flag &= ~SMP_TIMER_DELAYED_ACK;
				send_ack = true;
			
			} else {	
				
				smp_sk(sk)->smp_timer_flag |= SMP_TIMER_DELAYED_ACK;
			}	
		}
		
		err = sock_queue_rcv_skb(sk, skb);

		if (unlikely(err != 0)) {

			NDAS_BUG_ON(true);
			printk( "atomic_read(&sk->sk_rmem_alloc) = %d, skb->truesize = %d, sk->sk_rcvbuf = %d\n",
					 atomic_read(&sk->sk_rmem_alloc), skb->truesize, sk->sk_rcvbuf );

			printk( "RECV:receive data error err=%d\n", err );

			sk->sk_err = err;
			sk->sk_error_report(sk);
			break;
		}

		skb = NULL;

		smp_sk(sk)->smp_timer_flag |= SMP_TIMER_DELAYED_ACK;
		send_ack = false;

		if (unlikely(send_ack)) {

			reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

			if (reply_skb) {

				smp_route_skb( sk, reply_skb );
			}
		}

		mod_timer( &smp_sk(sk)->smp_timer, jiffies + msecs_to_jiffies(50) );

		break;	
	}

	case LSCTL_DISCONNREQ:
	case LSCTL_DISCONNREQ | LSCTL_ACK: {


		if (((s16)(ntohs(lpx_hdr(skb)->u.s.sequence) - smp_sk(sk)->remote_sequence)) > 0) {

			reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

			if (reply_skb) {

				smp_route_skb( sk, reply_skb );
			}

			break;
		}

		if (((s16)(ntohs(lpx_hdr(skb)->u.s.sequence) - smp_sk(sk)->remote_sequence)) < 0) {

			reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

			if (reply_skb) {

				smp_route_skb( sk, reply_skb );
			}

			break;
		}
	
		NDAS_BUG_ON( ((s16)(ntohs(lpx_hdr(skb)->u.s.sequence) != smp_sk(sk)->remote_sequence)) );

#if 0
		while ((skb2 = skb_dequeue(&sk->sk_write_queue)) != NULL) {

			kfree_skb(skb2);
		}

		smp_sk(sk)->fin_sequence = smp_sk(sk)->remote_ack_sequence;
		smp_sk(sk)->sequence = smp_sk(sk)->remote_ack_sequence;
		smp_sk(sk)->next_transmit_sequence = smp_sk(sk)->remote_ack_sequence;
#endif
				
		smp_sk(sk)->remote_sequence ++;

		sk->sk_state = SMP_STATE_CLOSE_WAIT;
		sk->sk_state_change(sk);

		err = sock_queue_rcv_skb(sk, skb);

		if (err != 0) {

			printk("RECV:receive data error err = %d\n", err);
			sk->sk_err = err;
			sk->sk_error_report(sk);
			break;
		}

		skb = NULL;

		reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

		if (reply_skb) {

			smp_route_skb( sk, reply_skb );
		}
		
		break;	
	}

	default:

		break;
	}

out:
	if (skb) {

		kfree_skb(skb);
	}

	return 0;
}

static int smp_state_do_receive_when_fin_wait1(struct sock* sk, struct sk_buff *skb)
{
	struct sk_buff 	*reply_skb;

	NDAS_BUG_ON( !sock_flag(sk, SOCK_DEAD) );

	if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACK) {

		if (((s16)(ntohs(lpx_hdr(skb)->u.s.ack_sequence) - smp_sk(sk)->remote_ack_sequence)) > 0) {

			smp_sk(sk)->remote_ack_sequence = ntohs(lpx_hdr(skb)->u.s.ack_sequence); 
			smp_retransmit_chk(sk, smp_sk(sk)->remote_ack_sequence);
		}

		if (smp_sk(sk)->remote_ack_sequence == smp_sk(sk)->fin_sequence) {

			sk->sk_state = SMP_STATE_FIN_WAIT2;
		}			
	}

	if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_DISCONNREQ) {

		dbg_call( 2, "SMP_STATE_FIN_WAI1\n" );

		smp_sk(sk)->remote_sequence ++;

		smp_sk(sk)->last_ack_timeout = jiffies + smp_sk(sk)->last_ack_interval;

		sk->sk_state = SMP_STATE_CLOSING;
	
		if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACK) {

			if (ntohs(lpx_hdr(skb)->u.s.ack_sequence) == smp_sk(sk)->fin_sequence) {
			
				smp_sk(sk)->time_wait_timeout = jiffies + smp_sk(sk)->time_wait_interval;
		
				sk->sk_state = SMP_STATE_FIN_WAIT2;
			}
		}

		reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

		if (reply_skb) {

			smp_route_skb( sk, reply_skb );
		}
	
	} else if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACKREQ) {

		reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

		if (reply_skb) {

			smp_route_skb( sk, reply_skb );
		}
	
	} else if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_DATA) {

		if (ntohs(lpx_hdr(skb)->u.s.sequence) == smp_sk(sk)->remote_sequence) {

			//int	err;

			smp_sk(sk)->remote_sequence ++;

#if 0		
			err = sock_queue_rcv_skb(sk, skb);

			if (err != 0) {

				printk("RECV:receive data error\n");
				sk->sk_err = err;
				sk->sk_error_report(sk);
			}

			skb = NULL;
#endif
			reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

			if (reply_skb) {

				smp_route_skb( sk, reply_skb );
			}
		}
	}

	if (skb) {

		kfree_skb(skb);
	}
	return 0;
}

static int smp_state_do_receive_when_fin_wait2(struct sock* sk, struct sk_buff *skb)
{
	struct sk_buff 	*reply_skb;

	NDAS_BUG_ON( !sock_flag(sk, SOCK_DEAD) );

	if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_DISCONNREQ) {

		dbg_call( 2, "SMP_STATE_FIN_WAI2\n" );

		smp_sk(sk)->remote_sequence ++;

		smp_sk(sk)->time_wait_timeout = jiffies + smp_sk(sk)->time_wait_interval;

		sk->sk_state = SMP_STATE_TIME_WAIT;
	
		reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

		if (reply_skb) {

			smp_route_skb( sk, reply_skb );
		}
	
	} else if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACKREQ) {

		reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

		if (reply_skb) {

			smp_route_skb( sk, reply_skb );
		}
	
	} else if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_DATA) {

		if (ntohs(lpx_hdr(skb)->u.s.sequence) == smp_sk(sk)->remote_sequence) {

			//int	err;
			
			smp_sk(sk)->remote_sequence ++;

#if 0		
			err = sock_queue_rcv_skb(sk, skb);

			if (err != 0) {

				printk("RECV:receive data error\n");
				sk->sk_err = err;
				sk->sk_error_report(sk);
			}

			skb = NULL;
#endif

			reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

			if (reply_skb) {

				smp_route_skb( sk, reply_skb );
			}
		}
	}

	if (skb) {

		kfree_skb(skb);
	}

	return 0;
}

static int smp_state_do_receive_when_closing(struct sock* sk, struct sk_buff *skb)
{
	struct sk_buff *reply_skb;

	if (sock_flag(sk, SOCK_DEAD)) {

		goto out;
	}

	if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACK) {

		if (((s16)(ntohs(lpx_hdr(skb)->u.s.ack_sequence) - smp_sk(sk)->remote_ack_sequence)) > 0) {

			smp_sk(sk)->remote_ack_sequence = ntohs(lpx_hdr(skb)->u.s.ack_sequence); 
			smp_retransmit_chk(sk, smp_sk(sk)->remote_ack_sequence);
		}

		if (ntohs(lpx_hdr(skb)->u.s.ack_sequence) == smp_sk(sk)->fin_sequence) {
			
			sk->sk_state = SMP_STATE_TIME_WAIT;
			sk->sk_state_change(sk);
		}	
	}

	if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACKREQ) {

		reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

		if (reply_skb) {

			smp_route_skb( sk, reply_skb );
		}
	}

out:
	if (skb) {

		kfree_skb(skb);
	}
	return 0;
}

static int smp_state_do_receive_when_close_wait(struct sock* sk, struct sk_buff *skb)
{
	if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACK) {

		if (((s16)(ntohs(lpx_hdr(skb)->u.s.ack_sequence) - smp_sk(sk)->remote_ack_sequence)) > 0) {

			smp_sk(sk)->remote_ack_sequence = ntohs(lpx_hdr(skb)->u.s.ack_sequence); 
			smp_retransmit_chk(sk, smp_sk(sk)->remote_ack_sequence);
		}
	}

	if (skb) {

		kfree_skb(skb);
	}
	return 0;
}

static int smp_state_do_receive_when_last_ack(struct sock* sk, struct sk_buff *skb)
{
	if (ntohs(lpx_hdr(skb)->u.s.lsctl) == LSCTL_ACK) {

		dbg_call( 2, "SMP_STATE_LAST_ACK\n" );

		if (((s16)(ntohs(lpx_hdr(skb)->u.s.ack_sequence) - smp_sk(sk)->remote_ack_sequence)) > 0) {

			smp_sk(sk)->remote_ack_sequence = ntohs(lpx_hdr(skb)->u.s.ack_sequence); 
			smp_retransmit_chk(sk, smp_sk(sk)->remote_ack_sequence);
		}

		if (ntohs(lpx_hdr(skb)->u.s.ack_sequence) == smp_sk(sk)->fin_sequence) {
			
			smp_sk(sk)->time_wait_timeout = jiffies + smp_sk(sk)->time_wait_interval;
			sk->sk_state = SMP_STATE_TIME_WAIT;
		}	
	}

	if (skb) {

		kfree_skb(skb);
	}
	return 0;
}

static int smp_state_do_receive_when_time_wait(struct sock* sk, struct sk_buff *skb)
{
	struct sk_buff *reply_skb;

	if (ntohs(lpx_hdr(skb)->u.s.lsctl) == (LSCTL_DISCONNREQ | LSCTL_ACK)) {

		reply_skb = smp_prefare_non_data_skb( sk, SMP_TYPE_ACK );

		if (reply_skb) {

			smp_route_skb( sk, reply_skb );
		}
	}

	if (skb) {

		kfree_skb(skb);
	}
	return 0;
}

#define MAX_ALLOWED_SEQ	1024

/* SMP lpx receive engine */
static int smp_do_rcv(struct sock* sk, struct sk_buff *skb)
{
	dbg_call( 3, "called smp_do_rcv, port = %x, sk->sk_state = %x\n", lpx_sk(sk)->port, sk->sk_state );

	//if (unlikely(sock_owned_by_user(sk))) {

		//NDAS_BUG_ON(true);
	//}

	lpx_hdr(skb)->u.s.lsctl = htons(ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_MASK);

	if (unlikely(((s16)(ntohs(lpx_hdr(skb)->u.s.sequence) - smp_sk(sk)->remote_sequence)) > MAX_ALLOWED_SEQ)) {

		NDAS_BUG_ON(true);
		kfree_skb(skb);
		return 0;
	}

	if (ntohs(lpx_hdr(skb)->u.s.lsctl) & LSCTL_ACK) {

		if (unlikely(((s16)(smp_sk(sk)->remote_ack_sequence - ntohs(lpx_hdr(skb)->u.s.ack_sequence))) > MAX_ALLOWED_SEQ)) {

			NDAS_BUG_ON(true);
			kfree_skb(skb);
			return 0;
		}
	}

	if (likely(smp_sk(sk)->retransmit_count == 0)) {

		smp_sk(sk)->max_stall_timeout = jiffies + smp_sk(sk)->max_stall_interval;
	}	

	smp_sk(sk)->alive_timeout = jiffies + smp_sk(sk)->alive_interval;

	switch(sk->sk_state) {

	case SMP_STATE_LISTEN:

		smp_state_do_receive_when_listen( sk, skb );
		break;

	case SMP_STATE_SYN_SENT:

		smp_state_do_receive_when_syn_sent( sk, skb );
		break;

	case SMP_STATE_SYN_RECV:

		smp_state_do_receive_when_syn_recv( sk, skb );
		break;

	case SMP_STATE_ESTABLISHED:

		smp_state_do_receive_when_established( sk, skb );
		break;

	case SMP_STATE_FIN_WAIT1:

		smp_state_do_receive_when_fin_wait1( sk, skb );
		break;

	case SMP_STATE_FIN_WAIT2:

		smp_state_do_receive_when_fin_wait2( sk, skb );
		break;

	case SMP_STATE_CLOSING:
		
		smp_state_do_receive_when_closing( sk, skb );
		break;

	case SMP_STATE_CLOSE_WAIT:
		
		smp_state_do_receive_when_close_wait( sk, skb );
		break;

	case SMP_STATE_LAST_ACK:
		
		smp_state_do_receive_when_last_ack( sk, skb );
		break;

	case SMP_STATE_TIME_WAIT:
		
		smp_state_do_receive_when_time_wait( sk, skb );
		break;

	case SMP_STATE_CLOSE:
		
		kfree_skb(skb);
		break;

	default:

		NDAS_BUG_ON(true);
		kfree_skb(skb);
		break;

	}

	dbg_call( 3, "smp_do_rcv end\n" );
	return 0;
}

/* Check lpx for retransmission, ConReqAck aware */

static void smp_retransmit_chk(struct sock *sk, unsigned short ackseq)
{
	struct sk_buff 	*skb;
	bool			freed = false;

	while ((skb = skb_peek(&sk->sk_write_queue)) != NULL) {

		if (((s16)(ntohs(lpx_hdr(skb)->u.s.sequence) - ackseq)) >= 0) {
		
			break;
		}
		
		skb = skb_dequeue(&sk->sk_write_queue);
		
		if (skb == NULL) {

			dbg_call( 1, "skb == NULL\n" );
			NDAS_BUG_ON( true );
		}
			
		kfree_skb( skb );
		freed = true;
	}

	if (freed == false) {

		dbg_call( 1, "freed == false\n" );
		NDAS_BUG_ON( true );
		return;
	}

	smp_sk(sk)->retransmit_count = 0;

	smp_sk(sk)->retransmit_timeout = jiffies + smp_sk(sk)->retransmit_interval;
	smp_sk(sk)->max_stall_timeout = jiffies + smp_sk(sk)->max_stall_interval;

	sk->sk_write_space(sk);

	return;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
static const struct proto_ops SOCKOPS_WRAPPED(lpxsmp_ops) = {
#else
static struct proto_ops SOCKOPS_WRAPPED(lpxsmp_ops) = {
#endif

	.family		= PF_LPX,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	.owner		= THIS_MODULE,
#endif
	.release	= smp_release,
	.bind		= smp_bind,
	.connect	= smp_connect,
	.socketpair	= sock_no_socketpair,
	.accept		= smp_accept,
	.getname	= smp_getname,
	.poll		= datagram_poll,
	.ioctl		= smp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= lpx_compat_ioctl,
#endif
	.listen		= smp_listen,
	.shutdown	= sock_no_shutdown, /* FIXME: support shutdown */
#if !__LPX__
	.setsockopt	= lpx_setsockopt,
	.getsockopt	= lpx_getsockopt,
#else
	.setsockopt	= sock_no_setsockopt,
	.getsockopt	= sock_no_getsockopt,
#endif
	.sendmsg	= smp_sendmsg,
	.recvmsg	= smp_recvmsg,
	.mmap		= sock_no_mmap,
	.sendpage	= sock_no_sendpage,
};


#include <linux/smp_lock.h>
SOCKOPS_WRAP(lpxsmp, PF_LPX);
