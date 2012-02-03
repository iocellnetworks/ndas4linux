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
#include "xixcore/buffer.h"
#include "xixcore/ondisk.h"
#include "xixcore/lotinfo.h"
#include "xixcore/md5.h"

/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCLOTINFO"


void 
xixcore_call
xixcore_InitializeCommonLotHeader(
	IN PXIDISK_COMMON_LOT_HEADER 	LotHeader,
	IN xc_uint32						LotSignature,
	IN xc_uint32						LotType,
	IN xc_uint32						LotFlag,
	IN xc_int64						LotNumber,
	IN xc_int64						BeginLotNumber,
	IN xc_int64						PreviousLotNumber,
	IN xc_int64 						NextLotNumber,
	IN xc_int64						LogicalStartOffset,
	IN xc_int32						StartDataOffset,
	IN xc_int32						TotalDataSize
)
{

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_InitializeCommonLotHeader \n"));

	memset(LotHeader, 0, (XIDISK_COMMON_LOT_HEADER_SIZE - XIXCORE_MD5DIGEST_AND_SEQSIZE));
	LotHeader->Lock.LockState = XIDISK_LOCK_RELEASED;
	LotHeader->LotInfo.Type = LotType;
	LotHeader->LotInfo.Flags = LotFlag;
	LotHeader->LotInfo.BeginningLotIndex = BeginLotNumber;
	LotHeader->LotInfo.LotIndex = LotNumber;
	LotHeader->LotInfo.PreviousLotIndex = PreviousLotNumber;
	LotHeader->LotInfo.NextLotIndex = NextLotNumber;
	LotHeader->LotInfo.LogicalStartOffset = LogicalStartOffset;
	LotHeader->LotInfo.StartDataOffset = StartDataOffset;
	LotHeader->LotInfo.LotTotalDataSize = TotalDataSize;
	LotHeader->LotInfo.LotSignature = LotSignature;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_InitializeCommonLotHeader \n"));
}


int
xixcore_call
xixcore_CheckLotInfo(
	IN	PXIDISK_LOT_INFO		pLotInfo,
	IN	xc_uint32				VolLotSignature,
	IN 	xc_int64				LotNumber,
	IN 	xc_uint32				Type,
	IN 	xc_uint32				Flags,
	IN OUT	xc_int32			*Reason
)
{

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter XixFsCheckLotInfo \n"));

	*Reason = 0;
	
	if(pLotInfo->LotIndex!= LotNumber)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixcore_CheckLotInfo is not same index(%lld) request(%lld).\n",pLotInfo->LotIndex, LotNumber));			
		*Reason = XCREASON_LOT_INDEX_WARN;
		return XCCODE_EINVAL;		
	}

	if(pLotInfo->Type != Type) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail xixcore_CheckLotInfo is not same Type(%x) request(%x).\n",pLotInfo->Type, Type));			
		*Reason = XCREASON_LOT_TYPE_WARN;
		return XCCODE_EINVAL;	
	}
	
	if(pLotInfo->Flags != Flags){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail xixcore_CheckLotInfo is not same Flags(%x) request(%x).\n",pLotInfo->Flags, Flags));
		*Reason = XCREASON_LOT_FLAG_WARN;
		return XCCODE_EINVAL;
	}


	if(pLotInfo->LotSignature != VolLotSignature){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixcore_CheckLotInfo is not same Signature(%x) request(%x).\n",pLotInfo->LotSignature, VolLotSignature));
		*Reason = XCREASON_LOT_SIGNATURE_WARN;
		return XCCODE_EINVAL;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_CheckLotInfo \n"));
	return XCCODE_SUCCESS;
}


int
xixcore_call
xixcore_CheckOutLotHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32			VolLotSignature,
	xc_uint64			LotIndex,
	xc_uint32			LotSize,
	xc_uint32			LotType,
	xc_uint32			LotFlag,
	xc_uint32			SectorSize,
	xc_uint32			SectorSizeBit,
	xc_int32			*Reason
)
{
	int			RC = 0;
	PXIXCORE_BUFFER	xbuf = NULL;
	PXIDISK_COMMON_LOT_HEADER	pLotHeader = NULL;
	PXIDISK_LOT_INFO			pLotInfo = NULL;
	xc_uint32				size = 0;

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_CheckOutLotHeader\n"));

	size = XIDISK_DUP_COMMON_LOT_HEADER_SIZE;

	xbuf = xixcore_AllocateBuffer(size);
	if( !xbuf ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error xixcore_CheckOutLotHeader : can't allocate Xbuf size(%d)\n", size));
		
		*Reason = XCREASON_BUF_NOT_ALLOCATED;
		return XCCODE_ENOMEM;
	}
	
	
	(void)xixcore_FillBuffer(xbuf, 0);
	RC = xixcore_RawReadLotHeader(bdev,LotSize,SectorSize,  SectorSizeBit, LotIndex, xbuf,Reason);

	if ( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error xixcore_CheckOutLotHeader : call xixcore_RawReadLotHeader\n"));

		goto exit;
	}

	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(xbuf);	
	
		
	pLotInfo = &(pLotHeader->LotInfo);


	RC = xixcore_CheckLotInfo(pLotInfo,VolLotSignature,LotIndex,LotType,LotFlag,Reason);

	if ( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error xixcore_CheckOutLotHeader : call xixcore_CheckLotInfo\n"));

		goto exit;
	}		
	


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_CheckOutLotHeader\n"));

exit:
	xixcore_FreeBuffer(xbuf);		
	return RC;
}


void
xixcore_call
xixcore_GetLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN OUT PXIXCORE_IO_LOT_INFO 	pAddressInfo
)
{

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_GetLotInfo \n"));

	pAddressInfo->Type = pLotInfo->Type;
	pAddressInfo->Flags = pLotInfo->Flags;
	pAddressInfo->BeginningLotIndex = pLotInfo->BeginningLotIndex;
	pAddressInfo->LotIndex = pLotInfo->LotIndex;
	pAddressInfo->NextLotIndex = pLotInfo->NextLotIndex;
	pAddressInfo->PreviousLotIndex = pLotInfo->PreviousLotIndex;
	pAddressInfo->LogicalStartOffset = pLotInfo->LogicalStartOffset;
	pAddressInfo->StartDataOffset = pLotInfo->StartDataOffset;
	pAddressInfo->LotTotalDataSize = pLotInfo->LotTotalDataSize;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_GetLotInfo \n"));
}


void
xixcore_call
xixcore_SetLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN 	PXIXCORE_IO_LOT_INFO 	pAddressInfo
)
{
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_SetLotInfo \n"));

	pLotInfo->Type = pAddressInfo->Type ;
	pLotInfo->Flags = pAddressInfo->Flags ;
	pLotInfo->BeginningLotIndex = pAddressInfo->BeginningLotIndex ;
	pLotInfo->LotIndex = pAddressInfo->LotIndex ;
	pLotInfo->NextLotIndex = pAddressInfo->NextLotIndex ;
	pLotInfo->PreviousLotIndex = pAddressInfo->PreviousLotIndex ;
	pLotInfo->LogicalStartOffset = pAddressInfo->LogicalStartOffset ;
	pLotInfo->StartDataOffset = pAddressInfo->StartDataOffset ;
	pLotInfo->LotTotalDataSize = pAddressInfo->LotTotalDataSize ;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_SetLotInfo \n"));

}



int
xixcore_call
xixcore_DumpFileLot(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32  VolLotSignature,
	xc_uint32	LotSize,
	xc_uint64	StartIndex,
	xc_uint32	Type,
	xc_uint32	SectorSize,
	xc_uint32	SectorSizeBit,
	xc_int32	*Reason
	
)
{
	int			RC = 0;
	PXIXCORE_BUFFER			xbuf = NULL;
	XIXCORE_IO_LOT_INFO 	AddressInfo;
	PXIDISK_COMMON_LOT_HEADER  pLotHeader = NULL;
	xc_uint32 		size = 0;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_DumpFileLot\n"));
	
	


	size = XIDISK_DUP_COMMON_LOT_HEADER_SIZE;

	xbuf = xixcore_AllocateBuffer(size);
	if( !xbuf ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error xixcore_DumpFileLot : can't allocate Xbuf size%d\n", size));
		
		*Reason = XCREASON_BUF_NOT_ALLOCATED;
		return XCCODE_ENOMEM;
	}

	
	
	
	RC = xixcore_RawReadLotHeader(bdev,LotSize,SectorSize, SectorSizeBit, StartIndex, xbuf, Reason);

	if ( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error xixcore_DumpFileLot : call xixcore_RawReadLotHeader\n"));

		goto exit;
	}
	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(xbuf);
	
		
	RC = xixcore_CheckLotInfo((PXIDISK_LOT_INFO)&(pLotHeader->LotInfo), VolLotSignature, StartIndex,Type,LOT_FLAG_BEGIN, Reason);	

	if ( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error xixcore_DumpFileLot : call xixcore_CheckLotInfo\n"));

		goto exit;
	}


		
	memset(&AddressInfo, 0,  sizeof(XIXCORE_IO_LOT_INFO));
	xixcore_GetLotInfo((PXIDISK_LOT_INFO)&(pLotHeader->LotInfo), &AddressInfo);

	//Debug Print Lot Information

	while( AddressInfo.NextLotIndex != 0){
		
		memset(&AddressInfo, 0,  sizeof(XIXCORE_IO_LOT_INFO));
		
		xixcore_ZeroBufferOffset(xbuf);
		(void)xixcore_FillBuffer(xbuf, 0);
		RC = xixcore_RawReadLotHeader(bdev,LotSize,SectorSize, SectorSizeBit, AddressInfo.NextLotIndex, xbuf, Reason);

		if ( RC < 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Error xixcore_DumpFileLot : call xixcore_RawReadLotHeader\n"));

			goto exit;
		}
		pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(xbuf);
		
		RC = xixcore_CheckLotInfo((PXIDISK_LOT_INFO)&(pLotHeader->LotInfo), VolLotSignature, AddressInfo.NextLotIndex,Type,LOT_FLAG_BODY,Reason);	

		if ( RC < 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Error xixcore_DumpFileLot : call xixcore_CheckLotInfo\n"));

			goto exit;
		}

	
		memset(&AddressInfo, 0,  sizeof(XIXCORE_IO_LOT_INFO));
		xixcore_GetLotInfo((PXIDISK_LOT_INFO)&(pLotHeader->LotInfo), &AddressInfo);	
	}
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_DumpFileLot\n"));

exit:
	
	xixcore_FreeBuffer(xbuf);		
	return RC;
	
}

