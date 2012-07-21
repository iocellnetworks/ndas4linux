/**********************************************************************
 * Copyright ( C ) 2012 IOCELL Networks
 * All rights reserved.
 **********************************************************************/
#include "xplatcfg.h"
#include "sal/libc.h"
#include "sal/types.h"
#include "sal/debug.h"
#include "sal/net.h"
#include "sal/io.h"
#include "sal/thread.h"
#include "lpx/lpx.h"
#include "lpx/lpxutil.h"
#include "ndasuser/io.h"
#include "ndasuser/ndasuser.h"
#include "xlib/xbuf.h" 
#include "xlib/gtypes.h" /* g_ntohl g_htonl g_ntohs g_htons */
#include "netdisk/netdiskid.h"

#ifdef NDAS_EMU
#include "hash.h"
#include "hdreg.h"
#include "binaryparameters.h"
#include "lanscsi.h"
#include "lsp.h"

//#define EMU_MAX_BLOCKS 64 /* Reduced max blocks to reduce peak memory usage */
#define EMU_MAX_BLOCKS 512
#define EMU_BYTES_OF_BLOCK	512
#define EMU_IDENTIFY_BYTES	512

#define __SOFTWARE_DISK_INFO__ 1

#ifdef DEBUG
#define debug_emu(l,x...) do { \
    if ( (l) <= DEBUG_LEVEL_NDASEMU ) {\
        sal_debug_print("EM|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x); \
    } \
} while(0) 
#else
#define debug_emu(l, x...) do {} while(0)
#endif

#define NR_MAX_TARGET                   1
#define MAX_DATA_BUFFER_SIZE    		EMU_MAX_BLOCKS * EMU_BYTES_OF_BLOCK
#define MAX_CONNECTION                  16
#define LANSCSI_CURRENT_EMU_VERSION 	LANSCSI_VERSION_2_0 // MAX supported version 

#define LANSCSI_CURRENT_EMU_REVISION 0x0e00	

#ifndef MAX_REQUEST_SIZE
#define    MAX_REQUEST_SIZE    1500
#endif

// Start from 0xe00
// v0. Released 2006/5/23 to ALtech.

#define HTONLL(Data)    ( (((Data)&0x00000000000000FFLL) << 56) | (((Data)&0x000000000000FF00LL) << 40) \
                                                | (((Data)&0x0000000000FF0000LL) << 24) | (((Data)&0x00000000FF000000LL) << 8)  \
                                                | (((Data)&0x000000FF00000000LL) >> 8)  | (((Data)&0x0000FF0000000000LL) >> 24) \
                                                | (((Data)&0x00FF000000000000LL) >> 40) | (((Data)&0xFF00000000000000LL) >> 56))

#define NTOHLL(Data)    ( (((Data)&0x00000000000000FFLL) << 56) | (((Data)&0x000000000000FF00LL) << 40) \
                                                | (((Data)&0x0000000000FF0000LL) << 24) | (((Data)&0x00000000FF000000LL) << 8)  \
                                                | (((Data)&0x000000FF00000000LL) >> 8)  | (((Data)&0x0000FF0000000000LL) >> 24) \
                                                | (((Data)&0x00FF000000000000LL) >> 40) | (((Data)&0xFF00000000000000LL) >> 56))

#define SOCKET int  // lpxfd

//#define NDASEMU_ZEROCOPY_MODE	1
#if !defined(NDASEMU_ZEROCOPY_MODE)
//#define NDASEMU_DIRECT_IO	1
#endif

#define NETDISK_DEV_LOCK_COUNT 4

typedef struct _LOCK_TABLE_ENTRY {
	volatile xbool Locked;
	volatile int	 count;
	volatile char owner[6];
} LOCK_TABLE_ENTRY, *PLOCK_TABLE_ENTRY;

/*
 * NDAS task
 */

typedef union _NDASEMU_PDU_REQ {
    LANSCSI_H2R_PDU_HEADER              gen;
    LANSCSI_LOGIN_REQUEST_PDU_HEADER    login;
    LANSCSI_LOGOUT_REQUEST_PDU_HEADER   logout;
    LANSCSI_TEXT_REQUEST_PDU_HEADER     text;
    LANSCSI_VENDOR_REQUEST_PDU_HEADER   vendor;
    LANSCSI_IDE_REQUEST_PDU_HEADER      ide_v0;
    LANSCSI_IDE_REQUEST_PDU_HEADER_V1   ide;
    unsigned char pduheader_buffer[MAX_REQUEST_SIZE];
} NDASEMU_PDU_REQ, *PNDASEMU_PDU_REQ;

typedef union _NDASEMU_PDU_REP {
    LANSCSI_R2H_PDU_HEADER              gen;
    LANSCSI_LOGIN_REPLY_PDU_HEADER      login;
    LANSCSI_LOGOUT_REPLY_PDU_HEADER     logout;
    LANSCSI_TEXT_REPLY_PDU_HEADER       text;
    LANSCSI_VENDOR_REPLY_PDU_HEADER     vendor;
    LANSCSI_IDE_REPLY_PDU_HEADER        ide_v0;
    LANSCSI_IDE_REPLY_PDU_HEADER_V1     ide;
    unsigned char pduheader_buffer[MAX_REQUEST_SIZE];
} NDASEMU_PDU_REP, *PNDASEMU_PDU_REP;

struct _NDASEMU_BUFFER_SEGMENT;
struct _NDASEMU_BUFFER_POOL;
struct  _SESSION_DATA;
typedef struct _NDAS_TASK {
    // task queue entry
    struct list_head    next;
    struct  _SESSION_DATA * session;
    int                 sockfd;
    lpx_aio             aio;
    // immediate data buffer specified in PDU's data segment length or
    // data transfer legnth.
    struct _NDASEMU_BUFFER_SEGMENT *bufseg;
    struct _NDASEMU_BUFFER_POOL *bufpool;
    // Task addresses
    unsigned int targetid;
    // PDU request information
    unsigned char command;
    xint64 blockaddr;
    unsigned int blocklen;
    unsigned int transferlen;
    xuint16 feature;
    LANSCSI_PDU_POINTERS pdureq_pointers;
    // PDU request/reply header
    PNDASEMU_PDU_REQ pdu_req;
    PNDASEMU_PDU_REP pdu_rep;
    sal_page *pdurep_pages[2];
    unsigned int pdurep_offset;
} NDAS_TASK, *PNDAS_TASK;

typedef struct _NDAS_TASK_QUEUE {
        sal_spinlock               task_queue_mutex;
        struct list_head        task_head;
} NDAS_TASK_QUEUE, PNDAS_TASK_QUEUE;

static
void
inline
ndasemu_init_task(
    PNDAS_TASK task,
    int sockfd,
    struct _NDASEMU_BUFFER_POOL * bufpool,
    struct _NDASEMU_BUFFER_SEGMENT * bufseg
){
    task->sockfd = sockfd;
    task->session = NULL;
    task->bufpool = bufpool;
    task->bufseg = bufseg;
}

/*
 * Target data
 */
typedef struct _TARGET_DATA {
    xbool                    bPresent;
    xbool                    bLBA;
    xbool                    bLBA48;
    xint8                   NRRWHost;
    xint8                   NRROHost;

	xuint16 PIO;
	xuint16 UDMA;
	xuint16 MWDMA;

	
	xuchar	Info[EMU_IDENTIFY_BYTES];
    char *                  ExportDev;
    sal_file				Export;
    xuint64                  Size;
	xuint32    TargetDataHi;
    xuint32    TargetDataLow;
	LOCK_TABLE_ENTRY LockTable[NETDISK_DEV_LOCK_COUNT];

    // Task queue
    NDAS_TASK_QUEUE task_queue;

} TARGET_DATA, *PTARGET_DATA;

/*
 * NDAS session
 */
#define NDASEMU_MAX_PENDING_TASKS   3
typedef struct  _SESSION_DATA {
        SOCKET                  connSock;
        int                 logincount;
        xuint16 TargetID;
        xuint32 LUN;
        int                             iSessionPhase;
        xuint16 CSubPacketSeq;
        xuint8  iLoginType;
        xuint16 CPSlot;
        unsigned                PathCommandTag;
        unsigned                iUser;
        xuint32 HPID;
        xuint16 RPID;
        unsigned                CHAP_I;
        unsigned                CHAP_C;
        xbool                    bIncCount;
        xuchar iPassword[8];
		xuchar Addr[6];
		xbool		HeldLock[NETDISK_DEV_LOCK_COUNT];
        NDAS_TASK   tasks[NDASEMU_MAX_PENDING_TASKS];
        PNDAS_TASK  next_avail_task;
        unsigned int    rtask_sn;
        unsigned int    rtask_pending_sn;
} SESSION_DATA, *PSESSION_DATA;

/*
 * PROM data
 */
typedef struct _PROM_DATA {
        xuint64 	MaxConnTime;
        xuint64 	MaxRetTime;
        xuchar 		UserPasswd[8];
        xuchar 		SuperPasswd[8];
} PROM_DATA, *PPROM_DATA;

#if 1
struct _NDASEMU_BUFFER_POOL;
struct SESSION_THREAD_PARAM {
    SOCKET sock;
    struct sockaddr_lpx slpx;
    struct _NDASEMU_BUFFER_POOL *ndasemu_bufferpool;
};
#endif
/*
 * Task thread worker data
 */
typedef struct _TASK_WORKER_CONTEXT {
    sal_semaphore   tw_task_semaphore;
    NDAS_TASK_QUEUE tw_task_queue;
} TASK_WORKER_CONTEXT, *PTASK_WORKER_CONTEXT;

#define NDASEMU_MAX_TASK_WORKERS 1
TASK_WORKER_CONTEXT task_worker_context[NDASEMU_MAX_TASK_WORKERS];

// Global Variables
static xint16                  G_RPID = 0;
static int ListenSock;

static sal_thread_id 	GenPnpThreadId;
static sal_semaphore 	GenPnpExited;
static volatile xbool	GenPnpRunning;

static sal_thread_id 	ListenThreadId;
static sal_semaphore 	ListenThreadExited;
static volatile xbool	ListenThreadRunning;

// to support version 1.1, 2.0
static xuint8  thisHWVersion;

static TARGET_DATA             *PerTarget[NR_MAX_TARGET];
static PROM_DATA               *Prom;

// Data buffers
#define NDASEMU_DATABUFFER_ACQUIRED 0x0001

typedef struct _NDASEMU_BUFFER_SEGMENT {
    void * bufseg_addr;
    sal_page **bufseg_pages;
    unsigned int bufseg_offset;
    int bufseg_size;
    int bufseg_flags;
} NDASEMU_BUFFER_SEGMENT, *PNDASEMU_BUFFER_SEGMENT;

//#define NDASEMU_BUFFER_MAX_SEGMENT_NUM NDASEMU_MAX_PENDING_TASKS
#define  NDASEMU_BUFFER_MAX_SEGMENT_NUM 1
typedef struct _NDASEMU_BUFFER_POOL {
    NDASEMU_BUFFER_SEGMENT segments[NDASEMU_BUFFER_MAX_SEGMENT_NUM];
    sal_spinlock buffer_mutex; /* Not accessed at interrupt context. Okay to use spinlock without irqsave */
    sal_semaphore buffer_avail;
    PNDASEMU_BUFFER_SEGMENT recent_segment;
    int acquired_count;
} NDASEMU_BUFFER_POOL, *PNDASEMU_BUFFER_POOL;

static NDASEMU_BUFFER_POOL global_databuffer;


static xuint16 HeaderEncryptAlgo = 0;
static xuint16 DataEncryptAlgo = 0;
static sal_spinlock EmuLock;
//static sal_semaphore DiskIoLock = SAL_INVALID_SEMAPHORE;
static int MaxBlockSize = EMU_MAX_BLOCKS; // EMU_BYTES_OF_BLOCK bytes of one block size.

/*
 * Random number generator
 */
static int _rand(void)
{
	static int i = 32217;
	return i++;
}

/*
 * Task service functions
 */

ndas_error_t
ndasemu_task_login_decode_service(
    PNDAS_TASK  ndastask
);

ndas_error_t
ndasemu_task_logout_decode_service(
    PNDAS_TASK  ndastask
);
ndas_error_t
ndasemu_task_text_decode_service(
    PNDAS_TASK  ndastask
);
ndas_error_t
ndasemu_task_vendor_decode_service(
    PNDAS_TASK ndastask
);

ndas_error_t
ndasemu_task_ide_v0_decode(
    PNDAS_TASK ndastask
);

ndas_error_t
ndasemu_task_ide_decode(
    PNDAS_TASK ndastask
);

/*
 * Buffer management
 */
int
ndasemu_is_bufseg_avail(
    PNDASEMU_BUFFER_POOL ndasemu_bufferpool);

PNDASEMU_BUFFER_SEGMENT
ndasemu_get_segment(
    PNDASEMU_BUFFER_POOL ndasemu_bufferpool,
    int wait);

void
ndasemu_put_segment(
    PNDASEMU_BUFFER_POOL ndasemu_bufferpool,
    PNDASEMU_BUFFER_SEGMENT segment);

/*
 * Raw device communication management
 */
NDAS_SAL_API
int ndasemu_read_actor(
	sal_read_descriptor_t *read_desc,
	sal_page *page,
	unsigned long offset,
	unsigned long size
){
	int		ret;
	int		sockdesc = (int)read_desc->arg.data;
	xsize_t	written;
	char	*vaddr;
	unsigned long count = read_desc->count;

    if (size > count)
        size = count;

#if 0
        vaddr = sal_map_page(page);
	ret = lpx_send(sockdesc, vaddr + offset, size, 0);
	sal_unmap_page(page);
#else
	ret = lpx_sendpages(sockdesc, &page, offset, size, 0);
#endif
	if(ret < 0) {
		debug_emu(1, "lpx_send failed vaddr=%p ret=%d", vaddr, ret);
		written = 0;
	} else {
//		debug_emu(1, "lpx_send vaddr=%p offset=%lu size=%lu", vaddr, offset, size);
		written = size;
		read_desc->count = count - size;
		read_desc->written += size;
	}

	return written;
}

//
// Send file data to network using SAL senfile API to reduce copy.
//

int
ndasemu_sendfile(
	int			sockfd,
	sal_file	file,
	xuint64		offset,
	xint32		size,
	int			*res
){
	xint32	ret;
	sal_read_actor_context read_actor_ctx;

	read_actor_ctx.sal_read_actor = ndasemu_read_actor;
	read_actor_ctx.target = (void *)sockfd;
	ret = sal_file_send(
		file,
		offset,
		size,
		&read_actor_ctx);

	if (ret<=0) {
		debug_emu(1, "sal_file_send faild. offset=%llu size=%d", offset, size);
        *res = 0;
		return -1;
	} else {
//		debug_emu(1, "sal_file_send. offset=%llu size=%d", offset, size);
		*res = size;
		return 0;
	}
}


//
// Send file data to network using SAL senfile API to reduce copy.
//

int
ndasemu_recvfile(
	int			sockfd,
	sal_file	file,
	xuint64		offset,
	xint32		size,
	int			*res
){
	xint32	ret;
    struct xbuf_queue rx_xq;

	xbuf_queue_init(&rx_xq);
	ret = lpx_recv_xbuf(sockfd, &rx_xq, size);
	xbuf_queue_destroy(&rx_xq);

	if (ret<=0) {
		debug_emu(1, "lpx_recv_xbuf faild. offset=%llu size=%d", offset, size);
		return -1;
	} else {
//		debug_emu(1, "lpx_recv_xbuf. offset=%llu size=%d", offset, size);
		*res = size;
		return 0;
	}
}

static int SendIt(int lpxfd, void* buf, int size, int *res)
{
	int ret;
    lpx_aio aio;

    lpx_prepare_aio(
        lpxfd,
        0,
        (void *)buf,
        0,
        size,
        0,
        &aio);

    ret = lpx_send_aio(&aio);
	if (ret<=0) {
		return -1;
	} else {
		*res = size;
		return 0;
	}
}


void ndasemu_aio_complete(struct _lpx_aio * aio, void *userparam)
{
    PNDAS_TASK task = (PNDAS_TASK)userparam;

    debug_emu(4, "aio=%p sockfd=%d len=%d comp=%d err=%d ret=%d",
    aio, aio->sockfd, aio->len, aio->completed_len, aio->error_len, aio->returncode);

    // return the buffer
    if(task->bufseg)
    {
        ndasemu_put_segment(task->bufpool, task->bufseg);
        task->bufseg = NULL;
    }
}

static
int
ndasemu_sendpages(PNDAS_TASK task, int size, int *res)
{
	int ret;

    debug_emu(4, "task=%p requested len=%d", task, size);
    
    if(size == 0) {
        *res = 0;
        return 0;
    }

    ret = lpx_sendpages(
        task->sockfd,
        task->bufseg->bufseg_pages,
        task->bufseg->bufseg_offset,
        size,
        0
        );
	if (ret<=0) {
		return -1;
	} else {
		*res = size;
		return 0;
	}
}

int SendtoIt(int lpxfd, void * buff, int len, unsigned flags,
                           const struct sockaddr_lpx *addr, int addr_len)
{
	return lpx_sendto(lpxfd, buff, len, flags, addr, addr_len);
}

int RecvIt(int lpxfd, void* buf, int len, int *res, lpx_aio_head_lookahead aio_head_lookahead, void *userparam)
{
	int ret;
    lpx_aio aio;

    lpx_prepare_aio(
        lpxfd,
        0,
        buf,
        0,
        len,
        0,
        &aio);
    if(aio_head_lookahead) {
        lpx_set_head_lookahead_routine( aio_head_lookahead, &aio);
        lpx_set_userparam(userparam, &aio);
    }
	
    ret = lpx_recv_aio(&aio);
	if (ret<0) { 
		debug_emu(1, "failed len=%d", len);
		return -1;
	} else if (ret==0) {
		// timeout
		debug_emu(4, "timeout len=%d", len);
		*res = 0;
		return 0;
	} else {
		*res = len;
		return 0;
	}
}
/*
 * PDU management
 */

int
ndasemu_head_lookahead(
    lpx_aio *aio,
    void *userparam,
    void *recvdata,
    int recvdata_len,
    int *request_len_adjustment
)
{
    PNDAS_TASK ndastask = (PNDAS_TASK)userparam;
    PSESSION_DATA   pSessionData = ndastask->session;
    PLANSCSI_H2R_PDU_HEADER pdu_gen_req = (PLANSCSI_H2R_PDU_HEADER)recvdata;
    xuchar*   pPtr = (xuchar*)pdu_gen_req;
    PLANSCSI_PDU_POINTERS    pPdu = &ndastask->pdureq_pointers;
    int    iTotalRecved;
    int dataseg_len, ahseg_len;
    int iret;

    if(recvdata_len < sizeof(LANSCSI_H2R_PDU_HEADER)) {
        return 0;
    }

    if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
        && HeaderEncryptAlgo != 0) {
        Decrypt32(
            (unsigned char*)pdu_gen_req,
            sizeof(LANSCSI_H2R_PDU_HEADER),
            (unsigned char *)&pSessionData->CHAP_C,
            (unsigned char *)&pSessionData->iPassword
            );
    }

    ndastask->pdu_req = (PNDASEMU_PDU_REQ)recvdata;
    iTotalRecved = sizeof(LANSCSI_H2R_PDU_HEADER);
    pPdu->u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pPtr;
    pPtr += sizeof(LANSCSI_H2R_PDU_HEADER);

    ahseg_len = g_ntohs(pPdu->u.pH2RHeader->AHSLen);
    dataseg_len = g_ntohl(pPdu->u.pH2RHeader->DataSegLen);

    // Read AHS.
    if(ahseg_len > 0) {
        //debug_emu(1, "AHSLen = %d", g_ntohs(pPdu->u.pH2RHeader->AHSLen));
        if(recvdata_len < iTotalRecved +ahseg_len) {
            debug_emu(1, "Can't Recv AHS...");
            return iTotalRecved;
        }

        iTotalRecved += ahseg_len;
        pPdu->pAHS = pPtr;
        pPtr += ahseg_len;

        // to support version 1.1, 2.0

        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
            thisHWVersion == LANSCSI_VERSION_2_0) {
            if (pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
                && HeaderEncryptAlgo != 0) {

                Decrypt32(
                    (unsigned char*)pPdu->pAHS,
                    ahseg_len,
                    (unsigned char *)&pSessionData->CHAP_C,
                    (unsigned char *)&pSessionData->iPassword
                    );
            }
        }
    }

    // Read Header Dig.
    pPdu->pHeaderDig = NULL;

    // Read Data segment.
    if(dataseg_len > 0) {
        //debug_emu(1, "DataSegLen = %d", g_ntohl(pPdu->u.pH2RHeader->DataSegLen));
        if(recvdata_len < iTotalRecved + dataseg_len) {
            debug_emu(1, "Can't Recv Data segment...");

            return iTotalRecved;
        }

        iTotalRecved += dataseg_len;
        pPdu->pDataSeg = pPtr;
        pPtr += dataseg_len;

        if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
            && DataEncryptAlgo != 0) {

            Decrypt32(
                (unsigned char*)pPdu->pDataSeg,
                dataseg_len,
                (unsigned char *)&pSessionData->CHAP_C,
                (unsigned char *)&pSessionData->iPassword
                );
        }

    }

    // Read Data Dig.
    pPdu->pDataDig = NULL;

    sal_assert(iTotalRecved >= sizeof(LANSCSI_H2R_PDU_HEADER));
    *request_len_adjustment = iTotalRecved - sizeof(LANSCSI_H2R_PDU_HEADER);

    //
    // Decode the request PDU.
    //
    ndastask->pdu_rep->gen.Flags = ndastask->pdu_req->gen.Flags;
    ndastask->pdu_rep->gen.PathCommandTag = ndastask->pdu_req->gen.PathCommandTag;
    ndastask->pdu_rep->gen.InitiatorTaskTag = ndastask->pdu_req->gen.InitiatorTaskTag;
    ndastask->pdu_rep->gen.TargetID = ndastask->pdu_req->gen.TargetID;
    ndastask->pdu_rep->gen.LunHi = ndastask->pdu_req->gen.LunHi;
    ndastask->pdu_rep->gen.LunLow = ndastask->pdu_req->gen.LunLow;
    ndastask->pdu_rep->gen.CSubPacketSeq = ndastask->pdu_req->gen.CSubPacketSeq;

    switch(ndastask->pdu_req->gen.Opcode) {
    case LOGIN_REQUEST:
        (void)ndasemu_task_login_decode_service(ndastask);
        break;
    case LOGOUT_REQUEST:
        (void)ndasemu_task_logout_decode_service(ndastask);
        break;
    case TEXT_REQUEST:
        (void)ndasemu_task_text_decode_service(ndastask);
        break;
    case VENDOR_SPECIFIC_COMMAND:
        (void)ndasemu_task_vendor_decode_service(ndastask);
        break;
    case IDE_COMMAND:
        if (thisHWVersion == LANSCSI_VERSION_1_0) {

            iret = ndasemu_task_ide_v0_decode(
                ndastask);

        } else if (thisHWVersion == LANSCSI_VERSION_1_1 ||
            thisHWVersion == LANSCSI_VERSION_2_0) {

            iret = ndasemu_task_ide_decode(
                ndastask);
        } else {
            iret = -1;
        }
        break;
    case NOP_H2R:
        ndastask->pdu_rep->gen.Opcode = NOP_R2H;
        break;
    }
    ndastask->pdu_req = NULL;

    return iTotalRecved;
}

#define NDASEMU_RECVREQ_FLAG_NO_WAIT    0x0001
#define NDASEMU_RECVREQ_FLAG_LOOKAHEAD  0x0002

int
ndasemu_receive_request(
    PNDAS_TASK  task,
    int flags
    )
{
    PSESSION_DATA   pSessionData = task->session;
    PLANSCSI_H2R_PDU_HEADER   pRequest;
    PLANSCSI_PDU_POINTERS    pPdu;
    xuchar*   pPtr;
    int iResult, res;
    int iTotalRecved = 0;

    if(flags & NDASEMU_RECVREQ_FLAG_NO_WAIT) {
        if(!lpx_is_recv_data(task->sockfd)) {
            return 0;
        }
    }

    pRequest = &task->pdu_req->gen;
    pPdu = &task->pdureq_pointers;
    pPtr = (xuchar*)pRequest;

timeout_retry:
    // Read Header.
    if(flags & NDASEMU_RECVREQ_FLAG_LOOKAHEAD) {
        iResult = RecvIt(
                task->sockfd,
                pPtr,
                sizeof(LANSCSI_H2R_PDU_HEADER),
                &res,
                ndasemu_head_lookahead,
                (void*)task
                );
    } else {
        iResult = RecvIt(
                task->sockfd,
                pPtr,
                sizeof(LANSCSI_H2R_PDU_HEADER),
                &res,
                NULL,
                NULL
                );
    }
    if (iResult == 0 && res == 0) {
    	// Emulator should wait forever for header.
    	debug_emu(1, "timeout. Wait again");
		goto timeout_retry;
    } else if(iResult < 0 || res != sizeof(LANSCSI_H2R_PDU_HEADER)) {
	    debug_emu(1, "Can't Recv Header...");
	    return iResult;
    } else {
        iTotalRecved += res;
	}
    if(flags & NDASEMU_RECVREQ_FLAG_LOOKAHEAD) {
        return res;
    }

    task->pdu_req = (PNDASEMU_PDU_REQ)task->pdu_rep;
    pPdu->u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pPtr;

    pPtr += sizeof(LANSCSI_H2R_PDU_HEADER);

    if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
		&& HeaderEncryptAlgo != 0) {
		Decrypt32(
			(unsigned char*)pPdu->u.pH2RHeader,
			sizeof(LANSCSI_H2R_PDU_HEADER),
			(unsigned char *)&pSessionData->CHAP_C,
			(unsigned char *)&pSessionData->iPassword
			);
    }

    // Read AHS.
    if(g_ntohs(pPdu->u.pH2RHeader->AHSLen) > 0) {
            //debug_emu(1, "AHSLen = %d", g_ntohs(pPdu->u.pH2RHeader->AHSLen));
	    iResult = RecvIt(
            task->sockfd,
            pPtr,
            g_ntohs(pPdu->u.pH2RHeader->AHSLen),
            &res,
            NULL,
            NULL
        );
        if(iResult < 0 || res != g_ntohs(pPdu->u.pH2RHeader->AHSLen)) {
            debug_emu(1, "Can't Recv AHS...");
            return iResult;
        } else
            iTotalRecved += res;

            pPdu->pAHS = pPtr;

            pPtr += g_ntohs(pPdu->u.pH2RHeader->AHSLen);

            // to support version 1.1, 2.0

            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                if (pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
                    && HeaderEncryptAlgo != 0) {

                    Decrypt32(
                        (unsigned char*)pPdu->pAHS,
                        g_ntohs(pPdu->u.pH2RHeader->AHSLen),
//                                      sizeof(pPdu->u.pR2HHeader->AHSLen),
                        (unsigned char *)&pSessionData->CHAP_C,
                        (unsigned char *)&pSessionData->iPassword
                        );
            }
        }

        // end of supporting version
    }

    // Read Header Dig.
    pPdu->pHeaderDig = NULL;

    // Read Data segment.
    if(g_ntohl(pPdu->u.pH2RHeader->DataSegLen) > 0) {
        //debug_emu(1, "DataSegLen = %d", g_ntohl(pPdu->u.pH2RHeader->DataSegLen));
        iResult = RecvIt(
            task->sockfd,
            pPtr,
            g_ntohl(pPdu->u.pH2RHeader->DataSegLen),
            &res,
            NULL,
            NULL
            );
        if(iResult < 0 || res != g_ntohl(pPdu->u.pH2RHeader->DataSegLen)) {
            debug_emu(1, "Can't Recv Data segment...");

            return iResult; 
        } else  
            iTotalRecved += res;

        pPdu->pDataSeg = pPtr;

        pPtr += g_ntohl(pPdu->u.pH2RHeader->DataSegLen);


        if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
            && DataEncryptAlgo != 0) {

            Decrypt32(
                (unsigned char*)pPdu->pDataSeg,
                g_ntohl(pPdu->u.pH2RHeader->DataSegLen),
                (unsigned char *)&pSessionData->CHAP_C,
                (unsigned char *)&pSessionData->iPassword
                );
        }

    }

    // Read Data Dig.
    pPdu->pDataDig = NULL;

    return iTotalRecved;
}

//
// Send reply
//
ndas_error_t
ndasemu_send_reply(
    PSESSION_DATA   pSessionData,
    PLANSCSI_R2H_PDU_HEADER pReplyHeader,
    PNDAS_TASK      task
){
    int             iResult, res =0;
    xuint32 iTemp = 0;

    // Send Reply.

    pReplyHeader->HPID = g_htonl(pSessionData->HPID);
    pReplyHeader->RPID = g_htons(pSessionData->RPID);
    pReplyHeader->CPSlot = g_htons(pSessionData->CPSlot);

    // to support version 1.1, 2.0 
    if (thisHWVersion == LANSCSI_VERSION_1_0) {
        pReplyHeader->AHSLen = 0;
    }
    if (thisHWVersion == LANSCSI_VERSION_1_1 ||
        thisHWVersion == LANSCSI_VERSION_2_0) {
        pReplyHeader->DataSegLen = 0;
    }
    // end of supporting version

    // to support version 1.1, 2.0 
    if (thisHWVersion == LANSCSI_VERSION_1_0) {
        iTemp = sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER) + g_ntohl(pReplyHeader->DataSegLen);
    }
    if (thisHWVersion == LANSCSI_VERSION_1_1 ||
        thisHWVersion == LANSCSI_VERSION_2_0) {
        iTemp = sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER) + g_ntohs(pReplyHeader->AHSLen);
    }
    // end of supporting version

    //
    // Encrypt the PDU header.
    //
    if(pReplyHeader->Opcode != LOGIN_RESPONSE) {
        if(HeaderEncryptAlgo != 0) {
            Encrypt32(
                (unsigned char*)pReplyHeader,
                sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER),
                (unsigned char *)&pSessionData->CHAP_C,
                (unsigned char *)&pSessionData->iPassword
                );          
        }

        // to support version 1.1, 2.0 
        if (thisHWVersion == LANSCSI_VERSION_1_0) {
            if(DataEncryptAlgo != 0 
                && g_ntohl(pReplyHeader->DataSegLen) != 0) {
                Encrypt32(
                    (unsigned char*)pReplyHeader + sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER),
                    iTemp,
                    (unsigned char *)&pSessionData->CHAP_C,
                    (unsigned char *)&pSessionData->iPassword
                    );
            }
        }
        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
            thisHWVersion == LANSCSI_VERSION_2_0) {
            if(HeaderEncryptAlgo != 0 
                && g_ntohs(pReplyHeader->AHSLen) != 0) {
                Encrypt32(
                    (unsigned char*)pReplyHeader + sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER),
                    iTemp,
                    (unsigned char *)&pSessionData->CHAP_C,
                    (unsigned char *)&pSessionData->iPassword
                    );
            }
        }
        // end of supporting version

    }

    //debug_emu(1, "SendIt : Send count is %d", iTemp);
    if(task) {
        lpx_prepare_aio(
            task->sockfd,
            0, // Indicate no mem block used.
            task->pdurep_pages,
            task->pdurep_offset,
            iTemp,
            0,
            &task->aio);
        lpx_set_comp_routine(task, ndasemu_aio_complete, &task->aio);
        iResult = lpx_sendpages_aio(&task->aio);
        if(iResult > 0) {
            res = iTemp;
        }
    } else {
        iResult = SendIt(
            pSessionData->connSock,
            pReplyHeader,
            iTemp,
            &res
            );
    }
    if(iResult < 0 || res != iTemp) {
        debug_emu(1, "Can't Send data body...");
        pSessionData->iSessionPhase = LOGOUT_PHASE;
        return NDAS_ERROR;
    }

    return NDAS_OK;
}

/*
 * Task functions
 */
 
 /*
    Login procedure
           Initiator                                        Target
  Step 1         [ CHAP_A=<A1,A2...>                           ] ->
              <- [ CHAP_A=<A> CHAP_I=<I> CHAP_C=<C>            ]
  Step 3         [ CHAP_N=<N> CHAP_R=<R>                       ] ->
                 or, if it requires target authentication, with:
                 [ CHAP_N=<N> CHAP_R=<R> CHAP_I=<I> CHAP_C=<C> ] ->
  Step 4      <- [ CHAP_N=<N> CHAP_R=<R>                       ]

    CHAP_A   hash algorithm type
    CHAP_I   identifier
    CHAP_N   name ( User ID )
    CHAP_R   response
    CHAP_C   chalenge

  */
 
ndas_error_t
ndasemu_task_login_decode_service(
    PNDAS_TASK  ndastask
)
{
    PSESSION_DATA          pSessionData = ndastask->session;
    PLANSCSI_LOGIN_REQUEST_PDU_HEADER    pLoginRequestHeader = &ndastask->pdu_req->login;
    PLANSCSI_LOGIN_REPLY_PDU_HEADER     pLoginReplyHeader = &ndastask->pdu_rep->login;
    PLANSCSI_PDU_POINTERS          pdu = &ndastask->pdureq_pointers;
    PBIN_PARAM_SECURITY         pSecurityParam = NULL;
    PAUTH_PARAMETER_CHAP            pAuthChapParam;
    PBIN_PARAM_NEGOTIATION          pParamNego = NULL;

    //  debug_emu(1, "Session(%p): Recevived..., opcode is %02X, SubPacketSeq is %d, login count is %d", 
    //  &current->session, pRequestHeader->Opcode, g_ntohs(pRequestHeader->CSubPacketSeq), pSession->logincount); 
    //
    // Init reply PDU.
    //
    pLoginReplyHeader->AHSLen = 0;
    pLoginReplyHeader->DataSegLen = 0;

    if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE) {
        // Bad Command...
        debug_emu(1, "Bad Command.");
        pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

        goto MakeLoginReply;
    } 

    // Check Header.
    if((pLoginRequestHeader->VerMin > thisHWVersion)
        || (pLoginRequestHeader->ParameterType != PARAMETER_TYPE_BINARY)
        || (pLoginRequestHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
        // Bad Parameter...
        debug_emu(1, "Bad Parameter.");

        pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_VERSION_MISMATCH;
        goto MakeLoginReply;
    }
#if 0   // this cause problem if peer try to login with ver1.0 protocol first.              
    // Check Sub Packet Sequence.
    if(g_ntohs(pLoginRequestHeader->CSubPacketSeq) != pSessionData->CSubPacketSeq) {
        // Bad Sub Sequence...
        debug_emu(1, "Bad Sub Packet Sequence. H %d R %d",
            pSessionData->CSubPacketSeq,
            g_ntohs(pLoginRequestHeader->CSubPacketSeq));

        pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
        goto MakeLoginReply;
    }
#endif
    // Check Port...
    if(pLoginRequestHeader->CSubPacketSeq > 0) {
        if((pSessionData->HPID != (unsigned)g_ntohl(pLoginRequestHeader->HPID))
            || (pSessionData->RPID != g_ntohs(pLoginRequestHeader->RPID))
            || (pSessionData->CPSlot != g_ntohs(pLoginRequestHeader->CPSlot))
            || (pSessionData->PathCommandTag != (unsigned)g_ntohl(pLoginRequestHeader->PathCommandTag))) {

            debug_emu(1, "Bad Port parameter.");

            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
            goto MakeLoginReply;
        }
    }

    switch(g_ntohs(pLoginRequestHeader->CSubPacketSeq)) {
    case 0:
        {
            //
            // Version and security negotiation
            //
            debug_emu(4, "*** First ***");
            // Check Flag.
            if((pLoginRequestHeader->u.s.T != 0)
                || (pLoginRequestHeader->u.s.CSG != FLAG_SECURITY_PHASE)
                || (pLoginRequestHeader->u.s.NSG != FLAG_SECURITY_PHASE)) {
                debug_emu(1, "BAD First Flag.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                goto MakeLoginReply;
            }

            // Check Parameter.

            if(g_ntohl(pLoginRequestHeader->DataSegLen) != 0) {     // Minus AuthParameter[1]
                pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pDataSeg;
            }
            else if(g_ntohl(pLoginRequestHeader->AHSLen) != 0) {    // Minus AuthParameter[1]
                pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pAHS;
            } else {
                debug_emu(1," AHS or data seg should be not 0");
                pSecurityParam = NULL;
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                goto MakeLoginReply;
            }

            if(pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) {
                debug_emu(1, "BAD First Request Parameter.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                goto MakeLoginReply;
            }

            // Login Type.
            if((pSecurityParam->LoginType != LOGIN_TYPE_NORMAL) 
                && (pSecurityParam->LoginType != LOGIN_TYPE_DISCOVERY)) {
                debug_emu(1, "BAD First Login Type.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                goto MakeLoginReply;
            }
//          debug_emu(1, "Login type is %d", pSecurityParam->LoginType);

            // Auth Type.
            if(!(g_ntohs(pSecurityParam->AuthMethod) & AUTH_METHOD_CHAP)) {
                debug_emu(1, "BAD First Auth Method.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeLoginReply;
            }

            // Store host ID and requested command process slot.
            pSessionData->HPID = g_ntohl(pLoginRequestHeader->HPID);
            pSessionData->CPSlot = g_ntohs(pLoginRequestHeader->CPSlot);
            // Store initial path command tag.
            pSessionData->PathCommandTag = g_ntohl(pLoginRequestHeader->PathCommandTag);
            // Store login type.
            pSessionData->iLoginType = pSecurityParam->LoginType;

            // Assign RPID...
            pSessionData->RPID = G_RPID;

            pSecurityParam = NULL;

            //debug_emu(1, "[LanScsiEmu] Version Min %d, Max %d Auth Method %d, Login Type %d, HWVersion %d",
                //pLoginRequestHeader->VerMin, pLoginRequestHeader->VerMax, g_ntohs(pSecurityParam->AuthMethod), pSecurityParam->LoginType, thisHWVersion);

            // Make Reply.
            pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
            pLoginReplyHeader->u.s.T = 0;
            pLoginReplyHeader->u.s.CSG = FLAG_SECURITY_PHASE;
            pLoginReplyHeader->u.s.NSG = FLAG_SECURITY_PHASE;

            // to support version 1.1, 2.0 

            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pLoginReplyHeader->DataSegLen = g_htonl(BIN_PARAM_SIZE_REPLY);
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                pLoginReplyHeader->AHSLen = g_htons(BIN_PARAM_SIZE_REPLY);
            }

            // end of supporting version

            pSecurityParam = (PBIN_PARAM_SECURITY)(pLoginReplyHeader +  1);
            pSecurityParam->ParamType = BIN_PARAM_TYPE_SECURITY;
            pSecurityParam->LoginType = pSessionData->iLoginType;
            pSecurityParam->AuthMethod = g_htons(AUTH_METHOD_CHAP);
        }
        break;
    case 1:
        {
            //
            // Security check 1
            //
            debug_emu(4, "*** Second ***");
            // Check Flag.
            if((pLoginRequestHeader->u.s.T != 0)
                || (pLoginRequestHeader->u.s.CSG != FLAG_SECURITY_PHASE)
                || (pLoginRequestHeader->u.s.NSG != FLAG_SECURITY_PHASE)) {
                debug_emu(1, "BAD Second Flag.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                goto MakeLoginReply;
            }

            // Check Parameter.

            // to support version 1.1, 2.0 
            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                if((g_ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST) // Minus AuthParameter[1]
                    || (pdu->pDataSeg == NULL)) {

                    debug_emu(1, "BAD Second Request Data.");
                    pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    goto MakeLoginReply;
                }   
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                if((g_ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST) // Minus AuthParameter[1]
                    || (pdu->pAHS == NULL)) {

                    debug_emu(1, "BAD Second Request Data.");
                    pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    goto MakeLoginReply;
                }
            }

            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pDataSeg;
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pAHS;
            }
            // end of supporting version

            if((pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) 
                || (pSecurityParam->LoginType != pSessionData->iLoginType)
                || (g_ntohs(pSecurityParam->AuthMethod) != AUTH_METHOD_CHAP)) {

                debug_emu(1, "BAD Second Request Parameter.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                goto MakeLoginReply;
            }

            // Hash Algorithm.
            pAuthChapParam = (PAUTH_PARAMETER_CHAP)pSecurityParam->u.AuthParameter;
            if(!(g_ntohl(pAuthChapParam->CHAP_A) & HASH_ALGORITHM_MD5)) {
                debug_emu(1, "Not Supported HASH Algorithm.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeLoginReply;
            }
            pSecurityParam = NULL;
            pAuthChapParam = NULL;

            // Create identifier
            pSessionData->CHAP_I = (_rand() << 16) + _rand();

            // Create Challenge
            pSessionData->CHAP_C = (_rand() << 16) + _rand();

            // Make Header
            pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
            pLoginReplyHeader->u.s.T = 0;
            pLoginReplyHeader->u.s.CSG = FLAG_SECURITY_PHASE;
            pLoginReplyHeader->u.s.NSG = FLAG_SECURITY_PHASE;

            // to support version 1.1, 2.0 
            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pLoginReplyHeader->DataSegLen = g_htonl(BIN_PARAM_SIZE_REPLY);
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                pLoginReplyHeader->AHSLen = g_htons(BIN_PARAM_SIZE_REPLY);
            }
            // end of supporting version

            pSecurityParam = (PBIN_PARAM_SECURITY)(pLoginReplyHeader + 1);
            pSecurityParam->ParamType = BIN_PARAM_TYPE_SECURITY;
            pSecurityParam->LoginType = pSessionData->iLoginType;
            pSecurityParam->AuthMethod = g_htons(AUTH_METHOD_CHAP);

            pAuthChapParam = &pSecurityParam->u.ChapParam;
            pAuthChapParam->CHAP_A = g_htonl(HASH_ALGORITHM_MD5);
            pAuthChapParam->CHAP_I = g_htonl(pSessionData->CHAP_I);
            sal_memcpy(pAuthChapParam->CHAP_C, (char*)&pSessionData->CHAP_C,4);

//          debug_emu(1, "CHAP_C %d", pSessionData->CHAP_C);
        }
        break;
    case 2:
        {
            //
            // Security check 2
            //
            debug_emu(4, "*** Third ***");
            // Check Flag.
            if((pLoginRequestHeader->u.s.T == 0)
                || (pLoginRequestHeader->u.s.CSG != FLAG_SECURITY_PHASE)
                || (pLoginRequestHeader->u.s.NSG != FLAG_LOGIN_OPERATION_PHASE)) {
                debug_emu(1, "BAD Third Flag.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                goto MakeLoginReply;
            }

            // Check Parameter.

            // to support version 1.1, 2.0 
            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                if((g_ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)  // Minus AuthParameter[1]
                    || (pdu->pDataSeg == NULL)) {

                    debug_emu(1, "BAD Third Request Data."); 
                    pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    goto MakeLoginReply;
                }
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                if((g_ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)  // Minus AuthParameter[1]
                    || (pdu->pAHS == NULL)) {

                    debug_emu(1, "BAD Third Request Data."); 
                    pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    goto MakeLoginReply;
                }
            }

            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pDataSeg;
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                pSecurityParam = (PBIN_PARAM_SECURITY)pdu->pAHS;
            }
            // end of supporting version

            if((pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) 
                || (pSecurityParam->LoginType != pSessionData->iLoginType)
                || (g_ntohs(pSecurityParam->AuthMethod) != AUTH_METHOD_CHAP)) {

                debug_emu(1, "BAD Third Request Parameter.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                goto MakeLoginReply;
            }
            pAuthChapParam = (PAUTH_PARAMETER_CHAP)pSecurityParam->u.AuthParameter;
            if(!(g_ntohl(pAuthChapParam->CHAP_A) == HASH_ALGORITHM_MD5)) {
                debug_emu(1, "Not Supported HASH Algorithm.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeLoginReply;
            }
            if((unsigned)g_ntohl(pAuthChapParam->CHAP_I) != pSessionData->CHAP_I) {
                debug_emu(1, "Bad CHAP_I.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeLoginReply;
            }

            // Store User ID(Name)
            pSessionData->iUser = g_ntohl(pAuthChapParam->CHAP_N);

            //debug_emu(1, "pSessionData->iUser = %x", pSessionData->iUser); 
            switch(pSessionData->iLoginType) {
            case LOGIN_TYPE_NORMAL:
                {
//                  xbool   bRW = FALSE;

                    // Target Exist?
                    if(pSessionData->iUser & 0x00000001) {  // Use Target0
                        if(PerTarget[0]->bPresent == FALSE) {
                            debug_emu(1, "No Target.");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
                            goto MakeLoginReply;
                        }

                    }
                    if(pSessionData->iUser & 0x00000002) {  // Use Target1
                        if(PerTarget[0]->bPresent == FALSE) {
                            debug_emu(1, "No Target.");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
                            goto MakeLoginReply;
                        }
                    }

                    // Select Passwd.
                    if((pSessionData->iUser & 0x00010001)
                        || (pSessionData->iUser & 0x00020002)) {
                        sal_memcpy(pSessionData->iPassword, Prom->UserPasswd, 8);
                    } else {
                        // ???
                        sal_memcpy(pSessionData->iPassword, Prom->UserPasswd, 8);
                    }

                    // Increse Login User Count.
                    if(pSessionData->iUser & 0x00000001) {  // Use Target0
                        sal_spinlock_take(EmuLock);
                        if(pSessionData->iUser &0x00010000) {
                            if(PerTarget[0]->NRRWHost > 0) {
                                //debug_emu(1, "Already RW. Logined");
                                pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                                sal_spinlock_give(EmuLock);
                                goto MakeLoginReply;
                            }
                            PerTarget[0]->NRRWHost++;
                            sal_spinlock_give(EmuLock);
                            //debug_emu(1, "!!!!!!!!!!!! RW Login !!!!!!!!!!!!!!");
                        } else {
                            PerTarget[0]->NRROHost++;
                            sal_spinlock_give(EmuLock);
                        }
                    }
                    if(pSessionData->iUser & 0x00000002) {  // Use Target0
                        sal_spinlock_take(EmuLock);
                        if(pSessionData->iUser &0x00020000) {
                            if(PerTarget[1]->NRRWHost > 0) {
//                              //debug_emu(1, "Already RW. Logined");
                                pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                                sal_spinlock_give(EmuLock);
                                goto MakeLoginReply;
                            }
                            PerTarget[1]->NRRWHost++;
                            sal_spinlock_give(EmuLock);
                        } else {
                            PerTarget[1]->NRROHost++;
                            sal_spinlock_give(EmuLock);
                        }
                    }
                    pSessionData->bIncCount = TRUE;
                }
                break;
            case LOGIN_TYPE_DISCOVERY:
                {
                    sal_memcpy(pSessionData->iPassword, Prom->UserPasswd, 8);
                }
                break;
            default:
                break;
            }

            //
            // Check CHAP_R
            //
            {
                unsigned char   result[16] = { 0 };

                Hash32To128(
                    (unsigned char*)&pSessionData->CHAP_C, 
                    result, 
                    (unsigned char*)&pSessionData->iPassword
                    );

                if(sal_memcmp(result, pAuthChapParam->CHAP_R, 16) != 0) {
                    debug_emu(1, "Auth Failed.");
/*                  sal_debug_print("Result=");
                    sal_debug_hexdump(result, 16);
                    sal_debug_print("CHAP_R=");
                    sal_debug_hexdump(pAuthChapParam->CHAP_R, 16);*/
                    pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED; //LANSCSI_RESPONSE_RI_AUTH_FAILED;
                    goto MakeLoginReply;                            
                }
            }

            pSecurityParam = NULL;
            pAuthChapParam = NULL;

            // Make Reply.
            pLoginReplyHeader->u.s.T = 1;
            pLoginReplyHeader->u.s.CSG = FLAG_SECURITY_PHASE;
            pLoginReplyHeader->u.s.NSG = FLAG_LOGIN_OPERATION_PHASE;
            pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;

            // to support version 1.1, 2.0 
            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pLoginReplyHeader->DataSegLen = g_htonl(BIN_PARAM_SIZE_REPLY);
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                pLoginReplyHeader->AHSLen = g_htons(BIN_PARAM_SIZE_REPLY);
            }
            // end of supporting version

            pSecurityParam = (PBIN_PARAM_SECURITY)(pLoginReplyHeader + 1);
            pSecurityParam->ParamType = BIN_PARAM_TYPE_SECURITY;
            pSecurityParam->LoginType = pSessionData->iLoginType;
            pSecurityParam->AuthMethod = g_htons(AUTH_METHOD_CHAP);
            // No need to set CHAP values.

            // Set Phase.
            pSessionData->iSessionPhase = FLAG_LOGIN_OPERATION_PHASE;
        }
        break;
    case 3:
        {
            //
            // Feature negotiation
            //
            debug_emu(4, "*** Fourth ***");
            // Check Flag.
            if((pLoginRequestHeader->u.s.T == 0)
                || (pLoginRequestHeader->u.s.CSG != FLAG_LOGIN_OPERATION_PHASE)
                || ((pLoginRequestHeader->u.Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)) {
                debug_emu(1, "BAD Fourth Flag.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeLoginReply;
            }

            // Check Parameter.

            // to support version 1.1, 2.0 
            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                if((g_ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST) // Minus AuthParameter[1]
                    || (pdu->pDataSeg == NULL)) {

                    debug_emu(1, "BAD Fourth Request Data.");
                    pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                    goto MakeLoginReply;
                }
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                if((g_ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST) // Minus AuthParameter[1]
                    || (pdu->pAHS == NULL)) {

                    debug_emu(1, "BAD Fourth Request Data.");
                    pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                    goto MakeLoginReply;
                }
            }

            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pParamNego = (PBIN_PARAM_NEGOTIATION)pdu->pDataSeg;
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                pParamNego = (PBIN_PARAM_NEGOTIATION)pdu->pAHS;
            }
            // end of supporting version

            if((pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION)) {
                debug_emu(1, "BAD Fourth Request Parameter.");
                pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeLoginReply;
            }
            pParamNego = NULL;

            // Make Reply.
            pLoginReplyHeader->u.s.T = 1;
            pLoginReplyHeader->u.s.CSG = FLAG_LOGIN_OPERATION_PHASE;
            pLoginReplyHeader->u.s.NSG = FLAG_FULL_FEATURE_PHASE;
            pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;

            // to support version 1.1, 2.0 
            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pLoginReplyHeader->DataSegLen = g_htonl(BIN_PARAM_SIZE_REPLY);
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                pLoginReplyHeader->AHSLen = g_htons(BIN_PARAM_SIZE_REPLY);
            }
            // end of supporting version

            pParamNego = (PBIN_PARAM_NEGOTIATION)(pLoginReplyHeader + 1);
            pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;
            pParamNego->HWType = HW_TYPE_ASIC;
            // fixed to support version 1.1, 2.0
            pParamNego->HWVersion = thisHWVersion;
            pParamNego->NRSlot = g_htonl(NDASEMU_MAX_PENDING_TASKS);
            pParamNego->MaxBlocks = g_htonl(MaxBlockSize);
            pParamNego->MaxTargetID = g_htonl(2);
            pParamNego->MaxLUNID = g_htonl(1);
            pParamNego->HeaderEncryptAlgo = g_htons(HeaderEncryptAlgo);
            pParamNego->HeaderDigestAlgo = 0;
            pParamNego->DataEncryptAlgo = g_htons(DataEncryptAlgo);
            pParamNego->DataDigestAlgo = 0;

            pSessionData->logincount++;
        }
        break;
    default:
        {
            debug_emu(1, "BAD Sub-Packet Sequence.");
            pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
            goto MakeLoginReply;
        }
        break;
    }
MakeLoginReply:
    pSessionData->CSubPacketSeq = g_ntohs(pLoginRequestHeader->CSubPacketSeq) + 1;

    pLoginReplyHeader->Opcode = LOGIN_RESPONSE;
    pLoginReplyHeader->VerMax = thisHWVersion; //LANSCSI_CURRENT_EMU_VERSION;
    pLoginReplyHeader->VerActive = thisHWVersion; //LANSCSI_CURRENT_EMU_VERSION;
    pLoginReplyHeader->Revision = g_htons(LANSCSI_CURRENT_EMU_REVISION);
    pLoginReplyHeader->ParameterType = PARAMETER_TYPE_BINARY;
    pLoginReplyHeader->ParameterVer = PARAMETER_CURRENT_VERSION;

    debug_emu(4, "Session(%p) : LoginReply : subpacketseq=%d Opcode = %d", 
      pSessionData, (int)g_ntohs(pLoginReplyHeader->CSubPacketSeq), pLoginReplyHeader->Opcode);

    if(pSessionData->CSubPacketSeq == 4) {
        debug_emu(1, "FULL FEATURE PHASE");
        pSessionData->CSubPacketSeq = 0;
        pSessionData->iSessionPhase = FLAG_FULL_FEATURE_PHASE;
    }

    return NDAS_OK;
}

ndas_error_t
ndasemu_task_logout_decode_service(
    PNDAS_TASK  ndastask
){
    PSESSION_DATA                       pSessionData = ndastask->session;
    PLANSCSI_LOGOUT_REQUEST_PDU_HEADER  pLogoutRequestHeader = &ndastask->pdu_req->logout;
    PLANSCSI_LOGOUT_REPLY_PDU_HEADER    pLogoutReplyHeader = &ndastask->pdu_rep->logout;

    debug_emu(4, "Session(%p) : Logout request recevied.", pSessionData);

    // Init reply PDU
    pLogoutReplyHeader->AHSLen = 0;
    pLogoutReplyHeader->DataSegLen = 0;
    pLogoutReplyHeader->u.s.F = 1;

    if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
        // Bad Command...
        debug_emu(1, "LOGOUT Bad Command.");
        pLogoutReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

        goto MakeLogoutReply;
    } 

    // Check Header.
    if((pLogoutRequestHeader->u.s.F == 0)
        || (pSessionData->HPID != (unsigned)g_ntohl(pLogoutRequestHeader->HPID))
        || (pSessionData->RPID != g_ntohs(pLogoutRequestHeader->RPID))
        || (pSessionData->CPSlot != g_ntohs(pLogoutRequestHeader->CPSlot))
        || (0 != g_ntohs(pLogoutRequestHeader->CSubPacketSeq))) {

        debug_emu(1, "LOGOUT Bad Port parameter.");

        pLogoutReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
        goto MakeLogoutReply;
    }

    // Do Logout.
    if(pSessionData->iLoginType == LOGIN_TYPE_NORMAL) {
        // Something to do...
    }

    // Make Reply.
    pLogoutReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;

MakeLogoutReply:
    pLogoutReplyHeader->Opcode = LOGOUT_RESPONSE;
    pSessionData->logincount--;

    return NDAS_OK;
}

ndas_error_t
ndasemu_task_text_decode_service(
    PNDAS_TASK  ndastask
){
    PSESSION_DATA                       pSessionData = ndastask->session;
    PLANSCSI_TEXT_REQUEST_PDU_HEADER    pRequestHeader = &ndastask->pdu_req->text;
    PLANSCSI_TEXT_REPLY_PDU_HEADER      pReplyHeader = &ndastask->pdu_rep->text;
    PLANSCSI_PDU_POINTERS                pdu = &ndastask->pdureq_pointers;
    xuchar              ucParamType = 0;
    int count, i;

    //debug_emu(1, "Text request");
    //
    // Init reply PDU.
    //
    pReplyHeader->AHSLen = 0;
    pReplyHeader->DataSegLen = 0;
    pReplyHeader->ParameterType = PARAMETER_TYPE_BINARY;
    pReplyHeader->ParameterVer = PARAMETER_CURRENT_VERSION;

    if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
        // Bad Command...
        debug_emu(1, "TEXT_REQUEST Bad Command.");
        pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

        goto MakeTextReply;
    }

    // Check Header.
    if((pRequestHeader->u.s.F == 0)
        || (pSessionData->HPID != (unsigned)g_ntohl(pRequestHeader->HPID))
        || (pSessionData->RPID != g_ntohs(pRequestHeader->RPID))
        || (pSessionData->CPSlot != g_ntohs(pRequestHeader->CPSlot))
        || (0 != g_ntohs(pRequestHeader->CSubPacketSeq))) {

        debug_emu(1, "TEXT Bad Port parameter.");

        pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
        goto MakeTextReply;
    }

    // Check Parameter.

    // to support version 1.1, 2.0 
    if (thisHWVersion == LANSCSI_VERSION_1_0) {
        if(g_ntohl(pRequestHeader->DataSegLen) < 4) {   // Minimum size.
            debug_emu(1, "TEXT No Data seg.");

            pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
            goto MakeTextReply;
        }
    }
    if (thisHWVersion == LANSCSI_VERSION_1_1 ||
        thisHWVersion == LANSCSI_VERSION_2_0) {
        if(g_ntohs(pRequestHeader->AHSLen) < 4) {   // Minimum size.
            debug_emu(1, "TEXT No AH seg.");

            pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
            goto MakeTextReply;
        }
    }
    // end of supporting version

    // to support version 1.1, 2.0 
    if (thisHWVersion == LANSCSI_VERSION_1_0) {
        ucParamType = ((PBIN_PARAM)pdu->pDataSeg)->ParamType;
    }
    if (thisHWVersion == LANSCSI_VERSION_1_1 ||
        thisHWVersion == LANSCSI_VERSION_2_0) {
        ucParamType = ((PBIN_PARAM)pdu->pAHS)->ParamType;
    }
    // end of supporting version

//              switch(((PBIN_PARAM)pdu->pDataSeg)->ParamType) {
//              debug_emu(1, "BIN_PARAM : type is %02X", ucParamType);
    switch(ucParamType) {
    case BIN_PARAM_TYPE_TARGET_LIST:
        {
            PBIN_PARAM_TARGET_LIST  pRepParam;

            pRepParam = (PBIN_PARAM_TARGET_LIST)(pReplyHeader + 1);
            pRepParam->ParamType = BIN_PARAM_TYPE_TARGET_LIST;
            count = 0;
            for(i=0;i<NR_MAX_TARGET;i++) 
                if(PerTarget[i]->bPresent) count ++;
            pRepParam->NRTarget = count;
            for(i = 0; i < pRepParam->NRTarget; i++) {
                pRepParam->PerTarget[i].TargetID = g_htonl(i);
                pRepParam->PerTarget[i].NRRWHost = PerTarget[i]->NRRWHost;
                pRepParam->PerTarget[i].NRROHost = PerTarget[i]->NRROHost;
                pRepParam->PerTarget[i].TargetDataHi = PerTarget[i]->TargetDataHi;
                pRepParam->PerTarget[i].TargetDataLow = PerTarget[i]->TargetDataLow;
            }

            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pReplyHeader->DataSegLen = g_htonl(BIN_PARAM_SIZE_REPLY); //g_htonl(4 + 8 * NRTarget);
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                    thisHWVersion == LANSCSI_VERSION_2_0) {
                pReplyHeader->AHSLen = g_htons(BIN_PARAM_SIZE_REPLY); //g_htonl(4 + 8 * NRTarget);
            }
        }
        break;
    case BIN_PARAM_TYPE_TARGET_DATA:
        {
            PBIN_PARAM_TARGET_DATA pReqParam, pRepParam;

            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pReqParam = (PBIN_PARAM_TARGET_DATA)pdu->pDataSeg;
            } else {
                pReqParam = (PBIN_PARAM_TARGET_DATA)pdu->pAHS;
            }
            pRepParam = (PBIN_PARAM_TARGET_DATA)(pReplyHeader + 1);
            pRepParam->ParamType = pReqParam->ParamType;
            pRepParam->GetOrSet = pReqParam->GetOrSet;

            if(pReqParam->GetOrSet == PARAMETER_OP_GET) {
                if(g_ntohl(pReqParam->TargetID) == 0) {
                    pRepParam->TargetDataHi = PerTarget[0]->TargetDataHi;
                    pRepParam->TargetDataLow = PerTarget[0]->TargetDataLow;
                } else if(g_ntohl(pReqParam->TargetID) == 1) {
                    pRepParam->TargetDataHi = PerTarget[1]->TargetDataHi;
                    pRepParam->TargetDataLow = PerTarget[1]->TargetDataLow;
                } else {
                    debug_emu(1, "No Access Right: TargetID=%x", pReqParam->TargetID);
                    pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                    goto MakeTextReply;
                }
            } else {
                if(g_ntohl(pReqParam->TargetID) == 0) {
                    PerTarget[0]->TargetDataHi = pReqParam->TargetDataHi;
                    PerTarget[0]->TargetDataLow = pReqParam->TargetDataLow;
                } else if(g_ntohl(pReqParam->TargetID) == 1) {
                    PerTarget[1]->TargetDataHi = pReqParam->TargetDataHi;
                    PerTarget[1]->TargetDataLow = pReqParam->TargetDataLow;
                } else {
                    debug_emu(1, "No Access Right: TargetID=%x", pReqParam->TargetID);
                    pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                    goto MakeTextReply;
                }
            }


            // to support version 1.1, 2.0 
            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pReplyHeader->DataSegLen = g_htonl(BIN_PARAM_SIZE_REPLY);
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                    thisHWVersion == LANSCSI_VERSION_2_0) {
                pReplyHeader->AHSLen = g_htons(BIN_PARAM_SIZE_REPLY);
            }
            // end of supporting version

        }
        break;
    default:
        break;
    }


    // to support version 1.1, 2.0 
    if (thisHWVersion == LANSCSI_VERSION_1_0) {
        pReplyHeader->DataSegLen = g_htonl(BIN_PARAM_SIZE_REPLY); //g_htonl(sizeof(BIN_PARAM_TARGET_DATA));
    }
    if (thisHWVersion == LANSCSI_VERSION_1_1 ||
            thisHWVersion == LANSCSI_VERSION_2_0) {
        pReplyHeader->AHSLen = g_htons(BIN_PARAM_SIZE_REPLY); //g_htonl(sizeof(BIN_PARAM_TARGET_DATA));
    }
    // end of supporting version

MakeTextReply:
    pReplyHeader->Opcode = TEXT_RESPONSE;

    return NDAS_OK;
}
// receive -> decode -> service -> reply
ndas_error_t
ndasemu_task_vendor_decode_service(
    PNDAS_TASK ndastask
){
    PSESSION_DATA   pSessionData = ndastask->session;
    PLANSCSI_VENDOR_REQUEST_PDU_HEADER  pRequestHeader = &ndastask->pdu_req->vendor;
    PLANSCSI_VENDOR_REPLY_PDU_HEADER    pReplyHeader = &ndastask->pdu_rep->vendor;
    int lock_index;
    PLOCK_TABLE_ENTRY lock;

    pReplyHeader->AHSLen = 0;
    pReplyHeader->DataSegLen = 0;
    pReplyHeader->VenderID = NKC_VENDOR_ID;
    pReplyHeader->VenderOpVersion = VENDOR_OP_CURRENT_VERSION;
    pReplyHeader->VenderOp = pRequestHeader->VenderOp;

    if((pRequestHeader->u.s.F == 0)
        || (pSessionData->HPID != (unsigned)g_ntohl(pRequestHeader->HPID))
        || (pSessionData->RPID != g_ntohs(pRequestHeader->RPID))
        || (pSessionData->CPSlot != g_ntohs(pRequestHeader->CPSlot))
        || (0 != g_ntohs(pRequestHeader->CSubPacketSeq))) {

        debug_emu(1, "Vender Bad Port parameter.");
        pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
        goto MakeVenderReply;
    }

    if( (pRequestHeader->VenderID != g_htons(NKC_VENDOR_ID)) ||
        (pRequestHeader->VenderOpVersion != VENDOR_OP_CURRENT_VERSION) 
    ) {
        debug_emu(1, "Vender Version don't match.");
        pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
        goto MakeVenderReply;

    }
    switch(pRequestHeader->VenderOp) {
        case VENDOR_OP_SET_MAX_RET_TIME:
            Prom->MaxRetTime = NTOHLL(pRequestHeader->VendorParameter.Int64);
            break;
        case VENDOR_OP_SET_MAX_CONN_TIME:
            Prom->MaxConnTime = NTOHLL(pRequestHeader->VendorParameter.Int64);
            break;
        case VENDOR_OP_GET_MAX_RET_TIME:
            pReplyHeader->VendorParameter.Int64 = HTONLL(Prom->MaxRetTime);
            break;
        case VENDOR_OP_GET_MAX_CONN_TIME:
            pReplyHeader->VendorParameter.Int64 = HTONLL(Prom->MaxConnTime);
            break;
        case VENDOR_OP_SET_SUPERVISOR_PW:
            sal_memcpy(Prom->SuperPasswd, pRequestHeader->VendorParameter.Bytes, 8);
            break;
        case VENDOR_OP_SET_USER_PW:
            sal_memcpy(Prom->UserPasswd, pRequestHeader->VendorParameter.Bytes, 8);
            break;
        case VENDOR_OP_RESET:
            pSessionData->iSessionPhase = LOGOUT_PHASE;
            break;
        case VENDOR_OP_SET_SEMA:
            lock_index = (NTOHLL(pRequestHeader->VendorParameter.Int64) >> 32) & 0x3;
            debug_emu(3, "Lock index = %d", lock_index);
            lock = &PerTarget[0]->LockTable[lock_index];
            if (lock->Locked) {
                // Already locked
                if (sal_memcmp((void*)lock->owner, pSessionData->Addr, 6)==0) {
                    debug_emu(3, "Lock request from lock owner");
                    pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
                } else {
                    debug_emu(3, "Already locked");
                    pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                }
            } else {
                lock->Locked = 1;
                pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
                sal_memcpy((void*)lock->owner, pSessionData->Addr, 6);
                pSessionData->HeldLock[lock_index] = TRUE;
                debug_emu(3, "Locked");
            }
            pRequestHeader->VendorParameter.Int64 = HTONLL((xuint64)lock->count);
            break;
        case VENDOR_OP_FREE_SEMA:
            lock_index = (NTOHLL(pRequestHeader->VendorParameter.Int64) >> 32) & 0x3;
            debug_emu(3, "Lock index = %d", lock_index);
            lock = &PerTarget[0]->LockTable[lock_index];
            pReplyHeader->VendorParameter.Int64 = HTONLL((xuint64)lock->count);
            if (lock->Locked) {
                if (sal_memcmp((void*)lock->owner, pSessionData->Addr, 6)==0) {
                    debug_emu(3, "Unlocked");
                    pSessionData->HeldLock[lock_index] = FALSE;
                    lock->Locked = 0;
                    lock->count++;
                } else {
                    debug_emu(3, "Not a lock owner");   
                }
            } else {
                debug_emu(1, "Tried to unlock not-locked lock");
            }
            pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
            break;
        case VENDOR_OP_GET_SEMA:
            lock_index = (NTOHLL(pRequestHeader->VendorParameter.Int64) >> 32) & 0x3;
            debug_emu(3, "Lock index = %d", lock_index);
            lock = &PerTarget[0]->LockTable[lock_index];
            pReplyHeader->VendorParameter.Int64 = HTONLL((xuint64)lock->count);
            pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
            break;
        case VENDOR_OP_GET_OWNER_SEMA:
            lock_index = (NTOHLL(pRequestHeader->VendorParameter.Int64) >> 32) & 0x3;
            debug_emu(3, "Lock index = %d", lock_index);
            lock = &PerTarget[0]->LockTable[lock_index];
            sal_memcpy((void*)((char*)(&pReplyHeader->VendorParameter.Int64)+2), (void*)lock->owner, 6);
            pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
            break;
        default:
            debug_emu(1, "Unhandled vendor op 0x%x.", pRequestHeader->VenderOp);
            pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;  //
            break;
    }

MakeVenderReply:
    pReplyHeader->Opcode = VENDOR_SPECIFIC_RESPONSE;

    return NDAS_OK;
}

ndas_error_t
ndasemu_task_ide_v0_decode(
    __in __out PNDAS_TASK ndastask
){
    PSESSION_DATA              pSessionData = ndastask->session;
    PLANSCSI_IDE_REQUEST_PDU_HEADER pRequestHeader = &ndastask->pdu_req->ide_v0;
    PLANSCSI_IDE_REPLY_PDU_HEADER   pReplyHeader = &ndastask->pdu_rep->ide_v0;

    //
    // Init reply PDU
    //
    pReplyHeader->AHSLen = 0;
    pReplyHeader->DataSegLen = 0;
    pReplyHeader->Opcode = IDE_RESPONSE;

    //
    // Copy registers to the reply
    //
    sal_memcpy(&pReplyHeader->Reserved2, &pRequestHeader->Reserved2, 16);
    pReplyHeader->DataTransferLength = pRequestHeader->DataTransferLength;

    //
    // Decode IDE registers
    //
    ndastask->targetid = g_ntohl(pRequestHeader->TargetID);
    ndastask->transferlen = g_ntohl(pRequestHeader->DataTransferLength);
    ndastask->command = pRequestHeader->Command;
    ndastask->feature = pRequestHeader->Feature;
    if(PerTarget[ndastask->targetid]->bLBA48 == TRUE) {

        ndastask->blockaddr = ((xint64)pRequestHeader->LBAHigh_Prev << 40)
            + ((xint64)pRequestHeader->LBAMid_Prev << 32)
            + ((xint64)pRequestHeader->LBALow_Prev << 24)
            + ((xint64)pRequestHeader->LBAHigh_Curr << 16)
            + ((xint64)pRequestHeader->LBAMid_Curr << 8)
            + ((xint64)pRequestHeader->LBALow_Curr);

        ndastask->blocklen = ((unsigned)pRequestHeader->SectorCount_Prev << 8)
            + (pRequestHeader->SectorCount_Curr);
    } else {
        ndastask->blockaddr = ((xint64)pRequestHeader->u2.s.LBAHeadNR << 24)
            + ((xint64)pRequestHeader->LBAHigh_Curr << 16)
            + ((xint64)pRequestHeader->LBAMid_Curr << 8)
            + ((xint64)pRequestHeader->LBALow_Curr);

        ndastask->blocklen = pRequestHeader->SectorCount_Curr;
    }


    // Check Header.
    if((pRequestHeader->u1.s.F == 0)
        || (pSessionData->HPID != (unsigned)g_ntohl(pRequestHeader->HPID))
        || (pSessionData->RPID != g_ntohs(pRequestHeader->RPID))
        || (pSessionData->CPSlot != g_ntohs(pRequestHeader->CPSlot))
        || (0 != g_ntohs(pRequestHeader->CSubPacketSeq))) {

        debug_emu(1, "IDE Bad Port parameter.");

        pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
        return NDAS_ERROR;
    }

    pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
    return NDAS_OK;
}

ndas_error_t
ndasemu_task_ide_v0_recv(
    __in __out PNDAS_TASK ndastask
){
    PSESSION_DATA              pSessionData = ndastask->session;
    PLANSCSI_IDE_REPLY_PDU_HEADER   pReplyHeader = &ndastask->pdu_rep->ide_v0;
    int             iResult = 0, res =0;

    //
    // No need AHS and DS for IDE command.
    //
    pReplyHeader->AHSLen = 0;
    pReplyHeader->DataSegLen = 0;

    // Common sanity check
    if(pSessionData->iLoginType != LOGIN_TYPE_NORMAL) {
        // Bad Command...
        debug_emu(1, "Not Normal Login.");
        pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

        goto MakeIDEReply;
    }

    if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
        // Bad Command...
        debug_emu(1, "Bad Command.");
        pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

        goto MakeIDEReply;
    }

    if(ndastask->transferlen > MaxBlockSize * EMU_BYTES_OF_BLOCK)
    {
        // Bad Command...
        debug_emu(1, "Too large data transfer:%u bytes.", ndastask->transferlen);
        pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

        return NDAS_ERROR;
    }

    // Request for existed target?
    if(PerTarget[ndastask->targetid]->bPresent == FALSE) {
        debug_emu(1, "Target Not exist.");

        pReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
        return NDAS_ERROR;
    }

    // LBA48 command? 
    if((PerTarget[ndastask->targetid]->bLBA48 == FALSE) &&
        ((ndastask->command == WIN_READDMA_EXT)
        || (ndastask->command == WIN_WRITEDMA_EXT))) {
        debug_emu(1, "Bad Command.");

        pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
        return NDAS_ERROR;
    }


    switch(ndastask->command) {
    case WIN_READDMA:
    case WIN_READDMA_EXT:
        {
            //
            // Check Bound.
            //
            if(((ndastask->blockaddr + ndastask->blocklen) * EMU_BYTES_OF_BLOCK) >
                PerTarget[ndastask->targetid]->Size)
            {
                debug_emu(1, "READ: Location = %lld, Sector_Size = %lld, TargetID = %d, Out of bound",
                    ndastask->blockaddr + ndastask->blocklen,
                    PerTarget[ndastask->targetid]->Size >> 9,
                    ndastask->targetid);
                pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeIDEReply;
            }
        }
        break;
    case WIN_WRITEDMA:
    case WIN_WRITEDMA_EXT:
        {
            //
            // Check access right.
            //
            if((pSessionData->iUser != FIRST_TARGET_RW_USER)
                && (pSessionData->iUser != SECOND_TARGET_RW_USER)) {
                debug_emu(1, "No Write right...");

                pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeIDEReply;
            }
            //
            // Check Bound.
            //
            if(((ndastask->blockaddr + ndastask->blocklen) * EMU_BYTES_OF_BLOCK) >
                PerTarget[ndastask->targetid]->Size) 
            {
                debug_emu(1, "WRITE: Out of bound");
                pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeIDEReply;
            }
#if 0 // defined(NDASEMU_ZEROCOPY_MODE)
            iResult = ndasemu_recvfile(ndastask->sockfd, PerTarget[ndastask->targetid]->Export, Location * EMU_BYTES_OF_BLOCK, (int)SectorCount * EMU_BYTES_OF_BLOCK, &res);
            sal_assert(task->bufseg);
            ndasemu_put_segment(ndastask->bufpool, ndastask->bufseg);
            ndastask->bufseg = NULL;
#else

            // Receive Data.
            ndastask->bufseg = ndasemu_get_segment(ndastask->bufpool, 1);
            sal_assert(ndastask->bufseg);
            iResult = RecvIt(
                ndastask->sockfd,
                ndastask->bufseg->bufseg_addr,
                ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                &res,
                NULL,
                NULL
                );
            if(iResult < 0 || res != ndastask->blocklen*EMU_BYTES_OF_BLOCK) {
                debug_emu(1, "Can't Recv Data...");
                pSessionData->iSessionPhase = LOGOUT_PHASE;
                return NDAS_ERROR;
            }

            //
            // Zero-copy mode decrypts the data on the fly during receving data.
            //
            //
            // Decrypt Data.
            //
            if(DataEncryptAlgo != 0) {
                Decrypt32(
                    (unsigned char*)ndastask->bufseg->bufseg_addr,
                    ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                    (unsigned char *)&pSessionData->CHAP_C,
                    (unsigned char *)&pSessionData->iPassword
                    );
            }
#endif

        }
        break;
    case WIN_VERIFY:
    case WIN_VERIFY_EXT:
        {
            debug_emu(1, "WIN_VERIFY");
            //
            // Check Bound.
            //
            if(((ndastask->blockaddr + ndastask->blocklen) * EMU_BYTES_OF_BLOCK) >
                PerTarget[ndastask->targetid]->Size) 
            {
                debug_emu(1, "Verify: Out of bound");
                pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeIDEReply;
            }
        }
        break;
    case WIN_SETFEATURES:
    case WIN_IDENTIFY:
        break;
    default:
        debug_emu(1, "Not Supported Command0 0x%x", ndastask->command);
        pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
        goto MakeIDEReply;
    }

    pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
MakeIDEReply:

    return iResult;
}

ndas_error_t
ndasemu_task_ide_v0_service(
    __in __out PNDAS_TASK ndastask
){
    PSESSION_DATA              pSessionData = ndastask->session;
    PLANSCSI_IDE_REPLY_PDU_HEADER   pReplyHeader = &ndastask->pdu_rep->ide_v0;
    int             iResult = 0, res =0;

    // init default value
    pReplyHeader->Command = READY_STAT;
    pReplyHeader->Feature = 0;

    switch(ndastask->command) {
    case WIN_READDMA:
    case WIN_READDMA_EXT:
        {
#if defined(NDASEMU_ZEROCOPY_MODE)
            iResult = ndasemu_sendfile(ndastask->sockfd, PerTarget[ndastask->targetid]->Export, ndastask->blockaddr * EMU_BYTES_OF_BLOCK, (int)ndastask->blocklen * EMU_BYTES_OF_BLOCK, &res);
#else
            ndastask->bufseg = ndasemu_get_segment(ndastask->bufpool, 1);
            sal_assert(ndastask->bufseg);
            res = sal_file_read(
                PerTarget[ndastask->targetid]->Export,
                ndastask->bufseg->bufseg_addr,
                (int)ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                ndastask->blockaddr * EMU_BYTES_OF_BLOCK);
#endif
            if (res!=ndastask->blocklen * EMU_BYTES_OF_BLOCK) {
                // failed to read from file/disk. 
                // Fail this operation.
                debug_emu(1, "Read from disk failed");
                pSessionData->iSessionPhase = LOGOUT_PHASE;
                return NDAS_ERROR;
            }
        }
        break;
    case WIN_WRITEDMA:
    case WIN_WRITEDMA_EXT:
        {
#if 0 // defined(NDASEMU_ZEROCOPY_MODE)
            iResult = ndasemu_recvfile(pSessionData->connSock, PerTarget[ndastask->targetid]->Export, Location * EMU_BYTES_OF_BLOCK, (int)SectorCount * EMU_BYTES_OF_BLOCK, &res);
            sal_assert(task->bufseg);
            ndasemu_put_segment(ndastask->bufpool, ndastask->bufseg);
            ndastask->bufseg = NULL;
#else
            res = sal_file_write(
                PerTarget[ndastask->targetid]->Export,
                ndastask->bufseg->bufseg_addr,
                (int)ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                ndastask->blockaddr * EMU_BYTES_OF_BLOCK);
            sal_assert(ndastask->bufseg);
            ndasemu_put_segment(ndastask->bufpool, ndastask->bufseg);
            ndastask->bufseg = NULL;
#endif
            if (res!=ndastask->blocklen *EMU_BYTES_OF_BLOCK) {
                // failed to read from file/disk. 
                // Fail this operation.
                debug_emu(1, "Writing to disk failed");
                pSessionData->iSessionPhase = LOGOUT_PHASE;
                return NDAS_ERROR;
            }

        }
        break;
    case WIN_VERIFY:
    case WIN_VERIFY_EXT:
        {
            debug_emu(1, "WIN_VERIFY");
        }
        break;
    case WIN_SETFEATURES:
        {
            int Mode;

            //debug_emu(1, "set features:");
            switch(ndastask->feature) {
                case SETFEATURES_XFER: 
                    Mode = ndastask->blocklen & 0xff; // extract SectorCount_Curr
                    if(Mode == XFER_MW_DMA_2) {
                        PerTarget[ndastask->targetid]->MWDMA &= 0xf8ff;
                        PerTarget[ndastask->targetid]->MWDMA|= (1 << ((Mode & 0x7) + 8));
                }
                    break;
                default:
                    break;
            }
        }
        break;
    case WIN_IDENTIFY:
        {
            struct  hd_driveid  *pInfo;
            char    serial_no[20] = { '0', '1', '0', '0', 0};
            char    firmware_rev[8] = {'.', '1', 0, '0', 0, };
            char    model[40] = { 'D', 'N', 'S', 'A', 'M', 'E', 'L', 'U', 0, 0,0, };

            //debug_emu(1, "Identify:");
            ndastask->bufseg = ndasemu_get_segment(ndastask->bufpool, 1);
            sal_assert(ndastask->bufseg);

            pInfo = (struct hd_driveid *)ndastask->bufseg->bufseg_addr;
            sal_memset(pInfo, 0, sizeof(struct hd_driveid));
            pInfo->field_valid = GINT16_TO_LE(0x0006);
            pInfo->lba_capacity = PerTarget[ndastask->targetid]->Size / EMU_BYTES_OF_BLOCK;
            pInfo->lba_capacity_2 = PerTarget[ndastask->targetid]->Size / EMU_BYTES_OF_BLOCK;
            pInfo->heads = 255;
            pInfo->sectors = 63;
            pInfo->cyls = pInfo->lba_capacity / (pInfo->heads * pInfo->sectors);
            pInfo->capability |= 0x0001;        // DMA
            if(PerTarget[ndastask->targetid]->bLBA) pInfo->capability |= 0x0002; // LBA
            if(PerTarget[ndastask->targetid]->bLBA48) { // LBA48
                pInfo->cfs_enable_2 |= 0x0400;
                pInfo->command_set_2 |= 0x0400;
            }
            pInfo->major_rev_num = 0x0004 | 0x0008 | 0x010; // ATAPI 5
            pInfo->dma_mword = PerTarget[ndastask->targetid]->MWDMA;
            pInfo->dma_ultra = PerTarget[ndastask->targetid]->UDMA;

            sal_memcpy(pInfo->serial_no, serial_no, 20);
            sal_memcpy(pInfo->fw_rev, firmware_rev, 8);
            sal_memcpy(pInfo->model, model, 40);

        }

        break;
    default:
        debug_emu(1, "Not Supported Command0 0x%x", ndastask->command);
        pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
        goto MakeIDEReply;
    }

    pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
MakeIDEReply:
    pReplyHeader->Opcode = IDE_RESPONSE;

    return iResult;
}

ndas_error_t
ndasemu_task_ide_v0_send(
    __in __out PNDAS_TASK ndastask
){
    PSESSION_DATA              pSessionData = ndastask->session;
    PLANSCSI_IDE_REPLY_PDU_HEADER   pReplyHeader = &ndastask->pdu_rep->ide_v0;
    int             iResult = 0, res =0;

    switch(ndastask->command) {
    case WIN_READDMA:
    case WIN_READDMA_EXT:
        {
#if !defined(NDASEMU_ZEROCOPY_MODE)
            //
            // Encrption.
            //
            if(DataEncryptAlgo != 0) {
                Encrypt32(
                    (unsigned char*)ndastask->bufseg->bufseg_addr,
                    ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                    (unsigned char *)&pSessionData->CHAP_C,
                    (unsigned char *)&pSessionData->iPassword
                    );
            }
#if 0
            // Send Data.
            iResult = SendIt(
                pSessionData->connSock,
                (xuchar*)(*buf_seg)->bufseg_addr,
                SectorCount * EMU_BYTES_OF_BLOCK,
                &res
                );
#else

            iResult = ndasemu_sendpages(
                ndastask,
                ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                &res);

#endif
            if(iResult < 0 || res != ndastask->blocklen*EMU_BYTES_OF_BLOCK) {
                debug_emu(1, "Can't Send READ Data...");
                pSessionData->iSessionPhase = LOGOUT_PHASE;
                return NDAS_ERROR;
            }
#endif
        }
        break;
    case WIN_WRITEDMA:
    case WIN_WRITEDMA_EXT:
    case WIN_VERIFY:
    case WIN_VERIFY_EXT:
    case WIN_SETFEATURES:
        break;
    case WIN_IDENTIFY:
        {
            // See ATAPI spec. for valid value.
            pReplyHeader->Command = 0x40;
            pReplyHeader->Feature = 0;
    
            //
            // Encrption.
            //
            if(DataEncryptAlgo != 0) {
                Encrypt32(
                    (unsigned char*)ndastask->bufseg->bufseg_addr,
                    EMU_IDENTIFY_BYTES,
                    (unsigned char *)&pSessionData->CHAP_C,
                    (unsigned char *)&pSessionData->iPassword
                    );
            }
    
            // Send Data.
            iResult = SendIt(
                pSessionData->connSock,
                ndastask->bufseg->bufseg_addr,
                EMU_IDENTIFY_BYTES,
                &res
                );
            sal_assert(ndastask->bufseg);
            ndasemu_put_segment(ndastask->bufpool, ndastask->bufseg);
            ndastask->bufseg = NULL;
            if(iResult < 0 || res != EMU_IDENTIFY_BYTES) {
                debug_emu(1, "Can't Send Identify Data...");
                pSessionData->iSessionPhase = LOGOUT_PHASE;
                return NDAS_ERROR;
            }

        }

        break;
    default:
        debug_emu(1, "Not Supported Command0 0x%x", ndastask->command);
        pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
        goto MakeIDEReply;
    }

    pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
MakeIDEReply:
    pReplyHeader->Opcode = IDE_RESPONSE;

    // Send reply
    iResult = ndasemu_send_reply(
        pSessionData,
        (PLANSCSI_R2H_PDU_HEADER)pReplyHeader,
        ndastask);

    return iResult;
}


ndas_error_t
ndasemu_task_ide_decode(
    __in __out PNDAS_TASK ndastask
){
    PSESSION_DATA              pSessionData = ndastask->session;
    PLANSCSI_IDE_REQUEST_PDU_HEADER_V1 pRequestHeader = &ndastask->pdu_req->ide;
    PLANSCSI_IDE_REPLY_PDU_HEADER_V1   pReplyHeader = &ndastask->pdu_rep->ide;

    //
    // Init reply PDU
    //
    pReplyHeader->AHSLen = 0;
    pReplyHeader->DataSegLen = 0;
    pReplyHeader->Opcode = IDE_RESPONSE;

    //
    // Copy registers to the reply
    //
    sal_memcpy(&pReplyHeader->Comm.CommTypeLength, &pRequestHeader->Comm.CommTypeLength, 16);
    pReplyHeader->DataTransferLength = pRequestHeader->DataTransferLength;

    //
    // Decode.
    //
    ndastask->targetid = g_ntohl(pRequestHeader->TargetID);
    ndastask->transferlen = g_ntohl(pRequestHeader->DataTransferLength);
    ndastask->command = pRequestHeader->Command;
    // High 1 byte: feature previous
    // Low 1 byte: feature current
    ndastask->feature = ((xuint16)pRequestHeader->Feature_Prev << 8) +
        pRequestHeader->Feature_Curr;
    if(PerTarget[ndastask->targetid]->bLBA48 == TRUE) {
        ndastask->blockaddr = ((xint64)pRequestHeader->LBAHigh_Prev << 40)
            + ((xint64)pRequestHeader->LBAMid_Prev << 32)
            + ((xint64)pRequestHeader->LBALow_Prev << 24)
            + ((xint64)pRequestHeader->LBAHigh_Curr << 16)
            + ((xint64)pRequestHeader->LBAMid_Curr << 8)
            + ((xint64)pRequestHeader->LBALow_Curr);
        ndastask->blocklen = ((unsigned)pRequestHeader->SectorCount_Prev << 8)
            + (pRequestHeader->SectorCount_Curr);
    } else {
        ndastask->blockaddr = ((xint64)pRequestHeader->u2.s.LBAHeadNR << 24)
            + ((xint64)pRequestHeader->LBAHigh_Curr << 16)
            + ((xint64)pRequestHeader->LBAMid_Curr << 8)
            + ((xint64)pRequestHeader->LBALow_Curr);

        ndastask->blocklen = pRequestHeader->SectorCount_Curr;
    }

    // Check Header.
    if((pRequestHeader->u1.s.F == 0)
        || (pSessionData->HPID != (unsigned)g_ntohl(pRequestHeader->HPID))
        || (pSessionData->RPID != g_ntohs(pRequestHeader->RPID))
        || (pSessionData->CPSlot != g_ntohs(pRequestHeader->CPSlot))
        || (0 != g_ntohs(pRequestHeader->CSubPacketSeq))) {

        debug_emu(1, "IDE Bad Port parameter.");

        pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
        return NDAS_ERROR;
    }

    pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;

    return NDAS_OK;
}

ndas_error_t
ndasemu_task_ide_recv(
    __in __out PNDAS_TASK ndastask
){
    PSESSION_DATA              pSessionData = ndastask->session;
    PLANSCSI_IDE_REPLY_PDU_HEADER_V1   pReplyHeader = &ndastask->pdu_rep->ide;
    int             iResult, res =0;

    //
    // Convert Location and Sector Count.
    //
    pReplyHeader->AHSLen = 0;
    pReplyHeader->DataSegLen = 0;

    // Common sanity check
    if(pSessionData->iLoginType != LOGIN_TYPE_NORMAL) {
        // Bad Command...
        debug_emu(1, "Not Normal Login.");
        pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

        goto error_out;
    }

    if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
        // Bad Command...
        debug_emu(1, "Bad Command.");
        pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

        goto error_out;
    }

    if(ndastask->transferlen > MaxBlockSize * EMU_BYTES_OF_BLOCK)
    {
        // Bad Command...
        debug_emu(1, "Too large data. sector count=%d transfer:%u bytes.",
            ndastask->blocklen, ndastask->transferlen);
        pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

        return NDAS_ERROR;
    }

    // Request for existed target?
    if(PerTarget[ndastask->targetid]->bPresent == FALSE) {
        debug_emu(1, "Target Not exist.");

        pReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
        return NDAS_ERROR;
    }

    // LBA48 command? 
    if((PerTarget[ndastask->targetid]->bLBA48 == FALSE) &&
        ((ndastask->command == WIN_READDMA_EXT)
        || (ndastask->command == WIN_WRITEDMA_EXT))) {
        debug_emu(1, "Bad Command.");

        pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
        return NDAS_ERROR;
    }


    switch(ndastask->command) {
    case WIN_READDMA:
    case WIN_READDMA_EXT:
        // Check requested block length.
        // Not required check for VERIFY command.
        if(ndastask->blocklen > MaxBlockSize) {
            debug_emu(1, "Too large requested block. sector count=%d transfer:%u bytes.",
                ndastask->blocklen, ndastask->transferlen);
            pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

            return NDAS_ERROR;
        }
    // Intentional fall-through
    case WIN_VERIFY:
    case WIN_VERIFY_EXT:
        {
            //
            // Check Bound.
            //
            if(((ndastask->blockaddr + ndastask->blocklen) * EMU_BYTES_OF_BLOCK) >
                PerTarget[ndastask->targetid]->Size)
            {
                debug_emu(1, "READ: Location = %lld, Sector_Size = %lld, TargetID = %d, Out of bound", 
                    ndastask->blockaddr + ndastask->blocklen,
                    PerTarget[ndastask->targetid]->Size >> 9,
                    ndastask->targetid);
                pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto error_out;
            }
        }
        break;
    case WIN_WRITEDMA:
    case WIN_WRITEDMA_EXT:
        {
            // Check requested block length.
            if(ndastask->blocklen > MaxBlockSize) {
                debug_emu(1, "Too large requested block. sector count=%d transfer:%u bytes.",
                    ndastask->blocklen, ndastask->transferlen);
                pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;

                return NDAS_ERROR;
            }

#if 0 // allow writing in RO mode to support multi-write.                       
            //
            // Check access right.
            //
            if((pSessionData->iUser != FIRST_TARGET_RW_USER)
                && (pSessionData->iUser != SECOND_TARGET_RW_USER)) {
                debug_emu(1, "No Write right...");

                pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto MakeIDEReply1;
            }
#endif
            //
            // Check Bound.
            //
            if(((ndastask->blockaddr + ndastask->blocklen) * EMU_BYTES_OF_BLOCK) >
                PerTarget[ndastask->targetid]->Size)
            {
                debug_emu(1, "WRITE: Out of bound");
                pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                goto error_out;
            }

            // Receive Data.
            ndastask->bufseg = ndasemu_get_segment(ndastask->bufpool, 1);
            sal_assert(ndastask->bufseg);
        //  do_gettimeofday(&start);
            iResult = RecvIt(
                ndastask->sockfd,
                ndastask->bufseg->bufseg_addr,
                ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                &res,
                NULL,
                NULL
                );
            if(iResult < 0 || res != ndastask->blocklen*EMU_BYTES_OF_BLOCK) {
                debug_emu(1, "Can't Recv Data...");
                pSessionData->iSessionPhase = LOGOUT_PHASE;
                return NDAS_ERROR;
            }

            //
            // Decrypt Data.
            //
            if(DataEncryptAlgo != 0) {
                Decrypt32(
                    (unsigned char*)ndastask->bufseg->bufseg_addr,
                    ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                    (unsigned char *)&pSessionData->CHAP_C,
                    (unsigned char *)&pSessionData->iPassword
                    );
            }
        }
        break;
    case WIN_SETFEATURES:
    case WIN_SETMULT:
    case WIN_CHECKPOWERMODE1:
    case WIN_STANDBY:
    case WIN_IDENTIFY:
    case WIN_PIDENTIFY:
    case WIN_FLUSH_CACHE_EXT:
    case WIN_FLUSH_CACHE:
    break;
    default:
        debug_emu(1, "Not Supported Command 0x%x", ndastask->command);
        pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
        goto error_out;
    }

    pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
error_out:
    return NDAS_OK;
}

ndas_error_t
ndasemu_task_ide_service(
    __in __out PNDAS_TASK ndastask
){
    PLANSCSI_IDE_REPLY_PDU_HEADER_V1   pReplyHeader = &ndastask->pdu_rep->ide;
    int             res =0;

    // init default value
    pReplyHeader->Command = READY_STAT;
    pReplyHeader->Feature_Curr = 0;
    debug_emu(1, "Command 0x%x 0x%x", ndastask->command, pReplyHeader->Command);

    switch(ndastask->command) {
    case WIN_READDMA:
    case WIN_READDMA_EXT:
        {
#if defined(NDASEMU_ZEROCOPY_MODE)
            ndasemu_sendfile(
                ndastask->sockfd,
                PerTarget[ndastask->targetid]->Export,
                ndastask->blockaddr * EMU_BYTES_OF_BLOCK,
                (int)ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                &res);
#else
            ndastask->bufseg = ndasemu_get_segment(ndastask->bufpool, 1);
            sal_assert(ndastask->bufseg);
#if 1
            debug_emu(1, "TargetID %d", ndastask->targetid);
            debug_emu(1, "READDMA/EXT %p %p %p %d %lld",
                PerTarget[ndastask->targetid]->Export,
                ndastask->bufseg,
                ndastask->bufseg->bufseg_addr,
                (int)ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                ndastask->blockaddr * EMU_BYTES_OF_BLOCK);
            res = sal_file_read(
                PerTarget[ndastask->targetid]->Export,
                ndastask->bufseg->bufseg_addr,
                (int)ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                ndastask->blockaddr * EMU_BYTES_OF_BLOCK);
#else
            res = (int)ndastask->blocklen * EMU_BYTES_OF_BLOCK;
#endif
#endif
            if (res!=ndastask->blocklen *EMU_BYTES_OF_BLOCK)
            {
                pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                debug_emu(1, "Reading from disk failed");
                goto error_out;
            }
        }
        break;
    case WIN_WRITEDMA:
    case WIN_WRITEDMA_EXT:
        {
            res = sal_file_write(
                PerTarget[ndastask->targetid]->Export,
                ndastask->bufseg->bufseg_addr,
                (int)ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                ndastask->blockaddr * EMU_BYTES_OF_BLOCK);
            sal_assert(ndastask->bufseg);
            ndasemu_put_segment(ndastask->bufpool, ndastask->bufseg);
            ndastask->bufseg = NULL;
            if (res!=ndastask->blocklen *EMU_BYTES_OF_BLOCK)
            {
                pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                debug_emu(1, "Writing to disk failed");
                goto error_out;
            }

        }
        break;
    case WIN_VERIFY:
    case WIN_VERIFY_EXT:
        {
            pReplyHeader->Command = 0x40;
            pReplyHeader->Feature_Curr = 0;
        }
        break;
    case WIN_SETFEATURES:
        {
            int Mode;


            // See only feature current
            //debug_emu(1, "set features:");
            // fixed to support version 1.1, 2.0

            switch(ndastask->feature & 0xff) {
                case SETFEATURES_XFER:
                    Mode = ndastask->blocklen & 0xff; // extract SectorCount_Curr
                    if((Mode & 0xf0) == 0x00) {         // PIO

                        PerTarget[ndastask->targetid]->PIO &= 0x00ff;
                        PerTarget[ndastask->targetid]->PIO |= (1 << ((Mode & 0xf) + 8));

                    } else if((Mode & 0xf0) == 0x20) {  // Multi-word DMA
                        PerTarget[ndastask->targetid]->MWDMA &= 0x00ff;
                        PerTarget[ndastask->targetid]->MWDMA|= (1 << ((Mode & 0xf) + 8));
                    } else if((Mode & 0xf0) == 0x40) {  // Ultra DMA

                        PerTarget[ndastask->targetid]->UDMA &= 0x00ff;
                        PerTarget[ndastask->targetid]->UDMA |= (1 << ((Mode & 0xf) + 8));

                    } else {
                        pReplyHeader->Command = ERR_STAT;
                        pReplyHeader->Feature_Curr = ABRT_ERR;
                        break;
                    }

                    break;
                default:
                    pReplyHeader->Command = ERR_STAT;
                    pReplyHeader->Feature_Curr = ABRT_ERR;

                    break;
            }
        }
        break;
    // to support version 1.1, 2.0
    case WIN_SETMULT:
        {
            debug_emu(1, "set multiple mode");
            pReplyHeader->Command = 0x40;
            pReplyHeader->Feature_Curr = 0;
        }
        break;
    case WIN_CHECKPOWERMODE1:
        {
            int Mode;

            Mode = ndastask->blocklen & 0xff; // extract SectorCount_Curr
            //debug_emu(1, "check power mode = 0x%02x", Mode);
            pReplyHeader->Command = 0x40;
            pReplyHeader->Feature_Curr = 0;                     
        }
        break;
    case WIN_STANDBY:
        {
            //debug_emu(1, "standby");
            pReplyHeader->Command = 0x40;
            pReplyHeader->Feature_Curr = 0;                     
        }
        break;
    case WIN_IDENTIFY:
    case WIN_PIDENTIFY:
        {
            struct  hd_driveid  *pInfo;
#ifdef __SOFTWARE_DISK_INFO__
            char    serial_no[20] = { '0', '1', '0', '0', 0};
            char    firmware_rev[8] = {'.', '1', 0, '0', 0, };
            char    model[40] = { 'D', 'N', 'S', 'A', 'M', 'E', 'L', 'U', 0, 0,0, };
#endif

            debug_emu(1, "Identify:");

            ndastask->bufseg = ndasemu_get_segment(ndastask->bufpool, 1);
            sal_assert(ndastask->bufseg);

            pInfo = (struct hd_driveid *) ndastask->bufseg->bufseg_addr;
            sal_memset(pInfo, 0, sizeof(struct hd_driveid));

#ifdef __SOFTWARE_DISK_INFO__
            pInfo->lba_capacity = PerTarget[ndastask->targetid]->Size / EMU_BYTES_OF_BLOCK;
            pInfo->lba_capacity_2 = PerTarget[ndastask->targetid]->Size / EMU_BYTES_OF_BLOCK;
            pInfo->heads = 255;
            pInfo->sectors = 63;
            pInfo->cyls = pInfo->lba_capacity / ((pInfo->heads+1) * (pInfo->sectors+1));
            pInfo->capability |= 0x0001;        // DMA

            if(PerTarget[ndastask->targetid]->bLBA) pInfo->capability |= 0x0002; // LBA
            if(PerTarget[ndastask->targetid]->bLBA48) { // LBA48
                pInfo->cfs_enable_2 |= 0x0400;
                pInfo->command_set_2 |= 0x0400;
            }
            pInfo->major_rev_num = 0x0004 | 0x0008 | 0x010; // ATAPI 5
            pInfo->dma_mword = PerTarget[ndastask->targetid]->MWDMA;
            pInfo->dma_ultra = PerTarget[ndastask->targetid]->UDMA;

            sal_memcpy(pInfo->serial_no, serial_no, 20);
            sal_memcpy(pInfo->fw_rev, firmware_rev, 8);
            sal_memcpy(pInfo->model, model, 40);
#else
            sal_memcpy(pInfo, PerTarget[ndastask->targetid]->Info, EMU_IDENTIFY_BYTES);
            pInfo->dma_mword = PerTarget[ndastask->targetid]->MWDMA;
#endif
            // command and feature will be set later.

//  The Linux kernel (normally) likes to deal with a 1K block size, 
//  if the drive contains an odd number of sectors, this scheme leaves the last sector unreachable
//  so, report disk size as even number of sectors.

            pInfo->lba_capacity = pInfo->lba_capacity & ~1UL;
            pInfo->lba_capacity_2 = pInfo->lba_capacity_2 & ~1UL;

        }
        break;
    case WIN_FLUSH_CACHE_EXT:
    case WIN_FLUSH_CACHE: {
            //
            //  Set return register values
            //

            pReplyHeader->Command = 0x40;
            pReplyHeader->Feature_Curr = 0;
            break;
    }
    default: ;
    }
    pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
error_out:
    return NDAS_OK;
}

ndas_error_t
ndasemu_task_ide_send(
    __in __out PNDAS_TASK ndastask
){
    PSESSION_DATA              pSessionData = ndastask->session;
    PLANSCSI_IDE_REPLY_PDU_HEADER_V1   pReplyHeader = &ndastask->pdu_rep->ide;
    int             iResult, res =0;

    switch(ndastask->command) {
    case WIN_READDMA:
    case WIN_READDMA_EXT:
        {
#if !defined(NDASEMU_ZEROCOPY_MODE)
        //
        // Send block data
        //

        // Encrption.
        if(DataEncryptAlgo != 0) {
            Encrypt32(
                (unsigned char*)ndastask->bufseg->bufseg_addr,
                ndastask->blocklen * EMU_BYTES_OF_BLOCK,
                (unsigned char *)&pSessionData->CHAP_C,
                (unsigned char *)&pSessionData->iPassword
                );
        }
        // Send Data.
    //      do_gettimeofday(&start);
#if 0
        iResult = SendIt(
            pSessionData->connSock,
            (xuchar*)(*buf_seg)->bufseg_addr,
            ndastask->blocklen * EMU_BYTES_OF_BLOCK,
            &res
            );
#else

        iResult = ndasemu_sendpages(
            ndastask,
            ndastask->blocklen * EMU_BYTES_OF_BLOCK,
            &res);
#endif
    //      do_gettimeofday(&end);
    //      debug_emu(1, "send data :   ");
    //      elasped_time(start, end);
        if(iResult < 0 || res != ndastask->blocklen*EMU_BYTES_OF_BLOCK) {
            debug_emu(1, "Can't Send READ data iResult=%d, res=%d", iResult,res);

            pSessionData->iSessionPhase = LOGOUT_PHASE;
            goto MakeIDEReply1;
        }

#endif
        }
        break;
    case WIN_IDENTIFY:
    case WIN_PIDENTIFY:
        {
        // See ATAPI spec. for valid value.
        pReplyHeader->Command = 0x40;
        pReplyHeader->Feature_Curr = 0;

        //
        // Encrption.
        //
        if(DataEncryptAlgo != 0) {
            Encrypt32(
                (unsigned char*)ndastask->bufseg->bufseg_addr,
                EMU_IDENTIFY_BYTES,
                (unsigned char *)&pSessionData->CHAP_C,
                (unsigned char *)&pSessionData->iPassword
                );
        }

        // Send Data.
        iResult = SendIt(
            pSessionData->connSock,
            (xuchar*)ndastask->bufseg->bufseg_addr,
            EMU_IDENTIFY_BYTES,
            &res
            );
        sal_assert(ndastask->bufseg);
        ndasemu_put_segment(ndastask->bufpool, ndastask->bufseg);
        ndastask->bufseg = NULL;
        if(iResult < 0 || res != EMU_IDENTIFY_BYTES) {
            debug_emu(1, "Can't Send Identify Data...");
            pSessionData->iSessionPhase = LOGOUT_PHASE;
            goto MakeIDEReply1;
        }

        }
        break;
    default: ;
    }

    pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;

MakeIDEReply1:
    pReplyHeader->Opcode = IDE_RESPONSE;
    // Send reply
    iResult = ndasemu_send_reply(
        pSessionData,
        (PLANSCSI_R2H_PDU_HEADER)pReplyHeader,
        ndastask);

    return iResult;
}


static
inline
unsigned int
ndasemu_task_idx(unsigned int task_sn)
{
    return (task_sn % NDASEMU_MAX_PENDING_TASKS);
}


/*
 * Task thread
 */

void*
TaskThreadProc(
	void *param
)
{
    struct SESSION_THREAD_PARAM * thread_param = (struct SESSION_THREAD_PARAM *) param;
	PSESSION_DATA			pSessionData;
	int				iResult;
	int 				i;
    PNDAS_TASK cur_ndastask;
    int pending_tasks;

    sal_thread_block_signals();
	sal_thread_daemonize("ndemu");
	sal_thread_nice(-20);

	if(MaxBlockSize * EMU_BYTES_OF_BLOCK > MAX_DATA_BUFFER_SIZE) {
		lpx_close(thread_param->sock);
        sal_free(thread_param);
		debug_emu(1, "Too large MaxBlockSize");
		return NULL;
	}

    //
    // Init session data
    //
    pSessionData = sal_malloc(sizeof(SESSION_DATA));
    if(pSessionData == NULL) {
        lpx_close(thread_param->sock);
        sal_free(thread_param);
        debug_emu(1, "Failed to alloc session data");
        return NULL;
    }

    sal_memset(pSessionData, 0, sizeof(SESSION_DATA));
	pSessionData->connSock = thread_param->sock;
	pSessionData->CSubPacketSeq = 0;
    pSessionData->logincount = 0;
	pSessionData->bIncCount = FALSE;
	pSessionData->iSessionPhase = FLAG_SECURITY_PHASE;
    pSessionData->next_avail_task = &pSessionData->tasks[0];
    pSessionData->rtask_sn = 0;
    pSessionData->rtask_pending_sn = 0;
	sal_memcpy(pSessionData->Addr,thread_param->slpx.slpx_node, 6);

    for(i=0;i<NETDISK_DEV_LOCK_COUNT;i++)
		pSessionData->HeldLock[i] = FALSE;

    for(i=0;i<NDASEMU_MAX_PENDING_TASKS;i++) {
        ndasemu_init_task(
            &pSessionData->tasks[i],
            pSessionData->connSock,
            thread_param->ndasemu_bufferpool,
            NULL
            );
        pSessionData->tasks[i].pdu_req = NULL;
        pSessionData->tasks[i].pdu_rep = (PNDASEMU_PDU_REP)sal_malloc_ex(
            sizeof(NDASEMU_PDU_REP), SAL_MEMFLAG_DMA);
        if(pSessionData->tasks[i].pdu_rep == NULL) {
            int idx_task;
            for(idx_task = 0; idx_task < i; idx_task++) {
                sal_free(pSessionData->tasks[idx_task].pdu_rep);
            }
            lpx_close(thread_param->sock);
            sal_free(thread_param);
            debug_emu(1, "Failed to alloc pdu");
            return NULL;
        }
        pSessionData->tasks[i].pdu_rep->gen.LunHi = 0;
        pSessionData->tasks[i].pdu_rep->gen.LunLow = 0;
        pSessionData->tasks[i].session = pSessionData;
        iResult = sal_get_pages_from_virtual(
            (void *)pSessionData->tasks[i].pdu_rep,
            sizeof(NDASEMU_PDU_REP),
            pSessionData->tasks[i].pdurep_pages,
            &pSessionData->tasks[i].pdurep_offset
            );
       sal_assert(iResult >= 0);
       debug_emu(4, "Task%d maxpdusize=%d pdusize=%d page=%p %p offset=%d", 
        i,
        MAX_REQUEST_SIZE,
        sizeof(NDASEMU_PDU_REP),
        pSessionData->tasks[i].pdurep_pages[0],
        pSessionData->tasks[i].pdurep_pages[1],
        pSessionData->tasks[i].pdurep_offset);
    }

    //
    // Worker's main loop
    //
    pending_tasks = 0;
    iResult = 0;
	while (pSessionData->iSessionPhase != LOGOUT_PHASE) {

        for(;pending_tasks < NDASEMU_MAX_PENDING_TASKS &&
            pSessionData->iSessionPhase != LOGOUT_PHASE;) {

            // Get the next task
            cur_ndastask = pSessionData->next_avail_task;

            //
            // Receive an NDAS task ( request )
            //
            if(pending_tasks == 0) {
                iResult = ndasemu_receive_request(
                    cur_ndastask,
                    NDASEMU_RECVREQ_FLAG_LOOKAHEAD);
                if(iResult <= 0) {
                    debug_emu(1, "Can't Read Request.");

                    pSessionData->iSessionPhase = LOGOUT_PHASE;
                    continue;
                }
            } else if(pending_tasks > 0) {
                //
                // Workaround for no buffer segments avail
                // because of immediate data.
                //
                if(!ndasemu_is_bufseg_avail(cur_ndastask->bufpool)) {
                    // Exit to process pending tasks.
                    debug_emu(1, "buffer full. exit..%d", pending_tasks);
                    break;
                }

                iResult = ndasemu_receive_request(
                    cur_ndastask,
                    NDASEMU_RECVREQ_FLAG_NO_WAIT|NDASEMU_RECVREQ_FLAG_LOOKAHEAD);
                if(iResult < 0) {
                    debug_emu(1, "Can't Read Request.");
                    pSessionData->iSessionPhase = LOGOUT_PHASE;
                    continue;
                } else if(iResult == 0) {
                    // Exit to process pending tasks.
                    debug_emu(4, "No more task. exit..%d", pending_tasks);
                    break;
                }
                // Another task arrives.
            } else {
                pending_tasks = 0;
                continue;
            }
            // Move to the next blank task after successful receive of a request.
            pSessionData->rtask_sn++;
            pSessionData->next_avail_task = 
                &pSessionData->tasks[ndasemu_task_idx(pSessionData->rtask_sn)];

            pending_tasks ++;

            //
            // Check task sanity and receive immediate data.
            //
            debug_emu(1, "fd %d: Opcode %x", pSessionData->connSock, cur_ndastask->pdu_rep->gen.Opcode);
            switch(cur_ndastask->pdu_rep->gen.Opcode) {
            case LOGOUT_RESPONSE:
            case LOGIN_RESPONSE:
            case TEXT_RESPONSE:
            case VENDOR_SPECIFIC_RESPONSE:
            case NOP_R2H:
                iResult = 0;
                break;
            case IDE_RESPONSE:
                if (thisHWVersion == LANSCSI_VERSION_1_0)
                {
                    iResult = ndasemu_task_ide_v0_recv(
                        cur_ndastask);
                }
                else if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                    thisHWVersion == LANSCSI_VERSION_2_0)
                {
                    iResult = ndasemu_task_ide_recv(
                        cur_ndastask);
                }
                break;

            default:
                // pending counts
                pending_tasks --;
                pSessionData->rtask_pending_sn++;
                // Bad Command...
                iResult = -1;
                break;
            }

            if(iResult < 0) {
                debug_emu(1, "collecting pending tasks() failed.");
                break;
            }
        }
        if(iResult < 0) {
            pSessionData->iSessionPhase = LOGOUT_PHASE;
        }
        if(pSessionData->iSessionPhase == LOGOUT_PHASE)
            continue;

        iResult = 0;
        debug_emu(4, "pending=%d", pending_tasks);
        for( ; pending_tasks > 0; pending_tasks--, pSessionData->rtask_pending_sn++)
        {
            cur_ndastask = &pSessionData->tasks[ndasemu_task_idx(pSessionData->rtask_pending_sn)];
            switch(cur_ndastask->pdu_rep->gen.Opcode) {
            case LOGOUT_RESPONSE:
                if(cur_ndastask->pdu_rep->gen.Response == LANSCSI_RESPONSE_SUCCESS) {
                    pSessionData->iSessionPhase = LOGOUT_PHASE;
                }
                // Fall through
            case LOGIN_RESPONSE:
            case TEXT_RESPONSE:
            case VENDOR_SPECIFIC_RESPONSE:
                // Send reply
                iResult = ndasemu_send_reply(
                    pSessionData,
                    &cur_ndastask->pdu_rep->gen,
                    cur_ndastask);
                break;
            case NOP_R2H:
                // do not reply to NOP
                {
                    iResult = 0;
                    continue;
                }
                break;
            case IDE_RESPONSE:
                if (thisHWVersion == LANSCSI_VERSION_1_0) {
                    iResult = ndasemu_task_ide_v0_service(
                        cur_ndastask);
                    if(iResult < 0) {
                        debug_emu(1, "ndasemu_task_ide_service failed.");
                        continue;
                    }
                    iResult = ndasemu_task_ide_v0_send(
                        cur_ndastask);
                    if(iResult < 0) {
                        debug_emu(1, "ndasemu_task_ide_send failed.");
                        continue;
                    }
                }
                else if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                    thisHWVersion == LANSCSI_VERSION_2_0)
                {
                    iResult = ndasemu_task_ide_service(
                        cur_ndastask);
                    if(iResult < 0) {
                        debug_emu(1, "ndasemu_task_ide_service failed.");
                        continue;
                    }
                    iResult = ndasemu_task_ide_send(
                        cur_ndastask);
                    if(iResult < 0) {
                        debug_emu(1, "ndasemu_task_ide_send failed.");
                        continue;
                    }
                } else {
                    debug_emu(1, "Invalid NDAS hardware version %d", thisHWVersion);
                    iResult = -1;
                }
                break;
            default:
                debug_emu(1, "Task %p OPCODE %x not supported.",
                    cur_ndastask, cur_ndastask->pdu_rep->gen.Opcode);
                iResult = -1;
            }
        }
        if(iResult < 0) {
            pSessionData->iSessionPhase = LOGOUT_PHASE;
        }
        if(pSessionData->iSessionPhase == LOGOUT_PHASE)
            continue;
    }


    //
    // Clean up the session
    //
	
    //debug_emu(1, "Logout Phase.");
	switch(pSessionData->iLoginType) {
	case LOGIN_TYPE_NORMAL:
		{
			if(pSessionData->bIncCount == TRUE) {
				
				// Decrese Login User Count.
				if(pSessionData->iUser & 0x00000001) {	// Use Target0
					if(pSessionData->iUser &0x00010000) {
						sal_spinlock_take(EmuLock);
						PerTarget[0]->NRRWHost--;
						sal_spinlock_give(EmuLock);
					} else {
						sal_spinlock_take(EmuLock);
						PerTarget[0]->NRROHost--;
						sal_spinlock_give(EmuLock);
					}
				}
				if(pSessionData->iUser & 0x00000002) {	// Use Target1
					if(pSessionData->iUser &0x00020000) {
						sal_spinlock_take(EmuLock);
						PerTarget[1]->NRRWHost--;
						sal_spinlock_give(EmuLock);
					} else {
						sal_spinlock_take(EmuLock);
						PerTarget[1]->NRROHost--;
						sal_spinlock_give(EmuLock);
					}
				}
			}
		}
		break;
	case LOGIN_TYPE_DISCOVERY:
	default:
		break;
	}

	for(i=0;i<NETDISK_DEV_LOCK_COUNT;i++) {
		if (pSessionData->HeldLock[i]) {
			pSessionData->HeldLock[i] = FALSE;
			PerTarget[0]->LockTable[i].Locked = FALSE;
		}
	}
    debug_emu(4, "NDAS device locks freed");


	if (pSessionData->connSock) {
		lpx_close(pSessionData->connSock);
        debug_emu(4, "Socket closed");
    }

    for(i = 0; i < NDASEMU_MAX_PENDING_TASKS; i++) {
        sal_free(pSessionData->tasks[i].pdu_rep);
    }
    if(pSessionData)
        sal_free(pSessionData);
    if(thread_param)
        sal_free(thread_param);

    debug_emu(1, "Terminated.");
    return 0;
}

/*
 * PnP message generator
 */

typedef struct _PNP_MESSAGE {
    xuchar ucType;
    xuchar ucVersion;
} __x_attribute_packed__ PNP_MESSAGE, *PPNP_MESSAGE;

void* GenPnpMessage(void *arg)
{
	struct sockaddr_lpx slpx;
	int pnpsock;
	int result;
	PNP_MESSAGE message;

    sal_thread_block_signals();
	sal_thread_daemonize("GenPnp");

	pnpsock = lpx_socket(LPX_TYPE_DATAGRAM, 0);
	if (pnpsock < 0) {
	    debug_emu(1, "Can't open socket for lpx:");
	    return NULL;
	}

#if 0 // check this is required..
		result = sock_setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&perm, sizeof(perm));
        if (result < 0) {
                debug_emu(1, "Error! when setsockopt...: ");
				sock_release(sock);
                return 0;
        }
#endif

    sal_memset(&slpx, 0, sizeof(slpx));
    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = g_htons(LPX_PORT_NUMBER_PNP-1); //???

    result = lpx_bind(pnpsock, &slpx, sizeof(slpx));
    if (result < 0) {
        debug_emu(1, "Error! when binding...: ");
        goto pnpout;
    }

    sal_memset(&slpx, 0, sizeof(slpx));
    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = g_htons(LPX_PORT_NUMBER_PNP);
    slpx.slpx_node[0] = 0xFF;
    slpx.slpx_node[1] = 0xFF;
    slpx.slpx_node[2] = 0xFF;
    slpx.slpx_node[3] = 0xFF;
    slpx.slpx_node[4] = 0xFF;
    slpx.slpx_node[5] = 0xFF;

    sal_memset(&message, 0, sizeof(message));
    message.ucType = 0;
    message.ucVersion = thisHWVersion;

	GenPnpRunning = TRUE;
	while(GenPnpRunning) {
		result = SendtoIt(pnpsock, (void *)&message, sizeof(message), 0, &slpx, sizeof(slpx));
        if (result < 0) {
            debug_emu(1, "Can't send broadcast message: ");
			goto pnpout;
		}
		sal_msleep(2500);
	}
pnpout:
	lpx_close(pnpsock);
	sal_semaphore_give(GenPnpExited);
	sal_thread_exit(0);
	return NULL;
}

/*
 * Listen thread
 */
void* ListenThreadProc(void* arg)
{
	int AcceptedSock;
	xuint32 addr_len;
	sal_thread_id ThreadId;
	struct sockaddr_lpx slpx;
	struct SESSION_THREAD_PARAM *stparam;
	int i;

    sal_thread_block_signals();
	sal_thread_daemonize("EmuListen");	
	ListenThreadRunning = TRUE;

	while (ListenThreadRunning) {
		//
		// to do: add way to stop this thread voluntarily
		//
        debug_emu(1, "Wait for accepting from listen sock %d", ListenSock);
        addr_len = sizeof(slpx);	
		AcceptedSock = lpx_accept(ListenSock, &slpx, &addr_len);
        if (AcceptedSock < 0) {
			debug_emu(1, "failed to accept");
			goto listen_out;
        }
        /* allocate thread context */
        stparam = (struct SESSION_THREAD_PARAM *)sal_malloc(sizeof(struct SESSION_THREAD_PARAM));
        if(stparam == NULL) {
            lpx_close(AcceptedSock);
            debug_emu(1, "failed to allocate thread context");
            continue;
        }
        stparam->sock = AcceptedSock;
        stparam->slpx = slpx;
        stparam->ndasemu_bufferpool = &global_databuffer;
        debug_emu(1, "Accepted a connection from %02x:%02x:%02x:%02x:%02x:%02x, fd=%d", 
                0x0ff & slpx.slpx_node[0], 0x0ff & slpx.slpx_node[1], 0x0ff & slpx.slpx_node[2],
                0x0ff & slpx.slpx_node[3], 0x0ff & slpx.slpx_node[4], 0x0ff & slpx.slpx_node[5],
                AcceptedSock);
        sal_thread_create(&ThreadId, "con session",  0, 0, TaskThreadProc, (void*)stparam);
        stparam = NULL;
	}
listen_out:

    for(i=0; i<NR_MAX_TARGET; i++) {
		if(PerTarget[i]->Export != NULL) {
			sal_file_close(PerTarget[i]->Export);
			PerTarget[i]->Export = NULL;
		}
	}
	sal_semaphore_give(ListenThreadExited);
	sal_thread_exit(0);	
	return 0;
}
/*
 * Buffer management
 */
int
ndasemu_is_bufseg_avail(PNDASEMU_BUFFER_POOL ndasemu_bufferpool)
{
    return sal_semaphore_getvalue(ndasemu_bufferpool->buffer_avail) > 0;
}

PNDASEMU_BUFFER_SEGMENT
ndasemu_get_segment(PNDASEMU_BUFFER_POOL ndasemu_bufferpool, int wait)
{
    int i;
    int iret;
    PNDASEMU_BUFFER_SEGMENT segment;

    sal_assert(ndasemu_bufferpool);

    // Wait for the segment avail event if the caller is waitable.
    do {
        iret = sal_semaphore_take(ndasemu_bufferpool->buffer_avail, wait?SAL_SYNC_FOREVER:SAL_SYNC_NOWAIT);
    } while (iret == SAL_SYNC_INTERRUPTED);
    if(iret != SAL_SYNC_OK) {
        debug_emu(1, "No segment available.");
        return NULL;
    }

    sal_spinlock_take(ndasemu_bufferpool->buffer_mutex);
    // 1. Search the recent used segment first.
    if(ndasemu_bufferpool->recent_segment) {
        segment = ndasemu_bufferpool->recent_segment;
        ndasemu_bufferpool->recent_segment = NULL;
        goto segment_acquired;
    }

    // 2. Loop until available segment found.
    for(i = 0; i < NDASEMU_BUFFER_MAX_SEGMENT_NUM; i++) {
        segment = &ndasemu_bufferpool->segments[i];
        if(segment->bufseg_flags & NDASEMU_DATABUFFER_ACQUIRED) {
            continue;
        }

        break;
    }

    if(i >= NDASEMU_BUFFER_MAX_SEGMENT_NUM) {
        segment = NULL;
        // Semaphore protect the resources. This must not happen.
        sal_assert(FALSE);
    }

segment_acquired:
    if(segment) {
        // set acqurired segment flag
        segment->bufseg_flags |= NDASEMU_DATABUFFER_ACQUIRED;
        // increase acquired segment count.
        ndasemu_bufferpool->acquired_count++;
        sal_assert(ndasemu_bufferpool->acquired_count > 0);
        sal_assert(ndasemu_bufferpool->acquired_count <= NDASEMU_BUFFER_MAX_SEGMENT_NUM);
    }
    sal_spinlock_give(ndasemu_bufferpool->buffer_mutex);

    return segment;
}

void
ndasemu_put_segment(
    PNDASEMU_BUFFER_POOL ndasemu_bufferpool,
    PNDASEMU_BUFFER_SEGMENT segment)
{
    sal_assert(ndasemu_bufferpool);
    sal_assert(segment);

    sal_spinlock_take(ndasemu_bufferpool->buffer_mutex);
    // Clear acquired flag.
    segment->bufseg_flags &= ~NDASEMU_DATABUFFER_ACQUIRED;
    // set recent used segment.
    ndasemu_bufferpool->recent_segment = segment;
    // decrease acquired segment count.
    ndasemu_bufferpool->acquired_count --;
    sal_assert(ndasemu_bufferpool->acquired_count >= 0);
    sal_assert(ndasemu_bufferpool->acquired_count < NDASEMU_BUFFER_MAX_SEGMENT_NUM);
    // Notify segment availability.
    sal_semaphore_give(ndasemu_bufferpool->buffer_avail);
    sal_spinlock_give(ndasemu_bufferpool->buffer_mutex);
}


void ndasemu_destroy_buffer_pool(
    PNDASEMU_BUFFER_POOL ndasemu_bufferpool);

ndas_error_t
ndasemu_init_buffer_pool(PNDASEMU_BUFFER_POOL ndasemu_bufferpool)
{
    int i;
    int iResult;
    PNDASEMU_BUFFER_SEGMENT segment;

/*    ndasemu_bufferpool->buffer_mutex = SAL_INVALID_MUTEX; */
    ndasemu_bufferpool->buffer_avail = SAL_INVALID_SEMAPHORE;
    ndasemu_bufferpool->recent_segment = NULL;
    ndasemu_bufferpool->acquired_count = 0;

    iResult = sal_spinlock_create("emu_buffer", &ndasemu_bufferpool->buffer_mutex);
    if(!iResult) {
        return NDAS_ERROR;
    }
    ndasemu_bufferpool->buffer_avail = sal_semaphore_create(
        "emu_avail",
        NDASEMU_BUFFER_MAX_SEGMENT_NUM,
        NDASEMU_BUFFER_MAX_SEGMENT_NUM);
    if(ndasemu_bufferpool->buffer_avail == SAL_INVALID_SEMAPHORE) {
        sal_spinlock_destroy(ndasemu_bufferpool->buffer_mutex);
        return NDAS_ERROR;
    }
    debug_emu(1, "Initial buff_avail = %d",
        sal_semaphore_getvalue(ndasemu_bufferpool->buffer_avail));

    for(i = 0; i < NDASEMU_BUFFER_MAX_SEGMENT_NUM; i++) {
        segment = &ndasemu_bufferpool->segments[i];
#if defined(NDASEMU_ZEROCOPY_MODE)
        segment->bufseg_addr = (xuchar *)sal_malloc_ex(
            MaxBlockSize * EMU_BYTES_OF_BLOCK,
            0);
        if(segment->bufseg_addr == NULL)
        {
            debug_emu(1, "Failed to alloc data buffer %d", i);
            goto error_out;
        }
        segment->bufseg_pages = NULL;
#else
        segment->bufseg_addr = (xuchar *)sal_malloc_ex(
            MaxBlockSize * EMU_BYTES_OF_BLOCK,
            SAL_MEMFLAG_DMA);
        if(segment->bufseg_addr == NULL)
        {
            debug_emu(1, "Failed to alloc data buffer %d", i);
            goto error_out;
        }
        segment->bufseg_pages = sal_alloc_page_array(
            segment->bufseg_addr,
            MaxBlockSize * EMU_BYTES_OF_BLOCK,
            0);
        if(segment->bufseg_pages == NULL)
        {
            debug_emu(1, "Failed to alloc data page array for buffer %d", i);
            goto error_out;
        }
        iResult = sal_get_pages_from_virtual(
            segment->bufseg_addr,
            MaxBlockSize * EMU_BYTES_OF_BLOCK,
            segment->bufseg_pages,
            &segment->bufseg_offset
            );
        if(iResult < 0) {
            debug_emu(1, "Failed to alloc data page array for buffer %d", i);
            goto error_out;
        }
        debug_emu(1, "buffer%d offset=%d", i, segment->bufseg_offset);

        /*
         * When using kmalloc the page _count is incremented only for the first page???
         * If we will use DirectIO, This will cause that after the dio/bio is completed
         * the page will be release due to zero page reference count, and we don't want it.
         * See in direct-io.c get_ker_pages inc, page_cache_release dec and free.
         */
        iResult = sal_inc_memref(
            segment->bufseg_addr,
            MaxBlockSize * EMU_BYTES_OF_BLOCK);
        if(iResult < 0) {
            debug_emu(1, "Failed to alloc data page array for buffer %d", i);
            goto error_out;
        }
#endif
    }

    return NDAS_OK;

error_out:
    ndasemu_destroy_buffer_pool(ndasemu_bufferpool);
    return NDAS_ERROR;
}

void
ndasemu_destroy_buffer_pool(PNDASEMU_BUFFER_POOL ndasemu_bufferpool)
{
    int i;
    PNDASEMU_BUFFER_SEGMENT segment;

    for(i = 0; i < NDASEMU_BUFFER_MAX_SEGMENT_NUM; i++) {
        segment = &ndasemu_bufferpool->segments[i];
        if (segment->bufseg_addr == NULL)
            continue;

#if !defined(NDASEMU_ZEROCOPY_MODE)
        sal_dec_memref(segment->bufseg_addr, MaxBlockSize * EMU_BYTES_OF_BLOCK);
        debug_emu(4, "Data buffer referece decreased");
#endif
        if(segment->bufseg_pages) {
            sal_free(segment->bufseg_pages);
            segment->bufseg_pages = NULL;
        }
        sal_free(segment->bufseg_addr);
        segment->bufseg_addr = NULL;
        debug_emu(4, "Data buffer freed");
    }
    if(ndasemu_bufferpool->buffer_avail != SAL_INVALID_SEMAPHORE) {
            sal_semaphore_destroy(ndasemu_bufferpool->buffer_avail);
            ndasemu_bufferpool->buffer_avail = SAL_INVALID_SEMAPHORE;
    }
    sal_spinlock_destroy(ndasemu_bufferpool->buffer_mutex);
}

//
// Start the emulator
// max_transfer_size is in kbytes
//
NDASUSER_API
ndas_error_t
ndas_emu_start(
    char* devfile,
    int version,
    int max_transfer_size
){
	int					i;
	char				*tdata;
	struct sockaddr_lpx slpx;
	int 				result;
	ndas_error_t 		ndaserr;
	xbool			bret;
	xuchar SuperPw[] = HASH_KEY_SUPER2;   

	if (max_transfer_size > 0 && max_transfer_size<=EMU_MAX_BLOCKS/2) {
		MaxBlockSize = max_transfer_size * 2;// transfer size is in kbytes.
	} else {
		return NDAS_ERROR;
	}
    if (version >2) {
        return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
    }
    ndaserr = ndasemu_init_buffer_pool(&global_databuffer);
    if(ndaserr < 0) {
        return ndaserr;
    }

    tdata = sal_malloc(sizeof(TARGET_DATA) * NR_MAX_TARGET);
	if(tdata == NULL) {
		debug_emu(1, "Failed to alloc TARGET DATA");
		return NDAS_ERROR_OUT_OF_MEMORY;
	}
	sal_memset(tdata, 0, sizeof(TARGET_DATA) * NR_MAX_TARGET);
	for(i=0;i<NR_MAX_TARGET;i++) {
		PerTarget[i] = (PTARGET_DATA)tdata;
		tdata += sizeof(TARGET_DATA);
	}

	Prom = (PPROM_DATA) sal_malloc(sizeof(PROM_DATA));
	if(Prom == NULL) {
		sal_free(tdata);
		debug_emu(1, "Failed to alloc PROM DATA");
		return NDAS_ERROR_OUT_OF_MEMORY;
	}

	Prom->MaxConnTime = 4999; // 5 sec
	Prom->MaxRetTime = 63; // 63 ms
	sal_memcpy(Prom->UserPasswd, NDIDV1PW_DEFAULT, 8);
	sal_memcpy(Prom->SuperPasswd, SuperPw, 8);	

    for(i=0; i<NR_MAX_TARGET; i++) {
		PerTarget[i]->ExportDev = devfile;
        debug_emu(1, "dev = %s", PerTarget[i]->ExportDev);

#if defined(NDASEMU_ZEROCOPY_MODE)
        PerTarget[i]->Export = sal_file_open(devfile, SAL_FILE_FLAG_RW, 0);
#else
#if defined(NDASEMU_DIRECT_IO)
        PerTarget[i]->Export = sal_file_open(devfile, SAL_FILE_FLAG_RW | SAL_FILE_FLAG_DIRECT, 0);
#else
        PerTarget[i]->Export = sal_file_open(devfile, SAL_FILE_FLAG_RW, 0);
#endif
#endif
        if (PerTarget[i]->Export == NULL) {
			debug_emu(1, "Can not open file: %s", devfile);
			return NDAS_ERROR;
        }
		// We don't need to use actual HDD's value
        PerTarget[i]->bLBA = TRUE;
        PerTarget[i]->bLBA48 = TRUE;
	PerTarget[i]->MWDMA = 0x207; 
	PerTarget[i]->PIO = 0x0; 
	PerTarget[i]->UDMA = 0x0; 
	
	bret = sal_file_get_size(PerTarget[i]->Export, &PerTarget[i]->Size);
	if (bret == FALSE) {
		debug_emu(1, "Failed to get size");
		return NDAS_ERROR;
	}

#ifndef __SOFTWARE_DISK_INFO__
	bret = sal_get_identify(PerTarget[i]->Export, PerTarget[i]->Info);
	if (bret == FALSE) {
		debug_emu(1, "Failed to get identify");
		return NDAS_ERROR;
	}
#endif

        PerTarget[i]->bPresent = TRUE;
        PerTarget[i]->NRRWHost = 0;
        PerTarget[i]->NRROHost = 0;
        PerTarget[i]->TargetDataHi = 0;
        PerTarget[i]->TargetDataLow = 0;

        debug_emu(1, "%d th Disk_Size = %llx", i, PerTarget[i]->Size);
    }

    sal_spinlock_create("emu", &EmuLock);

    thisHWVersion = version;

	ListenSock = lpx_socket(LPX_TYPE_STREAM, 0);

 	if (ListenSock < 0) {
		debug_emu(1, "failed to create socket!");
		goto ndemu_out;
	} else {
		debug_emu(1, "Listen sock = %d", ListenSock);
	}

	sal_memset(&slpx, 0, sizeof(slpx));
	slpx.slpx_family = AF_LPX;
	slpx.slpx_port = g_htons(LPX_PORT_NUMBER);

	ndaserr = lpx_bind(ListenSock, &slpx, sizeof(slpx));
	
	if (!NDAS_SUCCESS(ndaserr)) {
		debug_emu(1, "failed to bind !");
		goto ndemu_out;
	}

	ndaserr = lpx_listen(ListenSock, MAX_CONNECTION);

	if (!NDAS_SUCCESS(ndaserr)) {
		debug_emu(1, "failed to listen !");
		goto ndemu_out;
	}

	GenPnpExited = sal_semaphore_create("pnp_exited", 1, 0);
	GenPnpRunning = FALSE;
	
	// Create PNP thread
    result = sal_thread_create(&GenPnpThreadId, "genpnp", -2, 0, GenPnpMessage, NULL);
	if (result <0) {
		debug_emu(1, "failed to create GenPNP thread");
		goto ndemu_out;
	}

	ListenThreadExited = sal_semaphore_create("emulisten", 1, 0);
	ListenThreadRunning = FALSE;

	sal_thread_create(&ListenThreadId, "emulisten", -2, 0, ListenThreadProc, NULL);
	return 0;
ndemu_out:
	if (ListenSock) {
		lpx_close(ListenSock);
		ListenSock = 0;
	}
    for(i=0; i<NR_MAX_TARGET; i++) {
		if(PerTarget[i]->Export != NULL) {
			sal_file_close(PerTarget[i]->Export);
			PerTarget[i]->Export = NULL;
		}
	}
	return NDAS_ERROR;
}
/*
 * Stop the emulator
 */
NDASUSER_API ndas_error_t ndas_emu_stop(void)
{
        int ret;

	debug_emu(1, "Stopping ndemu");

	// Wait for listening thread stop
	if (ListenThreadRunning) {
		ListenThreadRunning = FALSE;
		lpx_close(ListenSock);
        do {
            ret = sal_semaphore_take(ListenThreadExited, SAL_SYNC_FOREVER); // wait till thread dead
        } while (ret == SAL_SYNC_INTERRUPTED);

	    sal_semaphore_destroy(ListenThreadExited);
	}
	debug_emu(1, "Listening thread exited");

	// Wait for all session thread stop

	// Wait for PNP gen thread to stop.
	if (GenPnpRunning) {
		GenPnpRunning = FALSE;
            do {
                ret = sal_semaphore_take(GenPnpExited, SAL_SYNC_FOREVER); // wait till thread dead
            } while (ret == SAL_SYNC_INTERRUPTED);
	    
	    sal_semaphore_destroy(GenPnpExited);
	}
	debug_emu(1, "PNP thread exited");
#ifndef XPLAT_EMBEDDED
    if(EmuLock != SAL_INVALID_MUTEX) {
        sal_spinlock_destroy(EmuLock);
    } 
#endif
    if (PerTarget[0]) {
        sal_free(PerTarget[0]);
    }
    if (Prom) {
        sal_free(Prom);
    }
    //
    // Free the data buffer
    //
    ndasemu_destroy_buffer_pool(&global_databuffer);

	return NDAS_OK;
}

#else /* NDAS_EMU */
NDASUSER_API ndas_error_t ndas_emu_start(char* devfile, int version, int max_transfer_size)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}
NDASUSER_API ndas_error_t ndas_emu_stop(void)
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}
#endif

