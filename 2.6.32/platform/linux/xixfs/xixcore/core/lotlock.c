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

#include "xcsystem/debug.h"
#include "xcsystem/errinfo.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/buffer.h"
#include "xixcore/ondisk.h"
#include "xixcore/file.h"
#include "xixcore/lotlock.h"
#include "internal_types.h"

/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCLOTLOCK"

int
xixcore_call
xixcore_ReadLockState(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint64				LotNumber,
	xc_uint32				LotSize,
	xc_uint32				SectorSize,
	xc_uint32				SectorSizeBit,
	xc_uint8				*pHostId,
	xc_uint8				*pLockOwnerId,
	xc_uint8				*pLockOwnerMac,
	xc_uint32				*LotState,
	xc_uint64				*LotLockAcqTime,
	PXIXCORE_BUFFER			xbuf,
	xc_int32				*Reason
	)
{
	int					RC = 0;
	PXIDISK_LOCK		LotLock = NULL;
	

	*Reason = 0;
	*LotState = INODE_FILE_LOCK_INVALID;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixcore_ReadLockState \n"));

	XIXCORE_ASSERT(bdev);
	XIXCORE_ASSERT(xbuf);
	XIXCORE_ASSERT(xixcore_GetBufferSize(xbuf) >= XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
	
	xixcore_ZeroBufferOffset(xbuf);
	memset(xixcore_GetDataBuffer(xbuf), 0, xixcore_GetBufferSize(xbuf));


	RC = xixcore_RawReadLotHeader(bdev, 
								LotSize, 
								SectorSize,
								SectorSizeBit,
								LotNumber, 
								xbuf,
								Reason
								);



	if ( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR xixcore_ReadLockState : call xixcore_RawReadLotHeader (0x%x) \n", *Reason));

		goto exit;
		
	}
	


	LotLock = (PXIDISK_LOCK)xixcore_GetDataBufferWithOffset(xbuf);

	memcpy(pLockOwnerId, LotLock->LockHostSignature, 16);
	memcpy(pLockOwnerMac, LotLock->LockHostMacAddress, 32);
	*LotLockAcqTime = LotLock->LockAcquireTime;

	if(LotLock->LockState == XIDISK_LOCK_RELEASED){
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
				("xixcore_ReadLockState : Lock is Free \n"));

		*LotState = INODE_FILE_LOCK_INVALID;
	}
	else if(LotLock->LockState == XIDISK_LOCK_ACQUIRED){
		if(memcmp(pHostId ,pLockOwnerId, 16) != 0){
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
					("xixcore_ReadLockState : Already Lock is set by other! \n"));

			*LotState = INODE_FILE_LOCK_OTHER_HAS;
		}else{
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
					("xixcore_ReadLockState : Already Lock has! \n"));

			*LotState = INODE_FILE_LOCK_HAS;
		}
	}else {
			DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
					("xixcore_ReadLockState : First Time lock acq! \n"));

			*LotState = INODE_FILE_LOCK_INVALID;
	}




	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit xixcore_ReadLockState  \n"));

exit:
	return RC;
}


int
xixcore_call
xixcore_LotInvalidateRoutine(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint64				LotNumber,
	xc_uint32				LotSize,
	xc_uint32				SectorSize,
	xc_uint32				SectorSizeBit,
	xc_uint8				*pLockHostId,
	xc_uint8				*pLockHostMac,
	PXIXCORE_BUFFER			xbuf,
	xc_int32				*Reason
	)
{
	int					RC = 0;
	
	PXIDISK_LOCK		Lock = NULL;
	*Reason = 0;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixcore_LotUnLockRoutine \n"));

	XIXCORE_ASSERT(bdev);
	XIXCORE_ASSERT(xbuf);
	XIXCORE_ASSERT(xixcore_GetBufferSizeWithOffset(xbuf) >= XIDISK_COMMON_LOT_HEADER_SIZE);


	Lock = (PXIDISK_LOCK)xixcore_GetDataBufferWithOffset(xbuf);
	Lock->LockAcquireTime = xixcore_GetCurrentTime64();

	
	memcpy(Lock->LockHostSignature,pLockHostId,16);
	memcpy(Lock->LockHostMacAddress, pLockHostMac, 32);

	Lock->LockState = XIDISK_LOCK_RELEASED;



	

	RC = xixcore_RawWriteLotHeader(bdev, 
							LotSize, 
							SectorSize,
							SectorSizeBit,
							LotNumber, 
							xbuf, 
							Reason
							);

	if ( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR xixcore_LotUnLockRoutine : call xixcore_RawWriteLotHeader(0x%x) \n", *Reason));

		goto exit;
	
	}
		
		
	RC = 0;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit xixcore_LotUnLockRoutine \n"));

exit:

	return RC;
	
}



int
xixcore_call
xixcore_LotUnLockRoutine(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint64				LotNumber,
	xc_uint32				LotSize,
	xc_uint32				SectorSize,
	xc_uint32				SectorSizeBit,
	xc_uint8				*pLockHostId,
	xc_uint8				*pLockHostMac,
	PXIXCORE_BUFFER			xbuf,
	xc_int32				*Reason
	)
{
	int					RC = 0;
	PXIDISK_LOCK		Lock = NULL;
	*Reason = 0;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixcore_LotUnLockRoutine \n"));

	XIXCORE_ASSERT(bdev);
	XIXCORE_ASSERT(xbuf);
	XIXCORE_ASSERT(xixcore_GetBufferSize(xbuf) >= XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	xixcore_ZeroBufferOffset(xbuf);
	memset(xixcore_GetDataBuffer(xbuf), 0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
	

	
	RC = xixcore_RawReadLotHeader(bdev, 
								LotSize, 
								SectorSize,
								SectorSizeBit,
								LotNumber, 
								xbuf,
								Reason
								);

	if ( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR xixcore_LotUnLockRoutine : call xixcore_RawReadLotHeader (0x%x) \n", *Reason));

		goto exit;
		
	}
		
	

	Lock = (PXIDISK_LOCK)xixcore_GetDataBufferWithOffset(xbuf);
	Lock->LockAcquireTime = xixcore_GetCurrentTime64();

	
	memcpy(Lock->LockHostSignature,pLockHostId,16);
	memcpy(Lock->LockHostMacAddress, pLockHostMac, 32);

	Lock->LockState = XIDISK_LOCK_RELEASED;



	

	RC = xixcore_RawWriteLotHeader(bdev, 
							LotSize, 
							SectorSize,
							SectorSizeBit,
							LotNumber, 
							xbuf, 
							Reason
							);

	if ( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR xixcore_LotUnLockRoutine : call xixcore_RawWriteLotHeader(0x%x) \n", *Reason));

		goto exit;
	
	}
		
		
	RC = 0;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit xixcore_LotUnLockRoutine \n"));

exit:

	return RC;
	
}


int
xixcore_call
xixcore_LotLockRoutine(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint64				LotNumber,
	xc_uint32				LotSize,
	xc_uint32				SectorSize,
	xc_uint32				SectorSizeBit,
	xc_uint8				*pLockHostId,
	xc_uint8				*pLockHostMac,
	xc_uint8				*VolumeId,
	xc_uint32				CheckLotLock,
	PXIXCORE_BUFFER			xbuf,
	xc_int32				*Reason
	)
{
	int				RC;
	xc_uint32			lockstate = INODE_FILE_LOCK_INVALID;
	
	xc_uint8			LotOwnerMac[32];
	xc_uint8			LotOwnerId[16];
	xc_uint64			LotOwnerAcqTime = 0;

	xc_uint8			tmpLotOwnerMac[32];
	xc_uint8			tmpLotOwnerId[16];
	xc_uint64			tmpLotOwnerAcqTime = 0;
	
	PXIDISK_LOCK		Lock = NULL;
	
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixcore_LotLockRoutine \n"));


	XIXCORE_ASSERT(bdev);
	XIXCORE_ASSERT(xbuf);
	XIXCORE_ASSERT(xixcore_GetBufferSize(xbuf) >= XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	xixcore_ZeroBufferOffset(xbuf);
	memset(xixcore_GetDataBuffer(xbuf), 0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);



	
	if(CheckLotLock){
		
		RC = xixcore_ReadLockState(
				bdev, 
				LotNumber,
				LotSize ,
				SectorSize,
				SectorSizeBit,
				pLockHostId,
				LotOwnerId,
				LotOwnerMac,
				&lockstate,
				&LotOwnerAcqTime,
				xbuf,
				Reason
				);
		

		if ( RC < 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ERROR xixcore_LotLockRoutine : call xixcore_ReadLockState(0x%x) \n", *Reason));

			goto exit;
			
		}


	}else{
		lockstate = INODE_FILE_LOCK_INVALID;
	}


	if(lockstate != INODE_FILE_LOCK_INVALID)
	{
		if(lockstate == INODE_FILE_LOCK_OTHER_HAS) {

			xixcore_ReleaseDevice(bdev);

			
			RC = xixcore_HaveLotLock(
							pLockHostMac,
							LotOwnerMac,
							LotNumber,
							VolumeId,
							LotOwnerId
							);
			

			if( RC == 1){

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("ERROR xixcore_LotLockRoutine : call xixcore_AreYouHaveLotLock Other has lock \n"));
						
				RC= XCCODE_UNSUCCESS;
				goto exit;
			}

			if( RC < 0 ) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("ERROR xixcore_LotLockRoutine : call xixcore_AreYouHaveLotLock(0x%x) \n", *Reason));

				goto exit;				
			}


			RC = xixcore_AcquireDeviceLock(bdev);

			if(RC < 0 ) {
				RC = XCCODE_EPERM;
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail : xixcore_LotLock in call xixcore_AcquireDeviceLock %x\n", 
					RC));
				
				goto exit;		
			}


			if(CheckLotLock){
				
				RC = xixcore_ReadLockState(
						bdev, 
						LotNumber,
						LotSize ,
						SectorSize,
						SectorSizeBit,
						pLockHostId,
						tmpLotOwnerId,
						tmpLotOwnerMac,
						&lockstate,
						&tmpLotOwnerAcqTime,
						xbuf,
						Reason
						);
				

				if ( RC < 0 ) {
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("ERROR xixcore_LotLockRoutine : call xixcore_ReadLockState(0x%x) \n", *Reason));

					goto exit;
					
				}


				if(	(memcmp(LotOwnerId, tmpLotOwnerId, 16) != 0)
					|| (memcmp(LotOwnerMac, tmpLotOwnerMac, 32) != 0 )
					|| (LotOwnerAcqTime != tmpLotOwnerAcqTime)
					) 
				{
					RC = XCCODE_EPERM;
					goto exit;
				}

				

			}
			
		}
		
		RC = xixcore_LotInvalidateRoutine(
								bdev,
								LotNumber,
								LotSize,
								SectorSize,
								SectorSizeBit,
								pLockHostId,
								pLockHostMac,
								xbuf,
								Reason
								);
		

		if ( RC < 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ERROR xixcore_LotLockRoutine : call xixcore_LotUnLockRoutine(0x%x) \n", *Reason));

			goto exit;
			
		}
		
		lockstate = INODE_FILE_LOCK_INVALID;
		
	}



	Lock = (PXIDISK_LOCK)xixcore_GetDataBufferWithOffset(xbuf);
	Lock->LockAcquireTime = xixcore_GetCurrentTime64();
	memcpy(Lock->LockHostSignature,pLockHostId, 16);
	memcpy(Lock->LockHostMacAddress,pLockHostMac,32);
	Lock->LockState = XIDISK_LOCK_ACQUIRED;



		
	RC = xixcore_RawWriteLotHeader(bdev, 
							LotSize, 
							SectorSize, 
							SectorSizeBit,
							LotNumber, 
							xbuf, 
							Reason
							);
	
	if ( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR xixcore_LotLockRoutine : call xixcore_RawWriteLotHeader(0x%x) \n", *Reason));

		goto exit;
		
	}
		
		




	RC = 0;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit xixcore_LotLockRoutine \n"));

exit:
	return RC;
}





int
xixcore_call
xixcore_LotLock(
	PXIXCORE_VCB	pVCB,
	xc_uint64		LotNumber,
	xc_uint32		*status,
	xc_uint32		bCheck,
	xc_uint32		bRetry
	)
{
	int 	RC = 0;
	int 	TryCount = 0;
	int		Reason = 0;
	PXIXCORE_BUFFER			xbuf = NULL;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_LOCK,
		("Enter xixcore_LotLock\n"));

	*status = 0;

	xbuf = xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if( !xbuf ) {
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR xixcore_LotLock : xixcore_AllocateBuffer \n"));

		return XCCODE_ENOMEM;
	}

	memset(xixcore_GetDataBuffer(xbuf), 0, xixcore_GetBufferSize(xbuf));


retry:

	RC = xixcore_AcquireDeviceLock(pVCB->XixcoreBlockDevice);

	if ( RC < 0 ) {

		RC = XCCODE_EPERM;
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail : xixcore_LotLock in call xixcore_AcquireDeviceLock %x\n"));

			xixcore_ReleaseDevice(pVCB->XixcoreBlockDevice);

			goto error_out;

	}

	
	RC = xixcore_LotLockRoutine(pVCB->XixcoreBlockDevice, 
								LotNumber, 
								pVCB->LotSize, 
								pVCB->SectorSize,
								pVCB->SectorSizeBit,
								pVCB->HostId,
								pVCB->HostMac,
								pVCB->VolumeId,
								bCheck,
								xbuf,
								&Reason
								);

	xixcore_ReleaseDevice(pVCB->XixcoreBlockDevice);

	if ( RC < 0 ) {
		RC =  XCCODE_EPERM;
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail : xixcore_LotLock in call xixcore_AcquireDeviceLock %x reason %x\n", RC, Reason));


		if( bRetry && TryCount < 3 ) {
			TryCount ++;
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Retry xixcore_LotLock TryCount %d", TryCount));

			goto retry;
		}else{
			goto error_out;
		}		
		
	}
	
	RC = 0;
	
		
error_out:	

	if( RC < 0 ) {
		*status = INODE_FILE_LOCK_INVALID;
	}else {
		*status = INODE_FILE_LOCK_HAS;
	}

	xixcore_FreeBuffer(xbuf);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_LOCK,
		("Exit xixcore_LotLock %x\n", RC));	
return RC;	
}


int 
xixcore_call
xixcore_LotUnLock(
	PXIXCORE_VCB	pVCB,
	xc_uint64		LotNumber,
	xc_uint32		*status
)
{
	int 	RC = 0;
	int		Reason = 0;
	PXIXCORE_BUFFER			xbuf = NULL;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_LOCK,
		("Enter xixcore_LotUnlock\n"));


	xbuf = xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if( !xbuf ) {
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR xixcore_LotUnLock : xixcore_AllocateBuffer \n"));

		return XCCODE_ENOMEM;
	}

	memset(xixcore_GetDataBuffer(xbuf), 0, xixcore_GetBufferSize(xbuf));



	RC = xixcore_AcquireDeviceLock(pVCB->XixcoreBlockDevice);

	if ( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail :xixcore_LotUnlock in call xixcore_AcquireDeviceLock %x\n"));
			goto error_out;

	}

	RC = xixcore_LotUnLockRoutine(pVCB->XixcoreBlockDevice, 
								LotNumber, 
								pVCB->LotSize, 
								pVCB->SectorSize,
								pVCB->SectorSizeBit,
								pVCB->HostId,
								pVCB->HostMac,
								xbuf,
								&Reason
								);
								
	xixcore_ReleaseDevice(pVCB->XixcoreBlockDevice);

	

error_out:
	*status = INODE_FILE_LOCK_INVALID;
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_LOCK,
		("Exit xixcore_LotUnlock %x\n", RC));	
	xixcore_FreeBuffer(xbuf);
	return RC;
}


int 
xixcore_call
xixcore_LockedOp(
	PXIXCORE_VCB	pVCB,
	xc_uint64		LotNumber,
	xc_uint32		*status,
	xc_uint32		bCheck,
	PXIXCORE_LOCKEDFUNCTION func,
	xc_uint8		*arg
)
{

	int 	RC = 0;
	int		Reason = 0;
	PXIXCORE_BUFFER			xbuf = NULL;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_LOCK,
		("Enter xixcore_LotLock\n"));
	
	*status = 0;

	xbuf = xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if( !xbuf ) {
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR xixcore_ReadLockState : xixcore_AllocateBuffer \n"));

		return XCCODE_ENOMEM;
	}

	memset(xixcore_GetDataBuffer(xbuf), 0, xixcore_GetBufferSize(xbuf));



	RC = xixcore_AcquireDeviceLock(pVCB->XixcoreBlockDevice);

	if ( RC < 0 ) {

		RC = XCCODE_EPERM;
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail : xixcore_LotLock in call xixcore_AcquireDeviceLock %x\n", 
			RC));

			xixcore_ReleaseDevice(pVCB->XixcoreBlockDevice);

			goto error_out;

	}

	RC = xixcore_LotLockRoutine(pVCB->XixcoreBlockDevice, 
								LotNumber, 
								pVCB->LotSize, 
								pVCB->SectorSize,
								pVCB->SectorSizeBit,
								pVCB->HostId,
								pVCB->HostMac,
								pVCB->VolumeId,
								bCheck,
								xbuf,
								&Reason
								);

	

	if ( RC < 0 ) {
		RC =  XCCODE_EPERM;
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail : xixcore_LotLock in call xixcore_AcquireDeviceLock %x reason %x\n", RC, Reason));
		*status = INODE_FILE_LOCK_INVALID;
		goto release_out;
	}else{
		*status = INODE_FILE_LOCK_HAS;
	}
	
	RC = (*func)(arg);
	
release_out:
	xixcore_LotInvalidateRoutine(pVCB->XixcoreBlockDevice, 
								LotNumber, 
								pVCB->LotSize, 
								pVCB->SectorSize,
								pVCB->SectorSizeBit,
								pVCB->HostId,
								pVCB->HostMac,
								xbuf,
								&Reason
								);

	xixcore_ReleaseDevice(pVCB->XixcoreBlockDevice);
		
error_out:	
	*status = INODE_FILE_LOCK_INVALID;
	
	xixcore_FreeBuffer(xbuf);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_LOCK,
		("Exit xixcore_LotLock %x\n", RC));	
return RC;	

}



void
xixcore_call
xixcore_RefAuxLotLock(
	PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo
)
{
	XIXCORE_ASSERT(xixcore_ReadAtomic(&(AuxLotInfo->RefCount)) > 0 );
	xixcore_IncreaseAtomic(&(AuxLotInfo->RefCount));
	return;
}


void
xixcore_call
xixcore_DeRefAuxLotLock(
	PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo
)
{
	
	XIXCORE_ASSERT(xixcore_ReadAtomic(&(AuxLotInfo->RefCount)) > 0 );
	xixcore_DecreaseAtomic(&(AuxLotInfo->RefCount));
	
		
	if (xixcore_ReadAtomic(&(AuxLotInfo->RefCount)) == 0) {
	
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("XixFsDeRefAuxLotLock: Delete VolumeId(0x%02x:%02x:%02x:%02x:%02x:%02x)  LotNum(%lld)\n",
			AuxLotInfo->VolumeId[10],
			AuxLotInfo->VolumeId[11],
			AuxLotInfo->VolumeId[12],
			AuxLotInfo->VolumeId[13],
			AuxLotInfo->VolumeId[14],
			AuxLotInfo->VolumeId[15],
			AuxLotInfo->LotNumber
			));

		xixcore_FreeMem(AuxLotInfo, XCTAG_AUXLOCKINFO);
	}
	

	return;
}



int
xixcore_call
xixcore_AuxLotLock(
	PXIXCORE_VCB		pVCB,
	xc_uint64			LotNumber,
	xc_uint32			bCheck,
	xc_uint32			bRetry
)
{
	int							RC = 0;
	PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo = NULL;
	XIXCORE_LISTENTRY			*pAuxLotLockList = NULL;
	xc_uint32						bNewEntry = 0;
	xc_uint32						Status = 0;
	XIXCORE_IRQL				oldIrql;

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixcore_AuxLotLock \n"));
	

	xixcore_AcquireSpinLock(xixcore_global.tmp_lot_lock_list_lock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));
	
	pAuxLotLockList = xixcore_global.tmp_lot_lock_list.next;

	while(pAuxLotLockList != &(xixcore_global.tmp_lot_lock_list))
	{
		AuxLotInfo = XIXCORE_CONTAINEROF(pAuxLotLockList, XIXCORE_AUXI_LOT_LOCK_INFO, AuxLink);
		
		if((memcmp(pVCB->VolumeId, AuxLotInfo->VolumeId, 16) == 0) && 
			(LotNumber == AuxLotInfo->LotNumber) )
		{
			xixcore_RefAuxLotLock(AuxLotInfo);
			break;
		}
		AuxLotInfo = NULL;
		pAuxLotLockList = pAuxLotLockList->next;
	}
	
	xixcore_ReleaseSpinLock(xixcore_global.tmp_lot_lock_list_lock, oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));

	
	if(AuxLotInfo == NULL){
		AuxLotInfo = (PXIXCORE_AUXI_LOT_LOCK_INFO)xixcore_AllocateMem(sizeof(XIXCORE_AUXI_LOT_LOCK_INFO), 0, XCTAG_AUXLOCKINFO);
		if(!AuxLotInfo){

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail :xixcore_AuxLotLockinsufficient resource  \n"));
			return XCCODE_UNSUCCESS;
		}

		memset(AuxLotInfo, 0, sizeof(XIXCORE_AUXI_LOT_LOCK_INFO));
		
		memcpy(AuxLotInfo->VolumeId, pVCB->VolumeId, 16);
		memcpy(AuxLotInfo->HostMac, pVCB->HostMac, 32);
		memcpy(AuxLotInfo->HostId, pVCB->HostId, 16);
		AuxLotInfo->LotNumber = LotNumber;
		AuxLotInfo->HasLock = INODE_FILE_LOCK_INVALID;
		xixcore_SetAtomic(&(AuxLotInfo->RefCount), 2);
	


		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("xixcore_AuxLotLock: New VolumeId(0x%02x:%02x:%02x:%02x:%02x:%02x)  LotNum(%lld)\n",
			AuxLotInfo->VolumeId[10],
			AuxLotInfo->VolumeId[11],
			AuxLotInfo->VolumeId[12],
			AuxLotInfo->VolumeId[13],
			AuxLotInfo->VolumeId[14],
			AuxLotInfo->VolumeId[15],
			AuxLotInfo->LotNumber
			));

		xixcore_InitializeListHead(&(AuxLotInfo->AuxLink));
		
		bNewEntry = 1;

		xixcore_AcquireSpinLock(xixcore_global.tmp_lot_lock_list_lock, &oldIrql);
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));
		
		xixcore_AddListEntryToHead(&(AuxLotInfo->AuxLink), &(xixcore_global.tmp_lot_lock_list));
		xixcore_ReleaseSpinLock(xixcore_global.tmp_lot_lock_list_lock, oldIrql);
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));

		
	}



	RC = xixcore_LotLock(pVCB,
					LotNumber,
					&Status,
					bCheck, 
					bRetry
					);



	AuxLotInfo->HasLock = Status;

	xixcore_DeRefAuxLotLock(AuxLotInfo);
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit xixcore_AuxLotLock %x\n", RC));
	
	return RC;	
}



int
xixcore_call
xixcore_AuxLotUnLock(
	PXIXCORE_VCB		pVCB,
	xc_uint64			LotNumber
)
{
	int							RC = 0;
	PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo = NULL;
	XIXCORE_LISTENTRY			*pAuxLotLockList = NULL;

	xc_uint32						Status = 0;
	XIXCORE_IRQL				oldIrql;

	//XifsdDebugTarget = DEBUG_TARGET_LOCK;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixcore_AuxLotUnLock \n"));

	
	
	xixcore_AcquireSpinLock(xixcore_global.tmp_lot_lock_list_lock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));

	pAuxLotLockList = xixcore_global.tmp_lot_lock_list.next;
	
	while(pAuxLotLockList != &(xixcore_global.tmp_lot_lock_list))
	{
		AuxLotInfo = XIXCORE_CONTAINEROF(pAuxLotLockList, XIXCORE_AUXI_LOT_LOCK_INFO, AuxLink);
		
		if((memcmp(pVCB->VolumeId, AuxLotInfo->VolumeId, 16) == 0) && 
			(LotNumber == AuxLotInfo->LotNumber) )
		{
			xixcore_RefAuxLotLock(AuxLotInfo);
			break;
		}

		AuxLotInfo = NULL;
		pAuxLotLockList = pAuxLotLockList->next;
	}
	xixcore_ReleaseSpinLock(xixcore_global.tmp_lot_lock_list_lock, oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));



	XIXCORE_ASSERT(AuxLotInfo);
	

	RC = xixcore_LotUnLock(pVCB,
						LotNumber,
						&Status
						);

	if(AuxLotInfo){
		AuxLotInfo->HasLock = INODE_FILE_LOCK_INVALID;
		xixcore_DeRefAuxLotLock(AuxLotInfo);
	}

	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit XixFsAuxLotUnLock  %x \n", RC));
	
	return RC;	
}



void
xixcore_call
xixcore_CleanUpAuxLotLockInfo(
		PXIXCORE_VCB		pVCB
)
{
	PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo = NULL;
	XIXCORE_LISTENTRY			*pAuxLotLockList = NULL;
	XIXCORE_IRQL				oldIrql;
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixcore_CleanUpAuxLockLockInfo \n"));


	

	xixcore_AcquireSpinLock(xixcore_global.tmp_lot_lock_list_lock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));

	pAuxLotLockList = xixcore_global.tmp_lot_lock_list.next;

	while(pAuxLotLockList != &(xixcore_global.tmp_lot_lock_list))
	{
		AuxLotInfo = XIXCORE_CONTAINEROF(pAuxLotLockList, XIXCORE_AUXI_LOT_LOCK_INFO, AuxLink);

		if(memcmp(pVCB->VolumeId, AuxLotInfo->VolumeId, 16) == 0)
		{
			pAuxLotLockList = pAuxLotLockList->next;
			xixcore_RemoveListEntry(&(AuxLotInfo->AuxLink));
			xixcore_DeRefAuxLotLock(AuxLotInfo);
		}else{
			pAuxLotLockList = pAuxLotLockList->next;
		}

	}
	xixcore_ReleaseSpinLock(xixcore_global.tmp_lot_lock_list_lock, oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit xixcore_CleanUpAuxLockLockInfo \n"));
}

void
xixcore_call
xixcore_CleanUpAllAuxLotLockInfo(
)
{
	PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo = NULL;
	XIXCORE_LISTENTRY			*pAuxLotLockList = NULL;
	XIXCORE_IRQL				oldIrql;
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixcore_CleanUpAllAuxLotLockInfo \n"));


	

	xixcore_AcquireSpinLock(xixcore_global.tmp_lot_lock_list_lock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));

	pAuxLotLockList = xixcore_global.tmp_lot_lock_list.next;

	while(pAuxLotLockList != &(xixcore_global.tmp_lot_lock_list))
	{
		AuxLotInfo = XIXCORE_CONTAINEROF(pAuxLotLockList, XIXCORE_AUXI_LOT_LOCK_INFO, AuxLink);

		pAuxLotLockList = pAuxLotLockList->next;
		xixcore_RemoveListEntry(&(AuxLotInfo->AuxLink));
		xixcore_DeRefAuxLotLock(AuxLotInfo);
	}
	xixcore_ReleaseSpinLock(xixcore_global.tmp_lot_lock_list_lock, oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(xixcore_global.tmp_lot_lock_list_lock))\n" ));
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Exit xixcore_CleanUpAllAuxLotLockInfo \n"));
}
