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

#ifndef __LPX_COMPAT_H__
#define __LPX_COMPAT_H__

#include <linux/version.h>
#include <linux/module.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#else

#define SEQ_START_TOKEN ((void *)1)
#define DEFINE_SPINLOCK(x)	spinlock_t x = SPIN_LOCK_UNLOCKED
#define eth_hdr(skb) (skb->mac.ethernet)

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,23))
#else

#define __user

#endif

#define module_refcount(module) GET_USE_COUNT(module)

#include <net/sock.h>

#define sock_owned_by_user(sk)  ((sk)->lock.users)

#define sk_write_queue		write_queue
#define sk_write_space		write_space
#define sk_state			state
#define sk_state_change 	state_change
#define sk_socket			socket
#define sk_wmem_alloc   	wmem_alloc
#define sk_rmem_alloc		rmem_alloc
#define sk_data_ready 		data_ready
#define sk_receive_queue	receive_queue
#define sk_err			 	err
#define sk_error_report		error_report
#define sk_sndbuf 			sndbuf
#define sk_rcvbuf			rcvbuf
#define sk_sleep		 	sleep
#define sk_prot				prot
#define sk_backlog_rcv		backlog_rcv
#define sk_shutdown			shutdown
#define sk_backlog		 	backlog
#define sk_no_check			no_check
#define sk_max_ack_backlog	max_ack_backlog
#define sk_ack_backlog		ack_backlog
#define sk_user_data		user_data
#define sk_refcnt			refcnt
#define sk_zapped			zapped
#define sk_stamp			stamp

enum sock_flags {
	SOCK_DEAD,
	//SOCK_DONE,
	//SOCK_URGINLINE,
	//SOCK_KEEPOPEN,
	//SOCK_LINGER,
	SOCK_DESTROY,
	//SOCK_BROADCAST,
	//SOCK_TIMESTAMP,
	SOCK_ZAPPED,
	//SOCK_USE_WRITE_QUEUE, /* whether to call sk->write_space in sock_wfree */
	//SOCK_DBG, /* %SO_DEBUG setting */
	//SOCK_RCVTSTAMP, /* %SO_TIMESTAMP setting */
	//SOCK_RCVTSTAMPNS, /* %SO_TIMESTAMPNS setting */
	//SOCK_LOCALROUTE, /* route locally only, %SO_DONTROUTE setting */
	//SOCK_QUEUE_SHRUNK, /* write queue has been shrunk recently */
};

static inline int sock_flag(struct sock *sk, enum sock_flags flag)
{
	switch (flag) {

	case SOCK_DEAD:
	
		return sk->dead;

	case SOCK_ZAPPED:
	
		return sk->zapped;

	case SOCK_DESTROY:
	
		return sk->destroy;

	default:
	
		NDAS_BUG_ON(true);
	}

	return 0;
}

static inline void sock_set_flag(struct sock *sk, enum sock_flags flag)
{
	switch (flag) {

	case SOCK_DEAD:
	
		sk->dead = 1;
		break;

	case SOCK_ZAPPED:
	
		sk->zapped = 1;
		break;

	case SOCK_DESTROY:
	
		sk->destroy = 1;
		break;

	default:
	
		NDAS_BUG_ON(true);
	}

	return;
}

static inline void sock_reset_flag(struct sock *sk, enum sock_flags flag)
{
	switch (flag) {

	case SOCK_DEAD:
	
		sk->dead = 0;
		break;

	case SOCK_ZAPPED:
	
		sk->zapped = 0;
		break;

	case SOCK_DESTROY:
	
		sk->destroy = 0;
		break;

	default:
	
		NDAS_BUG_ON(true);
	}

	return;
}

#endif

#endif // __LPX_COMPAT_H__
