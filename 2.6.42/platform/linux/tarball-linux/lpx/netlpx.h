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

#ifndef _NET_INET_LPX_H_
#define _NET_INET_LPX_H_

#include <linux/version.h>
#include <linux/module.h> 

#define __LPX__		1

#include <linux/version.h>
#include <linux/netdevice.h>
#include <net/datalink.h>
//#include <linux/lpx.h>
#include <linux/list.h>

#include "lpx_compat.h"

struct lpx_address {
	__be32  net;
	__u8    node[LPX_NODE_LEN]; 
	__be16 	port;
};

#define lpx_broadcast_node	"\377\377\377\377\377\377"
#define lpx_this_node           "\0\0\0\0\0\0"

#if !__LPX__

#define LPX_MAX_PPROP_HOPS 8

struct lpxhdr {
	__be16			lpx_checksum __attribute__ ((packed));
#define LPX_NO_CHECKSUM	__constant_htons(0xFFFF)
	__be16			lpx_pktsize __attribute__ ((packed));
	__u8			lpx_tctrl;
	__u8			lpx_type;
#define LPX_TYPE_UNKNOWN	0x00
#define LPX_TYPE_RIP		0x01	/* may also be 0 */
#define LPX_TYPE_SAP		0x04	/* may also be 0 */
#define LPX_TYPE_SPX		0x05	/* SPX protocol */
#define LPX_TYPE_NCP		0x11	/* $lots for docs on this (SPIT) */
#define LPX_TYPE_PPROP		0x14	/* complicated flood fill brdcast */
	struct lpx_address	lpx_dest __attribute__ ((packed));
	struct lpx_address	lpx_source __attribute__ ((packed));
};

#else

/* 16 bit align  total  16 bytes */

struct	lpxhdr {

	union {

#if defined(__LITTLE_ENDIAN_BITFIELD)

		struct {

			__u8	packet_size_msb:6;
			__u8	lpx_type:2;
#define LPX_TYPE_RAW		0x0
#define LPX_TYPE_DATAGRAM	0x2
#define LPX_TYPE_STREAM		0x3	

			__u8	packet_size_lsb;

		} __attribute__((packed)) p;

		__u16	packet_size;
#define LPX_TYPE_MASK		(unsigned short)0x00C0	

#elif defined(__BIG_ENDIAN_BITFIELD)

		struct {

			__u8	lpx_type:2;
#define LPX_TYPE_RAW		0x0
#define LPX_TYPE_DATAGRAM	0x2
#define LPX_TYPE_STREAM		0x3	

			__u8	packet_size_msb:6;
			__u8	packet_size_lsb;

		} __attribute__((packed)) p;

		__u16	packet_size;
#define LPX_TYPE_MASK		(unsigned short)0xC000	

#else
#error "Bitfield endian is not specified"
#endif 

	} __attribute__((packed)) pu;

	__u16	destination_port;
	__u16	source_port;

	union {

		struct {

			__u8 	reserved_r[9];
			__u8	option;
#define LPX_OPTION_PACKETS_CONTINUE_BIT	((__u8)0x80)
#define LPX_OPTION_NONE_DELAYED_ACK		((__u8)0x40)

		} __attribute__((packed)) r;
			
		struct {
/*
#if defined(__LITTLE_ENDIAN_BITFIELD)
			__u16	reserved_d1:8;
			__u16	fragment_last:1;
			__u16	reserved_d2:7;			
#elif defined(__BIG_ENDIAN_BITFIELD)
			__u16	reserved_u1:8;
			__u16	reserved_u2:7;			
			__u16	fragment_last:1;
#endif
*/
			__u16	message_id;
			__u16	message_length;
			__u16	fragment_id;
			__u16	fragment_length;
			__u16	reserved_d3;
		
		} __attribute__((packed)) d;
		
		struct {

			__u16	lsctl;

/* Lpx Stream Control bits */
#define LSCTL_CONNREQ		0x0001
#define LSCTL_DATA			0x0002
#define LSCTL_DISCONNREQ	0x0004
#define LSCTL_ACKREQ		0x0008
#define LSCTL_ACK			0x1000
#define LSCTL_MASK			((__u16)(LSCTL_CONNREQ | LSCTL_DATA | LSCTL_DISCONNREQ | LSCTL_ACKREQ | LSCTL_ACK))

			__u16	sequence;
			__u16	ack_sequence;
//			__u16	window_size; /* not implemented */
//			__u16	reserved_s1;
			__u8	server_tag;
			__u8	reserved_s1[2];

			union {

				__u8	option1;
			
				struct {

#if defined(__LITTLE_ENDIAN_BITFIELD)

					__u8	reserved2 : 6;
					__u8	none_delayed_ack : 1;
					__u8	pcb	: 1;

#elif defined(__BIG_ENDIAN_BITFIELD)

					__u8	pcb	: 1;
					__u8	none_delayed_ack : 1;
					__u8	reserved2 : 6;

#endif
				}__attribute__((packed)) p;

			} __attribute__((packed)) o;

		} __attribute__((packed)) s;

	} __attribute__((packed))u;

} __attribute__ ((packed));
	
#define	LPX_PKT_LEN	(sizeof(struct lpxhdr))

#endif

static __inline__ struct lpxhdr *lpx_hdr(struct sk_buff *skb)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
	return (struct lpxhdr *)skb_transport_header(skb);
#else
	return (struct lpxhdr *)skb->h.raw;
#endif
}

struct lpx_interface {
	/* LPX address */
#if !__LPX__
	__be32			if_netnum;
#endif
	unsigned char	if_node[LPX_NODE_LEN];
	atomic_t		refcnt;

	/* physical device info */
	struct net_device		*if_dev;
	struct datalink_proto	*if_dlink;
	__be16					if_dlink_type;

	/* socket support */
	unsigned short		if_sknum;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	struct hlist_head	if_sklist;
#else
	struct sock			*if_sklist;
#endif

	spinlock_t			if_sklist_lock;

	/* administrative overhead */
	int				if_lpx_offset;
	unsigned char	if_internal;
	unsigned char	if_primary;
	
	struct list_head	node; /* node in lpx_interfaces list */
};

#if !__LPX__

struct lpx_route {
	__be32			ir_net;
	struct lpx_interface	*ir_intrfc;
	unsigned char		ir_routed;
	unsigned char		ir_router_node[LPX_NODE_LEN];
	struct list_head	node; /* node in lpx_routes list */
	atomic_t		refcnt;
};

#endif

#ifdef __KERNEL__

#if !__LPX__

struct lpx_cb {
	u8	lpx_tctrl;
	__be32	lpx_dest_net;
	__be32	lpx_source_net;
	struct {
		__be32 netnum;
		int index;
	} last_hop;
};

#endif

#include <net/sock.h>

#define LPX_DEFAULT_SMP_TIMER_INTERVAL			msecs_to_jiffies(150)

#define LPX_DEFAULT_MAX_STALL_INTERVAL			msecs_to_jiffies(8000)
#define LPX_DEFAULT_MAX_CONNECT_WAIT_INTERVAL	msecs_to_jiffies(3000)

#define LPX_DEFAULT_RETRANSMIT_INTERVAL			msecs_to_jiffies(200)
#define LPX_DEFAULT_ALIVE_INTERVAL				msecs_to_jiffies(1000)

#define LPX_DEFAULT_TIME_WAIT_INTERVAL			msecs_to_jiffies(1000)
#define LPX_DEFAULT_LAST_ACK_INTERVAL			msecs_to_jiffies(4000)

struct smp_sock {	

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	struct hlist_head	child_sklist;
#else
	struct sock			*child_sklist;
#endif
	spinlock_t			child_sklist_lock;

	struct sock			*parent_sk;

	__u8	server_tag;
	bool	destination_non_delayed_ack;

#define DEFAULT_PCB_WINDOW	8

	__u16	pcb_window;

	__u16	sequence;
	__u16	remote_ack_sequence;
	__u16	fin_sequence;

	__u16	remote_sequence;

	__u16	next_transmit_sequence;

	struct timer_list	smp_timer; 
	
	__u32	smp_timer_flag;
#define	SMP_TIMER_DELAYED_ACK	0x0001
#define	SMP_SENDIBLE			0x0002
#define	SMP_RETRANSMIT_ACKED	0x0004

	unsigned long	max_stall_timeout;

	unsigned long	retransmit_timeout;

	unsigned long	alive_timeout;

	unsigned long	time_wait_timeout;
	unsigned long	last_ack_timeout;

	unsigned long	smp_timer_interval;

	unsigned long	max_stall_interval;
	unsigned long	max_connect_wait_interval;

	unsigned long	retransmit_interval;

	unsigned long	alive_interval;

	unsigned long	time_wait_interval;
	unsigned long	last_ack_interval;

#define SMP_MAX_FLIGHT 1024
	__u32	max_flights;
	__u32	retransmit_count;	

	struct sk_buff_head	receive_reorder_queue;

	int 			remained_user_data_length;
	unsigned int	received_data_length;

	void 	(*sk_data_ready) (struct sock *sk, int len);
	
	__u8	option;
};


/* Connection state defines */

typedef enum _SMP_STATE {

	SMP_STATE_ESTABLISHED = 1,
	SMP_STATE_SYN_SENT,
	SMP_STATE_SYN_RECV,
	SMP_STATE_FIN_WAIT1,
	SMP_STATE_FIN_WAIT2,
	SMP_STATE_TIME_WAIT,
	SMP_STATE_CLOSE,
	SMP_STATE_CLOSE_WAIT,
	SMP_STATE_LAST_ACK,
	SMP_STATE_LISTEN,
	SMP_STATE_CLOSING,     /* now a valid state */
	SMP_STATE_LAST,

	SMP_STATE_MAX /* Leave at the end! */

} SMP_STATE, *PSMP_STATE;

typedef enum _LPX_SMP_PACKET_TYPE {
    
	SMP_TYPE_DATA 		= 0,
    SMP_TYPE_ACK		= 1,
    SMP_TYPE_CONREQ 	= 4,
    SMP_TYPE_DISCON		= 6,
    SMP_TYPE_RETRANS	= 8,
    SMP_TYPE_ACKREQ		= 9

} LPX_SMP_PACKET_TYPE, *PLPX_SMP_PACKET_TYPE;

/* Packet transmit types - Internal */
#define DATA	0	/* Data */
#define ACK		1	/* Data ACK */
#define	CONREQ	4	/* Connection Request */
#define	DISCON	6	/* Informed Disconnect */
#define RETRAN	8	/* Int. Retransmit of packet */
#define ACKREQ	9	/* Int. Retransmit of packet */

struct lpx_sock {
	/* struct sock has to be the first member of lpx_sock */
#if !__LPX__
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	struct sock			sk;
#endif
#endif
	struct lpx_address		dest_addr;
	struct lpx_interface	*intrfc;
	__be16					port;
#ifdef CONFIG_LPX_INTERN
	unsigned char			node[LPX_NODE_LEN];
#endif
	unsigned short	type;
	/*
	 * To handle special ncp connection-handling sockets for mars_nwe,
 	 * the connection number must be stored in the socket.
	 */
#if !__LPX__
	unsigned short	lpx_ncp_conn;
#endif

	__u8 source_node[LPX_NODE_LEN]; // don't use it except finding intrfc
	struct net_device	*source_dev;

#if __LPX__
	struct smp_sock	smp_sk;
#endif
};

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

static inline struct lpx_sock *lpx_sk(struct sock *sk)
{
	return (struct lpx_sock *)(sk+1);
}

static inline struct smp_sock *smp_sk(struct sock *sk)
{
	return &lpx_sk(sk)->smp_sk;
}

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

#define lpx_sk(__sk) ((struct lpx_sock *)(__sk)->sk_protinfo)

#if 0
static inline struct lpx_sock *lpx_sk(struct sock *sk)
{
	return (struct lpx_sock *)sk->sk_protinfo;
}
#endif

static inline struct smp_sock *smp_sk(struct sock *sk)
{
	return &lpx_sk(sk)->smp_sk;
}

#else

C_ASSERT( 0, sizeof(struct lpx_sock) <= sizeof(((struct sock *)NULL)->tp_pinfo) );

static inline struct lpx_sock *lpx_sk(struct sock *sk)
{
	return (struct lpx_sock *)&sk->tp_pinfo;
}

static inline struct smp_sock *smp_sk(struct sock *sk)
{
	return &lpx_sk(sk)->smp_sk;
}

#endif

#if !__LPX__
#define LPX_SKB_CB(__skb) ((struct lpx_cb *)&((__skb)->cb[0]))
#endif

#endif

#define LPX_MIN_EPHEMERAL_SOCKET	0x4000
#define LPX_MAX_EPHEMERAL_SOCKET	0x7fff

extern struct list_head lpx_routes;
extern rwlock_t lpx_routes_lock;

extern struct list_head lpx_interfaces;
extern struct lpx_interface *lpx_interfaces_head(void);
extern spinlock_t lpx_interfaces_lock;

extern struct lpx_interface *lpx_lo_interface;

extern int lpx_proc_init(void);
extern void lpx_proc_exit(void);

extern const char *lpx_frame_name(__be16);
extern const char *lpx_device_name(struct lpx_interface *intrfc);

static __inline__ void lpxitf_hold(struct lpx_interface *intrfc)
{
	atomic_inc(&intrfc->refcnt);
}

extern void lpxitf_down(struct lpx_interface *intrfc);

static __inline__ void lpxitf_put(struct lpx_interface *intrfc)
{
	if (atomic_dec_and_test(&intrfc->refcnt))
		lpxitf_down(intrfc);
}

#if !__LPX__

static __inline__ void lpxrtr_hold(struct lpx_route *rt)
{
	        atomic_inc(&rt->refcnt);
}

static __inline__ void lpxrtr_put(struct lpx_route *rt)
{
	        if (atomic_dec_and_test(&rt->refcnt))
			                kfree(rt);
}

#endif

#endif /* _NET_INET_LPX_H_ */

