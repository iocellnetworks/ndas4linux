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
#include "xcsystem/system.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/buffer.h"
#include "xixcore/ondisk.h"
#include "xixcore/lotlock.h"
#include "xixcore/lotinfo.h"
#include "xixcore/lotaddr.h"
#include "xixcore/bitmap.h"

/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCBITMAP"


void *
xixcore_call
xixcore_GetDataBufferOfBitMap(
	IN PXIXCORE_BUFFER Buffer
)
{
	PXIDISK_BITMAP_DATA tmpBuffer = NULL;
	tmpBuffer = (PXIDISK_BITMAP_DATA)xixcore_GetDataBufferWithOffset(Buffer);
	return (void *)&tmpBuffer->RealData[0];
}

xc_int64 
xixcore_call
xixcore_FindSetBit(
	IN		xc_int64 bitmap_count, 
	IN		xc_int64 bitmap_hint, 
	IN OUT	volatile void * Mpa
)
{
	xc_int64 i;
	xc_int64 hint = 0;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_FindSetBit \n"));

	bitmap_hint++;
	hint = bitmap_hint;

	XIXCORE_ASSERT(hint >= 0);
	for(i = hint;  i< bitmap_count; i++)
	{
		if(xixcore_TestBit(i, Mpa) )
		{
			return i;
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsfindSetBitFromMap \n"));
	return i;
}

xc_uint64 
xixcore_call
xixcore_FindFreeBit(
	IN		xc_uint64 bitmap_count, 
	IN		xc_uint64 bitmap_hint, 
	IN OUT	volatile void * Mpa
)
{
	xc_uint64 i;
	xc_uint64 hint = bitmap_hint;

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsfindFreeBitFromMap \n"));

	if(hint >0){
		hint ++;
	}
	for(i = hint; i< bitmap_count; i++)
	{
		if(!xixcore_TestBit(i, Mpa) )
		{
			return i;
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsfindFreeBitFromMap \n"));
	return i;
}

xc_uint64 
xixcore_call
xixcore_FindSetBitCount(
	IN		xc_uint64 bitmap_count, 
	IN OUT	volatile void *LotMapData
)
{
	xc_uint64 i;
	xc_uint32 count = 0;

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsfindSetBitMapCount \n"));

	for(i = 0; i< bitmap_count; i++)
	{
		if(xixcore_TestBit(i, LotMapData) )
		{
			count ++;
		}
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsfindSetBitMapCount \n"));
	return count;
}

xc_uint64
xixcore_call
xixcore_AllocLotMapFromFreeLotMap(
	IN	xc_uint64 bitmap_count, 
	IN	xc_uint64 request,  
	IN	volatile void * FreeLotMapData, 
	IN	volatile void * CheckOutLotMapData,
	IN	xc_uint64 * 	AllocatedCount
)
{
	xc_uint64 i;
	xc_uint64 alloc = 0;
	xc_uint64 StartIndex = XIFS_RESERVED_LOT_SIZE;
	xc_uint64 ReturnIndex = 0;
	int	InitSet =0;
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter XixFsAllocLotMapFromFreeLotMap \n"));

	for( i = StartIndex;  i < bitmap_count; i++)
	{
		if(alloc >= request) break;
		
		if(xixcore_TestBit(i, FreeLotMapData))
		{
			if(InitSet == 0){
				ReturnIndex = i;
				InitSet = 1;
			}
			xixcore_SetBit(i,CheckOutLotMapData);
			alloc++;
		}
	}

	if( alloc < request) {
		for( i = XIFS_RESERVED_LOT_SIZE;  i < bitmap_count; i++)
		{
			if(alloc >= request) break;
			
			if(xixcore_TestBit(i, FreeLotMapData))
			{
				if(InitSet == 0){
					ReturnIndex = i;
					InitSet = 1;
				}
				xixcore_SetBit(i,CheckOutLotMapData);
				alloc++;
			}
		}	
	}

	*AllocatedCount= alloc;
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit XixFsAllocLotMapFromFreeLotMap \n"));

	return ReturnIndex;
}




int
xixcore_call
xixcore_ReadBitMapWithBuffer(
	PXIXCORE_VCB VCB,
	xc_sector_t BitMapLotNumber,
	PXIXCORE_LOT_MAP	Bitmap,	
	PXIXCORE_BUFFER 		LotHeader,
	PXIXCORE_BUFFER 		BitmapHeader_xbuf 
)
{
	int							RC = 0;
	xc_uint32					size = 0;
	xc_uint32					BuffSize = 0;
	PXIDISK_LOT_MAP_INFO    	pMapInfo = NULL;
	PXIDISK_COMMON_LOT_HEADER 	pLotHeader = NULL;
	xc_int32					Reason = 0;

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_ReadBitMapWithBuffer \n"));


	XIXCORE_ASSERT_VCB(VCB);
	XIXCORE_ASSERT(VCB->XixcoreBlockDevice);
	XIXCORE_ASSERT(Bitmap);
	XIXCORE_ASSERT(Bitmap->Data);
	XIXCORE_ASSERT(LotHeader);
	XIXCORE_ASSERT(BitmapHeader_xbuf);


	size = SECTOR_ALIGNED_SIZE(VCB->SectorSizeBit, (xc_uint32) ((VCB->NumLots + 7)/8 + sizeof(XIDISK_BITMAP_DATA) - 1));

	BuffSize = size * 2;

	memset(xixcore_GetDataBuffer(Bitmap->Data), 0, BuffSize);
	memset(xixcore_GetDataBuffer(BitmapHeader_xbuf), 0, XIDISK_DUP_LOT_MAP_INFO_SIZE);
	memset(xixcore_GetDataBuffer(LotHeader), 0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	RC = xixcore_RawReadLotHeader(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					BitMapLotNumber,
					LotHeader ,
					&Reason
					);
	

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_ReadBitMapWithBuffer--> xixcore_RawReadLotHeader .\n", RC, Reason));	
		goto error_out;
	}


	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(LotHeader);

	RC = xixcore_CheckLotInfo(
			&pLotHeader->LotInfo,
			VCB->VolumeLotSignature,
			BitMapLotNumber,
			LOT_INFO_TYPE_BITMAP,
			LOT_FLAG_BEGIN,
			&Reason
			);
	
	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_ReadBitMapWithBuffer --> xixcore_CheckLotInfo .\n", RC, Reason));	
		goto error_out;
	}



	RC = xixcore_RawReadLotMapHeader(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					BitMapLotNumber,
					BitmapHeader_xbuf ,
					&Reason
					);
	

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_ReadBitMapWithBuffer--> read LotMapInfo .\n", RC, Reason));	
		goto error_out;
	}

	pMapInfo = (PXIDISK_LOT_MAP_INFO)xixcore_GetDataBufferWithOffset(BitmapHeader_xbuf);
	

	Bitmap->BitMapBytes = pMapInfo->BitMapBytes;
	Bitmap->NumLots = pMapInfo->NumLots;
	Bitmap->MapType = pMapInfo->MapType;


	RC = xixcore_RawReadBitmapData(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					BitMapLotNumber,
					size,
					Bitmap->Data,
					&Reason
					);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_RawReadBitmapData.\n", RC, Reason));	
		goto error_out;
	}

	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("NumLots(%lld) ,Num Bytes(%d)\n",
			Bitmap->NumLots, Bitmap->BitMapBytes));

	/*
	{
		xc_uint8 * Buffer = NULL;
		xc_uint32	i = 0;

		Buffer = xixcore_GetDataBufferOfBitMap(Bitmap->Data); 
		for(i = 0; i< (xc_uint32)(Bitmap->BitMapBytes/8); i++)
		{
			DebugTrace(0, (DEBUG_TRACE_FILESYSCTL| DEBUG_TRACE_TRACE), 
				("0x%04x\t[%02x:%02x:%02x:%02x\t%02x:%02x:%02x:%02x]\n",
				i*8,
				Buffer[i*8 + 0],Buffer[i*8 + 1],
				Buffer[i*8 + 2],Buffer[i*8 + 3],
				Buffer[i*8 + 4],Buffer[i*8 + 5],
				Buffer[i*8 + 6],Buffer[i*8 + 7]
				));
		}
	}
	*/

error_out:	


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("Exit xixcore_ReadBitMapWithBuffer  Status(0x%x)\n", RC));
	
	return RC;
}




int
xixcore_call
xixcore_WriteBitMapWithBuffer(
	PXIXCORE_VCB			VCB,
	xc_sector_t				BitMapLotNumber,
	PXIXCORE_LOT_MAP		Bitmap,
	PXIXCORE_BUFFER 		LotHeader,
	PXIXCORE_BUFFER 		BitmapHeader_xbuf , 
	xc_uint32				Initialize
)
{
	int						RC = 0;
	
	xc_uint32				size = 0;
	PXIDISK_LOT_MAP_INFO    pMapInfo = NULL;
	PXIDISK_COMMON_LOT_HEADER 		pLotHeader = NULL;
	xc_int32				Reason = 0;

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_WriteBitMapWithBuffer LotNum(%lld) \n", BitMapLotNumber));

	XIXCORE_ASSERT_VCB(VCB);
	XIXCORE_ASSERT(VCB->XixcoreBlockDevice);
	XIXCORE_ASSERT(Bitmap);
	XIXCORE_ASSERT(Bitmap->Data);
	XIXCORE_ASSERT(BitmapHeader_xbuf);
	XIXCORE_ASSERT(LotHeader);


	memset(xixcore_GetDataBuffer(BitmapHeader_xbuf), 0, XIDISK_DUP_LOT_MAP_INFO_SIZE);
	memset(xixcore_GetDataBuffer(LotHeader), 0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	size = SECTOR_ALIGNED_SIZE(VCB->SectorSizeBit, (xc_uint32) ((VCB->NumLots + 7)/8 + sizeof(XIDISK_BITMAP_DATA)- 1));
	

	if(Initialize){
		PXIXCORE_BUFFER 		tmpBuffer = NULL;
		xc_uint32				BuffSize = 0;
		xc_sector_t				startSec = 0;

		
		BuffSize = size * 2;

		tmpBuffer = xixcore_AllocateBuffer(BuffSize);
		if(!tmpBuffer) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail Allocate Temp buffer .\n"));	
			goto error_out;
		}


		xixcore_ZeroBufferOffset(tmpBuffer);
		memset(xixcore_GetDataBuffer(tmpBuffer), 0, BuffSize);


		startSec = xixcore_GetAddrOfBitmapDataSec(
								VCB->LotSize,
								VCB->SectorSizeBit,
								BitMapLotNumber
								);	


		RC = xixcore_DeviceIoSync(
				VCB->XixcoreBlockDevice,
				startSec,
				BuffSize,
				VCB->SectorSize,
				VCB->SectorSizeBit,
				tmpBuffer, 
				XIXCORE_WRITE, 
				&Reason
				);

	 	xixcore_FreeBuffer(tmpBuffer);


		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMapWithBuffer -->InvalDate bitmapData .\n", RC, Reason));	
			goto error_out;
		}

		xixcore_ZeroBufferOffset(LotHeader);

		RC = xixcore_RawReadLotHeader(
						VCB->XixcoreBlockDevice,
						VCB->LotSize,
						VCB->SectorSize,
						VCB->SectorSizeBit,
						BitMapLotNumber,
						LotHeader ,
						&Reason
						);

		if( RC < 0){
			if( RC == XCCODE_CRCERROR ) {
				xixcore_ZeroBufferOffset(LotHeader);
			}else {
				goto error_out;
			}
		}



		pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(LotHeader);

		xixcore_InitializeCommonLotHeader(
				pLotHeader,
				VCB->VolumeLotSignature,
				LOT_INFO_TYPE_BITMAP,
				LOT_FLAG_BEGIN,
				BitMapLotNumber,
				BitMapLotNumber,
				0,
				0,
				0,
				sizeof(XIDISK_MAP_LOT),
				VCB->LotSize - sizeof(XIDISK_MAP_LOT)
				);	


		RC = xixcore_RawWriteLotHeader(
						VCB->XixcoreBlockDevice,
						VCB->LotSize,
						VCB->SectorSize,
						VCB->SectorSizeBit,
						BitMapLotNumber,
						LotHeader ,
						&Reason
						);

		if( RC < 0){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMapWithBuffer --> xixcore_RawWriteLotHeader .\n", RC, Reason));	
			goto error_out;
		}

		
	}


	xixcore_ZeroBufferOffset(BitmapHeader_xbuf);
	RC = xixcore_RawReadLotMapHeader(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					BitMapLotNumber,
					BitmapHeader_xbuf ,
					&Reason
					);


	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMapWithBuffer --> xixcore_RawWriteLotMapHeader .\n", RC, Reason));	
		if( RC == XCCODE_CRCERROR ) {
			xixcore_ZeroBufferOffset(BitmapHeader_xbuf);
		}else {
			goto error_out;
		}
	}


 	pMapInfo = (PXIDISK_LOT_MAP_INFO)xixcore_GetDataBufferWithOffset(BitmapHeader_xbuf);
	
	pMapInfo->BitMapBytes = Bitmap->BitMapBytes;
	pMapInfo->MapType = Bitmap->MapType;
	pMapInfo->NumLots = Bitmap->NumLots;
	


	RC = xixcore_RawWriteLotMapHeader(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					BitMapLotNumber,
					BitmapHeader_xbuf ,
					&Reason
					);


	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMapWithBuffer --> xixcore_RawWriteLotMapHeader .\n", RC, Reason));	
		goto error_out;
	}


	RC = xixcore_RawWriteBitmapData(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					BitMapLotNumber,
					size,
					Bitmap->Data,
					&Reason
					);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMapWithBuffer -->xixcore_RawWriteBitmapData .\n", RC, Reason));	
		goto error_out;
	}
	


error_out:
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit xixcore_WriteBitMapWithBuffer Status(0x%x)\n", RC));
	return RC;
}



int
xixcore_call
xixcore_InvalidateLotBitMapWithBuffer(
	PXIXCORE_VCB VCB,
	xc_sector_t BitMapLotNumber,
	PXIXCORE_BUFFER LotHeader 
)
{

	int						RC = 0;
	xc_int32					size;
	int						Reason = 0;
	PXIDISK_COMMON_LOT_HEADER 		pLotHeader = NULL;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_InvalidateLotBitMapWithBuffer \n"));

	XIXCORE_ASSERT_VCB(VCB);
	XIXCORE_ASSERT(VCB->XixcoreBlockDevice);
	
	

	size = XIDISK_MAP_LOT_SIZE;

	xixcore_ZeroBufferOffset(LotHeader);

	memset(xixcore_GetDataBuffer(LotHeader), 0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	

	RC =xixcore_RawReadLotHeader(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					BitMapLotNumber,
					LotHeader,
					&Reason
					);
	

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_InvalidateLotBitMapWithBuffer -->read LotMapInfo .\n", RC, Reason));	
		if( RC == XCCODE_CRCERROR ) {
			xixcore_ZeroBufferOffset(LotHeader);
		}else {
			goto error_out;
		}
	}




	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(LotHeader);

	xixcore_InitializeCommonLotHeader(
			pLotHeader,
			VCB->VolumeLotSignature,
			LOT_INFO_TYPE_INVALID,
			LOT_FLAG_INVALID,
			BitMapLotNumber,
			BitMapLotNumber,
			0,
			0,
			0,
			sizeof(XIDISK_MAP_LOT),
			VCB->LotSize - sizeof(XIDISK_MAP_LOT)
			);	
		


	
	
	RC =xixcore_RawWriteLotHeader(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					BitMapLotNumber,
					LotHeader,
					&Reason
					);
	

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_InvalidateLotBitMapWithBuffer -->read LotMapInfo .\n", RC, Reason));	
		goto error_out;
	}


error_out:	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit xixcore_InvalidateLotBitMapWithBuffer Status(0x%x)\n", RC));
	return RC;
}




int
xixcore_call
xixcore_ReadAndAllocBitMap(	
	PXIXCORE_VCB VCB,
	xc_sector_t LotMapIndex,
	PXIXCORE_LOT_MAP *ppLotMap
)
{
	int						RC = 0;
	xc_uint32					size;
	xc_uint32					BuffSize = 0;
	PXIDISK_COMMON_LOT_HEADER 	pLotHeader = NULL;
	PXIDISK_LOT_MAP_INFO    	pMapInfo = NULL;
	PXIXCORE_LOT_MAP			pLotMap = NULL;
	xc_int32					Reason = 0;
	PXIXCORE_BUFFER				tmpBuf = NULL;
	PXIXCORE_BUFFER				tmpLotHeader = NULL;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_ReadAndAllocBitMap \n"));


	XIXCORE_ASSERT_VCB(VCB);
	XIXCORE_ASSERT(VCB->XixcoreBlockDevice);
	

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("xixcore_ReadAndAllocBitMap : Read Lot Map Index(%lld) \n", LotMapIndex));

	size = SECTOR_ALIGNED_SIZE(VCB->SectorSizeBit, (xc_uint32) ((VCB->NumLots + 7)/8 + sizeof(XIDISK_BITMAP_DATA)- 1));
	BuffSize = size * 2;

	pLotMap = xixcore_AllocateMem(sizeof(XIXCORE_LOT_MAP),0, XCTAG_LOTMAP);

	if(!pLotMap){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixcore_ReadAndAllocBitMap Allocation pLotMap \n"));
		*ppLotMap = NULL;
		return XCCODE_ENOMEM;
	}

	memset(pLotMap, 0, sizeof(XIXCORE_LOT_MAP));


	pLotMap->Data   =  xixcore_AllocateBuffer(BuffSize);
	if(!pLotMap){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Failxixcore_ReadAndAllocBitMap  Allocation pLotMap->Data \n"));
		xixcore_FreeMem(pLotMap, XCTAG_LOTMAP);
		*ppLotMap = NULL;
		return XCCODE_ENOMEM;
	}


	memset(xixcore_GetDataBuffer(pLotMap->Data), 0, BuffSize);


	tmpBuf = xixcore_AllocateBuffer(XIDISK_DUP_LOT_MAP_INFO_SIZE);

	if( !tmpBuf ) {
		xixcore_FreeBuffer(pLotMap->Data);
		xixcore_FreeMem(pLotMap, XCTAG_LOTMAP);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixcore_ReadAndAllocBitMap Allocation tmpBuf \n"));
		
		*ppLotMap = NULL;
		return XCCODE_ENOMEM;
	}

	memset(xixcore_GetDataBuffer(tmpBuf), 0, XIDISK_DUP_LOT_MAP_INFO_SIZE);

	tmpLotHeader = xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
	if( !tmpLotHeader ) {
		xixcore_FreeBuffer(pLotMap->Data);
		xixcore_FreeMem(pLotMap, XCTAG_LOTMAP);
		xixcore_FreeBuffer(tmpBuf);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixcore_ReadAndAllocBitMap Allocation tmpBuf \n"));
		
		*ppLotMap = NULL;
		return XCCODE_ENOMEM;
	}


	


	xixcore_ZeroBufferOffset(tmpLotHeader);

	memset(xixcore_GetDataBuffer(tmpLotHeader), 0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
	


	RC = xixcore_RawReadLotHeader(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					LotMapIndex,
					tmpLotHeader,
					&Reason
					);
	

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_ReadAndAllocBitMap--> xixcore_RawReadLotHeader .\n", RC, Reason));	

		goto error_out;
	}

	pLotHeader = (PXIDISK_COMMON_LOT_HEADER) xixcore_GetDataBufferWithOffset(tmpLotHeader);

	RC = xixcore_CheckLotInfo(
			&pLotHeader->LotInfo,
			VCB->VolumeLotSignature,
			LotMapIndex,
			LOT_INFO_TYPE_BITMAP,
			LOT_FLAG_BEGIN,
			&Reason
			);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_ReadAndAllocBitMap -->Check LotInfo .\n", RC, Reason));
		
		goto error_out;
	}


	xixcore_ZeroBufferOffset(tmpBuf);

	RC = xixcore_RawReadLotMapHeader(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					LotMapIndex,
					tmpBuf,
					&Reason
					);
	

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_ReadAndAllocBitMap--> xixcore_RawReadLotMapHeader .\n", RC, Reason));	

		goto error_out;
	}


	pMapInfo = (PXIDISK_LOT_MAP_INFO) xixcore_GetDataBufferWithOffset(tmpBuf);
	pLotMap->BitMapBytes = pMapInfo->BitMapBytes;
	pLotMap->NumLots = pMapInfo->NumLots;
	pLotMap->MapType = pMapInfo->MapType;

	

	RC = xixcore_RawReadBitmapData(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					LotMapIndex,
					size,
					pLotMap->Data, 
					&Reason
					);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_ReadAndAllocBitMap -->xixcore_RawReadBitmapData .\n", RC, Reason));	

		goto error_out;
		
	}


	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("NumLots(%lld) ,Num Bytes(%d)\n",
			pLotMap->NumLots, pLotMap->BitMapBytes));

	/*
	{
		xc_uint8 * Buffer = NULL;
		xc_uint32	i = 0;

		Buffer = xixcore_GetDataBufferOfBitMap(pLotMap->Data); 
		for(i = 0; i< (xc_uint32)(pLotMap->BitMapBytes/8); i++)
		{
			DebugTrace(0, (DEBUG_TRACE_FILESYSCTL| DEBUG_TRACE_TRACE), 
				("0x%04x\t[%02x:%02x:%02x:%02x\t%02x:%02x:%02x:%02x]\n",
				i*8,
				Buffer[i*8 + 0],Buffer[i*8 + 1],
				Buffer[i*8 + 2],Buffer[i*8 + 3],
				Buffer[i*8 + 4],Buffer[i*8 + 5],
				Buffer[i*8 + 6],Buffer[i*8 + 7]
				));
		}
	}
	*/
	*ppLotMap = pLotMap;
	RC = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit (0x%x)  xixcore_ReadAndAllocBitMap \n", RC));

	xixcore_FreeBuffer(tmpBuf);
	xixcore_FreeBuffer(tmpLotHeader);
	return RC;

error_out:

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("Fail Exit (0x%x)  xixcore_ReadAndAllocBitMap\n", RC));


	if(pLotMap) {
		if(pLotMap->Data) {
			xixcore_FreeBuffer(pLotMap->Data);
		}

		xixcore_FreeMem(pLotMap, XCTAG_LOTMAP);
	}


	if(tmpBuf){
		xixcore_FreeBuffer(tmpBuf);
	}

	if(tmpLotHeader){
		xixcore_FreeBuffer(tmpLotHeader);
	}
	*ppLotMap = NULL;
	
	return RC;
}


int
xixcore_call
xixcore_WriteBitMap(
	PXIXCORE_VCB VCB,
	xc_sector_t LotMapIndex,
	PXIXCORE_LOT_MAP pLotMap,
	xc_uint32		Initialize
)
{
	int					RC = 0;
	
	xc_uint32				size;
	PXIDISK_COMMON_LOT_HEADER 	pLotHeader = NULL;
	PXIDISK_LOT_MAP_INFO     pMapInfo = NULL;
	PXIXCORE_BUFFER			tmpBuf = NULL;
	PXIXCORE_BUFFER			tmpLotHeader = NULL;
	xc_int32				Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_WriteBitMap \n"));

	XIXCORE_ASSERT_VCB(VCB);
	XIXCORE_ASSERT(VCB->XixcoreBlockDevice);
	XIXCORE_ASSERT(pLotMap);

	
	size = XIDISK_MAP_LOT_SIZE;


	tmpBuf = xixcore_AllocateBuffer(XIDISK_DUP_LOT_MAP_INFO_SIZE);

	if( !tmpBuf ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail : xixcore_WriteBitMap -->Allocation tmpBuf \n"));

		return XCCODE_ENOMEM;
	}

	memset(xixcore_GetDataBuffer(tmpBuf), 0, XIDISK_DUP_LOT_MAP_INFO_SIZE);
	

	size = SECTOR_ALIGNED_SIZE(VCB->SectorSizeBit, (xc_uint32) ((VCB->NumLots + 7)/8 + sizeof(XIDISK_BITMAP_DATA)- 1));


	if(Initialize){
		PXIXCORE_BUFFER			tmpData = NULL;
		xc_uint32				BuffSize = 0;
		xc_sector_t				startSec = 0;

		BuffSize = size * 2;
		tmpData = xixcore_AllocateBuffer(BuffSize);
		if(!tmpData) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail : xixcore_WriteBitMap -->Allocation tmpData \n"));

			xixcore_FreeBuffer(tmpBuf);
			return XCCODE_ENOMEM;
		}

		xixcore_ZeroBufferOffset(tmpData);
		memset(xixcore_GetDataBuffer(tmpData), 0, BuffSize);



		startSec = xixcore_GetAddrOfBitmapDataSec(
								VCB->LotSize,
								VCB->SectorSizeBit,
								LotMapIndex
								);	


		RC = xixcore_DeviceIoSync(
				VCB->XixcoreBlockDevice,
				startSec,
				BuffSize,
				VCB->SectorSize,
				VCB->SectorSizeBit,
				tmpData, 
				XIXCORE_WRITE, 
				&Reason
				);

	 	xixcore_FreeBuffer(tmpData);


		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMapWithBuffer -->InvalDate bitmapData .\n", RC, Reason));	
			goto error_out;
		}



		tmpLotHeader = xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

		if( !tmpLotHeader ) {
			xixcore_FreeBuffer(tmpBuf);
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail : xixcore_WriteBitMap -->Allocation tmpLotHeader \n"));

			return XCCODE_ENOMEM;
		}

		xixcore_ZeroBufferOffset(tmpLotHeader);

		memset(xixcore_GetDataBuffer(tmpLotHeader), 0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

		RC = xixcore_RawReadLotHeader(
						VCB->XixcoreBlockDevice,
						VCB->LotSize,
						VCB->SectorSize,
						VCB->SectorSizeBit,
						LotMapIndex,
						tmpLotHeader,
						&Reason
						);
		

		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMap -->xixcore_RawWriteLotHeader .\n", RC, Reason));	

			if( RC == XCCODE_CRCERROR ) {
				xixcore_ZeroBufferOffset(tmpLotHeader);
			}else {
				goto error_out;
			}
		}



		pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(tmpLotHeader);
		
		xixcore_InitializeCommonLotHeader(
				pLotHeader,
				VCB->VolumeLotSignature,
				LOT_INFO_TYPE_BITMAP,
				LOT_FLAG_BEGIN,
				LotMapIndex,
				LotMapIndex,
				0,
				0,
				0,
				sizeof(XIDISK_MAP_LOT),
				VCB->LotSize - sizeof(XIDISK_MAP_LOT)
				);


		RC = xixcore_RawWriteLotHeader(
						VCB->XixcoreBlockDevice,
						VCB->LotSize,
						VCB->SectorSize,
						VCB->SectorSizeBit,
						LotMapIndex,
						tmpLotHeader,
						&Reason
						);
		

		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMap -->xixcore_RawWriteLotHeader .\n", RC, Reason));	

			goto error_out;
		}



					
	}

	xixcore_ZeroBufferOffset(tmpBuf);

	RC = xixcore_RawReadLotMapHeader(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					LotMapIndex,
					tmpBuf,
					&Reason
					);
	

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMap -->xixcore_RawWriteLotMapHeader .\n", RC, Reason));	

		if( RC == XCCODE_CRCERROR ) {
			xixcore_ZeroBufferOffset(tmpBuf);
		}else {
			goto error_out;
		}
	}


	pMapInfo = (PXIDISK_LOT_MAP_INFO)xixcore_GetDataBufferWithOffset(tmpBuf);
	
	pMapInfo->BitMapBytes = pLotMap->BitMapBytes;
	pMapInfo->MapType = pLotMap->MapType;
	pMapInfo->NumLots = pLotMap->NumLots;
	
	RC = xixcore_RawWriteLotMapHeader(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					LotMapIndex,
					tmpBuf,
					&Reason
					);
	

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMap -->xixcore_RawWriteLotMapHeader .\n", RC, Reason));	

		goto error_out;
	}


	RC = xixcore_RawWriteBitmapData(
					VCB->XixcoreBlockDevice,
					VCB->LotSize,
					VCB->SectorSize,
					VCB->SectorSizeBit,
					LotMapIndex,
					size,
					pLotMap->Data,
					&Reason
					);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x) Reason(0x%x) xixcore_WriteBitMap -->xixcore_RawWriteBitmapData .\n", RC, Reason));
		
		goto error_out;
	}

	RC = 0;



error_out:

	if(tmpBuf) {
		xixcore_FreeBuffer(tmpBuf);
	}

	if(tmpLotHeader) {
		xixcore_FreeBuffer(tmpLotHeader);
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit (0x%x) XixFsWriteBitMap \n", RC));
	return RC;
}


#define DEFAULT_MAX_LOT_ALLOC_COUNT		(1000)

int
xixcore_call
xixcore_SetCheckOutLotMap(
	PXIXCORE_LOT_MAP	FreeLotMap,
	PXIXCORE_LOT_MAP	CheckOutLotMap,
	xc_int32 			HostCount
)
{
	//int 		RC = 0;
	
	xc_uint64 	FreeLotCount = 0;
	xc_uint64 	RequestCount = 0;
	xc_uint32 	ModIndicator = 0;
	xc_uint64 	AllocatedCount = 0;
	//xc_uint32 	i = 0;


	XIXCORE_ASSERT(FreeLotMap);
	XIXCORE_ASSERT(FreeLotMap->Data);
	XIXCORE_ASSERT(CheckOutLotMap);
	XIXCORE_ASSERT(CheckOutLotMap->Data);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_SetCheckOutLotMap \n"));

	if(HostCount > XIFS_DEFAULT_USER){
		ModIndicator = HostCount;
	}else {
		ModIndicator = XIFS_DEFAULT_USER;
	}

	FreeLotCount = xixcore_FindSetBitCount(FreeLotMap->NumLots,xixcore_GetDataBufferOfBitMap(FreeLotMap->Data));
	xixcore_Divide64(&FreeLotCount ,ModIndicator);
	RequestCount = FreeLotCount;
	xixcore_Divide64(&RequestCount, 10);

	if(RequestCount > DEFAULT_MAX_LOT_ALLOC_COUNT) {
		RequestCount = DEFAULT_MAX_LOT_ALLOC_COUNT;
	}else if(RequestCount < 100) {
		RequestCount = 100;
	}


	CheckOutLotMap->StartIndex = xixcore_AllocLotMapFromFreeLotMap(
									FreeLotMap->NumLots, 
									RequestCount, 
									xixcore_GetDataBufferOfBitMap(FreeLotMap->Data), 
									xixcore_GetDataBufferOfBitMap(CheckOutLotMap->Data),
									&AllocatedCount);
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("NumLots(%lld) ,Num Bytes(%d)\n",
			FreeLotMap->NumLots, FreeLotMap->BitMapBytes));

	/*
	{	xc_uint32 i = 0;
		xc_uint8 * Buffer = xixcore_GetDataBufferOfBitMap(FreeLotMap->Data);
		
		for(i = 0; i< (xc_uint32)(FreeLotMap->BitMapBytes/8); i++)
		{
			DebugTrace(0, (DEBUG_TRACE_FILESYSCTL| DEBUG_TRACE_TRACE), 
				("0x%04x\t[%02x:%02x:%02x:%02x\t%02x:%02x:%02x:%02x]\n",
				i*8,
				Buffer[i*8 + 0],Buffer[i*8 + 1],
				Buffer[i*8 + 2],Buffer[i*8 + 3],
				Buffer[i*8 + 4],Buffer[i*8 + 5],
				Buffer[i*8 + 6],Buffer[i*8 + 7]
				));
		}
	}
	*/

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("NumLots(%lldd) ,FreeLotMapCount(%lld), RequestCount(%lld), StartIndex(%lld)\n",
			FreeLotMap->NumLots, FreeLotCount, RequestCount,CheckOutLotMap->StartIndex ));


	if(AllocatedCount < XIFS_DEFAULT_HOST_LOT_COUNT){
		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
			("Exit Error xixcore_SetCheckOutLotMap \n"));
		return XCCODE_UNSUCCESS;
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit  xixcore_SetCheckOutLotMap \n"));
	return XCCODE_SUCCESS;
}


void
xixcore_call
xixcore_ORMap(
	PXIXCORE_LOT_MAP		pDestMap,
	PXIXCORE_LOT_MAP		pSourceMap
	)
{
	xc_uint64	LotMapBytes;
	xc_uint64	i = 0;
	xc_uint8	*srcRawMap, *destRawMap;


	XIXCORE_ASSERT(pDestMap);
	XIXCORE_ASSERT(pDestMap->Data);
	XIXCORE_ASSERT(pSourceMap);
	XIXCORE_ASSERT(pSourceMap->Data);
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_ORMap \n"));

	LotMapBytes = pDestMap->BitMapBytes;
	srcRawMap = xixcore_GetDataBufferOfBitMap(pSourceMap->Data);
	destRawMap = xixcore_GetDataBufferOfBitMap(pDestMap->Data);
	for(i = 0; i < LotMapBytes; i++){
		destRawMap[i] = destRawMap[i] | srcRawMap[i];
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit xixcore_ORMap \n"));

	return;
}

void
xixcore_call
xixcore_XORMap(
	PXIXCORE_LOT_MAP		pDestMap,
	PXIXCORE_LOT_MAP		pSourceMap
)
{
	xc_uint64	LotMapBytes;
	xc_uint64	i = 0;
	xc_uint8	*srcRawMap, *destRawMap;


	XIXCORE_ASSERT(pDestMap);
	XIXCORE_ASSERT(pDestMap->Data);
	XIXCORE_ASSERT(pSourceMap);
	XIXCORE_ASSERT(pSourceMap->Data);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_XORMap \n"));

	LotMapBytes = pDestMap->BitMapBytes;
	srcRawMap = xixcore_GetDataBufferOfBitMap(pSourceMap->Data);
	destRawMap = xixcore_GetDataBufferOfBitMap(pDestMap->Data);
	for(i = 0; i < LotMapBytes; i++){
		destRawMap[i] = destRawMap[i] & ~srcRawMap[i];
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit xixcore_XORMap \n"));

	return;
}





int
xixcore_call
xixcore_InitializeBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx,
	PXIXCORE_VCB	pVCB,
	xc_uint64 UsedBitmapIndex,
	xc_uint64 UnusedBitmapIndex,
	xc_uint64 CheckOutBitmapIndex
)
{
	int RC = 0;
	xc_uint32 size;
	xc_uint32	BuffSize = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_InitializeBitmapContext.\n"));		


	memset(BitmapEmulCtx, 0, sizeof(XIXCORE_BITMAP_EMUL_CONTEXT));
	BitmapEmulCtx->VCB = pVCB;
	BitmapEmulCtx->UsedBitmapIndex = UsedBitmapIndex;
	BitmapEmulCtx->UnusedBitmapIndex = UnusedBitmapIndex;
	BitmapEmulCtx->CheckOutBitmapIndex = CheckOutBitmapIndex;	
	BitmapEmulCtx->BitMapBytes =(xc_uint32) ((pVCB->NumLots + 7)/8);
	BitmapEmulCtx->BitmapLotHeader = NULL;
	BitmapEmulCtx->CheckOutBitmap.Data = NULL;
	BitmapEmulCtx->UnusedBitmap.Data = NULL;
	BitmapEmulCtx->UsedBitmap.Data = NULL;

	
	size = SECTOR_ALIGNED_SIZE(pVCB->SectorSizeBit,  (xc_uint32) ((pVCB->NumLots + 7)/8 + sizeof(XIDISK_BITMAP_DATA) -1));
	BuffSize = size * 2;

	BitmapEmulCtx->BitmapLotHeader = xixcore_AllocateBuffer(XIDISK_DUP_LOT_MAP_INFO_SIZE);
	if(!BitmapEmulCtx->BitmapLotHeader){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("fail error allocate xixcore_InitializeBitmapContext --> BitmapLotHeader.\n"));
		
		RC = XCCODE_ENOMEM;
		goto error_out;	
	}


	memset(xixcore_GetDataBuffer(BitmapEmulCtx->BitmapLotHeader), 0, XIDISK_DUP_LOT_MAP_INFO_SIZE);





	BitmapEmulCtx->LotHeader = xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
	if(!BitmapEmulCtx->LotHeader){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("fail error allocate xixcore_InitializeBitmapContext --> BitmapLotHeader.\n"));
		
		RC = XCCODE_ENOMEM;
		goto error_out;	
	}


	memset(xixcore_GetDataBuffer(BitmapEmulCtx->LotHeader), 0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);




	BitmapEmulCtx->UsedBitmap.Data= xixcore_AllocateBuffer(BuffSize);
	if(!BitmapEmulCtx->UsedBitmap.Data){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("fail error allocate xixcore_InitializeBitmapContext --> UsedBitmap.Data.\n"));
		
		RC =XCCODE_ENOMEM;
		goto error_out;	
	}

	memset(xixcore_GetDataBuffer(BitmapEmulCtx->UsedBitmap.Data), 0,  BuffSize);


	BitmapEmulCtx->UnusedBitmap.Data = xixcore_AllocateBuffer(BuffSize);
	if(!BitmapEmulCtx->UnusedBitmap.Data){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("fail error allocate xixcore_InitializeBitmapContext --> UnusedBitmap.Data.\n"));
		
		RC =XCCODE_ENOMEM;
		goto error_out;	
	}

	memset(xixcore_GetDataBuffer(BitmapEmulCtx->UnusedBitmap.Data), 0,BuffSize);

	BitmapEmulCtx->CheckOutBitmap.Data = xixcore_AllocateBuffer(BuffSize);
	if(!BitmapEmulCtx->CheckOutBitmap.Data){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("fail error allocate xixcore_InitializeBitmapContext --> CheckOutBitmap.Data.\n"));
		
		RC =XCCODE_ENOMEM;
		goto error_out;		
	}

	memset(xixcore_GetDataBuffer(BitmapEmulCtx->CheckOutBitmap.Data), 0,  BuffSize);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit XixFsdInitializeBitmapContext Statue(0x%x).\n", RC));		

	return XCCODE_SUCCESS;
		
error_out:

	if(BitmapEmulCtx->LotHeader){
		xixcore_FreeBuffer(BitmapEmulCtx->LotHeader);
		BitmapEmulCtx->LotHeader = NULL;
	}

		
	if(BitmapEmulCtx->BitmapLotHeader){
		xixcore_FreeBuffer(BitmapEmulCtx->BitmapLotHeader);
		BitmapEmulCtx->BitmapLotHeader = NULL;
	}

	if(BitmapEmulCtx->UsedBitmap.Data){
		xixcore_FreeBuffer(BitmapEmulCtx->UsedBitmap.Data);
		BitmapEmulCtx->UsedBitmap.Data = NULL;
	}

	if(BitmapEmulCtx->UnusedBitmap.Data){
		xixcore_FreeBuffer(BitmapEmulCtx->UnusedBitmap.Data);
		BitmapEmulCtx->UnusedBitmap.Data = NULL;
	}

	if(BitmapEmulCtx->CheckOutBitmap.Data){
		xixcore_FreeBuffer(BitmapEmulCtx->CheckOutBitmap.Data);
		BitmapEmulCtx->CheckOutBitmap.Data = NULL;
	}	

	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Fail Exit XixFsdInitializeBitmapContext Statue(0x%x).\n", RC));		

	return RC;
}




int
xixcore_call
xixcore_CleanupBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{

	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_CleanupBitmapContext.\n"));
	if(BitmapEmulCtx->LotHeader){
		xixcore_FreeBuffer(BitmapEmulCtx->LotHeader);
		BitmapEmulCtx->LotHeader = NULL;
	}

	if(BitmapEmulCtx->BitmapLotHeader){
		xixcore_FreeBuffer(BitmapEmulCtx->BitmapLotHeader);
		BitmapEmulCtx->BitmapLotHeader = NULL;
	}

	if(BitmapEmulCtx->UsedBitmap.Data){
		xixcore_FreeBuffer(BitmapEmulCtx->UsedBitmap.Data);
		BitmapEmulCtx->UsedBitmap.Data = NULL;
	}

	if(BitmapEmulCtx->UnusedBitmap.Data){
		xixcore_FreeBuffer(BitmapEmulCtx->UnusedBitmap.Data);
		BitmapEmulCtx->UnusedBitmap.Data = NULL;
	}

	if(BitmapEmulCtx->CheckOutBitmap.Data){
		xixcore_FreeBuffer(BitmapEmulCtx->CheckOutBitmap.Data);
		BitmapEmulCtx->CheckOutBitmap.Data = NULL;
	}	


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit xixcore_CleanupBitmapContext.\n"));

	return XCCODE_SUCCESS;
}



int
xixcore_call
xixcore_ReadDirmapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_ReadDirmapFromBitmapContext.\n"));

	return xixcore_ReadBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			(xc_sector_t)BitmapEmulCtx->UsedBitmapIndex, 
			(PXIXCORE_LOT_MAP)&(BitmapEmulCtx->UsedBitmap),
			BitmapEmulCtx->LotHeader,
			BitmapEmulCtx->BitmapLotHeader);
}


int
xixcore_call
xixcore_WriteDirmapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_WriteDirmapFromBitmapContext.\n"));

	return xixcore_WriteBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			(xc_sector_t)BitmapEmulCtx->UsedBitmapIndex, 
			(PXIXCORE_LOT_MAP)&(BitmapEmulCtx->UsedBitmap),
			BitmapEmulCtx->LotHeader,
			BitmapEmulCtx->BitmapLotHeader,
			0
			);
}


int
xixcore_call
xixcore_ReadFreemapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_ReadFreemapFromBitmapContext.\n"));

	return xixcore_ReadBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			(xc_sector_t)BitmapEmulCtx->UnusedBitmapIndex, 
			(PXIXCORE_LOT_MAP)&(BitmapEmulCtx->UnusedBitmap),
			BitmapEmulCtx->LotHeader,
			BitmapEmulCtx->BitmapLotHeader);
}


int
xixcore_call
xixcore_WriteFreemapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_WriteFreemapFromBitmapContext.\n"));


	return xixcore_WriteBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			(xc_sector_t)BitmapEmulCtx->UnusedBitmapIndex, 
			(PXIXCORE_LOT_MAP)&(BitmapEmulCtx->UnusedBitmap), 
			BitmapEmulCtx->LotHeader,
			BitmapEmulCtx->BitmapLotHeader,
			0);
}


int
xixcore_call
xixcore_ReadCheckoutmapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_ReadCheckoutmapFromBitmapContext.\n"));

	return xixcore_ReadBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			(xc_sector_t)BitmapEmulCtx->CheckOutBitmapIndex, 
			(PXIXCORE_LOT_MAP)&(BitmapEmulCtx->CheckOutBitmap),
			BitmapEmulCtx->LotHeader,
			BitmapEmulCtx->BitmapLotHeader);
}


int
xixcore_call
xixcore_WriteCheckoutmapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_WriteCheckoutmapFromBitmapContext.\n"));

	return xixcore_WriteBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			(xc_sector_t)BitmapEmulCtx->CheckOutBitmapIndex, 
			(PXIXCORE_LOT_MAP)&(BitmapEmulCtx->CheckOutBitmap), 
			BitmapEmulCtx->LotHeader,
			BitmapEmulCtx->BitmapLotHeader,
			0);
}


int
xixcore_call
xixcore_InvalidateDirtyBitMap(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_InvalidateDirtyBitMap.\n"));

	return xixcore_InvalidateLotBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			(xc_sector_t)BitmapEmulCtx->UsedBitmapIndex, 
			BitmapEmulCtx->LotHeader
			);
}


int
xixcore_call
xixcore_InvalidateFreeBitMap(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_InvalidateFreeBitMap.\n"));

	return xixcore_InvalidateLotBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			(xc_sector_t)BitmapEmulCtx->UnusedBitmapIndex, 
			BitmapEmulCtx->LotHeader
			);
}


int
xixcore_call
xixcore_InvalidateCheckOutBitMap(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixcore_InvalidateCheckOutBitMap.\n"));

	return xixcore_InvalidateLotBitMapWithBuffer(
			BitmapEmulCtx->VCB,
			(xc_sector_t)BitmapEmulCtx->CheckOutBitmapIndex, 
			BitmapEmulCtx->LotHeader
			);
}



xc_int64
xixcore_call
xixcore_AllocVCBLot(
	PXIXCORE_VCB	XixcoreVcb
)
{
//	PXIXFS_LINUX_VCB	VCB = container_of(XixcoreVCB, XIXFS_LINUX_VCB, XixcoreVcb);
	xc_uint64 		i = 0;
	xc_uint64 		BitmapCount = 0;
	xc_uint32 		bRetry = 0;
	int			RC = 0;
	XIXCORE_IRQL	oldIrql;
#if 0
#if LINUX_VERSION_25_ABOVE		
	int			TimeOut;
#endif
#endif
//	PXIXFS_LINUX_META_CTX		pCtx = NULL;
	PXIXCORE_META_CTX				pXixcoreCtx = NULL;



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_AllocVCBLot \n"));


	
	XIXCORE_ASSERT_VCB(XixcoreVcb);

//	pCtx = &VCB->MetaCtx;
	pXixcoreCtx = &XixcoreVcb->MetaContext;

//	XIXCORE_ASSERT(pCtx);
	XIXCORE_ASSERT(pXixcoreCtx->HostFreeLotMap);
	XIXCORE_ASSERT(pXixcoreCtx->HostFreeLotMap->Data);
	XIXCORE_ASSERT(pXixcoreCtx->HostDirtyLotMap);
	XIXCORE_ASSERT(pXixcoreCtx->HostDirtyLotMap->Data);

	if(XixcoreVcb->IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail xixcore_AllocVCBLot : is Readonly device \n"));
		return -1;
	}



retry:


	xixcore_AcquireSpinLock(pXixcoreCtx->MetaLock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(pCtx->MetaLock)) pXixcoreCtx(%p)\n", pXixcoreCtx ));
/*
	if(XIXCORE_TEST_FLAGS(pCtx->VCBMetaFlags, XIXCORE_META_FLAGS_RECHECK_RESOURCES)){
		spin_unlock(&(pCtx->MetaLock));

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("WAKE UP VCBMETAEVENT 1 \n"));

		wake_up(&(pCtx->VCBMetaEvent));
		
		TimeOut = DEFAULT_XIXFS_RECHECKRESOURCE;
		RC = wait_event_timeout(pCtx->VCBResourceEvent, 
							!XIXCORE_TEST_FLAGS(pCtx->VCBMetaFlags, XIXCORE_META_FLAGS_RECHECK_RESOURCES),
							TimeOut);

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("WAKE UP from VCB RESOURCE EVENT 1 \n"));

		
		if(RC< 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail xixcore_AllocVCBLot -->wait_event_timeout \n"));
			return -ENOSPC;
		}

		spin_lock(&(pCtx->MetaLock));
	}


	if(XIXCORE_TEST_FLAGS(pCtx->VCBMetaFlags, XIXCORE_META_FLAGS_INSUFFICIENT_RESOURCES)){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail xixcore_AllocVCBLot Insufficient resource \n"));
		
		spin_unlock(&(pCtx->MetaLock));
		return -ENOSPC;
	}

*/

	BitmapCount = pXixcoreCtx->HostFreeLotMap->NumLots;


	for( i =  pXixcoreCtx->HostFreeLotMap->StartIndex; i < BitmapCount; i++)
	{
		
		if(xixcore_TestBit(i,  (unsigned long *)xixcore_GetDataBufferOfBitMap(pXixcoreCtx->HostFreeLotMap->Data)))
		{
			xixcore_ClearBit(i,(unsigned long *)xixcore_GetDataBufferOfBitMap(pXixcoreCtx->HostFreeLotMap->Data));
			xixcore_SetBit(i,(unsigned long *)xixcore_GetDataBufferOfBitMap(pXixcoreCtx->HostDirtyLotMap->Data));
			pXixcoreCtx->HostFreeLotMap->StartIndex = i;
			pXixcoreCtx->HostDirtyLotMap->StartIndex = i;
			XIXCORE_SET_FLAGS(pXixcoreCtx->ResourceFlag, XIXCORE_META_RESOURCE_NEED_UPDATE);
			xixcore_ReleaseSpinLock(pXixcoreCtx->MetaLock, oldIrql);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(pCtx->MetaLock)) pXixcoreCtx(%p)\n", pXixcoreCtx ));

			return (xc_int64)i;			
		}
	}

	pXixcoreCtx->HostFreeLotMap->StartIndex = 0;
	pXixcoreCtx->HostDirtyLotMap->StartIndex = 0;

	for( i = pXixcoreCtx->HostFreeLotMap->StartIndex; i < BitmapCount; i++)
	{
		
		if(xixcore_TestBit(i, (unsigned long *)xixcore_GetDataBufferOfBitMap(pXixcoreCtx->HostFreeLotMap->Data)))
		{
			xixcore_ClearBit(i,(unsigned long *)xixcore_GetDataBufferOfBitMap(pXixcoreCtx->HostFreeLotMap->Data));
			xixcore_SetBit(i,(unsigned long *)xixcore_GetDataBufferOfBitMap(pXixcoreCtx->HostDirtyLotMap->Data));
			pXixcoreCtx->HostFreeLotMap->StartIndex = i;
			pXixcoreCtx->HostDirtyLotMap->StartIndex = i;
			DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
				("Exit Success xixcore_AllocVCBLot \n"));
			XIXCORE_SET_FLAGS(pXixcoreCtx->ResourceFlag, XIXCORE_META_RESOURCE_NEED_UPDATE);
			xixcore_ReleaseSpinLock(pXixcoreCtx->MetaLock, oldIrql);
			DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(pCtx->MetaLock)) pXixcoreCtx(%p)\n", pXixcoreCtx ));

			return (xc_int64)i;			
		}
	}
	
	if(bRetry < 2){
#if 0
		XIXFS_WAIT_CTX wait;
#endif

		bRetry ++;
		XIXCORE_SET_FLAGS(pXixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_RECHECK_RESOURCES);

		xixcore_ReleaseSpinLock(pXixcoreCtx->MetaLock, oldIrql);
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(pCtx->MetaLock)) pXixcoreCtx(%p)\n", pXixcoreCtx ));


		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("WAKE UP VCBMETAEVENT 2 \n"));

#if 0
		xixcore_init_wait_ctx(&wait);
		xixcore_add_resource_wait_list(&wait, pCtx);
		
		wake_up(&(pCtx->VCBMetaEvent));


		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("WAKE UP from VCB RESOURCE EVENT 2 \n"));

		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("RESOUCE REQ\n"));

		printk("WAIT Resource\n");
		
#if LINUX_VERSION_25_ABOVE			
		TimeOut = DEFAULT_XIXFS_RECHECKRESOURCE;
		RC = wait_for_completion_timeout(&(wait.WaitCompletion),TimeOut);
#else
		wait_for_completion(&(wait.WaitCompletion));
		RC = 1;
#endif

		printk("END Wait Resource\n");
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("END RESOUCE REQ\n"));
#else
		RC = xixcore_WaitForMetaResource(XixcoreVcb);
#endif
		if(RC <= 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("Fail xixcore_AllocVCBLot -->wait_event_timeout \n"));
			
			return -1;
		}else {
			goto retry;
		}
		
	}else{
		xixcore_ReleaseSpinLock(pXixcoreCtx->MetaLock, oldIrql);
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(pCtx->MetaLock)) pXixcoreCtx(%p)\n", pXixcoreCtx ));
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Fail Exit xixcore_AllocVCBLot \n"));
	return -1;
}

void
xixcore_call
xixcore_FreeVCBLot(
	PXIXCORE_VCB	XixcoreVcb,
	xc_uint64 LotIndex
)
{
	xc_int64 BitmapCount = 0;
	PXIXCORE_META_CTX				pXixcoreCtx = NULL;
	XIXCORE_IRQL			oldIrql;


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Enter xixcore_FreeVCBLot \n"));





	XIXCORE_ASSERT_VCB(XixcoreVcb);

	if(XixcoreVcb->IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail xixcore_AllocVCBLot : is Readonly device \n"));
		return ;
	}

	pXixcoreCtx = &XixcoreVcb->MetaContext;

	XIXCORE_ASSERT(pXixcoreCtx->HostFreeLotMap);
	XIXCORE_ASSERT(pXixcoreCtx->HostDirtyLotMap);

	xixcore_AcquireSpinLock(pXixcoreCtx->MetaLock, &oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_lock(&(pCtx->MetaLock)) pXixcoreCtx(%p)\n", pXixcoreCtx ));

	BitmapCount = pXixcoreCtx->HostFreeLotMap->NumLots;
			
	if(xixcore_TestBit(LotIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(pXixcoreCtx->HostDirtyLotMap->Data)))
	{
		xixcore_ClearBit(LotIndex,(unsigned long *)xixcore_GetDataBufferOfBitMap(pXixcoreCtx->HostDirtyLotMap->Data));
		xixcore_SetBit(LotIndex,(unsigned long *)xixcore_GetDataBufferOfBitMap(pXixcoreCtx->HostFreeLotMap->Data));
	}

	if(LotIndex > pXixcoreCtx->HostFreeLotMap->StartIndex){
		pXixcoreCtx->HostFreeLotMap->StartIndex = LotIndex;
		pXixcoreCtx->HostDirtyLotMap->StartIndex = LotIndex;
	}
	XIXCORE_SET_FLAGS(pXixcoreCtx->ResourceFlag, XIXCORE_META_RESOURCE_NEED_UPDATE);
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VCB),
		("Exit xixcore_FreeVCBLot \n"));

	xixcore_ReleaseSpinLock(pXixcoreCtx->MetaLock, oldIrql);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
					("spin_unlock(&(pCtx->MetaLock)) pXixcoreCtx(%p)\n", pXixcoreCtx ));
	return;
}
