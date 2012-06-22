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
#include "xixcore/layouts.h"
#include "xcsystem/system.h"
#include "xixcore/lotaddr.h"

/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCLOTADDR"

xc_sector_t
xixcore_call
xixcore_GetAddrOfLotSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
)
{
	xc_uint64 logicalAddress = 0;
	xc_sector_t logicalSecNum = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter xixcore_GetAddrOfLot\n"));

	logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize );

	XIXCORE_ASSERT(IS_4096_SECTOR(logicalAddress));

	logicalSecNum = (xc_sector_t)SECTOR_ALIGNED_COUNT(SectorsizeBit,logicalAddress);
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfLot logical address(%lld) logical Sec num (%lld)\n", logicalAddress, logicalSecNum));
#else
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfLot logical address(%lld) logical Sec num (%ld)\n", logicalAddress, logicalSecNum));
#endif
	return logicalSecNum;
}


xc_sector_t
xixcore_call
xixcore_GetAddrOfLotLockSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
	)
{
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("call xixcore_GetAddrOfLotLock\n"));
	return xixcore_GetAddrOfLotSec(LotSize, SectorsizeBit, LotIndex);
}


xc_sector_t
xixcore_call
xixcore_GetAddrOfFileHeaderSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
)
{
	xc_uint64 logicalAddress = 0;
	xc_sector_t logicalSecNum = 0;

	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter xixcore_GetAddrOfFileHeader\n"));
	
	logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_DATA_LOT_SIZE;
	
	XIXCORE_ASSERT(IS_4096_SECTOR(logicalAddress));
	
	logicalSecNum = (xc_sector_t)SECTOR_ALIGNED_COUNT(SectorsizeBit,logicalAddress);
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfFileHeader logical address(%lld) logical Sec num (%lld)\n", logicalAddress, logicalSecNum));
#else
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfFileHeader logical address(%lld) logical Sec num (%ld)\n", logicalAddress, logicalSecNum));
#endif
	return logicalSecNum;
}




xc_sector_t
xixcore_call
xixcore_GetAddrOfFileAddrInfoSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
)
{
	xc_uint64 logicalAddress = 0;
	xc_sector_t logicalSecNum = 0;
	

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter xixcore_GetAddrOfFileAddrInfo\n"));

	logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_FILE_HEADER_LOT_SIZE;
	XIXCORE_ASSERT(IS_4096_SECTOR(logicalAddress));

	logicalSecNum = (xc_sector_t)SECTOR_ALIGNED_COUNT(SectorsizeBit,logicalAddress);

#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfFileAddrInfo logical address(%lld) logical Sec num (%lld)\n", logicalAddress, logicalSecNum));
#else
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfFileAddrInfo logical address(%lld) logical Sec num (%ld)\n", logicalAddress, logicalSecNum));
#endif
	return logicalSecNum ;
}



xc_sector_t
xixcore_call
xixcore_GetAddrOfFileDataSec(
		xc_uint32 Flag,
		xc_uint32	LotSize,
		xc_uint32 	SectorsizeBit,
		xc_uint64	LotIndex
		
)
{
	xc_uint64 logicalAddress = 0;
	xc_sector_t logicalSecNum = 0;

	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter xixcore_GetAddrOfFileData\n"));

	if(Flag == LOT_FLAG_BEGIN){
		logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_FILE_HEADER_SIZE;
	}else{
		logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_DATA_LOT_SIZE;
	}
	
	XIXCORE_ASSERT(IS_4096_SECTOR(logicalAddress));

	logicalSecNum = (xc_sector_t)SECTOR_ALIGNED_COUNT(SectorsizeBit,logicalAddress);
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfFileData  logical address(%lld) logical Sec num (%lld)\n", logicalAddress, logicalSecNum));
#else
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfFileData  logical address(%lld) logical Sec num (%ld)\n", logicalAddress, logicalSecNum));
#endif
	return logicalSecNum ;
}






xc_sector_t
xixcore_call
xixcore_GetAddrOfFileAddrInfoFromSec(
	xc_uint32	LotSize,
	xc_uint64	LotIndex,
	xc_uint32	AddrIndex,
	xc_uint64	AuxiAddrLot,
	xc_uint32	AddrLotSize,
	xc_uint32	SectorSizeBit
)
{
	xc_sector_t logicalSecNum = 0;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter xixcore_GetAddrOfFileAddrInfoFromSec\n"));

	if(AddrIndex < XIDISK_ADDR_MAP_INDEX_COUNT(AddrLotSize)){
		logicalSecNum= (xc_sector_t) (xixcore_GetAddrOfFileAddrInfoSec(LotSize, SectorSizeBit, LotIndex) + 
													SECTOR_ALIGNED_COUNT(SectorSizeBit, (AddrIndex*AddrLotSize)) ) ;
	
	}else{
		XIXCORE_ASSERT(AuxiAddrLot > XIFS_RESERVED_LOT_SIZE);
		AddrIndex -= XIDISK_ADDR_MAP_INDEX_COUNT(AddrLotSize);
		logicalSecNum = (xc_sector_t) (xixcore_GetAddrOfFileDataSec(LOT_FLAG_BODY, LotSize, SectorSizeBit, AuxiAddrLot) + 
							SECTOR_ALIGNED_COUNT(SectorSizeBit, (AddrIndex*AddrLotSize)) ) ;
	}
	
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfFileAddrInfoFromSec logical Sec(%lld)\n", logicalSecNum));
#else
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfFileAddrInfoFromSec logical Sec(%ld)\n", logicalSecNum));
#endif
	return logicalSecNum;
}



xc_uint64
xixcore_call
xixcore_GetAddrOfFileData(
		xc_uint32 Flag,
		xc_uint32	LotSize,
		xc_uint64	LotIndex
		
)
{
	xc_uint64 logicalAddress = 0;

	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter xixcore_GetAddrOfFileData\n"));

	if(Flag == LOT_FLAG_BEGIN){
		logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_FILE_HEADER_SIZE;
	}else{
		logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_DATA_LOT_SIZE;
	}
	
	XIXCORE_ASSERT(IS_4096_SECTOR(logicalAddress));

		
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfFileData  logical address(%lld) \n", logicalAddress));
	return logicalAddress ;
}


xc_uint64
xixcore_call
xixcore_GetLogicalStartAddrOfFile(
	xc_uint64		Index,
	xc_uint32		LotSize
)
{
	xc_uint64		LogicalStartAddress = 0;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter GetLogicalStartAddrOfFile\n"));

	if(Index == 0) {
		return 0;
	}else {
		LogicalStartAddress = (xc_uint64)((LotSize - XIDISK_DATA_LOT_SIZE) * (Index -1)) + (LotSize - XIDISK_FILE_HEADER_SIZE);
	}

	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit GetLogicalStartAddrOfFile LogAddr(%lld)\n", LogicalStartAddress));

	return LogicalStartAddress;
}



xc_uint64
xixcore_call
xixcore_GetPhysicalAddrOfFile(
		xc_uint32 Flag,
		xc_uint32	LotSize,
		xc_uint64	LotIndex
)
{
	return   xixcore_GetAddrOfFileData(Flag, LotSize, LotIndex);
}

xc_uint32
xixcore_call
xixcore_GetLotCountOfFileOffset(
	xc_uint32		LotSize,
	xc_uint64		Offset	
)
{
	xc_uint32	Count = 0;
	xc_uint64 	Size = Offset + 1;
	xc_uint64	tmpSize = 0;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter xixcore_GetLotCountOfFileOffset\n"));

	XIXCORE_ASSERT(LotSize > XIDISK_FILE_HEADER_SIZE);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ADDRTRANS, 
					("xixcore/getLotCountOfFileOffset Size(%lld).\n", Size));
	

	if(Size == 0) {
		DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
			("Exit xixcore_GetLotCountOfFileOffset Count 1\n"));
		return 1;
	}
	
	if(Size <= (LotSize - XIDISK_FILE_HEADER_SIZE )){
		DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
			("Exit xixcore_GetLotCountOfFileOffset Count 1\n"));
		return 1;
	}

	Size -= (LotSize - XIDISK_FILE_HEADER_SIZE);

	tmpSize = Size + (LotSize - XIDISK_DATA_LOT_SIZE -1);

	(void)xixcore_Divide64(&tmpSize, (LotSize - XIDISK_DATA_LOT_SIZE));

	Count = (xc_uint32)(tmpSize) + 1;


	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetLotCountOfFileOffset Count(%d)\n", Count));

	return Count;
}



xc_uint32
xixcore_call
xixcore_GetIndexOfLogicalAddress(
	xc_uint32		LotSize,
	xc_uint64		offset
)
{
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("call xixcore_GetIndexOfLogicalAddress\n"));

	return (xixcore_GetLotCountOfFileOffset(LotSize, offset) - 1);	
}




/*
xc_sector_t
xixcore_call
xixcore_GetAddrOfVolLotHeaderSec(
	xc_uint32	SectorsizeBit
)
{
	xc_uint64 logicalAddress = 0;
	xc_sector_t logicalSecNum = 0;

	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter xixcore_GetAddrOfVolLotHeader\n"));
	
	logicalAddress  = XIFS_OFFSET_VOLUME_LOT_LOC + XIDISK_MAP_LOT_SIZE;
	
	XIXCORE_ASSERT(IS_4096_SECTOR(logicalAddress ));

	logicalSecNum = (xc_sector_t)SECTOR_ALIGNED_COUNT(SectorsizeBit,logicalAddress);

#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfVolLotHeader logical address(%lld) logical Sec num (%lld)\n", logicalAddress, logicalSecNum));
#else
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfVolLotHeader logical address(%lld) logical Sec num (%ld)\n", logicalAddress, logicalSecNum));
#endif
	return logicalSecNum ;
}
*/

xc_sector_t
xixcore_call
xixcore_GetAddrOfBitMapHeaderSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("call xixcore_GetAddrOfBitMapHeader\n"));
	return xixcore_GetAddrOfFileHeaderSec(LotSize, SectorsizeBit, LotIndex);
}



xc_sector_t
xixcore_call
xixcore_GetAddrOfBitmapDataSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
)
{
	xc_uint64 logicalAddress = 0;
	xc_sector_t logicalSecNum = 0;

	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter xixcore_GetAddrOfBitmapData\n"));

	logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_MAP_LOT_SIZE;
	XIXCORE_ASSERT(IS_4096_SECTOR(logicalAddress));

	logicalSecNum = (xc_sector_t)SECTOR_ALIGNED_COUNT(SectorsizeBit,logicalAddress);
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfBitmapData logical address(%lld) logical Sec num (%lld)\n", logicalAddress, logicalSecNum));
#else
DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfBitmapData logical address(%lld) logical Sec num (%ld)\n", logicalAddress, logicalSecNum));
#endif


	return logicalSecNum;			
	
}




xc_sector_t
xixcore_call
xixcore_GetAddrOfHostInfoSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	LotIndex
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("call xixcore_GetAddrOfHostInfo\n"));
	return xixcore_GetAddrOfFileHeaderSec(LotSize, SectorsizeBit, LotIndex);
}

xc_sector_t
xixcore_call
xixcore_GetAddrOfHostRecordSec(
	xc_uint32	LotSize,
	xc_uint32	SectorsizeBit,
	xc_uint64	Index,
	xc_uint64	LotIndex
)
{
	xc_uint64 logicalAddress = 0;
	xc_sector_t logicalSecNum = 0;

	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ADDRTRANS, 
		("Enter xixcore_GetAddrOfHostRecord\n"));

	logicalAddress = XIFS_OFFSET_VOLUME_LOT_LOC + (LotIndex * LotSize ) + XIDISK_HOST_REG_LOT_SIZE 
																		+ Index * XIDISK_DUP_HOST_RECORD_SIZE;
	XIXCORE_ASSERT(IS_4096_SECTOR(logicalAddress ));

	logicalSecNum = (xc_sector_t)SECTOR_ALIGNED_COUNT(SectorsizeBit,logicalAddress);
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfHostRecord logical address(%lld) logical Sec num (%lld)\n", logicalAddress, logicalSecNum));
#else
	DebugTrace((DEBUG_LEVEL_TRACE|DEBUG_LEVEL_INFO), DEBUG_TARGET_ADDRTRANS, 
		("Exit xixcore_GetAddrOfHostRecord logical address(%lld) logical Sec num (%ld)\n", logicalAddress, logicalSecNum));
#endif
	return logicalSecNum;			
}



