#ifndef _NDAS_LPX_H_
#define _NDAS_LPX_H_

#define __LPX__		1

#include <linux/types.h>
#include <linux/socket.h>

#define LPX_NODE_LEN	6
#define LPX_MTU			576

#define ETH_P_LPX		0x88ad

#ifndef AF_LPX
#define	AF_LPX          29      /* NetKingCall LPX */ 
#ifndef PF_LPX
#define PF_LPX          AF_LPX
#endif
#endif

#define LPX_TYPE_DATAGRAM	0x2
#define LPX_TYPE_STREAM		0x3	

#define LPX_IOCTL_DEL_TIMER		0x00000001

struct sockaddr_lpx {

	sa_family_t		slpx_family;
	__u16			slpx_port;
	unsigned char 	slpx_node[LPX_NODE_LEN];
	__u8			slpx_type;
	unsigned char	slpx_zero;	/* 12 byte fill */
};

#ifdef __KERNEL__

#include <linux/version.h>
#include <linux/module.h> 

#include <linux/sockios.h>
#include <linux/socket.h>

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

//#include <ndasuser/ndaserr.h>

/*
 * So we can fit the extra info for SIOCSIFADDR into the address nicely
 */
#define slpx_special	slpx_port
#define slpx_action		slpx_zero
#define LPX_DLTITF	0
#define LPX_CRTITF	1

#if !__LPX__
struct lpx_route_definition {

	__be32        lpx_network;
	__be32        lpx_router_network;
	unsigned char lpx_router_node[LPX_NODE_LEN];
};
#endif

struct lpx_interface_definition {

	__be32        lpx_network;
	unsigned char lpx_device[16];

	unsigned char lpx_dlink_type;
#define LPX_FRAME_NONE		0
#define LPX_FRAME_SNAP		1
#define LPX_FRAME_8022		2
#define LPX_FRAME_ETHERII	3
#define LPX_FRAME_8023		4
#define LPX_FRAME_TR_8022       5 /* obsolete */

	unsigned char lpx_special;
#define LPX_SPECIAL_NONE	0
#define LPX_PRIMARY			1
#define LPX_INTERNAL		2

	unsigned char lpx_node[LPX_NODE_LEN];
};
	
struct lpx_config_data {

	unsigned char	lpxcfg_auto_select_primary;
	unsigned char	lpxcfg_auto_create_interfaces;
};

#if !__LPX__

/*
 * OLD Route Definition for backward compatibility.
 */

struct lpx_route_def {

	__be32		lpx_network;

	__be32		lpx_router_network;
#define LPX_ROUTE_NO_ROUTER	0

	unsigned char	lpx_router_node[LPX_NODE_LEN];
	unsigned char	lpx_device[16];

	unsigned short	lpx_flags;
#define LPX_RT_SNAP		8
#define LPX_RT_8022		4
#define LPX_RT_BLUEBOOK		2
#define LPX_RT_ROUTED		1
};

#endif

#define SIOCALPXITFCRT		(SIOCPROTOPRIVATE)
#define SIOCALPXPRISLT		(SIOCPROTOPRIVATE + 1)
#define SIOCLPXCFGDATA		(SIOCPROTOPRIVATE + 2)
#define SIOCLPXNCPCONN		(SIOCPROTOPRIVATE + 3)

#define LPX_BROADCAST_NODE    "\xFF\xFF\xFF\xFF\xFF\xFF"

typedef struct _lpx_iocb {

    struct socket *sock;
    void* buf;
    /* size of received data */
    int nbytes;
    ndas_error_t result;

} lpx_siocb;

#ifndef __AF_LPX__

ndas_error_t lpx_sock_create(int lpxtype, int protocol, struct socket **sock);
void lpx_sock_close(struct socket *sock);

ndas_error_t lpx_sock_bind(struct socket *sock, struct sockaddr_lpx* addr, int addrlen);
ndas_error_t lpx_sock_connect(struct socket *sock, const struct sockaddr_lpx *serv_addr, int addrlen);

ndas_error_t lpx_sock_listen(struct socket *sock, int backlog);
ndas_error_t lpx_sock_accept(struct socket *sock, struct socket **newsock, struct sockaddr_lpx *cli_addr, unsigned int *addrlen, int flags);

ndas_error_t lpx_sock_send(struct socket *sock, void* msg, int len, int flags);
ndas_error_t lpx_sock_recv(struct socket *sock, void* buf, int len, int flags);

ndas_error_t lpx_sock_sendto(struct socket *sock, void* buf, int len, int flags, struct sockaddr_lpx *to, int tolen);
ndas_error_t lpx_sock_recvfrom(struct socket *lpxfd, void* buf, int len, int flags, struct sockaddr_lpx *from, int fromlen);

ndas_error_t lpx_sock_sendmsg(struct socket *sockfd, struct iovec *blocks, size_t nr_blocks, size_t len, int flags);
ndas_error_t lpx_sock_recvmsg(struct socket *sockfd, struct iovec *blocks, size_t nr_blocks, size_t len, int flags);

typedef void (*lpx_sio_handler)(lpx_siocb* cb, struct sockaddr_lpx *from, void* arg);

ndas_error_t lpx_is_connected(struct socket *sock);

#define SIO_FLAG_MULTIPLE_NOTIFICATION 0x1

ndas_error_t 
lpx_sio_recvfrom(struct socket *sock, void* buf, int len, int flags, 
				 lpx_sio_handler handler, void* user_arg, unsigned long timeout);

struct net_device *lpxitf_find_first(void);
int lpxitf_iterate_address(void (*func)(unsigned char*, void*), void* arg); 

#define LPX_ADDR_INIT(addr_lpx, n, port) do { \
    struct sockaddr_lpx *__addr__ = addr_lpx; \
    unsigned short __port__ = port; \
    __u8 *__node__ = (__u8*)n; \
    __addr__->slpx_family = PF_LPX; \
    if ( __node__ ) \
        memcpy(__addr__->slpx_node, __node__, LPX_NODE_LEN); \
    else  \
        memset(__addr__->slpx_node, 0, LPX_NODE_LEN); \
    __addr__->slpx_port = htons(__port__); \
} while(0) 

#endif

#ifdef DEBUG

/**
 * Transform LPX node into string.
 * DEBUG purpose
 */
#define DEBUG_LPX_NODE(node) ({\
    static char buf[LPX_NODE_LEN*3]; \
    if ( node ) \
        snprintf(buf, sizeof(buf), \
            "%02x:%02x:%02x:%02x:%02x:%02x", \
            ((char*)node)[0] & 0xff, \
            ((char*)node)[1] & 0xff, \
            ((char*)node)[2] & 0xff, \
            ((char*)node)[3] & 0xff, \
            ((char*)node)[4] & 0xff, \
            ((char*)node)[5] & 0xff); \
    else \
        snprintf(buf, sizeof(buf), "NULL"); \
    (const char *) buf; \
})

#else //DEBUG
#define DEBUG_LPX_NODE(node) ({ ""; })
#endif // DEBUG    

#endif

#endif /* _LPX_H_ */
