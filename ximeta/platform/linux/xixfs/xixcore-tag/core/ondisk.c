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
#include "xixcore/layouts.h"
#include "xcsystem/system.h"
#include "xixcore/lotaddr.h"
#include "xixcore/ondisk.h"
#include "internal_types.h"
#include "endiansafe.h"

/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCONDISK"

/*
 *	Function must be done within waitable thread context
 */

/*
 *	Read and Write Lot Header
 */

int
xixcore_call
xixcore_RawReadLotHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex, 
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawReadLotHeader \n"));

	xixcore_ZeroBufferOffset(xbuf);

	if(xixcore_GetBufferSize(xbuf) < XIDISK_DUP_COMMON_LOT_HEADER_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}


	lsn = xixcore_GetAddrOfLotSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeReadLotHeader(
			bdev,
			lsn,
			XIDISK_DUP_COMMON_LOT_HEADER_SIZE, 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawReadLotHeader \n"));

	return RC;
}


int
xixcore_call
xixcore_RawWriteLotHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex, 
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawWriteLotHeader \n"));


	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_COMMON_LOT_HEADER_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}


	lsn = xixcore_GetAddrOfLotSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeWriteLotHeader(
			bdev,
			lsn,
			XIDISK_COMMON_LOT_HEADER_SIZE, 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawWriteLotHeader \n"));

	return RC;
}


	
	







/*
 *	Read and Write Lot and File Header
 */

/*
int
xixcore_call
xixcore_RawReadLotAndFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawReadLotAndFileHeader \n"));


	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_FILE_HEADER_LOT_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}
	
	

	lsn = xixcore_GetAddrOfLotSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeReadLotAndFileHeader(
			bdev,
			lsn,
			(XIDISK_FILE_HEADER_LOT_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawReadLotAndFileHeader\n"));

	return RC;
}



int
xixcore_call
xixcore_RawWriteLotAndFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawWriteLotAndFileHeader \n"));


	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_FILE_HEADER_LOT_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}
	
	

	lsn = xixcore_GetAddrOfLotSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeWriteLotAndFileHeader(
			bdev,
			lsn,
			(XIDISK_FILE_HEADER_LOT_SIZE),
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawWriteLotAndFileHeader\n"));

	return RC;
}
*/


// Added by ILGU HONG 12082006
/*
 *  Read and Write Directory Entry Hash Value table
 */
int
xixcore_call
xixcore_RawReadDirEntryHashValueTable(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawReadDirEntryHashValueTable \n"));
	

	xixcore_ZeroBufferOffset(xbuf);	

	if(xixcore_GetBufferSize(xbuf) < XIDISK_DUP_HASH_VALUE_TABLE_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}


	lsn= xixcore_GetAddrOfFileDataSec(LOT_FLAG_BEGIN, LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeReadDirEntryHashValueTable(
			bdev,
			lsn,
			(XIDISK_DUP_HASH_VALUE_TABLE_SIZE),
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawReadDirEntryHashValueTable \n"));

	return RC;
}

int
xixcore_call
xixcore_RawWriteDirEntryHashValueTable(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawWriteDirEntryHashValueTable \n"));
	

	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_HASH_VALUE_TABLE_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}


	lsn= xixcore_GetAddrOfFileDataSec(LOT_FLAG_BEGIN, LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeWriteDirEntryHashValueTable(
			bdev,
			lsn,
			(XIDISK_HASH_VALUE_TABLE_SIZE),
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawWriteDirEntryHashValueTable \n"));

	return RC;
}

/*
 *	Read and Write File Header
 */






/*
 *	Read and Write File Header
 */

int
xixcore_call
xixcore_RawReadFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawReadFileHeader \n"));
	
	xixcore_ZeroBufferOffset(xbuf);	

	if(xixcore_GetBufferSize(xbuf) < XIDISK_DUP_DIR_INFO_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}


	lsn = xixcore_GetAddrOfFileHeaderSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeReadFileHeader(
			bdev,
			lsn,
			(XIDISK_DUP_DIR_INFO_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawReadFileHeader\n"));

	return RC;
}



int
xixcore_call
xixcore_RawWriteFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawWriteFileHeader \n"));
	
	

	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_FILE_INFO_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}


	lsn = xixcore_GetAddrOfFileHeaderSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeWriteFileHeader(
			bdev,
			lsn,
			(XIDISK_FILE_INFO_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawWriteFileHeader\n"));

	return RC;
}






/*
 *		Read and Write Address of File
 */
int
xixcore_call
xixcore_RawReadAddressOfFile(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	xc_uint64 AdditionalLotNumber,
	xc_uint32 AddrLotSize,
	xc_uint32 *AddrStartSecIndex,
	xc_uint32 NewAddrLotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Enter xixcore_RawReadAddressOfFile\n"));
	
	if(xixcore_GetBufferSizeWithOffset(xbuf) < AddrLotSize){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}

	if(NewAddrLotIndex >= (xc_uint32)XIDISK_ADDR_MAP_INDEX_COUNT(AddrLotSize)){
		XIXCORE_ASSERT(AdditionalLotNumber > XIFS_RESERVED_LOT_SIZE);
	}

	lsn = xixcore_GetAddrOfFileAddrInfoFromSec(
									LotSize,
									LotIndex,
									NewAddrLotIndex,
									AdditionalLotNumber,
									AddrLotSize,
									SectorSizeBit
									);
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__		
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("xixcore_RawReadAddressOfFile of FCB(%lldd) Index(%d) Lot Infor Loc(%lld)\n", 
			LotIndex, NewAddrLotIndex, lsn));
#else
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("xixcore_RawReadAddressOfFile of FCB(%lldd) Index(%d) Lot Infor Loc(%ld)\n", 
			LotIndex, NewAddrLotIndex, lsn));
#endif
	RC = xixcore_EndianSafeReadAddrInfo(
									bdev,
									lsn,
									AddrLotSize,
									SectorSize,
									SectorSizeBit,
									xbuf, 
									Reason);	
	
	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_RawReadAddressOfFile:XixFsEndianSafeReadAddrInfo Status(0x%x)\n", RC));

		return RC;
	}

	*AddrStartSecIndex = NewAddrLotIndex;
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("After xixcore_RawReadAddressOfFile of FCB(%lld) Sec(%d)\n", 
				LotIndex,NewAddrLotIndex));
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Exit xixcore_RawReadAddressOfFile Status(0x%x)\n", RC));	
	return RC;
}


int
xixcore_call
xixcore_RawWriteAddressOfFile(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	xc_uint64 AdditionalLotNumber,
	xc_uint32 AddrLotSize,
	xc_uint32 *AddrStartSecIndex,
	xc_uint32 NewAddrLotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Enter xixcore_RawWriteAddressOfFile\n"));
	
	if(xixcore_GetBufferSizeWithOffset(xbuf) < AddrLotSize){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}

	
	if(NewAddrLotIndex >= (xc_uint32)XIDISK_ADDR_MAP_INDEX_COUNT(AddrLotSize)){
		XIXCORE_ASSERT(AdditionalLotNumber > XIFS_RESERVED_LOT_SIZE);
	}

	lsn = xixcore_GetAddrOfFileAddrInfoFromSec(
									LotSize, 
									LotIndex, 
									NewAddrLotIndex, 
									AdditionalLotNumber, 
									AddrLotSize,
									SectorSizeBit
									);
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__		
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("xixcore_RawWriteAddressOfFile of FCB(%lld) Index(%d) Lot Infor Loc(%lld)\n", 
			LotIndex, NewAddrLotIndex, lsn));
#else
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("xixcore_RawWriteAddressOfFile of FCB(%lld) Index(%d) Lot Infor Loc(%ld)\n", 
			LotIndex, NewAddrLotIndex, lsn));
#endif
	RC = xixcore_EndianSafeWriteAddrInfo(
									bdev,
									lsn,
									AddrLotSize,
									SectorSize,
									SectorSizeBit,
									xbuf, 
									Reason);	
	
	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_RawWriteAddressOfFile:XixFsEndianSafeReadAddrInfo Status(0x%x)\n", RC));

		return RC;
	}

	*AddrStartSecIndex = NewAddrLotIndex;
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
			("After xixcore_RawWriteAddressOfFile of FCB(%lld) \n", 
				LotIndex));
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_ADDRTRANS|DEBUG_TARGET_FCB), 
		("Exit xixcore_RawWriteAddressOfFile Status(0x%x)\n", RC));	
	return RC;
}







/*
 *	Get and Set Dir Entry Info
 */

void
xixcore_call
xixcore_GetDirEntryInfo(
	IN PXIDISK_CHILD_INFORMATION	Buffer
)
{
#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeDirEntry(Buffer);
#else
	XIXCORE_UNREFERENCED_PARAMETER(Buffer);
#endif 
	return;
}


void
xixcore_call
xixcore_SetDirEntryInfo(
	IN PXIDISK_CHILD_INFORMATION	Buffer
)
{
#if defined(__BIG_ENDIAN_BITFIELD)	
		writeChangeDirEntry((PXIDISK_CHILD_INFORMATION)Buffer);
#else
	XIXCORE_UNREFERENCED_PARAMETER(Buffer);
#endif 
	return;
}



/*
 *	Read Volume Header
 */

int
xixcore_call
xixcore_RawReadVolumeHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_sector_t LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawReadVolumeHeader\n"));

	xixcore_ZeroBufferOffset(xbuf);	

	if(xixcore_GetBufferSize(xbuf) < XIDISK_DUP_VOLUME_INFO_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	
	
	

	lsn= xixcore_GetAddrOfFileHeaderSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeReadVolumeHeader(
			bdev,
			lsn,
			(XIDISK_DUP_VOLUME_INFO_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawReadVolumeHeader \n"));
	
	return RC;
}



int
xixcore_call
xixcore_RawWriteVolumeHeader(
	PXIXCORE_BLOCK_DEVICE bdev,
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_sector_t LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawWriteVolumeHeader\n"));

	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_VOLUME_INFO_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	
	
	

	lsn= xixcore_GetAddrOfFileHeaderSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeWriteVolumeHeader(
			bdev,
			lsn,
			(XIDISK_VOLUME_INFO_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawWriteVolumeHeader \n"));
	
	return RC;
}


/*
  *		Read and Write BootSector
  */

int
xixcore_call
 xixcore_RawReadBootSector(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_sector_t StartSec,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawReadVolumeHeader\n"));

	xixcore_ZeroBufferOffset(xbuf);	

	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_VOLUME_INFO_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	
	
	

	lsn= StartSec;
	
	RC = xixcore_EndianSafeReadBootSector(
			bdev,
			lsn,
			SectorSize, 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawReadVolumeHeader \n"));
	
	return RC;
}



int
xixcore_call
xixcore_RawWriteBootSector(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_sector_t StartSec,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawWriteVolumeHeader\n"));

	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_VOLUME_INFO_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	
	
	

	lsn= StartSec;
	
	RC = xixcore_EndianSafeWriteBootSector(
			bdev,
			lsn,
			SectorSize, 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawWriteVolumeHeader \n"));
	
	return RC;
}



/*
 *	Read and Write Lot Map Header
 */
int
xixcore_call
xixcore_RawReadLotMapHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_sector_t LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawReadLotMapHeader\n"));

	xixcore_ZeroBufferOffset(xbuf);	

	if(xixcore_GetBufferSize(xbuf) < XIDISK_DUP_LOT_MAP_INFO_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	

	XIXCORE_ASSERT(xbuf);

	lsn= xixcore_GetAddrOfFileHeaderSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeReadLotMapHeader(
			bdev,
			lsn,
			(XIDISK_DUP_LOT_MAP_INFO_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawReadLotMapHeader \n"));
	
	return RC;
}



int
xixcore_call
xixcore_RawWriteLotMapHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_sector_t LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawWriteLotMapHeader\n"));


	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_LOT_MAP_INFO_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	
	

	lsn= xixcore_GetAddrOfFileHeaderSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeWriteLotMapHeader(
			bdev,
			lsn,
			(XIDISK_LOT_MAP_INFO_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawWriteLotMapHeader\n"));
	
	return RC;
}





/*
 *	Read and Write Register Host Info
 */

int
xixcore_call
xixcore_RawReadRegisterHostInfo(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawReadRegisterHostInfo\n"));
	
	xixcore_ZeroBufferOffset(xbuf);	

	if(xixcore_GetBufferSize(xbuf) < XIDISK_DUP_HOST_INFO_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	


	lsn  = xixcore_GetAddrOfHostInfoSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeReadHostInfo(
			bdev,
			lsn,
			(XIDISK_DUP_HOST_INFO_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawReadRegisterHostInfo \n"));
	
	return RC;
}


int
xixcore_call
xixcore_RawWriteRegisterHostInfo(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawWriteRegisterHostInfo \n"));
	
	

	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_HOST_INFO_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	


	lsn  = xixcore_GetAddrOfHostInfoSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeWriteHostInfo(
			bdev,
			lsn,
			(XIDISK_HOST_INFO_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawWriteRegisterHostInfo \n"));
	
	return RC;
}


/*
 *	Read and Write Register Record Info
 */
int
xixcore_call
xixcore_RawReadRegisterRecord(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	xc_uint64 Index,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawReadRegisterRecord \n"));

	xixcore_ZeroBufferOffset(xbuf);	

	if(xixcore_GetBufferSize(xbuf) < XIDISK_DUP_HOST_RECORD_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	


	lsn = xixcore_GetAddrOfHostRecordSec(LotSize, SectorSizeBit, Index, LotIndex);
	
	RC = xixcore_EndianSafeReadHostRecord(
			bdev,
			lsn,
			(XIDISK_DUP_HOST_RECORD_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawReadRegisterRecord \n"));
	
	return RC;
}


int
xixcore_call
xixcore_RawWriteRegisterRecord(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	xc_uint32 Index,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawWriteRegisterRecord \n"));

	if(xixcore_GetBufferSizeWithOffset(xbuf) < XIDISK_HOST_RECORD_SIZE){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	


	lsn = xixcore_GetAddrOfHostRecordSec(LotSize, SectorSizeBit, Index, LotIndex);
	
	RC = xixcore_EndianSafeWriteHostRecord(
			bdev,
			lsn,
			(XIDISK_HOST_RECORD_SIZE), 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawWriteRegisterRecord\n"));
	
	return RC;
}



/*
 *	Read and Write Bitmap Data
 */
int
xixcore_call
xixcore_RawReadBitmapData(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	xc_uint32 DataSize,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawReadBitmapData \n"));

	xixcore_ZeroBufferOffset(xbuf);	
	
	if(xixcore_GetBufferSize(xbuf) < (DataSize * 2)){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	

	memset(xixcore_GetDataBuffer(xbuf), 0, (DataSize * 2));


	lsn = xixcore_GetAddrOfBitmapDataSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeReadBitmapData(
			bdev,
			lsn,
			DataSize, 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawReadBitmapData \n"));
	
	return RC;
}


int
xixcore_call
xixcore_RawWriteBitmapData(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 LotSize,
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_uint64 LotIndex,
	xc_uint32 DataSize,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int		RC = 0;
	xc_sector_t	lsn = 0;
	*Reason = 0;
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Enter xixcore_RawWriteBitmapData \n"));

	if(xixcore_GetBufferSizeWithOffset(xbuf) < DataSize){
		*Reason = XCREASON_BUF_SIZE_SMALL;
		return XCCODE_UNSUCCESS;
	}	


	lsn = xixcore_GetAddrOfBitmapDataSec(LotSize, SectorSizeBit, LotIndex);
	
	RC = xixcore_EndianSafeWriteBitmapData(
			bdev,
			lsn,
			DataSize, 
			SectorSize,
			SectorSizeBit,
			xbuf,
			Reason
			);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DEVCTL|DEBUG_TARGET_FCB),
		("Exit xixcore_RawWriteBitmapData\n"));
	
	return RC;
}


