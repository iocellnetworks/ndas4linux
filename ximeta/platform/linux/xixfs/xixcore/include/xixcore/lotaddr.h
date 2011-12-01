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
#ifndef __XIXCORE_LOTADDR_H__
#define __XIXCORE_LOTADDR_H__

#include "xixcore/xctypes.h"


xc_sector_t
xixcore_call
xixcore_GetAddrOfLotSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
);

xc_sector_t
xixcore_call
xixcore_GetAddrOfLotLockSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
	);

xc_sector_t
xixcore_call
xixcore_GetAddrOfFileHeaderSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
);

xc_sector_t
xixcore_call
xixcore_GetAddrOfFileAddrInfoSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
);

xc_sector_t
xixcore_call
xixcore_GetAddrOfFileDataSec(
		xc_uint32 Flag,
		xc_uint32	LotSize,
		xc_uint32 	SectorsizeBit,
		xc_uint64	LotIndex
		
);

xc_sector_t
xixcore_call
xixcore_GetAddrOfFileAddrInfoFromSec(
	xc_uint32	LotSize,
	xc_uint64	LotIndex,
	xc_uint32	AddrIndex,
	xc_uint64	AuxiAddrLot,
	xc_uint32	AddrLotSize,
	xc_uint32	SectorSizeBit
);

xc_uint64
xixcore_call
xixcore_GetAddrOfFileData(
		xc_uint32 Flag,
		xc_uint32	LotSize,
		xc_uint64	LotIndex
		
);

xc_uint64
xixcore_call
xixcore_GetLogicalStartAddrOfFile(
	xc_uint64		Index,
	xc_uint32		LotSize
);


xc_uint64
xixcore_call
xixcore_GetPhysicalAddrOfFile(
		xc_uint32 Flag,
		xc_uint32	LotSize,
		xc_uint64	LotIndex
);

xc_uint32
xixcore_call
xixcore_GetLotCountOfFileOffset(
	xc_uint32		LotSize,
	xc_uint64		Offset	
);

xc_uint32
xixcore_call
xixcore_GetIndexOfLogicalAddress(
	xc_uint32		LotSize,
	xc_uint64		offset
);

/*
xc_sector_t
xixcore_call
xixcore_GetAddrOfVolLotHeaderSec(
	xc_uint32	SectorsizeBit
);
*/

xc_sector_t
xixcore_call
xixcore_GetAddrOfBitMapHeaderSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
);

xc_sector_t
xixcore_call
xixcore_GetAddrOfBitmapDataSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
);

xc_sector_t
xixcore_call
xixcore_GetAddrOfHostInfoSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
);

xc_sector_t
xixcore_call
xixcore_GetAddrOfHostRecordSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	Index,
	xc_uint64	LotIndex
);


#endif //#ifndef __XIXCORE_LOTADDR_H__
