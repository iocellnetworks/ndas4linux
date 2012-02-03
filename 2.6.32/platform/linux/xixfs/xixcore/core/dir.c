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
#include "xixcore/layouts.h"
#include "xixcore/ondisk.h"
#include "xixcore/volume.h"
#include "xixcore/fileaddr.h"
#include "xixcore/lotaddr.h"
#include "xixcore/lotlock.h"
#include "xixcore/dir.h"
#include "xixcore/md5.h"
#include "endiansafe.h"
#include "internal_types.h"


/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCDIR"

xc_le16 XixfsUnicodeSelfArray[] = { XIXCORE_CPU2LE16('.') };
xc_le16 XixfsUnicodeParentArray[] = { XIXCORE_CPU2LE16('.'), XIXCORE_CPU2LE16('.') };


int
xixcore_call
xixcore_RawReadDirEntry(
	PXIXCORE_VCB 	pVCB,
	PXIXCORE_FCB 	pFCB,
	xc_uint32		Index,
	PXIXCORE_BUFFER 	buffer,
	xc_uint32		bufferSize,
	PXIXCORE_BUFFER	AddrBuffer,
	xc_uint32		AddrBufferSize,
	xc_uint32		*AddrStartSecIndex
)
{
	int					RC = 0;
	xc_int64				Margin = 0;
	xc_uint64				DataOffset = 0;
	xc_uint64				Offset;
	XIXCORE_IO_LOT_INFO		AddressInfo;
	PXIDISK_CHILD_INFORMATION pChildInfo = NULL;
	xc_uint32				LotSize = 0;
	xc_int32				Reason = 0;
	xc_uint32				BlockSize = 0;
	xc_sector_t				ReqSec = 0;

#ifndef XIXCORE_DEBUG
	XIXCORE_UNREFERENCED_PARAMETER(bufferSize);
#endif


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_RawReadDirEntry .\n"));

	XIXCORE_ASSERT_VCB(pVCB);
	XIXCORE_ASSERT_FCB(pFCB);
	LotSize = pVCB->LotSize;

	XIXCORE_ASSERT(buffer);	
	xixcore_ZeroBufferOffset(buffer);

	XIXCORE_ASSERT(xixcore_GetBufferSize(buffer) >=XIDISK_DUP_CHILD_RECORD_SIZE);
	XIXCORE_ASSERT(AddrBufferSize >= pVCB->AddrLotSize);

	
	DataOffset = GetChildOffset(Index);
	BlockSize = XIDISK_DUP_CHILD_RECORD_SIZE;
	

	if(DataOffset + BlockSize > (xc_uint64)pFCB->RealAllocationSize){
		
		xc_uint64 LastOffset = 0;
		xc_uint64 RequestStartOffset = 0;
		xc_uint32 EndLotIndex = 0;
		xc_uint32 CurentEndIndex = 0;
		xc_uint32 LotCount = 0;
		xc_uint32 AllocatedLotCount = 0;

		if((pVCB->IsVolumeWriteProtected) || (pFCB->HasLock != INODE_FILE_LOCK_HAS)) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail Too Big Allocation Size : DataOffset (%lld) BlockSize(%d) Realallocationsize(%lld)\n",
					DataOffset, BlockSize, pFCB->RealAllocationSize));

			return XCCODE_EINVAL;
		}

		RequestStartOffset = DataOffset;
		LastOffset = DataOffset + BlockSize;
		

		CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pVCB->LotSize, (pFCB->RealAllocationSize-1));
		EndLotIndex = xixcore_GetIndexOfLogicalAddress(pVCB->LotSize, LastOffset);

		if(EndLotIndex > CurentEndIndex){
			LotCount = EndLotIndex - CurentEndIndex;

		
			RC = xixcore_AddNewLot(pVCB, 
									pFCB, 
									CurentEndIndex, 
									LotCount, 
									&AllocatedLotCount, 
									AddrBuffer,
									AddrBufferSize,
									AddrStartSecIndex
									);
			
			if( RC < 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail xixcore_AddNewLot FCB LotNumber(%lld) CurrentEndIndex(%d), Request Lot(%d) AllocatLength(%lld) .\n",
						pFCB->LotNumber, CurentEndIndex, LotCount, pFCB->RealAllocationSize));
				
				
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail Too Big Allocation Size : DataOffset (%lld) BlockSize(%d) Realallocationsize(%lld)\n",
						DataOffset, BlockSize, pFCB->RealAllocationSize));

				return RC;

			}

		}

		
	}

	memset(&AddressInfo, 0,sizeof(XIXCORE_IO_LOT_INFO));


	
	RC = xixcore_GetAddressInfoForOffset(
							pVCB,
							pFCB, 
							DataOffset,  
							AddrBuffer,
							AddrBufferSize, 
							AddrStartSecIndex, 
							&Reason, 
							&AddressInfo,
							1
							);


	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_RawReadDirEntry:  xixcore_GetAddressInfoForOffset\n",
				RC, Reason));
		
		if(Reason != 0){
			//	print reason
		}
		//Raise exception
		goto error_out;
	}

	XIXCORE_ASSERT(!(AddressInfo.LogicalStartOffset % SECTORSIZE_512));
	XIXCORE_ASSERT(!(AddressInfo.LotTotalDataSize % SECTORSIZE_512));
	XIXCORE_ASSERT(DataOffset >= AddressInfo.LogicalStartOffset);

	Margin = DataOffset - AddressInfo.LogicalStartOffset;
	// Check Request and forward
	if((DataOffset  + BlockSize) <=(AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize)){

		
		
		Margin = DataOffset - AddressInfo.LogicalStartOffset;
		Offset = 
			xixcore_GetAddrOfFileData(AddressInfo.Flags, pVCB->LotSize, AddressInfo.LotIndex) + Margin;
		//Make sigle request and forward request to lower driver


		XIXCORE_ASSERT(IS_512_SECTOR(Offset));
		ReqSec = (xc_sector_t)SECTOR_ALIGNED_COUNT(pVCB->SectorSizeBit,Offset);


		RC =xixcore_DeviceIoSync(
				pVCB->XixcoreBlockDevice,
				ReqSec,
				BlockSize,
				pVCB->SectorSize,
				pVCB->SectorSizeBit,
				buffer,
				XIXCORE_READ,
				&Reason
				);
				
		

		if ( RC < 0 )
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x):xixcore_RawReadDirEntry  xixcore_DeviceIoSync \n",
					RC, Reason));
			goto error_out;
		}

		xixcore_ZeroBufferOffset(buffer);
		RC = checkMd5DirEntry(buffer, &pChildInfo);
		
		if ( RC < 0 )
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) xixcore_RawReadDirEntry  checkMd5DirEntry \n",
					RC));
			goto error_out;
		}		
		
		readChangeDirEntry(pChildInfo);
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("Exit!!! xixcore_RawReadDirEntry Margin(%lld) Offset(%lld) ReqSec(%lld)  .\n",
				Margin, Offset, ReqSec));
#else
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("Exit!!! xixcore_RawReadDirEntry Margin(%lld) Offset(%lld) ReqSec(%ld)  .\n",
				Margin, Offset, ReqSec));
#endif
		RC =  0 ;
		goto error_out;
	}

	if((DataOffset + BlockSize) > (AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize)){	
		xc_int64				RemainingDataSize = 0;
		xc_int32				TransferDataSize = 0;
		//char *				TransferBuffer = 0;
		
		
		Margin = DataOffset - AddressInfo.LogicalStartOffset;
		RemainingDataSize = BlockSize;
		//TransferBuffer = buffer;
		xixcore_ZeroBufferOffset(buffer);

		


		
		while(RemainingDataSize){
			
			TransferDataSize = (xc_uint32)(AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize - DataOffset);
			Offset = 
				xixcore_GetAddrOfFileData(AddressInfo.Flags,pVCB->LotSize, AddressInfo.LotIndex) + Margin;
			Margin = 0;

			XIXCORE_ASSERT(IS_512_SECTOR(TransferDataSize));
			XIXCORE_ASSERT(IS_512_SECTOR(Offset));
			ReqSec = (xc_sector_t)SECTOR_ALIGNED_COUNT(pVCB->SectorSizeBit,Offset);

			
			RC =xixcore_DeviceIoSync(
					pVCB->XixcoreBlockDevice,
					ReqSec,
					TransferDataSize,
					pVCB->SectorSize,
					pVCB->SectorSizeBit,
					buffer,
					XIXCORE_READ,
					&Reason
					);

			if ( RC < 0 )
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) Reason(0x%x):xixcore_RawReadDirEntry  xixcore_DeviceIoSync \n",
						RC, Reason));
				goto error_out;
			}


			DataOffset += TransferDataSize;
			xixcore_IncreaseBufferOffset(buffer, TransferDataSize);
			RemainingDataSize -= TransferDataSize;				


			RC = xixcore_GetAddressInfoForOffset(
										pVCB,
										pFCB, 
										DataOffset,  
										AddrBuffer,
										AddrBufferSize, 
										AddrStartSecIndex, 
										&Reason, 
										&AddressInfo,
										1
										);

			if( RC < 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail(0x%x) Reason(0x%x) xixcore_RawReadDirEntry:  xixcore_GetAddressInfoForOffset\n",
							RC, Reason));
				
				if(Reason != 0){
					//	print reason
				}
				//Raise exception
				goto error_out;
			}

			XIXCORE_ASSERT(DataOffset == AddressInfo.LogicalStartOffset);
		}

		xixcore_ZeroBufferOffset(buffer);
		RC = checkMd5DirEntry(buffer, &pChildInfo);
		if ( RC < 0 )
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) :xixcore_RawReadDirEntry  checkMd5DirEntry \n",
					RC));
			goto error_out;
		}

		readChangeDirEntry(pChildInfo);
		
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("Exit2!!! xixcore_RawReadDirEntry Margin(%lld) Offset(%lld) ReqSec(%lld)  .\n",
				Margin, Offset, ReqSec));
#else
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("Exit2!!! xixcore_RawReadDirEntry Margin(%lld) Offset(%lld) ReqSec(%ld)  .\n",
				Margin, Offset, ReqSec));
#endif
		
		RC = 0;
	}


error_out:	

	return RC;
}


int
xixcore_call
xixcore_RawWriteDirEntry(
	PXIXCORE_VCB 	pVCB,
	PXIXCORE_FCB 	pFCB,
	xc_uint32		Index,
	PXIXCORE_BUFFER 	buffer,
	xc_uint32		bufferSize,
	PXIXCORE_BUFFER	AddrBuffer,
	xc_uint32		AddrBufferSize,
	xc_uint32		*AddrStartSecIndex
)
{
	int					RC = 0;
	//xc_int64				RequestOffset = 0;
	//xc_int64				LotAddressOffset = 0;
	xc_int64				Margin = 0;
	xc_uint64				DataOffset = 0;
	xc_uint64				Offset;
	XIXCORE_IO_LOT_INFO		AddressInfo;
	PXIDISK_CHILD_INFORMATION pChildInfo = NULL;
	xc_uint32				LotSize = 0;
	xc_int32				Reason = 0;
	xc_uint32				BlockSize = 0;
	xc_sector_t				ReqSec = 0;

	
#ifndef XIXCORE_DEBUG
	XIXCORE_UNREFERENCED_PARAMETER(bufferSize);
#endif

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter XixFsRawWriteDirEntry .\n"));

	XIXCORE_ASSERT_VCB(pVCB);
	XIXCORE_ASSERT_FCB(pFCB);
	LotSize = pVCB->LotSize;


	
	XIXCORE_ASSERT(buffer);	
	XIXCORE_ASSERT(xixcore_GetBufferSizeWithOffset(buffer) >=XIDISK_CHILD_RECORD_SIZE);
	XIXCORE_ASSERT(AddrBufferSize >= pVCB->AddrLotSize);

	
	pChildInfo = (PXIDISK_CHILD_INFORMATION)xixcore_GetDataBufferWithOffset(buffer);
	pChildInfo->SequenceNum++;
	if(pChildInfo->SequenceNum % 2){
		DataOffset = GetChildOffset(Index) + XIDISK_CHILD_RECORD_SIZE;
	}else{
		DataOffset = GetChildOffset(Index);
	}
	BlockSize = XIDISK_CHILD_RECORD_SIZE;
	

	if(DataOffset + BlockSize > (xc_uint64)pFCB->RealAllocationSize){
		xc_uint64 LastOffset = 0;
		xc_uint64 RequestStartOffset = 0;
		xc_uint32 EndLotIndex = 0;
		xc_uint32 CurentEndIndex = 0;
		//xc_uint32 RequestStatIndex = 0;
		xc_uint32 LotCount = 0;
		xc_uint32 AllocatedLotCount = 0;
		
		RequestStartOffset = DataOffset;
		LastOffset = DataOffset + BlockSize;
		

		CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pVCB->LotSize, (pFCB->RealAllocationSize-1));
		EndLotIndex = xixcore_GetIndexOfLogicalAddress(pVCB->LotSize, LastOffset);

		if(EndLotIndex > CurentEndIndex){
			LotCount = EndLotIndex - CurentEndIndex;

		
			RC = xixcore_AddNewLot(pVCB, 
									pFCB, 
									CurentEndIndex, 
									LotCount, 
									&AllocatedLotCount, 
									AddrBuffer,
									AddrBufferSize,
									AddrStartSecIndex
									);
			
			if( RC < 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("Fail xixcore_AddNewLot FCB LotNumber(%lld) CurrentEndIndex(%d), Request Lot(%d) AllocatLength(%lld) .\n",
						pFCB->LotNumber, CurentEndIndex, LotCount, pFCB->RealAllocationSize));
				
				
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail Too Big Allocation Size : DataOffset (%lld) BlockSize(%d) Realallocationsize(%lld)\n",
						DataOffset, BlockSize, pFCB->RealAllocationSize));

				return RC;

			}

		}
	}

	memset(&AddressInfo, 0, sizeof(XIXCORE_IO_LOT_INFO));


	writeChangeDirEntry(pChildInfo);
	
	makeMd5DirEntry(pChildInfo);

	RC = xixcore_GetAddressInfoForOffset(
							pVCB,
							pFCB, 
							DataOffset, 
							AddrBuffer,
							AddrBufferSize, 
							AddrStartSecIndex,
							&Reason, 
							&AddressInfo,
							1
							);


	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) xixcore_RawWriteDirEntry\n", RC));
		if(Reason != 0){
			//	print reason
		}
		//Raise exception
		goto error_out;
	}

	XIXCORE_ASSERT(!(AddressInfo.LogicalStartOffset % SECTORSIZE_512));
	XIXCORE_ASSERT(!(AddressInfo.LotTotalDataSize % SECTORSIZE_512));
	XIXCORE_ASSERT(DataOffset >= AddressInfo.LogicalStartOffset);

	Margin = DataOffset - AddressInfo.LogicalStartOffset;
	// Check Request and forward
	if((DataOffset  + BlockSize) <=(AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize)){
		
		Margin = DataOffset - AddressInfo.LogicalStartOffset;
		Offset= 
			xixcore_GetAddrOfFileData(AddressInfo.Flags, pVCB->LotSize, AddressInfo.LotIndex) + Margin;
		//Make sigle request and forward request to lower driver


		XIXCORE_ASSERT(IS_512_SECTOR(Offset));
		ReqSec = (xc_sector_t)SECTOR_ALIGNED_COUNT(pVCB->SectorSizeBit,Offset);
		
		RC =xixcore_DeviceIoSync(
				pVCB->XixcoreBlockDevice,
				ReqSec,
				BlockSize,
				pVCB->SectorSize,
				pVCB->SectorSizeBit,
				buffer,
				XIXCORE_WRITE,
				&Reason
				);

		if ( RC < 0 )
		{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x):xixcore_RawWriteDirEntry  xixcore_DeviceIoSync \n",
					RC, Reason));
			goto error_out;
		}



	
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("Exit xixcore_RawWriteDirEntry .\n"));
		RC =  0 ;
		goto error_out;
	}

	if((DataOffset + BlockSize) > (AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize)){	
		xc_int64				RemainingDataSize = 0;
		xc_int32				TransferDataSize = 0;
		//char *				TransferBuffer = 0;
		
		
		Margin = DataOffset - AddressInfo.LogicalStartOffset;
		RemainingDataSize = BlockSize;
		//TransferBuffer = buffer;
		xixcore_ZeroBufferOffset(buffer);

		while(RemainingDataSize){
			TransferDataSize = (xc_uint32)(AddressInfo.LogicalStartOffset + AddressInfo.LotTotalDataSize - DataOffset);
			Offset = 
				xixcore_GetAddrOfFileData(AddressInfo.Flags,pVCB->LotSize, AddressInfo.LotIndex) + Margin;
			Margin = 0;

			XIXCORE_ASSERT(IS_512_SECTOR(TransferDataSize));
			XIXCORE_ASSERT(IS_512_SECTOR(Offset));
			ReqSec = (xc_sector_t)SECTOR_ALIGNED_COUNT(pVCB->SectorSizeBit,Offset);
		
			RC =xixcore_DeviceIoSync(
					pVCB->XixcoreBlockDevice,
					ReqSec,
					TransferDataSize,
					pVCB->SectorSize,
					pVCB->SectorSizeBit,
					buffer,
					XIXCORE_WRITE,
					&Reason
					);

			if ( RC < 0 )
			{
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) Reason(0x%x):xixcore_RawWriteDirEntry  xixcore_DeviceIoSync \n",
						RC, Reason));
				goto error_out;
			}


			DataOffset += TransferDataSize;
			xixcore_IncreaseBufferOffset(buffer, TransferDataSize);
			RemainingDataSize -= TransferDataSize;				



			RC = xixcore_GetAddressInfoForOffset(
										pVCB,
										pFCB, 
										DataOffset,  
										AddrBuffer,
										AddrBufferSize, 
										AddrStartSecIndex, 
										&Reason, 
										&AddressInfo,
										1
										);

			if( RC < 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail(0x%x) Reason(0x%x) xixcore_RawWriteDirEntry:  xixcore_GetAddressInfoForOffset\n",
							RC, Reason));
				
				if(Reason != 0){
					//	print reason
				}
				//Raise exception
				goto error_out;
			}

			XIXCORE_ASSERT(DataOffset == AddressInfo.LogicalStartOffset);
		}
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), ("Exit XixFsRawWriteDirEntry .\n"));
		RC = 0;
	}


error_out:
	
	readChangeDirEntry(pChildInfo);
	
	return RC;	
}

// Added by ILGU HONG

void
xixcore_call
xixcore_RefChildCacheEntry(
	PXIXCORE_CHILD_CACHE_ENTRY	pChildCacheEntry
)
{
	XIXCORE_ASSERT(xixcore_ReadAtomic(&(pChildCacheEntry->RefCount)) > 0 );
	xixcore_IncreaseAtomic(&(pChildCacheEntry->RefCount));
	return;
}

void
xixcore_call
xixcore_DeRefChildCacheEntry(
	PXIXCORE_CHILD_CACHE_ENTRY	pChildCacheEntry
)
{
	XIXCORE_ASSERT(xixcore_ReadAtomic(&(pChildCacheEntry->RefCount)) > 0 );
	xixcore_DecreaseAtomic(&(pChildCacheEntry->RefCount));
	
		
	if (xixcore_ReadAtomic(&(pChildCacheEntry->RefCount)) == 0) {
	
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("Dealloc ChildCache Entry  Parent Dir %Ild ChildIndex %ld entry%p\n", 
				pChildCacheEntry->ChildCacheInfo.ParentDirLotIndex, 
				pChildCacheEntry->ChildCacheInfo.ChildIndex, 
				pChildCacheEntry)); 

		xixcore_FreeMem(pChildCacheEntry, XCTAG_CHILDCACHE);
	}
}


void
xixcore_call
xixcore_RemoveLRUCacheEntry(
	IN PXIXCORE_VCB 	pVCB,
	IN xc_uint32		count
)
{
	XIXCORE_LISTENTRY			*pChildCacheEntryList = NULL;
	PXIXCORE_CHILD_CACHE_ENTRY	pChildCacheEntry = NULL;
	XIXCORE_IRQL				oldIrql;
	xc_uint32					i = 0;

	XIXCORE_ASSERT(pVCB);

	xixcore_AcquireSpinLock(pVCB->childEntryCacheSpinlock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_RemoveLRUCacheEntry: xixcore_AcquireSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	
	
	pChildCacheEntryList = pVCB->LRUchildEntryCacheHeader.prev;

	for(i = 0; i< count; i++){

		if(pChildCacheEntryList == &(pVCB->LRUchildEntryCacheHeader)) break;
		
		pChildCacheEntry = XIXCORE_CONTAINEROF(pChildCacheEntryList, XIXCORE_CHILD_CACHE_ENTRY, LRUChildCacheLink);
		pChildCacheEntryList = pChildCacheEntryList->prev;
		
		xixcore_RemoveListEntry(&(pChildCacheEntry->LRUChildCacheLink));
		xixcore_RemoveListEntry(&(pChildCacheEntry->HASHChildCacheLink));
		
		pVCB->childCacheCount--;

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("xixcore_RemoveLRUCacheEntry ChildCache Entry  Parent Dir %lld ChildIndex %ld entry%p Total Child %ld\n", 
				pChildCacheEntry->ChildCacheInfo.ParentDirLotIndex, 
				pChildCacheEntry->ChildCacheInfo.ChildIndex, 
				pChildCacheEntry, 
				pVCB->childCacheCount));

		xixcore_DeRefChildCacheEntry(pChildCacheEntry);
	

	}

	xixcore_ReleaseSpinLock(pVCB->childEntryCacheSpinlock, oldIrql);

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_RemoveLRUCacheEntry: xixcore_ReleaseSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	return;
}



int
xixcore_call
xixcore_CreateChildCacheEntry(
	IN PXIXCORE_VCB 	pVCB,
	IN xc_uint64		ParentDirLotIndex,
	IN PXIDISK_CHILD_INFORMATION	pChildEntry,
	IN PXIDISK_FILE_INFO			pFileInfo
)
{
	xc_uint32 size = 0;
	PXIXCORE_CHILD_CACHE_ENTRY	pChildCacheEntry = NULL;
	XIXCORE_IRQL					oldIrql;
	xc_uint32							startHash = 0;
	xc_uint64							tmpParentDirLotIndex = 0;

	
	XIXCORE_ASSERT(pVCB);
	XIXCORE_ASSERT(pChildEntry);
	XIXCORE_ASSERT(pFileInfo);
	XIXCORE_ASSERT(pFileInfo->LotIndex == pChildEntry->StartLotIndex);
	XIXCORE_ASSERT( XIXCORE_TEST_FLAGS(pFileInfo->Type, pChildEntry->Type));
	
	if(pChildEntry->NameSize > XIFS_MAX_NAME_LEN){
		size = XIFS_MAX_NAME_LEN;
	}else{
		size = pChildEntry->NameSize;
	}

	size = sizeof(XIXCORE_CHILD_CACHE_ENTRY) + size -1;

	pChildCacheEntry = (PXIXCORE_CHILD_CACHE_ENTRY)xixcore_AllocateMem(size , 0, XCTAG_CHILDCACHE);
	if(!pChildCacheEntry){

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail :xixcore_CreateChildCacheEntry resource  \n"));
		return XCCODE_UNSUCCESS;
	}

	memset(pChildCacheEntry, 0, size);
	pChildCacheEntry->ChildCacheInfo.ParentDirLotIndex = ParentDirLotIndex;
	pChildCacheEntry->ChildCacheInfo.Type = pChildEntry->Type;
	pChildCacheEntry->ChildCacheInfo.State = pChildEntry->State;
	pChildCacheEntry->ChildCacheInfo.NameSize = pChildEntry->NameSize;
	pChildCacheEntry->ChildCacheInfo.ChildIndex = pChildEntry->ChildIndex;
	pChildCacheEntry->ChildCacheInfo.StartLotIndex = pChildEntry->StartLotIndex;
	pChildCacheEntry->ChildCacheInfo.HashValue = pChildEntry->HashValue;

	pChildCacheEntry->ChildCacheInfo.FileAttribute = pFileInfo->FileAttribute;
	pChildCacheEntry->ChildCacheInfo.FileSize = pFileInfo->FileSize;
	pChildCacheEntry->ChildCacheInfo.AllocationSize = pFileInfo->AllocationSize;	
	pChildCacheEntry->ChildCacheInfo.Modified_time = pFileInfo->Modified_time;;
	pChildCacheEntry->ChildCacheInfo.Change_time = pFileInfo->Change_time;
	pChildCacheEntry->ChildCacheInfo.Access_time = pFileInfo->Access_time;
	pChildCacheEntry->ChildCacheInfo.Create_time = pFileInfo->Create_time;



	memcpy(&(pChildCacheEntry->ChildCacheInfo.Name[0]), pChildEntry->Name, pChildEntry->NameSize);

	xixcore_SetAtomic(&(pChildCacheEntry->RefCount), 1);

	pChildCacheEntry->RefTime = xixcore_GetCurrentTime64();

	xixcore_InitializeListHead(&(pChildCacheEntry->LRUChildCacheLink));
	xixcore_InitializeListHead(&(pChildCacheEntry->HASHChildCacheLink));
		
	tmpParentDirLotIndex = ParentDirLotIndex;
	startHash = (xc_uint32)xixcore_Divide64(&tmpParentDirLotIndex, 10);

	if(pVCB->childCacheCount >= XIXCORE_MAX_CHILD_CACHE){
		xixcore_RemoveLRUCacheEntry(pVCB, XIXCORE_DEFAULT_REMOVE);
	}

	xixcore_AcquireSpinLock(pVCB->childEntryCacheSpinlock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_AcquireSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	xixcore_AddListEntryToHead(&(pChildCacheEntry->LRUChildCacheLink), &(pVCB->LRUchildEntryCacheHeader));
	xixcore_AddListEntryToHead(&(pChildCacheEntry->HASHChildCacheLink), &(pVCB->HASHchildEntryCacheHeader[startHash]));
	pVCB->childCacheCount ++;
	xixcore_ReleaseSpinLock(pVCB->childEntryCacheSpinlock, oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
				("xixcore_ReleaseSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("Create ChildCache Entry FCB %Ild hash %ld ChildIndex %ld entry%p total child count%ld\n", 
			ParentDirLotIndex, startHash, 
			pChildCacheEntry->ChildCacheInfo.ChildIndex, 
			pChildCacheEntry, 
			pVCB->childCacheCount)); 

	return XCCODE_SUCCESS;
}



int
xixcore_call
xixcore_FindChildCacheEntry(
	IN PXIXCORE_VCB 	pVCB,
	IN xc_uint64		ParentDirLotIndex,
	IN xc_uint32		ChildIndex,
	OUT PXIXCORE_CHILD_CACHE_ENTRY *ppChildCacheEntry
)
{

	XIXCORE_LISTENTRY				*pChildCacheEntryList = NULL;
	PXIXCORE_CHILD_CACHE_ENTRY	pChildCacheEntry = NULL;
	XIXCORE_IRQL					oldIrql;
	int								RC = XCCODE_UNSUCCESS;
	xc_uint32							startHash = 0;
	xc_uint64							tmpParentDirLotIndex = 0;
	XIXCORE_ASSERT(pVCB);

	*ppChildCacheEntry = NULL;

	tmpParentDirLotIndex = ParentDirLotIndex;
	startHash = (xc_uint32)xixcore_Divide64(&tmpParentDirLotIndex, 10);

	xixcore_AcquireSpinLock(pVCB->childEntryCacheSpinlock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_FindChildCacheEntry :xixcore_AcquireSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	

	//xixcore_DebugPrintf("request ChildCache Entry FCB 0x%I64x Hash %ld ChildIndex %ld\n", ParentDirLotIndex, startHash, ChildIndex); 

	pChildCacheEntryList = pVCB->HASHchildEntryCacheHeader[startHash].next;
	while(pChildCacheEntryList != &(pVCB->HASHchildEntryCacheHeader[startHash]))
	{
		pChildCacheEntry = XIXCORE_CONTAINEROF(pChildCacheEntryList, XIXCORE_CHILD_CACHE_ENTRY, HASHChildCacheLink);
		
		if( (pChildCacheEntry->ChildCacheInfo.ParentDirLotIndex == ParentDirLotIndex) 
			&& (pChildCacheEntry->ChildCacheInfo.ChildIndex == ChildIndex))
		{
			xixcore_RemoveListEntry(&(pChildCacheEntry->LRUChildCacheLink));
			xixcore_InitializeListHead(&(pChildCacheEntry->LRUChildCacheLink));
			xixcore_AddListEntryToHead(&(pChildCacheEntry->LRUChildCacheLink), &(pVCB->LRUchildEntryCacheHeader));
			xixcore_RefChildCacheEntry(pChildCacheEntry);
			pChildCacheEntry->RefTime = xixcore_GetCurrentTime64();

			*ppChildCacheEntry = pChildCacheEntry;
			
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
				("find ChildCache Entry FCB %lld ChildIndex %ld\n", 
					pChildCacheEntry->ChildCacheInfo.ParentDirLotIndex, 
					pChildCacheEntry->ChildCacheInfo.ChildIndex)); 
			
			RC = XCCODE_SUCCESS;
			break;
		}

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("Miss ChildCache Entry FCB %lld ChildIndex %ld\n", 
				pChildCacheEntry->ChildCacheInfo.ParentDirLotIndex, 
				pChildCacheEntry->ChildCacheInfo.ChildIndex)); 
		
		pChildCacheEntry = NULL;
		pChildCacheEntryList = pChildCacheEntryList->next;
	}


	xixcore_ReleaseSpinLock(pVCB->childEntryCacheSpinlock, oldIrql);
	//xixcore_DebugPrintf("xixcore_FindChildCacheEntry end\n ");
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_FindChildCacheEntry: xixcore_ReleaseSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	
	return RC;
}




int
xixcore_call
xixcore_UpdateChildCacheEntry(
	IN PXIXCORE_VCB 	pVCB,
	IN xc_uint64		ParentDirLotIndex,
	IN xc_uint64		FileLotIndex,
	IN xc_uint32		FileAttribute,
	IN xc_uint64		FileSize,
	IN xc_uint64		AllocationSize,	
	IN xc_uint64		Modified_time,
	IN xc_uint64		Change_time,
	IN xc_uint64		Access_time,
	IN xc_uint64		Create_time
)
{

	XIXCORE_LISTENTRY				*pChildCacheEntryList = NULL;
	PXIXCORE_CHILD_CACHE_ENTRY	pChildCacheEntry = NULL;
	XIXCORE_IRQL					oldIrql;
	int								RC = XCCODE_UNSUCCESS;
	xc_uint32							startHash = 0;
	xc_uint64							tmpParentDirLotIndex = 0;
	XIXCORE_ASSERT(pVCB);

	tmpParentDirLotIndex = ParentDirLotIndex;
	startHash = (xc_uint32)xixcore_Divide64(&tmpParentDirLotIndex, 10);

	xixcore_AcquireSpinLock(pVCB->childEntryCacheSpinlock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_FindChildCacheEntry :xixcore_AcquireSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	

	//xixcore_DebugPrintf("request ChildCache Entry FCB 0x%I64x Hash %ld ChildIndex %ld\n", ParentDirLotIndex, startHash, ChildIndex); 

	pChildCacheEntryList = pVCB->HASHchildEntryCacheHeader[startHash].next;
	while(pChildCacheEntryList != &(pVCB->HASHchildEntryCacheHeader[startHash]))
	{
		pChildCacheEntry = XIXCORE_CONTAINEROF(pChildCacheEntryList, XIXCORE_CHILD_CACHE_ENTRY, HASHChildCacheLink);
		
		if( (pChildCacheEntry->ChildCacheInfo.ParentDirLotIndex == ParentDirLotIndex) 
			&& (pChildCacheEntry->ChildCacheInfo.StartLotIndex == FileLotIndex))
		{
			xixcore_RemoveListEntry(&(pChildCacheEntry->LRUChildCacheLink));
			xixcore_InitializeListHead(&(pChildCacheEntry->LRUChildCacheLink));
			xixcore_AddListEntryToHead(&(pChildCacheEntry->LRUChildCacheLink), &(pVCB->LRUchildEntryCacheHeader));

			pChildCacheEntry->ChildCacheInfo.FileAttribute = FileAttribute;
			pChildCacheEntry->ChildCacheInfo.FileSize = FileSize;
			pChildCacheEntry->ChildCacheInfo.AllocationSize = AllocationSize;
			pChildCacheEntry->ChildCacheInfo.Modified_time = Modified_time;
			pChildCacheEntry->ChildCacheInfo.Change_time = Change_time;
			pChildCacheEntry->ChildCacheInfo.Access_time = Access_time;
			pChildCacheEntry->ChildCacheInfo.Create_time = Create_time;
			pChildCacheEntry->RefTime = xixcore_GetCurrentTime64();

			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
				("find ChildCache Entry FCB %lld ChildIndex %ld\n", 
					pChildCacheEntry->ChildCacheInfo.ParentDirLotIndex, 
					pChildCacheEntry->ChildCacheInfo.ChildIndex)); 
			
			RC = XCCODE_SUCCESS;
			break;
		}

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			("Miss ChildCache Entry FCB %lld ChildIndex %ld\n", 
				pChildCacheEntry->ChildCacheInfo.ParentDirLotIndex, 
				pChildCacheEntry->ChildCacheInfo.ChildIndex)); 
		
		pChildCacheEntry = NULL;
		pChildCacheEntryList = pChildCacheEntryList->next;
	}


	xixcore_ReleaseSpinLock(pVCB->childEntryCacheSpinlock, oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_FindChildCacheEntry: xixcore_ReleaseSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	
	return RC;
}





void
xixcore_call
xixcore_DeleteSpecialChildCacheEntry(
	IN PXIXCORE_VCB 	pVCB,
	IN xc_uint64		ParentDirLotIndex,
	IN xc_uint32		ChildIndex
)
{
	XIXCORE_LISTENTRY				*pChildCacheEntryList = NULL;
	PXIXCORE_CHILD_CACHE_ENTRY	pChildCacheEntry = NULL;
	XIXCORE_IRQL					oldIrql;
	xc_uint32							startHash = 0;
	xc_uint64							tmpParentDirLotIndex = 0;

	XIXCORE_ASSERT(pVCB);

	tmpParentDirLotIndex = ParentDirLotIndex;
	startHash = (xc_uint32)xixcore_Divide64(&tmpParentDirLotIndex , 10);

	xixcore_AcquireSpinLock(pVCB->childEntryCacheSpinlock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_DeleteChildCacheEntry: xixcore_AcquireSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	
	pChildCacheEntryList = pVCB->HASHchildEntryCacheHeader[startHash].next;
	while(pChildCacheEntryList != &(pVCB->HASHchildEntryCacheHeader[startHash]))
	{
		pChildCacheEntry = XIXCORE_CONTAINEROF(pChildCacheEntryList, XIXCORE_CHILD_CACHE_ENTRY, HASHChildCacheLink);
		
		if( (pChildCacheEntry->ChildCacheInfo.ParentDirLotIndex == ParentDirLotIndex) 
			&& (pChildCacheEntry->ChildCacheInfo.ChildIndex == ChildIndex) )
		{
			
			pChildCacheEntryList = pChildCacheEntryList->next;

			xixcore_RemoveListEntry(&(pChildCacheEntry->LRUChildCacheLink));
			xixcore_RemoveListEntry(&(pChildCacheEntry->HASHChildCacheLink));
			pVCB->childCacheCount --;

			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
				("delete ChildCache Entry FCB %lld Hash %ld ChildIndex %ld total child count\n", 
					ParentDirLotIndex, 
					startHash, 
					ChildIndex, 
					pVCB->childCacheCount));

			xixcore_DeRefChildCacheEntry(pChildCacheEntry);
	
			break;
		}
		pChildCacheEntry = NULL;
		pChildCacheEntryList = pChildCacheEntryList->next;
	}


	xixcore_ReleaseSpinLock(pVCB->childEntryCacheSpinlock, oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_DeleteChildCacheEntry: xixcore_ReleaseSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	return;
}




void
xixcore_call
xixcore_DeleteALLChildCacheEntry(
	IN PXIXCORE_VCB 	pVCB
)
{
	XIXCORE_LISTENTRY			*pChildCacheEntryList = NULL;
	PXIXCORE_CHILD_CACHE_ENTRY	pChildCacheEntry = NULL;
	XIXCORE_IRQL				oldIrql;

	XIXCORE_ASSERT(pVCB);

	xixcore_AcquireSpinLock(pVCB->childEntryCacheSpinlock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_DeleteALLChildCacheEntry: xixcore_AcquireSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("delete all ChildCache Entry\n"));

	pChildCacheEntryList = pVCB->LRUchildEntryCacheHeader.next;
	while(pChildCacheEntryList != &(pVCB->LRUchildEntryCacheHeader))
	{
		pChildCacheEntry = XIXCORE_CONTAINEROF(pChildCacheEntryList, XIXCORE_CHILD_CACHE_ENTRY, LRUChildCacheLink);
		pChildCacheEntryList = pChildCacheEntryList->next;
		xixcore_RemoveListEntry(&(pChildCacheEntry->LRUChildCacheLink));
		xixcore_RemoveListEntry(&(pChildCacheEntry->HASHChildCacheLink));
		xixcore_DeRefChildCacheEntry(pChildCacheEntry);
	}

	pVCB->childCacheCount = 0;
	xixcore_ReleaseSpinLock(pVCB->childEntryCacheSpinlock, oldIrql);

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		("xixcore_DeleteALLChildCacheEntry: xixcore_ReleaseSpinLock : pVCB->childEntryCacheSpinlock\n" ));
	return;
}


// Added by ILGU HONG END

int
xixcore_call
xixcore_InitializeDirContext(
	PXIXCORE_VCB		pVCB,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext
)
{
	xc_uint32		Size = 0;
	int			RC = 0;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_InitializeDirContext .\n"));
	

	if(DirContext->ChildEntryBuff == NULL) {

	
		DirContext->ChildEntryBuff= xixcore_AllocateBuffer(XIDISK_DUP_CHILD_RECORD_SIZE);
		if(DirContext->ChildEntryBuff == NULL){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("fail: xixcore_InitializeDirContext:can't  Alloc ChildEntry.\n"));
			RC = XCCODE_ENOMEM;	
			goto error_out;
		}
		
	}

	Size = pVCB->AddrLotSize;

	
	DirContext->AddrMapSize = Size;
	DirContext->AddrIndexNumber = 0;

	if(DirContext->AddrMapBuff == NULL) {
	
	
		DirContext->AddrMapBuff= xixcore_AllocateBuffer(Size);

		if(DirContext->AddrMapBuff == NULL){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("fail: xixcore_InitializeDirContext:can't  Alloc addr buff.\n"));
		
			RC = XCCODE_ENOMEM;
			goto error_out;
		}
		
		DirContext->AddrMap = xixcore_GetDataBuffer(DirContext->AddrMapBuff);

	}

	if(DirContext->ChildBitMap == NULL) {
		DirContext->ChildBitMap = xixcore_AllocateMem(128, 0, XCTAG_CHILDBITMAP);
		if(DirContext->ChildBitMap == NULL){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("fail: xixcore_InitializeDirContext:can't  Alloc child bitmap.\n"));
		
			RC = XCCODE_ENOMEM;
			goto error_out;
		}
	}


// Addedd by ILGU HONG 12082006
	if(DirContext->ChildHashTable == NULL) {
		
		DirContext->ChildHashTable = xixcore_AllocateBuffer(XIDISK_DUP_HASH_VALUE_TABLE_SIZE);

		if(DirContext->ChildHashTable == NULL){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("fail: xixcore_InitializeDirContext:can't  Alloc child hash table.\n"));
		
			RC = XCCODE_ENOMEM;
			goto error_out;
			
		}
		
	}
// Addedd by ILGU HONG 12082006 END


	RC = 0;


error_out:
	
	if( RC < 0 ){
		if(DirContext->ChildEntryBuff){
			xixcore_FreeBuffer(DirContext->ChildEntryBuff);
			DirContext->ChildEntryBuff= NULL;
			
		}

		if(DirContext->AddrMapBuff){
			xixcore_FreeBuffer(DirContext->AddrMapBuff);
			DirContext->AddrMapBuff= NULL;
			DirContext->AddrMap = NULL;
		}

		if(DirContext->ChildBitMap){
			xixcore_FreeMem(DirContext->ChildBitMap, XCTAG_CHILDBITMAP);
			DirContext->ChildBitMap = NULL;
		}

// Addedd by ILGU HONG 12082006
		if(DirContext->ChildHashTable){
			xixcore_FreeBuffer(DirContext->ChildHashTable);
			DirContext->ChildHashTable= NULL;
		}

// Addedd by ILGU HONG 12082006 END

		
	}



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("Exit xixcore_InitializeDirContext (0x%x) .\n",
			RC));
	return RC;

}




void
xixcore_call
xixcore_CleanupDirContext(
    PXIXCORE_DIR_EMUL_CONTEXT DirContext
)
{

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_CleanupDirContext .\n"));


	if(DirContext->ChildEntryBuff){
		xixcore_FreeBuffer(DirContext->ChildEntryBuff);
		DirContext->ChildEntryBuff= NULL;
		
	}

	if(DirContext->AddrMapBuff){
		xixcore_FreeBuffer(DirContext->AddrMapBuff);
		DirContext->AddrMapBuff= NULL;
		DirContext->AddrMap = NULL;
	}

	if(DirContext->ChildBitMap){
		xixcore_FreeMem(DirContext->ChildBitMap, XCTAG_CHILDBITMAP);
		DirContext->ChildBitMap = NULL;
	}


// Addedd by ILGU HONG 12082006
	if(DirContext->ChildHashTable){
		xixcore_FreeBuffer(DirContext->ChildHashTable);
		DirContext->ChildHashTable= NULL;
	}

// Addedd by ILGU HONG 12082006 END

	memset(DirContext, 0, sizeof(XIXCORE_DIR_EMUL_CONTEXT));
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Exit xixcore_CleanupDirContext .\n"));
}



int
xixcore_call
xixcore_LookupInitialDirEntry(
	PXIXCORE_VCB	pVCB,
	PXIXCORE_FCB 	pFCB,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext,
	xc_uint32 InitIndex
)
{
	int			RC = 0;
//	xc_uint32		i = 0;
//	xc_uint64		*addr = NULL;
	PXIXCORE_BUFFER	xbuf = NULL;
    	int			Reason = 0;
	PXIDISK_DIR_INFO pFileInfo = NULL;	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_LookupInitialDirEntry .\n"));
    	

    //
    //  Check inputs.
    //

	XIXCORE_ASSERT_FCB(pFCB );
	XIXCORE_ASSERT_VCB(pVCB);
    
	DirContext->RealDirIndex = 0;
	DirContext->VirtualDirIndex = InitIndex;

	if(DirContext->VirtualDirIndex > 1){
		DirContext->RealDirIndex = -1;
	}

	DirContext->LotNumber = pFCB->LotNumber;
	DirContext->pVCB = pVCB; 
	DirContext->pFCB = pFCB;
	
	xbuf = xixcore_AllocateBuffer(XIDISK_DUP_DIR_INFO_SIZE);
	if(xbuf == NULL){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("fail:  xixcore_LookupInitialDirEntry: can't  alloc xbuf\n"));

		return XCCODE_ENOMEM;
	}



	

		
	RC = xixcore_RawReadAddressOfFile(
					DirContext->pVCB->XixcoreBlockDevice,
					DirContext->pVCB->LotSize,
					DirContext->pVCB->SectorSize,
					DirContext->pVCB->SectorSizeBit,
					DirContext->pFCB->LotNumber,
					DirContext->pFCB->AddrLotNumber,
					DirContext->pFCB->AddrLotSize,
					&(DirContext->AddrIndexNumber),
					DirContext->AddrIndexNumber,
					DirContext->AddrMapBuff,
					&Reason
					);			
						
		
		
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) : xixcore_LookupInitialDirEntry:  xixcore_RawReadAddressOfFile\n", RC, Reason));
		
		goto error_out;
	}




//		addr = (xc_uint64 *)DirContext->AddrMap;

//		for(i = 0; i< 10; i++)
//				DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_INFO), 
//						("Read AddreeBCB Address[%ld](%lld).\n",i,addr[i]));

	

	xixcore_ZeroBufferOffset(xbuf);
	memset(xixcore_GetDataBuffer(xbuf), 0, XIDISK_DUP_DIR_INFO_SIZE);

	RC = xixcore_RawReadFileHeader(
				DirContext->pVCB->XixcoreBlockDevice,
				DirContext->pVCB->LotSize,
				DirContext->pVCB->SectorSize,
				DirContext->pVCB->SectorSizeBit,
				DirContext->pFCB->LotNumber,
				xbuf,
				&Reason
				);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) : xixcore_LookupInitialDirEntry:  xixcore_RawReadFileHeader\n", RC, Reason));
		
		goto error_out;
	}
		
	pFileInfo = (PXIDISK_DIR_INFO)xixcore_GetDataBufferWithOffset(xbuf);
	memcpy(DirContext->ChildBitMap,  pFileInfo->ChildMap, 128);


//		for(i = 0; i< 10; i++)
//				DebugTrace(0, (DEBUG_TRACE_CREATE| DEBUG_TRACE_INFO), 
//						("Read ChildBitMap bitmap[%ld](0x%02x).\n",i,DirContext->ChildBitMap[i]));

		
	xixcore_ZeroBufferOffset(DirContext->ChildHashTable);	
	memset(xixcore_GetDataBuffer(DirContext->ChildHashTable), 0, XIDISK_DUP_HASH_VALUE_TABLE_SIZE );

// Addedd by ILGU HONG 12082006
	RC = xixcore_RawReadDirEntryHashValueTable(
				DirContext->pVCB->XixcoreBlockDevice,
				DirContext->pVCB->LotSize,
				DirContext->pVCB->SectorSize,
				DirContext->pVCB->SectorSizeBit,
				DirContext->pFCB->LotNumber,
				DirContext->ChildHashTable,
				&Reason
				);

	if( RC < 0 ){
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) : xixcore_LookupInitialDirEntry:  xixcore_RawReadDirEntryHashValueTable\n", RC, Reason));
		
		goto error_out;
	}
// Addedd by ILGU HONG 12082006 END
error_out:


	if(xbuf) {
		xixcore_FreeBuffer(xbuf);
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Exit xixcore_LookupInitialDirEntry Status(0x%x).\n",
			RC));

	return RC;
 
}

/*
return value 
	XCCODE_SUCCESS --> find
	XCCODE_UNSUCCESS --> error
	XCCODE_ENOENT --> no more file
*/

int
xixcore_call
xixcore_UpdateDirNames(
	PXIXCORE_DIR_EMUL_CONTEXT DirContext
)
{
	int				RC = XCCODE_SUCCESS;
	xc_uint32			Size = 0;
	xc_int64			NextRealIndex = 0;
	PXIDISK_CHILD_INFORMATION Child = NULL;
	PXIXCORE_CHILD_CACHE_ENTRY ChildCacheEntry = NULL;
	PXIDISK_HASH_VALUE_TABLE	pChildHashTable = NULL;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_UpdateDirNames .\n"));

	//
	//  Handle the case of the self directory entry.
	//

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("SearCh Dir(%lld) of FCB(%lld)\n", 
					DirContext->VirtualDirIndex, DirContext->pFCB->LotNumber));

	if (DirContext->VirtualDirIndex == 0) {
		DirContext->ChildNameLength = sizeof(XixfsUnicodeSelfArray);
		DirContext->ChildName = (xc_uint8 *)XixfsUnicodeSelfArray;
		
		DirContext->SearchedVirtualDirIndex = 0;
		DirContext->SearchedRealDirIndex = 0;
		DirContext->VirtualDirIndex = 1;
		DirContext->RealDirIndex = -1;
		DirContext->LotNumber = DirContext->pFCB->LotNumber;
		DirContext->FileType = DirContext->pFCB->FCBType;
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("SearCh Dir vitural Index(0) of FCB(%lld)\n",DirContext->pFCB->LotNumber ));
		DirContext->IsChildCache = 0;
		DirContext->IsVirtual =	1;
		return XCCODE_SUCCESS;
	}else if ( DirContext->VirtualDirIndex == 1){
		DirContext->ChildNameLength = sizeof(XixfsUnicodeParentArray);
		DirContext->ChildName = (xc_uint8 *)XixfsUnicodeParentArray;

		DirContext->SearchedVirtualDirIndex = 1;
		DirContext->SearchedRealDirIndex = 0;		
		DirContext->VirtualDirIndex = 2;
		DirContext->RealDirIndex = -1;
		DirContext->LotNumber = DirContext->pFCB->ParentLotNumber;
		DirContext->FileType = FCB_TYPE_DIR;
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("SearCh Dir vitural Index(1) of FCB(%lld)\n",DirContext->pFCB->LotNumber));
		DirContext->IsChildCache = 0;
		DirContext->IsVirtual =	1;
		return XCCODE_SUCCESS;
	}else {

	
		DirContext->IsVirtual =	0;
		NextRealIndex = DirContext->RealDirIndex;
		pChildHashTable = xixcore_GetDataBufferWithOffset(DirContext->ChildHashTable);	
		
		while(1) 
		{	
			DirContext->IsVirtual =	0;
			DirContext->IsChildCache = 0;
			NextRealIndex = (xc_uint64)xixcore_FindSetBit(1024, NextRealIndex, DirContext->ChildBitMap);
			if(NextRealIndex == 1024){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Can't find Dir entry\n"));
				RC =  XCCODE_ENOENT;
				goto error_out;
			}
				
			DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
						("SearCh Dir RealIndex(%lld) of FCB(%lld)\n", 
						NextRealIndex, DirContext->pFCB->LotNumber));
				

			RC = xixcore_FindChildCacheEntry(DirContext->pVCB, DirContext->pFCB->LotNumber, (xc_uint32)NextRealIndex, &ChildCacheEntry);
			
			if( RC < 0 ) {
do_real_read:

				xixcore_ZeroBufferOffset(DirContext->ChildEntryBuff);
				memset(xixcore_GetDataBuffer(DirContext->ChildEntryBuff),0,XIDISK_DUP_CHILD_RECORD_SIZE );

				RC = xixcore_RawReadDirEntry(
									DirContext->pVCB,
									DirContext->pFCB,
									(xc_uint32)NextRealIndex,
									DirContext->ChildEntryBuff,
									XIDISK_CHILD_RECORD_SIZE,
									DirContext->AddrMapBuff,
									DirContext->AddrMapSize,
									&(DirContext->AddrIndexNumber)
									);


				if( RC < 0 ){
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("Fail(0x%x)xixcore_UpdateDirNames: xixcore_RawReadDirEntry\n", RC));

					if(RC == XCCODE_CRCERROR){
						continue;
					}else{
						goto error_out;
					}
				}

				Child = (PXIDISK_CHILD_INFORMATION)xixcore_GetDataBufferWithOffset(DirContext->ChildEntryBuff);

				if(pChildHashTable->EntryHashVal[NextRealIndex] != Child->HashValue){
					continue;
				}

				if(Child->State == XIFS_FD_STATE_DELETED){
					continue;
				}else{

					DirContext->Type = Child->Type;
					DirContext->State = Child->State;						
					DirContext->NameSize = Child->NameSize;
					DirContext->ChildIndex = Child->ChildIndex;
					DirContext->StartLotIndex = Child->StartLotIndex;
					DirContext->HashValue = Child->HashValue;
					DirContext->Name = Child->Name;
					DirContext->IsChildCache = 0;
					break;
				}

			}else{
				PXIXCORE_CHILD_CACHE_INFO ChildInfo = NULL;
				if(pChildHashTable->EntryHashVal[NextRealIndex] != ChildCacheEntry->ChildCacheInfo.HashValue){
					xixcore_DeleteSpecialChildCacheEntry(DirContext->pVCB, ChildCacheEntry->ChildCacheInfo.ParentDirLotIndex, (xc_uint32)NextRealIndex);
					xixcore_DeRefChildCacheEntry(ChildCacheEntry);
					goto do_real_read;
				}

				xixcore_ZeroBufferOffset(DirContext->ChildEntryBuff);
				memset(xixcore_GetDataBuffer(DirContext->ChildEntryBuff),0,XIDISK_DUP_CHILD_RECORD_SIZE );
				
				Size = sizeof(XIXCORE_CHILD_CACHE_INFO) + ChildCacheEntry->ChildCacheInfo.NameSize -1;
				memcpy(xixcore_GetDataBuffer(DirContext->ChildEntryBuff),&ChildCacheEntry->ChildCacheInfo, Size);
				
				ChildInfo = (PXIXCORE_CHILD_CACHE_INFO)xixcore_GetDataBuffer(DirContext->ChildEntryBuff);

				DirContext->Type = ChildInfo->Type;
				DirContext->State = ChildInfo->State;						
				DirContext->NameSize = ChildInfo->NameSize;
				DirContext->ChildIndex = ChildInfo->ChildIndex;
				DirContext->StartLotIndex = ChildInfo->StartLotIndex;
				DirContext->HashValue = ChildInfo->HashValue;
				DirContext->Name = &(ChildInfo->Name[0]);
				DirContext->IsChildCache = 1;
				xixcore_DeRefChildCacheEntry(ChildCacheEntry);
				break;
			}
		}

		//Update Dir Context
		
		DirContext->SearchedVirtualDirIndex = NextRealIndex + 2;
		DirContext->SearchedRealDirIndex = NextRealIndex;
		DirContext->RealDirIndex = NextRealIndex;
		DirContext->VirtualDirIndex = NextRealIndex + 2;
		DirContext->LotNumber = DirContext->StartLotIndex;
		
		Size = DirContext->NameSize;
		if(Size){
			if(Size > XIFS_MAX_NAME_LEN){
				Size = XIFS_MAX_NAME_LEN;
			}
		}
		DirContext->ChildNameLength =  Size;
		DirContext->ChildName = DirContext->Name;

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("SearCh Dir RealIndex(%lld)   LotNumber(%lld)  ChildType(%d), ChildNamelen(%d) ChildIndex(%d)\n", 
				DirContext->RealDirIndex, 
				DirContext->LotNumber, 
				DirContext->Type, 
				DirContext->NameSize, 
				DirContext->ChildIndex
				));

		if(XIXCORE_TEST_FLAGS(DirContext->Type , XIFS_FD_TYPE_DIRECTORY)){
			DirContext->FileType = FCB_TYPE_DIR;
		}else{
			DirContext->FileType = FCB_TYPE_FILE;
		}

		RC = XCCODE_SUCCESS;
	}
		
error_out:
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("Exit xixcore_UpdateDirNames Status (0x%x) .\n", RC));
	
    return RC;
}




int
xixcore_call
xixcore_LookupSetFileIndex(
	PXIXCORE_DIR_EMUL_CONTEXT DirContext,
	xc_uint64 InitialIndex
)
{
	int				RC = 0;
	//xc_uint64			SearchIndex = InitialIndex;


	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
		("Enter xixcore_LookupInitialFileIndex .\n"));


	DirContext->VirtualDirIndex = InitialIndex;
	DirContext->RealDirIndex = -1;
	if(DirContext->VirtualDirIndex > 1)
	{
		DirContext->RealDirIndex = DirContext->VirtualDirIndex - 3;
	}

	
	RC = xixcore_UpdateDirNames( DirContext);


	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail (0x%x)xixcore_LookupInitialFileIndex: xixcore_UpdateDirNames .\n", RC));
		
		return RC;
	}

	if(DirContext->SearchedVirtualDirIndex != InitialIndex){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("SearCh Dir InitialIndex(%lld) and SearchedDirIndex(%lld)\n", 
					InitialIndex, DirContext->SearchedVirtualDirIndex));
			
		return XCCODE_EINVAL;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit xixcore_LookupInitialFileIndex .\n"));
	return XCCODE_SUCCESS;
}



int
xixcore_call
xixcore_FindDirEntryByLotNumber(
    PXIXCORE_VCB	pVCB,
    PXIXCORE_FCB 	pFCB,
    xc_uint64	LotNumber,
    PXIXCORE_DIR_EMUL_CONTEXT DirContext,
    xc_uint64	*EntryIndex
)
{
	int							RC = 0;
	
	 



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_FindDirEntryByLotNumberr .\n"));

	//
	//  Check inputs.
	//

	XIXCORE_ASSERT_VCB( pVCB );
	XIXCORE_ASSERT_FCB( pFCB );

	//
	//  Go get the first entry.
	//

	RC = xixcore_LookupInitialDirEntry( 
								pVCB,
								pFCB,
								DirContext,
								2 
								);



	if( RC< 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x) xixcore_FindDirEntryByLotNumber: xixcore_LookupInitialDirEntry\n", RC));
		return RC;
	}


	//
	//  Now loop looking for a good match.
	//
    
	while(1) {

		RC = xixcore_UpdateDirNames( DirContext );
		    
	    
		if( RC< 0){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR(0x%x) xixcore_FindDirEntryByLotNumber: xixcore_UpdateDirNames\n", RC));
			break;
		}

		
		if (DirContext->StartLotIndex == LotNumber){
			*EntryIndex = DirContext->SearchedRealDirIndex;
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("Exit xixcore_FindDirEntryByLotNumber .\n"));
			return XCCODE_SUCCESS;
		}

	} 
    //
    //  No match was found.
    //
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Fail Exit xixcore_FindDirEntryByLotNumber .\n"));

	return RC;
}


// Added by ILGU HONG 12082006 


XIXCORE_INLINE 
xc_uint32
xixcore_partial_file_name_hash(
	xc_uint32 c, 
	xc_uint32 prevhash
)
{
	return (prevhash + (c << 4) + (c >> 4)) * 11;
}


XIXCORE_INLINE 
xc_uint32
xixcore_file_name_hash(const unsigned char *name, unsigned int len)
{
	xc_uint32 hash = 0;
	while (len--)
		hash = xixcore_partial_file_name_hash(*name++, hash);
	return hash;
}


int 
xixcore_call
xixcore_Gethashvalueforfile(	
	xc_uint8		*Name,
	xc_uint32		NameLength,
	xc_uint16		*Hashval
)
{

	int				RC = 0;
	xc_uint8			*UpName = NULL;
	xc_int32			UpNameLength = 0;
	
	xc_uint32			hash = 0;

	XIXCORE_ASSERT(Name);
	XIXCORE_ASSERT(NameLength > 0);
	

	UpName = xixcore_AllocateLargeMem(NameLength,0,XCTAG_UPCASENAME);
	
	if(UpName == NULL) {
		RC = XCCODE_ENOMEM;
	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		(" FAIL ALLOCATION FILE NAME\n"));	
		return RC;
	}


	UpNameLength = NameLength;
	
	memset(UpName,0,UpNameLength);
	memcpy(UpName, Name, UpNameLength);

	xixcore_UpcaseUcsName(
					(xc_le16 *)UpName,
					(xc_uint32)(UpNameLength/2),
					xixcore_global.xixcore_case_table, 
					xixcore_global.xixcore_case_table_length
					);



	hash = xixcore_file_name_hash((xc_uint8 *)UpName, (xc_uint32)UpNameLength);

	*Hashval = (xc_uint16)( (hash  % 1024) + 1);
	RC = 0;

	if(UpName) {
		xixcore_FreeLargeMem(UpName, XCTAG_UPCASENAME);
	}


	DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
		("EXIT(%d) xixcore_hash value(%d) .\n", RC, *Hashval));	


	return RC;
	
}

int
xixcore_call
xixcore_FindDirEntry (
	PXIXCORE_VCB	pVCB,
	PXIXCORE_FCB 	pFCB,
	xc_uint8		*Name,
	xc_uint32		NameLength,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext,
	xc_uint64					*EntryIndex,
	xc_uint32					bIgnoreCase
    )
{
	int		RC = 0;
	xc_uint16	HashVal = 0;
	xc_int64			NextIndex = -1;
	PXIDISK_CHILD_INFORMATION Child = NULL;
	PXIDISK_HASH_VALUE_TABLE pDirHashTable = NULL;
	PXIXCORE_CHILD_CACHE_ENTRY ChildCacheEntry = NULL;
	xc_uint32			Size = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_FindDirEntry .\n"));

	//
	//  Check inputs.
	//

	
	XIXCORE_ASSERT_VCB(pVCB );
	XIXCORE_ASSERT_FCB(pFCB );

	//
	//  Go get the first entry.
	//

	//MatchName = DirContext->ChildName;


	RC = xixcore_LookupInitialDirEntry( 
							pVCB,
							pFCB,
							DirContext,
							0 
							);


	if( RC< 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)xixcore_FindDirEntry :  xixcore_LookupInitialDirEntry\n", RC));
		return RC;
	}



	RC = xixcore_Gethashvalueforfile(Name, NameLength, &HashVal );

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)xixcore_FindDirEntry :  xixcore_Gethashvalueforfile\n", RC));

		return RC;
	}


	pDirHashTable = (PXIDISK_HASH_VALUE_TABLE)xixcore_GetDataBufferWithOffset(DirContext->ChildHashTable); 
    //
    //  Now loop looking for a good match.
    //
    
	DirContext->IsVirtual =	0;
    while(1) {
		DirContext->IsVirtual =	0;
		DirContext->IsChildCache = 0;
		NextIndex = (xc_uint64)xixcore_FindSetBit(1024, NextIndex, DirContext->ChildBitMap);
		if(NextIndex == 1024){
			RC =  XCCODE_ENOENT;
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Can't find Dir entry\n"));
				break;
		}

		if(pDirHashTable->EntryHashVal[NextIndex] != HashVal){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR Searched Index(%lld) EntryHashVal(%d) ReqHashVal(%d)\n",
						NextIndex,
						pDirHashTable->EntryHashVal[NextIndex],
						HashVal));
			continue;
		}



		RC = xixcore_FindChildCacheEntry(DirContext->pVCB, DirContext->pFCB->LotNumber, (xc_uint32)NextIndex, &ChildCacheEntry);
		
		if( RC < 0 ) {
do_real_read:

			DirContext->IsChildCache = 0;
			xixcore_ZeroBufferOffset(DirContext->ChildEntryBuff);
			memset(xixcore_GetDataBuffer(DirContext->ChildEntryBuff),0,XIDISK_DUP_CHILD_RECORD_SIZE );

			RC = xixcore_RawReadDirEntry(
								DirContext->pVCB,
								DirContext->pFCB,
								(xc_uint32)NextIndex,
								DirContext->ChildEntryBuff,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMapBuff,
								DirContext->AddrMapSize,
								&(DirContext->AddrIndexNumber)
								);


			if( RC < 0 ){
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("Fail(0x%x)xixcore_UpdateDirNames: xixcore_RawReadDirEntry\n", RC));

				if(RC == XCCODE_CRCERROR){
					continue;
				}else{
					break;
				}
			}

			Child = (PXIDISK_CHILD_INFORMATION)xixcore_GetDataBufferWithOffset(DirContext->ChildEntryBuff);

			if(pDirHashTable->EntryHashVal[NextIndex] != Child->HashValue){
				continue;
			}

			if(Child->State == XIFS_FD_STATE_DELETED){
				continue;
			}else{

				DirContext->Type = Child->Type;
				DirContext->State = Child->State;						
				DirContext->NameSize = Child->NameSize;
				DirContext->ChildIndex = Child->ChildIndex;
				DirContext->StartLotIndex = Child->StartLotIndex;
				DirContext->HashValue = Child->HashValue;
				DirContext->Name = Child->Name;
				DirContext->IsChildCache = 0;
			}

		}else{
			PXIXCORE_CHILD_CACHE_INFO ChildInfo = NULL;
			if(pDirHashTable->EntryHashVal[NextIndex] != ChildCacheEntry->ChildCacheInfo.HashValue){
				xixcore_DeleteSpecialChildCacheEntry(DirContext->pVCB, ChildCacheEntry->ChildCacheInfo.ParentDirLotIndex, (xc_uint32)NextIndex);
				xixcore_DeRefChildCacheEntry(ChildCacheEntry);
				goto do_real_read;
			}

			xixcore_ZeroBufferOffset(DirContext->ChildEntryBuff);
			memset(xixcore_GetDataBuffer(DirContext->ChildEntryBuff),0,XIDISK_DUP_CHILD_RECORD_SIZE );
			
			Size = sizeof(XIXCORE_CHILD_CACHE_INFO) + ChildCacheEntry->ChildCacheInfo.NameSize -1;
			memcpy(xixcore_GetDataBuffer(DirContext->ChildEntryBuff),&ChildCacheEntry->ChildCacheInfo, Size);
			
			ChildInfo = (PXIXCORE_CHILD_CACHE_INFO)xixcore_GetDataBuffer(DirContext->ChildEntryBuff);

			DirContext->Type = ChildInfo->Type;
			DirContext->State = ChildInfo->State;						
			DirContext->NameSize = ChildInfo->NameSize;
			DirContext->ChildIndex = ChildInfo->ChildIndex;
			DirContext->StartLotIndex = ChildInfo->StartLotIndex;
			DirContext->HashValue = ChildInfo->HashValue;
			DirContext->Name = &(ChildInfo->Name[0]);
			DirContext->IsChildCache = 1;
			xixcore_DeRefChildCacheEntry(ChildCacheEntry);
		}


		//Update Dir Context
		
		DirContext->SearchedVirtualDirIndex = NextIndex + 2;
		DirContext->SearchedRealDirIndex = NextIndex;
		DirContext->RealDirIndex = NextIndex;
		DirContext->VirtualDirIndex = NextIndex + 2;
		DirContext->LotNumber = DirContext->StartLotIndex;
		
		Size = DirContext->NameSize;
		if(Size){
			if(Size > XIFS_MAX_NAME_LEN){
				Size = XIFS_MAX_NAME_LEN;
			}
		}
		DirContext->ChildNameLength =  Size;
		DirContext->ChildName = DirContext->Name;

		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("SearCh Dir RealIndex(%lld)   LotNumber(%lld)  ChildType(%d), ChildNamelen(%d) ChildIndex(%d)\n", 
				DirContext->RealDirIndex, 
				DirContext->LotNumber, 
				DirContext->Type, 
				DirContext->NameSize, 
				DirContext->ChildIndex
				));

		if(XIXCORE_TEST_FLAGS(DirContext->Type , XIFS_FD_TYPE_DIRECTORY)){
			DirContext->FileType = FCB_TYPE_DIR;
		}else{
			DirContext->FileType = FCB_TYPE_FILE;
		}

			
		if(  xixcore_IsNamesEqual(
						(xc_le16 *)Name, 
						(xc_uint32)(NameLength/2),
						(xc_le16 *)DirContext->ChildName, 
						(xc_uint32)(DirContext->ChildNameLength/2),
						bIgnoreCase,
						xixcore_global.xixcore_case_table, 
						xixcore_global.xixcore_case_table_length
			) == 1 ) 
		{
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
					("Exit xixcore_FindDirEntry .\n"));
			*EntryIndex = DirContext->SearchedRealDirIndex;
			return XCCODE_SUCCESS;
		}

    } 
    //
    //  No match was found.
    //

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Fail Exit XixFsFindDirEntry .\n"));



    return XCCODE_ENOENT;
}
// Added by ILGU HONG 12082006  END

/*
int
xixcore_FindDirEntry (
	PXIXCORE_VCB	pVCB,
	PXIXCORE_FCB 	pFCB,
	xc_uint8		*Name,
	xc_uint32		NameLength,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext,
	xc_uint64					*EntryIndex,
	xc_uint32					bIgnoreCase
    )
{
	int		RC = 0;
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_FindDirEntry .\n"));

	//
	//  Check inputs.
	//

	
	XIXCORE_ASSERT_VCB(pVCB );
	XIXCORE_ASSERT_FCB(pFCB );

	//
	//  Go get the first entry.
	//

	//MatchName = DirContext->ChildName;


	RC = xixcore_LookupInitialDirEntry( 
							pVCB,
							pFCB,
							DirContext,
							0 
							);


	if( RC< 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)xixcore_FindDirEntry :  xixcore_LookupInitialDirEntry\n", RC));
		return RC;
	}



    //
    //  Now loop looking for a good match.
    //
    
    while(1) {

	RC = xixcore_UpdateDirNames(DirContext);

	if( RC< 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)xixcore_FindDirEntry : xixcore_UpdateDirNames\n", RC));
		break;
	}			
	    
		
	if(  xixcore_IsNamesEqual(
					(xc_le16 *)Name, 
					(xc_uint32)(NameLength/2),
					(xc_le16 *)DirContext->ChildName, 
					(xc_uint32)(DirContext->ChildNameLength/2),
					bIgnoreCase,
					xixcore_global.xixcore_case_table, 
					xixcore_global.xixcore_cas_table_length
		) == 1 ) 
	{
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("Exit xixcore_FindDirEntry .\n"));
		*EntryIndex = DirContext->SearchedRealDirIndex;
		return XCCODE_SUCCESS;
	}

    } 
    //
    //  No match was found.
    //

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Fail Exit XixFsFindDirEntry .\n"));



    return RC;
}
*/



// Added by ILGU HONG 12082006
int
xixcore_call
xixcore_AddChildToDir(
	PXIXCORE_VCB					pVCB,
	PXIXCORE_FCB					pDir,
	xc_uint64						ChildLotNumber,
	xc_uint32						Type,
	xc_uint8 						*ChildName,
	xc_uint32						ChildNameLength,
	PXIXCORE_DIR_EMUL_CONTEXT		DirContext,
	xc_uint64 *						ChildIndex
)
{
	int					RC = 0;
	PXIXCORE_BUFFER			xbuf = NULL;
	xc_int32				Reason = 0;
	

	PXIDISK_DIR_INFO			DirInfo = NULL;
	xc_uint64					LotNumber = pDir->LotNumber;
	PXIDISK_CHILD_INFORMATION	pChildRecord = NULL;
	PXIDISK_HASH_VALUE_TABLE	pTable = NULL;
	xc_uint64					Index = 0;
	xc_uint32					bSetLock = 0;
	xc_uint16					HashVal = 0;




	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_AddChildToDir .\n"));


	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProtected){
		
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR is write protected .\n"));
		
		return XCCODE_EPERM;
	}
	// Added by ILGU HONG for readonly end

	
	if(pDir->HasLock != INODE_FILE_LOCK_HAS){

		RC = xixcore_LotLock(pVCB, pDir->LotNumber, &pDir->HasLock, 1, 1);

		if( RC < 0){
			
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)  xixcore_AddChildToDir : xixcore_LotLock .\n", RC));

					
			return XCCODE_EPERM;
		}
		
		bSetLock = 1;
	
	}




	RC = xixcore_Gethashvalueforfile(ChildName, ChildNameLength, &HashVal );

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)xixcore_AddChildToDir :  xixcore_Gethashvalueforfile\n", RC));

		goto error_out;
	}





	xbuf = xixcore_AllocateBuffer(XIDISK_DUP_DIR_INFO_SIZE);

	
	
	if(xbuf == NULL) {

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR  xixcore_AddChildToDir : can't alloc xbuf .\n"));
		
		RC = XCCODE_ENOMEM;
		goto error_out;
	}	
	
	xixcore_ZeroBufferOffset(xbuf);
	memset(xixcore_GetDataBuffer(xbuf), 0, XIDISK_DUP_DIR_INFO_SIZE);

	RC = xixcore_RawReadFileHeader(
					pVCB->XixcoreBlockDevice,
					pVCB->LotSize,
					pVCB->SectorSize,
					pVCB->SectorSizeBit,
					LotNumber,
					xbuf,
					&Reason
					);
					
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR(0x%x) Reason(0x%x) xixcore_AddChildToDir : xixcore_RawReadLotAndFileHeader .\n", 
				RC, Reason));

				
		goto error_out;
	}

	
	DirInfo = (PXIDISK_DIR_INFO)xixcore_GetDataBufferWithOffset(xbuf);
	Index = xixcore_FindFreeBit(1024, 0, DirInfo->ChildMap);
	if(Index > 1024){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR  xixcore_AddChildToDir : too many child already allocated..xixcore_FindFreeBit.\n"));

		RC = XCCODE_EMLINK;
		goto error_out;
	}


	xixcore_ZeroBufferOffset(DirContext->ChildHashTable);
	memset(xixcore_GetDataBuffer(DirContext->ChildHashTable), 0, XIDISK_DUP_HASH_VALUE_TABLE_SIZE);

	RC = xixcore_RawReadDirEntryHashValueTable(
				DirContext->pVCB->XixcoreBlockDevice,
				DirContext->pVCB->LotSize,
				DirContext->pVCB->SectorSize,
				DirContext->pVCB->SectorSizeBit,
				DirContext->pFCB->LotNumber,
				DirContext->ChildHashTable,
				&Reason
				);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) : xixcore_AddChildToDir:  xixcore_RawReadDirEntryHashValueTable\n", RC, Reason));
		
		goto error_out;
	}


	pTable = (PXIDISK_HASH_VALUE_TABLE)xixcore_GetDataBufferWithOffset(DirContext->ChildHashTable);





	xixcore_ZeroBufferOffset(DirContext->ChildEntryBuff);
	memset(xixcore_GetDataBuffer(DirContext->ChildEntryBuff),0, XIDISK_DUP_CHILD_RECORD_SIZE);

	RC = xixcore_RawReadDirEntry(
							pVCB,
							pDir,
							(xc_uint32)Index,
							DirContext->ChildEntryBuff,
							XIDISK_CHILD_RECORD_SIZE,
							DirContext->AddrMapBuff,
							DirContext->AddrMapSize,
							&(DirContext->AddrIndexNumber)
							);


	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR(0x%x)  xixcore_AddChildToDir : xixcore_RawWriteDirEntry .\n", 
				RC));

		if(RC == XCCODE_CRCERROR){
			xixcore_ZeroBufferOffset(DirContext->ChildEntryBuff);
		}else{
			goto error_out;
		}
	}

	// Make Child Record Endry
	pChildRecord = (PXIDISK_CHILD_INFORMATION)xixcore_GetDataBufferWithOffset(DirContext->ChildEntryBuff);


	pTable->EntryHashVal[Index]=HashVal;

	memset(pChildRecord,0,  (XIDISK_CHILD_RECORD_SIZE - XIXCORE_MD5DIGEST_AND_SEQSIZE));
	memcpy(pChildRecord->HostSignature, pVCB->HostId, 16);
	pChildRecord->NameSize = ChildNameLength;
	memcpy(pChildRecord->Name, ChildName, ChildNameLength);
	pChildRecord->ChildIndex = (xc_uint32)Index;
	pChildRecord->State = XIFS_FD_STATE_CREATE;
	pChildRecord->Type = Type;
	pChildRecord->StartLotIndex = ChildLotNumber;
	pChildRecord->HashValue = HashVal;
	
	xixcore_SetBit(Index, (unsigned long *)DirInfo->ChildMap);
	DirInfo->childCount++;

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB),
				("ChildMap Index(%lld).\n", Index));	

	
	RC = xixcore_RawWriteDirEntry(
							pVCB,
							pDir,
							(xc_uint32)Index,
							DirContext->ChildEntryBuff,
							XIDISK_CHILD_RECORD_SIZE,
							DirContext->AddrMapBuff,
							DirContext->AddrMapSize,
							&(DirContext->AddrIndexNumber)
							);


	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR(0x%x)  xixcore_AddChildToDir : xixcore_RawWriteDirEntry .\n", 
				RC));

		goto error_out;
	}


	RC = xixcore_RawWriteDirEntryHashValueTable(
				DirContext->pVCB->XixcoreBlockDevice,
				DirContext->pVCB->LotSize,
				DirContext->pVCB->SectorSize,
				DirContext->pVCB->SectorSizeBit,
				DirContext->pFCB->LotNumber,
				DirContext->ChildHashTable,
				&Reason
				);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) : xixcore_AddChildToDir:  xixcore_RawReadDirEntryHashValueTable\n", RC, Reason));
		
		goto error_out;
	}



	RC = xixcore_RawWriteFileHeader(
					pVCB->XixcoreBlockDevice,
					pVCB->LotSize,
					pVCB->SectorSize,
					pVCB->SectorSizeBit,
					LotNumber,
					xbuf,
					&Reason
					);


	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR(0x%x) Reason(0x%x) xixcore_AddChildToDir :xixcore_RawWriteLotAndFileHeader .\n", 
				RC, Reason));

				
		goto error_out;
	}
	
	pDir->ChildCount ++;
	*ChildIndex = Index;
	RC = 0;


error_out:
		
	if(bSetLock == 1){
		xixcore_LotUnLock(pVCB,  pDir->LotNumber, &pDir->HasLock);
	}

	
	if(xbuf){
		xixcore_FreeBuffer(xbuf);
	}
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Exit xixcore_AddChildToDir Status(0x%x) .\n", RC));

	return RC;				
}
// Added by ILGU HONG 12082006  END


/*
int
xixcore_AddChildToDir(
	PXIXCORE_VCB					pVCB,
	PXIXCORE_FCB					pDir,
	xc_uint64						ChildLotNumber,
	xc_uint32						Type,
	xc_uint8 						*ChildName,
	xc_uint32						ChildNameLength,
	PXIXCORE_DIR_EMUL_CONTEXT	DirContext,
	xc_uint64 *						ChildIndex
)
{
	int					RC = 0;
	PXIXCORE_BUFFER			xbuf = NULL;
	xc_int32				Reason = 0;
	
	PXIDISK_DIR_HEADER_LOT		DirLotHeader = NULL;
	PXIDISK_DIR_INFO			DirInfo = NULL;
	xc_uint64						LotNumber = pDir->LotNumber;
	PXIDISK_CHILD_INFORMATION	pChildRecord = NULL;
	xc_uint64						Index = 0;
	xc_uint32						bSetLock = 0;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_AddChildToDir .\n"));


	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProtected){
		
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR is write protected .\n"));
		
		return XCCODE_EPERM;
	}
	// Added by ILGU HONG for readonly end

	
	if(pDir->HasLock != INODE_FILE_LOCK_HAS){

		RC = xixcore_LotLock(pVCB, pDir->LotNumber, &pDir->HasLock, 1, 1);

		if( RC < 0){
			
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)  xixcore_AddChildToDir : xixcore_LotLock .\n", RC));

					
			return XCCODE_EPERM;
		}
		
		bSetLock = 1;
	
	}




	xbuf = xixcore_AllocateBuffer(XIDISK_DIR_HEADER_LOT_SIZE);

	
	
	if(xbuf == NULL) {

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR  xixcore_AddChildToDir : can't alloc xbuf .\n"));
		
		RC = XCCODE_ENOMEM;
		goto error_out;
	}	
	

	RC = xixcore_RawReadLotAndFileHeader(
					pVCB->XixcoreBlockDevice,
					pVCB->LotSize,
					pVCB->SectorSize,
					pVCB->SectorSizeBit,
					LotNumber,
					xbuf,
					&Reason
					);
					
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR(0x%x) Reason(0x%x) xixcore_AddChildToDir : xixcore_RawReadLotAndFileHeader .\n", 
				RC, Reason));

				
		goto error_out;
	}

	
	DirLotHeader = (PXIDISK_DIR_HEADER_LOT)xbuf->xix_data;
	DirInfo = &(DirLotHeader->DirInfo);

		
	// Make Child Record Endry
	pChildRecord = (PXIDISK_CHILD_INFORMATION) DirContext->ChildEntry;
	if(!pChildRecord){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR  xixcore_AddChildToDir : chile record is not allocated in Dir context .\n"));
		
		RC = XCCODE_UNSUCCESS;
		goto error_out;
	}

	Index = xixcore_FindFreeBit(1024, 0, DirInfo->ChildMap);
	if(Index > 1024){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR  xixcore_AddChildToDir : too many child already allocated..xixcore_FindFreeBit.\n"));
		
		RC = XCCODE_UNSUCCESS;
		goto error_out;
	}

	memset(pChildRecord,0,  XIDISK_CHILD_RECORD_SIZE);
	memcpy(pChildRecord->HostSignature, pVCB->HostId, 16);
	pChildRecord->NameSize = ChildNameLength;
	memcpy(pChildRecord->Name, ChildName, ChildNameLength);
	pChildRecord->ChildIndex = (xc_uint32)Index;
	pChildRecord->State = XIFS_FD_STATE_CREATE;
	pChildRecord->Type = Type;
	pChildRecord->StartLotIndex = ChildLotNumber;
	
	
	xixcore_SetBit(Index, (unsigned long *)DirInfo->ChildMap);
	DirInfo->childCount++;

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB),
				("ChildMap Index(%lld).\n", Index));	

	
	RC = xixcore_RawWriteDirEntry(
							pVCB,
							pDir,
							(xc_uint32)Index,
							DirContext->ChildEntryBuff,
							XIDISK_CHILD_RECORD_SIZE,
							DirContext->AddrMapBuff,
							DirContext->AddrMapSize,
							&(DirContext->AddrIndexNumber)
							);


	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR(0x%x)  xixcore_AddChildToDir : xixcore_RawWriteDirEntry .\n", 
				RC));

		goto error_out;
	}


	RC = xixcore_RawWriteLotAndFileHeader(
					pVCB->XixcoreBlockDevice,
					pVCB->LotSize,
					pVCB->SectorSize,
					pVCB->SectorSizeBit,
					LotNumber,
					xbuf,
					&Reason
					);


	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR(0x%x) Reason(0x%x) xixcore_AddChildToDir :xixcore_RawWriteLotAndFileHeader .\n", 
				RC, Reason));

				
		goto error_out;
	}
	
	pDir->ChildCount ++;
	*ChildIndex = Index;
	RC = 0;


error_out:
		
	if(bSetLock == 1){
		xixcore_LotUnLock(pVCB,  pDir->LotNumber, &pDir->HasLock);
	}

	
	if(xbuf){
		xixcore_FreeBuffer(xbuf);
	}
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Exit xixcore_AddChildToDir Status(0x%x) .\n", RC));

	return RC;				
}
*/

// Added by ILGU HONG 12082006
int
xixcore_call
xixcore_DeleteChildFromDir(
	PXIXCORE_VCB 				pVCB,
	PXIXCORE_FCB				pDir,
	xc_uint64					ChildIndex,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext
)
{
	int						RC = 0;
	xc_uint32					bSetLock = 0;
	PXIXCORE_BUFFER				xbuf = NULL;
	xc_int32					Reason = 0;
	
	PXIDISK_DIR_INFO			DirInfo = NULL;
	xc_uint64					LotNumber = pDir->LotNumber;
	PXIDISK_CHILD_INFORMATION	pChildRecord = NULL;
	PXIDISK_HASH_VALUE_TABLE	pTable = NULL;
	xc_uint64					Index = ChildIndex;
	
	



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_DeleteChildFromDir .\n"));

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProtected){
		
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("ERROR xixcore_DeleteChildFromDir : is write protected .\n"));		

		return XCCODE_EROFS;
	}
	// Added by ILGU HONG for readonly end



	if(pDir->HasLock != INODE_FILE_LOCK_HAS){

		RC = xixcore_LotLock(pVCB, pDir->LotNumber, &pDir->HasLock, 1, 1);
		
		if( RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("ERROR (0x%x) xixcore_DeleteChildFromDir : xixcore_LotLock .\n", RC));		
			
			return XCCODE_EPERM;
		}
		
		bSetLock = 1;
	
	}


		
		xbuf = xixcore_AllocateBuffer(XIDISK_DUP_DIR_INFO_SIZE);
		
		if(xbuf == NULL) {

			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("ERROR  xixcore_DeleteChildFromDir : can't allocate xbuf .\n"));	

			RC = XCCODE_ENOMEM;
			goto error_out;
		}
		
		xixcore_ZeroBufferOffset(xbuf);
		memset(xixcore_GetDataBuffer(xbuf), 0, XIDISK_DUP_DIR_INFO_SIZE);

		RC = xixcore_RawReadFileHeader(
						pVCB->XixcoreBlockDevice,
						pVCB->LotSize,
						pVCB->SectorSize,
						pVCB->SectorSizeBit,
						LotNumber,
						xbuf,
						&Reason
						);
						
		
		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("ERROR (0x%x) Reason(0x%x) xixcore_DeleteChildFromDir : xixcore_RawReadLotAndFileHeader .\n", 
					RC, Reason));

			goto error_out;
		}
		

		DirInfo = (PXIDISK_DIR_INFO)xixcore_GetDataBufferWithOffset(xbuf);


		xixcore_ZeroBufferOffset(DirContext->ChildHashTable);
		memset(xixcore_GetDataBuffer(DirContext->ChildHashTable), 0, XIDISK_DUP_HASH_VALUE_TABLE_SIZE);

		RC = xixcore_RawReadDirEntryHashValueTable(
					DirContext->pVCB->XixcoreBlockDevice,
					DirContext->pVCB->LotSize,
					DirContext->pVCB->SectorSize,
					DirContext->pVCB->SectorSizeBit,
					DirContext->pFCB->LotNumber,
					DirContext->ChildHashTable,
					&Reason
					);

		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) Reason(0x%x) : xixcore_DeleteChildFromDir :  xixcore_RawReadDirEntryHashValueTable\n", RC, Reason));
			
			goto error_out;
		}


		pTable = (PXIDISK_HASH_VALUE_TABLE)xixcore_GetDataBufferWithOffset(DirContext->ChildHashTable);



		xixcore_ZeroBufferOffset(DirContext->ChildEntryBuff);
		memset(xixcore_GetDataBuffer(DirContext->ChildEntryBuff), 0, XIDISK_DUP_CHILD_RECORD_SIZE);

		RC = xixcore_RawReadDirEntry(
								pVCB,
								pDir,
								(xc_uint32)Index,
								DirContext->ChildEntryBuff,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMapBuff,
								DirContext->AddrMapSize,
								&(DirContext->AddrIndexNumber)
								);


		if(RC < 0 ){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)  xixcore_DeleteChildFromDir : xixcore_RawReadDirEntry .\n", 
					RC));

			goto error_out;
		}


		

		pChildRecord = (PXIDISK_CHILD_INFORMATION)xixcore_GetDataBufferWithOffset(DirContext->ChildEntryBuff);


		// Make Child Record Endry
		
		pChildRecord->State = XIFS_FD_STATE_DELETED;

		RC = xixcore_RawWriteDirEntry(
								pVCB,
								pDir,
								(xc_uint32)Index,
								DirContext->ChildEntryBuff,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMapBuff,
								DirContext->AddrMapSize,
								&(DirContext->AddrIndexNumber)
								);


		if(RC < 0 ){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)  xixcore_DeleteChildFromDir : xixcore_RawWriteDirEntry .\n", 
					RC));

			goto error_out;
		}




		pTable->EntryHashVal[Index] = 0;		
		xixcore_ClearBit(Index, (unsigned long *)DirInfo->ChildMap);
		DirInfo->childCount--;
		pDir->ChildCount--;



		RC = xixcore_RawWriteDirEntryHashValueTable(
					DirContext->pVCB->XixcoreBlockDevice,
					DirContext->pVCB->LotSize,
					DirContext->pVCB->SectorSize,
					DirContext->pVCB->SectorSizeBit,
					DirContext->pFCB->LotNumber,
					DirContext->ChildHashTable,
					&Reason
					);

		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail(0x%x) Reason(0x%x) : xixcore_DeleteChildFromDir :  xixcore_RawReadDirEntryHashValueTable\n", RC, Reason));
			
			goto error_out;
		}


		

		RC = xixcore_RawWriteFileHeader(
						pVCB->XixcoreBlockDevice,
						pVCB->LotSize,
						pVCB->SectorSize,
						pVCB->SectorSizeBit,
						LotNumber,
						xbuf,
						&Reason
						);
						
		
		if( RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("ERROR (0x%x) Reason(0x%x) xixcore_DeleteChildFromDir : xixcore_RawWriteLotAndFileHeader .\n", 
					RC, Reason));

			goto error_out;
		}


		RC = 0;

error_out:
	
		if(bSetLock == 1){
			xixcore_LotUnLock(pVCB, pDir->LotNumber, &pDir->HasLock);
		}

		if(xbuf) {
			xixcore_FreeBuffer(xbuf);
		}
		
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("exit xixcore_DeleteChildFromDir Status(0x%x).\n", RC));
	return RC;	
				
}
// Added by ILGU HONG 12082006  END

/*
int
xixcore_DeleteChildFromDir(
	PXIXCORE_VCB 				pVCB,
	PXIXCORE_FCB				pDir,
	xc_uint64					ChildIndex,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext
)
{
	int						RC = 0;
	xc_uint32					bSetLock = 0;
	PXIXCORE_BUFFER				xbuf = NULL;
	xc_int32					Reason = 0;
	PXIDISK_DIR_HEADER_LOT DirLotHeader = NULL;
	PXIDISK_DIR_INFO		DirInfo = NULL;
	xc_uint64					LotNumber = pDir->LotNumber;
	PXIDISK_CHILD_INFORMATION	pChildRecord = NULL;
	xc_uint64					Index = ChildIndex;
	
	



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_DeleteChildFromDir .\n"));

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProtected){
		
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("ERROR xixcore_DeleteChildFromDir : is write protected .\n"));		

		return XCCODE_EPERM;
	}
	// Added by ILGU HONG for readonly end



	if(pDir->HasLock != INODE_FILE_LOCK_HAS){

		RC = xixcore_LotLock(pVCB, pDir->LotNumber, &pDir->HasLock, 1, 1);
		
		if( RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("ERROR (0x%x) xixcore_DeleteChildFromDir : xixcore_LotLock .\n", RC));		
			
			return XCCODE_EPERM;
		}
		
		bSetLock = 1;
	
	}


		
		xbuf = xixcore_AllocateBuffer(XIDISK_DIR_HEADER_LOT_SIZE);
		
		if(xbuf == NULL) {

			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("ERROR  xixcore_DeleteChildFromDir : can't allocate xbuf .\n"));	

			RC = XCCODE_ENOMEM;
			goto error_out;
		}
		
		
		RC = xixcore_RawReadLotAndFileHeader(
						pVCB->XixcoreBlockDevice,
						pVCB->LotSize,
						pVCB->SectorSize,
						pVCB->SectorSizeBit,
						LotNumber,
						xbuf,
						&Reason
						);
						
		
		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("ERROR (0x%x) Reason(0x%x) xixcore_DeleteChildFromDir : xixcore_RawReadLotAndFileHeader .\n", 
					RC, Reason));

			goto error_out;
		}
		

		DirLotHeader = (PXIDISK_DIR_HEADER_LOT)xbuf->xix_data;
		DirInfo = &(DirLotHeader->DirInfo);


		pChildRecord = (PXIDISK_CHILD_INFORMATION) DirContext->ChildEntry;

		
		if(!pChildRecord){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR  xixcore_DeleteChildFromDir : chile record is not allocated in Dir context .\n"));
			
			RC = XCCODE_ENOMEM;
			goto error_out;
		}

		RC = xixcore_RawReadDirEntry(
								pVCB,
								pDir,
								(xc_uint32)Index,
								DirContext->ChildEntryBuff,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMapBuff,
								DirContext->AddrMapSize,
								&(DirContext->AddrIndexNumber)
								);


		if(RC < 0 ){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)  xixcore_DeleteChildFromDir : xixcore_RawReadDirEntry .\n", 
					RC));

			goto error_out;
		}



		// Make Child Record Endry
		
		pChildRecord->State = XIFS_FD_STATE_DELETED;

		RC = xixcore_RawWriteDirEntry(
								pVCB,
								pDir,
								(xc_uint32)Index,
								DirContext->ChildEntryBuff,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMapBuff,
								DirContext->AddrMapSize,
								&(DirContext->AddrIndexNumber)
								);


		if(RC < 0 ){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)  xixcore_DeleteChildFromDir : xixcore_RawWriteDirEntry .\n", 
					RC));

			goto error_out;
		}

		
		xixcore_ClearBit(Index, (unsigned long *)DirInfo->ChildMap);
		DirInfo->childCount--;
		pDir->ChildCount--;
		

		RC = xixcore_RawWriteLotAndFileHeader(
						pVCB->XixcoreBlockDevice,
						pVCB->LotSize,
						pVCB->SectorSize,
						pVCB->SectorSizeBit,
						LotNumber,
						xbuf,
						&Reason
						);
						
		
		if( RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("ERROR (0x%x) Reason(0x%x) xixcore_DeleteChildFromDir : xixcore_RawWriteLotAndFileHeader .\n", 
					RC, Reason));

			goto error_out;
		}


		RC = 0;

error_out:
	
		if(bSetLock == 1){
			xixcore_LotUnLock(pVCB, pDir->LotNumber, &pDir->HasLock);
		}

		if(xbuf) {
			xixcore_FreeBuffer(xbuf);
		}
		
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("exit xixcore_DeleteChildFromDir Status(0x%x).\n", RC));
	return RC;	
				
}
*/



// Added by ILGU HONG 12082006
int
xixcore_call
xixcore_UpdateChildFromDir(
	PXIXCORE_VCB		pVCB,
	PXIXCORE_FCB		pDir,
	xc_uint64			ChildLotNumber,
	xc_uint32			Type,
	xc_uint32			State,
	xc_uint8			*ChildName,
	xc_uint32			ChildNameLength,
	xc_uint64			ChildIndex,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext
)
{
	int							RC = 0;
	xc_uint32						bSetLock = 0;
	//xc_uint64						LotNumber = pDir->LotNumber;
	PXIDISK_CHILD_INFORMATION	pChildRecord = NULL;
	PXIDISK_HASH_VALUE_TABLE	pTable = NULL;
	xc_uint16						HashVal = 0;
	xc_int32						Reason = 0;
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_UpdateChildFromDir .\n"));
	

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("ERROR xixcore_UpdateChildFromDir : is write protected .\n"));		

		return XCCODE_EPERM;
	}
	// Added by ILGU HONG for readonly end


	if(pDir->HasLock != INODE_FILE_LOCK_HAS){

		RC = xixcore_LotLock(pVCB, pDir->LotNumber, &pDir->HasLock, 1, 1);

		if( RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("ERROR (0x%x) xixcore_UpdateChildFromDir : xixcore_LotLock .\n", RC));		
			
			return XCCODE_EPERM;
		}
		
		bSetLock = 1;
	
	}


	RC = xixcore_Gethashvalueforfile(ChildName, ChildNameLength, &HashVal );

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)xixcore_UpdateChildFromDir :  xixcore_Gethashvalueforfile\n", RC));

		goto error_out;
	}

	xixcore_ZeroBufferOffset(DirContext->ChildHashTable);
	memset(xixcore_GetDataBuffer(DirContext->ChildHashTable),0,XIDISK_DUP_HASH_VALUE_TABLE_SIZE);

	RC = xixcore_RawReadDirEntryHashValueTable(
				DirContext->pVCB->XixcoreBlockDevice,
				DirContext->pVCB->LotSize,
				DirContext->pVCB->SectorSize,
				DirContext->pVCB->SectorSizeBit,
				DirContext->pFCB->LotNumber,
				DirContext->ChildHashTable,
				&Reason
				);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) : xixcore_UpdateChildFromDir:  xixcore_RawReadDirEntryHashValueTable\n", RC, Reason));
		
		goto error_out;
	}


	pTable = (PXIDISK_HASH_VALUE_TABLE)xixcore_GetDataBufferWithOffset(DirContext->ChildHashTable);
	pTable->EntryHashVal[ChildIndex] = HashVal;


	RC = xixcore_RawWriteDirEntryHashValueTable(
				DirContext->pVCB->XixcoreBlockDevice,
				DirContext->pVCB->LotSize,
				DirContext->pVCB->SectorSize,
				DirContext->pVCB->SectorSizeBit,
				DirContext->pFCB->LotNumber,
				DirContext->ChildHashTable,
				&Reason
				);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) : xixcore_UpdateChildFromDir:  xixcore_RawReadDirEntryHashValueTable\n", RC, Reason));
		
		goto error_out;
	}



	xixcore_ZeroBufferOffset(DirContext->ChildEntryBuff);
	memset(xixcore_GetDataBuffer(DirContext->ChildEntryBuff), 0, XIDISK_DUP_CHILD_RECORD_SIZE);

	RC =xixcore_RawReadDirEntry(
							pVCB,
							pDir,
							(xc_uint32)ChildIndex,
							DirContext->ChildEntryBuff,
							XIDISK_CHILD_RECORD_SIZE,
							DirContext->AddrMapBuff,
							DirContext->AddrMapSize,
							&(DirContext->AddrIndexNumber)
							);


	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR(0x%x)  xixcore_UpdateChildFromDir : xixcore_RawReadDirEntry .\n", 
				RC));

		goto error_out;
	}

	pChildRecord = (PXIDISK_CHILD_INFORMATION)xixcore_GetDataBufferWithOffset(DirContext->ChildEntryBuff);

	if(pChildRecord->State == XIFS_FD_STATE_DELETED){
		RC = XCCODE_ENOENT;

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR  xixcore_UpdateChildFromDir : previous deleted item .\n"));
		
		goto error_out;
	}


	pChildRecord->State = State;
	pChildRecord->Type = Type;
	pChildRecord->StartLotIndex = ChildLotNumber;
	pChildRecord->ChildIndex = (xc_uint32)ChildIndex;
	pChildRecord->HashValue = HashVal;

	if(ChildName != NULL){
		pChildRecord->NameSize = ChildNameLength;
		memset(pChildRecord->Name,0,2000);
		memcpy(pChildRecord->Name, ChildName, ChildNameLength);
	}


	RC = xixcore_RawWriteDirEntry(
							pVCB,
							pDir,
							(xc_uint32)ChildIndex,
							DirContext->ChildEntryBuff,
							XIDISK_CHILD_RECORD_SIZE,
							DirContext->AddrMapBuff,
							DirContext->AddrMapSize,
							&(DirContext->AddrIndexNumber)
							);



	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("ERROR(0x%x)  xixcore_UpdateChildFromDir :  xixcore_RawWriteDirEntry .\n", 
				RC));

		goto error_out;
	}

	RC = 0;

error_out:
	
	if(bSetLock == 1){
		xixcore_LotUnLock(pVCB, pDir->LotNumber , &pDir->HasLock);
		
	}



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Exit xixcore_UpdateChildFromDir Status(0x%x) .\n", RC));
	return RC;	
}
// Added by ILGU HONG 12082006  END
/*
int
xixcore_UpdateChildFromDir(
	PXIXCORE_VCB		pVCB,
	PXIXCORE_FCB		pDir,
	xc_uint64			ChildLotNumber,
	xc_uint32			Type,
	xc_uint32			State,
	xc_uint8			*ChildName,
	xc_uint32			ChildNameLength,
	xc_uint64			ChildIndex,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext
)
{
	int							RC = 0;
	xc_uint32						bSetLock = 0;
	//xc_uint64						LotNumber = pDir->LotNumber;
	PXIDISK_CHILD_INFORMATION	pChildRecord = NULL;
	//xc_uint32						Reason = 0;
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Enter xixcore_UpdateChildFromDir .\n"));
	

	// Added by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
			("ERROR xixcore_UpdateChildFromDir : is write protected .\n"));		

		return XCCODE_EPERM;
	}
	// Added by ILGU HONG for readonly end


	if(pDir->HasLock != INODE_FILE_LOCK_HAS){

		RC = xixcore_LotLock(pVCB, pDir->LotNumber, &pDir->HasLock, 1, 1);

		if( RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
				("ERROR (0x%x) xixcore_UpdateChildFromDir : xixcore_LotLock .\n", RC));		
			
			return XCCODE_EPERM;
		}
		
		bSetLock = 1;
	
	}


		pChildRecord = (PXIDISK_CHILD_INFORMATION) DirContext->ChildEntry;


		if(!pChildRecord){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR  xixcore_UpdateChildFromDir : chile record is not allocated in Dir context .\n"));
			
			RC = XCCODE_UNSUCCESS;
			goto error_out;
		}



		RC =xixcore_RawReadDirEntry(
								pVCB,
								pDir,
								(xc_uint32)ChildIndex,
								DirContext->ChildEntryBuff,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMapBuff,
								DirContext->AddrMapSize,
								&(DirContext->AddrIndexNumber)
								);


		if(RC < 0 ){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)  xixcore_UpdateChildFromDir : xixcore_RawReadDirEntry .\n", 
					RC));

			goto error_out;
		}


		if(pChildRecord->State == XIFS_FD_STATE_DELETED){
			RC = XCCODE_ENOENT;

			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR  xixcore_UpdateChildFromDir : previous deleted item .\n"));
			
			goto error_out;
		}

		pChildRecord->State = State;
		pChildRecord->Type = Type;
		pChildRecord->StartLotIndex = ChildLotNumber;
		pChildRecord->ChildIndex = (xc_uint32)ChildIndex;
		
		if(ChildName != NULL){
			pChildRecord->NameSize = ChildNameLength;
			memset(pChildRecord->Name,0,2000);
			memcpy(pChildRecord->Name, ChildName, ChildNameLength);
		}


		RC = xixcore_RawWriteDirEntry(
								pVCB,
								pDir,
								(xc_uint32)ChildIndex,
								DirContext->ChildEntryBuff,
								XIDISK_CHILD_RECORD_SIZE,
								DirContext->AddrMapBuff,
								DirContext->AddrMapSize,
								&(DirContext->AddrIndexNumber)
								);



		if(RC < 0 ){
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
				("ERROR(0x%x)  xixcore_UpdateChildFromDir :  xixcore_RawWriteDirEntry .\n", 
					RC));

			goto error_out;
		}

		RC = 0;

error_out:
	
		if(bSetLock == 1){
			xixcore_LotUnLock(pVCB, pDir->LotNumber , &pDir->HasLock);
			
		}



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB), 
		("Exit xixcore_UpdateChildFromDir Status(0x%x) .\n", RC));
	return RC;	
}
*/


/*
 * Unicode manipulation
 */

xc_uint32 
xixcore_call
xixcore_CompareUcs(const xc_le16 *s1, const xc_le16 *s2, xc_size_t n)
{
	xc_uint16 c1, c2;
	xc_size_t i;

	for (i = 0; i < n; ++i) {
		c1 = XIXCORE_LE2CPU16(s1[i]);
		c2 = XIXCORE_LE2CPU16(s2[i]);
		if (c1 < c2)
			return XCCODE_SUCCESS;
		if (c1 > c2)
			return XCCODE_SUCCESS;
		if (!c1)
			break;
	}
	return 1;
}


xc_uint32
xixcore_call
xixcore_CompareUcsNCase(
		const xc_le16 *s1, 
		const xc_le16 *s2, 
		xc_size_t n,
		const xc_le16 *upcase, 
		const xc_uint32 upcase_size
)
{
	xc_size_t i;
	xc_uint16 c1, c2;

	for (i = 0; i < n; ++i) {
		if ((c1 = XIXCORE_LE2CPU16(s1[i])) < upcase_size)
			c1 = XIXCORE_LE2CPU16(upcase[c1]);
		if ((c2 = XIXCORE_LE2CPU16(s2[i])) < upcase_size)
			c2 = XIXCORE_LE2CPU16(upcase[c2]);
		if (c1 < c2)
			return XCCODE_SUCCESS;
		if (c1 > c2)
			return XCCODE_SUCCESS;
		if (!c1)
			break;
	}
	return 1;
}


xc_uint32 
xixcore_call
xixcore_IsNamesEqual(
	const xc_le16 		*s1, 
	xc_size_t 			s1_len,
	const xc_le16 		*s2, 
	xc_size_t 			s2_len, 
	xc_uint32 			bIgnoreCase,
	const xc_le16 		*upcase, 
	const xc_uint32 		upcase_size
)
{
	if (s1_len != s2_len)
		return XCCODE_SUCCESS;
	if (bIgnoreCase== 0)
		return xixcore_CompareUcs(s1, s2, s1_len);
	return xixcore_CompareUcsNCase(s1, s2, s1_len, upcase, upcase_size);
}

void 
xixcore_call
xixcore_UpcaseUcsName(
	xc_le16 *name, 
	xc_uint32 name_len, 
	const xc_le16 *upcase,
	const xc_uint32 upcase_len
)
{
	xc_uint32 i;
	xc_uint16 u;

	for (i = 0; i < name_len; i++)
		if ((u = XIXCORE_LE2CPU16(name[i])) < upcase_len)
			name[i] = upcase[u];
}

xc_le16 *
xixcore_call
xixcore_GenerateDefaultUpcaseTable(void)
{
	static const int uc_run_table[][3] = { /* Start, End, Add */
	{0x0061, 0x007B,  -32}, {0x0451, 0x045D, -80}, {0x1F70, 0x1F72,  74},
	{0x00E0, 0x00F7,  -32}, {0x045E, 0x0460, -80}, {0x1F72, 0x1F76,  86},
	{0x00F8, 0x00FF,  -32}, {0x0561, 0x0587, -48}, {0x1F76, 0x1F78, 100},
	{0x0256, 0x0258, -205}, {0x1F00, 0x1F08,   8}, {0x1F78, 0x1F7A, 128},
	{0x028A, 0x028C, -217}, {0x1F10, 0x1F16,   8}, {0x1F7A, 0x1F7C, 112},
	{0x03AC, 0x03AD,  -38}, {0x1F20, 0x1F28,   8}, {0x1F7C, 0x1F7E, 126},
	{0x03AD, 0x03B0,  -37}, {0x1F30, 0x1F38,   8}, {0x1FB0, 0x1FB2,   8},
	{0x03B1, 0x03C2,  -32}, {0x1F40, 0x1F46,   8}, {0x1FD0, 0x1FD2,   8},
	{0x03C2, 0x03C3,  -31}, {0x1F51, 0x1F52,   8}, {0x1FE0, 0x1FE2,   8},
	{0x03C3, 0x03CC,  -32}, {0x1F53, 0x1F54,   8}, {0x1FE5, 0x1FE6,   7},
	{0x03CC, 0x03CD,  -64}, {0x1F55, 0x1F56,   8}, {0x2170, 0x2180, -16},
	{0x03CD, 0x03CF,  -63}, {0x1F57, 0x1F58,   8}, {0x24D0, 0x24EA, -26},
	{0x0430, 0x0450,  -32}, {0x1F60, 0x1F68,   8}, {0xFF41, 0xFF5B, -32},
	{0}
	};

	static const int uc_dup_table[][2] = { /* Start, End */
	{0x0100, 0x012F}, {0x01A0, 0x01A6}, {0x03E2, 0x03EF}, {0x04CB, 0x04CC},
	{0x0132, 0x0137}, {0x01B3, 0x01B7}, {0x0460, 0x0481}, {0x04D0, 0x04EB},
	{0x0139, 0x0149}, {0x01CD, 0x01DD}, {0x0490, 0x04BF}, {0x04EE, 0x04F5},
	{0x014A, 0x0178}, {0x01DE, 0x01EF}, {0x04BF, 0x04BF}, {0x04F8, 0x04F9},
	{0x0179, 0x017E}, {0x01F4, 0x01F5}, {0x04C1, 0x04C4}, {0x1E00, 0x1E95},
	{0x018B, 0x018B}, {0x01FA, 0x0218}, {0x04C7, 0x04C8}, {0x1EA0, 0x1EF9},
	{0}
	};

	static const int uc_word_table[][2] = { /* Offset, Value */
	{0x00FF, 0x0178}, {0x01AD, 0x01AC}, {0x01F3, 0x01F1}, {0x0269, 0x0196},
	{0x0183, 0x0182}, {0x01B0, 0x01AF}, {0x0253, 0x0181}, {0x026F, 0x019C},
	{0x0185, 0x0184}, {0x01B9, 0x01B8}, {0x0254, 0x0186}, {0x0272, 0x019D},
	{0x0188, 0x0187}, {0x01BD, 0x01BC}, {0x0259, 0x018F}, {0x0275, 0x019F},
	{0x018C, 0x018B}, {0x01C6, 0x01C4}, {0x025B, 0x0190}, {0x0283, 0x01A9},
	{0x0192, 0x0191}, {0x01C9, 0x01C7}, {0x0260, 0x0193}, {0x0288, 0x01AE},
	{0x0199, 0x0198}, {0x01CC, 0x01CA}, {0x0263, 0x0194}, {0x0292, 0x01B7},
	{0x01A8, 0x01A7}, {0x01DD, 0x018E}, {0x0268, 0x0197},
	{0}
	};

	int i, r;
	xc_le16 *uc;

	uc = xixcore_AllocateUpcaseTable();
	if (!uc)
		return uc;
	memset(uc, 0, XIXCORE_DEFAULT_UPCASE_NAME_LEN * sizeof(xc_le16));
	/* Generate the little endian Unicode upcase table used by ntfs. */
	for (i = 0; i < XIXCORE_DEFAULT_UPCASE_NAME_LEN; i++)
		uc[i] = XIXCORE_CPU2LE16(i);
	for (r = 0; uc_run_table[r][0]; r++)
		for (i = uc_run_table[r][0]; i < uc_run_table[r][1]; i++)
			uc[i] = XIXCORE_CPU2LE16(XIXCORE_LE2CPU16(uc[i]) +
					uc_run_table[r][2]);
	for (r = 0; uc_dup_table[r][0]; r++)
		for (i = uc_dup_table[r][0]; i < uc_dup_table[r][1]; i += 2)
			uc[i + 1] = XIXCORE_CPU2LE16(XIXCORE_LE2CPU16(uc[i + 1]) - 1);
	for (r = 0; uc_word_table[r][0]; r++)
		uc[uc_word_table[r][0]] = XIXCORE_CPU2LE16(uc_word_table[r][1]);
	return uc;
}
