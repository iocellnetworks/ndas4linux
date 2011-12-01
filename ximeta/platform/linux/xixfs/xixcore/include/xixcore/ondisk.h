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
#ifndef __XIXCORE_ON_DISK_H__
#define __XIXCORE_ON_DISK_H__

#include "xcsystem/system.h"
#include "xixcore/xctypes.h"
#include "xixcore/buffer.h"
#include "xixcore/layouts.h"

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
);

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
);





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
);

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
);
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
);

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
);
// Added by ILGU HONG 12082006 END

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
);

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
);



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
);

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
);



/*
 *	Get and Set Dir Entry Info
 */

void
xixcore_call
xixcore_GetDirEntryInfo(
	IN PXIDISK_CHILD_INFORMATION	Buffer
);

void
xixcore_call
xixcore_SetDirEntryInfo(
	IN PXIDISK_CHILD_INFORMATION	Buffer
);



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
);

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
);

/*
*	Read and Write bootsector
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
);

int
xixcore_call
xixcore_RawWriteBootSector(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_uint32 SectorSize,
	xc_uint32 SectorSizeBit,
	xc_sector_t StartSec,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
);


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
);

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
);


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
);

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
);

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
);

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
);


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
);


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
);

#endif //#ifndef __XIXCORE_ON_DISK_H__
