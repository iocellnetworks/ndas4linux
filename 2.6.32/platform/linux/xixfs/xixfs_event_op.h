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
*/
#ifndef __XIXFS_EVENT_OP_H__
#define __XIXFS_EVENT_OP_H__

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <xixfsevent/xixfs_event.h>

#include "xixfs_global.h"
#include "xixfs_types.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"

#include "xixcore/evtpkt.h"

#define XI_HTONS		cpu_to_be16
#define XI_HTONL		cpu_to_be32
#define XI_HTONLL		cpu_to_be64 
#define XI_NTOHS		be16_to_cpu
#define XI_NTOHL		be32_to_cpu
#define XI_NTOHLL		be64_to_cpu 


#define	DEFAULT_XIXFS_CLIWAIT				( 1 * XIXFS_TIC_P_SEC)	// 1 seconds.
#define DEFAULT_REQUEST_MAX_TIMEOUT			(5 * XIXFS_TIC_P_SEC)
#define MAX_XI_DATAGRAM_DATA_SIZE			(1024 - sizeof(XIXFS_COMM_HEADER))

typedef union _XIXFSDG_RAWPK_DATA {
		XIXFS_LOCK_REQUEST					LockReq;
		XIXFS_LOCK_REPLY						LockReply;
		XIXFS_LOCK_BROADCAST					LockBroadcast;
		XIXFS_RANGE_FLUSH_BROADCAST			FlushReq;
		XIXFS_DIR_ENTRY_CHANGE_BROADCAST		DirChangeReq;
		XIXFS_FILE_LENGTH_CHANGE_BROADCAST	FileLenChangeReq;
		XIXFS_FILE_CHANGE_BROADCAST			FileChangeReq;
		XIXFS_FILE_RANGE_LOCK_BROADCAST		FileRangeLockReq;
}__attribute__((packed, aligned(8)))
XIXFSDG_RAWPK_DATA, *PXIXFSDG_RAWPK_DATA;

#define LOCK_INVALID				(0x00000000)
#define LOCK_OWNED_BY_OWNER		(0x00000001)
#define LOCK_NOT_OWNED			(0x00000002)
#define LOCK_TIMEOUT				(0x00000004)

typedef struct _XIXFSDG_CTL {
	struct completion		*PktCompletion;
	int					*status;
}XIXFSDG_CTL, *PXIXFSDG_CTL;


typedef struct _XIXFSDG_PKT{
	//
	//	raw packet
	//	Do not insert any code between RawHeadDG and RawDataDG
	//	We assume that RawHead and RawData are continuos memory area.
	//
	//	do not insert field befoure ...
	XIXFS_COMM_HEADER			RawHeadDG ;
	XIXFSDG_RAWPK_DATA		RawDataDG ;	
	struct sockaddr_lpx	SourceAddr ;
	//  packed data

	atomic_t				RefCnt ;				// reference count
	long					Flags ;			
	uint64				TimeOut;
	struct list_head		PktListEntry ;
	
	int					PacketSize ;
	int					DataSize ;			// Data part size in packet.

	XIXFSDG_CTL			PkCtl;
#if LINUX_VERSION_WORK_QUEUE
	struct work_struct		Pktwork;
#else
	struct tq_struct		Pktwork;
#endif
	
	//--> 40 bytes

}XIXFSDG_PKT, *PXIXFSDG_PKT;




int
xixfs_event_server(void * ctx);

int
xixfs_event_client(void *ctx);

void 
xixfs_event_handler_SrvErrorNetwork(void * ctx);

void 
xixfs_event_handler_CliErrorNetwork(void * ctx);;

void 
xixfs_event_handler_RcvPaket(void * ctx);



int
xixfs_AllocDGPkt(
		PXIXFSDG_PKT	*ppkt,
		uint8 			*SrcMac,
		uint8			*DstMac,
		uint32			Type
) ;


void
xixfs_DereferenceDGPkt(PXIXFSDG_PKT pkt) ;


void 
xixfs_Pkt_add_sendqueue(
		PXIXFS_SERVICE_CTX SrvCtx,
		PXIXFSDG_PKT		Pkt	
	);

void
xixfs_event_handler_RcvPaket(void * ctx);



#endif //#ifndef __XIXFS_EVENT_OP_H__
