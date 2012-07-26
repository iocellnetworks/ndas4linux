/*
 -------------------------------------------------------------------------
 Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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
#include <linux/spinlock.h>
#include <linux/nls.h>
#include <asm/uaccess.h>


#include "xcsystem/debug.h"
#include "xixcore/layouts.h"
#include "xcsystem/system.h"
#include "xixcore/ondisk.h"
#include "xixcore/volume.h"
#include "xixcore/hostreg.h" 
#include "xixcore/bitmap.h"
#include "xixcore/lotlock.h"

#include "xixfs_global.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"
#include "xixfs_name_cov.h"
#include "xixfs_ndasctl.h"
#include "xixfs_event_op.h"
#include "inode.h"


/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSMISC"

int
xixfs_IHaveLotLock(
	uint8	* HostMac,
	uint64	 LotNumber,
	uint8	* VolumeId
)
{
	int						RC = 0;
	PXIXFSDG_PKT			pPacket = NULL;
	PXIXFS_LOCK_BROADCAST	pPacketData = NULL;
	uint8					DstAddress[6] ={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixfs_IHaveLotLock \n"));
	
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	if(FALSE ==LfsAllocDGPkt(&pPacket, HostMac, DstAddress, XIFS_TYPE_LOCK_BROADCAST))
	{
		return FALSE;
	}
	*/
	RC = xixfs_AllocDGPkt(
					&pPacket, 
					HostMac, 
					DstAddress, 
					XIXFS_TYPE_LOCK_BROADCAST
					);
	if(RC < 0  )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error (0x%x) xixfs_IHaveLotLock:xixfs_AllocDGPkt  \n",
				RC));
		
		return RC;
	}

	pPacketData = &(pPacket->RawDataDG.LockBroadcast);

	// Changed by ILGU HONG

	memcpy(pPacketData->VolumeId, VolumeId, 16);
	
	pPacketData->LotNumber = XI_HTONLL(LotNumber);
	pPacket->TimeOut = xixcore_GetCurrentTime64()+ DEFAULT_REQUEST_MAX_TIMEOUT;

	xixfs_Pkt_add_sendqueue(
			&xixfs_linux_global.EventCtx,
			pPacket 
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit xixfs_IHaveLotLock\n"));
	return 0;
}



int
xixfs_SendRenameLinkBC(
	uint32		SubCommand,
	uint8		* HostMac,
	uint64		LotNumber,
	uint8		* VolumeId,
	uint64		OldParentLotNumber,
	uint64		NewParentLotNumber
)
{
	int								RC = 0;
	PXIXFSDG_PKT					pPacket = NULL;
	PXIXFS_FILE_CHANGE_BROADCAST	pPacketData = NULL;
	uint8							DstAddress[6] ={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter XifsdSendRenameLinkBC \n"));
	
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	if(FALSE ==LfsAllocDGPkt(&pPacket, HostMac, DstAddress, XIFS_TYPE_FILE_CHANGE))
	{
		return FALSE;
	}
	*/
	RC = xixfs_AllocDGPkt(
				&pPacket, 
				HostMac, 
				DstAddress, 
				XIXFS_TYPE_FILE_CHANGE
				);
	
	if(RC < 0  )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error (0x%x) xixfs_SendRenameLinkBC:xixfs_AllocDGPkt  \n",
				RC));
		
		return RC;
	}

	pPacketData = &(pPacket->RawDataDG.FileChangeReq);
	// Changed by ILGU HONG

	memcpy(pPacketData->VolumeId, VolumeId, 16);
	pPacketData->LotNumber = XI_HTONLL(LotNumber);
	pPacketData->PrevParentLotNumber = XI_HTONLL(OldParentLotNumber);
	pPacketData->NewParentLotNumber = XI_HTONLL(NewParentLotNumber);
	pPacketData->SubCommand = XI_HTONL(SubCommand);

	pPacket->TimeOut = xixcore_GetCurrentTime64()+ DEFAULT_REQUEST_MAX_TIMEOUT;




	xixfs_Pkt_add_sendqueue(
			&xixfs_linux_global.EventCtx,
			pPacket 
			);
	

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit xixfs_SendRenameLinkBC \n"));

	return 0;	
}


int
xixfs_SendFileChangeRC(
	uint8		* HostMac,
	uint64		LotNumber,
	uint8		* VolumeId,
	uint64		FileLength,
	uint64		AllocationLength,
	uint64		UpdateStartOffset
)
{
	int										RC = 0;
	PXIXFSDG_PKT							pPacket = NULL;
	PXIXFS_FILE_LENGTH_CHANGE_BROADCAST	pPacketData = NULL;
	uint8									DstAddress[6] ={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixfs_SendFileChangeRC \n"));

	
	// Changed by ILGU HONG
	//	chesung suggest
	/*
	if(FALSE ==LfsAllocDGPkt(&pPacket, HostMac, DstAddress, XIFS_TYPE_FILE_LENGTH_CHANGE))
	{
		return FALSE;
	}
	*/

	RC = xixfs_AllocDGPkt(
				&pPacket, 
				HostMac, 
				DstAddress, 
				XIXFS_TYPE_FILE_LENGTH_CHANGE
				);

	
	if(RC < 0  )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error (0x%x) xixfs_SendFileChangeRC:xixfs_AllocDGPkt  \n",
				RC));
		
		return RC;
	}



	pPacketData = &(pPacket->RawDataDG.FileLenChangeReq);
	// Changed by ILGU HONG
	memcpy(pPacketData->VolumeId, VolumeId, 16);
	pPacketData->LotNumber = XI_HTONLL(LotNumber);
	pPacketData->FileLength = XI_HTONLL(FileLength);
	pPacketData->AllocationLength = XI_HTONLL(AllocationLength);
	pPacketData->WriteStartOffset = XI_HTONLL(UpdateStartOffset);
	pPacket->TimeOut = xixcore_GetCurrentTime64()+ DEFAULT_REQUEST_MAX_TIMEOUT;




	xixfs_Pkt_add_sendqueue(
			&xixfs_linux_global.EventCtx,
			pPacket 
			);
	

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit xixfs_SendFileChangeRC \n"));

	return 0;	
}


int
xixfs_CheckLockRequest(
	PXIXFS_SERVICE_CTX	serviceCtx,
	PXIXFSDG_PKT		Pkt
)
{

	PXIXFSDG_PKT			pReq =NULL;
	PXIXFS_LOCK_REQUEST 	pReqDataHeader = NULL;
	PXIXFS_LOCK_REPLY		pReplyDataHeader = NULL;
	struct list_head			*pListEntry = NULL;
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( KERN_DEBUG "Enter CheckLockRequest\n" ) );

	pReplyDataHeader = &(Pkt->RawDataDG.LockReply);
	
	spin_lock(&serviceCtx->SendPktListLock);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.EventCtx.SendPktListLock)); \n" ));
	
	pListEntry = serviceCtx->SendPktList.next;
	printk(KERN_DEBUG "check lock request !!\n");
	while ( pListEntry != &(serviceCtx->SendPktList) ) {
		pReq = container_of(pListEntry, XIXFSDG_PKT, PktListEntry);

		if (XI_NTOHL(pReq->RawHeadDG.Type) == XIXFS_TYPE_LOCK_REQUEST) {

			pReqDataHeader = &(pReq->RawDataDG.LockReq);

			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
				("Req #Packet(%d)  LotNumber(%lld) : Rep #Packet(%d)  LotNumber(%lld)\n",
					XI_NTOHL(pReqDataHeader->PacketNumber), XI_NTOHLL(pReqDataHeader->LotNumber),
					XI_NTOHL(pReplyDataHeader->PacketNumber),XI_NTOHLL(pReplyDataHeader->LotNumber) )); 
			


			if((XI_NTOHL(pReqDataHeader->PacketNumber) == XI_NTOHL(pReplyDataHeader->PacketNumber))
				&& (memcmp(pReqDataHeader->VolumeId, pReplyDataHeader->VolumeId, 16) == 0)
				&& ( XI_NTOHLL(pReqDataHeader->LotNumber) == XI_NTOHLL(pReplyDataHeader->LotNumber)) )
			{	

				DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
					("Search Req and Set Event\n"));

				list_del(&pReq->PktListEntry);
				INIT_LIST_HEAD(&pReq->PktListEntry);

				if(pReq->PkCtl.status)
					*(pReq->PkCtl.status) = XI_NTOHL(pReplyDataHeader->LotState);

				printk(KERN_DEBUG "!!! Receive Lock Reply 4 LotNumber(%lld) status(%x)!!\n", 
							XI_NTOHLL(pReqDataHeader->LotNumber), *(pReq->PkCtl.status));


				if(pReq->PkCtl.PktCompletion){
					printk(KERN_DEBUG "wake up lock !!\n");
#if LINUX_VERSION_25_ABOVE						
					complete_all(pReq->PkCtl.PktCompletion);
#else
					
					complete(pReq->PkCtl.PktCompletion);
#endif
				}	
				

				xixfs_DereferenceDGPkt(pReq);
				spin_unlock(&serviceCtx->SendPktListLock);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.EventCtx.SendPktListLock)); \n" ));
				
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
					( KERN_DEBUG "Exit xixfs_CheckLockRequest Event SET!!\n" ) );
				return 1;
			}


		}
		pListEntry = pListEntry->next;

	}
		
		
	spin_unlock(&serviceCtx->SendPktListLock);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.EventCtx.SendPktListLock)); \n" ));
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_CheckLockRequest\n" ) );
	return 1;
}


int
xixfs_IsFileLocked(
	PXIXFS_LINUX_VCB		pVCB,
	uint64			LotNumber,
	uint32			*LockState
	)
{	
	struct super_block * sb = NULL;
	struct inode		*inode = NULL;
	PXIXFS_LINUX_FCB		pFCB = NULL;
	int				RC = 0;
	

	XIXFS_ASSERT_VCB(pVCB);
	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( "Enter xixfs_IsFileLocked\n" ) );
	
	
	if(LotNumber == pVCB->XixcoreVcb.MetaContext.HostRegLotMapIndex){
		if(pVCB->XixcoreVcb.MetaContext.HostRegLotMapLockStatus == INODE_FILE_LOCK_HAS){
			*LockState = INODE_FILE_LOCK_HAS;
		}else{
			*LockState = INODE_FILE_LOCK_INVALID;
		}
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_HOSTCOM|DEBUG_TARGET_FCB), 
			( "LockState (%x)\n", *LockState ) );

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_HOSTCOM|DEBUG_TARGET_FCB), 
			( "VCB lock Request !!! Exit IsFileLocked\n"));

		return 0;
	}


	sb = pVCB->supreblockCtx;
	XIXCORE_ASSERT(sb);

	inode = xixfs_iget(sb,LotNumber);
	

	
	if(inode){
		

		pFCB = XIXFS_I(inode);
		XIXFS_ASSERT_FCB(pFCB);
		
		if(pFCB->XixcoreFcb.HasLock == INODE_FILE_LOCK_HAS){
			*LockState = INODE_FILE_LOCK_HAS;
		}else {
			*LockState = INODE_FILE_LOCK_INVALID;
		}
		iput(inode);
		RC = 0;
	}else{
	
		*LockState = INODE_FILE_LOCK_INVALID;
		RC =  0;
	}
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_HOSTCOM|DEBUG_TARGET_FCB), ( "LockState (%x)\n", *LockState ) );

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, ( "Exit IsFileLocked Status (0x%x)\n", RC ) );
	return RC;	
}



int
xixfs_SendLockReply(
	PXIXFS_LINUX_VCB 			pVCB,
	PXIXFSDG_PKT		pRequest,
	uint32				LockState
)
{
	PXIXFS_COMM_HEADER		pHeader = NULL;
	PXIXFS_LOCK_REPLY		pPacketData = NULL;
	//uint64					LotNumber = 0;
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Enter xixfs_SendLockReply\n" ) );

	pHeader = &(pRequest->RawHeadDG);
	// Changed by ILGU HONG
	//	chesung suggest
	//RtlCopyMemory(pHeader->DstMac, pHeader->SrcMac, 6);
	//RtlCopyMemory(pHeader->SrcMac, pVCB->HostMac, 6);

	memcpy(pHeader->DstMac, pHeader->SrcMac, 32);
	memcpy(pHeader->SrcMac, pVCB->XixcoreVcb.HostMac, 32);

	pHeader->Type = XI_HTONL(XIXFS_TYPE_LOCK_REPLY);

	pPacketData = &(pRequest->RawDataDG.LockReply);

	if(LockState == INODE_FILE_LOCK_HAS){
		pPacketData->LotState = XI_HTONL(LOCK_OWNED_BY_OWNER);
	}else{
		pPacketData->LotState = XI_HTONL(LOCK_INVALID);
	}

	xixfs_Pkt_add_sendqueue(
			&xixfs_linux_global.EventCtx,
			pRequest 
			);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_SendLockReply\n" ) );
	return 0;
}



int
xixfs_SendLockReplyFromAuxLotLockInfo(
	PXIXCORE_AUXI_LOT_LOCK_INFO		AuxLotInfo,
	PXIXFSDG_PKT					pRequest,
	uint32							LockState
)
{
	PXIXFS_COMM_HEADER		pHeader = NULL;
	PXIXFS_LOCK_REPLY		pPacketData = NULL;
	//uint64					LotNumber = 0;
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Enter xixfs_SendLockReplyFromAuxLotLockInfo\n" ) );

	pHeader = &(pRequest->RawHeadDG);
	// Changed by ILGU HONG
	//	chesung suggest
	//RtlCopyMemory(pHeader->DstMac, pHeader->SrcMac, 6);
	//RtlCopyMemory(pHeader->SrcMac, AuxLotInfo->HostMac, 6);
	
	memcpy(pHeader->DstMac, pHeader->SrcMac, 32);
	memcpy(pHeader->SrcMac, AuxLotInfo->HostMac, 32);

	pHeader->Type = XI_HTONL(XIXFS_TYPE_LOCK_REPLY);

	pPacketData = &(pRequest->RawDataDG.LockReply);

	if(LockState == INODE_FILE_LOCK_HAS){
		pPacketData->LotState = XI_HTONL(LOCK_OWNED_BY_OWNER);
	}else{
		pPacketData->LotState = XI_HTONL(LOCK_INVALID);
	}

	xixfs_Pkt_add_sendqueue(
			&xixfs_linux_global.EventCtx,
			pRequest 
			);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( "Exit xixfs_SendLockReplyFromAuxLotLockInfo\n" ) );
	return 0;
}


int
xixfs_SendLockReplyRaw(
	PXIXFSDG_PKT		pRequest,
	uint8				*SrcMac,
	uint32				LockState
)
{
	PXIXFS_COMM_HEADER		pHeader = NULL;
	PXIXFS_LOCK_REPLY		pPacketData = NULL;
	//uint64					LotNumber = 0;
	
	//PAGED_CODE();
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Enter xixfs_SendLockReplyRaw\n" ) );

	pHeader = &(pRequest->RawHeadDG);	
	memcpy(pHeader->DstMac, pHeader->SrcMac, 32);
	memcpy(pHeader->SrcMac, SrcMac, 32);

	pHeader->Type = XI_HTONL(XIXFS_TYPE_LOCK_REPLY);

	pPacketData = &(pRequest->RawDataDG.LockReply);

	if(LockState == INODE_FILE_LOCK_HAS){
		pPacketData->LotState = XI_HTONL(LOCK_OWNED_BY_OWNER);
	}else{
		pPacketData->LotState = XI_HTONL(LOCK_INVALID);
	}

	xixfs_Pkt_add_sendqueue(
			&xixfs_linux_global.EventCtx,
			pRequest 
			);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_SendLockReplyRaw\n" ) );
	return 0;
}

/*
struct work_struct *work)
{
	struct aoedev *d = container_of(work, struct aoedev, work);
*/
void
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
xixfs_ProcessLockRequest(struct work_struct *work)
#else
xixfs_ProcessLockRequest(void * data)
#endif
{
	PXIXFSDG_PKT			pRequest = NULL;
	int						RC = 0;
	struct list_head 			*pListEntry = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIXFS_LOCK_REQUEST	pDataHeader = NULL;
	uint32					LockState = INODE_FILE_LOCK_INVALID;
	
	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( KERN_DEBUG "Enter ProcessLockRequest\n" ) );
	
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	pRequest = container_of(work, XIXFSDG_PKT, Pktwork);
#else
	pRequest = (PXIXFSDG_PKT)data;
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	XIXCORE_ASSERT(pRequest);


	pDataHeader = &(pRequest->RawDataDG.LockReq);


	spin_lock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	pListEntry = xixfs_linux_global.sb_list.next;
	while(pListEntry != &(xixfs_linux_global.sb_list)){
		pVCB = container_of(pListEntry, XIXFS_LINUX_VCB, VCBLink);
		

		if( memcmp(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 0){

			spin_unlock(&(xixfs_linux_global.sb_list_lock));
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
					
			RC = xixfs_IsFileLocked(pVCB, XI_NTOHLL(pDataHeader->LotNumber), &LockState);


			
			
			if( RC < 0 ){
				
				xixfs_DereferenceDGPkt(pRequest);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
					("Exit xixfs_ProcessLockRequest\n" ) );
				return;

				
	
				return;
			}else{
						// this pk disappered by client routine
				xixfs_SendLockReply(pVCB, pRequest, LockState);
				printk(KERN_DEBUG "!!! Send Lock Reply 1!!LotNumber(%lld), LotState(%x)\n", 
						XI_NTOHLL(pDataHeader->LotNumber), 
						LockState);
				
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
					("Exit xixfs_ProcessLockRequest\n" ) );
				return;
			}
			
		}



		pVCB = NULL;
		pListEntry = pListEntry->next;
	}
	spin_unlock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));


	if(pVCB == NULL){
	
		PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo = NULL;
		XIXCORE_LISTENTRY				*pAuxLotLockList = NULL;
		XIXCORE_IRQL				oldIrql;
		//uint32						bNewEntry = 0;


		xixcore_AcquireSpinLock(xixcore_global.tmp_lot_lock_list_lock, &oldIrql);
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));
		
		pAuxLotLockList = xixcore_global.tmp_lot_lock_list.next;
		// Check Aux List
	
		
		while(pAuxLotLockList != &(xixcore_global.tmp_lot_lock_list))
		{
			AuxLotInfo = container_of(pAuxLotLockList, XIXCORE_AUXI_LOT_LOCK_INFO, AuxLink);
			if((memcmp(pDataHeader->VolumeId, AuxLotInfo->VolumeId, 16) == 0) && 
				(XI_NTOHLL(pDataHeader->LotNumber) == AuxLotInfo->LotNumber) )
			{
				
				xixcore_RefAuxLotLock(AuxLotInfo);
				xixcore_ReleaseSpinLock(xixcore_global.tmp_lot_lock_list_lock, oldIrql);
				DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));

				LockState = AuxLotInfo->HasLock;
				xixfs_SendLockReplyFromAuxLotLockInfo(AuxLotInfo, pRequest, LockState);
				printk(KERN_DEBUG "!!! Send Lock Reply 2!!\n");

				xixcore_DeRefAuxLotLock(AuxLotInfo);
				return;
			}

			AuxLotInfo = NULL;
			pAuxLotLockList = pAuxLotLockList->next;
		}

		xixcore_ReleaseSpinLock(xixcore_global.tmp_lot_lock_list_lock, oldIrql);
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));
		
		xixfs_SendLockReplyRaw(pRequest, xixcore_global.HostMac, INODE_FILE_LOCK_INVALID);
		printk(KERN_DEBUG "!!! Send Lock Reply 3!!\n");
		return;

	}

	xixfs_DereferenceDGPkt(pRequest);
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_ProcessLockRequest\n" ) );
	return;		
}






int
xixfs_SetFileLock(
	PXIXFS_LINUX_VCB		pVCB,
	uint64			LotNumber
	)
{
	struct inode * inode;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	//int			RC = 0;
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Enter xixfs_SetFileLock\n" ) );
	
	XIXCORE_ASSERT(pVCB->supreblockCtx);

	inode = xixfs_iget(pVCB->supreblockCtx,  LotNumber);
	
	if(inode){

		pFCB = XIXFS_I(inode);
		XIXFS_ASSERT_FCB(pFCB);

		if(pFCB->XixcoreFcb.HasLock == INODE_FILE_LOCK_OTHER_HAS){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
				("Exit xixfs_SetFileLock FCB_FILE_LOCK_OTHER_HAS \n" ) );	
		}
		else {
			pFCB->XixcoreFcb.HasLock = INODE_FILE_LOCK_OTHER_HAS;
		}
		
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
			("Exit xixfs_SetFileLock FCB_FILE_LOCK_OTHER_HAS\n" ) );
		iput(inode);
		return 0;	
	}
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_SetFileLock\n" ) );	
	return 0;	
}



void
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
xixfs_ProcessLockBroadCast(struct work_struct *work)
#else
xixfs_ProcessLockBroadCast(void * data)
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
{
	PXIXFSDG_PKT			pRequest = NULL;
	int						RC = 0;
	struct list_head 			*pListEntry = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIXFS_LOCK_BROADCAST	pDataHeader = NULL;
	



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( KERN_DEBUG "Enter ProcessLockRequest\n" ) );
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	pRequest = container_of(work, XIXFSDG_PKT, Pktwork);
#else
	pRequest = (PXIXFSDG_PKT)data;
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE

	XIXCORE_ASSERT(pRequest);


	pDataHeader = &(pRequest->RawDataDG.LockBroadcast);

	spin_lock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	pListEntry = xixfs_linux_global.sb_list.next;
	while(pListEntry != &(xixfs_linux_global.sb_list)){
		pVCB = container_of(pListEntry, XIXFS_LINUX_VCB, VCBLink);
		XIXFS_ASSERT_VCB(pVCB);

		if( memcmp(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 0){
			
			spin_unlock(&(xixfs_linux_global.sb_list_lock));
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
			
			RC = xixfs_SetFileLock(pVCB, XI_NTOHLL(pDataHeader->LotNumber));

			
			xixfs_DereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
				("Exit ProcessLockBroadCast\n" ) );
			return ;
		}

		pListEntry = pListEntry->next;		
	}
	xixfs_DereferenceDGPkt(pRequest);
	spin_unlock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit ProcessLockBroadCast\n" ) );

	return;
}



int
xixfs_FileFlush(
	PXIXFS_LINUX_VCB 	pVCB, 
	uint64		LotNumber,
	uint64		StartOffset, 
	uint32		DataSize
	)
{
	struct inode * inode;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	int			RC = 0;
	loff_t	map_StartOffset;
	loff_t 	map_EndOffset;

	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
		( KERN_DEBUG "Enter xixfs_FileFlush\n" ) );
	
	XIXCORE_ASSERT(pVCB->supreblockCtx);

	inode = xixfs_iget(pVCB->supreblockCtx,  LotNumber);

	if(inode){
		pFCB = XIXFS_I(inode);
		XIXFS_ASSERT_FCB(pFCB);
		
		
		
		if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_FILE){
			map_StartOffset= StartOffset;
			map_EndOffset = StartOffset + DataSize;

			if(inode->i_mapping) {

#if LINUX_VERSION_OVER_2_6_16				
				truncate_inode_pages(inode->i_mapping, map_StartOffset);
#else
				truncate_inode_pages(inode->i_mapping, map_StartOffset);				
#endif

				
				// filemap_flush(inode->i_mapping);
				 
			}
		}


		iput(inode);
		
		RC = 0;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
			( KERN_DEBUG"Exit xixfs_FileFlush\n" ) );
		return RC;
	}
	
	RC = 0;
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( KERN_DEBUG"Exit xixfs_FileFlush\n" ) );
	return RC;	
}



void
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
xixfs_ProcessFlushBroadCast(struct work_struct *work)
#else
xixfs_ProcessFlushBroadCast(void * data)
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
{
	PXIXFSDG_PKT			pRequest = NULL;
	int						RC = 0;
	struct list_head 			*pListEntry = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIXFS_RANGE_FLUSH_BROADCAST	pDataHeader = NULL;
	



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( KERN_DEBUG "Enter xixfs_ProcessFlushBroadCast\n" ) );

#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	pRequest = container_of(work, XIXFSDG_PKT, Pktwork);
#else
	pRequest = (PXIXFSDG_PKT)data;
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	XIXCORE_ASSERT(pRequest);


	pDataHeader = &(pRequest->RawDataDG.FlushReq);

	spin_lock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));

	
	pListEntry = xixfs_linux_global.sb_list.next;
	while(pListEntry != &(xixfs_linux_global.sb_list)){
		pVCB = container_of(pListEntry, XIXFS_LINUX_VCB, VCBLink);
		XIXFS_ASSERT_VCB(pVCB);
		
		if( memcmp(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 0){
			
			spin_unlock(&(xixfs_linux_global.sb_list_lock));
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
			
			RC = xixfs_FileFlush(pVCB, 
					XI_NTOHLL(pDataHeader->LotNumber), 
					XI_NTOHLL(pDataHeader->StartOffset), 
					XI_NTOHL(pDataHeader->DataSize));

			
			xixfs_DereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
				("Exit xixfs_ProcessFlushBroadCast\n" ) );
			return ;
		}

		pListEntry = pListEntry->next;		
	}
	xixfs_DereferenceDGPkt(pRequest);
	spin_unlock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_ProcessFlushBroadCast\n" ) );

	return;
}



int
xixfs_ChangeFileLen(
	PXIXFS_LINUX_VCB		pVCB,
	uint64			LotNumber,
	uint64			FileLen,
	uint64			AllocationLen,
	uint64			WriteStartOffset
)	
{
	struct inode * inode;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	int			RC = 0;
	uint64		PrevFileSize = 0;
	loff_t		map_StartOffset = 0;
	loff_t 		map_EndOffset = 0;

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Enter xixfs_ChangeFileLen\n" ) );	

	XIXCORE_ASSERT(pVCB->supreblockCtx);

	inode = xixfs_iget(pVCB->supreblockCtx,  LotNumber);

	if(inode){
		pFCB = XIXFS_I(inode);
		XIXFS_ASSERT_FCB(pFCB);
		
		spin_lock(&(pFCB->FCBLock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_lock(&(pFCB->FCBLock) pFCB(%p)\n", pFCB));

		
		//PrevFileSize = pFCB->FileSize;
		PrevFileSize = i_size_read(inode);
		//pFCB->FileSize = FileLen;
		//pFCB->ValidDataLength = FileLen;
		i_size_write(inode, (uint32)FileLen);
		pFCB->XixcoreFcb.AllocationSize= AllocationLen;
		spin_unlock(&(pFCB->FCBLock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_unlock(&(pFCB->FCBLock) pFCB(%p)\n", pFCB));


		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
			("PrevFileSize (%lld) FileLen %lld ValidDataLength %lld AllocationSize %lld\n",
				PrevFileSize, FileLen, FileLen, AllocationLen) );


		printk(KERN_DEBUG "PrevFileSize (%lld) FileLen %lld ValidDataLength %lld AllocationSize %lld\n",
				PrevFileSize, FileLen, FileLen, AllocationLen);


		if((uint64)PrevFileSize > WriteStartOffset){
			map_StartOffset = WriteStartOffset;
			map_EndOffset = PrevFileSize;


			if(inode->i_mapping) {
#if LINUX_VERSION_OVER_2_6_16			
				truncate_inode_pages(inode->i_mapping, map_StartOffset);
#else
				truncate_inode_pages(inode->i_mapping, map_StartOffset);				
#endif
			}
		}

		
		

		iput(inode); 

		RC = 0;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
			("Exit ChangeFileLen\n" ) );
		return RC;
	}
	
	RC = 0;
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit ChangeFileLen\n" ) );	
	return RC;	
}


void
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
xixfs_ProcessFileLenChange(struct work_struct *work)
#else
xixfs_ProcessFileLenChange(void * data)
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
{
	PXIXFSDG_PKT			pRequest = NULL;
	int						RC = 0;
	struct list_head 			*pListEntry = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIXFS_FILE_LENGTH_CHANGE_BROADCAST	pDataHeader = NULL;
	



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( KERN_DEBUG "Enter xixfs_ProcessFileLenChange\n" ) );

#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	pRequest = container_of(work, XIXFSDG_PKT, Pktwork);
#else
	pRequest = (PXIXFSDG_PKT)data;
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	XIXCORE_ASSERT(pRequest);





	pDataHeader = &(pRequest->RawDataDG.FileLenChangeReq);

	/*
	printk(KERN_DEBUG "FileLenChange LotNum(%ld), FileLen(%ld)\n",
				XI_NTOHLL(pDataHeader->LotNumber), 
				XI_NTOHLL(pDataHeader->FileLength)
				);
	*/
	spin_lock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	pListEntry = xixfs_linux_global.sb_list.next;
	while(pListEntry != &(xixfs_linux_global.sb_list)){
		pVCB = container_of(pListEntry, XIXFS_LINUX_VCB, VCBLink);
		XIXFS_ASSERT_VCB(pVCB);
		
		if( memcmp(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 0){
			
			spin_unlock(&(xixfs_linux_global.sb_list_lock));
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
			
			RC = xixfs_ChangeFileLen(pVCB, 
					XI_NTOHLL(pDataHeader->LotNumber), 
					XI_NTOHLL(pDataHeader->FileLength), 
					XI_NTOHLL(pDataHeader->AllocationLength),
					XI_NTOHLL(pDataHeader->WriteStartOffset));

			
			xixfs_DereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
				("Exit xixfs_ProcessFileLenChange\n" ) );
			return ;
		}

		pListEntry = pListEntry->next;		
	}
	xixfs_DereferenceDGPkt(pRequest);
	spin_unlock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_ProcessFileLenChange\n" ) );

	return;
}







int
xixfs_FileChange(
	PXIXFS_LINUX_VCB	pVCB,
	uint64		LotNumber,
	uint32		SubCommand,
	uint64		OldParentLotNumber,
	uint64		NewParentFcb
)
{
	struct inode * inode = NULL;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	int			RC = 0;

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Enter xixfs_FileChange\n" ) );	
	
	XIXCORE_ASSERT(pVCB->supreblockCtx);

	inode = xixfs_iget(pVCB->supreblockCtx,  LotNumber);

	if(inode){

		
		pFCB = XIXFS_I(inode);
		XIXFS_ASSERT_FCB(pFCB);
		
		spin_lock(&(pFCB->FCBLock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_lock(&(pFCB->FCBLock) pFCB(%p)\n", pFCB));
		
		if(SubCommand == XIXFS_SUBTYPE_FILE_DEL){
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);
			printk(KERN_DEBUG "XIX EVENT XIXFS_LINUX_FCB_CHANGE_DELETED\n");
		}else if(SubCommand == XIXFS_SUBTYPE_FILE_MOD){
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_RELOAD);
			printk(KERN_DEBUG "XIX EVENT XIXFS_LINUX_FCB_CHANGE_RELOAD\n");
		}else if(SubCommand == XIXFS_SUBTYPE_FILE_RENAME){
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_RENAME);	
			printk(KERN_DEBUG "XIX EVENT XIXFS_LINUX_FCB_CHANGE_RENAME\n");
		}else if(SubCommand == XIXFS_SUBTYPE_FILE_LINK){
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_LINK);
			printk(KERN_DEBUG "XIX EVENT XIXFS_LINUX_FCB_CHANGE_LINK\n");
		}
		spin_unlock(&(pFCB->FCBLock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_unlock(&(pFCB->FCBLock) pFCB(%p)\n", pFCB));

		if(SubCommand == XIXFS_SUBTYPE_FILE_RENAME){
			
			//if(OldParentLotNumber != NewParentFcb){
				d_prune_aliases(inode);
			//}
			
		}else if (SubCommand == XIXFS_SUBTYPE_FILE_DEL) {
			d_prune_aliases(inode);
			truncate_inode_pages(&inode->i_data, 0);
		}
		
 		iput(inode);

		
		RC = 0;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
			("Exit xixfs_FileChange\n" ) );	
		return RC;
	}
	
	RC = 0;
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_FileChange\n" ) );	
	return RC;	
}



void
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
xixfs_ProcessFileChange(struct work_struct *work)
#else
xixfs_ProcessFileChange(void * data)
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
{
	PXIXFSDG_PKT			pRequest = NULL;
	int						RC = 0;
	struct list_head 			*pListEntry = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIXFS_FILE_CHANGE_BROADCAST	pDataHeader = NULL;
	



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( KERN_DEBUG "Enter xixfs_ProcessFileChange\n" ) );

#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	pRequest = container_of(work, XIXFSDG_PKT, Pktwork);
#else
	pRequest = (PXIXFSDG_PKT)data;
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	XIXCORE_ASSERT(pRequest);


	pDataHeader = &(pRequest->RawDataDG.FileChangeReq);

	/*
	printk(KERN_DEBUG "!!FileChange LotNum(%ld), SubCommand(%x)\n",
				XI_NTOHLL(pDataHeader->LotNumber), 
					XI_NTOHL(pDataHeader->SubCommand)
				);
	*/


	spin_lock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	pListEntry = xixfs_linux_global.sb_list.next;
	while(pListEntry != &(xixfs_linux_global.sb_list)){
		pVCB = container_of(pListEntry, XIXFS_LINUX_VCB, VCBLink);
		XIXFS_ASSERT_VCB(pVCB);
		
		if( memcmp(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 0){
			
			spin_unlock(&(xixfs_linux_global.sb_list_lock));
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
			
			RC =  xixfs_FileChange(pVCB, 
					XI_NTOHLL(pDataHeader->LotNumber), 
					XI_NTOHL(pDataHeader->SubCommand),
					XI_NTOHLL(pDataHeader->PrevParentLotNumber),
					XI_NTOHLL(pDataHeader->NewParentLotNumber)
					);

			
			xixfs_DereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
				("Exit xixfs_ProcessFileChange\n" ) );
			return ;
		}

		pListEntry = pListEntry->next;		
	}
	xixfs_DereferenceDGPkt(pRequest);
	spin_unlock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_ProcessFileChange\n" ) );

	return;
}





int
xixfs_DirChange(
	PXIXFS_LINUX_VCB pVCB, 
	uint64	LotNumber, 
	uint32 ChildSlotNumber,
	uint32 SubCommand
)
{
	struct inode * inode = NULL;
	PXIXFS_LINUX_FCB		pFCB = NULL;
	int				RC = 0;
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Enter DirChange\n" ) );		
	
	XIXCORE_ASSERT(pVCB->supreblockCtx);

	inode = xixfs_iget(pVCB->supreblockCtx,  LotNumber);

	if(inode){

		
		pFCB = XIXFS_I(inode);
		XIXFS_ASSERT_FCB(pFCB);
		
		spin_lock(&(pFCB->FCBLock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_lock(&(pFCB->FCBLock) pFCB(%p)\n", pFCB));
		
		
		
		if(!XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.Type, 
			(XIFS_FD_TYPE_DIRECTORY| XIFS_FD_TYPE_ROOT_DIRECTORY)))
		{
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Not Dir\n" ) );
			spin_unlock(&(pFCB->FCBLock));
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
				("spin_unlock(&(pFCB->FCBLock) pFCB(%p)\n", pFCB));
			
			iput(inode);
			return 0;
		}
		

		if(SubCommand == XIXFS_SUBTYPE_DIR_ADD){
			/*
			RC = XifsdSearchChildByIndex(pFCB, ChildSlotNumber, FALSE, TRUE, &pChild);
			if(!NT_SUCCESS(RC)){
				XifsdDecRefFCB(pFCB);
				return STATUS_SUCCESS;
			}
			XifsdSetFlag(pChild->ChildFlag, XIFSD_CHILD_CHANGED_ADDED) ;
			setBitToMap(ChildSlotNumber, pFCB->ChildLotMap);
			*/
		}else if(SubCommand == XIXFS_SUBTYPE_DIR_DEL){
			/*
			RC = XifsdSearchChildByIndex(pFCB, ChildSlotNumber, FALSE, TRUE, &pChild);
			if(!NT_SUCCESS(RC)){
				XifsdDecRefFCB(pFCB);
				return STATUS_SUCCESS;
			
			}
			clearBitToMap(ChildSlotNumber, pFCB->ChildLotMap);
			XifsdSetFlag(pChild->ChildFlag, XIFSD_CHILD_CHANGED_DELETE) ;
			*/
		}else {
			/*
			RC = XifsdSearchChildByIndex(pFCB, ChildSlotNumber, FALSE, TRUE, &pChild);
			if(!NT_SUCCESS(RC)){
				XifsdDecRefFCB(pFCB);
				return STATUS_SUCCESS;
				
			}
		
			XifsdSetFlag(pChild->ChildFlag, XIFSD_CHILD_CHANGED_UPDATE) ;
			*/
			
		}
		
		spin_unlock(&(pFCB->FCBLock));
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_unlock(&(pFCB->FCBLock) pFCB(%p)\n", pFCB));


		iput(inode);
		
		RC = 0;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
			("Exit DirChange\n" ) );
		return RC;	
	}
	
	RC = 0;
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit DirChange\n" ) );	
	return RC;	
}


void
#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
xixfs_ProcessDirChange(struct work_struct *work)
#else
xixfs_ProcessDirChange(void * data)
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
{
	PXIXFSDG_PKT			pRequest = NULL;
	int						RC = 0;
	struct list_head 			*pListEntry = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIXFS_DIR_ENTRY_CHANGE_BROADCAST	pDataHeader = NULL;
	



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		( KERN_DEBUG "Enter xixfs_ProcessDirChange\n" ) );

#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	pRequest = container_of(work, XIXFSDG_PKT, Pktwork);
#else
	pRequest = (PXIXFSDG_PKT)data;
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
	XIXCORE_ASSERT(pRequest);


	pDataHeader = &(pRequest->RawDataDG.DirChangeReq);

	spin_lock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	pListEntry = xixfs_linux_global.sb_list.next;
	while(pListEntry != &(xixfs_linux_global.sb_list)){
		pVCB = container_of(pListEntry, XIXFS_LINUX_VCB, VCBLink);
		XIXFS_ASSERT_VCB(pVCB);


		if( memcmp(pVCB->XixcoreVcb.VolumeId, pDataHeader->VolumeId, 16) == 0){
			
			spin_unlock(&(xixfs_linux_global.sb_list_lock));
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
			
			RC =  xixfs_DirChange(pVCB, 
					XI_NTOHLL(pDataHeader->LotNumber), 
					XI_NTOHL(pDataHeader->ChildSlotNumber),
					XI_NTOHL(pDataHeader->SubCommand)
					);

			
			xixfs_DereferenceDGPkt(pRequest);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_HOSTCOM, 
				("Exit xixfs_ProcessDirChange\n" ) );
			return ;
		}

		pListEntry = pListEntry->next;		
	}
	xixfs_DereferenceDGPkt(pRequest);
	spin_unlock(&(xixfs_linux_global.sb_list_lock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixfs_linux_global.sb_list_lock)) \n" ));
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, 
		("Exit xixfs_ProcessDirChange\n" ) );

	return;
}



void 
xixfs_init_wait_ctx(
	PXIXFS_WAIT_CTX wait
)
{
	INIT_LIST_HEAD(&(wait->WaitList));
	init_completion(&(wait->WaitCompletion));
}


void 
xixfs_add_resource_wait_list(
	PXIXFS_WAIT_CTX wait,
	PXIXFS_LINUX_META_CTX ctx
)
{

	spin_lock(&(ctx->VCBResourceWaitSpinlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_lock(&(ctx->VCBResourceWaitSpinlock)) ctx(%p)\n", ctx));
	list_add_tail(&wait->WaitList, &ctx->VCBResourceWaitList);
	spin_unlock(&(ctx->VCBResourceWaitSpinlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_unlock(&(ctx->VCBResourceWaitSpinlock)) ctx(%p)\n", ctx));
}

void 
xixfs_remove_resource_wait_list(
	PXIXFS_WAIT_CTX wait,
	PXIXFS_LINUX_META_CTX ctx
)
{

	spin_lock(&(ctx->VCBResourceWaitSpinlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_lock(&(ctx->VCBResourceWaitSpinlock)) ctx(%p)\n", ctx));
	list_del_init(&wait->WaitList);
	spin_unlock(&(ctx->VCBResourceWaitSpinlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_unlock(&(ctx->VCBResourceWaitSpinlock)) ctx(%p)\n", ctx));
}

void 
xixfs_wakeup_resource_waiter(
		PXIXFS_LINUX_META_CTX ctx
)
{
	struct list_head *pos, *tmp;
	PXIXFS_WAIT_CTX wait;

 	
	spin_lock(&(ctx->VCBResourceWaitSpinlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_lock(&(ctx->VCBResourceWaitSpinlock)) ctx(%p)\n", ctx));
	list_for_each_safe(pos, tmp, &ctx->VCBResourceWaitList) {
		wait = list_entry(pos, XIXFS_WAIT_CTX, WaitList);
		list_del_init(&wait->WaitList);
		complete(&(wait->WaitCompletion));
	}
	spin_unlock(&(ctx->VCBResourceWaitSpinlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_unlock(&(ctx->VCBResourceWaitSpinlock)) ctx(%p)\n", ctx));
}

#if !LINUX_VERSION_25_ABOVE

void xixfs_metaevent_timeouthandler(unsigned long data)
{
	PXIXFS_LINUX_META_CTX pCtx = (PXIXFS_LINUX_META_CTX)data;
	PXIXFS_LINUX_VCB		pVCB = NULL;
	PXIXCORE_META_CTX		xixcoreCtx = NULL;

	pVCB = pCtx->VCBCtx;
	XIXFS_ASSERT_VCB(pVCB);
	xixcoreCtx = &pVCB->XixcoreVcb.MetaContext;

	spin_lock(&(pCtx->MetaLock));
	XIXCORE_SET_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_TIMEOUT);
	spin_unlock(&(pCtx->MetaLock));
	//printk(KERN_DEBUG "wake up meta update event \n");
	wake_up(&(pCtx->VCBMetaEvent));
}

void xixfs_clievent_timeouthandler(unsigned long data)
{
	
	spin_lock(&(xixfs_linux_global.EventCtx.Eventlock));
	XIXCORE_SET_FLAGS(xixfs_linux_global.EventCtx.CliEventFlags, XIXFS_EVENT_CLI_TIMEOUT);
	spin_unlock(&(xixfs_linux_global.EventCtx.Eventlock));
	//printk(KERN_DEBUG "wake up client event timeout \n");
	wake_up(&(xixfs_linux_global.EventCtx.CliEvent));
}


void 
xixfs_add_metaThread_stop_wait_list(
	PXIXFS_WAIT_CTX wait,
	PXIXFS_LINUX_META_CTX ctx
)
{

	spin_lock(&(ctx->VCBMetaThreadStopWaitSpinlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_lock(&(ctx->VCBResourceWaitSpinlock)) ctx(%p)\n", ctx));
	list_add_tail(&wait->WaitList, &ctx->VCBMetaThreadStopWaitList);
	spin_unlock(&(ctx->VCBMetaThreadStopWaitSpinlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_unlock(&(ctx->VCBResourceWaitSpinlock)) ctx(%p)\n", ctx));
}


void 
xixfs_wakeup_metaThread_stop_waiter(
		PXIXFS_LINUX_META_CTX ctx
)
{
	struct list_head *pos, *tmp;
	PXIXFS_WAIT_CTX wait;

 	
	spin_lock(&(ctx->VCBMetaThreadStopWaitSpinlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_lock(&(ctx->VCBResourceWaitSpinlock)) ctx(%p)\n", ctx));
	list_for_each_safe(pos, tmp, &ctx->VCBMetaThreadStopWaitList) {
		wait = list_entry(pos, XIXFS_WAIT_CTX, WaitList);
		list_del_init(&wait->WaitList);
		complete(&(wait->WaitCompletion));
	}
	spin_unlock(&(ctx->VCBMetaThreadStopWaitSpinlock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("spin_unlock(&(ctx->VCBResourceWaitSpinlock)) ctx(%p)\n", ctx));
}
#endif
