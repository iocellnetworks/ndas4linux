/**********************************************************************
 * Copyright ( C ) 2012 IOCELL Networks
 * All rights reserved.
 **********************************************************************/
/* This is the internal source file of IOCELL Networks 
   If you get this header file without any permission of IOCELL Networks
   Please let us know linux@iocellnetworks.com
*/
/* Non Public APIs of LPX Protocol */

#ifndef _LPX_LPXUTIL_H_
#define _LPX_LPXUTIL_H_

#include "xplatcfg.h"
#include "sal/libc.h" // sal_snprintf
#include "sal/types.h"
#include "sal/net.h" // sal_sock
#include "sal/time.h" // sal_msec
#include "sal/net.h" // sal_msec
#include "sal/debug.h" // sal_snprintf
#include "sal/sync.h" // sal_spinlock
#include "lpx/lpx.h"
#include "ndasuser/ndaserr.h"
#include "xlib/dpc.h"
#include "xlib/xbuf.h"
#include "netdisk/list.h"

#define PORT_HASH    16

#define SOMAXCONN       128

#define MIN_MTU_SIZE        1200    /* Device with too small MTU does not work with NDAS. */

#define LPX_MIN_EPHEMERAL_SOCKET    0x4000
#define LPX_MAX_EPHEMERAL_SOCKET    0x7fff

#define LPX_BROADCAST_NODE    "\xFF\xFF\xFF\xFF\xFF\xFF"

#define CACHE_ALIVE            ((xuint32)(10* SAL_TICKS_PER_SEC)) /* in msec */
#define CACHE_TIMEOUT        (5* SAL_TICKS_PER_SEC) /* in msec */

#define SMP_TIMEOUT        (SAL_TICKS_PER_SEC/20)  /* 50 ms */
/* #define SMP_TIMEOUT        (SAL_TICKS_PER_SEC/100)  */
#define SMP_DESTROY_TIMEOUT    (SAL_TICKS_PER_SEC * 2)
#define TIME_WAIT_INTERVAL    (SAL_TICKS_PER_SEC)
#define ALIVE_INTERVAL      (SAL_TICKS_PER_SEC)
#define SMP_TIMER_RETRY_TIMEOUT    ((SAL_TICKS_PER_SEC+99)/100)  /* 10ms or minimum tick */

#define RETRANSMIT_TIME        (SAL_TICKS_PER_SEC/5)
#define MAX_RETRANSMIT_DELAY    (SAL_TICKS_PER_SEC)
#define DEFAULT_LOSS_DETECTION_TIME  (RETRANSMIT_TIME*7/8)

#define MAX_ALIVE_COUNT         (5)
#define MAX_RETRANSMIT_TIME        (5 * SAL_TICKS_PER_SEC)

#define LPX_SOCK_RECV_WAITTIME (SAL_TICKS_PER_SEC * 10)    /* disk warm-up can be taken up to 7 sec */ 
#define LPX_SOCK_RECV_MEDTERM_WAITTIME (SAL_TICKS_PER_SEC * 30)
#define LPX_SOCK_RECV_LONGTERM_WAITTME (SAL_TICKS_PER_SEC * 100)


#define LPX_MIN_PACKET_SIZE 60
#define LPX_20PATCH_PACKET_SIZE 60
#define LPX_20PATCH_PACKET_DATA_SIZE (LPX_20PATCH_PACKET_SIZE - sizeof(struct lpxhdr) - 14)

//#define MAX_ACK_DELAY ((xuint32)(RETRANSMIT_TIME/4))
#define MAX_ACK_DELAY ((SAL_TICKS_PER_SEC+99)/100)   /* 10ms or minimum tick. Though delayed ack will be sent by timer */
#define MAX_DELAYED_ACK_COUNT   4   /* Do not use delayed ack for more than this packet count */

struct sockaddr_lpx {
	unsigned short	slpx_family;
	unsigned short	slpx_port;
	unsigned char 	slpx_node[LPX_NODE_LEN];
	unsigned char	slpx_type;
	unsigned char	slpx_zero;	/* 12 byte fill */
} __x_attribute_packed__;;

struct lpx_sock;

typedef    struct _lpx_address {
    xuint16    port;    // Big endian
    xuint8    node[LPX_NODE_LEN];
} __x_attribute_packed__ lpx_address;

/*
 Data structures for internal implementations 
*/
struct lpx_bindhead {
    struct lpx_sock*     bind_next;
};

struct lpx_portinfo {
    xuint32        last_alloc_port;
    sal_spinlock    bind_lock; /* Accessed from interrupt context */
    struct lpx_bindhead port_hashtable[PORT_HASH];
};

struct lpx_cache{
    struct lpx_cache * next;
    sal_tick         time;
    unsigned char     dest_addr[LPX_NODE_LEN];
    unsigned char     itf_addr[LPX_NODE_LEN];
};

struct lpx_eth_cache {
    sal_spinlock        cache_lock;    /* Accessed from interrupt context */
    struct lpx_cache *    head;
    dpc_id         cache_timer;
};

typedef struct _lpx_interface {    
    /* handle to interface        */
    struct _lpx_interface    *itf_next;
    sal_netdev_desc            nd;
    sal_spinlock               itf_sklock; /* Not accessed from interrupt context */
    struct lpx_sock         *itf_sklist;
    unsigned int            mss;    /* mtu - size of lpx header */
    unsigned char             itf_node[LPX_NODE_LEN];
    unsigned int             hard_header_len;
    char                    dev[12];
} lpx_interface;

typedef enum {
   LSS_FREE = 0,                          /* not allocated                */
   LSS_UNCONNECTED,                       /* unconnected to any socket    */
   LSS_CONNECTING,                        /* in process of connecting     */
   LSS_CONNECTED,                         /* connected to socket          */
   LSS_DISCONNECTING                      /* in process of disconnecting  */
} lpx_socket_state;

// Stream and datagram options
#define LPX_OPTION_DESTINATION_ADDRESS          ((xuint8)0x01) // Currently not implemented
#define LPX_OPTION_SOURCE_ADDRESS               ((xuint8)0x02) // Currently not implemented
#define LPX_OPTION_DESTINATION_ADDRESS_ACCEPT   ((xuint8)0x04) // Currently not implemented
#define LPX_OPTION_SOURCE_ADDRESS_ACCEPT        ((xuint8)0x08) // Currently not implemented

// Stream only options.
#define LPX_OPTION_PACKETS_CONTINUE_BIT         ((xuint8)0x80)
#define LPX_OPTION_NONE_DELAYED_ACK             ((xuint8)0x40) // Currently not implemented

/* 16 bit align  total  16 bytes */
struct    lpxhdr
{
#if defined(__LITTLE_ENDIAN_BITFIELD)
    union { 
        struct {
            xuint8    pktsizeMsb:6;
            xuint8    type:2; /* LPX_TYPE_xxx */
            xuint8    pktsizeLsb;
        } __x_attribute_packed__ p;
        xuint16    pktsize;
    } __x_attribute_packed__ pu;
#define LPX_TYPE_MASK        ((xuint16)0x00C0)    

#elif defined(__BIG_ENDIAN_BITFIELD)
    union {
        struct {
            xuint8    type:2; /* LPX_TYPE_xxx */
            xuint8    pktsizeMsb:6;
            xuint8    pktsizeLsb;
        } __x_attribute_packed__ p;
        xuint16    pktsize;
    }__x_attribute_packed__ pu;
#define LPX_TYPE_MASK        ((xuint16)0xC000)
#else
#error "Bitfield endian is not specified"
#endif 

    xuint16    dest_port;
    xuint16    source_port;

    union {
        struct {
            xuint16    reserved_r[5];
        } __x_attribute_packed__ r;
            
        struct _datagram_hdr{
            xuint16    message_id;
            xuint16    message_length;
            xuint16    fragment_id;
            xuint16    fragment_length;
            xuint8     reserved_d3;
            xuint8     option;
        } __x_attribute_packed__ d;
        struct _stream_hdr{
            xuint16    lsctl;

/* Lpx Stream Control bits */
#define LSCTL_CONNREQ        0x0001
#define LSCTL_DATA        0x0002
#define LSCTL_DISCONNREQ    0x0004
#define LSCTL_ACKREQ        0x0008
#define LSCTL_ACK        0x1000

            xuint16    sequence;
            xuint16    ackseq;
            xuint8     server_tag;
            xuint8     reserved_s1[2];
            xuint8     option;
        } __x_attribute_packed__ s;
    } __x_attribute_packed__ u;
} __x_attribute_packed__;

    
#define    LPX_HEADER_SIZE    (sizeof(struct lpxhdr))

struct lpx_opt {
    lpx_interface        *interface;
    lpx_address           dest_addr; // 10 bytes
	// 16 bytes
    lpx_address           source_addr; // 10 bytes
    xuint32            virtual_mapping;
	// 32 bytes
};

struct lpx_dgram_opt {
    xuint16                message_id;
    xuint16                receive_data_size;
#ifdef XPLAT_SIO
    sal_spinlock      sio_lock;
    struct list_head    sio_request_queue;
	// 16 bytes
#endif
};

#define SMP_MAX_WINDOW 256

struct lpxstm_retransmits_count {
	sal_atomic tx_retransmit;
	sal_atomic rx_packet_loss;
};

#define    SMP_SENDIBLE        0x0001
#define    SMP_RETRANSMIT_ACKED    0x0002    

struct lpx_stream_opt
{
    void    *owner;

    struct lpx_sock        *child_sklist;
    struct lpx_sock        *parent_sk;

    xuint16     max_window;
    xuint16     fin_seq;
	// 16 bytes

    xuint16     sequence;    /* Host order - our current pkt # */
    xuint16     rmt_seq;
    xuint16     rmt_ack;    /* Host order - last pkt ACKd by remote */
    xuint8      server_tag;

    xuint8      timer_reason;    // SMP_SENDIBLE, SMP_RETRANSMIT_ACKED

    xuint16     last_rexmit_seq;
    xuint16    alive_retries;    /* Number of WD retries */

    int    rexmits;    /* Number of rexmits */
	// 32 bytes

    struct xbuf_queue         rexmit_queue; /* Retransmission packet queue */

    dpc_id      lpx_stream_timer;
    sal_tick    rexmit_timeout;
    sal_tick    alive_timeout;
	// 48 bytes

    sal_tick last_data_received;	/* Time since last data received. Used to detect packet loss */
    sal_tick loss_detection_time; /* Detect as packet loss if this amount of time is passed since last packet */
    struct lpxstm_retransmits_count retransmit_count;
	// 64 bytes

    sal_tick    time_wait_timeout;

    int prevseq; /* Used only for debug use to find missing packet.  */
	// 72 bytes
};

/* Connection state defines */

enum SMP_STATUS {
  SMP_ESTABLISHED = 1,
  SMP_SYN_SENT,
  SMP_SYN_RECV,
  SMP_FIN_WAIT1,
  SMP_FIN_WAIT2,
  SMP_TIME_WAIT,
  SMP_CLOSE,
  SMP_CLOSE_WAIT,
  SMP_LAST_ACK,
  SMP_LISTEN,
  SMP_CLOSING,     /* now a valid state */

  SMP_MAX_STATES /* Leave at the end! */
};
/* Packet transmit types - Internal */
#define DATA        0    /* Data */
#define ACK        1    /* Data ACK */
#define CONREQ    4    /* Connection Request */
#define DISCON    6    /* Informed Disconnect */
#define RETRAN    8    /* Int. Retransmit of packet */
#define ACKREQ    9    /* Int. Retransmit of packet */

#ifdef USE_DELAYED_ACK
#define DELAYED_ACK 10 /* It is okay to send this ACK with delay*/
#endif

struct _lpx_aio;

struct lpx_sock {
    struct lpx_sock*     next;  // interface list
    struct lpx_sock*     next_sibling;  // sibling (that has same parent sk) socket list.    
    struct list_head     all_node; /* all socket list */
	// 16 bytes

    sal_atomic         refcnt;
    int                lpxfd;
    sal_sock           system_sock;
    /* Protect lpx_opt, opt.dgram, opt.stream, state, dgram, stream */
    /* put the packets to the back log quere while lock is acquired */
    sal_spinlock               lock;   /* Accessed from interrupt context. Use irq version of spinlock */
	// 32 bytes

    struct lpx_opt     lpx_opt; // 32 bytes
	// 64 bytes

    union {
        struct lpx_dgram_opt  dgram;
        struct lpx_stream_opt stream;
    } opt;  // 72 bytes
    /* Members from linux struct lpx_sock .. */
    enum SMP_STATUS state; /* Connection state: SMP_xxx */

    unsigned short     type;     /* SOCK_STREAM, SOCK_DGRAM, SOCK_SEQPACKET */
    unsigned short       shutdown:1; /* 1 if shutdown is requested */
    unsigned short       dead:1;
    unsigned short       zapped:1; /* 1 if sk is not linked to network interface */
    unsigned short       destroy:1;
    unsigned short       acceptcon:1; /* 1 if this sk accept connection */
    unsigned short       creationref:1; /* 1 if creation reference exists */
    unsigned short       fixedport:1; /* 1 if the port number is specified at the craeation */
	// 144 bytes

    struct _lpx_aio *aio;
    struct xbuf_queue receive_queue; /* Receive packet queue */ // 20 bytes
    struct xbuf_queue backlog; /* Backlog packet queue */ // 20 bytes
#ifdef USE_DELAYED_RX_NOTIFICATION
    sal_spinlock to_recv_count_lock;
    int    to_recv_count; /* hint for receive. Should be guarded by to_recv_count_lock */
#endif
    struct xbuf_queue write_queue; /* Send packet queue */ // 20 bytes

#ifdef     USE_DELAYED_ACK
    sal_spinlock    delayed_ack_lock; /* protect unsent_ack_count, delayed_ack_xb, last_txed_ack. accessed from interrupt */
    struct xbuf*    delayed_ack_xb;
    sal_tick        last_txed_ack;
    int		        unsent_ack_count;
#endif

    sal_event        sleep;	/* RX ready event */
    sal_event        destruct_event; /* Socket destruction event */

    sal_tick            rtimeout; /* read time out */
    sal_tick            wtimeout; /* write time out */

    ndas_error_t   err;

    struct lpx_sock *     bind_next;

    lpx_socket_state       sock_state; // LSS_FREE, LSS_UNCONNECTED, LSS_CONNECTED

};

static inline 
struct lpx_opt * LPX_OPT(struct lpx_sock *sk)
{
    return &sk->lpx_opt;
}

static inline 
struct lpx_dgram_opt * LPX_DGRAM_OPT(struct lpx_sock *sk) 
{
    return &sk->opt.dgram;
}

static inline 
struct lpx_stream_opt * LPX_STREAM_OPT(struct lpx_sock *sk) 
{
    return &sk->opt.stream;
}

#define LPX_ADDR_INIT(addr_lpx, n, port) do { \
    struct sockaddr_lpx *__addr__ = addr_lpx; \
    unsigned short __port__ = port; \
    xuchar *__node__ = (xuchar*)n; \
    __addr__->slpx_family = PF_LPX; \
    if ( __node__ ) \
        sal_memcpy(__addr__->slpx_node, __node__, LPX_NODE_LEN); \
    else  \
        sal_memset(__addr__->slpx_node, 0, LPX_NODE_LEN); \
    __addr__->slpx_port = g_htons(__port__); \
} while(0) 


#ifdef XPLAT_SIO

typedef struct _lpx_iocb {
    int filedes;
    void* buf;
    /* size of received data */
    int nbytes;
    ndas_error_t result;
} lpx_siocb;
/* */
typedef void (*lpx_sio_handler)(lpx_siocb* cb, struct sockaddr_lpx *from, void* arg);

/* INTERNAL STRUCTION FOR */
typedef struct _lpx_sio_ {
    lpx_siocb           cb;
    // 16 bytes
    struct list_head    node;
    sal_atomic          refcnt;
    int                 keep_notifying;
    // 32 bytes
    struct lpx_sock    *sock;
    lpx_sio_handler     func;
    void               *user;
    dpc_id              notify_bpc;
    // 48 bytes
    dpc_id              timeout_timer;
} lpx_siocb_i;
#endif

/*
 * Asynchronous IO structures.
 */

struct _lpx_aio;
typedef void (*lpx_aio_complete)(struct _lpx_aio * aio, void *userparam);
typedef int (*lpx_aio_head_lookahead)(
    struct _lpx_aio * aio,
    void *userparam,
    void *recvdata,
    int recvdata_len,
    int *request_len_adjustment);
typedef int (*lpx_aio_user_copy)(
    struct _lpx_aio * aio,
    void *recvdata,
    int recvdata_len);

#define LPXAIO_FLAG_COMPLETED           0x8000
#define AIO_FLAG_COMMON_IO		0x0000
#define AIO_FLAG_MEDTERM_IO		0x0002
#define AIO_FLAG_LOGTERM_IO		0x0003

typedef struct _lpx_aio {
    int sockfd;
    xuint16 aio_flags;
    xuint16 nr_blocks; // Number of blocks.
    union {
        void *buf;
        struct sal_mem_block* blocks;
        sal_page **pages; // the number of elements is indicated by len / page size.
        struct xbuf_queue* xb_queue;
    } buf;
    xuint16 index; // Current index of scatter/gather buffer such as pages, blocks.
    xuint16 offset; // Current offset for buffer.
    // 16 bytes
    int len;
    int completed_len;
    int flags;
    void *userparam;
    // 32 bytes
    lpx_aio_head_lookahead aio_head_lookahead;
    lpx_aio_user_copy aio_user_copy;
    lpx_aio_complete aio_complete;
    sal_event comp_event;
    // 48 bytes
    // Reserved for system
    int error_len;
    ndas_error_t returncode;
    void *systemuse_sk;
    // 60 bytes
} lpx_aio;


/*
 * Prepare an AIO
 */

static inline
void
lpx_prepare_aio(
    int sockfd,
    int nr_blocks,
    void* buf,
    int offset,
    int len,
    int flags,
    lpx_aio *aio)
{
    sal_memset(aio, 0, sizeof(lpx_aio));

    aio->sockfd = sockfd;
    aio->nr_blocks = nr_blocks;
    // if bufblockcnt is zero, simple buffer pointer is given.
    if(!nr_blocks) {
        aio->buf.buf = buf;
    } else {
        aio->buf.blocks = (struct sal_mem_block*)buf;
    }
    aio->offset = offset;
    aio->len = len;
    aio->flags = flags;
}

static inline
void
lpx_set_comp_routine(
    void *userparam,
    lpx_aio_complete aio_complete,
    lpx_aio *aio)
{
    aio->userparam = userparam;
    aio->aio_complete = aio_complete;
}

static inline
void
lpx_set_userparam(
    void *userparam,
    lpx_aio *aio)
{
    aio->userparam = userparam;
}

static inline
void
lpx_set_head_lookahead_routine(
    lpx_aio_head_lookahead aio_head_lookahead,
    lpx_aio *aio)
{
    aio->aio_head_lookahead = aio_head_lookahead;
}

static inline
void
lpx_set_user_copy_routine(
    lpx_aio_user_copy aio_user_copy,
    lpx_aio *aio)
{
    aio->aio_user_copy = aio_user_copy;
}

static inline
void
lpx_set_comp_event(
    sal_event comp_event,
    lpx_aio *aio)
{
    aio->comp_event = comp_event;
}

static inline
void
_lpx_set_system_sk(
    struct lpx_sock *systemuse_sk,
    lpx_aio *aio)
{
    aio->systemuse_sk = (void *)systemuse_sk;
}

/*
 * Indicate to complete partial amount of data.
 */
static
int
inline
_lpx_complete_aio_datalen(
    lpx_aio * aio,
    int datalen,
    ndas_error_t returncode
){
    int completed = 0;
    //
    // If the completed data length is equal to requested data lenth,
    //  call the completion routine.
    //
    if(returncode >= 0) {
        aio->completed_len += datalen;
    } else {
        aio->error_len += datalen;
        aio->returncode = returncode;
    }
#if 0
    sal_debug_println("aio(%p fd=%d len=%d comp=%d err=%d ret=%d) len=%d ret=%d",
        aio, aio->sockfd, aio->len, aio->completed_len, aio->error_len, aio->returncode, datalen, returncode);
#endif
    sal_assert(aio->len >= aio->completed_len + aio->error_len);
    if(aio->len == aio->completed_len + aio->error_len) {
        sal_event comp_event = aio->comp_event;

        completed = 1;

        sal_assert((aio->aio_flags & LPXAIO_FLAG_COMPLETED) == 0);
        aio->aio_flags |= LPXAIO_FLAG_COMPLETED;

        if(aio->aio_complete) {
            aio->aio_complete(aio, aio->userparam);
        }
        // Do not touch aio memory afterward becuase the completion routine
        // or other user porcess may free the aio memory.
        if(comp_event) {
            sal_event_set(comp_event);
       }
#ifdef DEBUG
    sal_debug_println("aio(%p fd=%d len=%d comp=%d err=%d ret=%d) len=%d ret=%d",
        aio, aio->sockfd, aio->len, aio->completed_len, aio->error_len, aio->returncode, datalen, returncode);
#endif		
    }

    return completed;
}

static
int
inline
lpx_is_aio_completed(
    lpx_aio * aio
){
    return (aio->aio_flags & LPXAIO_FLAG_COMPLETED) != 0;
}

static
int
inline
lpx_complete_aio_datalen(
    lpx_aio * aio,
    int datalen
){
    return _lpx_complete_aio_datalen(aio, datalen, NDAS_OK);
}

/* 
 * Better be non-inline function to exclude error path for performace.
 */
void
lpx_complete_aio_datalen_error(
    lpx_aio * aio,
    int datalen,
    ndas_error_t returncode
);

/* 
 * Better be non-inline function to exclude error path for performace.
 */
void
lpx_complete_aio_leftover(
    lpx_aio *aio,
    ndas_error_t returncode
);

#include "lpx/debug.h" 

ndas_error_t lpx_init(int transfer_unit);

void lpx_cleanup(void);


/*
 * lpx_socket equivalent API to pass the system-implementation of socket
 */
ndas_error_t lpx_socket(int type, int protocol);
ndas_error_t lpx_accept(int parent_fd, struct sockaddr_lpx* addr, unsigned int *addrlen);

ndas_error_t lpx_is_connected(int fd);
ndas_error_t lpx_is_recv_data(int fd);

ndas_error_t lpx_close(int lpxfd);
ndas_error_t lpx_bind(int lpxfd, struct sockaddr_lpx* addr, int addrlen);

/* 
 * Description
 *  Receive the 'len' bytes of data into 'buf' from stream socket.
 * Parameters
 *  lpxfd: stream socket
 *  buf: 
 *  len: size of data to receive
 *  flags: ignored
 * Returns
 * positive : number of bytes received
 * NDAS_OK : if the size is 0.
 * NDAS_ERROR_NOT_CONNECTED: socket is zapped
 * NDAS_ERROR_SHUTDOWN: socket is shutdown
 * NDAS_ERROR_INVALID_HANDLE: lpxfd is invalid
 * NDAS_ERROR_INVALID_OPERATION: lpxfd is datagram socket
 * negative: other socket error from sk->err
 */
ndas_error_t lpx_recv(int lpxfd, void* buf, int len, int flags);
ndas_error_t lpx_recv_aio(lpx_aio * aio);
/* 
 * Description
 *  Receive the 'len' bytes of data into 'buf' from stream socket.
 * Parameters
 *  lpxfd: stream socket
 *  buf: 
 *  len: size of data to receive
 *  flags: ignored
 * Returns
 * positive : number of bytes received
 * NDAS_OK : if the size is 0.
 * NDAS_ERROR_NOT_CONNECTED: socket is zapped
 * NDAS_ERROR_SHUTDOWN: socket is shutdown
 * NDAS_ERROR_INVALID_HANDLE: lpxfd is invalid
 * NDAS_ERROR_INVALID_OPERATION: lpxfd is datagram socket
 * negative: other socket error from sk->err
 */
ndas_error_t lpx_recvmsg(int lpxfd, struct sal_mem_block* blocks, int nr_blocks, int len, int flags);
/**
 * Description
 *  Send the data via the LPX stream socket.
 *
 * Parameters
 *  lpxfd: the file descriptor for the LPX stream socket.
 *  msg: the data to be sent
 *  len: the size of the data to be sent
 *  flags: the value is ignored for the current implementation.
 *
 * Returns
 * Positive: The number of bytes sents
 * NDAS_OK: Successful
 * NDAS_ERROR_INVALID_HANDLE: lpxfd is invalid
 * NDAS_ERROR_INVALID_OPERATION: lpxfd is datagram socket.
 * NDAS_ERROR_NOT_CONNECTED: lpxfd is not connected
 */
ndas_error_t lpx_send(int lpxfd, const void* msg, int len, int flags);
/**
 * Description
 *  Send the data via the LPX datagram socket.
 *
 * Parameters
 *  lpxfd: the file descriptor for the LPX datagram socket.
 *  msg: the data to be sent
 *  len: the size of the data to be sent
 *  flags: the value is ignored for the current implementation.
 *  to: the address to send the data
 *  tolen: should be sizeof(*to)
 *
 * Returns
 * Positive: The number of bytes sents
 * NDAS_OK: Successful
 * NDAS_ERROR_INVALID_HANDLE: lpxfd is invalid
 * NDAS_ERROR_INVALID_OPERATION: lpxfd is stream socket.
 * NDAS_ERROR_NOT_CONNECTED: lpxfd is not connected
 * NDAS_ERROR_NETWORK_DOWN: the network is down
 * NDAS_ERROR_OUT_OF_MEMORY: the memory can't be allocated.
 */
ndas_error_t lpx_sendto(int lpxfd, const void* buf, int len, int flags, const struct sockaddr_lpx *to, int tolen);
/**
 * lpx_sendmsg - send the io data segments 
 */
ndas_error_t lpx_sendmsg(int lpxfd ,struct sal_mem_block* blocks, int nr_blocks, int totoal_length, int flags);
ndas_error_t lpx_sendto(int lpxfd, const void* buf, int len, int flags, const struct sockaddr_lpx *to, int tolen);
ndas_error_t lpx_sendpages(
    int lpxfd, sal_page** pages, unsigned long offset, unsigned long size, int flags);
ndas_error_t lpx_sendpages_aio(lpx_aio *aio);
ndas_error_t lpx_send_aio(lpx_aio *aio);

ndas_error_t lpx_connect(int lpxfd, const struct sockaddr_lpx *serv_addr, int addrlen);
ndas_error_t lpx_listen(int lpxfd, int backlog);
ndas_error_t lpx_getname(int lpxfd, struct sockaddr_lpx*, int* size, int peer);
ndas_error_t lpx_recvfrom(int lpxfd, void* buf, int len, int flags, struct sockaddr_lpx *from, int* fromlen);
ndas_error_t lpx_recvmsg(int lpxfd, struct sal_mem_block* blocks, int nr_blocks, int total_len, int flags);
ndas_error_t lpx_recv_xbuf(int lpxfd, struct xbuf_queue* queue, int len);

ndas_error_t lpx_register_dev(const char* devname);
ndas_error_t lpx_unregister_dev(const char* devname);

ndas_error_t lpx_set_rtimeout(int lpxfd, sal_msec timeout);
ndas_error_t lpx_set_wtimeout(int lpxfd, sal_msec timeout);

struct lpx_retransmits_count {
    xuint32 tx_retransmit;
    xuint32 rx_packet_loss;
};

ndas_error_t lpx_get_retransmit_count(int lpxfd, struct lpx_retransmits_count* cnt);
ndas_error_t lpx_reset_retransmit_count(int lpxfd);
ndas_error_t lpx_set_packet_loss_detection_time(int lpxfd, sal_msec msec);

#ifdef XPLAT_SIO
#define SIO_FLAG_MULTIPLE_NOTIFICATION 0x1
/* Description
 * 
 * If the timeout is not 0, then handler will be call with error NDAS_ERROR_TIMED_OUT
 */
ndas_error_t 
lpx_sio_recvfrom(int lpxfd, void* buf, int len, int flags, 
                    lpx_sio_handler handler, void* user_arg, sal_tick timeout);

#ifdef XPLAT_XIXFS_EVENT
int lpxitf_Is_local_node(unsigned char *node);
#endif //#ifdef XPLAT_XIXFS_EVENT

#endif // XPLAT_SIO



#endif // _LPX_LPXUTIL_H_

