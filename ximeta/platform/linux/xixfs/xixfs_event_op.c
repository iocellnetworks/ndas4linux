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
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#include "linux_ver.h"
#if LINUX_VERSION_25_ABOVE
#include <linux/fs.h> // blkdev_ioctl , register file system
#include <linux/buffer_head.h> // file_fsync
#include <linux/genhd.h>  // rescan_parition
#include <linux/workqueue.h>  // 
#else
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h>

#include "xcsystem/debug.h"
#include "xixcore/layouts.h"
#include "xcsystem/system.h"
#include "xixcore/ondisk.h"
#include "xixcore/lotlock.h"
#include "xixcore/bitmap.h"
#include "xixcore/lotinfo.h"

#include "xixfs_global.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"
#include "xixfs_name_cov.h"
#include "xixfs_event_op.h"
#include "xixfs_ndasctl.h"
#include <xixfsevent/xixfs_event.h>
#include "misc.h"

/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSEVTXHG"

//
//	Packet function
//
int
xixfs_AllocDGPkt(
		PXIXFSDG_PKT	*ppkt,
		uint8 			*SrcMac,
		uint8			*DstMac,
		uint32			Type
) {
	PXIXFS_COMM_HEADER	rawHead ;
	long					DGPktSz ;
	static uint8			Protocol[4] = XIXFS_DATAGRAM_PROTOCOL ;
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter xixfs_AllocDGPkt\n"));

	DGPktSz = sizeof(XIXFSDG_PKT);	// context + header
														// body

	*ppkt = kmalloc(DGPktSz, GFP_KERNEL);
														
	if(NULL == *ppkt) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("xixfs_AllocDGPkt: not enough memory.\n") );
		return -ENOMEM ;
	}

	memset(*ppkt, 0, DGPktSz) ;

	atomic_set(&((*ppkt)->RefCnt), 1);
	(*ppkt)->PacketSize = sizeof(XIXFS_COMM_HEADER) + sizeof(XIXFSDG_RAWPK_DATA) ;
	(*ppkt)->DataSize	= sizeof(XIXFSDG_RAWPK_DATA);

	INIT_LIST_HEAD(&(*ppkt)->PktListEntry) ;

	rawHead = &((*ppkt)->RawHeadDG) ;

	memcpy(rawHead->Protocol, Protocol, 4) ;
	
	if(DstMac)memcpy(rawHead->DstMac, DstMac, 32);
	if(SrcMac)memcpy(rawHead->SrcMac, SrcMac, 32);

	rawHead->Type = XI_HTONL(Type);
	rawHead->XifsMajorVersion = XI_HTONL(XIXFS_PROTO_MAJOR_VERSION);
	rawHead->XifsMinorVersion = XI_HTONL(XIXFS_PROTO_MINOR_VERSION)	;
	rawHead->MessageSize		= XI_HTONL(sizeof(XIXFS_COMM_HEADER) + sizeof(XIXFSDG_RAWPK_DATA)) ;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit xixfs_AllocDGPkt \n"));

	(*ppkt)->PkCtl.PktCompletion = NULL;
	(*ppkt)->PkCtl.status = NULL;
	//pDGCtrl = &((*ppkt)->PkCtl);
	//init_completion(&(pDGCtrl->PktCompletion));

	return 0 ;
}


int
xixfs_FreeDGPkt(PXIXFSDG_PKT pkt) {

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter xixfs_FreeDGPkt \n"));

	kfree(pkt) ;
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit xixfs_FreeDGPkt \n"));
	return 0 ;
}

void
xixfs_ReferenceDGPkt(PXIXFSDG_PKT pkt) {
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter xixfs_ReferenceDGPkt \n"));

	atomic_inc(&pkt->RefCnt);		

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit xixfs_ReferenceDGPkt \n"));
}


void
xixfs_DereferenceDGPkt(PXIXFSDG_PKT pkt) {


	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Enter xixfs_DereferenceDGPkt \n"));

	XIXCORE_ASSERT(atomic_read(&pkt->RefCnt) >  0);
	atomic_dec(&pkt->RefCnt);

	
	if(atomic_read(&pkt->RefCnt) == 0) {
		xixfs_FreeDGPkt(pkt) ;
	}
		
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM,
		("Exit xixfs_DereferenceDGPkt \n"));
}



int
xixfs_SendDGPkt(
	PXIXFS_EVENT_CTX	EventCtx,
	PXIXFSDG_PKT pkt	
)
{

	int RC = 0;

	/*
	printk(KERN_DEBUG "SEND PACKET PROTO (%c,%c,%c,%c), MV(%d), MINV(%d), TYPE(%d) PACKETSIZE(%d)\n",
		pkt->RawHeadDG.Protocol[0],
		pkt->RawHeadDG.Protocol[1],
		pkt->RawHeadDG.Protocol[2],
		pkt->RawHeadDG.Protocol[3],
		XI_NTOHL(pkt->RawHeadDG.XifsMajorVersion),
		XI_NTOHL(pkt->RawHeadDG.XifsMinorVersion),
		XI_NTOHL(pkt->RawHeadDG.Type),
		pkt->PacketSize
		);
	*/

	memcpy(EventCtx->buf, (void *)(&(pkt->RawHeadDG)), pkt->PacketSize);
	EventCtx->sz = pkt->PacketSize;
	RC = xixfs_ndas_sendEvent(EventCtx);

	if(RC<0) {
		printk(KERN_DEBUG "ERROR SEND EVENT (%d)\n", RC);
	}

	return RC;
	
}






void xixfs_event_handler_SrvErrorNetwork(void * ctx)
{
	PXIXFS_SERVICE_CTX 	SrvCtx = (PXIXFS_SERVICE_CTX)ctx;
	

	XIXCORE_ASSERT(SrvCtx);


	spin_lock(&(SrvCtx->Eventlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_global.EventCtx.Eventlock)) \n" ));
	
	XIXCORE_SET_FLAGS(SrvCtx->SrvEventFlags, XIXFS_EVENT_SRV_ERRNETWORK);
	spin_unlock(&(SrvCtx->Eventlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_global.EventCtx.Eventlock)) \n" ));
	
	wake_up(&SrvCtx->SrvEvent);
	
	
	return;
}

void xixfs_event_handler_CliErrorNetwork(void * ctx)
{
	PXIXFS_SERVICE_CTX 	SrvCtx = (PXIXFS_SERVICE_CTX)ctx;


	XIXCORE_ASSERT(SrvCtx);


	spin_lock(&(SrvCtx->Eventlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_global.EventCtx.Eventlock)) \n" ));

	XIXCORE_SET_FLAGS(SrvCtx->SrvEventFlags, XIXFS_EVENT_CLI_ERRNETWORK);
	spin_unlock(&(SrvCtx->Eventlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_global.EventCtx.Eventlock)) \n" ));

	wake_up(&SrvCtx->CliEvent);
	return;
}


void 
xixfs_Pkt_add_sendqueue(
		PXIXFS_SERVICE_CTX SrvCtx,
		PXIXFSDG_PKT		Pkt	
	)
{
	


	
	spin_lock(&SrvCtx->SendPktListLock);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_global.EventCtx.SendPktListLock)); \n" ));
	
	Pkt->TimeOut = xixcore_GetCurrentTime64() +  DEFAULT_REQUEST_MAX_TIMEOUT;
	list_add_tail(&(Pkt->PktListEntry), &(SrvCtx->SendPktList));
	spin_unlock(&SrvCtx->SendPktListLock);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_global.EventCtx.SendPktListLock)); \n" ));

	spin_lock(&(SrvCtx->Eventlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_global.EventCtx.Eventlock)) \n" ));
	
	XIXCORE_SET_FLAGS(SrvCtx->CliEventFlags, XIXFS_EVENT_CLI_SENDPKT);
	spin_unlock(&(SrvCtx->Eventlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_global.EventCtx.Eventlock)) \n" ));

	wake_up(&SrvCtx->CliEvent);
	return;
}


void
xixfs_event_handler_RcvPaket(void * ctx)
{

	PXIXFS_SERVICE_CTX SrvCtx = NULL;
	PXIXFS_EVENT_CTX	EventCtx = NULL;
	PXIXFSDG_PKT		Pkt = NULL;
	int					RC = 0;
	static uint8			Protocol[4] = XIXFS_DATAGRAM_PROTOCOL;


	EventCtx = (PXIXFS_EVENT_CTX)ctx;
	XIXCORE_ASSERT(EventCtx);


	SrvCtx = (PXIXFS_SERVICE_CTX)EventCtx->Context;
	XIXCORE_ASSERT(SrvCtx);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( KERN_DEBUG "Enter xixfs_event_handler_RcvPaket\n" ) );


	if(EventCtx->sz < sizeof(XIXFS_COMM_HEADER)) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			( KERN_DEBUG "fail: xixfs_event_handler_RcvPaket --> too small data %d\n", EventCtx->sz ) );
		return;
	}

	RC = xixfs_AllocDGPkt(&Pkt, 
						NULL, 
						NULL, 
						XIXFS_TYPE_UNKOWN
						);

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			( KERN_DEBUG "fail: xixfs_event_handler_RcvPaket --> PK alloc \n" ) );	
		return;
	}

	memcpy(&Pkt->RawHeadDG, EventCtx->buf, sizeof(Pkt->RawHeadDG) );
	
	if(	memcmp(Pkt->RawHeadDG.Protocol, Protocol, 4) != 0 ||
		XI_NTOHL(Pkt->RawHeadDG.XifsMajorVersion) != XIXFS_PROTO_MAJOR_VERSION ||
		XI_NTOHL(Pkt->RawHeadDG.XifsMinorVersion) != XIXFS_PROTO_MINOR_VERSION ||
		!(XI_NTOHL(Pkt->RawHeadDG.Type) &  XIXFS_TYPE_MASK)  
	) {


		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("xixfs_event_handler_RcvPaket: Invalid  header. PT(%c,%c,%c,%c) MaxV(%d) MinV(%d) Type(%d) \n",
				Pkt->RawHeadDG.Protocol[0],
				Pkt->RawHeadDG.Protocol[1],
				Pkt->RawHeadDG.Protocol[2],
				Pkt->RawHeadDG.Protocol[3],
				XI_NTOHL(Pkt->RawHeadDG.XifsMajorVersion),
				XI_NTOHL(Pkt->RawHeadDG.XifsMinorVersion),
				XI_NTOHL(Pkt->RawHeadDG.Type)
				));

		xixfs_DereferenceDGPkt(Pkt);
		
		goto not_accepted;
	}


		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("xixfs_event_handler_RcvPaket: VALID  header. PT(%c,%c,%c,%c) MaxV(%d) MinV(%d) Type(%d) \n",
				Pkt->RawHeadDG.Protocol[0],
				Pkt->RawHeadDG.Protocol[1],
				Pkt->RawHeadDG.Protocol[2],
				Pkt->RawHeadDG.Protocol[3],
				XI_NTOHL(Pkt->RawHeadDG.XifsMajorVersion),
				XI_NTOHL(Pkt->RawHeadDG.XifsMinorVersion),
				XI_NTOHL(Pkt->RawHeadDG.Type)
				));


	

	Pkt->PacketSize	= XI_NTOHL(Pkt->RawHeadDG.MessageSize);
	Pkt->DataSize	=  XI_NTOHL(Pkt->RawHeadDG.MessageSize) - sizeof(XIXFS_COMM_HEADER);		

	if( Pkt->PacketSize != sizeof(XIXFS_COMM_HEADER) + Pkt->DataSize ) {

		xixfs_DereferenceDGPkt(Pkt);
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("xixfs_event_handler_RcvPaket: Invalid  header size is not correct.\n"));
		
		goto not_accepted;
	}
	
	//
	//	read the data
	//
	memcpy(&Pkt->RawDataDG, (uint8 *)EventCtx->buf + sizeof(Pkt->RawHeadDG), Pkt->DataSize);
	memcpy(&Pkt->SourceAddr, EventCtx->dest, sizeof(struct sockaddr_lpx));


	INIT_LIST_HEAD(&Pkt->PktListEntry);

	spin_lock(&(SrvCtx->RecvPktListLock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_global.EventCtx.RecvPktListLock)); \n" ));
	
	list_add_tail(&Pkt->PktListEntry,&(SrvCtx->RecvPktList));
	spin_unlock(&(SrvCtx->RecvPktListLock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_global.EventCtx.RecvPktListLock)); \n" ));

	
	spin_lock(&(SrvCtx->Eventlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_global.EventCtx.Eventlock)) \n" ));
	
	XIXCORE_SET_FLAGS(SrvCtx->SrvEventFlags, XIXFS_EVENT_SRV_RECVPKT);
	spin_unlock(&(SrvCtx->Eventlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_global.EventCtx.Eventlock)) \n" ));
	wake_up(&SrvCtx->SrvEvent);
	


not_accepted:
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( KERN_DEBUG "Exit xixfs_event_handler_RcvPaket\n" ) );

	return;
}





int
xixfs_dispatchPkt(
	PXIXFS_SERVICE_CTX	serviceCtx,
	PXIXFSDG_PKT		Pkt
)
{
	int									RC = 0;
	PXIXFS_COMM_HEADER					pHeader = NULL;
	PXIXFSDG_RAWPK_DATA				pData = NULL;

	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Enter DispatchDGReqPkt\n" ) );

	pHeader = &(Pkt->RawHeadDG);
	pData = &(Pkt->RawDataDG);
	
	switch(XI_NTOHL(pHeader->Type)){
	case XIXFS_TYPE_LOCK_REQUEST:
	{
		
		
		if(memcmp(xixcore_global.HostMac, pData->LockReq.LotOwnerMac, 32) == 0){

#if LINUX_VERSION_WORK_QUEUE
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
			INIT_WORK(&(Pkt->Pktwork), xixfs_ProcessLockRequest);
#else
			INIT_WORK(&(Pkt->Pktwork), xixfs_ProcessLockRequest, Pkt);
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
			queue_work(xixfs_workqueue, &(Pkt->Pktwork));

#else
			memset(&(Pkt->Pktwork), 0, sizeof(struct tq_struct));
			Pkt->Pktwork.routine = xixfs_ProcessLockRequest;
			Pkt->Pktwork.data = Pkt;
			schedule_task(&(Pkt->Pktwork));
#endif
			return 0;
		}
		return 1;


	}
	break;
	case XIXFS_TYPE_LOCK_REPLY:
	{


		if(memcmp(xixcore_global.HostMac, pHeader->DstMac, 32) == 0){
			RC= xixfs_CheckLockRequest(serviceCtx, Pkt);
			if(RC == 0) return 0;
			
			return 1;
		}
		return 1;

	}
	break;
	case XIXFS_TYPE_LOCK_BROADCAST:
	{
#if LINUX_VERSION_WORK_QUEUE
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
		INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessLockBroadCast);
#else
		INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessLockBroadCast,Pkt);
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE

		queue_work(xixfs_workqueue, &(Pkt->Pktwork));	
#else
		memset(&(Pkt->Pktwork), 0, sizeof(struct tq_struct));
		Pkt->Pktwork.routine = xixfs_ProcessLockBroadCast;
		Pkt->Pktwork.data = Pkt;
		schedule_task(&(Pkt->Pktwork));
#endif
		return 0;
	}
	break;
	case XIXFS_TYPE_FLUSH_BROADCAST:
	{

#if LINUX_VERSION_WORK_QUEUE
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
		INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessFlushBroadCast);
#else
		INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessFlushBroadCast,Pkt);
#endif //#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
		queue_work(xixfs_workqueue, &(Pkt->Pktwork));	
#else
		memset(&(Pkt->Pktwork), 0, sizeof(struct tq_struct));
		Pkt->Pktwork.routine = xixfs_ProcessFlushBroadCast;
		Pkt->Pktwork.data = Pkt;
		schedule_task(&(Pkt->Pktwork));
#endif
		return 0;
	}
	break;
	case XIXFS_TYPE_FILE_LENGTH_CHANGE:
	{
#if LINUX_VERSION_WORK_QUEUE
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
		INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessFileLenChange);
#else
		INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessFileLenChange,Pkt);
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
		queue_work(xixfs_workqueue, &(Pkt->Pktwork));	
#else
		memset(&(Pkt->Pktwork), 0, sizeof(struct tq_struct));
		Pkt->Pktwork.routine = xixfs_ProcessFileLenChange;
		Pkt->Pktwork.data = Pkt;
		schedule_task(&(Pkt->Pktwork));
#endif
		return 0;

	}
	break;
	case XIXFS_TYPE_FILE_CHANGE:
	{
#if LINUX_VERSION_WORK_QUEUE	
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
		INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessFileChange);
#else
		INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessFileChange,Pkt);
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
		queue_work(xixfs_workqueue, &(Pkt->Pktwork));	
#else
		memset(&(Pkt->Pktwork), 0, sizeof(struct tq_struct));
		Pkt->Pktwork.routine = xixfs_ProcessFileChange;
		Pkt->Pktwork.data = Pkt;
		schedule_task(&(Pkt->Pktwork));
#endif
		return 0;
	}
	break;
	case XIXFS_TYPE_DIR_CHANGE:
	{
#if LINUX_VERSION_WORK_QUEUE
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
		INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessDirChange);
#else
		INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessDirChange,Pkt);
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
		queue_work(xixfs_workqueue, &(Pkt->Pktwork));	
#else
		memset(&(Pkt->Pktwork), 0, sizeof(struct tq_struct));
		Pkt->Pktwork.routine = xixfs_ProcessDirChange;
		Pkt->Pktwork.data = Pkt;
		schedule_task(&(Pkt->Pktwork));
#endif
		return 0;
		
	}
	break;
	default:
		return 1;
		break;
	}

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_dispatchPkt\n" ) );
}

int
xixfs_event_server(void * ctx)
{
	PXIXFS_SERVICE_CTX	serviceCtx = (PXIXFS_SERVICE_CTX)ctx;
	int 					RC = 0;
	
	//int					TimeOut =0;
	PXIXFSDG_PKT		Pkt = NULL;
	unsigned long flags;
	XIXCORE_ASSERT(serviceCtx);

	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( KERN_DEBUG "Enter xixfs_event_server\n" ) );

#if defined(NDAS_ORG2423) || defined(NDAS_SIGPENDING_OLD)
	spin_lock_irqsave(&current->sigmask_lock, flags);
	siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGTERM));
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);
#else
	spin_lock_irqsave(&current->sighand->siglock, flags);
    	siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGTERM));
    	recalc_sigpending();
    	spin_unlock_irqrestore(&current->sighand->siglock, flags);
#endif

#if LINUX_VERSION_25_ABOVE
	daemonize("xixfs event server");
#else
	daemonize();
#endif
	// check context
	if(serviceCtx->SrvCtx.IsSrvType != 1) {
		serviceCtx->SrvCtx.IsSrvType = 1;
	}


	if(serviceCtx->SrvCtx.packet_handler == NULL) {
		serviceCtx->SrvCtx.packet_handler = xixfs_event_handler_RcvPaket;
	}


	if(serviceCtx->SrvCtx.error_handler == NULL) {
		serviceCtx->SrvCtx.error_handler = xixfs_event_handler_SrvErrorNetwork;
	}

	if(serviceCtx->SrvCtx.Context == NULL) {
		serviceCtx->SrvCtx.Context = (void *)serviceCtx;
	}

	// create event
	RC = xixfs_ndas_createEvent(&(serviceCtx->SrvCtx));
	
	if( RC  < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixfs_event_server->create event server\n" ) );
		return -1;
	}


	complete(&(serviceCtx->SrvCompletion));




	while(1){

		 if(signal_pending(current)) {
        		flush_signals(current);
    		 }
		
		//TimeOut = DEFAULT_XIXFS_CHECK_REQ;
		//RC = wait_event_timeout(serviceCtx->SrvEvent, 
		//					XIXCORE_TEST_FLAGS(serviceCtx->SrvEventFlags , XIXFS_EVENT_SRV_MASK),
		//					TimeOut
		//					);


		wait_event(serviceCtx->SrvEvent, XIXCORE_TEST_FLAGS(serviceCtx->SrvEventFlags , XIXFS_EVENT_SRV_MASK));

		spin_lock(&(serviceCtx->Eventlock));
		//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		//			("spin_lock(&(xixfs_global.EventCtx.Eventlock)) \n" ));
		
		if(XIXCORE_TEST_FLAGS(serviceCtx->SrvEventFlags,  XIXFS_EVENT_SRV_SHUTDOWN)){
			spin_unlock(&(serviceCtx->Eventlock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(xixfs_global.EventCtx.Eventlock)) \n" ));
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("xixfs_event_server requested stop\n" ) );
			
			xixfs_ndas_closeEvent(&(serviceCtx->SrvCtx));
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("xixfs_event_server STOP!! event mechanism\n" ) );
			
			break;
		}else if(XIXCORE_TEST_FLAGS(serviceCtx->SrvEventFlags, XIXFS_EVENT_SRV_ERRNETWORK)){
			int try_count = 0;
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("xixfs_event_server requested stop\n" ) );

			XIXCORE_CLEAR_FLAGS(serviceCtx->SrvEventFlags, XIXFS_EVENT_SRV_ERRNETWORK);
			spin_unlock(&(serviceCtx->Eventlock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(xixfs_global.EventCtx.Eventlock)) \n" ));

retry:				
			if(serviceCtx->SrvCtx.IsSrvType != 1) {
				serviceCtx->SrvCtx.IsSrvType = 1;					
			}

			if(serviceCtx->SrvCtx.packet_handler == NULL) {
				serviceCtx->SrvCtx.packet_handler = xixfs_event_handler_RcvPaket;
			}

			if(serviceCtx->SrvCtx.error_handler == NULL) {
				serviceCtx->SrvCtx.error_handler = xixfs_event_handler_SrvErrorNetwork;
			}

			if(serviceCtx->SrvCtx.Context == NULL) {
				serviceCtx->SrvCtx.Context = (void *)serviceCtx;
			}

			
			// create event
			RC = xixfs_ndas_createEvent(&(serviceCtx->SrvCtx));

			if( RC  < 0 ) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail xixfs_event_server->create event server Try_count(%d) \n", try_count ) );

				if(try_count > 3) {
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail xixfs_event_server ERROR!!! OUT!!!! \n") );
					break;
				}

				goto retry;
			}
				
		}else {
			int					result = 0;

			if(XIXCORE_TEST_FLAGS(serviceCtx->SrvEventFlags, XIXFS_EVENT_SRV_RECVPKT)){
				XIXCORE_CLEAR_FLAGS(serviceCtx->SrvEventFlags, XIXFS_EVENT_SRV_RECVPKT);
			}
			spin_unlock(&(serviceCtx->Eventlock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(xixfs_global.EventCtx.Eventlock)) \n" ));

			spin_lock(&(serviceCtx->RecvPktListLock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_lock(&(xixfs_global.EventCtx.RecvPktListLock)); \n" ));
			
			while(!list_empty(&(serviceCtx->RecvPktList)) ) {
				Pkt = container_of(serviceCtx->RecvPktList.next, XIXFSDG_PKT, PktListEntry);
				list_del(&(Pkt->PktListEntry));

				if(xixfs_IsLocalAddress(Pkt->SourceAddr.slpx_node)  == 1) {
					xixfs_DereferenceDGPkt(Pkt);
					continue;
				}
				
				DebugTrace( DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
					( KERN_DEBUG "xixfs_event_server: Datagram packet came from the other address "
						"%02X:%02X:%02X:%02X:%02X:%02X/%d\n", 
					Pkt->SourceAddr.slpx_node[0], Pkt->SourceAddr.slpx_node[1], Pkt->SourceAddr.slpx_node[2],
					Pkt->SourceAddr.slpx_node[3], Pkt->SourceAddr.slpx_node[4], Pkt->SourceAddr.slpx_node[5],
					Pkt->SourceAddr.slpx_port
					));

				result = xixfs_dispatchPkt(serviceCtx, Pkt);

				if(result == 1) {
					xixfs_DereferenceDGPkt(Pkt);
				}
			}			
			spin_unlock(&(serviceCtx->RecvPktListLock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(xixfs_global.EventCtx.RecvPktListLock)); \n" ));
			
			continue;

			
		}

	}


	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("xixfs_event_server CLEANUP!! recieved packet\n" ) );

	spin_lock(&(serviceCtx->RecvPktListLock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_global.EventCtx.RecvPktListLock)); \n" ));
	while(!list_empty(&(serviceCtx->RecvPktList)) ) {
		Pkt = container_of(serviceCtx->RecvPktList.next, XIXFSDG_PKT, PktListEntry);
		list_del(&(Pkt->PktListEntry));
		xixfs_DereferenceDGPkt(Pkt);
	}			
	spin_unlock(&(serviceCtx->RecvPktListLock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_global.EventCtx.RecvPktListLock)); \n" ));

	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("xixfs_event_server COMPLETE!! recieved packet\n" ) );
	
	complete(&(serviceCtx->SrvCompletion));
	
	return 0;
}


int
xixfs_event_client(void *ctx)
{
	PXIXFS_SERVICE_CTX	serviceCtx = (PXIXFS_SERVICE_CTX)ctx;
	int RC = 0;
#if LINUX_VERSION_25_ABOVE
	int					TimeOut =0;
#endif
	PXIXFSDG_PKT		Pkt = NULL;
	unsigned long flags;

	
	XIXCORE_ASSERT(serviceCtx);


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ( KERN_DEBUG "Enter xixfs_event_client\n" ) );

#if defined(NDAS_ORG2423) || defined(NDAS_SIGPENDING_OLD)
	spin_lock_irqsave(&current->sigmask_lock, flags);
	siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGTERM));
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);
#else
	spin_lock_irqsave(&current->sighand->siglock, flags);
    	siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGTERM));
    	recalc_sigpending();
    	spin_unlock_irqrestore(&current->sighand->siglock, flags);
#endif

#if LINUX_VERSION_25_ABOVE
	daemonize("xixfs event cli");
#else
	daemonize();
#endif


	// check context
	if(serviceCtx->CliCtx.IsSrvType == 1) {
		serviceCtx->CliCtx.IsSrvType = 0;
	}


	if(serviceCtx->CliCtx.error_handler == NULL) {
		serviceCtx->CliCtx.error_handler = xixfs_event_handler_CliErrorNetwork;
	}

	if(serviceCtx->CliCtx.Context == NULL) {
		serviceCtx->CliCtx.Context = (void *)serviceCtx;
	}

	// create event
	RC = xixfs_ndas_createEvent(&(serviceCtx->CliCtx));
	
	if( RC  < 0 ) {
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixfs_event_client->create event server\n" ) );
		return -1;
	}

	complete(&(serviceCtx->CliCompletion));

	while(1){


		 if(signal_pending(current)) {
        		flush_signals(current);
    		 }

#if LINUX_VERSION_25_ABOVE		
		TimeOut = DEFAULT_XIXFS_CHCKE_EVENT;
		RC = wait_event_timeout( serviceCtx->CliEvent, 
								XIXCORE_TEST_FLAGS(serviceCtx->CliEventFlags, XIXFS_EVENT_CLI_MASK), 
								TimeOut
								);
#else
		mod_timer(&(serviceCtx->CliEventTimeOut), jiffies + HZ);
		wait_event(serviceCtx->CliEvent, 
								XIXCORE_TEST_FLAGS(serviceCtx->CliEventFlags, XIXFS_EVENT_CLI_MASK));
#endif
		

		//printk(KERN_DEBUG "Call Cli Event\n");

		spin_lock(&(serviceCtx->Eventlock));	
		//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		//			("spin_lock(&(xixfs_global.EventCtx.Eventlock)) \n" ));
		
		
		if(XIXCORE_TEST_FLAGS(serviceCtx->CliEventFlags, XIXFS_EVENT_CLI_SHUTDOWN)){


			spin_unlock(&(serviceCtx->Eventlock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(xixfs_global.EventCtx.Eventlock)) \n" ));
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("xixfs_event_client requested stop\n" ) );

#if !LINUX_VERSION_25_ABOVE
			del_timer(&(serviceCtx->CliEventTimeOut));
#endif
			xixfs_ndas_closeEvent(&(serviceCtx->CliCtx));


			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("xixfs_event_client STOPED!!!!\n" ) );
			
			
			break;

			
		} else if(XIXCORE_TEST_FLAGS(serviceCtx->CliEventFlags, XIXFS_EVENT_CLI_ERRNETWORK)){
			
			
			int try_count = 0;
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("xixfs_event_client requested stop\n" ) );
			XIXCORE_CLEAR_FLAGS(serviceCtx->CliEventFlags, XIXFS_EVENT_CLI_ERRNETWORK);
			spin_unlock(&(serviceCtx->Eventlock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(xixfs_global.EventCtx.Eventlock)) \n" ));
retry:
			// check context
			if(serviceCtx->CliCtx.IsSrvType == 1) {
				serviceCtx->CliCtx.IsSrvType = 0;
			}


			if(serviceCtx->CliCtx.error_handler == NULL) {
				serviceCtx->CliCtx.error_handler = xixfs_event_handler_CliErrorNetwork;
			}

			if(serviceCtx->CliCtx.Context == NULL) {
				serviceCtx->CliCtx.Context = (void *)serviceCtx;
			}


			// create event
			RC = xixfs_ndas_createEvent(&(serviceCtx->CliCtx));

			if( RC  < 0 ) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail xixfs_event_client->create event server Try_count(%d) \n", try_count ) );

				if(try_count > 3) {
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail xixfs_event_client ERROR!!! OUT!!!! \n") );
					break;
				}

				goto retry;
			}
			
		} else {

			int					IsRmoved = 0;
			struct list_head		tmpListHead;
#if !LINUX_VERSION_25_ABOVE
			if(XIXCORE_TEST_FLAGS(serviceCtx->CliEventFlags, XIXFS_EVENT_CLI_TIMEOUT)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("xixfs_event_client requested timeout\n" ) );
				//printk(KERN_DEBUG "Cli EVENT Timeout event\n");
				XIXCORE_CLEAR_FLAGS(serviceCtx->CliEventFlags, XIXFS_EVENT_CLI_TIMEOUT);
			}
#endif
			if(XIXCORE_TEST_FLAGS(serviceCtx->CliEventFlags, XIXFS_EVENT_CLI_SENDPKT)){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("xixfs_event_client requested send packet\n" ) );
				printk(KERN_DEBUG "Cli EVENT Send Packet event\n");	
		
				XIXCORE_CLEAR_FLAGS(serviceCtx->CliEventFlags, XIXFS_EVENT_CLI_SENDPKT);
			}
	
			spin_unlock(&(serviceCtx->Eventlock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(xixfs_global.EventCtx.Eventlock)) \n" ));

			INIT_LIST_HEAD(&tmpListHead);
			

			spin_lock(&serviceCtx->SendPktListLock);
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_lock(&(xixfs_global.EventCtx.SendPktListLock)); \n" ));
			
			while(!list_empty(&(serviceCtx->SendPktList)) ) {
				Pkt = container_of(serviceCtx->SendPktList.next, XIXFSDG_PKT, PktListEntry);
				list_del(&(Pkt->PktListEntry));

				if(XI_NTOHL(Pkt->RawHeadDG.Type) == XIXFS_TYPE_LOCK_REQUEST){
					if(Pkt->TimeOut < xixcore_GetCurrentTime64()){
						
						DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_HOSTCOM, 
							( KERN_DEBUG "xixfs_event_client LOCK TIMEOUT Req(%lld) Check(%lld)\n", 
									Pkt->TimeOut, 
									xixcore_GetCurrentTime64() ));	

						printk(KERN_DEBUG "Cli EVENT Send Packet event Timeout\n");	
					
						
						INIT_LIST_HEAD(&Pkt->PktListEntry);
						if(Pkt->PkCtl.status)
							*(Pkt->PkCtl.status) = LOCK_TIMEOUT;
						
						if(Pkt->PkCtl.PktCompletion)
							complete(Pkt->PkCtl.PktCompletion);
						
						xixfs_DereferenceDGPkt(Pkt);
						continue;
					}
					
					DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_HOSTCOM, 
							( KERN_DEBUG "xixfs_event_client LOCK  Retry Req(%lld) Check(%lld)\n", 
									Pkt->TimeOut, 
									xixcore_GetCurrentTime64() ));	
					
					IsRmoved = 0;
				}else{
					IsRmoved = 1;
				}

				printk(KERN_DEBUG "START Cli EVENT Send Packet event call xixfs_SendDGPkt\n");	
				RC = xixfs_SendDGPkt(&(serviceCtx->CliCtx), Pkt);

				printk(KERN_DEBUG "END Cli EVENT Send Packet event call xixfs_SendDGPkt\n");	
				if(RC < 0) {
					printk(KERN_DEBUG "ERROR Cli EVENT Send Packet event call xixfs_SendDGPkt\n");	
				}
				
				if(IsRmoved == 1) {
					
					xixfs_DereferenceDGPkt(Pkt);
				}else {
					list_add_tail(&(Pkt->PktListEntry),&tmpListHead );
				}
			
			}			


			while(!list_empty(&tmpListHead) ) {
				Pkt = container_of(tmpListHead.next, XIXFSDG_PKT, PktListEntry);
				list_del(tmpListHead.next);
				list_add_tail(&(Pkt->PktListEntry),&(serviceCtx->SendPktList));
			}			
			
			spin_unlock(&serviceCtx->SendPktListLock);
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(xixfs_global.EventCtx.SendPktListLock)); \n" ));
			continue;	
		}

	}

	
	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("xixfs_event_client CLEANUP SendPaket queue!!!!\n" ) );
	
	spin_lock(&serviceCtx->SendPktListLock);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_global.EventCtx.SendPktListLock)); \n" ));
	
	while(!list_empty(&(serviceCtx->SendPktList)) ) {
		Pkt = container_of(serviceCtx->SendPktList.next, XIXFSDG_PKT, PktListEntry);
		list_del(&(Pkt->PktListEntry));
		if(XI_NTOHL(Pkt->RawHeadDG.Type) == XIXFS_TYPE_LOCK_REQUEST){

			if(Pkt->PkCtl.status)
				*(Pkt->PkCtl.status) = LOCK_TIMEOUT;
			
			if(Pkt->PkCtl.PktCompletion)
				complete(Pkt->PkCtl.PktCompletion);
		}
		xixfs_DereferenceDGPkt(Pkt);
	}
	spin_unlock(&serviceCtx->SendPktListLock);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_global.EventCtx.SendPktListLock)); \n" ));
	
	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("xixfs_event_client STOP completion!!!!\n" ) );
	
	
	complete(&(serviceCtx->CliCompletion));
	return 0;
}



