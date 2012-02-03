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
#ifndef __XIXCORE_LOT_INFO_H__
#define __XIXCORE_LOT_INFO_H__

#include "xixcore/xctypes.h"
#include "xixcore/layouts.h"

typedef struct _XIXCORE_IO_LOT_INFO {
	xc_uint64						LogicalStartOffset;
	xc_uint64						PhysicalAddress;
	xc_uint32						Type;						
	xc_uint32						Flags;						
	xc_uint64						LotIndex;					
	xc_uint64						BeginningLotIndex;			
	xc_uint64						PreviousLotIndex;			
	xc_uint64						NextLotIndex;
	xc_uint32						StartDataOffset;
	xc_uint32						LotTotalDataSize;			
	xc_uint32						LotUsedDataSize;			
}XIXCORE_IO_LOT_INFO, *PXIXCORE_IO_LOT_INFO;


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
);

int
xixcore_call
xixcore_CheckLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN	xc_uint32				VolLotSignature,
	IN 	xc_int64				LotNumber,
	IN 	xc_uint32				Type,
	IN 	xc_uint32				Flags,
	IN OUT	xc_int32			*Reason
);

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
);

void
xixcore_call
xixcore_GetLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN OUT PXIXCORE_IO_LOT_INFO 	pAddressInfo
);

void
xixcore_call
xixcore_SetLotInfo(
	IN	PXIDISK_LOT_INFO	pLotInfo,
	IN 	PXIXCORE_IO_LOT_INFO 	pAddressInfo
);

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
	
);



#endif // #ifndef __XIXCORE_LOT_INFO_H__
