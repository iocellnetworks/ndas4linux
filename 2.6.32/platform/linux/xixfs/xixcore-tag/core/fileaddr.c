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


#include "xcsystem/debug.h"
#include "xcsystem/errinfo.h"
#include "xcsystem/system.h"
#include "xixcore/callback.h"
#include "xixcore/buffer.h"
#include "xixcore/ondisk.h"
#include "xixcore/volume.h"
#include "xixcore/file.h"
#include "xixcore/lotaddr.h"
#include "xixcore/fileaddr.h"
#include "internal_types.h"

#define MAX_NEW_LOT_COUNT 64


/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCFILEADDR"

typedef struct _XIX_ADDR_TEMP{
	xc_uint64 tmpAddressBuffer[MAX_NEW_LOT_COUNT];
	xc_uint64 PrevAddrBuffer[MAX_NEW_LOT_COUNT];
	xc_uint32 Index[MAX_NEW_LOT_COUNT];
}XIX_ADDR_TEMP, *PXIX_ADDR_TEMP;

int
xixcore_call
xixcore_GetAddressInfoForOffset(
	PXIXCORE_VCB			pVCB,
	PXIXCORE_FCB			pFCB,
	xc_uint64				Offset,
	PXIXCORE_BUFFER			Addr,
	xc_uint32				AddrSize,
	xc_uint32				* AddrSavedIndex,
	xc_int32				* Reason,
	PXIXCORE_IO_LOT_INFO	pAddress,
	xc_uint32				CanWait
)
{
	int				RC = 0;
	xc_uint32 			LotIndex = 0;
	xc_uint32			RealLotIndex = 0;
	xc_uint32			AddrIndex = 0;
	xc_uint32			LotSize = 0;
	//xc_uint32 			LotType = 0;
	//xc_uint32 			AllocatedAddrCount = 0;
	//xc_int32 			ReadAddrCount = 0;
	xc_uint32			DefaultSecPerLotAddr = (xc_uint32)(AddrSize/sizeof(xc_uint64));
	xc_uint64			*AddrLot = NULL;

#ifndef XIXCORE_DEBUG
	XIXCORE_UNREFERENCED_PARAMETER(AddrSize);
#endif


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Enter.\n"));
	
	XIXCORE_ASSERT_FCB(pFCB);
	XIXCORE_ASSERT_VCB(pVCB);

	XIXCORE_ASSERT(AddrSize >= pVCB->AddrLotSize);

	LotSize = pVCB->LotSize;



	if(pFCB->FCBType == FCB_TYPE_FILE){
		if(Offset >= (xc_uint64)pFCB->RealAllocationSize) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Error!! Offset(%lld) > pFCB->RealAllocationSize(%lld)\n", 
					Offset, pFCB->RealAllocationSize));

			*Reason =  XCREASON_ADDR_INVALID;
			return XCCODE_EFBIG;
		}
	}else{
		if(Offset >= (xc_uint64)pFCB->RealAllocationSize) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Error xixcore_GetAddressInfoForOffset Offset(%lld) > pFCB->RealAllocationSize(%lld)\n", 
					Offset, pFCB->RealAllocationSize));

			*Reason =  XCREASON_ADDR_INVALID;
			return XCCODE_EFBIG;
		}
	}


	
	XIXCORE_ASSERT((pFCB->FCBType == FCB_TYPE_FILE)|| (pFCB->FCBType == FCB_TYPE_DIR));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CHECK|DEBUG_TARGET_THREAD),
			("down rq 1((&pFCB->AddrLock)) pFCB(%p)\n", pFCB));
	xixcore_DecreaseSemaphore(pFCB->AddrLock);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CHECK|DEBUG_TARGET_THREAD),
			("down rq 1 end((&pFCB->AddrLock)) pFCB(%p)\n", pFCB));


	LotIndex = xixcore_GetIndexOfLogicalAddress( LotSize, Offset);
		
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
				("FCB(%lld) LotIndex(%d) Offset(%lld)\n",
				pFCB->LotNumber, LotIndex, Offset));
		
	DefaultSecPerLotAddr = (xc_uint32)(AddrSize/sizeof(xc_uint64));



	AddrIndex = (xc_uint32)(LotIndex / DefaultSecPerLotAddr);

	if(*AddrSavedIndex != AddrIndex){
		if(!CanWait) {
			xixcore_IncreaseSemaphore(pFCB->AddrLock);
			return XCCODE_UNSUCCESS;
		}
		

		if(pFCB->FCBType == FCB_TYPE_FILE){
			if(XIXCORE_TEST_FLAGS(pFCB->FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK)){
				RC = xixcore_RawWriteAddressOfFile(
								pVCB->XixcoreBlockDevice,
								pVCB->LotSize,
								pVCB->SectorSize,
								pVCB->SectorSizeBit,
								pFCB->LotNumber,
								pFCB->AddrLotNumber,
								pFCB->AddrLotSize,
								AddrSavedIndex, 
								*AddrSavedIndex,
								Addr,
								Reason
								);
				if (RC< 0 ){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail(0x%x) Reason(0x%x):xixcore_GetAddressInfoForOffsett:xixcore_RawWriteAddressOfFile\n",
							RC, Reason));
					
					goto error_out;
				}

				XIXCORE_CLEAR_FLAGS(pFCB->FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK);

			}
		}


		RC = xixcore_RawReadAddressOfFile(
								pVCB->XixcoreBlockDevice,
								pVCB->LotSize,
								pVCB->SectorSize,
								pVCB->SectorSizeBit,
								pFCB->LotNumber,
								pFCB->AddrLotNumber,
								pFCB->AddrLotSize,
								AddrSavedIndex,
								AddrIndex,
								Addr, 
								Reason
								);
										
		
		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL, 
				("fail(0x%x) Reason(0x%x) xixcore_GetAddressInfoForOffset : xixcore_RawReadAddressOfFile \n",RC, *Reason));

			goto error_out;
		}
	}


	AddrLot = (xc_uint64 *)xixcore_GetDataBuffer(Addr);
	
	RealLotIndex = LotIndex - (DefaultSecPerLotAddr* (xc_uint32)AddrIndex);

		
	pAddress->BeginningLotIndex = pFCB->LotNumber;
	pAddress->LogicalStartOffset = xixcore_GetLogicalStartAddrOfFile((xc_uint64)LotIndex,LotSize);
	
	if(LotIndex == 0){
		pAddress->LotTotalDataSize =  LotSize - XIDISK_FILE_HEADER_SIZE;
		pAddress->Flags = LOT_FLAG_BEGIN;
	}else{
		pAddress->LotTotalDataSize = LotSize- XIDISK_DATA_LOT_SIZE;
		pAddress->Flags = LOT_FLAG_BODY;
	}
	
	pAddress->LotIndex = AddrLot[RealLotIndex];

	if(pAddress->LotIndex < XIFSD_MAX_SYSTEM_RESERVED_LOT) {
		DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("error !!! bug!!!\n"));
		XIXCORE_ASSERT(0);
		*Reason =  XCREASON_ADDR_INVALID;
		RC = XCCODE_UNSUCCESS;
		goto error_out;

	}

	if(RealLotIndex  > 0){
		pAddress->PreviousLotIndex = AddrLot[RealLotIndex -1];	
	}else {
		pAddress->PreviousLotIndex = 0;
	}

	if(RealLotIndex < (DefaultSecPerLotAddr -1)){
		pAddress->NextLotIndex = AddrLot[RealLotIndex + 1];
	}else{
		pAddress->NextLotIndex = 0;
	}


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("ADDR INFO[FCB(%lld) RealAlloc(%lld) LotIndex(%d) Offset(%lld)] : LotIndex(%lld) Begin(%lld) LogAddr(%lld) PhyAddr(%lld) LotTotDataSize(%d)\n",
			pFCB->LotNumber,
			pFCB->RealAllocationSize,
			LotIndex,
			Offset,
			pAddress->LotIndex,
			pAddress->BeginningLotIndex, 
			pAddress->LogicalStartOffset, 
			pAddress->PhysicalAddress,
			pAddress->LotTotalDataSize));
	

	if(pAddress->LotIndex == 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Error xixcore_GetAddressInfoForOffset pAddress->LotIndex == 0 Req Offset(%lld) Real Lot Index(%d)\n"
				,Offset,RealLotIndex));

		XIXCORE_ASSERT(0);
		*Reason =  XCREASON_ADDR_INVALID;
		RC = XCCODE_UNSUCCESS;
		goto error_out;
	}

	pAddress->PhysicalAddress = xixcore_GetPhysicalAddrOfFile(
										pAddress->Flags, 
										LotSize,
										pAddress->LotIndex
										);

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("ADDR INFO[FCB(%lld)] : Begin(%lld) LogAddr(%lld) PhyAddr(%lld) LotTotDataSize(%d)\n",
			pFCB->LotNumber,
			pAddress->BeginningLotIndex, 
			pAddress->LogicalStartOffset, 
			pAddress->PhysicalAddress,
			pAddress->LotTotalDataSize));


error_out:
	
	xixcore_IncreaseSemaphore(pFCB->AddrLock);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("up((&pFCB->AddrLock)) pFCB(%p)\n", pFCB));

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Exit xixcore_GetAddressInfoForOffset Status(0x%x)\n", RC));	

	return RC;
	
}




int
xixcore_call
xixcore_AddNewLot(
	PXIXCORE_VCB 	pVCB, 
	PXIXCORE_FCB 	pFCB, 
	xc_uint32		RequestStatIndex, 
	xc_uint32		LotCount,
	xc_uint32		*AddedLotCount,
	PXIXCORE_BUFFER 	Addr,
	xc_uint32		AddrSize,
	xc_uint32		* AddrStartIndex
)
{
	int 				RC =0;
	xc_uint32			AddrIndex = 0;
	xc_uint32			RealLotIndex = 0;
	PXIX_ADDR_TEMP	AddrInfo = NULL;
	xc_uint32 i =0;
	xc_uint32			bGetLot = 0;
	PXIXCORE_BUFFER		Buff = NULL;
	xc_uint64			LastValidLotNumber = 0;
	xc_uint32			NewLotIndex = 0;
	xc_uint32			DefaultSecPerLotAddr = 0;
	xc_uint64			*AddrLot = NULL;
	int				Reason = 0;
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Enter xixcore_AddNewLot\n"));


	XIXCORE_ASSERT_VCB(pVCB);
	XIXCORE_ASSERT_FCB(pFCB);
	


	Buff = xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if(Buff == NULL){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixcore_AddNewLot:can't alloc buffer\n"));
		return XCCODE_ENOMEM;
	}



	AddrInfo = (PXIX_ADDR_TEMP)xixcore_AllocateMem(sizeof(XIX_ADDR_TEMP), 0, XCTAG_ADDRINFO);
	if(AddrInfo == NULL){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixcore_AddNewLot:can't alloc addr info\n"));
		
		xixcore_FreeBuffer(Buff);
		return XCCODE_ENOMEM;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CHECK|DEBUG_TARGET_THREAD),
			("down rq((&pFCB->AddrLock)) pFCB(%p)\n", pFCB));
	xixcore_DecreaseSemaphore(pFCB->AddrLock);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_CHECK|DEBUG_TARGET_THREAD),
			("down end((&pFCB->AddrLock)) pFCB(%p)\n", pFCB));


	memset(AddrInfo, 0, sizeof(XIX_ADDR_TEMP));

	


	// Adjust Lot Count
	if(LotCount > MAX_NEW_LOT_COUNT){
		LotCount = MAX_NEW_LOT_COUNT; 
	}	
	

		
	AddrLot = (xc_uint64 *)xixcore_GetDataBuffer(Addr);

	DefaultSecPerLotAddr = (xc_uint32)(AddrSize/sizeof(xc_uint64));



	AddrIndex = (xc_uint32)(RequestStatIndex / DefaultSecPerLotAddr);
	RealLotIndex = RequestStatIndex - (DefaultSecPerLotAddr*(xc_uint32)AddrIndex);
	NewLotIndex = RequestStatIndex + 1;
 
	
	


		// Check Current Request Index
		if (AddrIndex != *AddrStartIndex){
			if(pFCB->FCBType == FCB_TYPE_FILE){
				if(XIXCORE_TEST_FLAGS(pFCB->FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK)){
					RC = xixcore_RawWriteAddressOfFile(
									pVCB->XixcoreBlockDevice,
									pVCB->LotSize,
									pVCB->SectorSize,
									pVCB->SectorSizeBit,
									pFCB->LotNumber,
									pFCB->AddrLotNumber,
									pFCB->AddrLotSize,
									AddrStartIndex, 
									*AddrStartIndex,
									Addr,
									&Reason
									);
					if (RC< 0 ){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail(0x%x) Reason(0x%x):xixcore_AddNewLot:xixcore_RawWriteAddressOfFile\n",
								RC, Reason));
						
						goto error_out;
					}

					XIXCORE_CLEAR_FLAGS(pFCB->FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK);

				}
			}

			RC = xixcore_RawReadAddressOfFile(
							pVCB->XixcoreBlockDevice,
							pVCB->LotSize,
							pVCB->SectorSize,
							pVCB->SectorSizeBit,
							pFCB->LotNumber,
							pFCB->AddrLotNumber,
							pFCB->AddrLotSize,
							AddrStartIndex, 
							AddrIndex,
							Addr,
							&Reason
							);
						
		
			if (RC< 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) Reason(0x%x):xixcore_AddNewLot:xixcore_RawReadAddressOfFile\n",
						RC, Reason));
				
				goto error_out;
			}
		}

		if(RealLotIndex == (DefaultSecPerLotAddr-1))
		{
			LastValidLotNumber = AddrLot[RealLotIndex];

			if(pFCB->FCBType == FCB_TYPE_FILE){
				if(XIXCORE_TEST_FLAGS(pFCB->FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK)){
					RC = xixcore_RawWriteAddressOfFile(
									pVCB->XixcoreBlockDevice,
									pVCB->LotSize,
									pVCB->SectorSize,
									pVCB->SectorSizeBit,
									pFCB->LotNumber,
									pFCB->AddrLotNumber,
									pFCB->AddrLotSize,
									AddrStartIndex, 
									AddrIndex,
									Addr,
									&Reason
									);
					if (RC< 0 ){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail(0x%x) Reason(0x%x):xixcore_AddNewLot:xixcore_RawWriteAddressOfFile\n",
								RC, Reason));
						
						goto error_out;
					}

					XIXCORE_CLEAR_FLAGS(pFCB->FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK);

				}
			}



			AddrIndex++;


			RC = xixcore_RawReadAddressOfFile(
							pVCB->XixcoreBlockDevice,
							pVCB->LotSize,
							pVCB->SectorSize,
							pVCB->SectorSizeBit,
							pFCB->LotNumber,
							pFCB->AddrLotNumber,
							pFCB->AddrLotSize,
							AddrStartIndex, 
							AddrIndex,
							Addr,
							&Reason
							);
						
		
			if (RC< 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) Reason(0x%x):xixcore_AddNewLot:xixcore_RawReadAddressOfFile\n",
						RC, Reason));
				
				goto error_out;
			}

			RealLotIndex = 0;	

		}else{



			LastValidLotNumber = AddrLot[RealLotIndex];
			RealLotIndex ++;
		}


	
		for(i =0; i<LotCount; i++){
			AddrInfo->tmpAddressBuffer[i] = xixcore_AllocVCBLot(pVCB);
			
			if(AddrInfo->tmpAddressBuffer[i] == -1){
				bGetLot = 1;

				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("ERROR xixcore_AddNewLot:xixcore_AllocVCBLot Fail Insufficient resource\n"));

				RC = XCCODE_ENOSPC;
				goto error_out;
			}
			
			AddrInfo->Index[i] = NewLotIndex ++;
			
			if(i ==0){
				AddrInfo->PrevAddrBuffer[i] = LastValidLotNumber;
			}else{
				AddrInfo->PrevAddrBuffer[i] = AddrInfo->tmpAddressBuffer[i-1];
			}
		}

		bGetLot = 1;


		
		for(i = 0; i<LotCount; i++){
			AddrLot[RealLotIndex] = AddrInfo->tmpAddressBuffer[i];
			if(RealLotIndex == (DefaultSecPerLotAddr -1)){


				RC = xixcore_RawWriteAddressOfFile(
								pVCB->XixcoreBlockDevice,
								pVCB->LotSize,
								pVCB->SectorSize,
								pVCB->SectorSizeBit,
								pFCB->LotNumber,
								pFCB->AddrLotNumber,
								pFCB->AddrLotSize,
								AddrStartIndex, 
								AddrIndex,
								Addr,
								&Reason
								);
							
			
				if (RC< 0 ){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail(0x%x) Reason(0x%x):xixcore_AddNewLot:xixcore_RawWriteAddressOfFile\n",
							RC, Reason));
					
					goto error_out;
				}


				AddrIndex++;
				RealLotIndex = 0;


				RC = xixcore_RawReadAddressOfFile(
								pVCB->XixcoreBlockDevice,
								pVCB->LotSize,
								pVCB->SectorSize,
								pVCB->SectorSizeBit,
								pFCB->LotNumber,
								pFCB->AddrLotNumber,
								pFCB->AddrLotSize,
								AddrStartIndex, 
								AddrIndex,
								Addr,
								&Reason
								);
							
			
				if (RC< 0 ){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail(0x%x) Reason(0x%x):xixcore_AddNewLot:xixcore_RawReadAddressOfFile\n",
							RC, Reason));
					
					goto error_out;
				}

			}else{
				RealLotIndex++;
			}

			
		}


		if(pFCB->FCBType == FCB_TYPE_FILE){
			XIXCORE_SET_FLAGS(pFCB->FCBFlags,XIXCORE_FCB_MODIFIED_ADDR_BLOCK);
		}else{

			RC = xixcore_RawWriteAddressOfFile(
							pVCB->XixcoreBlockDevice,
							pVCB->LotSize,
							pVCB->SectorSize,
							pVCB->SectorSizeBit,
							pFCB->LotNumber,
							pFCB->AddrLotNumber,
							pFCB->AddrLotSize,
							AddrStartIndex, 
							AddrIndex,
							Addr,
							&Reason
							);
						
		
			if (RC< 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) Reason(0x%x):xixcore_AddNewLot:xixcore_RawWriteAddressOfFile\n",
						RC, Reason));
				
				goto error_out;
			}

		}

		for(i = 0; i<LotCount; i++){
			xixcore_ZeroBufferOffset(Buff);
			memset(xixcore_GetDataBuffer(Buff), 0,XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

			RC = xixcore_RawReadLotHeader(
									pVCB->XixcoreBlockDevice, 
									pVCB->LotSize,
									pVCB->SectorSize,
									pVCB->SectorSizeBit,
									AddrInfo->tmpAddressBuffer[i], 
									Buff,
									&Reason);
			

			if (RC< 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) Reason(0x%x):xixcore_AddNewLot:xixcore_RawWriteLotHeader\n",
						RC, Reason));
				if(RC == XCCODE_CRCERROR){
					xixcore_ZeroBufferOffset(Buff);
				}else{
					goto error_out;
				}

			}




			xixcore_InitializeCommonLotHeader(
						(PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBuffer(Buff),
						pVCB->VolumeLotSignature,
						((pFCB->FCBType == FCB_TYPE_FILE)? LOT_INFO_TYPE_FILE : LOT_INFO_TYPE_DIRECTORY),
						LOT_FLAG_BODY,
						AddrInfo->tmpAddressBuffer[i],
						pFCB->LotNumber,
						AddrInfo->PrevAddrBuffer[i],
						((i == (LotCount-1))?0:AddrInfo->tmpAddressBuffer[i+1]),
						xixcore_GetLogicalStartAddrOfFile((xc_uint64)(AddrInfo->Index[i]), pVCB->LotSize),
						XIDISK_DATA_LOT_SIZE,
						pVCB->LotSize - XIDISK_DATA_LOT_SIZE
						);

	

			RC = xixcore_RawWriteLotHeader(
									pVCB->XixcoreBlockDevice, 
									pVCB->LotSize,
									pVCB->SectorSize,
									pVCB->SectorSizeBit,
									AddrInfo->tmpAddressBuffer[i], 
									Buff,
									&Reason);
			

			if (RC< 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) Reason(0x%x):xixcore_AddNewLot:xixcore_RawWriteLotHeader\n",
						RC, Reason));
				
				goto error_out;
			}
		}
		


		*AddedLotCount = LotCount;

		//spin_lock(&(pFCB->FCBLock));
		pFCB->RealAllocationSize += ((pVCB->LotSize- XIDISK_DATA_LOT_SIZE)*LotCount);

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ADDRTRANS, 
			("Changed Size pFCB(%lld)  pFCB->RealAllocationSize(%lld)\n", pFCB->LotNumber, pFCB->RealAllocationSize));		


		
		if(pFCB->FCBType == FCB_TYPE_FILE){
			pFCB->AllocationSize = pFCB->RealAllocationSize;
			XIXCORE_SET_FLAGS(pFCB->FCBFlags, XIXCORE_FCB_MODIFIED_ALLOC_SIZE);
		}
		
		//spin_unlock(&(pFCB->FCBLock));
/*
			= GetAddressOfFileData(LOT_FLAG_BODY, pVCB->LotSize, (RequestStatIndex + LotCount))
										+ (pVCB->LotSize- XIDISK_DATA_LOT_SIZE);
*/
		DebugTrace(DEBUG_LEVEL_CRITICAL, DEBUG_TARGET_ALL, 
			("Changed Size pFCB(%lld)  pFCB->RealAllocationSize(%lld)\n", pFCB->LotNumber, pFCB->RealAllocationSize));		


		

		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("Modified flag pFCB(%lld) pFCB->FCBFlags(0x%ld)\n", pFCB->LotNumber, pFCB->FCBFlags));


		bGetLot = 0;
		RC = 0;

error_out:

		
		xixcore_IncreaseSemaphore(pFCB->AddrLock);
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("up((&pFCB->AddrLock)) pFCB(%p)\n", pFCB));
		

	

		if(RC < 0 ){
			if(bGetLot == 1){
				for(i =0; i<LotCount; i++){
					
					if(AddrInfo->tmpAddressBuffer[i] != -1){
						
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Release Lot pFCB(%lld) LotAddress(%lld)\n", pFCB->LotNumber, AddrInfo->tmpAddressBuffer[i]));
						
						xixcore_FreeVCBLot(pVCB, AddrInfo->tmpAddressBuffer[i]);


					}else{
						break;
					}
				}				
			}
		}

		xixcore_FreeBuffer(Buff);
		xixcore_FreeMem(AddrInfo, XCTAG_ADDRINFO);
	


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Exit xixcore_AddNewLot Status(0x%x)\n", RC));	
	return RC;
}



int
xixcore_call
xixcore_DeleteFileLotAddress(
	IN PXIXCORE_VCB pVCB,
	IN PXIXCORE_FCB pFCB
)
{
	int		RC = 0;
	xc_uint64 * 	pAddrmapInfo = NULL;
	xc_uint32	i = 0;
	xc_uint32	j = 0;
	xc_uint32	maxCount = 	xixcore_GetLotCountOfFileOffset(pVCB->LotSize, (pFCB->RealAllocationSize -1));
	xc_uint32	maxLot = 0; //(xc_uint32)((maxCount + 63)/64);
	xc_uint32	AddrCount = 0;
	xc_uint32	DefaultSecPerLotAddr = 0;
	//xc_uint32	AddrStartSecIndex = 0;
	xc_uint32	AddrBufferSize = 0;
	xc_uint8	*AddrBuffer = NULL;
	xc_uint32	bDone = 0;	
	int		Reason = 0;


	XIXCORE_ASSERT_VCB(pVCB);
	XIXCORE_ASSERT_FCB(pFCB);

	AddrBufferSize = pVCB->AddrLotSize;
	DefaultSecPerLotAddr = (xc_uint32)(pVCB->AddrLotSize/sizeof(xc_uint64));



	maxLot = (xc_uint32)((maxCount + DefaultSecPerLotAddr -1)/DefaultSecPerLotAddr);


	AddrBuffer = xixcore_GetDataBuffer(pFCB->AddrLot);

	XIXCORE_ASSERT(AddrBuffer);
	
	for(i = 0; i<maxLot; i++){


		RC = xixcore_RawReadAddressOfFile(
					pVCB->XixcoreBlockDevice, 
					pVCB->LotSize, 
					pVCB->SectorSize, 
					pVCB->SectorSizeBit,
					pFCB->LotNumber, 
					pFCB->AddrLotNumber,
					pFCB->AddrLotSize,
					&pFCB->AddrStartSecIndex, 
					i, 
					pFCB->AddrLot, 
					&Reason);
										

		if( RC < 0){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL, 
				("Error xixcore_DeleteFileLotAddress :xixcore_RawReadAddressOfFile Status(0x%x)\n",RC));

			goto error_out;
		}

		

		pAddrmapInfo = (xc_uint64 *)AddrBuffer;
		for(j = 0; j< DefaultSecPerLotAddr; j++){
			
			if(AddrCount >= maxCount){
				bDone = 1;
				break;
			}

			if(pAddrmapInfo[j] != 0){
				xixcore_FreeVCBLot(pVCB, pAddrmapInfo[j]);
				AddrCount = ((i) *  DefaultSecPerLotAddr + (j+1));
			}else{
				bDone = 1;
				break;
			}

		}
		
		if(bDone == 1){
			break;
		}

	}

	if(pFCB->AddrLotNumber != 0){
		xixcore_FreeVCBLot(pVCB, pFCB->AddrLotNumber);
	}



error_out:	
		
	return RC;
	
}
