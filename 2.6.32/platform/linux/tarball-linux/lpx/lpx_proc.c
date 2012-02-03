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

#include <linux/version.h>
#include <linux/module.h> 

#define __LPX__		1

#include <linux/init.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/seq_file.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#include <net/tcp_states.h>
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#include <net/tcp.h>
#endif
#if !__LPX__
#include <net/lpx.h>
#endif

#if __LPX__
#include <ndas_lpx.h>
#include "netlpx.h"
#endif

#ifdef DEBUG

extern int	dbg_level_lpx;

#define dbg_call(l,x...) do { 								\
    if ( l <= dbg_level_lpx ) {								\
        printk("LP|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
        printk(x); 											\
    } 														\
} while(0)

#define DEBUG_CALL(l,x) do { 								\
    if ( l <= dbg_level_lpx ) {								\
        printk("LP|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		printk x;											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#define DEBUG_CALL(l,x) do {} while(0)
#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

static __inline__ struct lpx_interface *lpx_get_interface_idx(loff_t pos)
{
	struct lpx_interface *i;

	list_for_each_entry(i, &lpx_interfaces, node)
		if (!pos--)
			goto out;
	i = NULL;
out:
	return i;
}

static struct lpx_interface *lpx_interfaces_next(struct lpx_interface *i)
{
	struct lpx_interface *rc = NULL;

	if (i->node.next != &lpx_interfaces)
		rc = list_entry(i->node.next, struct lpx_interface, node);
	return rc;
}

static void *lpx_seq_interface_start(struct seq_file *seq, loff_t *pos)
{
	loff_t l = *pos;

	spin_lock_bh(&lpx_interfaces_lock);
	return l ? lpx_get_interface_idx(--l) : SEQ_START_TOKEN;
}

static void *lpx_seq_interface_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct lpx_interface *i;

	++*pos;
	if (v == SEQ_START_TOKEN)
		i = lpx_interfaces_head();
	else
		i = lpx_interfaces_next(v);
	return i;
}

static void lpx_seq_interface_stop(struct seq_file *seq, void *v)
{
	spin_unlock_bh(&lpx_interfaces_lock);
}

static int lpx_seq_interface_show(struct seq_file *seq, void *v)
{
	struct lpx_interface *i;

	if (v == SEQ_START_TOKEN) {
		seq_puts(seq, "Network    Node_Address   Primary  Device     "
			      "Frame_Type");
#ifdef LPX_REFCNT_DEBUG
		seq_puts(seq, "  refcnt");
#endif
		seq_puts(seq, "\n");
		goto out;
	}

	i = v;
#if !__LPX__
	seq_printf(seq, "%08lX   ", (unsigned long int)ntohl(i->if_netnum));
#endif
	seq_printf(seq, "%02X%02X%02X%02X%02X%02X   ",
			i->if_node[0], i->if_node[1], i->if_node[2],
			i->if_node[3], i->if_node[4], i->if_node[5]);
	seq_printf(seq, "%-9s", i == lpx_lo_interface ? "Yes" : "No");
	seq_printf(seq, "%-11s", lpx_device_name(i));
	seq_printf(seq, "%-9s", lpx_frame_name(i->if_dlink_type));
#ifdef LPX_REFCNT_DEBUG
	seq_printf(seq, "%6d", atomic_read(&i->refcnt));
#endif
	seq_puts(seq, "\n");
out:
	return 0;
}

#if !__LPX__
static struct lpx_route *lpx_routes_head(void)
{
	struct lpx_route *rc = NULL;

	if (!list_empty(&lpx_routes))
		rc = list_entry(lpx_routes.next, struct lpx_route, node);
	return rc;
}

static struct lpx_route *lpx_routes_next(struct lpx_route *r)
{
	struct lpx_route *rc = NULL;

	if (r->node.next != &lpx_routes)
		rc = list_entry(r->node.next, struct lpx_route, node);
	return rc;
}

static __inline__ struct lpx_route *lpx_get_route_idx(loff_t pos)
{
	struct lpx_route *r;

	list_for_each_entry(r, &lpx_routes, node)
		if (!pos--)
			goto out;
	r = NULL;
out:
	return r;
}

static void *lpx_seq_route_start(struct seq_file *seq, loff_t *pos)
{
	loff_t l = *pos;
	read_lock_bh(&lpx_routes_lock);
	return l ? lpx_get_route_idx(--l) : SEQ_START_TOKEN;
}

static void *lpx_seq_route_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct lpx_route *r;

	++*pos;
	if (v == SEQ_START_TOKEN)
		r = lpx_routes_head();
	else
		r = lpx_routes_next(v);
	return r;
}

static void lpx_seq_route_stop(struct seq_file *seq, void *v)
{
	read_unlock_bh(&lpx_routes_lock);
}

static int lpx_seq_route_show(struct seq_file *seq, void *v)
{
	struct lpx_route *rt;

	if (v == SEQ_START_TOKEN) {
		seq_puts(seq, "Network    Router_Net   Router_Node\n");
		goto out;
	}
	rt = v;
	seq_printf(seq, "%08lX   ", (unsigned long int)ntohl(rt->ir_net));
	if (rt->ir_routed)
		seq_printf(seq, "%08lX     %02X%02X%02X%02X%02X%02X\n",
			   (long unsigned int)ntohl(rt->ir_intrfc->if_netnum),
			   rt->ir_router_node[0], rt->ir_router_node[1],
			   rt->ir_router_node[2], rt->ir_router_node[3],
			   rt->ir_router_node[4], rt->ir_router_node[5]);
	else
		seq_puts(seq, "Directly     Connected\n");
out:
	return 0;
}
#endif

static __inline__ struct sock *lpx_get_socket_idx(loff_t pos)
{
	struct sock *s = NULL;
	struct hlist_node *node = NULL;
	struct lpx_interface *i;

	list_for_each_entry(i, &lpx_interfaces, node) {
		spin_lock_bh(&i->if_sklist_lock);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

		sk_for_each(s, node, &i->if_sklist) {
			if (!pos)
				break;
			--pos;
		}

#else

		for (s = i->if_sklist; s;) {
			
			if (!pos)
				break;
			--pos;

			s = s->next;
		}

#endif

		spin_unlock_bh(&i->if_sklist_lock);
		if (!pos) {
			if (node)
				goto found;
			break;
		}
	}
	s = NULL;
found:
	return s;
}

static void *lpx_seq_socket_start(struct seq_file *seq, loff_t *pos)
{
	loff_t l = *pos;

	spin_lock_bh(&lpx_interfaces_lock);
	return l ? lpx_get_socket_idx(--l) : SEQ_START_TOKEN;
}

static void *lpx_seq_socket_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct sock* sk, *next;
	struct lpx_interface *i;
	struct lpx_sock *lpxs;

	++*pos;
	if (v == SEQ_START_TOKEN) {
		sk = NULL;
		i = lpx_interfaces_head();
		if (!i)
			goto out;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		sk = sk_head(&i->if_sklist);
#else
		sk = i->if_sklist;
#endif
		if (sk)
			spin_lock_bh(&i->if_sklist_lock);
		goto out;
	}
	sk = v;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	next = sk_next(sk);
#else
	next = sk->next;
#endif
	if (next) {
		sk = next;
		goto out;
	}
	lpxs = lpx_sk(sk);
	i = lpxs->intrfc;
	spin_unlock_bh(&i->if_sklist_lock);
	sk = NULL;
	for (;;) {
		i = lpx_interfaces_next(i);
		if (!i)
			break;
		spin_lock_bh(&i->if_sklist_lock);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
		if (!hlist_empty(&i->if_sklist)) {
			sk = sk_head(&i->if_sklist);
			break;
		}
#else
		if (i->if_sklist) {
			sk = i->if_sklist;
			break;
		}
#endif
		spin_unlock_bh(&i->if_sklist_lock);
	}
out:
	return sk;
}

static int lpx_seq_socket_show(struct seq_file *seq, void *v)
{
	struct sock *s;
	struct lpx_sock *lpxs;

	if (v == SEQ_START_TOKEN) {
#ifdef CONFIG_LPX_INTERN
		seq_puts(seq, "Local_Address               "
			      "Remote_Address              Tx_Queue  "
			      "Rx_Queue  State  Uid\n");
#else
		seq_puts(seq, "Local_Address  Remote_Address              "
			      "Tx_Queue  Rx_Queue  State  Uid\n");
#endif
		goto out;
	}

	s = v;
	lpxs = lpx_sk(s);
#ifdef CONFIG_LPX_INTERN
	seq_printf(seq, "%08lX:%02X%02X%02X%02X%02X%02X:%04X  ",
		   (unsigned long)ntohl(lpxs->intrfc->if_netnum),
		   lpxs->node[0], lpxs->node[1], lpxs->node[2], lpxs->node[3],
		   lpxs->node[4], lpxs->node[5], ntohs(lpxs->port));
#else
	seq_printf(seq, "%04X  ", ntohs(lpxs->port));
#endif	
	/* CONFIG_LPX_INTERN */
	if (s->sk_state != TCP_ESTABLISHED)
		seq_printf(seq, "%-28s", "Not_Connected");
	else {
		seq_printf(seq, "%08lX:%02X%02X%02X%02X%02X%02X:%04X  ",
			   (unsigned long)ntohl(lpxs->dest_addr.net),
			   lpxs->dest_addr.node[0], lpxs->dest_addr.node[1],
			   lpxs->dest_addr.node[2], lpxs->dest_addr.node[3],
			   lpxs->dest_addr.node[4], lpxs->dest_addr.node[5],
			   ntohs(lpxs->dest_addr.port));
	}

	seq_printf(seq, "%08X  %08X  %02X     %03d\n",
		   atomic_read(&s->sk_wmem_alloc),
		   atomic_read(&s->sk_rmem_alloc),
		   s->sk_state, SOCK_INODE(s->sk_socket)->i_uid);
out:
	return 0;
}

static struct seq_operations lpx_seq_interface_ops = {
	.start  = lpx_seq_interface_start,
	.next   = lpx_seq_interface_next,
	.stop   = lpx_seq_interface_stop,
	.show   = lpx_seq_interface_show,
};

#if !__LPX__
static struct seq_operations lpx_seq_route_ops = {
	.start  = lpx_seq_route_start,
	.next   = lpx_seq_route_next,
	.stop   = lpx_seq_route_stop,
	.show   = lpx_seq_route_show,
};
#endif

static struct seq_operations lpx_seq_socket_ops = {
	.start  = lpx_seq_socket_start,
	.next   = lpx_seq_socket_next,
	.stop   = lpx_seq_interface_stop,
	.show   = lpx_seq_socket_show,
};

#if !__LPX__
static int lpx_seq_route_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &lpx_seq_route_ops);
}
#endif

static int lpx_seq_interface_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &lpx_seq_interface_ops);
}

static int lpx_seq_socket_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &lpx_seq_socket_ops);
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17))
static const struct file_operations lpx_seq_interface_fops = {
#else
static struct file_operations lpx_seq_interface_fops = {
#endif

	.owner		= THIS_MODULE,
	.open           = lpx_seq_interface_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = seq_release,
};

#if !__LPX__
static const struct file_operations lpx_seq_route_fops = {
	.owner		= THIS_MODULE,
	.open           = lpx_seq_route_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = seq_release,
};
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17))
static const struct file_operations lpx_seq_socket_fops = {
#else
static struct file_operations lpx_seq_socket_fops = {
#endif

	.owner		= THIS_MODULE,
	.open           = lpx_seq_socket_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = seq_release,
};

static struct proc_dir_entry *lpx_proc_dir;

int __init lpx_proc_init(void)
{
	struct proc_dir_entry *p;
	int rc = -ENOMEM;

	dbg_call( 1, "start\n" );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	lpx_proc_dir = proc_mkdir("lpx", init_net.proc_net);
#else
	lpx_proc_dir = proc_mkdir("lpx", proc_net);
#endif

	if (!lpx_proc_dir)
		goto out;
	p = create_proc_entry("interface", S_IRUGO, lpx_proc_dir);
	if (!p)
		goto out_interface;

	p->proc_fops = &lpx_seq_interface_fops;
	p = create_proc_entry("route", S_IRUGO, lpx_proc_dir);
	if (!p)
		goto out_route;

#if !__LPX__
	p->proc_fops = &lpx_seq_route_fops;
	p = create_proc_entry("socket", S_IRUGO, lpx_proc_dir);
	if (!p)
		goto out_socket;
#endif

	p->proc_fops = &lpx_seq_socket_fops;

	rc = 0;
out:
	dbg_call( 1, "end\n" );

	return rc;
#if !__LPX__
out_socket:
#endif
	remove_proc_entry("route", lpx_proc_dir);
out_route:
	remove_proc_entry("interface", lpx_proc_dir);
out_interface:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	remove_proc_entry("lpx", init_net.proc_net);
#else
	remove_proc_entry("lpx", proc_net);
#endif

	dbg_call( 1, "end\n" );
	goto out;
}

void __exit lpx_proc_exit(void)
{
	remove_proc_entry("interface", lpx_proc_dir);
	remove_proc_entry("route", lpx_proc_dir);
	remove_proc_entry("socket", lpx_proc_dir);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
	remove_proc_entry("lpx", init_net.proc_net);
#else
	remove_proc_entry("lpx", proc_net);
#endif
}

#else // (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

int __init lpx_proc_init(void)
{
	return 0;
}

void __exit lpx_proc_exit(void)
{
}

#endif

#endif /* CONFIG_PROC_FS */
