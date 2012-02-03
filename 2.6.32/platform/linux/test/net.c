/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2005, XIMETA, Inc., IRVINE, CA, USA.
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
#include <linux/module.h> // EXPORT_NO_SYMBOLS, MODULE_LICENSE
#include <linux/version.h> // LINUX_VERSION_CODE, KERNEL_VERSION
#include <linux/init.h> // module_init, module_exit
#include <linux/config.h> // CONFIG_SMP
#include <linux/netdevice.h> // struct netdevice, register_netdevice_notifier, unregister_netdevice_notifier
#include <linux/net.h> // sock_create
#include <net/datalink.h> // struct datalink_proto
#include <net/sock.h> // sock_no_socketpair, sock_no_setsockopt, sock_no_getsockopt, struct sock
#include <linux/timer.h> // timer

#define PF_NDAS 29
#define ETH_NDAS    0x88ad
#define BROADCAST_NODE    "\xFF\xFF\xFF\xFF\xFF\xFF" 
    
#define ETHER_HEADER_SIZE 14
#define SAMPLE_PACKET_SIZE 30
#define SOCK_TYPE_SAMPLE 2

#define NETDISK_MAC_ADDRESS { 0x00, 0x0b, 0xd0, 0x00, 0xfe, 0xeb }

#define HEXDUMP(data,len) \
     __builtin_constant_p(len) ?  \
     _HEXDUMPS(data,len) : _HEXDUMP(data,len) 

#define _HEXDUMPS(data, len) ({      \
    static char buf[len*2+1]; \
    if ( data ) { \
        int i = 0;  \
        for (i = 0; i < (len) && i*2 < sizeof(buf); i++)  \
            snprintf(buf+i*2,3,"%02x:",(unsigned int)((*(data+i)) & 0xffU));  \
    } \
    else  \
        snprintf(buf,sizeof(buf),"NULL"); \
    (const char*) buf; \
})

static inline const char* _HEXDUMP(char *data, int len) {     
    static char buf[1024];
    if ( data ) {
        int i = 0; 
        for (i = 0; i < (len) && i*2 < sizeof(buf); i++) 
            snprintf(buf+i*2,3,"%02x:",(unsigned int)((*(data+i)) & 0xffU)); 
    }
    else 
        snprintf(buf,sizeof(buf),"NULL");
    return (const char*) buf; \
}


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
MODULE_LICENSE("Proprietary, Send bug reports to support@ximeta.com");
#endif
MODULE_AUTHOR("Ximeta, Inc.");

static struct net_proto_family family;
static struct packet_type ptype;
static struct net_device *dev;
static struct socket* sock;

struct proto_ops pops = {
    .family = PF_NDAS, 
    .release = sock_no_release,
    .bind = sock_no_bind,
    .connect = sock_no_connect,
    .socketpair = sock_no_socketpair,
    .accept = sock_no_accept,
    .getname = sock_no_getname,
    .poll = datagram_poll,  /* this does seqlpx too */
    .ioctl = sock_no_ioctl,
    .listen = sock_no_listen,
    .shutdown = sock_no_shutdown,
    .setsockopt = sock_no_setsockopt,
    .getsockopt = sock_no_getsockopt,
    .sendmsg = sock_no_sendmsg,
    .recvmsg = sock_no_recvmsg,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    .owner = THIS_MODULE
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0))
    .mmap = sock_no_mmap,
    .sendpage = sock_no_sendpage
#else    
    .dup = sock_no_dup,
    .fcntl = sock_no_fcntl,
#endif    
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14))
int    rx_handler(struct sk_buff *skb, struct net_device *dev,
                     struct packet_type *ptype)
#else
int    rx_handler(struct sk_buff *skb, struct net_device *dev,
                     struct packet_type *ptype, struct net_device *pdev)
#endif
{
    if(memcmp(BROADCAST_NODE, ((struct ethhdr *)skb->mac.raw)->h_dest, 6) == 0) {
        printk("D");
        return 0;
    }
    printk("rx_handler sk_buff=%p net_device=%p packet_type=%p\n", skb,dev,ptype);
    return 0;
}

static void v_sleep(long msec) 
{
    long jit;
    wait_queue_head_t wait;
#if 1000 > HZ
    jit = (msec/(1000/HZ));
#else
    jit = (msec*(HZ/1000)); // TODO early overflow
#endif    

    if ( in_interrupt() ) {
        long t = jit + jiffies;
        printk("v_sleep in interrupt\n");
        while( jiffies < t) 
            schedule();
        return;
    }
    while(1) {
        long waited;
        long begin = jiffies;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
        DECLARE_WAIT_QUEUE_HEAD(wait);
        wait_event_interruptible_timeout(wait, 0, jit);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20))
        init_waitqueue_head (&wait);
        interruptible_sleep_on_timeout(&wait, jit);
#else
        init_waitqueue_head (&wait);
        
        interruptible_sleep_on_timeout(&wait, jit);

#endif
        
        waited = (jiffies - begin);

        /* somehow when there are many thread active,
          sleep_on_timeout doesn't sleep for the requested time.
          so check if slept for the requested time,
          if 8/HZ sec less, do it again.
         */
        if ( waited + 8 >= jit )  {
            /* TODO: fix me where HZ is not 100*/
            break;
        }
        
        jit -= waited;
    }

}

static int    mycreate(struct socket *sock, int protocol) {
    struct sock* sk;
    printk("mycreate\n");
    sk = sk_alloc(PF_NDAS, GFP_ATOMIC, 1);
    printk("allocated sock=%p\n", sk);
    v_sleep(1000);
    sock->ops = &pops;
    sock_init_data(sock, sk);
    sk->destruct= NULL;
    sk->allocation= GFP_ATOMIC;
    return 0;
}
static void send_packet(unsigned long arg) {
    struct sk_buff* skb;        
    int err;
    char dest_node[6] = NETDISK_MAC_ADDRESS;
    while(1) {
        skb = sock_alloc_send_skb(sock->sk, SAMPLE_PACKET_SIZE, 0, &err);/* TODO: check err */
        printk("allocated skb=%p\n", skb);
        skb->mac.raw = skb->data;
            skb_reserve(skb, ETHER_HEADER_SIZE);
        
        skb->h.raw = skb->nh.raw = 
            skb_put(skb,SAMPLE_PACKET_SIZE - ETHER_HEADER_SIZE);

        printk("dev->addr_len=%d\n",dev->addr_len );
        printk("dev->dev_addr=%s\n", HEXDUMP(dev->dev_addr, 6));
        
        skb->protocol = htons(PF_NDAS);
        skb->dev = dev;

        if(dev->hard_header)
            dev->hard_header(skb, dev, ETH_NDAS, dest_node, NULL, skb->len);

        printk("packet=%s\n", HEXDUMP(skb->mac.raw, 30));
        dev_queue_xmit(skb);
        printk("sent\n");
        v_sleep(10000);
    }
}

static void create_socket(unsigned long arg) {

    int err;
    printk("create_socket\n");
    v_sleep(1000);
    err = sock_create(PF_NDAS, SOCK_TYPE_SAMPLE, 0,&sock); /* TODO: check err */
    printk("allocated socket=%p\n", sock);
    v_sleep(1000);
    v_sleep(10000);
    send_packet(0);

}


int main(void) 
{
    ptype.type = htons(ETH_NDAS);
    ptype.func = rx_handler;
    family.family = PF_NDAS;
    family.create = mycreate;
    sock_register(&family);
    printk("net proto family %d registered\n", PF_NDAS);
    
    dev_add_pack(&ptype);
    printk("packet type %d registered\n", PF_NDAS);
    dev = dev_get_by_name("eth0");
    printk("dev=%p\n", dev);
    
    v_sleep(10000);
    create_socket(0);
    return 0;
}

int test_init(void) 
{
    printk("%s %s initializing\n", __DATE__ , __TIME__);
    return main();
}

void test_exit(void) 
{    
    sock_release(sock);
    dev_remove_pack(&ptype);
    sock_unregister(PF_NDAS);
}
module_init(test_init);
module_exit(test_exit);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,0))
EXPORT_NO_SYMBOLS;
#endif
