#ifndef _LPX_DEBUG_H_
#define _LPX_DEBUG_H_
/* only included by lpxutil.h */

#include "xlib/gtypes.h"

#ifdef DEBUG
/* type check */

#define DEBUG_LPX_OPT(sk) ({\
    static char buf1[1024]; \
    struct lpx_sock *lpx_sk = sk; \
    sal_snprintf(buf1,1024,"LPXOPT(struct lpx_sock %p)={dst=%s,src=%s"\
        "vmap=%s,interface=%p}",\
        sk,\
        SAL_DEBUG_HEXDUMP_S(LPX_OPT(lpx_sk)->dest_addr.node,6),\
        SAL_DEBUG_HEXDUMP_S(LPX_OPT(lpx_sk)->source_addr.node,6),\
        (LPX_OPT(lpx_sk)->virtual_mapping?"true":"false"),\
        LPX_OPT(lpx_sk)->interface\
    );\
    (const char*) buf1;\
})
#define DEBUG_LPX_STREAM_OPT(sk) ({\
    static char buf2[1024]; \
    struct lpx_sock *lpx_sk = sk; \
    sal_snprintf(buf2,1024,"LPX_STREAM_OPT(struct lpx_sock %p)={"\
        "sequence=0x%x}",\
        lpx_sk,\
        LPX_STREAM_OPT(lpx_sk)->sequence\
    );\
    (const char*) buf2;\
})    


static inline 
const char* DEBUG_LPX_SOCK(struct lpx_sock *sock) 
{
    static char buf[1024];
    static const char* v_state_type[] = {
        "0", "SMP_ESTABLISHED", "SMP_SYN_SENT", "SMP_SYN_RECV", "SMP_FIN_WAIT1", 
        "SMP_FIN_WAIT2", "SMP_TIME_WAIT", "SMP_CLOSE", "SMP_CLOSE_WAIT",
        "SMP_LAST_ACK", "SMP_LISTEN", "SMP_CLOSING"
    };
    sal_snprintf(buf,1024,"struct lpx_sock(%p)={"
        "fd=%d,state=%s,%s,%s,%s,%s,system_sock=%p}",
        sock,
        sock->lpxfd,
        v_state_type[sock->state],
        (sock->zapped) ? "zapped":"not zapped", 
        (sock->dead) ? "dead":"alive", 
        DEBUG_LPX_OPT(sock),
        DEBUG_LPX_STREAM_OPT(sock),
        sock->system_sock
    );
    return (const char*) buf;
}

static inline 
const char* DEBUG_LPX_STREAM_HDR(struct lpxhdr *hdr) 
{
    static char buf[1024];
    if ( hdr ) 
        sal_snprintf(buf,sizeof(buf),
            "struct lpxhdr(%p)={"
            "type=STREAM,pktsize=0x%x,"
            "dest_port=0x%x,source_port=0x%x,"
            "lsctl=0x%x,sequence=0x%x,"
            "ackseq=0x%x,server_tag=0x%x,reserved_s1=%s}",
            hdr,
            g_ntohs(hdr->pu.pktsize & ~LPX_TYPE_MASK),
            g_ntohs(hdr->dest_port),
            g_ntohs(hdr->source_port),
            g_ntohs(hdr->u.s.lsctl),
            g_ntohs(hdr->u.s.sequence),
            g_ntohs(hdr->u.s.ackseq),
            (int)hdr->u.s.server_tag,
            SAL_DEBUG_HEXDUMP_S(hdr->u.s.reserved_s1,3)
        );
    else 
        sal_snprintf(buf, sizeof(buf), "NULL"); 
    return (const char*) buf; 
}

static inline const char* DEBUG_LPX_DATAGRAM_HDR(struct lpxhdr *__hdr__) {
    static char __buf__[1024];
    if ( __hdr__ ) 
        sal_snprintf(__buf__,sizeof(__buf__),"struct lpxhdr(%p)={"
            "type=DATAGRAM,pktsize=0x%x,"
            "dest_port=0x%x,source_port=0x%x,"
            "message_id=0x%x,message_length=0x%x,fragment_id=0x%x,"
            "fragment_length=0x%x,reserved_d3=%x}",
            __hdr__,
            g_ntohs(__hdr__->pu.pktsize & ~LPX_TYPE_MASK),
            g_ntohs(__hdr__->dest_port),
            g_ntohs(__hdr__->source_port),
            __hdr__->u.d.message_id,
            __hdr__->u.d.message_length,
            __hdr__->u.d.fragment_id,
            __hdr__->u.d.fragment_length,
            __hdr__->u.d.reserved_d3
        );
    else 
        sal_snprintf(__buf__, sizeof(__buf__), "NULL");    
    return (const char*) __buf__; 
}
#define DEBUG_LPX_RAW_HDR(__l__) ({\
    static char __buf__[1024];\
    struct lpxhdr *__hdr__ = __l__;\
    if ( __hdr__ ) \
        sal_snprintf(__buf__,sizeof(__buf__),"struct lpxhdr(%p)={"\
            "type=RAW,pktsize=0x%d,"\
            "dest_port=0x%x,source_port=0x%x}",\
            __hdr__,\
            g_ntohs(__hdr__->pu.pktsize & ~LPX_TYPE_MASK),\
            g_ntohs(__hdr__->dest_port),\
            g_ntohs(__hdr__->source_port)\
        );\
    else \
        sal_snprintf(__buf__, sizeof(__buf__), "NULL");    \
    (const char*) __buf__; \
})
static inline 
const char* DEBUG_LPX_HDR(struct lpxhdr *__hdr__) {
    if ( __hdr__ == NULL )
        return "NULL";
    if ( __hdr__->pu.p.type == LPX_TYPE_STREAM) 
        return DEBUG_LPX_STREAM_HDR(__hdr__);
    if     ( __hdr__->pu.p.type == LPX_TYPE_DATAGRAM )
        return DEBUG_LPX_DATAGRAM_HDR(__hdr__);
    return DEBUG_LPX_RAW_HDR(__hdr__);
}
/**
 * Transform LPX node into string.
 * DEBUG purpose
 */
#define DEBUG_LPX_NODE(node) ({\
    static char buf[LPX_NODE_LEN*3]; \
    if ( node ) \
        sal_snprintf(buf, sizeof(buf), \
            "%02x:%02x:%02x:%02x:%02x:%02x", \
            ((char*)node)[0] & 0xff, \
            ((char*)node)[1] & 0xff, \
            ((char*)node)[2] & 0xff, \
            ((char*)node)[3] & 0xff, \
            ((char*)node)[4] & 0xff, \
            ((char*)node)[5] & 0xff); \
    else \
        sal_snprintf(buf, sizeof(buf), "NULL"); \
    (const char *) buf; \
})

#ifdef XPLAT_SIO
static inline 
const char * DEBUG_LPX_SIOCB_I(lpx_siocb_i *acb) 
{
    static char buf[1024];
    if ( acb )
        sal_snprintf(buf,sizeof(buf),"(%p)={fd=%d,buf=%p,nbytes=%d,"
            "result=%d,func=%p,user_arg=%p}"
            ,acb,acb->cb.filedes,acb->cb.buf,acb->cb.nbytes
            ,acb->cb.result, acb->func, acb->user);
    else
        sal_snprintf(buf,sizeof(buf),"NULL");
    return (const char *) buf;
}
#endif
#else //DEBUG
#define DEBUG_LPX_OPT(sk) ({"";})
#define DEBUG_LPX_STREAM_OPT(sk) ({"";})
#define DEBUG_LPX_SOCK(sk) ({"";})
#define DEBUG_LPX_HDR(lpx_hdr) ({"";})
#define DEBUG_LPX_NODE(node) ({ ""; })
#ifdef XPLAT_SIO
#define DEBUG_LPX_SIOCB_I(x) do {} while(0)
#endif
#endif // DEBUG    

#endif //_LPX_DEBUG_H_

