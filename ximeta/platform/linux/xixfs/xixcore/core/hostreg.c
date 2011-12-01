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

#include "xcsystem/debug.h"
#include "xcsystem/errinfo.h"
#include "xcsystem/system.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/buffer.h"
#include "xixcore/ondisk.h"
#include "xixcore/bitmap.h"
#include "xixcore/lotlock.h"
#include "xixcore/file.h"
#include "xixcore/hostreg.h"
#include "xixcore/md5.h"

/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCHOSTREG"

int
xixcore_call
xixcore_InitializeRecordContext(
	PXIXCORE_RECORD_EMUL_CONTEXT RecordEmulCtx,
	PXIXCORE_VCB pVCB,
	xc_uint8 * HOSTSIGNATURE
)
{
	int RC = 0;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_InitializeRecordContext .\n"));	

	memset(RecordEmulCtx, 0, sizeof(XIXCORE_RECORD_EMUL_CONTEXT));
	memcpy(RecordEmulCtx->HostSignature, HOSTSIGNATURE, 16);


	RecordEmulCtx->RecordInfo = xixcore_AllocateBuffer(XIDISK_DUP_HOST_INFO_SIZE);
	if(!RecordEmulCtx->RecordInfo) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_InitializeRecordContext -->can allocate RecordInfo .\n"));	
		RC = XCCODE_ENOMEM;
		
		goto error_out;
	}
	
	memset(xixcore_GetDataBuffer(RecordEmulCtx->RecordInfo), 0, XIDISK_DUP_HOST_INFO_SIZE);

	RecordEmulCtx->RecordEntry = xixcore_AllocateBuffer(XIDISK_DUP_HOST_RECORD_SIZE);
	
	if(!RecordEmulCtx->RecordEntry) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_InitializeRecordContext -->can allocate RecordEntry .\n"));	

		RC = XCCODE_ENOMEM;
		
		goto error_out;
	}

	memset(xixcore_GetDataBuffer(RecordEmulCtx->RecordEntry), 0, XIDISK_DUP_HOST_RECORD_SIZE);

	RecordEmulCtx->VCB = pVCB;


	return RC;
error_out:	

	if(RecordEmulCtx->RecordEntry) {
		xixcore_FreeBuffer(RecordEmulCtx->RecordEntry);
		RecordEmulCtx->RecordEntry = NULL;
	}

	if(RecordEmulCtx->RecordInfo) {
		xixcore_FreeBuffer(RecordEmulCtx->RecordInfo);
		RecordEmulCtx->RecordInfo = NULL;
	}

	return RC;
	
}





int
xixcore_call
xixcore_CleanupRecordContext(
	PXIXCORE_RECORD_EMUL_CONTEXT RecordEmulCtx
)
{

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter XixFsdCleanupRecordContext .\n"));


	if(RecordEmulCtx->RecordEntry) {
		xixcore_FreeBuffer(RecordEmulCtx->RecordEntry);
		RecordEmulCtx->RecordEntry = NULL;
	}

	if(RecordEmulCtx->RecordInfo) {
		xixcore_FreeBuffer(RecordEmulCtx->RecordInfo);
		RecordEmulCtx->RecordInfo = NULL;
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdCleanupRecordContext.\n"));

	return XCCODE_SUCCESS;
}



int
xixcore_call
xixcore_LookUpInitializeRecord(
	PXIXCORE_RECORD_EMUL_CONTEXT RecordEmulCtx
)
{
	int		RC = 0;
	xc_int32	Reason = 0;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_LookUpInitializeRecord .\n"));
	
	XIXCORE_ASSERT_VCB(RecordEmulCtx->VCB);

	RecordEmulCtx->RecordIndex = 0;
	RecordEmulCtx->RecordSearchIndex = -1;


	XIXCORE_ASSERT(RecordEmulCtx->RecordEntry);
	XIXCORE_ASSERT(RecordEmulCtx->RecordInfo);
	
	xixcore_ZeroBufferOffset(RecordEmulCtx->RecordInfo);
	memset(xixcore_GetDataBuffer(RecordEmulCtx->RecordInfo), 0, XIDISK_DUP_HOST_INFO_SIZE);

	RC = xixcore_RawReadRegisterHostInfo(
						RecordEmulCtx->VCB->XixcoreBlockDevice,
						RecordEmulCtx->VCB->LotSize,
						RecordEmulCtx->VCB->SectorSize,
						RecordEmulCtx->VCB->SectorSizeBit,
						RecordEmulCtx->VCB->MetaContext.HostRegLotMapIndex,
						RecordEmulCtx->RecordInfo,
						&Reason
						);


	if ( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL , 
			("FAIL  XixFsdLookUpInitializeRecord Get:RecordInfo .\n"));			
	}

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdLookUpInitializeRecord  Status(0x%x).\n", RC));
	
	return RC;
}



int
xixcore_call
xixcore_GetNextRecord(
	IN PXIXCORE_RECORD_EMUL_CONTEXT RecordEmulCtx
)
{
	int						RC = 0;
	PXIDISK_HOST_INFO 		RecordInfo = NULL;
	//PXIDISK_HOST_RECORD	RecordEntry = NULL;
	int						Reason = 0;
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_GetNextRecord.\n"));	


	RecordInfo = (PXIDISK_HOST_INFO)xixcore_GetDataBufferWithOffset(RecordEmulCtx->RecordInfo);
	
		
	RecordEmulCtx->RecordIndex = 
		RecordEmulCtx->RecordSearchIndex = (xc_uint32)xixcore_FindSetBit(
														XIFSD_MAX_HOST, 
														RecordEmulCtx->RecordSearchIndex, 
														RecordInfo->RegisterMap
														);
	
	if(RecordEmulCtx->RecordIndex == XIFSD_MAX_HOST) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail: xixcore_GetNextRecord -->Can't found record.\n"));
		RC = XCCODE_ENOSPC;
		goto error_out;
	}
	xixcore_ZeroBufferOffset(RecordEmulCtx->RecordEntry);
	memset(xixcore_GetDataBuffer(RecordEmulCtx->RecordEntry), 0, XIDISK_DUP_HOST_RECORD_SIZE);

	RC = xixcore_RawReadRegisterRecord(
					RecordEmulCtx->VCB->XixcoreBlockDevice,
					RecordEmulCtx->VCB->LotSize,
					RecordEmulCtx->VCB->SectorSize,
					RecordEmulCtx->VCB->SectorSizeBit,
					RecordEmulCtx->VCB->MetaContext.HostRegLotMapIndex,
					RecordEmulCtx->RecordIndex,
					RecordEmulCtx->RecordEntry,
					&Reason
					);

	if ( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail xixcore_GetNextRecord -->xixcore_RawReadRegisterRecord  Status(0x%x).\n", RC));
	
		goto error_out;
	}

	
error_out:
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit xixcore_RawReadRegisterRecord Status(0x%x).\n", RC));	
	return RC;
}




int
xixcore_call
xixcore_SetNextRecord(
	PXIXCORE_RECORD_EMUL_CONTEXT RecordEmulCtx
)
{
	int						RC = 0;
	int						Reason = 0;
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_SetNextRecord .\n"));		
		

	RC = xixcore_RawWriteRegisterRecord(
					RecordEmulCtx->VCB->XixcoreBlockDevice,
					RecordEmulCtx->VCB->LotSize,
					RecordEmulCtx->VCB->SectorSize,
					RecordEmulCtx->VCB->SectorSizeBit,
					RecordEmulCtx->VCB->MetaContext.HostRegLotMapIndex,
					RecordEmulCtx->RecordIndex,
					RecordEmulCtx->RecordEntry,
					&Reason
					);

	if( RC <0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail xixcore_SetNextRecord --> xixcore_RawWriteRegisterRecord Status(0x%x) Reason(%x).\n", RC, Reason));
		goto error_out;
	}


	RC = xixcore_RawWriteRegisterHostInfo(
					RecordEmulCtx->VCB->XixcoreBlockDevice,
					RecordEmulCtx->VCB->LotSize,
					RecordEmulCtx->VCB->SectorSize,
					RecordEmulCtx->VCB->SectorSizeBit,
					RecordEmulCtx->VCB->MetaContext.HostRegLotMapIndex,
					RecordEmulCtx->RecordInfo,
					&Reason
					);


	if( RC <0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail xixcore_SetNextRecord --> xixcore_RawWriteRegisterHostInfo Status(0x%x) Reason(%x).\n", RC, Reason));
		goto error_out;
	}

error_out:
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdSetNextRecord Status(0x%x).\n", RC));	
	return RC;
}


int
xixcore_call
xixcore_SetHostRecord(
	PXIXCORE_RECORD_EMUL_CONTEXT RecordEmulCtx
)
{
	int	RC = 0;
	PXIXCORE_BITMAP_EMUL_CONTEXT pBitmapEmulCtx;
	PXIXCORE_LOT_MAP		HostFreeMap = NULL;
	PXIXCORE_LOT_MAP		HostDirtyMap = NULL;
	// Added by ILGU HONG 2006 06 12
	PXIXCORE_LOT_MAP		VolumeFreeMap = NULL;
	// Added by ILGU HONG 2006 06 12 End
	PXIXCORE_LOT_MAP		ptempLotMap = NULL;
	PXIXCORE_LOT_MAP 		TempFreeLotMap = NULL;
	PXIDISK_HOST_INFO	HostInfo = NULL;
	PXIDISK_HOST_RECORD HostRecord = NULL; 
	PXIXCORE_VCB			VCB = NULL;
	xc_uint32				size = 0;
	xc_uint32				BuffSize = 0;
	xc_uint32				RecodeIndex = 0;
	xc_uint32				Step = 0;
	xc_int32				Reason = 0;
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter XixFsdSetHostRecord .\n"));	


	VCB = RecordEmulCtx->VCB;
	XIXCORE_ASSERT_VCB(VCB);

	size = SECTOR_ALIGNED_SIZE(VCB->SectorSizeBit, (xc_uint32) ((VCB->NumLots + 7)/8 + sizeof(XIDISK_BITMAP_DATA) -1 ));
	BuffSize = size * 2;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
	(" XixFsdSetHostRecord SectorSizeBit(%d), NumLots(%lld) size(%d) .\n", 
		VCB->SectorSizeBit, VCB->NumLots,  size));	


	pBitmapEmulCtx = (PXIXCORE_BITMAP_EMUL_CONTEXT) xixcore_AllocateMem(sizeof(XIXCORE_BITMAP_EMUL_CONTEXT), 0, XCTAG_BITMAPEMUL);

	if(!pBitmapEmulCtx) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_SetHostRecord -->Not Alloc buffer .\n"));
		return XCCODE_ENOMEM;
	}


	// Zero Bitmap Context
	memset(pBitmapEmulCtx, 0, sizeof(XIXCORE_BITMAP_EMUL_CONTEXT));

	HostInfo = (PXIDISK_HOST_INFO)xixcore_GetDataBufferWithOffset(RecordEmulCtx->RecordInfo);

	RecodeIndex = (xc_uint32)xixcore_FindFreeBit(XIFSD_MAX_HOST, -1, HostInfo->RegisterMap);

	if(RecodeIndex == XIFSD_MAX_HOST){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_SetHostRecord : eXeed host max 64 .\n"));
		RC = XCCODE_ENOSPC;
		goto error_out;
	}
		
	
	
	// allocate host dirty map
	HostDirtyMap = (PXIXCORE_LOT_MAP)xixcore_AllocateMem(sizeof(XIXCORE_LOT_MAP), 0, XCTAG_HOSTDIRTYMAP);
	if(!HostDirtyMap){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_SetHostRecord -->Not Alloc buffer .\n"));
		RC = XCCODE_ENOMEM;
		goto error_out;
	}

	memset(HostDirtyMap, 0, sizeof(XIXCORE_LOT_MAP));

	HostDirtyMap->Data = xixcore_AllocateBuffer(BuffSize);

	if(!HostDirtyMap->Data) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_SetHostRecord -->Not Alloc buffer .\n"));
		RC = XCCODE_ENOMEM;
		goto error_out;			
	}
	
	memset(xixcore_GetDataBuffer(HostDirtyMap->Data), 0,  BuffSize);



	// allocate host free map
	HostFreeMap = (PXIXCORE_LOT_MAP)xixcore_AllocateMem(sizeof(XIXCORE_LOT_MAP), 0, XCTAG_HOSTFREEMAP);
	if(!HostFreeMap){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_SetHostRecord -->Not Alloc buffer .\n"));
		RC = XCCODE_ENOMEM;
		goto error_out;
	}
	
	memset(HostFreeMap, 0, sizeof(XIXCORE_LOT_MAP));


	HostFreeMap->Data = xixcore_AllocateBuffer(BuffSize);

	if(!HostFreeMap->Data) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_SetHostRecord -->Not Alloc buffer .\n"));
		RC = XCCODE_ENOMEM;
		goto error_out;			
	}
	

	memset(xixcore_GetDataBuffer(HostFreeMap->Data), 0,  BuffSize);



	
	//	Added by ILGU HONG 2006 06 12
	VolumeFreeMap = (PXIXCORE_LOT_MAP)xixcore_AllocateMem(sizeof(XIXCORE_LOT_MAP), 0, XCTAG_VOLFREEMAP);
	if(!VolumeFreeMap){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_SetHostRecord -->Not Alloc buffer .\n"));
		RC = XCCODE_ENOMEM;
		goto error_out;
	}
	
	memset(VolumeFreeMap, 0, sizeof(XIXCORE_LOT_MAP));

	VolumeFreeMap->Data = xixcore_AllocateBuffer(BuffSize);

	if(!VolumeFreeMap->Data) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_SetHostRecord -->Not Alloc buffer .\n"));
		RC = XCCODE_ENOMEM;
		goto error_out;			
	}
	

	memset(xixcore_GetDataBuffer(VolumeFreeMap->Data), 0,  BuffSize);
	//	Added by ILGU HONG 2006 06 12 End


	
	TempFreeLotMap = (PXIXCORE_LOT_MAP)xixcore_AllocateMem(sizeof(XIXCORE_LOT_MAP),0,XCTAG_TEMPFREEMAP);

	if(!TempFreeLotMap){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_SetHostRecord -->Not Alloc buffer .\n"));
		RC = XCCODE_ENOMEM;
		goto error_out;
	}


	memset(TempFreeLotMap, 0, sizeof(XIXCORE_LOT_MAP));

	TempFreeLotMap->Data = xixcore_AllocateBuffer(BuffSize);

	if(!TempFreeLotMap->Data) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail: xixcore_SetHostRecord -->Not Alloc buffer .\n"));
		RC = XCCODE_ENOMEM;
		goto error_out;			
	}
		

	memset(xixcore_GetDataBuffer(TempFreeLotMap->Data), 0,  BuffSize);



	RC = xixcore_InitializeBitmapContext(
							pBitmapEmulCtx,
							RecordEmulCtx->VCB,
							HostInfo->UsedLotMapIndex,
							HostInfo->UnusedLotMapIndex,
							HostInfo->CheckOutLotMapIndex);

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  XifsdInitializeBitmapContext (0x%x) .\n", RC));
		goto error_out;
	}





	RC = xixcore_ReadFreemapFromBitmapContext(pBitmapEmulCtx);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  XifsdReadFreemapFromBitmapContext (0x%x) .\n", RC));
		goto error_out;			
	}


	RC = xixcore_ReadCheckoutmapFromBitmapContext(pBitmapEmulCtx);

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
		goto error_out;			
	}

		
	// Get Real FreeMap without CheckOut
	xixcore_XORMap((PXIXCORE_LOT_MAP)&(pBitmapEmulCtx->UnusedBitmap), 
					(PXIXCORE_LOT_MAP)&(pBitmapEmulCtx->CheckOutBitmap));
	

	// Initialize HostFree/HostDirty
	ptempLotMap = (PXIXCORE_LOT_MAP)&(pBitmapEmulCtx->UnusedBitmap);
	TempFreeLotMap->BitMapBytes = HostDirtyMap->BitMapBytes = HostFreeMap->BitMapBytes = ptempLotMap->BitMapBytes;
	TempFreeLotMap->MapType = HostDirtyMap->MapType = HostFreeMap->MapType = ptempLotMap->MapType;
	TempFreeLotMap->NumLots = HostDirtyMap->NumLots = HostFreeMap->NumLots = ptempLotMap->NumLots;	

	RC = xixcore_SetCheckOutLotMap((PXIXCORE_LOT_MAP)&(pBitmapEmulCtx->UnusedBitmap), TempFreeLotMap, RecodeIndex + 1);
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail  SetCheckOutLotMap Status (0x%x) .\n", RC));
		goto error_out;
	}

	HostDirtyMap->StartIndex = HostFreeMap->StartIndex;


	//Update CheckOut LotMap --> include Host aligned bit map
	xixcore_ORMap((PXIXCORE_LOT_MAP)&(pBitmapEmulCtx->CheckOutBitmap), TempFreeLotMap);
	
	RC = xixcore_WriteCheckoutmapFromBitmapContext(pBitmapEmulCtx);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  XifsdReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
		goto error_out;			
	}

	xixcore_ORMap(HostFreeMap, TempFreeLotMap);
		
		
	Step = 1;	
	
	VCB->MetaContext.HostFreeLotMap = HostFreeMap;
	VCB->MetaContext.HostDirtyLotMap = HostDirtyMap;

	//	Added by ILGU HONG	2006 06 12
	VolumeFreeMap->BitMapBytes = ((PXIXCORE_LOT_MAP)&(pBitmapEmulCtx->UnusedBitmap))->BitMapBytes;
	VolumeFreeMap->MapType = ((PXIXCORE_LOT_MAP)&(pBitmapEmulCtx->UnusedBitmap))->MapType;
	VolumeFreeMap->NumLots = ((PXIXCORE_LOT_MAP)&(pBitmapEmulCtx->UnusedBitmap))->NumLots;
	VolumeFreeMap->StartIndex = ((PXIXCORE_LOT_MAP)&(pBitmapEmulCtx->UnusedBitmap))->StartIndex;
	memcpy(xixcore_GetDataBufferOfBitMap(VolumeFreeMap->Data), xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UnusedBitmap.Data), VolumeFreeMap->BitMapBytes);
	VCB->MetaContext.VolumeFreeMap	= VolumeFreeMap;
	//	Added by ILGU HONG	2006 06 12 End


	HostFreeMap = NULL;
	HostDirtyMap = NULL;

	// Alloc Host specified map
	VCB->MetaContext.HostUsedLotMapIndex = xixcore_AllocVCBLot(VCB);
	VCB->MetaContext.HostUnUsedLotMapIndex = xixcore_AllocVCBLot(VCB);
	VCB->MetaContext.HostCheckOutLotMapIndex = xixcore_AllocVCBLot(VCB);


	// Write Host Check Out
	RC = xixcore_WriteBitMapWithBuffer(
						VCB,
						(xc_sector_t)VCB->MetaContext.HostCheckOutLotMapIndex, 
						TempFreeLotMap, 
						pBitmapEmulCtx->LotHeader,
						pBitmapEmulCtx->BitmapLotHeader,
						1);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  WriteBitMap for Checkout (0x%x) .\n", RC));
		goto error_out;			
	}		



	RC = xixcore_WriteBitMapWithBuffer(
						VCB,
						(xc_sector_t)VCB->MetaContext.HostUnUsedLotMapIndex, 
						VCB->MetaContext.HostFreeLotMap,
						pBitmapEmulCtx->LotHeader,
						pBitmapEmulCtx->BitmapLotHeader,
						1);

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  WriteBitMap for Free (0x%x) .\n", RC));
		goto error_out;			
	}		


	RC = xixcore_WriteBitMapWithBuffer(VCB,
						(xc_sector_t)VCB->MetaContext.HostUsedLotMapIndex, 
						VCB->MetaContext.HostDirtyLotMap,
						pBitmapEmulCtx->LotHeader,
						pBitmapEmulCtx->BitmapLotHeader,
						1);

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  WriteBitMap for Free (0x%x) .\n", RC));
		goto error_out;			
	}		


	Step =2;





	// Set Host Record
	VCB->MetaContext.HostRecordIndex = RecodeIndex;	
	RecordEmulCtx->RecordIndex = RecodeIndex;

	xixcore_ZeroBufferOffset(RecordEmulCtx->RecordEntry);
	memset(xixcore_GetDataBuffer(RecordEmulCtx->RecordEntry),0, xixcore_GetBufferSize(RecordEmulCtx->RecordEntry));

	RC = xixcore_RawReadRegisterRecord(
					RecordEmulCtx->VCB->XixcoreBlockDevice,
					RecordEmulCtx->VCB->LotSize,
					RecordEmulCtx->VCB->SectorSize,
					RecordEmulCtx->VCB->SectorSizeBit,
					RecordEmulCtx->VCB->MetaContext.HostRegLotMapIndex,
					RecordEmulCtx->RecordIndex,
					RecordEmulCtx->RecordEntry,
					&Reason
					);

	if( RC <0 ){
		if(RC == XCCODE_CRCERROR) {
			xixcore_ZeroBufferOffset(RecordEmulCtx->RecordEntry);
			memset(xixcore_GetDataBuffer(RecordEmulCtx->RecordEntry),0, xixcore_GetBufferSize(RecordEmulCtx->RecordEntry));

		}else{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail xixcore_SetNextRecord --> xixcore_RawWriteRegisterRecord Status(0x%x) Reason(%x).\n", RC, Reason));
			goto error_out;
		}
	}


	xixcore_SetBit(RecodeIndex, (unsigned long *)HostInfo->RegisterMap);
	HostInfo->NumHost ++;

	// UpdateHostInfo
	HostRecord = (PXIDISK_HOST_RECORD)xixcore_GetDataBufferWithOffset(RecordEmulCtx->RecordEntry);
	memset(HostRecord, 0, (XIDISK_HOST_RECORD_SIZE - XIXCORE_MD5DIGEST_AND_SEQSIZE));
	HostRecord->HostCheckOutLotMapIndex = VCB->MetaContext.HostCheckOutLotMapIndex;
	HostRecord->HostUnusedLotMapIndex = VCB->MetaContext.HostUnUsedLotMapIndex;
	HostRecord->HostUsedLotMapIndex = VCB->MetaContext.HostUsedLotMapIndex;
	HostRecord->HostMountTime = xixcore_GetCurrentTime64();	
	memcpy(HostRecord->HostSignature, RecordEmulCtx->HostSignature, 16);
	HostRecord->HostState = HOST_MOUNT;			
	

	RC = xixcore_SetNextRecord(RecordEmulCtx);

	if( RC <0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail(0x%x) XifsdSetNextRecord .\n", RC));
		goto error_out;
	}		

	RC = 0;

error_out:
	
	if( RC < 0){
		if(HostDirtyMap){

			if(HostDirtyMap->Data) {
				xixcore_FreeBuffer(HostDirtyMap->Data);
			}
			
			xixcore_FreeMem(HostDirtyMap, XCTAG_HOSTDIRTYMAP);
			HostDirtyMap = NULL;
		}

		if(HostFreeMap){
			
			if(HostFreeMap->Data) {
				xixcore_FreeBuffer(HostFreeMap->Data);
			}
			
			xixcore_FreeMem(HostFreeMap, XCTAG_HOSTFREEMAP);
			HostFreeMap = NULL;
		}
	}

	if(TempFreeLotMap) {
		
		if(TempFreeLotMap->Data) {
			xixcore_FreeBuffer(TempFreeLotMap->Data);
		}
		
		xixcore_FreeMem(TempFreeLotMap, XCTAG_TEMPFREEMAP);
		TempFreeLotMap = NULL;
	}

	if(pBitmapEmulCtx) {
		xixcore_CleanupBitmapContext(pBitmapEmulCtx);
		xixcore_FreeMem(pBitmapEmulCtx, XCTAG_BITMAPEMUL);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdSetHostRecord Status(0x%x).\n", RC));	



	return RC;
}




int
xixcore_call
xixcore_CheckAndInvalidateHost(
	PXIXCORE_RECORD_EMUL_CONTEXT 	RecordEmulCtx,
	xc_uint32							*IsLockHead
)
{
	int							RC = 0;
	PXIXCORE_BITMAP_EMUL_CONTEXT 	pBitmapEmulCtx = NULL;
	PXIXCORE_BITMAP_EMUL_CONTEXT 	pDiskBitmapEmulCtx = NULL;


	PXIDISK_HOST_INFO	HostInfo = NULL;
	PXIDISK_HOST_RECORD HostRecord = NULL; 
	PXIXCORE_VCB			VCB = NULL;
	//xc_uint32				size = 0;	
	xc_uint32				RecordIndex = 0;
	//xc_uint32				Step = 0;
	xc_uint32				bAcqLotLock = 0;



	
	VCB = RecordEmulCtx->VCB;
	XIXCORE_ASSERT_VCB(VCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixcore_CheckAndInvalidateHost .\n"));

	
	pBitmapEmulCtx = (PXIXCORE_BITMAP_EMUL_CONTEXT)xixcore_AllocateMem(sizeof(XIXCORE_BITMAP_EMUL_CONTEXT), 0, XCTAG_BITMAPEMUL);

	if(!pBitmapEmulCtx) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost->allocate pBitmapEmulCtx  .\n"));

		return XCCODE_ENOMEM;
	}

	memset(pBitmapEmulCtx, 0, sizeof(XIXCORE_BITMAP_EMUL_CONTEXT));


	pDiskBitmapEmulCtx= (PXIXCORE_BITMAP_EMUL_CONTEXT)xixcore_AllocateMem(sizeof(XIXCORE_BITMAP_EMUL_CONTEXT), 0, XCTAG_BITMAPEMUL);

	if(!pDiskBitmapEmulCtx) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost-> allocate pDiskBitmapEmulCtx  .\n"));

		RC = XCCODE_ENOMEM;
		goto error_out;
	}

	memset(pDiskBitmapEmulCtx, 0, sizeof(XIXCORE_BITMAP_EMUL_CONTEXT));
	




	// Save Record Index
	RecordIndex = RecordEmulCtx->RecordIndex;

	// Initialize Host Bitmap Context
	HostRecord = (PXIDISK_HOST_RECORD)xixcore_GetDataBufferWithOffset(RecordEmulCtx->RecordEntry);
	
	

	RC =xixcore_InitializeBitmapContext(
									pBitmapEmulCtx,
									RecordEmulCtx->VCB,
									HostRecord->HostUsedLotMapIndex,
									HostRecord->HostUnusedLotMapIndex,
									HostRecord->HostCheckOutLotMapIndex
									);
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_InitializeBitmapContext(0x%x) .\n", RC));
		goto error_out;
	}





	// Read Host Bitmap information

	RC = xixcore_ReadDirmapFromBitmapContext(pBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_ReadDirmapFromBitmapContext(0x%x) .\n", RC));
		goto error_out;
	}
	
	/*	
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UsedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Before Validate Host Dirty Bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/


	RC = xixcore_ReadFreemapFromBitmapContext(pBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_ReadFreemapFromBitmapContext(0x%x) .\n", RC));
		goto error_out;
	}

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UnusedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Before Validate Host Free Bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/

	RC = xixcore_ReadCheckoutmapFromBitmapContext(pBitmapEmulCtx);

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_ReadCheckoutmapFromBitmapContext(0x%x) .\n", RC));
		goto error_out;
	}

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->CheckOutBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Before Validate Host CheckOut Bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/




	RC = xixcore_AuxLotUnLock(
		VCB,
		VCB->MetaContext.HostRegLotMapIndex
		);	


	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_AuxLotUnLock(0x%x) .\n", RC));
		goto error_out;
	}


	*IsLockHead = 0;
	bAcqLotLock = 0;

	// Check Host Dirty map information and make new Host Free Bitmap 

	RC = xixcore_CheckFileDirFromBitMapContext(VCB, pBitmapEmulCtx);

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_CheckFileDirFromBitMapContext (0x%x) .\n", RC));
		goto error_out;
	}

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UsedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Validate Host Dirty Bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	

	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UnusedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Validate Host Free Bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}		
	*/


	RC = xixcore_AuxLotLock(
		VCB,
		VCB->MetaContext.HostRegLotMapIndex, 
		1,
		1
		);

	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_AuxLotLock (0x%x) .\n", RC));
		goto error_out;
	}
		
		

	bAcqLotLock = 1;
	*IsLockHead = 1;



	// Initialize Disk Bitmap Context
	HostInfo = (PXIDISK_HOST_INFO)xixcore_GetDataBufferWithOffset(RecordEmulCtx->RecordInfo);
	RC = xixcore_InitializeBitmapContext(
									pDiskBitmapEmulCtx,
									RecordEmulCtx->VCB,
									HostInfo->UsedLotMapIndex,
									HostInfo->UnusedLotMapIndex,
									HostInfo->CheckOutLotMapIndex);

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_InitializeBitmapContext(0x%x) .\n", RC));
		goto error_out;
	}
		

	// Update Disk Free bitmap , dirty map  and Checkout Bitmap from free Bitmap
	
	RC = xixcore_ReadDirmapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_ReadDirmapFromBitmapContext (0x%x) .\n", RC));
		goto error_out;
	}

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UsedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Befor Validate Disk Dirty Bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}		
	*/



	RC = xixcore_ReadFreemapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_ReadFreemapFromBitmapContext (0x%x) .\n", RC));
		goto error_out;
	}

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UnusedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Befor Validate Disk Free Bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}		
	*/

	RC = xixcore_ReadCheckoutmapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_ReadCheckoutmapFromBitmapContext (0x%x) .\n", RC));
		goto error_out;
	}
	
	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->CheckOutBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Befor Validate Disk CheckOut Bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}			
	*/


	xixcore_ClearBit(HostRecord->HostUsedLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UsedBitmap.Data));
	xixcore_SetBit(HostRecord->HostUsedLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UnusedBitmap.Data));
	xixcore_ClearBit(HostRecord->HostUnusedLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UsedBitmap.Data));
	xixcore_SetBit(HostRecord->HostUnusedLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UnusedBitmap.Data));
	xixcore_ClearBit(HostRecord->HostCheckOutLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UsedBitmap.Data));
	xixcore_SetBit(HostRecord->HostCheckOutLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(pBitmapEmulCtx->UnusedBitmap.Data));

	

	xixcore_XORMap(&(pDiskBitmapEmulCtx->CheckOutBitmap), 
							&(pBitmapEmulCtx->CheckOutBitmap));

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->CheckOutBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Disk Check Out bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/


	xixcore_ORMap(&(pDiskBitmapEmulCtx->UsedBitmap), 
							&(pBitmapEmulCtx->UsedBitmap));

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UsedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Disk Dirty bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/

	xixcore_XORMap(&(pDiskBitmapEmulCtx->UnusedBitmap), 
							&(pBitmapEmulCtx->UsedBitmap));

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UnusedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Disk Free bitmap 1 \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/



	xixcore_ORMap(&(pDiskBitmapEmulCtx->UnusedBitmap), &(pBitmapEmulCtx->UnusedBitmap));

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data = NULL;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UnusedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Disk Free bitmap 2 \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/




	// Invalidate Host Bitmap

	xixcore_InvalidateDirtyBitMap(pBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_InvalidateDirtyBitMap(0x%x) .\n", RC));
		goto error_out;
	}


	xixcore_InvalidateFreeBitMap(pBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_InvalidateFreeBitMap (0x%x) .\n", RC));
		goto error_out;
	}



	xixcore_InvalidateCheckOutBitMap(pBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_InvalidateCheckOutBitMap(0x%x) .\n", RC));
		goto error_out;
	}


	// Update Information
	RC = xixcore_WriteDirmapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_WriteDirmapFromBitmapContext (0x%x) .\n", RC));
		goto error_out;
	}

		

	RC = xixcore_WriteFreemapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_WriteFreemapFromBitmapContext (0x%x) .\n", RC));
		goto error_out;
	}



	RC = xixcore_WriteCheckoutmapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_WriteCheckoutmapFromBitmapContext (0x%x) .\n", RC));
		goto error_out;
	}



		
	//Update Record
	xixcore_ClearBit(RecordIndex, (unsigned long *)HostInfo->RegisterMap);
	HostInfo->NumHost --;

	RecordEmulCtx->RecordIndex = RecordIndex;	

	// UpdateHostInfo
	HostRecord = (PXIDISK_HOST_RECORD)xixcore_GetDataBufferWithOffset(RecordEmulCtx->RecordEntry);
	memset(HostRecord, 0, (XIDISK_HOST_RECORD_SIZE - XIXCORE_MD5DIGEST_AND_SEQSIZE));
	HostRecord->HostCheckOutLotMapIndex = 0;
	HostRecord->HostUnusedLotMapIndex = 0;
	HostRecord->HostUsedLotMapIndex = 0;
	HostRecord->HostMountTime = xixcore_GetCurrentTime64();	
	memcpy(HostRecord->HostSignature, RecordEmulCtx->HostSignature, 16);
	HostRecord->HostState = HOST_UMOUNT;			

	

	RC = xixcore_SetNextRecord(RecordEmulCtx);

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixcore_CheckAndInvalidateHost ->xixcore_SetNextRecord (0x%x) .\n", RC));
		goto error_out;
	}

	RC = 0;

error_out:

	if(pBitmapEmulCtx){
		xixcore_CleanupBitmapContext(pBitmapEmulCtx);
		xixcore_FreeMem(pBitmapEmulCtx, XCTAG_BITMAPEMUL);
		pBitmapEmulCtx = NULL;
		
	}

	if(pDiskBitmapEmulCtx) {
		xixcore_CleanupBitmapContext(pDiskBitmapEmulCtx);
		xixcore_FreeMem(pDiskBitmapEmulCtx, XCTAG_BITMAPEMUL);
		pDiskBitmapEmulCtx = NULL;
	}


	*IsLockHead = bAcqLotLock;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XifsdCheckAndInvalidateHost .\n"));
	return RC;
}


int
xixcore_call
xixcore_RegisterHost(
	PXIXCORE_VCB	pVCB
	)
{
	int RC = 0;
	XIXCORE_RECORD_EMUL_CONTEXT	RecordEmulCtx;
	PXIDISK_HOST_INFO			HostInfo =NULL;
	PXIDISK_HOST_RECORD		HostRecord = NULL;	
	xc_uint32						bLockHeld = 0;

	XIXCORE_ASSERT_VCB(pVCB);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixcore_RegisterHost .\n"));

	RC = xixcore_AuxLotLock(
			pVCB, 
			pVCB->MetaContext.HostRegLotMapIndex,
			1, 
			1
			);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) xixcore_RegisterHost : xixcore_AuxLotLock .\n", RC));
		return RC;		
	}

	bLockHeld = 1;
	

	RC = xixcore_InitializeRecordContext(&RecordEmulCtx, pVCB, pVCB->HostId);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Fail(0x%x)xixcore_RegisterHost -->  xixcore_InitializeRecordContext .\n", RC));
		goto error_out;
	}
		
	RC = xixcore_LookUpInitializeRecord(&RecordEmulCtx);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Fail(0x%x) xixcore_RegisterHost --> xixcore_LookUpInitializeRecord.\n", RC));
		goto error_out;
	}


	// Set BitMap Lot address information
	HostInfo = (PXIDISK_HOST_INFO)xixcore_GetDataBufferWithOffset(RecordEmulCtx.RecordInfo);
	pVCB->MetaContext.AllocatedLotMapIndex = HostInfo->UsedLotMapIndex;
	pVCB->MetaContext.FreeLotMapIndex = HostInfo->UnusedLotMapIndex;
	pVCB->MetaContext.CheckOutLotMapIndex = HostInfo->CheckOutLotMapIndex;	


	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
		("HostInfo AllocIndex (%lld) FreeIndex (%lld) CheckIndex(%lld) : NumHost(%d).\n", 
		HostInfo->UsedLotMapIndex,
		HostInfo->UnusedLotMapIndex,
		HostInfo->CheckOutLotMapIndex,
		HostInfo->NumHost
		));		


	//// Check Record for Error recovery
	if(HostInfo->NumHost !=0){			
		do{
			RC = xixcore_GetNextRecord(&RecordEmulCtx);
			if( RC >= 0){

				HostRecord = (PXIDISK_HOST_RECORD)xixcore_GetDataBufferWithOffset(RecordEmulCtx.RecordEntry);
				
				if((memcmp(HostRecord->HostSignature, pVCB->HostId, 16) == 0)
					&& (HostRecord->HostState != HOST_UMOUNT ))
				{

					
					RC = xixcore_CheckAndInvalidateHost(&RecordEmulCtx, &bLockHeld);

					if(RC <0 ){
						//call check out routine
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
							("Fail(0x%x)xixcore_RegisterHost  --> xixcore_CheckAndInvalidateHost .\n", RC));
						goto error_out;
					}
					break;	
				}				
			}

		}while( RC >= 0);
	}



	RC = xixcore_SetHostRecord(&RecordEmulCtx);
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Fail(0x%x) xixcore_RegisterHost -->xixcore_SetHostRecord  .\n", RC));
		goto error_out;
	}


	

	RC = 0;


	


error_out:


	
	
	xixcore_CleanupRecordContext(&RecordEmulCtx);
	
	if(bLockHeld == 1) {
		xixcore_AuxLotUnLock(
			pVCB,
			pVCB->MetaContext.HostRegLotMapIndex
			);	
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit xixcore_RegisterHost Status(0x%x).\n", RC));

	return RC;
}






int
xixcore_call
xixcore_DeregisterHost(
	PXIXCORE_VCB VCB
	)
{
	int								RC = 0;
	XIXCORE_RECORD_EMUL_CONTEXT		RecordEmulCtx ;
	PXIXCORE_BITMAP_EMUL_CONTEXT	pDiskBitmapEmulCtx = NULL;
	PXIDISK_HOST_INFO 				HostInfo = NULL;
	PXIDISK_HOST_RECORD				HostRecord = NULL;	
	XIXCORE_LOT_MAP 				TempFreeLotMap ;
	xc_uint32						size = 0;
	xc_uint32						BuffSize = 0;
	xc_int32						Reason = 0;
	
	XIXCORE_ASSERT_VCB(VCB);

	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixcore_DeregisterHost.\n"));

	memset(&RecordEmulCtx, 0 , sizeof(XIXCORE_RECORD_EMUL_CONTEXT));

	RC = xixcore_LotLock(
		VCB,
		VCB->MetaContext.HostRegLotMapIndex,
		&VCB->MetaContext.HostRegLotMapLockStatus,
		1,
		1
		);

	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) xixcore_DeregisterHost: xixcore_LotLock .\n", RC));
		return RC;
	}

	
	pDiskBitmapEmulCtx = xixcore_AllocateMem(sizeof(XIXCORE_BITMAP_EMUL_CONTEXT), 0, XCTAG_BITMAPEMUL);

	if(pDiskBitmapEmulCtx == NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail : xixcore_DeregisterHost: can't alloc pDiskBitmapEmulCtx.\n"));

		RC = XCCODE_ENOMEM;

		goto error_out;
	
	}


	// Zero Bit map context;
	memset(pDiskBitmapEmulCtx, 0, sizeof(XIXCORE_BITMAP_EMUL_CONTEXT));
	






	// Read Record Lot header Info
	RC = xixcore_InitializeRecordContext(&RecordEmulCtx, VCB, VCB->HostId);
	
	if ( RC< 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Fail(0x%x) xixcore_DeregisterHost: xixcore_InitializeRecordContext .\n", RC));
		goto error_out;
	}


	memset(&TempFreeLotMap, 0, sizeof(XIXCORE_LOT_MAP));
	
	size = SECTOR_ALIGNED_SIZE(VCB->SectorSizeBit, (xc_uint32) ((VCB->NumLots + 7)/8 + sizeof(XIDISK_BITMAP_DATA) -1));
	BuffSize = size * 2;

	TempFreeLotMap.Data = xixcore_AllocateBuffer(BuffSize);

	if(TempFreeLotMap.Data  == NULL){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL:xixcore_DeregisterHost--> Allocate TempFreeLotMap .\n"));
		RC = XCCODE_ENOMEM;
		goto error_out;
	}


	memset(xixcore_GetDataBuffer(TempFreeLotMap.Data), 0, BuffSize);

	RC = xixcore_LookUpInitializeRecord(&RecordEmulCtx);

	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Fail(0x%x) xixcore_DeregisterHost: xixcore_LookUpInitializeRecord .\n", RC));
		goto error_out;
	}

	// Set BitMap Lot address information
	HostInfo = (PXIDISK_HOST_INFO)xixcore_GetDataBufferWithOffset(RecordEmulCtx.RecordInfo);
	VCB->MetaContext.AllocatedLotMapIndex = HostInfo->UsedLotMapIndex;
	VCB->MetaContext.FreeLotMapIndex = HostInfo->UnusedLotMapIndex;
	VCB->MetaContext.CheckOutLotMapIndex = HostInfo->CheckOutLotMapIndex;



	// Read Disk Bitmap information
	RC = xixcore_InitializeBitmapContext(
							pDiskBitmapEmulCtx,
							VCB,
							HostInfo->UsedLotMapIndex,
							HostInfo->UnusedLotMapIndex,
							HostInfo->CheckOutLotMapIndex);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL(0x%x) xixcore_DeregisterHost:  xixcore_InitializeBitmapContext.\n", RC));
		goto error_out;
	}

	

	// Update Disk Free bitmap , dirty map  and Checkout Bitmap from free Bitmap
	
	RC = xixcore_ReadDirmapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:  xixcore_ReadDirmapFromBitmapContext  .\n", RC));
		goto error_out;		
	}
	
	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UsedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk Dirty Bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/

	RC = xixcore_ReadFreemapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:  xixcore_ReadFreemapFromBitmapContext  .\n", RC));
		goto error_out;		
	}

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UnusedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk Free bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/

	RC = xixcore_ReadCheckoutmapFromBitmapContext(pDiskBitmapEmulCtx);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:  xixcore_ReadCheckoutmapFromBitmapContext  .\n", RC));
		goto error_out;		
	}

	/*
	{
		xc_uint32 i = 0;
		xc_uint8	*Data;
		Data =  xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->CheckOutBitmap.Data);
		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Disk Check Out bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/

	RC = xixcore_ReadBitMapWithBuffer(
					VCB, 
					(xc_sector_t)VCB->MetaContext.HostCheckOutLotMapIndex, 
					&TempFreeLotMap, 
					pDiskBitmapEmulCtx->LotHeader,
					pDiskBitmapEmulCtx->BitmapLotHeader
					);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:  xixcore_ReadBitMapWithBuffer  .\n", RC));
		goto error_out;			
	}

	/*
	{
		xc_uint32 i = 0;
		xc_uint8	*Data;
		Data = xixcore_GetDataBufferOfBitMap(TempFreeLotMap.Data);
		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,("Host Check Out bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/




	xixcore_ClearBit(VCB->MetaContext.HostUnUsedLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(VCB->MetaContext.HostDirtyLotMap->Data));
	xixcore_SetBit(VCB->MetaContext.HostUnUsedLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(VCB->MetaContext.HostFreeLotMap->Data));
	xixcore_ClearBit(VCB->MetaContext.HostUsedLotMapIndex,(unsigned long *) xixcore_GetDataBufferOfBitMap(VCB->MetaContext.HostDirtyLotMap->Data));
	xixcore_SetBit(VCB->MetaContext.HostUsedLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(VCB->MetaContext.HostFreeLotMap->Data));
	xixcore_ClearBit(VCB->MetaContext.HostCheckOutLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(VCB->MetaContext.HostDirtyLotMap->Data));
	xixcore_SetBit(VCB->MetaContext.HostCheckOutLotMapIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(VCB->MetaContext.HostFreeLotMap->Data));

	
	xixcore_XORMap(&pDiskBitmapEmulCtx->CheckOutBitmap, &TempFreeLotMap);

	/*
	{
		xc_uint32 i = 0;
		xc_uint8	*Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->CheckOutBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Disk Check Out bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/


	xixcore_ORMap(&pDiskBitmapEmulCtx->UsedBitmap, VCB->MetaContext.HostDirtyLotMap);

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UsedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Disk Dirty bitmap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/

	xixcore_XORMap(&pDiskBitmapEmulCtx->UnusedBitmap, VCB->MetaContext.HostDirtyLotMap);

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UnusedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Disk Free bitmap 1 \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/


	xixcore_ORMap(&pDiskBitmapEmulCtx->UnusedBitmap, VCB->MetaContext.HostFreeLotMap);

	/*
	{
		xc_uint32 i = 0;
		xc_uint8 	*Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UnusedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Disk Free bitmap 2 \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/




	// Update Disk Information
	RC = xixcore_WriteDirmapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:   xixcore_WriteDirmapFromBitmapContext .\n", RC));
		goto error_out;			
	}
	

	RC = xixcore_WriteFreemapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:   xixcore_WriteFreemapFromBitmapContext .\n", RC));
		goto error_out;			
	}


	RC = xixcore_WriteCheckoutmapFromBitmapContext(pDiskBitmapEmulCtx);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:   xixcore_WriteCheckoutmapFromBitmapContext .\n", RC));
		goto error_out;			
	}


	// Invalidate Host Bitmap

	RC = xixcore_InvalidateLotBitMapWithBuffer(
					VCB,
					(xc_sector_t)VCB->MetaContext.HostUnUsedLotMapIndex, 
					pDiskBitmapEmulCtx->LotHeader
					);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:   xixcore_InvalidateLotBitMapWithBuffer .\n", RC));
		goto error_out;			
	}


	RC = xixcore_InvalidateLotBitMapWithBuffer(
					VCB,
					(xc_sector_t)VCB->MetaContext.HostUnUsedLotMapIndex, 
					pDiskBitmapEmulCtx->LotHeader
					);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:   xixcore_InvalidateLotBitMapWithBuffer .\n", RC));
		goto error_out;			
	}



	RC = xixcore_InvalidateLotBitMapWithBuffer(
					VCB,
					(xc_sector_t)VCB->MetaContext.HostCheckOutLotMapIndex, 
					pDiskBitmapEmulCtx->LotHeader
					);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:   xixcore_InvalidateLotBitMapWithBuffer .\n", RC));
		goto error_out;			
	}


	
	//Update Record
	xixcore_ClearBit(VCB->MetaContext.HostRecordIndex, (unsigned long *)HostInfo->RegisterMap);
	HostInfo->NumHost --;


	RecordEmulCtx.RecordIndex = VCB->MetaContext.HostRecordIndex;	

	xixcore_ZeroBufferOffset(RecordEmulCtx.RecordEntry);
	memset(xixcore_GetDataBuffer(RecordEmulCtx.RecordEntry),0, xixcore_GetBufferSize(RecordEmulCtx.RecordEntry));

	RC = xixcore_RawReadRegisterRecord(
					RecordEmulCtx.VCB->XixcoreBlockDevice,
					RecordEmulCtx.VCB->LotSize,
					RecordEmulCtx.VCB->SectorSize,
					RecordEmulCtx.VCB->SectorSizeBit,
					RecordEmulCtx.VCB->MetaContext.HostRegLotMapIndex,
					RecordEmulCtx.RecordIndex,
					RecordEmulCtx.RecordEntry,
					&Reason
					);

	if( RC <0 ){
		if(RC == XCCODE_CRCERROR) {
			xixcore_ZeroBufferOffset(RecordEmulCtx.RecordEntry);
			memset(xixcore_GetDataBuffer(RecordEmulCtx.RecordEntry),0, xixcore_GetBufferSize(RecordEmulCtx.RecordEntry));

		}else{

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail xixcore_DeregisterHost --> xixcore_RawWriteRegisterRecord Status(0x%x) Reason(%x).\n", RC, Reason));
			goto error_out;
		}
	}


	// UpdateHostInfo
	HostRecord = (PXIDISK_HOST_RECORD)xixcore_GetDataBufferWithOffset(RecordEmulCtx.RecordEntry);
	memset(HostRecord, 0, XIDISK_HOST_RECORD_SIZE - XIXCORE_MD5DIGEST_AND_SEQSIZE);
	HostRecord->HostCheckOutLotMapIndex = 0;
	HostRecord->HostUnusedLotMapIndex = 0;
	HostRecord->HostUsedLotMapIndex = 0;
	HostRecord->HostMountTime = xixcore_GetCurrentTime64();	
	memcpy(HostRecord->HostSignature, RecordEmulCtx.HostSignature, 16);
	HostRecord->HostState = HOST_UMOUNT;			



	RC = xixcore_SetNextRecord(&RecordEmulCtx);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL (0x%x) xixcore_DeregisterHost:   xixcore_SetNextRecord .\n", RC));
		goto error_out;			
	}




error_out:
	
	xixcore_LotUnLock(
		VCB,
		VCB->MetaContext.HostRegLotMapIndex,
		&VCB->MetaContext.HostRegLotMapLockStatus
		);	
	

	if(TempFreeLotMap.Data) {
		xixcore_FreeBuffer(TempFreeLotMap.Data);
		TempFreeLotMap.Data = NULL;
	}

	if(pDiskBitmapEmulCtx) {
		xixcore_CleanupBitmapContext(pDiskBitmapEmulCtx);
		xixcore_FreeMem(pDiskBitmapEmulCtx, XCTAG_BITMAPEMUL);
	}

	xixcore_CleanupRecordContext(&RecordEmulCtx);
		

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit xixcore_DeregisterHost Status (0x%x).\n", RC));

	return RC;
}



int
xixcore_call
xixcore_IsSingleHost(
	PXIXCORE_VCB VCB
	)
{
	int								RC = 0;
	XIXCORE_RECORD_EMUL_CONTEXT		RecordEmulCtx ;
	PXIDISK_HOST_INFO 				HostInfo = NULL;
	
	
	XIXCORE_ASSERT_VCB(VCB);

	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixcore_CheckAndLockHost.\n"));

	memset(&RecordEmulCtx, 0 , sizeof(XIXCORE_RECORD_EMUL_CONTEXT));

	RC = xixcore_LotLock(
		VCB,
		VCB->MetaContext.HostRegLotMapIndex,
		&VCB->MetaContext.HostRegLotMapLockStatus,
		1,
		1
		);

	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) xixcore_CheckAndLockHost: xixcore_LotLock .\n", RC));
		return RC;
	}


	// Read Record Lot header Info
	RC = xixcore_InitializeRecordContext(&RecordEmulCtx, VCB, VCB->HostId);
	
	if ( RC< 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Fail(0x%x) xixcore_CheckAndLockHost: xixcore_InitializeRecordContext .\n", RC));
		goto error_out;
	}


	RC = xixcore_LookUpInitializeRecord(&RecordEmulCtx);

	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Fail(0x%x) xixcore_CheckAndLockHost: xixcore_LookUpInitializeRecord .\n", RC));
		goto error_out;
	}

	HostInfo = (PXIDISK_HOST_INFO)xixcore_GetDataBufferWithOffset(RecordEmulCtx.RecordInfo);
	
	if( HostInfo->NumHost > 1){
		xixcore_DebugPrintf("Too many Host is using this Voluem!\n");
		RC = XCCODE_EPERM;
		goto error_out;
	}
	

	xixcore_DebugPrintf("host acquire lock success!\n");

	RC = XCCODE_SUCCESS;

error_out:
	
	xixcore_LotUnLock(
		VCB,
		VCB->MetaContext.HostRegLotMapIndex,
		&VCB->MetaContext.HostRegLotMapLockStatus
		);	
	
	xixcore_CleanupRecordContext(&RecordEmulCtx);
		

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit xixcore_CheckAndLockHost Status (0x%x).\n", RC));

	return RC;
}



