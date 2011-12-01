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
#ifndef __XIXCORE_ENDIAN_SAFE_H__
#define __XIXCORE_ENDIAN_SAFE_H__

#include "xixcore/xctypes.h"



int
xixcore_EndianSafeReadLotHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


int
xixcore_EndianSafeWriteLotHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


int
xixcore_EndianSafeReadLotAndFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


int
xixcore_EndianSafeWriteLotAndFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


int
xixcore_EndianSafeReadFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


int
xixcore_EndianSafeWriteFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


// Added by ILGU HONG 12082006
int
xixcore_EndianSafeReadDirEntryHashValueTable(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
);



int
xixcore_EndianSafeWriteDirEntryHashValueTable(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
);
// Added by ILGU HONG 12082006 END



int
xixcore_EndianSafeReadAddrInfo(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);





int
xixcore_EndianSafeWriteAddrInfo(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);






void
readChangeDirEntry(
	PXIDISK_CHILD_INFORMATION	pDirEntry
	);


void
writeChangeDirEntry(
	PXIDISK_CHILD_INFORMATION	pDirEntry
	);

int 
checkMd5DirEntry(
	IN PXIXCORE_BUFFER 	xbuf,
	OUT PXIDISK_CHILD_INFORMATION	*ppDirEntry
);

void
makeMd5DirEntry(
	IN PXIDISK_CHILD_INFORMATION	pDirEntry
);

int
xixcore_EndianSafeReadVolumeHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);



int
xixcore_EndianSafeWriteVolumeHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);



int
xixcore_EndianSafeReadBootSector(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
);


int
xixcore_EndianSafeWriteBootSector(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
);



int
xixcore_EndianSafeReadLotMapHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


int
xixcore_EndianSafeWriteLotMapHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


int
xixcore_EndianSafeReadHostInfo(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


int
xixcore_EndianSafeWriteHostInfo(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);



int
xixcore_EndianSafeReadHostRecord(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


int
xixcore_EndianSafeWriteHostRecord(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize,
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
	);


int
xixcore_EndianSafeReadBitmapData(
	PXIXCORE_BLOCK_DEVICE	bdev, 
	xc_sector_t				startsector, 
	xc_uint32				DataSize,	
	xc_int32				sectorsize, 
	xc_uint32				sectorsizeBit,
	PXIXCORE_BUFFER			xbuf,
	xc_int32				*Reason
);


int
xixcore_EndianSafeWriteBitmapData(
	PXIXCORE_BLOCK_DEVICE	bdev, 
	xc_sector_t				startsector, 
	xc_uint32				DataSize,	
	xc_int32				sectorsize, 
	xc_uint32				sectorsizeBit,
	PXIXCORE_BUFFER			xbuf,
	xc_int32				*Reason
);


#endif //#ifndef __XIXCORE_ENDIAN_SAFE_H__
