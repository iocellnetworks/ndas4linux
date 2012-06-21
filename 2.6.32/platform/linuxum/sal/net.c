#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <asm/types.h>
#include <sys/types.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h> 
#include <linux/if_arcnet.h> 
#include <linux/version.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include "xplatcfg.h"
#include "sal/net.h"
#include "sal/debug.h"
#include "sal/sync.h"
#include "sal/types.h"
#include "sal/mem.h"
#include "sal/thread.h"

#ifdef DEBUG
#define dbgl_salnet(l, x...) do { \
    if ( l <= DEBUG_LEVEL_SAL_NET ) {\
        sal_debug_print("SN|%d|",l);\
        sal_debug_println(x); \
    }\
} while(0)
#define debug_sal_net_packet(l, x...) do { \
    if ( l <= DEBUG_LEVEL_SAL_NET_PACKET ) {\
        sal_debug_print("SP|%d|",l);\
        sal_debug_println(x); \
    }\
} while(0)
#else
#define dbgl_salnet(l, x...) do {} while(0)
#define debug_sal_net_packet(l, x...) do {} while(0)
#endif

#define MAX_PACKET_LENGTH 1601 /* set to strange value for easy of trace */

typedef struct _SAL_NET {
    struct _SAL_NET* next;
    char    devname[SAL_NET_MAX_DEV_NAME_LENGTH];
    char    mac[6]; /* MAC address of this interface */
    int ifindex;
    int sockfd; /* Linux PF_PACKET socket */
    int is_bound;    /* TRUE if interface is bound */
    unsigned short proto; /* Protocol ID in Network order */
    sal_net_rx_proc             rx_handler;
    sal_thread_id    rxpollthread; /* RX polling thread */
    int destroy;
    int use_virtual_mac; /* Used to change my MAC address to fake MAC. Used for netdisk emulation.*/
    char virtual_mac[6];
} SAL_NET;

struct linuxum_platform_buf {
    int ref;
    char buf[1];
};

static SAL_NET* v_sal_net_list = NULL;
static sal_semaphore v_sal_net_list_lock = SAL_INVALID_SEMAPHORE;
static int v_platform_buf_count = 0;
static int v_platform_buf_max_count = 0;

static SAL_NET* _sal_find_next_desc(sal_netdev_desc nd)
{
    return (SAL_NET*) nd;
}

static void _sal_free_sal_net(SAL_NET* sn)
{
    SAL_NET* ite;
    int ret;

    do {
        ret = sal_semaphore_take(v_sal_net_list_lock, SAL_SYNC_FOREVER);
    } while (ret == SAL_SYNC_INTERRUPTED);

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

#if 0
#define PACKET_DROP_RATE 1000
static int v_packet_drop_count;
#endif 

static void* _sal_net_recv_thread(void* data)
{
    SAL_NET *sn = (SAL_NET*) data;
    fd_set fds;
    struct timeval t;
    int i;
    int ret;
    sal_net_buf_seg segbuf;
    sal_net_buff pbuf;
    char drop_buf[1600];
    while(1) {
        FD_ZERO(&fds);    
        FD_SET(sn->sockfd, &fds);
        t.tv_sec = 0;
        t.tv_usec = 50*1000; /* 50ms */
        i = select(FD_SETSIZE, &fds, NULL, NULL, &t);
        if (i>0 && sn->rx_handler) 
        {
#ifdef PACKET_DROP_RATE
            if (v_packet_drop_count==PACKET_DROP_RATE) {
                sal_error_print("Dropping packet\n");
                pbuf = sal_net_alloc_platform_buf(NULL,MAX_PACKET_LENGTH, &segbuf);
                ret = recv(sn->sockfd, &((struct linuxum_platform_buf*) pbuf)->buf[0], 
			sal_net_len_of_buf_seg(&segbuf), 0);
                sal_net_free_platform_buf(pbuf);
                v_packet_drop_count = 0;    
                continue;
            }
            v_packet_drop_count++;
#endif

            pbuf = sal_net_alloc_platform_buf(NULL, 16, MAX_PACKET_LENGTH, &segbuf);
            if (pbuf==NULL) {
                recv(sn->sockfd, drop_buf, sizeof(drop_buf), 0);
                ret =0;
            } else {
                ret = recv(sn->sockfd, &((struct linuxum_platform_buf*) pbuf)->buf[0], 
			sal_net_len_of_buf_seg(&segbuf), 0);
            }
            if (ret>0) {
                segbuf.data_segs[0].len = ret - segbuf.mac_header.len - segbuf.network_header.len;

#ifdef NDAS_DEBUG_DROP_BROADCAST                
                if ( 0 == memcmp(segbuf.mac_header.ptr, 
                                "\xFF\xFF\xFF\xFF\xFF\xFF", SAL_ETHER_ADDR_LEN) ) 
                { 

                    continue;
                }
#endif
#if 0
                if ( 0 != memcmp(segbuf.mac_header.ptr, 
                                "\xFF\xFF\xFF\xFF\xFF\xFF", SAL_ETHER_ADDR_LEN) ) {
                    dbgl_salnet(1,"rx(%utick):%d", sal_get_tick()%100000,ret);
                    dbgl_salnet(1,"seg=%s", SAL_DEBUG_HEXDUMP(segbuf.segs[0].ptr, segbuf.segs[0].len));
                    {
                        int i = 0; 
                        printf("Recv %d: ", ret);
                        for (i= 0; i < segbuf.segs[0].len;i++) {
                            printf("%02x", (((char*)segbuf.segs[0].ptr)[i]) & 0x0ff);
                        }
                        printf("\n");
                    }
                }
#endif                
                dbgl_salnet(9,"rx(%u tick):%d", sal_get_tick()%100000,ret);
                dbgl_salnet(9,"seg=%s", SAL_DEBUG_HEXDUMP(segbuf.data_segs[0].ptr, 64));    
                sn->rx_handler((sal_netdev_desc)sn, pbuf, &segbuf);                    
            }
        }
        if (i<0) {
            break; /* Maybe net down event?? */
        }
        if (sn->destroy) {
            break;
        }
    }
    sn->rxpollthread = 0;
//    dbgl_salnet(1, "Exiting _sal_net_recv_thread\n");
    sal_thread_exit(0);
    return 0;
}

int sal_netdev_set_address(sal_netdev_desc nd, unsigned char* mac)
{
    SAL_NET* sn;
    sn = _sal_find_next_desc(nd);
    if (!sn) {
        return -1;    
    }
    
    sn->use_virtual_mac = 1;
    memcpy(sn->virtual_mac, mac, 6);
    return 0;
}

/* return 0 for OK, negative value for error */
int sal_netdev_get_address(sal_netdev_desc nd, unsigned char* mac)
{
    struct ifreq ifr;
    struct sockaddr myaddr;
    SAL_NET* sn;
    sn = _sal_find_next_desc(nd);
    if (!sn) {
        return -1;    
    }
    
    if (sn->use_virtual_mac) {
        memcpy(mac, sn->virtual_mac, 6);
        return 0;
    }
    /* get hardware address */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, sn->devname, IFNAMSIZ);
    if (ioctl(sn->sockfd, SIOCGIFHWADDR, &ifr) == -1) {
        dbgl_salnet(1, "SIOCGIFHWADDR failed");
        return -1;
    }
    myaddr = ifr.ifr_hwaddr;

    if (myaddr.sa_family == ARPHRD_ETHER) {
        memcpy(mac, myaddr.sa_data, 6);
        return 0;
    } 
    
    dbgl_salnet(1, "Invalid address family");
    return -1;
}

sal_netdev_desc sal_netdev_open(const char* devname)
{
    int ret;
    SAL_NET* psn;
    int sockfd;
    sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sockfd == -1) {
        perror("socket():");
        return SAL_INVALID_NET_DESC;
    }


    /* Now everything OK, 
        create place holder for new net interface */
    psn = (SAL_NET*) sal_malloc(sizeof(SAL_NET));
    memset(psn, 0, sizeof(SAL_NET));
    strcpy(psn->devname, devname);
    
    psn->is_bound = FALSE;
    psn->sockfd = sockfd;

    ret = sal_netdev_get_address((sal_netdev_desc)psn, (unsigned char*)psn->mac);
    if (ret!=0) {
        sal_free(psn);
        psn = NULL;
        goto errout;
    }
    /* Link to net list */
    do {
        ret = sal_semaphore_take(v_sal_net_list_lock, SAL_SYNC_FOREVER);
    } while (ret == SAL_SYNC_INTERRUPTED);

    psn->next = v_sal_net_list;
    v_sal_net_list = psn;
    
    sal_semaphore_give(v_sal_net_list_lock);

errout:
    return (sal_netdev_desc)psn;
}

int sal_netdev_close(sal_netdev_desc nd)
{
    SAL_NET* sn;
    dbgl_salnet(1, "sal_netdev_close starting");
    sn = _sal_find_next_desc(nd);
    if (!sn) {
        return -1;    
    }
    sn->destroy = TRUE;
    while(1) {
        if (sn->rxpollthread==0)
            break;
        else
            sal_msleep(100);
    }
    close(sn->sockfd);
    _sal_free_sal_net(sn);
    dbgl_salnet(1, "sal_netdev_close succeeded");
    return 0;
}

int sal_net_init(void)
{
    v_sal_net_list_lock = sal_semaphore_create("sal_net_list", 1, 1);
    sal_assert(v_sal_net_list_lock!=SAL_INVALID_SEMAPHORE);
    return 0;
}
void sal_net_cleanup(void)
{
    sal_semaphore_destroy(v_sal_net_list_lock);
    if (v_platform_buf_count!=0) {
        sal_debug_print("v_platform_buf_count=%d\n", v_platform_buf_count);
    }
    sal_debug_print("v_platform_buf_max_count=%d\n", v_platform_buf_max_count);
}

int sal_netdev_get_mtu(sal_netdev_desc nd)
{
    struct ifreq ifr;    
    SAL_NET* sn;    
    sn = _sal_find_next_desc(nd);
    if (!sn) {
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, sn->devname, IFNAMSIZ);    
    if (ioctl(sn->sockfd, SIOCGIFMTU, &ifr) == -1) {
        return -1;    
    }
    return ifr.ifr_mtu;
}

/**
 * register protocol
 */
int sal_netdev_register_proto(sal_netdev_desc nd, xuint16 protocolid, sal_net_rx_proc rxhandler)
{
    SAL_NET* sn;
    struct sockaddr_ll sll;
    int i;
    struct ifreq ifr;
    int ifindex;
    unsigned char buf[2048];
    
    sn = _sal_find_next_desc(nd);
    if (!sn) {
        return -1;    
    }
    
    /* get interface index number */
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, sn->devname, IFNAMSIZ);
    if (ioctl(sn->sockfd, SIOCGIFINDEX, &ifr) == -1) {
        perror("SIOCGIFINDEX");
        return -1;
    }
    sn->ifindex = ifindex = ifr.ifr_ifindex;

    if (sn->use_virtual_mac) {
        /* Setting promisc mode */
        memset(&ifr, 0, sizeof(ifr));
        strncpy(ifr.ifr_name, sn->devname, IFNAMSIZ);
        ioctl(sn->sockfd,SIOCGIFFLAGS,&ifr);
        ifr.ifr_flags|=IFF_PROMISC;
        ioctl(sn->sockfd,SIOCSIFFLAGS,&ifr);
    }
        
    memset(&sll, 0xff, sizeof(sll));
    sll.sll_family = AF_PACKET;    /* allways AF_PACKET */
    sll.sll_protocol = protocolid;
    sll.sll_ifindex = ifindex;
    if (bind(sn->sockfd, (struct sockaddr *)&sll, sizeof sll) == -1) {
        perror("bind():");
        return -1;
    }
    sn->proto = protocolid;
    /* 
     * flush all received packets. 
     *
     */
    do {
        fd_set fds;
        struct timeval t;
        FD_ZERO(&fds);    
        FD_SET(sn->sockfd, &fds);
        memset(&t, 0, sizeof(t));
        i = select(FD_SETSIZE, &fds, NULL, NULL, &t);
        if (i > 0)
            recv(sn->sockfd, buf, i, 0);
    } while (i);

    sn->rx_handler = rxhandler;

    sal_thread_create(&sn->rxpollthread, "netrecvthread", 3, 0, _sal_net_recv_thread, (void*)sn);    
    return 0;
}

int sal_netdev_unregister_proto(sal_netdev_desc nd, xuint16 protocolid)
{
    // TODO refactor this.
    return 0;
}

//static int alloced;
/* to do: use memory pool for platform buffer */
sal_net_buff sal_net_alloc_platform_buf(
    sal_sock nd, xuint32 nethdr_len, xuint32 data_len, sal_net_buf_seg* bufs)
{
    struct linuxum_platform_buf* buf;
#if 0 /* platform buffer allocation fail simulation */
    static int drop_count=0;
    drop_count++;
    if (drop_count%200==0) {
        sal_debug_print("Platform buf alloc fail\n");
        return (sal_net_buff ) NULL;
    }
#endif
    buf = sal_malloc(SAL_ETHER_HEADER_SIZE + nethdr_len + data_len + 4);
    if (!buf)
        sal_assert(0); /* out of memory */
    buf->ref = 1;

    bufs->mac_header.ptr = &(buf->buf[0]);
    bufs->mac_header.len = SAL_ETHER_HEADER_SIZE; 
    bufs->network_header.ptr = &(buf->buf[0]) + SAL_ETHER_HEADER_SIZE;
    bufs->network_header.len = nethdr_len;
    bufs->nr_data_segs = 1;
    bufs->data_segs[0].ptr = ((char*)bufs->network_header.ptr) + nethdr_len;
    bufs->data_segs[0].len = data_len;
    v_platform_buf_count++;
    if (v_platform_buf_count>v_platform_buf_max_count)
        v_platform_buf_max_count= v_platform_buf_count;
    return (sal_net_buff)buf;
}

int sal_net_free_platform_buf(sal_net_buff buf)
{
    struct linuxum_platform_buf* lpf = (struct linuxum_platform_buf*)buf;
    sal_assert(lpf->ref>=1 && lpf->ref<16);
    lpf->ref--;
    v_platform_buf_count--;
    if (lpf->ref==0) {
        sal_free(buf);
    }
    return 0;
}

sal_net_buff sal_net_clone_platform_buf(sal_net_buff buf)
{
    struct linuxum_platform_buf* lpb = (struct linuxum_platform_buf*)buf;
    v_platform_buf_count++;
    lpb->ref++;
    return buf;
}

sal_net_buff sal_net_copy_platform_buf(sal_net_buff srcbuf, xuint32 neth_len, xuint32 data_len, sal_net_buf_seg* bufs)
{
	sal_net_buff snbuf;
	struct linuxum_platform_buf	* lpb;
	snbuf = sal_net_alloc_platform_buf(0, neth_len, data_len, bufs);
	lpb = (struct linuxum_platform_buf *) snbuf;

	memcpy(lpb->buf, ((struct linuxum_platform_buf *)srcbuf)->buf, sal_net_len_of_buf_seg(bufs));

	return snbuf;
}

int sal_ether_send(sal_netdev_desc nd, sal_net_buff buf, char* dest, xuint32 len)
{
    SAL_NET* sn;
    struct sockaddr_ll sll = {0};
    int c=0;
    char *data = &(((struct linuxum_platform_buf*) buf)->buf[0]);
/*
    static int first_packet = 1;
    if (first_packet) {
        sal_debug_print("Discarding first packet\n");
        sal_net_free_platform_buf(buf);
        first_packet =0;
        return 0;
    }
*/
    dbgl_salnet(1, "sal_ether_send(%u tick): len=%d", sal_get_tick()%100000, len);
    debug_sal_net_packet(1, "%s", SAL_DEBUG_HEXDUMP(&(((struct linuxum_platform_buf*) buf)->buf[0]),len));

#if 0
    {
        int i = 0; 
        printf("Sending %d:", len);
        for (i= 0; i < len;i++) {
            printf("%02x", (data[i]) & 0x0ff);
        }
        printf("\n");
    }
#endif    
    sn = _sal_find_next_desc(nd);
    if (!sn) {
        return -1;
    }


    /* only sll_ifindex is used */
    sll.sll_ifindex = sn->ifindex;
    
    c = sendto(sn->sockfd, (void*) data, len, 0, (struct sockaddr *)&sll, sizeof(sll));
    if (c<=0)
        perror("sendto:");
    sal_net_free_platform_buf(buf);
    return c;
}

sal_sock sal_sock_create(int type, int protocol, int mts)
{
    return (sal_sock)1;
}

void sal_sock_destroy(sal_sock sock) {}
