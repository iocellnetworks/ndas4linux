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

#include <linux/version.h>
#include <linux/module.h> 

#include <linux/delay.h>

#define __LPX__		1

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

#include <asm/uaccess.h>

#if __LPX__
#include <ndas_lpx.h>
#include "netlpx.h"
#endif

#ifdef DEBUG

int	dbg_level_lpx_ksocket = DBG_LEVEL_LPX_KSOCKET;

#define dbg_call(l,x...) do { 								\
    if ( l <= dbg_level_lpx_ksocket ) {						\
        printk("LPK|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif

struct net_device *lpxitf_find_first(void) 
{
    return !list_empty(&lpx_interfaces) ? lpx_interfaces_head()->if_dev : NULL;
}

int lpxitf_iterate_address(void (*func)(unsigned char*, void*), void* arg) 
{
	struct lpx_interface	*i;
	int						count = 0;

	list_for_each_entry(i, &lpx_interfaces, node) {

        func(i->if_node, arg);
        count ++;
	}

    return count;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#else

int kernel_accept(struct socket *sock, struct socket **newsock, int flags)
{
	struct sock *sk = sock->sk;
	int err;

	err = lpx_sock_create(sk->type, sk->protocol, newsock);
	if (err < 0)
		goto done;

	err = sock->ops->accept(sock, *newsock, flags);
	if (err < 0) {
		lpx_sock_close(*newsock);
		*newsock = NULL;
		goto done;
	}

	(*newsock)->ops = sock->ops;

done:
	return err;
}

int kernel_sendmsg(struct socket *sock, struct msghdr *msg,
		   struct iovec *vec, size_t num, size_t size)
{
	mm_segment_t oldfs = get_fs();
	int result;

	set_fs(KERNEL_DS);
	/*
	 * the following is safe, since for compiler definitions of kvec and
	 * iovec are identical, yielding the same in-core layout and alignment
	 */
	msg->msg_iov = (struct iovec *)vec,
	msg->msg_iovlen = num;
	result = sock_sendmsg(sock, msg, size);
	set_fs(oldfs);
	return result;
}

int kernel_recvmsg(struct socket *sock, struct msghdr *msg, 
		   struct iovec *vec, size_t num,
		   size_t size, int flags)
{
	mm_segment_t oldfs = get_fs();
	int result;

	set_fs(KERNEL_DS);
	/*
	 * the following is safe, since for compiler definitions of kvec and
	 * iovec are identical, yielding the same in-core layout and alignment
	 */
	msg->msg_iov = (struct iovec *)vec,
	msg->msg_iovlen = num;
	result = sock_recvmsg(sock, msg, size, flags);
	set_fs(oldfs);
	return result;
}

#endif

ndas_error_t lpx_sock_create(int lpxtype, int protocol, struct socket **socket)
{
	int 			ret;
	struct socket	*sock;
	int				sock_type;

	dbg_call( 1, "module_refcount(THIS_MODULE) = %d\n", module_refcount(THIS_MODULE) );

	if (lpxtype == LPX_TYPE_DATAGRAM) {
		
		sock_type = SOCK_DGRAM;
	
	} else if (lpxtype == LPX_TYPE_STREAM) {

		sock_type = SOCK_STREAM;
	
	} else {

		BUG_ON(true);
        return NDAS_ERROR_INVALID_HANDLE;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	ret = sock_create_kern(AF_LPX, sock_type, 0, &sock);
#else
	ret = sock_create(AF_LPX, sock_type, 0, &sock);
#endif

	if (ret < 0) {

		BUG_ON(true);
		return ret;
	}

	dbg_call( 1, "lpxtype = %d, sock->type = %d, sock = %p, sock->sk = %p, "
				 "ret = %d, module_refcount = %d, sock->sk->sk_refcnt = %d\n", 
				 lpxtype, sock->type, sock, sock->sk, ret, module_refcount(THIS_MODULE), atomic_read(&sock->sk->sk_refcnt) );

	*socket = sock;

	return NDAS_OK;
}

void lpx_sock_close(struct socket *lpxfd)
{
	struct socket	*sock = (struct socket *)lpxfd;
	struct sock		*sk;

	dbg_call( 1, "st\n" );

	NDAS_BUG_ON(sock == NULL);
	sk = sock->sk;

	dbg_call( 4, "module_refcount(THIS_MODULE) = %d, atomic_read(&sock->sk_refcnt) =%d\n",
				  module_refcount(THIS_MODULE), atomic_read(&sk->sk_refcnt) );

	dbg_call( 4, "sock = %p, sk = %p\n", sock, sk );

	sock_release( sock );

	dbg_call( 4, "sock_release return\n" );

	dbg_call( 4, "module_refcount(THIS_MODULE) = %d\n", module_refcount(THIS_MODULE) );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#else
	msleep(2000);
#endif
	dbg_call( 1, "end\n" );

	return;
}

ndas_error_t lpx_sock_bind(struct socket *sock, struct sockaddr_lpx* addr, int addrlen)
{
	ndas_error_t	err;
	struct sock		*sk;

	dbg_call( 4, "st\n" );

	NDAS_BUG_ON(sock == NULL);
	sk = sock->sk;

	sock_hold(sk);

	dbg_call( 4, "module_refcount(THIS_MODULE) = %d, atomic_read(&sock->sk_refcnt) = %d\n", 
				  module_refcount(THIS_MODULE), atomic_read(&sk->sk_refcnt) );

	dbg_call( 4, "sock = %p\n", container_of(&sk, struct socket, sk) );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
	err = kernel_bind( sock, (struct sockaddr *)addr, addrlen );
#else
	err = sock->ops->bind( sock, (struct sockaddr *)addr, addrlen );
#endif

	sock_put(sk);

	dbg_call( 4, "end module_refcount(THIS_MODULE) = %d, atomic_read(&sock->sk_refcnt) =%d\n", 
				  module_refcount(THIS_MODULE), atomic_read(&sk->sk_refcnt) );

	return err;
}

ndas_error_t lpx_sock_connect(struct socket *sock, const struct sockaddr_lpx *serv_addr, int addrlen)
{
	ndas_error_t	err;
	struct sock		*sk;

	dbg_call( 1, "st\n" );

	NDAS_BUG_ON( sock == NULL );
	sk = sock->sk;

	sock_hold(sk);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
	err = kernel_connect( sock, (struct sockaddr *)serv_addr, addrlen, 0 );
#else
	err = sock->ops->connect( sock, (struct sockaddr *)serv_addr, addrlen, 0 );
#endif

	sock_put(sk);

	dbg_call( 1, "rt err = %d\n", err );

	return err;
}

ndas_error_t lpx_sock_listen(struct socket *sock, int backlog)
{
	ndas_error_t	err;
	struct sock		*sk;

	dbg_call( 1, "et\n" );

	NDAS_BUG_ON(sock == NULL);
	sk = sock->sk;

	sock_hold(sk);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
	err = kernel_listen( sock, backlog );
#else
	err = sock->ops->listen( sock, backlog );
#endif

	sock_put(sk);

	dbg_call( 1, "rt err = %d\n", err );

	return err;
}

ndas_error_t lpx_sock_accept(struct socket *sock, struct socket **newsock, struct sockaddr_lpx *cli_addr, unsigned int *addrlen, int flags)
{
	ndas_error_t	err;
	struct sock		*sk;

	dbg_call( 1, "et\n" );

	NDAS_BUG_ON( cli_addr );

	NDAS_BUG_ON( sock == NULL );
	sk = sock->sk;

	sock_hold(sk);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))
	err = kernel_accept( sock, newsock, flags );
#else
	err = kernel_accept( sock, newsock, flags );
#endif

	sock_put(sk);

	dbg_call( 1, "rt err = %d\n", err );

	return err;
}

ndas_error_t lpx_sock_send(struct socket *sock, void *buf, int len, int flags)
{
	ndas_error_t	err;

	struct msghdr	msg;
	struct iovec	iov;

	dbg_call( 4, "et\n" );

	NDAS_BUG_ON( sock == NULL );

    if (sock->type != SOCK_STREAM) {

		BUG_ON(true);
	    return NDAS_ERROR_INVALID_OPERATION;
    }

	iov.iov_base = buf;
	iov.iov_len = len;
	
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = flags | MSG_NOSIGNAL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	err = kernel_sendmsg( sock, &msg, (struct kvec *)&iov, 1, len );
#else
	err = kernel_sendmsg( sock, &msg, &iov, 1, len );
#endif

	dbg_call( 4, "rt err = %d\n", err );

	if (err <= 0) {

		dbg_call( 1, "rt err = %d\n", err );
	}

	return err;
}

ndas_error_t lpx_sock_recv(struct socket *sock, void* buf, int len, int flags)
{
	ndas_error_t	err;

	struct msghdr	msg;
	struct iovec	iov;

	dbg_call( 4, "et len = %d\n", len );

	NDAS_BUG_ON( sock == NULL );

    if (sock->type != SOCK_STREAM) {

		BUG_ON(true);
	    return NDAS_ERROR_INVALID_OPERATION;
    }

	iov.iov_base = buf;
	iov.iov_len = len;
	
	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = flags | MSG_NOSIGNAL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	err = kernel_recvmsg( sock, &msg, (struct kvec *)&iov, 1, len, 0 );
#else
	err = kernel_recvmsg( sock, &msg, &iov, 1, len, 0 );
#endif

	if (err <= 0) {

		dbg_call( 1, "rt err = %d\n", err );
	}
	
	dbg_call( 3, "rt err = %d\n", err );

	return err;
}

ndas_error_t lpx_sock_sendmsg (struct socket *sock, struct iovec *blocks, size_t nr_blocks, size_t len, int flags)
{
	ndas_error_t	err;
	struct msghdr	msg;

	dbg_call( 4, "et\n" );

	NDAS_BUG_ON( sock == NULL );

	BUG_ON( nr_blocks == 0 || nr_blocks > 128 );

	dbg_call( 4, "sock->type = %d len = %lu\n", sock->type, (unsigned long)len );

	dbg_call( 4, "blocks = %p\n", blocks );

    if (sock->type != SOCK_STREAM) {

		BUG_ON(true);
	    return NDAS_ERROR_INVALID_OPERATION;
    }

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = flags | MSG_NOSIGNAL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	err = kernel_sendmsg( sock, &msg, (struct kvec *)blocks, nr_blocks, len );
#else
	err = kernel_sendmsg( sock, &msg, blocks, nr_blocks, len );
#endif

	dbg_call( 4, "rt return err = %d\n", err );

	if (err <= 0) {

		dbg_call( 1, "rt err = %d\n", err );
	}

	return err;
}

ndas_error_t lpx_sock_recvmsg(struct socket *sock, struct iovec *blocks, size_t nr_blocks, size_t len, int flags)
{
	ndas_error_t	err;
	struct msghdr	msg;

	dbg_call( 4, "et\n" );

	NDAS_BUG_ON( sock == NULL );

	BUG_ON( nr_blocks == 0 || nr_blocks > 128 );

	dbg_call( 4, "sock->type = %d len = %lu\n", sock->type, (unsigned long)len );

	dbg_call( 4, "blocks = %p\n", blocks );

    if (sock->type != SOCK_STREAM) {

		BUG_ON(true);
	    return NDAS_ERROR_INVALID_OPERATION;
    }

	msg.msg_name = NULL;
	msg.msg_namelen = 0;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = flags | MSG_NOSIGNAL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	err = kernel_recvmsg( sock, &msg, (struct kvec *)blocks, nr_blocks, len, 0 );
#else
	err = kernel_recvmsg( sock, &msg, blocks, nr_blocks, len, 0 );
#endif

	dbg_call( 4, "rt return err = %d\n", err );

	if (err <= 0) {

		dbg_call( 1, "rt err = %d\n", err );
	}

	return err;
}

ndas_error_t lpx_sock_sendto(struct socket *sock, void* buf, int len, int flags, struct sockaddr_lpx *to, int tolen)
{
	ndas_error_t	err;
	struct sock		*sk;

	struct msghdr	msg;
	struct iovec	iov;

	dbg_call( 4, "et len = %d\n", len );

	NDAS_BUG_ON(sock == NULL);
	sk = sock->sk;

	sock_hold(sk);

    if (sock->type != SOCK_DGRAM) {

		BUG_ON(true);
		sock_put(sk);
	    return NDAS_ERROR_INVALID_OPERATION;
    }

	iov.iov_base = buf;
	iov.iov_len = len;
	
	msg.msg_name = to;
	msg.msg_namelen = tolen;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = flags | MSG_NOSIGNAL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	err = kernel_sendmsg( sock, &msg, (struct kvec *)&iov, 1, len );
#else
	err = kernel_sendmsg( sock, &msg, &iov, 1, len );
#endif

	sock_put(sk);

	dbg_call( 4, "rt err = %d\n", err );

	return err;
}

ndas_error_t lpx_sock_recvfrom(struct socket *sock, void* buf, int len, int flags, struct sockaddr_lpx *from, int fromlen)
{
	ndas_error_t	err;
	struct sock		*sk;

	struct msghdr	msg;
	struct iovec	iov;

	dbg_call( 4, "et len = %d\n", len );

	NDAS_BUG_ON(sock == NULL);
	sk = sock->sk;

	sock_hold(sk);

    if (sock->type != SOCK_DGRAM) {

		BUG_ON(true);
		sock_put(sk);
	    return NDAS_ERROR_INVALID_OPERATION;
    }

	iov.iov_base = buf;
	iov.iov_len = len;
	
	msg.msg_name = from;
	msg.msg_namelen = fromlen;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = flags | MSG_NOSIGNAL;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
		err = kernel_recvmsg( sock, &msg, (struct kvec *)&iov, 1, len, flags );
#else
		err = kernel_recvmsg( sock, &msg, &iov, 1, len, flags );
#endif

	sock_put(sk);

	dbg_call( 4, "rt err = %d\n", err );

	return err;
}

struct lpx_parameter {

	struct socket  *sock;
	void			*buf;
	int				len;
	int				flags;
	lpx_sio_handler	func;
	void *			arg;
	int				timeout;
};

static int lpx_sio_recvfrom_thread(void *arg)
{
	struct lpx_parameter *lpx_parameter = (struct lpx_parameter *)arg;

	const char		*name = "lpx_siod";
    unsigned long	flags;

	ndas_error_t	err;
	struct socket	*sock = (struct socket *)lpx_parameter->sock;
	struct sock		*sk = sock->sk;
	
	struct msghdr	msg;
	struct iovec	iov;

	struct sockaddr_lpx from;
	int					fromlen = sizeof(struct sockaddr_lpx);

	lpx_siocb		lpx_siocb;

	dbg_call( 1, "timeout = %d\n", lpx_parameter->timeout/HZ );

	BUG_ON( lpx_parameter->timeout != 0 );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	daemonize(name);
#else
	daemonize();
	sprintf( current->comm, name );
    reparent_to_init(); // kernel over 2.5 don't need this because daemonize call this function.
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

    spin_lock_irqsave( &current->sighand->siglock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending();
    spin_unlock_irqrestore( &current->sighand->siglock, flags );

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,27))

    spin_lock_irqsave( &current->sigmask_lock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending(current);
    spin_unlock_irqrestore( &current->sigmask_lock, flags );

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,23))

    spin_lock_irqsave( &current->sighand->siglock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending();
    spin_unlock_irqrestore( &current->sighand->siglock, flags );

#else

    spin_lock_irqsave( &current->sigmask_lock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending(current);
    spin_unlock_irqrestore( &current->sigmask_lock, flags );

#endif

	sock_hold(sk);

	dbg_call( 4, "module_refcount(THIS_MODULE) = %d, atomic_read(&sock->sk_refcnt) =%d\n", 
				  module_refcount(THIS_MODULE), atomic_read(&sk->sk_refcnt) );

    if (sock->type != SOCK_DGRAM) {

		BUG_ON(true);
		sock_put(sk);
	    return NDAS_ERROR_INVALID_OPERATION;
    }

	do {
	
		iov.iov_base = lpx_parameter->buf;
		iov.iov_len = lpx_parameter->len;
	
		msg.msg_name = &from;
		msg.msg_namelen = fromlen;
		msg.msg_control = NULL;
		msg.msg_controllen = 0;
		msg.msg_flags = lpx_parameter->flags | MSG_NOSIGNAL;

		dbg_call( 4, "kernel_recvmsg before\n" );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
		err = kernel_recvmsg( sock, &msg, (struct kvec *)&iov, 1, lpx_parameter->len, lpx_parameter->flags );
#else
		err = kernel_recvmsg( sock, &msg, &iov, 1, lpx_parameter->len, lpx_parameter->flags );
#endif

		dbg_call( 4, "err = %x, sock = %p\n", err, sock );

		lpx_siocb.sock = sock;
		lpx_siocb.buf = lpx_parameter->buf;
		lpx_siocb.nbytes = err;
		lpx_siocb.result = err;
	
		if (lpx_sk(sk)->port == 0xee00 || lpx_sk(sk)->port == 0xee) {

			dbg_call( 1, "lpx_sio_recvfrom_thread, call lpx(sk)->port = %x\n", lpx_sk(sk)->port );
		}

		lpx_parameter->func( &lpx_siocb, &from, lpx_parameter->arg );

		if (lpx_sk(sk)->port == 0xee00 || lpx_sk(sk)->port == 0xee) {
	
			dbg_call( 1, "rt call return\n" );
		}

	} while (err > 0);

	dbg_call( 1, "rtmodule_refcount(THIS_MODULE) = %d, atomic_read(&sock->sk_refcnt) =%d\n", 
				  module_refcount(THIS_MODULE), atomic_read(&sk->sk_refcnt) );

	sock_put(sk);
	kfree(lpx_parameter);

	return 0;
}


ndas_error_t 
lpx_sio_recvfrom(struct socket *sock, void* buf, int len, int flags, 
                 lpx_sio_handler func, void* arg, unsigned long timeout)
{
	ndas_error_t	err;
	struct sock		*sk;

	struct lpx_parameter *lpx_parameter;
	
	dbg_call( 4, "et timeout = %ld\n", timeout/HZ );

	NDAS_BUG_ON(timeout != 0);
	NDAS_BUG_ON(sock == NULL);

	sk = sock->sk;

	lpx_parameter = kmalloc(sizeof(struct lpx_parameter), GFP_KERNEL);

	if (lpx_parameter == NULL) {

		BUG_ON(true);
		return NDAS_ERROR_INVALID_HANDLE;
	}

	sock_hold(sk);

    if (sock->type != SOCK_DGRAM) {

		BUG_ON(true);
		sock_put(sk);
	    return NDAS_ERROR_INVALID_OPERATION;
    }

	dbg_call( 4, "module_refcount(THIS_MODULE) = %d, atomic_read(&sock->sk_refcnt) =%d\n", 
				  module_refcount(THIS_MODULE), atomic_read(&sk->sk_refcnt) );

	sock_put(sk);

	dbg_call( 4, "module_refcount(THIS_MODULE) = %d, atomic_read(&sock->sk_refcnt) =%d\n", 
				  module_refcount(THIS_MODULE), atomic_read(&sk->sk_refcnt) );

	lpx_parameter->sock = sock;
	lpx_parameter->buf	= buf;
	lpx_parameter->len	= len;
	lpx_parameter->flags = flags;
	lpx_parameter->func	= func;
	lpx_parameter->arg	= arg;
	lpx_parameter->timeout	= timeout;

	err = kernel_thread( lpx_sio_recvfrom_thread, lpx_parameter, 0 );

	if (err < 0) {

		BUG_ON(true);
	}

	dbg_call( 4, "rt module_refcount(THIS_MODULE) = %d\n", module_refcount(THIS_MODULE) );

	return err;
}

ndas_error_t lpx_is_connected(struct socket *sock) {

	ndas_error_t	err;
	struct sock		*sk;

	NDAS_BUG_ON(sock == NULL);
	sk = sock->sk;

	sock_hold(sk);

	if (lpx_sk(sk)->type == LPX_TYPE_STREAM && sk->sk_state == SMP_STATE_ESTABLISHED) {

		err = NDAS_OK;
	
	} else {

		err = NDAS_ERROR_NOT_CONNECTED;
	}

	sock_put(sk);
	return err;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
MODULE_LICENSE("Dual BSD/GPL");
#endif
MODULE_AUTHOR("Ximeta, Inc.");


EXPORT_SYMBOL(lpxitf_find_first);
EXPORT_SYMBOL(lpxitf_iterate_address);

EXPORT_SYMBOL(lpx_sock_create);
EXPORT_SYMBOL(lpx_sock_close);
EXPORT_SYMBOL(lpx_sock_bind);
EXPORT_SYMBOL(lpx_sock_connect);
EXPORT_SYMBOL(lpx_sock_listen);
EXPORT_SYMBOL(lpx_sock_accept);
EXPORT_SYMBOL(lpx_sock_send);
EXPORT_SYMBOL(lpx_sock_recv);
EXPORT_SYMBOL(lpx_sock_recvmsg);
EXPORT_SYMBOL(lpx_sock_sendmsg);
EXPORT_SYMBOL(lpx_sock_sendto);
EXPORT_SYMBOL(lpx_sock_recvfrom);
EXPORT_SYMBOL(lpx_sio_recvfrom);

EXPORT_SYMBOL(lpx_is_connected);

