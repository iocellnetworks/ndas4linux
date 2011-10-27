/*
 -------------------------------------------------------------------------
 Copyright (c) 2005, XIMETA, Inc., IRVINE, CA, USA.
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
*/
#ifndef _SAL_NET_H_
#define _SAL_NET_H_
#include "sal/sal.h"
#include "sal/mem.h" /* sal_mem_block */
#ifdef LINUXUM
#include <netinet/in.h>
#endif

#ifdef PS2DEV
#ifndef _PS2
#include <types.h>
#include <tcpip.h>
#endif
#endif

#if defined(SYABAS) || defined(_PS2) 
#define XPLAT_BUILTIN_NETWORK_BYTE_UTIL
#endif

#ifdef XPLAT_BUILTIN_NETWORK_BYTE_UTIL
#ifdef __LITTLE_ENDIAN_BYTEORDER
#define htons(x) ({\
    xuint16 __x = (x); \
    ((xuint16) \
        ( \
            ( ( ((xuint16)(__x)) & (xuint16)0x00ffU) << 8) | \
            ( ( ((xuint16)(__x)) & (xuint16)0xff00U) >> 8) \
        ) \
    ); \
})
#define ntohs(x) htons(x)
#define ntohl(x) ({\
    xuint32 __x = (x); \
        ((xuint32)( \
        ((((xuint32)(__x)) & (xuint32)0x00ffU) << 24) | \
        ((((xuint32)(__x)) & (xuint32)0xff00U) << 8) | \
        ((((xuint32)(__x)) & (xuint32)0xff0000U) >> 8) | \
        ((((xuint32)(__x)) & (xuint32)0xff000000U) >> 24) )); \
})
#define htonl(x) ntohl(x)
#else
#define htons(x) (x)
#define ntohs(x) (x)
#define htonl(x) (x)
#define ntohl(x) (x)
#endif /* __LITTLE_ENDIAN_BYTEORDER */
#endif /* XPLAT_BUILTIN_NETWORK_BYTE_UTIL */

#include "sal/time.h"
#include "sal/types.h"
#include "sal/mem.h"

typedef void* sal_netdev_desc;

/* ID of platform-dependent network buffer such as sk_buff 
    This ID is used for freeing the buffer
*/
typedef void* sal_net_buff; 

/**
 * ID of platform-dependent network socket such as struct sock, and etc.
 * only used for linux kernel driver implementation.
 */
typedef void* sal_sock;
    
#define SAL_INVALID_NET_DESC     ((sal_netdev_desc) 0)

#define SAL_NET_MAX_DEV_NAME_LENGTH 32
#define SAL_ETHER_HEADER_SIZE             14
#define SAL_ETHER_ADDR_LEN            6
#define SAL_LPX_HEADER_RESERVE  16

#define SAL_NET_MAX_BUF_SEG_COUNT    4

typedef struct _sal_ether_header_s {
    xuchar    des[SAL_ETHER_ADDR_LEN];
    xuchar    src[SAL_ETHER_ADDR_LEN];
    xuint16     proto; /* Protocol ID in network byte order */
} __attribute__((packed)) sal_ether_header;

/*
 * Abstracted representation of the network packet data buffer for Sal implementation layer.
 * The structure has the field
 * collection of the pointers to platform dependant network buffer 
 * Objective : accessing platform-depend network buffer data
 *                 without calling platform-depend netowrk buffer related APIs
 *             --> reduce the number of fuctions to be port.
 * @private private data for SAL implemetation layer.
 */

#define SAL_NETBUFSEG_FLAG_FOLLOWING_PACKET_EXIST   0x0001
 
typedef struct _sal_net_buf_seg_s {
    struct sal_mem_block mac_header;    /* Used only by packet receive */
    struct sal_mem_block network_header;
    xuint16    nr_data_segs;    /* Number of segs */
    xuint16    packet_flags;
    xuint32    data_len;   /* Length of data */
    struct sal_mem_block data_segs[SAL_NET_MAX_BUF_SEG_COUNT];
} sal_net_buf_seg;

static inline int sal_net_len_of_buf_seg(sal_net_buf_seg* bufs) {
	int i;
	int len = bufs->mac_header.len + bufs->network_header.len;
	for(i=0;i<bufs->nr_data_segs;i++) {
		len += bufs->data_segs[i].len;
	}
	return len;
}

/* 
If "nbuf" is not NULL, NDAS core will call sal_free_platform_buf with nbuf after using the buffer
*/
typedef void (*sal_net_rx_proc)(sal_netdev_desc nd, sal_net_buff nbuf, sal_net_buf_seg* bufs) NDAS_CALL;

NDAS_SAL_API extern sal_netdev_desc sal_netdev_open(const char* devname);
NDAS_SAL_API extern int sal_netdev_close(sal_netdev_desc nd);
NDAS_SAL_API extern int sal_netdev_get_mtu(sal_netdev_desc nd);

/**
 * register the protocol type by the given protocol-id.
 * On the packet arrived to the specified network device(by nd) 
 * the handler function (rxhandler) will be called.
 * @nd the network interface
 * @protocolid the type of ether packet in network byte ored
 * @rxhandler the func will be called on the packet arrived
 * @return 0 for success, negative when error or no such device
 */
NDAS_SAL_API extern int sal_netdev_register_proto(sal_netdev_desc nd, xuint16 protocolid, sal_net_rx_proc rxhandler);
/**
 * unregister the protocol type by the given protocol-id.
 * the fuction registered by sal_netdev_register_proto will not be called 
 * on the packet arrived at the network device specified by nd.
 * @nd the network interface
 * @protocolid the type of ether packet in network byte ored
 * @return 0 for success, negative when error or no such device
 */
NDAS_SAL_API extern int sal_netdev_unregister_proto(sal_netdev_desc nd, xuint16 protocolid);

/**
 * Allocate a platform network buffer
 */
NDAS_SAL_API extern sal_net_buff sal_net_alloc_platform_buf(sal_sock ns, xuint32 neth_len, xuint32 len, sal_net_buf_seg* bufs);
NDAS_SAL_API extern sal_net_buff sal_net_alloc_platform_buf_ex(sal_sock ns, xuint32 neth_len, xuint32 len, sal_net_buf_seg* bufs, int noblock);

/**
 * Free a platform network buffer
 */
NDAS_SAL_API extern int sal_net_free_platform_buf(sal_net_buff buf);

/**
 * clone a platform network buffer
 */
NDAS_SAL_API extern sal_net_buff sal_net_clone_platform_buf(sal_net_buff buf);

/**
 * copy a platform network buffer
 */
NDAS_SAL_API extern sal_net_buff sal_net_copy_platform_buf(sal_net_buff buf, xuint32 neth_len, xuint32 data_len, sal_net_buf_seg* bufs);

/**
 * Is a platform network buffer shared?
 */
NDAS_SAL_API extern int sal_net_is_platform_buf_shared(sal_net_buff buf);


#if 0
/**
 * Increase a reference count on a platform network buffer
 */
NDAS_SAL_API extern sal_net_buff sal_net_get_platform_buf(sal_net_buff buf);
#endif

/**
 * Send the packet - 'buf' over the network interface - nd.
 * 
 * @nd the network interface
 * @buf the network buffer packet to be sent
 * @len the size of the buffer to be sent
 * @return the size sent
 */
NDAS_SAL_API extern int sal_ether_send(sal_netdev_desc nd, sal_net_buff buf, char* node, xuint32 len);

/* APIs is not used for now */
//NDAS_SAL_API extern int sal_ether_recv(sal_netdev_desc nd, sal_net_buff buf );

/**
 * Create system implementation socket object.
 * Some platform need the system socket object to allocate the network buffer.
 * In such a case, the object created by sal_sock_create will be passed to 
 *  sal_net_alloc_platform_buf function call.
 * The most of platforms can just return non-NULL.
 * @type
 * @protocol
 * @mts Maximum transfer size. Hint system about proper size about buffer for this socket.
 * Return : NULL on fail to create socket
 *          non-NULL if success
 */
NDAS_SAL_API extern sal_sock sal_sock_create(int type, int protocol, int mts);
/**
 * Destroy system implementation socket.
 */
NDAS_SAL_API extern void sal_sock_destroy(sal_sock);

/**
 * Initialize the system specific initialization jobs.
 * This function is called when sal_init is called.
 */
NDAS_SAL_API extern int sal_net_init(void);

/**
 * clean up resources allocated by sal_net_init.
 * This function is called when sal_clean is called.
 */
NDAS_SAL_API extern void sal_net_cleanup(void);

/* Return 0 for success */
NDAS_SAL_API extern int sal_netdev_get_address(sal_netdev_desc nd, unsigned char* mac);
NDAS_SAL_API extern int sal_netdev_set_address(sal_netdev_desc nd, unsigned char* mac);

typedef enum {
    SAL_NET_EVENT_DOWN,
    SAL_NET_EVENT_UP
} sal_net_change_event;

typedef ndas_error_t (*sal_net_change_handler_func)(sal_net_change_event /*event*/, char* /*devname*/);
NDAS_SAL_API void sal_net_set_change_handler(sal_net_change_handler_func handler);
   
typedef NDAS_SAL_API ndas_error_t (*sal_net_buf_sender_func)(void* /*context*/, sal_net_buff /*nbuff*/, sal_net_buf_seg* /*bseg*/);

NDAS_SAL_API int sal_process_page_to_net_buf_mapping(
    void* context, 
    sal_sock system_sock, 
    int nh_len, 
    int mss,
    sal_net_buf_sender_func shandler, 
    sal_page** pages, 
    unsigned long poffset, 
    unsigned long psize
);

NDAS_SAL_API
struct sk_buff *
sal_net_alloc_buf_bufseg(
    sal_sock system_sock,
    sal_net_buf_seg *bufseg
);


#endif /* _SAL_NET_H_ */
