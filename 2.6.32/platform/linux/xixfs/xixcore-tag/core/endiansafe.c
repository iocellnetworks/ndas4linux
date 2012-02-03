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
#include "xcsystem/system.h"
#include "xcsystem/errinfo.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/buffer.h"
#include "xixcore/ondisk.h"
#include "xixcore/md5.h"
#include "endiansafe.h"

/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCENDSAFE"

/*
 * On-disk time value conversion
 */

#define XIXFS_TIME_OFFSET ((xc_uint64)(369 * 365 + 89) * 24 * 3600 * 10000000)

XIXCORE_INLINE xc_uint64 xixcore_GetTime(const xc_uint64 time)
{
	return (xc_uint64)(time - XIXFS_TIME_OFFSET);
}

XIXCORE_INLINE xc_uint64 xixcore_SetTime(const xc_uint64 time)
{
	return (xc_uint64)(time + XIXFS_TIME_OFFSET);
}

/*
 *	Function must be done within waitable thread context
 */


static int
compareSequence(
	xc_uint64 TestEvenSequence,
	xc_uint64 TestOddSequence
)
{
	xc_uint64 EvenSequenceNum = 0;
	xc_uint64 OddSequenceNum = 0;

	EvenSequenceNum = TestEvenSequence;
	OddSequenceNum = TestOddSequence;

	XIXCORE_LE2CPU64p(&EvenSequenceNum);
	XIXCORE_LE2CPU64p(&OddSequenceNum);
	
	if((EvenSequenceNum + 1) > OddSequenceNum) {
		return 1;
	}else {
		return -1;
	}
}



/*
 *	Lot Lock Info
 */

static void
readChangeLockInfo(
	IN PXIDISK_LOCK					Lock
)
{
	// Lock Info
	XIXCORE_LE2CPU32p(&(Lock->LockState));
	XIXCORE_LE2CPU64p(&(Lock->LockAcquireTime));

	return;
}


static void
writeChangeLockInfo(
	IN PXIDISK_LOCK					Lock
)
{
	// Lock Info
	XIXCORE_CPU2LE32p(&(Lock->LockState));
	XIXCORE_CPU2LE64p(&(Lock->LockAcquireTime));

	return;
}

static void
readCVLockInfo(
	PXIDISK_LOCK					Lock
)
{
	Lock->LockAcquireTime = xixcore_GetTime(Lock->LockAcquireTime);
}

static void
writeCVLockInfo(
	PXIDISK_LOCK					Lock
)
{
	Lock->LockAcquireTime = xixcore_SetTime(Lock->LockAcquireTime);
}


/*
 *	Lot Header
 */
static void
readChangeLotHeader(
	IN PXIDISK_COMMON_LOT_HEADER	pLotHeader
)
{
	// Lock Info
	readChangeLockInfo(&pLotHeader->Lock);
	


	// Lot Header
	XIXCORE_LE2CPU32p(&(pLotHeader->LotInfo.Type));
	XIXCORE_LE2CPU32p(&(pLotHeader->LotInfo.Flags));
	XIXCORE_LE2CPU64p(&(pLotHeader->LotInfo.LotIndex));
	XIXCORE_LE2CPU64p(&(pLotHeader->LotInfo.BeginningLotIndex));
	XIXCORE_LE2CPU64p(&(pLotHeader->LotInfo.PreviousLotIndex));
	XIXCORE_LE2CPU64p(&(pLotHeader->LotInfo.NextLotIndex));
	XIXCORE_LE2CPU64p(&(pLotHeader->LotInfo.LogicalStartOffset));
	XIXCORE_LE2CPU32p(&(pLotHeader->LotInfo.StartDataOffset));
	XIXCORE_LE2CPU32p(&(pLotHeader->LotInfo.LotTotalDataSize));
	XIXCORE_LE2CPU32p(&(pLotHeader->LotInfo.LotSignature));
	XIXCORE_LE2CPU64p(&(pLotHeader->SequenceNum));
	return;
}



static void
writeChangeLotHeader(
	IN PXIDISK_COMMON_LOT_HEADER	pLotHeader
)
{
	// Lock Info
	writeChangeLockInfo(&pLotHeader->Lock);


	// Lot Header
	XIXCORE_CPU2LE32p(&(pLotHeader->LotInfo.Type));
	XIXCORE_CPU2LE32p(&(pLotHeader->LotInfo.Flags));
	XIXCORE_CPU2LE64p(&(pLotHeader->LotInfo.LotIndex));
	XIXCORE_CPU2LE64p(&(pLotHeader->LotInfo.BeginningLotIndex));
	XIXCORE_CPU2LE64p(&(pLotHeader->LotInfo.PreviousLotIndex));
	XIXCORE_CPU2LE64p(&(pLotHeader->LotInfo.NextLotIndex));
	XIXCORE_CPU2LE64p(&(pLotHeader->LotInfo.LogicalStartOffset));
	XIXCORE_CPU2LE32p(&(pLotHeader->LotInfo.StartDataOffset));
	XIXCORE_CPU2LE32p(&(pLotHeader->LotInfo.LotTotalDataSize));
	XIXCORE_CPU2LE32p(&(pLotHeader->LotInfo.LotSignature));
	XIXCORE_CPU2LE64p(&(pLotHeader->SequenceNum));	
	return;
}


static void
readCV_LotHeader(
	PXIDISK_COMMON_LOT_HEADER	pLotHeader
)
{
	readCVLockInfo(&pLotHeader->Lock);
	
}


static void
writeCV_LotHeader(
	PXIDISK_COMMON_LOT_HEADER	pLotHeader
)
{
	writeCVLockInfo(&pLotHeader->Lock);
	

}






static
int 
checkMd5LotHeader(
    IN PXIXCORE_BUFFER xbuf,
	OUT PXIDISK_COMMON_LOT_HEADER		*ppLotHeader
)
{
	PXIDISK_DUP_COMMON_LOT_HEADER	pDupLotHeader = NULL;
	PXIDISK_COMMON_LOT_HEADER	pLotHeader = NULL;
	xc_uint32	out[4];
	xc_uint32	*digest;
	xc_uint32	IsOkFirstCheck = 0;

	XIXCORE_ASSERT(xbuf);
	pDupLotHeader = (PXIDISK_DUP_COMMON_LOT_HEADER)xixcore_GetDataBuffer(xbuf);
	/* First check */
	pLotHeader = &(pDupLotHeader->LotHeaderEven);


	xixcore_md5digest_metadata((xc_uint8 *)pLotHeader, ( sizeof(XIDISK_COMMON_LOT_HEADER)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pLotHeader->Digest);
	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5LotHeader pLotHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pLotHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL EVEN checkMd5LotHeader pLotHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pLotHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

		IsOkFirstCheck = 0;
	}else {
		IsOkFirstCheck = 1;
	}

	/* Second Check */
	pLotHeader = &(pDupLotHeader->LotHeaderOdd);

	xixcore_md5digest_metadata((xc_uint8 *)pLotHeader, ( sizeof(XIDISK_COMMON_LOT_HEADER)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pLotHeader->Digest);
	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5LotHeader pLotHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pLotHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL ODD checkMd5LotHeader pLotHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pLotHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));


		if( IsOkFirstCheck == 1) {
			*ppLotHeader = &(pDupLotHeader->LotHeaderEven);
			return XCCODE_SUCCESS;
		}else {
			/*
			xixcore_DebugPrintf("FaIL ALL checkMd5LotHeader pLotHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
				pLotHeader,
				out[0],out[1],out[2],out[3],
				digest[0], digest[1], digest[2], digest[3]);
			*/
			return XCCODE_CRCERROR;
		}

	}else {
		if(IsOkFirstCheck == 1){
			if( compareSequence(
					pDupLotHeader->LotHeaderEven.SequenceNum, 
					pDupLotHeader->LotHeaderOdd.SequenceNum 
					)  >= 0 )
			{
				*ppLotHeader = &(pDupLotHeader->LotHeaderEven);
				return XCCODE_SUCCESS;
			}else{
				*ppLotHeader = &(pDupLotHeader->LotHeaderOdd);
				xixcore_IncreaseBufferOffset(xbuf, XIDISK_COMMON_LOT_HEADER_SIZE);
				return XCCODE_SUCCESS;
			}
		}else{
			*ppLotHeader = &(pDupLotHeader->LotHeaderOdd);
			xixcore_IncreaseBufferOffset(xbuf, XIDISK_COMMON_LOT_HEADER_SIZE);
			return XCCODE_SUCCESS;
		}
	}
}


static 
void
makeMd5LotHeader(
	IN PXIDISK_COMMON_LOT_HEADER	pLotHeader
)
{
	xc_uint32	*digest;

	xixcore_md5digest_metadata((xc_uint8 *)pLotHeader, ( sizeof(XIDISK_COMMON_LOT_HEADER) - XIXCORE_MD5DIGEST_SIZE), pLotHeader->Digest);
	digest = (xc_uint32 *)pLotHeader->Digest;

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL,
		("makeMd5LotHeader pLotHeader(0x%p) lotnumber(0x%x%x%x%x)\n", 
		pLotHeader,digest[0],digest[1],digest[2],digest[3]));
}


int
xixcore_EndianSafeReadLotHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_COMMON_LOT_HEADER		pLotHeader = NULL;
	*Reason = 0;
	
	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);	
		
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadLotHeader Status(0x%x)\n", RC));

		return RC;
	}		

	RC = checkMd5LotHeader(xbuf, &pLotHeader);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error checkMd5LotHeader Status(0x%x)\n", RC));

		return RC;
	}		

#if defined(__BIG_ENDIAN_BITFIELD)
	readChangeLotHeader(pLotHeader);
#endif


	readCV_LotHeader(pLotHeader);

	return RC;
}


int
xixcore_EndianSafeWriteLotHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_COMMON_LOT_HEADER	pLotHeader = NULL;
	xc_sector_t newStartsector = 0;
	*Reason = 0;
	
	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(xbuf);	
	pLotHeader->SequenceNum++;

	if(pLotHeader->SequenceNum % 2) {
		newStartsector = startsector + SECTOR_ALIGNED_COUNT(sectorsizeBit, XIDISK_COMMON_LOT_HEADER_SIZE); 
	}else{
		newStartsector = startsector;
	}


	writeCV_LotHeader(pLotHeader);
#if defined(__BIG_ENDIAN_BITFIELD)
	writeChangeLotHeader(pLotHeader);
#endif

	makeMd5LotHeader(pLotHeader);

	RC = xixcore_DeviceIoSync(
			bdev,
			newStartsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);	
		
	if( RC < 0 ){
#if defined(__BIG_ENDIAN_BITFIELD)		
		readChangeLotHeader(pLotHeader);
#endif
		readCV_LotHeader(pLotHeader);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeWriteLotHeader Status(0x%x)\n", RC));

		return RC;
	}		

	
#if defined(__BIG_ENDIAN_BITFIELD)
	readChangeLotHeader(pLotHeader);
#endif
	readCV_LotHeader(pLotHeader);
	return RC;
}







/*
 *	File Header
 */

static void
readChangeFileHeader(
	IN PXIDISK_DIR_INFO	pDirHeader
)
{
	//xc_uint32 NameSize = 0;
	//xc_uint32 i = 0;
	//xc_uint16 * Name = NULL;



	// FileHeader

	XIXCORE_LE2CPU32p(&(pDirHeader->State));
	XIXCORE_LE2CPU32p(&(pDirHeader->Type));

	XIXCORE_LE2CPU64p(&(pDirHeader->ParentDirLotIndex));
	XIXCORE_LE2CPU64p(&(pDirHeader->FileSize));
	XIXCORE_LE2CPU64p(&(pDirHeader->AllocationSize));

	XIXCORE_LE2CPU32p(&(pDirHeader->FileAttribute));

	XIXCORE_LE2CPU64p(&(pDirHeader->LotIndex));
	XIXCORE_LE2CPU64p(&(pDirHeader->AddressMapIndex));

	XIXCORE_LE2CPU32p(&(pDirHeader->LinkCount));

	XIXCORE_LE2CPU64p(&(pDirHeader->Create_time));
	XIXCORE_LE2CPU64p(&(pDirHeader->Change_time));
	XIXCORE_LE2CPU64p(&(pDirHeader->Modified_time));
	XIXCORE_LE2CPU64p(&(pDirHeader->Access_time));

	XIXCORE_LE2CPU32p(&(pDirHeader->AccessFlags));
	XIXCORE_LE2CPU32p(&(pDirHeader->ACLState));
	XIXCORE_LE2CPU32p(&(pDirHeader->NameSize));
	XIXCORE_LE2CPU32p(&(pDirHeader->childCount));
	XIXCORE_LE2CPU64p(&(pDirHeader->SequenceNum));
	/*
	 *	 Change Unicode string order
	 */
	 /*
	 // it's done in xixfs file system
	NameSize = (xc_uint32)(pDirHeader->NameSize/2);
	Name = (xc_uint16 *)pDirHeader->Name;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XIXCORE_LE2CPU16p(&Name[i]);
		}
	}
	*/
	return;
}

static void
writeChangeFileHeader(
	IN PXIDISK_DIR_INFO	pDirHeader
)
{
	//xc_uint32 NameSize = 0;
	//xc_uint32 i = 0;
	//xc_uint16 * Name = NULL;

	// FileHeader

	XIXCORE_CPU2LE32p(&(pDirHeader->State));
	XIXCORE_CPU2LE32p(&(pDirHeader->Type));

	XIXCORE_CPU2LE64p(&(pDirHeader->ParentDirLotIndex));
	XIXCORE_CPU2LE64p(&(pDirHeader->FileSize));
	XIXCORE_CPU2LE64p(&(pDirHeader->AllocationSize));

	XIXCORE_CPU2LE32p(&(pDirHeader->FileAttribute));

	XIXCORE_CPU2LE64p(&(pDirHeader->LotIndex));
	XIXCORE_CPU2LE64p(&(pDirHeader->AddressMapIndex));

	XIXCORE_CPU2LE32p(&(pDirHeader->LinkCount));

	XIXCORE_CPU2LE64p(&(pDirHeader->Create_time));
	XIXCORE_CPU2LE64p(&(pDirHeader->Change_time));
	XIXCORE_CPU2LE64p(&(pDirHeader->Modified_time));
	XIXCORE_CPU2LE64p(&(pDirHeader->Access_time));

	XIXCORE_CPU2LE32p(&(pDirHeader->AccessFlags));
	XIXCORE_CPU2LE32p(&(pDirHeader->ACLState));
	XIXCORE_CPU2LE32p(&(pDirHeader->NameSize));
	XIXCORE_CPU2LE32p(&(pDirHeader->childCount));
	XIXCORE_CPU2LE64p(&(pDirHeader->SequenceNum));
	/*
	 *	 Change Unicode string order
	 */
	 /*
	 // it's done in xixfs file system
	NameSize = (xc_uint32)(pDirHeader->NameSize/2);
	Name = (xc_uint16 *)pDirHeader->Name;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XIXCORE_CPU2LE16p(&Name[i]);
		}
	}
	*/
	return;
}




static void
readCV_FileHeader(
	IN PXIDISK_DIR_INFO	pDirHeader
)
{
	pDirHeader->Create_time = xixcore_GetTime(pDirHeader->Create_time);
	pDirHeader->Change_time = xixcore_GetTime(pDirHeader->Change_time);
	pDirHeader->Modified_time = xixcore_GetTime(pDirHeader->Modified_time);
	pDirHeader->Access_time = xixcore_GetTime(pDirHeader->Access_time);
}



static void
writeCV_FileHeader(
	IN PXIDISK_DIR_INFO	pDirHeader
)
{
	pDirHeader->Create_time = xixcore_SetTime(pDirHeader->Create_time);
	pDirHeader->Change_time = xixcore_SetTime(pDirHeader->Change_time);
	pDirHeader->Modified_time = xixcore_SetTime(pDirHeader->Modified_time);
	pDirHeader->Access_time = xixcore_SetTime(pDirHeader->Access_time);
}


static
int 
checkMd5FileHeader(
	IN PXIXCORE_BUFFER xbuf,
	OUT PXIDISK_DIR_INFO	*ppDirHeader
)
{
	PXIDISK_DUP_DIR_INFO	pDupDirHeader = NULL;
	PXIDISK_DIR_INFO	pDirHeader = NULL;
	xc_uint32	out[4];
	xc_uint32	*digest;
	xc_uint32	IsOkFirstCheck = 0;

	pDupDirHeader = (PXIDISK_DUP_DIR_INFO)xixcore_GetDataBuffer(xbuf);


	/* First check */
	pDirHeader = &(pDupDirHeader->DirInfoEven);

	xixcore_md5digest_metadata((xc_uint8 *)pDirHeader, ( sizeof(XIDISK_DIR_INFO)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pDirHeader->Digest);
	



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5FileHeader pDirHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pDirHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL EVEN checkMd5FileHeader pDirHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pDirHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

		
		IsOkFirstCheck = 0;
	}else {
		IsOkFirstCheck = 1;
	}

	/* Second check */
	pDirHeader = &(pDupDirHeader->DirInfoOdd);

	xixcore_md5digest_metadata((xc_uint8 *)pDirHeader, ( sizeof(XIDISK_DIR_INFO)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pDirHeader->Digest);
	



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5FileHeader pDirHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pDirHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL ODD checkMd5FileHeader pDirHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pDirHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

		if(IsOkFirstCheck == 1) {
			*ppDirHeader = &(pDupDirHeader->DirInfoEven);
			return XCCODE_SUCCESS;
		}
		/*	
		xixcore_DebugPrintf("FaIL ALL checkMd5FileHeader pDirHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
			pDirHeader,
			out[0],out[1],out[2],out[3],
			digest[0], digest[1], digest[2], digest[3]);
		*/
		return XCCODE_CRCERROR;
	}else {
		if(IsOkFirstCheck == 1 ) {
			if( compareSequence(
					pDupDirHeader->DirInfoEven.SequenceNum, 
					pDupDirHeader->DirInfoOdd.SequenceNum 
					)  >= 0 )
			{
				*ppDirHeader = &(pDupDirHeader->DirInfoEven);
				return XCCODE_SUCCESS;
			}else{
				*ppDirHeader = &(pDupDirHeader->DirInfoOdd);
				xixcore_IncreaseBufferOffset(xbuf, XIDISK_DIR_INFO_SIZE);
				return XCCODE_SUCCESS;
			}
		}else {
			*ppDirHeader = 	&(pDupDirHeader->DirInfoOdd);
			xixcore_IncreaseBufferOffset(xbuf, XIDISK_DIR_INFO_SIZE);
			return XCCODE_SUCCESS;
		}
	}

}


static 
void
makeMd5FileHeader(
	IN PXIDISK_DIR_INFO	pDirHeader
)
{
	xc_uint32	*digest;


	xixcore_md5digest_metadata((xc_uint8 *)pDirHeader, ( sizeof(XIDISK_DIR_INFO) - XIXCORE_MD5DIGEST_SIZE), pDirHeader->Digest);
	digest = (xc_uint32 *)pDirHeader->Digest;


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL,
		("makeMd5FileHeader pDirHeader(0x%p) lotnumber(0x%x%x%x%x)\n", 
		pDirHeader,digest[0],digest[1],digest[2],digest[3]));
}


int
xixcore_EndianSafeReadFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_DIR_INFO	pLotHeader = NULL;
	*Reason = 0;
	

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);			
	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadFileHeader Status(0x%x)\n", RC));

		return RC;
	}		

	RC = checkMd5FileHeader(xbuf, &pLotHeader);
	
	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error checkMd5FileHeader Status(0x%x)\n", RC));

		return RC;
	}

#if defined(__BIG_ENDIAN_BITFIELD)
	readChangeFileHeader(pLotHeader);
#endif
	readCV_FileHeader(pLotHeader);

	return RC;
}




int
xixcore_EndianSafeWriteFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_DIR_INFO	pLotHeader = NULL;
	xc_sector_t newStartsector = 0;
	*Reason = 0;
	
	pLotHeader = (PXIDISK_DIR_INFO)xixcore_GetDataBufferWithOffset(xbuf);
	pLotHeader->SequenceNum ++;
	
	if(pLotHeader->SequenceNum % 2) {
		newStartsector = startsector + SECTOR_ALIGNED_COUNT(sectorsizeBit, XIDISK_DIR_INFO_SIZE); 
	}else{
		newStartsector = startsector;
	}


	writeCV_FileHeader(pLotHeader);
#if defined(__BIG_ENDIAN_BITFIELD)
	writeChangeFileHeader(pLotHeader);
#endif


	makeMd5FileHeader(pLotHeader);

	RC = xixcore_DeviceIoSync(
			bdev,
			newStartsector,
			size,
			sectorsize,
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);			
	if(RC < 0){
		
#if defined(__BIG_ENDIAN_BITFIELD)		
		readChangeFileHeader(pLotHeader);
#endif
		readCV_FileHeader(pLotHeader);

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadFileHeader Status(0x%x)\n", RC));

		return RC;
	}
	
#if defined(__BIG_ENDIAN_BITFIELD)
	readChangeFileHeader(pLotHeader);
#endif
	readCV_FileHeader(pLotHeader);

	return RC;
}




// Added by ILGU HONG 12082006
/*
 *	Directory Entry Hash Value Table
 */



static void
ReadChangeDirHashValTable(
	IN PXIDISK_HASH_VALUE_TABLE	pTable
)
{

	xc_uint32 i = 0;
	for(i = 0; i< MAX_DIR_ENTRY; i++){
		XIXCORE_LE2CPU16p( &(pTable->EntryHashVal[i]) );
	}
	XIXCORE_LE2CPU64p(&(pTable->SequenceNum));

	return;
}

static void
WriteChangeDirHashValTable(
	IN PXIDISK_HASH_VALUE_TABLE	pTable
)
{
	
	xc_uint32 i = 0;
	for(i = 0; i<MAX_DIR_ENTRY; i++){
		XIXCORE_CPU2LE16p( &(pTable->EntryHashVal[i]) );
	}
	XIXCORE_CPU2LE64p(&(pTable->SequenceNum));
	return;
}


static
int 
checkMd5DirHashValTable(
	IN	PXIXCORE_BUFFER xbuf,
	OUT	PXIDISK_HASH_VALUE_TABLE	*ppTable
)
{

	PXIDISK_DUP_HASH_VALUE_TABLE pDupTable = NULL;
	PXIDISK_HASH_VALUE_TABLE	pTable = NULL;

	xc_uint32	out[4];
	xc_uint32	*digest;
	xc_uint32	IsOkFirstCheck = 0;

	pDupTable = (PXIDISK_DUP_HASH_VALUE_TABLE)xixcore_GetDataBuffer(xbuf);


	/* First check */
	pTable = &(pDupTable->HashTableEven);


	xixcore_md5digest_metadata((xc_uint8 *)pTable, ( sizeof(XIDISK_HASH_VALUE_TABLE)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pTable->Digest);
	



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5DirHashValTable pTable(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pTable,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL EVEN checkMd5DirHashValTable pTable(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pTable,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));


		IsOkFirstCheck = 0;
		
	}else {
		IsOkFirstCheck = 1;
	}

	/* Second Check */
	pTable = &(pDupTable->HashTableOdd);


	xixcore_md5digest_metadata((xc_uint8 *)pTable, ( sizeof(XIDISK_HASH_VALUE_TABLE)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pTable->Digest);
	



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5DirHashValTable pTable(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pTable,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL ODD checkMd5DirHashValTable pTable(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pTable,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));



		if(IsOkFirstCheck == 1){
			*ppTable = 	&(pDupTable->HashTableEven);
			return XCCODE_SUCCESS;
		}
		/*
		xixcore_DebugPrintf("FaIL ALL checkMd5DirHashValTable pTable(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
			pTable,
			out[0],out[1],out[2],out[3],
			digest[0], digest[1], digest[2], digest[3]);
		*/
		return XCCODE_CRCERROR;
		

	}else {
		if(IsOkFirstCheck == 1 ) {
			if( compareSequence(
					pDupTable->HashTableEven.SequenceNum, 
					pDupTable->HashTableOdd.SequenceNum 
					)  >= 0 )
			{
				*ppTable = &(pDupTable->HashTableEven);
				return XCCODE_SUCCESS;
			}else{
				*ppTable = &(pDupTable->HashTableOdd);
				xixcore_IncreaseBufferOffset(xbuf, XIDISK_HASH_VALUE_TABLE_SIZE);
				return XCCODE_SUCCESS;
			}
		}else {
			*ppTable = 	&(pDupTable->HashTableOdd);
			xixcore_IncreaseBufferOffset(xbuf, XIDISK_HASH_VALUE_TABLE_SIZE);
			return XCCODE_SUCCESS;
		}
	}




}


static 
void
makeMd5HashValTable(
	IN PXIDISK_HASH_VALUE_TABLE	pTable
)
{
	xc_uint32	*digest;


	xixcore_md5digest_metadata((xc_uint8 *)pTable, ( sizeof(XIDISK_HASH_VALUE_TABLE) - XIXCORE_MD5DIGEST_SIZE), pTable->Digest);
	digest = (xc_uint32 *)pTable->Digest;



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL,
		("makeMd5HashValTable pTable(0x%p) lotnumber(0x%x%x%x%x)\n", 
		pTable,digest[0],digest[1],digest[2],digest[3]));
}

int
xixcore_EndianSafeReadDirEntryHashValueTable(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_HASH_VALUE_TABLE	pTable = NULL;
	*Reason = 0;
	
	
	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);			
	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadDirEntryHashValueTable Status(0x%x)\n", RC));

		return RC;
	}		
	
	RC = checkMd5DirHashValTable(xbuf, &pTable);

	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error checkMd5DirHashValTable Status(0x%x)\n", RC));

		return RC;
	}		
#if defined(__BIG_ENDIAN_BITFIELD)
	ReadChangeDirHashValTable(pTable);
#endif		
	

	return RC;
}

int
xixcore_EndianSafeWriteDirEntryHashValueTable(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_HASH_VALUE_TABLE	pTable = NULL;
	xc_sector_t newStartsector = 0;
	*Reason = 0;
	
	pTable = (PXIDISK_HASH_VALUE_TABLE)xixcore_GetDataBufferWithOffset(xbuf);
	pTable->SequenceNum++;
	

	if(pTable->SequenceNum % 2) {
		newStartsector = startsector + SECTOR_ALIGNED_COUNT(sectorsizeBit, XIDISK_HASH_VALUE_TABLE_SIZE); 
	}else{
		newStartsector = startsector;
	}


#if defined(__BIG_ENDIAN_BITFIELD)
	WriteChangeDirHashValTable(pTable);
#endif 

	makeMd5HashValTable(pTable);

	RC = xixcore_DeviceIoSync(
			bdev,
			newStartsector,
			size,
			sectorsize,
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);			
	if(RC < 0){
		
#if defined(__BIG_ENDIAN_BITFIELD)		
		ReadChangeDirHashValTable(pTable);
#endif

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeWriteDirEntryHashValueTable Status(0x%x)\n", RC));

		return RC;
	}
	
#if defined(__BIG_ENDIAN_BITFIELD)
	ReadChangeDirHashValTable(pTable);
#endif

	return RC;
}

// Added by ILGU HONG 12082006 END





/*
 *	Lot and File Header
 */

/*
static void
readChangeLotAndFileHeader(
	IN PXIDISK_DIR_HEADER_LOT	pDirLotHeader
)
{
 
	

	// Lot Header
	readChangeLotHeader(&pDirLotHeader->LotHeader);
	


	// FileHeader
	readChangeFileHeader(&pDirLotHeader->DirInfo);



	
	return;
}

static void
wirteChangeLotAndFileHeader(
	IN PXIDISK_DIR_HEADER_LOT	pDirLotHeader
)
{
 

	

	// Lot Header
	writeChangeLotHeader(&pDirLotHeader->LotHeader);

	// FileHeader
	writeChangeFileHeader(&pDirLotHeader->DirInfo);
	
	return;
}



static void
readCV_LotAndFileHeader(
	IN PXIDISK_DIR_HEADER_LOT	pDirLotHeader
)
{
	

	readCV_LotHeader(&pDirLotHeader->LotHeader);
	readCV_FileHeader(&pDirLotHeader->DirInfo);
	return;
}


static void
wirteCV_LotAndFileHeader(
	IN PXIDISK_DIR_HEADER_LOT	pDirLotHeader
)
{
	
	writeCV_LotHeader(&pDirLotHeader->LotHeader);
	writeCV_FileHeader(&pDirLotHeader->DirInfo);
	return;
}




static
int 
checkMd5LotAndFileHeader(
	IN PXIDISK_DIR_HEADER_LOT	pDirLotHeader
)
{
	int RC = 0;
	xc_uint32	out[4];
	xc_uint32	*digest;

	RC = checkMd5LotHeader(&pDirLotHeader->LotHeader);

	if(RC < 0 ) {
		return RC;
	}


	RC = checkMd5FileHeader(&pDirLotHeader->DirInfo);
	
	return RC;
}


static 
void
makeMd5LotAndFileHeader(
	IN PXIDISK_DIR_HEADER_LOT	pDirLotHeader
)
{
	makeMd5LotHeader(&pDirLotHeader->LotHeader);
	makeMd5FileHeader(&pDirLotHeader->DirInfo);
}



int
xixcore_EndianSafeReadLotAndFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_DIR_HEADER_LOT	pLotHeader = NULL;
	*Reason = 0;
	
	pLotHeader = (PXIDISK_DIR_HEADER_LOT)xixcore_GetDataBufferWithOffset(xbuf);

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize,
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);	
		
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadLotAndFileHeaderStatus(0x%x)\n", RC));

		return RC;
	}

	RC = checkMd5LotAndFileHeader(pLotHeader);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error checkMd5LotAndFileHeader Status(0x%x)\n", RC));

		return RC;
	}

#if defined(__BIG_ENDIAN_BITFIELD)
	readChangeLotAndFileHeader(pLotHeader);
#endif
	readCV_LotAndFileHeader(pLotHeader);

	return RC;
}




int
xixcore_EndianSafeWriteLotAndFileHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_DIR_HEADER_LOT	pLotHeader = NULL;
	*Reason = 0;
	
	
	pLotHeader = (PXIDISK_DIR_HEADER_LOT)xixcore_GetDataBufferWithOffset(xbuf);

	wirteCV_LotAndFileHeader(pLotHeader);
#if defined(__BIG_ENDIAN_BITFIELD)
	wirteChangeLotAndFileHeader(pLotHeader);
#endif


	makeMd5LotAndFileHeader(pLotHeader);

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);	
		
	if( RC < 0 ){
#if defined(__BIG_ENDIAN_BITFIELD)		
		readChangeLotAndFileHeader(pLotHeader);
#endif		
		readCV_LotAndFileHeader(pLotHeader);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeWriteLotAndFileHeader Status(0x%x)\n", RC));

		return RC;
	}
	
#if defined(__BIG_ENDIAN_BITFIELD)
	readChangeLotAndFileHeader(pLotHeader);
#endif
	readCV_LotAndFileHeader(pLotHeader);
	return RC;
}

*/









/*
 *		For Addr information of File function
 */
static void
readChangeAddr(
	IN xc_uint8 * Addr,
	IN xc_uint32  BlockSize
)
{
	xc_uint64 * tmpAddr = NULL;
	xc_uint32 i = 0;
	xc_uint32 maxLoop = 0;

	tmpAddr = (xc_uint64 *)Addr;
	maxLoop = (xc_uint32)( BlockSize/sizeof(xc_uint64));	
	
	for(i = 0; i < maxLoop; i++){
		XIXCORE_LE2CPU64p(&tmpAddr[i]);
	}

	return;
}

static void
writeChangeAddr(
	IN xc_uint8 * Addr,
	IN xc_uint32  BlockSize
)
{
	xc_uint64 * tmpAddr = NULL;
	xc_uint32 i = 0;
	xc_uint32 maxLoop = 0;

	tmpAddr = (xc_uint64 *)Addr;
	maxLoop = (xc_uint32)( BlockSize/sizeof(xc_uint64));	
	
	for(i = 0; i < maxLoop; i++){
		XIXCORE_CPU2LE64p(&tmpAddr[i]);
	}

	return;
}


int
xixcore_EndianSafeReadAddrInfo(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	*Reason = 0;
	
	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize,
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);			
		
	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadAddrInfo Status(0x%x)\n", RC));

		return RC;
	}		

#if defined(__BIG_ENDIAN_BITFIELD)
	readChangeAddr(xixcore_GetDataBufferWithOffset(xbuf),size);
#endif		
	

	return RC;
}




int
xixcore_EndianSafeWriteAddrInfo(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	*Reason = 0;

#if defined(__BIG_ENDIAN_BITFIELD)
	writeChangeAddr(xixcore_GetDataBufferWithOffset(xbuf), size);
#endif

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);

	if(RC < 0){

#if defined(__BIG_ENDIAN_BITFIELD)
		readChangeAddr(xixcore_GetDataBufferWithOffset(xbuf),size);
#endif
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeWriteAddrInfo Status(0x%x)\n", RC));

		return RC;
	}		

#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeAddr(xixcore_GetDataBufferWithOffset(xbuf),size);
#endif
	

	return RC;
}




/*
 *	Dir Entry
 */

void
readChangeDirEntry(
	IN PXIDISK_CHILD_INFORMATION	pDirEntry
)
{
	//xc_uint32 NameSize = 0;
	//xc_uint32 i = 0;
	//xc_uint16 * Name = NULL;



	// DirEntry

	XIXCORE_LE2CPU32p(&(pDirEntry->Type));
	XIXCORE_LE2CPU32p(&(pDirEntry->State));

	XIXCORE_LE2CPU64p(&(pDirEntry->StartLotIndex));

	XIXCORE_LE2CPU32p(&(pDirEntry->NameSize));
	XIXCORE_LE2CPU32p(&(pDirEntry->ChildIndex));
	XIXCORE_LE2CPU32p(&(pDirEntry->HashValue));
	XIXCORE_LE2CPU64p(&(pDirEntry->SequenceNum));

	/*
	 *	 Change Unicode string order
	 */
	 /*
	 // it's done in xixfs file system
	NameSize = (xc_uint32)(pDirEntry->NameSize/2);
	Name = (xc_uint16 *)pDirEntry->Name;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XIXCORE_LE2CPU16p(&Name[i]);
		}
	}
	*/
	return;
}

void
writeChangeDirEntry(
	IN PXIDISK_CHILD_INFORMATION	pDirEntry
)
{
	//xc_uint32 NameSize = 0;
	//xc_uint32 i = 0;
	//xc_uint16 * Name = NULL;



	// DirEntry

	XIXCORE_CPU2LE32p(&(pDirEntry->Type));
	XIXCORE_CPU2LE32p(&(pDirEntry->State));

	XIXCORE_CPU2LE64p(&(pDirEntry->StartLotIndex));

	XIXCORE_CPU2LE32p(&(pDirEntry->NameSize));
	XIXCORE_CPU2LE32p(&(pDirEntry->ChildIndex));
	XIXCORE_CPU2LE32p(&(pDirEntry->HashValue));
	XIXCORE_CPU2LE64p(&(pDirEntry->SequenceNum));

	/*
	 *	 Change Unicode string order
	 */
	 /*
	 // it's done in xixfs file system
	NameSize = (xc_uint32)(pDirEntry->NameSize/2);
	Name = (xc_uint16 *)pDirEntry->Name;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XIXCORE_CPU2LE16p(&Name[i]);
		}
	}
	*/
	return;
}


int 
checkMd5DirEntry(
	IN PXIXCORE_BUFFER xbuf,
	OUT PXIDISK_CHILD_INFORMATION	*ppDirEntry
)
{
	PXIDISK_DUP_CHILD_INFORMATION	pDupDirEntry = NULL;
	PXIDISK_CHILD_INFORMATION	pDirEntry = NULL;
	xc_uint32	out[4];
	xc_uint32	*digest;
	xc_uint32	IsOkFirstCheck = 0;

	pDupDirEntry = (PXIDISK_DUP_CHILD_INFORMATION)xixcore_GetDataBuffer(xbuf);

	/* First Check */
	pDirEntry = &(pDupDirEntry->ChildInfoEven);

	xixcore_md5digest_metadata((xc_uint8 *)pDirEntry, ( sizeof(XIDISK_CHILD_INFORMATION)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pDirEntry->Digest);
	
	


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5DirEntry pDirEntry(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pDirEntry,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL EVEN checkMd5DirEntry pDirEntry(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pDirEntry,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));


		IsOkFirstCheck = 0;
	}else {
		IsOkFirstCheck = 1;	
	}

	/* Second Check */
	pDirEntry = &(pDupDirEntry->ChildInfoOdd);

	xixcore_md5digest_metadata((xc_uint8 *)pDirEntry, ( sizeof(XIDISK_CHILD_INFORMATION)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pDirEntry->Digest);
	
	


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5DirEntry pDirEntry(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pDirEntry,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL checkMd5DirEntry pDirEntry(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pDirEntry,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));


		
		if(IsOkFirstCheck == 1 ) {
			*ppDirEntry = &(pDupDirEntry->ChildInfoEven);
			return XCCODE_SUCCESS;
		}
		/*
		xixcore_DebugPrintf("FaIL ALL checkMd5DirEntry pDirEntry(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
			pDirEntry,
			out[0],out[1],out[2],out[3],
			digest[0], digest[1], digest[2], digest[3]);
		*/
		return XCCODE_CRCERROR;
	}else {
		if(IsOkFirstCheck == 1 ) {
			if( compareSequence(
					pDupDirEntry->ChildInfoEven.SequenceNum, 
					pDupDirEntry->ChildInfoOdd.SequenceNum 
					)  >= 0 )
			{
				*ppDirEntry = &(pDupDirEntry->ChildInfoEven);
				return XCCODE_SUCCESS;
			}else{
				*ppDirEntry = &(pDupDirEntry->ChildInfoOdd);
				xixcore_IncreaseBufferOffset(xbuf, XIDISK_CHILD_RECORD_SIZE);
				return XCCODE_SUCCESS;
			}
		}else {
			*ppDirEntry = &(pDupDirEntry->ChildInfoOdd);
			xixcore_IncreaseBufferOffset(xbuf, XIDISK_CHILD_RECORD_SIZE);
			return XCCODE_SUCCESS;
		}
	}

}


 
void
makeMd5DirEntry(
	IN PXIDISK_CHILD_INFORMATION	pDirEntry
)
{
	xc_uint32	*digest;


	xixcore_md5digest_metadata((xc_uint8 *)pDirEntry, ( sizeof(XIDISK_HASH_VALUE_TABLE) - XIXCORE_MD5DIGEST_SIZE), pDirEntry->Digest);
	digest = (xc_uint32 *)pDirEntry->Digest;

	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL,
		("makeMd5DirEntry pDirEntry(0x%p) lotnumber(0x%x%x%x%x)\n", 
		pDirEntry,digest[0],digest[1],digest[2],digest[3]));
}




/*
 *	Volume Header
 */
static void
readChangeVolumeHeader(
	IN	PXIDISK_VOLUME_INFO	pVolHeader
)
{
	//xc_uint32 NameSize = 0;
	//xc_uint32 i = 0;
	//xc_uint16 * Name = NULL;


	// VolumeHeader

	XIXCORE_LE2CPU64p(&(pVolHeader->VolumeSignature));

	XIXCORE_LE2CPU32p(&(pVolHeader->XifsVesion));

	XIXCORE_LE2CPU64p(&(pVolHeader->NumLots));

	XIXCORE_LE2CPU32p(&(pVolHeader->LotSize));

	XIXCORE_LE2CPU64p(&(pVolHeader->HostRegLotMapIndex));
	XIXCORE_LE2CPU64p(&(pVolHeader->RootDirectoryLotIndex));
	XIXCORE_LE2CPU64p(&(pVolHeader->VolCreationTime));

	XIXCORE_LE2CPU32p(&(pVolHeader->VolSerialNumber));
	XIXCORE_LE2CPU32p(&(pVolHeader->VolLabelLength));
	XIXCORE_LE2CPU32p(&(pVolHeader->LotSignature));
	XIXCORE_LE2CPU64p(&(pVolHeader->SequenceNum));
	/*
	 *	 Change Unicode string order
	 */
	 /*
	 // it's done in xixfs file system
	NameSize = (xc_uint32)(pVolHeader->VolInfo.VolLabelLength/2);
	Name = (xc_uint16 *)pVolHeader->VolInfo.VolLabel;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XIXCORE_LE2CPU16p(&Name[i]);
		}
	}
	*/
	return;
}

static void
writeChangeVolumeHeader(
	IN PXIDISK_VOLUME_INFO	pVolHeader
)
{
	//xc_uint32 NameSize = 0;
	//xc_uint32 i = 0;
	//xc_uint16 * Name = NULL;



	// FileHeader

	XIXCORE_CPU2LE64p(&(pVolHeader->VolumeSignature));

	XIXCORE_CPU2LE32p(&(pVolHeader->XifsVesion));

	XIXCORE_CPU2LE64p(&(pVolHeader->NumLots));

	XIXCORE_CPU2LE32p(&(pVolHeader->LotSize));

	XIXCORE_CPU2LE64p(&(pVolHeader->HostRegLotMapIndex));
	XIXCORE_CPU2LE64p(&(pVolHeader->RootDirectoryLotIndex));
	XIXCORE_CPU2LE64p(&(pVolHeader->VolCreationTime));

	XIXCORE_CPU2LE32p(&(pVolHeader->VolSerialNumber));
	XIXCORE_CPU2LE32p(&(pVolHeader->VolLabelLength));
	XIXCORE_CPU2LE32p(&(pVolHeader->LotSignature));
	XIXCORE_CPU2LE64p(&(pVolHeader->SequenceNum));
	/*
	 *	 Change Unicode string order
	 */
	 /*
	 // it's done in xixfs file system
	NameSize = (xc_uint32)(pVolHeader->VolInfo.VolLabelLength/2);
	Name = (xc_uint16 *)pVolHeader->VolInfo.VolLabel;
	
	if(NameSize > 0){
		for(i = 0; i < NameSize; i++){
			XIXCORE_CPU2LE16p(&Name[i]);
		}
	}
	*/
	return;
}



static void
readCV_VolumeHeader(
	IN PXIDISK_VOLUME_INFO	pVolHeader
)
{

	pVolHeader->VolCreationTime = xixcore_GetTime(pVolHeader->VolCreationTime);

	return;
}



static void
writeCV_VolumeHeader(
	IN PXIDISK_VOLUME_INFO	pVolHeader
)
{

	pVolHeader->VolCreationTime = xixcore_SetTime(pVolHeader->VolCreationTime);

	return;
}

static
int 
checkMd5VolumeHeader(
    IN PXIXCORE_BUFFER xbuf,
	OUT PXIDISK_VOLUME_INFO	*ppVolHeader
)
{
	PXIDISK_DUP_VOLUME_INFO	pDupVolHeader = NULL;
	PXIDISK_VOLUME_INFO	pVolHeader = NULL;
	xc_uint32	out[4];
	xc_uint32	*digest;
	xc_uint32	IsOkFirstCheck = 0;

	pDupVolHeader = (PXIDISK_DUP_VOLUME_INFO)xixcore_GetDataBuffer(xbuf);
	
	/* First Check */
	pVolHeader = &(pDupVolHeader->VolInfoEven);
	xixcore_md5digest_metadata((xc_uint8 *)pVolHeader, ( sizeof(XIDISK_VOLUME_INFO)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pVolHeader->Digest);
	
	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5VolumeHeader pVolHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pVolHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL EVEN checkMd5VolumeHeader pVolHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pVolHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

		IsOkFirstCheck = 0;
	}else {
		IsOkFirstCheck = 1;
	}

	/* Second Check */
	pVolHeader = &(pDupVolHeader->VolInfoOdd);
	xixcore_md5digest_metadata((xc_uint8 *)pVolHeader, ( sizeof(XIDISK_VOLUME_INFO)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pVolHeader->Digest);
	
	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5VolumeHeader pVolHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pVolHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL ODD checkMd5VolumeHeader pVolHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pVolHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));


		if(IsOkFirstCheck == 1 ) {
			*ppVolHeader = &(pDupVolHeader->VolInfoEven);
			return XCCODE_SUCCESS;
		}
		/*
		xixcore_DebugPrintf("FaIL ALL checkMd5VolumeHeader pVolHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
			pVolHeader,
			out[0],out[1],out[2],out[3],
			digest[0], digest[1], digest[2], digest[3]);
		*/
		return XCCODE_CRCERROR;
	}else {
		if(IsOkFirstCheck == 1 ) {
			if( compareSequence(
					pDupVolHeader->VolInfoEven.SequenceNum, 
					pDupVolHeader->VolInfoOdd.SequenceNum 
					)  >= 0 )
			{
				*ppVolHeader = &(pDupVolHeader->VolInfoEven);
				return XCCODE_SUCCESS;
			}else{
				*ppVolHeader = &(pDupVolHeader->VolInfoOdd);
				xixcore_IncreaseBufferOffset(xbuf, XIDISK_VOLUME_INFO_SIZE);
				return XCCODE_SUCCESS;
			}
		}else {
			*ppVolHeader = &(pDupVolHeader->VolInfoOdd);
			xixcore_IncreaseBufferOffset(xbuf, XIDISK_VOLUME_INFO_SIZE);
			return XCCODE_SUCCESS;
		}
	}
}


static 
void
makeMd5VolumeHeader(
	IN PXIDISK_VOLUME_INFO	pVolHeader
)
{
	xc_uint32	*digest;

	xixcore_md5digest_metadata((xc_uint8 *)pVolHeader, ( sizeof(XIDISK_VOLUME_INFO) - XIXCORE_MD5DIGEST_SIZE), pVolHeader->Digest);
	digest = (xc_uint32 *)pVolHeader->Digest;


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL,
		("makeMd5VolumeHeader pVolHeader(0x%p) lotnumber(0x%x%x%x%x)\n", 
		pVolHeader,digest[0],digest[1],digest[2],digest[3]));
}


int
xixcore_EndianSafeReadVolumeHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_VOLUME_INFO	pVolHeader = NULL;
	
	*Reason = 0;
	

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize,
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);			
		
	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadVolumeHeader Status(0x%x)\n", RC));

		return RC;
	}		

	RC = checkMd5VolumeHeader(xbuf, &pVolHeader);

	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error checkMd5VolumeHeader Status(0x%x)\n", RC));

		return RC;
	}		

#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeVolumeHeader(pVolHeader);
#endif
	readCV_VolumeHeader(pVolHeader);

	return RC;
}




int
xixcore_EndianSafeWriteVolumeHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_VOLUME_INFO	pVolHeader = NULL;
	xc_sector_t newStartsector = 0;
	*Reason = 0;
	
	pVolHeader = (PXIDISK_VOLUME_INFO)xixcore_GetDataBufferWithOffset(xbuf);
	pVolHeader->SequenceNum ++;

	

	if(pVolHeader->SequenceNum % 2) {
		newStartsector = startsector + SECTOR_ALIGNED_COUNT(sectorsizeBit, XIDISK_VOLUME_INFO_SIZE); 
	}else{
		newStartsector = startsector;
	}
	

	writeCV_VolumeHeader(pVolHeader);
#if defined(__BIG_ENDIAN_BITFIELD)	
	writeChangeVolumeHeader(pVolHeader);
#endif

	makeMd5VolumeHeader(pVolHeader);

	RC = xixcore_DeviceIoSync(
			bdev,
			newStartsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);			
		
	if(RC < 0 ){

#if defined(__BIG_ENDIAN_BITFIELD)			
		readChangeVolumeHeader(pVolHeader);
#endif
		readCV_VolumeHeader(pVolHeader);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeWriteVolumeHeader(0x%x)\n", RC));

		return RC;
	}		

#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeVolumeHeader(pVolHeader);
#endif
	readCV_VolumeHeader(pVolHeader);
	return RC;
}


/*
*	BootSector
*/
static void
readChangeBootSector(
	IN PPACKED_BOOT_SECTOR	pBootSector
)
{
	XIXCORE_LE2CPU64p(&(pBootSector->VolumeSignature));
	XIXCORE_LE2CPU64p(&(pBootSector->NumLots));

	XIXCORE_LE2CPU32p(&(pBootSector->LotSignature ));
	XIXCORE_LE2CPU32p(&(pBootSector->XifsVesion ));
	XIXCORE_LE2CPU32p(&(pBootSector->LotSize));
	XIXCORE_LE2CPU64p(&(pBootSector->FirstVolumeIndex));
	XIXCORE_LE2CPU64p(&(pBootSector->SecondVolumeIndex));
	XIXCORE_LE2CPU64p(&(pBootSector->SequenceNum));

	return;
}

static void
writeChangeBootSector(
	IN PPACKED_BOOT_SECTOR	pBootSector
)
{
	XIXCORE_CPU2LE64p(&(pBootSector->VolumeSignature));
	XIXCORE_CPU2LE64p(&(pBootSector->NumLots));

	XIXCORE_CPU2LE32p(&(pBootSector->LotSignature ));
	XIXCORE_CPU2LE32p(&(pBootSector->XifsVesion ));
	XIXCORE_CPU2LE32p(&(pBootSector->LotSize));
	XIXCORE_CPU2LE64p(&(pBootSector->FirstVolumeIndex));
	XIXCORE_CPU2LE64p(&(pBootSector->SecondVolumeIndex));
	XIXCORE_CPU2LE64p(&(pBootSector->SequenceNum));
	return;
}


static
int 
checkMd5BootSector(
	IN PPACKED_BOOT_SECTOR	pBootSector
)
{
	xc_uint32	out[4];
	xc_uint32	*digest;


	xixcore_md5digest_metadata((xc_uint8 *)pBootSector, ( sizeof(PACKED_BOOT_SECTOR)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pBootSector->Digest);
	


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5BootSector pBootSector(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pBootSector,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL checkMd5BootSector pBootSector(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pBootSector,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

		/*
		xixcore_DebugPrintf("FaIL ALL checkMd5BootSector pBootSector(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
			pBootSector,
			out[0],out[1],out[2],out[3],
			digest[0], digest[1], digest[2], digest[3]);

		*/
		return XCCODE_CRCERROR;
	}else {
		return XCCODE_SUCCESS;
	}
}


static 
void
makeMd5BootSector(
	IN PPACKED_BOOT_SECTOR	pBootSector
)
{
	xc_uint32	*digest;


	xixcore_md5digest_metadata((xc_uint8 *)pBootSector, ( sizeof(PACKED_BOOT_SECTOR) - XIXCORE_MD5DIGEST_SIZE), pBootSector->Digest);
	digest = (xc_uint32 *)pBootSector->Digest;


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL,
		("makeMd5BootSector pBootSector(0x%p) lotnumber(0x%x%x%x%x)\n", 
		pBootSector,digest[0],digest[1],digest[2],digest[3]));
}




int
xixcore_EndianSafeReadBootSector(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PPACKED_BOOT_SECTOR	pBootSector = NULL;
	
	*Reason = 0;
	
	
	pBootSector = (PPACKED_BOOT_SECTOR)xixcore_GetDataBufferWithOffset(xbuf);

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize,
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);			
		
	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadBootSector Status(0x%x)\n", RC));

		return RC;
	}

	RC = checkMd5BootSector(pBootSector);
	
	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error checkMd5BootSector Status(0x%x)\n", RC));

		return RC;
	}

#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeBootSector(pLotHeader);
#endif

	return RC;
}




int
xixcore_EndianSafeWriteBootSector(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PPACKED_BOOT_SECTOR	pBootSector = NULL;
	*Reason = 0;
	
	
	pBootSector = (PPACKED_BOOT_SECTOR)xixcore_GetDataBufferWithOffset(xbuf);

#if defined(__BIG_ENDIAN_BITFIELD)	
	writeChangeBootSector(pLotHeader);
#endif

	makeMd5BootSector(pBootSector);

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);			
		
	if(RC < 0 ){

#if defined(__BIG_ENDIAN_BITFIELD)			
		readChangeBootSector(pLotHeader);
#endif
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeWriteBootSector Status(0x%x)\n", RC));

		return RC;
	}		

#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeBootSector(pLotHeader);
#endif
	return RC;
}



/*
 *	Lot and Lot map Header
 */


static void
readChangeLotMapHeader(
	IN PXIDISK_LOT_MAP_INFO	pLotMapHeader
)
{

	// LotMapHeader

	XIXCORE_LE2CPU32p(&(pLotMapHeader->MapType));
	XIXCORE_LE2CPU32p(&(pLotMapHeader->BitMapBytes));
	XIXCORE_LE2CPU64p(&(pLotMapHeader->NumLots));
	XIXCORE_LE2CPU64p(&(pLotMapHeader->SequenceNum));
	return;
}

static void
writeChangeLotMapHeader(
	IN PXIDISK_LOT_MAP_INFO	pLotMapHeader
)
{


	// LotMapHeader

	XIXCORE_CPU2LE32p(&(pLotMapHeader->MapType));
	XIXCORE_CPU2LE32p(&(pLotMapHeader->BitMapBytes));
	XIXCORE_CPU2LE64p(&(pLotMapHeader->NumLots));
	XIXCORE_CPU2LE64p(&(pLotMapHeader->SequenceNum));
	return;
}





static
int 
checkMd5LotMapHeader(
	IN PXIXCORE_BUFFER xbuf,
	OUT PXIDISK_LOT_MAP_INFO	*ppLotMapHeader
)
{
	PXIDISK_DUP_LOT_MAP_INFO	pDupLotMapHeader = NULL;
	PXIDISK_LOT_MAP_INFO	pLotMapHeader = NULL;
	xc_uint32	out[4];
	xc_uint32	*digest;
	xc_uint32	IsOkFirstCheck = 0;


	pDupLotMapHeader = (PXIDISK_DUP_LOT_MAP_INFO)xixcore_GetDataBuffer(xbuf);

	/* First Check */

	pLotMapHeader = &(pDupLotMapHeader->LotMapInfoEven);

	xixcore_md5digest_metadata((xc_uint8 *)pLotMapHeader, ( sizeof(XIDISK_LOT_MAP_INFO)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pLotMapHeader->Digest);
	


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5LotAndLotMapHeader pLotMapHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pLotMapHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL EVEN checkMd5LotAndLotMapHeader pLotMapHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pLotMapHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

		
		IsOkFirstCheck = 0;
	}else {
		IsOkFirstCheck = 1;
	}

	/* Second Check */

	pLotMapHeader = &(pDupLotMapHeader->LotMapInfoOdd);

	xixcore_md5digest_metadata((xc_uint8 *)pLotMapHeader, ( sizeof(XIDISK_LOT_MAP_INFO)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pLotMapHeader->Digest);
	


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5LotAndLotMapHeader pLotMapHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pLotMapHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL ODD checkMd5LotAndLotMapHeader pLotMapHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pLotMapHeader,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));


		
		if(IsOkFirstCheck ==1){
			*ppLotMapHeader = &(pDupLotMapHeader->LotMapInfoEven);
			return XCCODE_SUCCESS;
		}
		/*
		xixcore_DebugPrintf("FaIL ALL checkMd5LotAndLotMapHeader pLotMapHeader(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
			pLotMapHeader,
			out[0],out[1],out[2],out[3],
			digest[0], digest[1], digest[2], digest[3]);
		*/
		return XCCODE_CRCERROR;
	}else {
		if(IsOkFirstCheck == 1 ) {
			if( compareSequence(
					pDupLotMapHeader->LotMapInfoEven.SequenceNum, 
					pDupLotMapHeader->LotMapInfoOdd.SequenceNum 
					)  >= 0 )
			{
				*ppLotMapHeader = &(pDupLotMapHeader->LotMapInfoEven);
				return XCCODE_SUCCESS;
			}else{
				*ppLotMapHeader = &(pDupLotMapHeader->LotMapInfoOdd);
				xixcore_IncreaseBufferOffset(xbuf, XIDISK_LOT_MAP_INFO_SIZE);
				return XCCODE_SUCCESS;
			}
		}else {
			*ppLotMapHeader = &(pDupLotMapHeader->LotMapInfoOdd);
			xixcore_IncreaseBufferOffset(xbuf, XIDISK_LOT_MAP_INFO_SIZE);
			return XCCODE_SUCCESS;
		}
	}

}


static 
void
makeMd5LotMapHeader(
	IN PXIDISK_LOT_MAP_INFO	pLotMapHeader
)
{
	xc_uint32	*digest;

	xixcore_md5digest_metadata((xc_uint8 *)pLotMapHeader, ( sizeof(XIDISK_LOT_MAP_INFO) - XIXCORE_MD5DIGEST_SIZE), pLotMapHeader->Digest);
	digest = (xc_uint32 *)pLotMapHeader->Digest;



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL,
		("makeMd5LotAndLotMapHeader pLotMapHeader(0x%p) lotnumber(0x%x%x%x%x)\n", 
		pLotMapHeader,digest[0],digest[1],digest[2],digest[3]));
}

int
xixcore_EndianSafeReadLotMapHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_LOT_MAP_INFO	pLotHeader = NULL;
	*Reason = 0;

	XIXCORE_ASSERT(xbuf);
	
	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);			
		
	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadLotMapHeader Status(0x%x)\n", RC));

		return RC;
	}		

	RC = checkMd5LotMapHeader(xbuf, &pLotHeader);
	
	if(RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error checkMd5LotAndLotMapHeader Status(0x%x)\n", RC));

		return RC;
	}		


#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeLotMapHeader(pLotHeader);
#endif
	return RC;
}




int
xixcore_EndianSafeWriteLotMapHeader(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_LOT_MAP_INFO	pLotHeader = NULL;
	xc_sector_t newStartsector = 0;
	
	pLotHeader = (PXIDISK_LOT_MAP_INFO)xixcore_GetDataBufferWithOffset(xbuf);
	pLotHeader->SequenceNum ++;

	

	if(pLotHeader->SequenceNum % 2) {
		newStartsector = startsector + SECTOR_ALIGNED_COUNT(sectorsizeBit, XIDISK_LOT_MAP_INFO_SIZE); 
	}else{
		newStartsector = startsector;
	}


	*Reason = 0;
	
	


#if defined(__BIG_ENDIAN_BITFIELD)		
	writeChangeLotMapHeader(pLotHeader);
#endif

	makeMd5LotMapHeader(pLotHeader);

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);			
		
	if(RC < 0 ){

#if defined(__BIG_ENDIAN_BITFIELD)			
		readChangeLotMapHeader(pLotHeader);
#endif
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeWriteLotMapHeader Status(0x%x)\n", RC));

		return RC;
	}		
	
#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeLotMapHeader(pLotHeader);
#endif
	return RC;
}





/*
 *	Read and Write Register Host Info	
 */

static void
readChangeHostInfo(
	IN PXIDISK_HOST_INFO	pHostInfo
)
{

	// HostInfo
	XIXCORE_LE2CPU32p(&(pHostInfo->NumHost));

	XIXCORE_LE2CPU64p(&(pHostInfo->UsedLotMapIndex));
	XIXCORE_LE2CPU64p(&(pHostInfo->UnusedLotMapIndex));
	XIXCORE_LE2CPU64p(&(pHostInfo->CheckOutLotMapIndex));
	XIXCORE_LE2CPU64p(&(pHostInfo->LogFileIndex));
	XIXCORE_LE2CPU64p(&(pHostInfo->SequenceNum));
	return;
}

static void
writeChangeHostInfo(
	IN PXIDISK_HOST_INFO	pHostInfo
)
{
	// Lock Info
	XIXCORE_CPU2LE32p(&(pHostInfo->NumHost));

	XIXCORE_CPU2LE64p(&(pHostInfo->UsedLotMapIndex));
	XIXCORE_CPU2LE64p(&(pHostInfo->UnusedLotMapIndex));
	XIXCORE_CPU2LE64p(&(pHostInfo->CheckOutLotMapIndex));
	XIXCORE_CPU2LE64p(&(pHostInfo->LogFileIndex));
	XIXCORE_CPU2LE64p(&(pHostInfo->SequenceNum));
	return;
}


static
int 
checkMd5HostInfo(
	IN PXIXCORE_BUFFER xbuf, 
	IN PXIDISK_HOST_INFO	*ppHostInfo
)
{
	PXIDISK_DUP_HOST_INFO	pDupHostInfo = NULL;
	PXIDISK_HOST_INFO	pHostInfo = NULL;
	xc_uint32	out[4];
	xc_uint32	*digest;
	xc_uint32	IsOkFirstCheck = 0;
	
	pDupHostInfo = (PXIDISK_DUP_HOST_INFO)xixcore_GetDataBuffer(xbuf);
	

	/* First Check */
	pHostInfo = &(pDupHostInfo->HostInfoEven);

	xixcore_md5digest_metadata((xc_uint8 *)pHostInfo, ( sizeof(XIDISK_HOST_INFO)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pHostInfo->Digest);
	
	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5HostInfo pHostInfo(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pHostInfo,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL EVEN checkMd5HostInfo pHostInfo(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pHostInfo,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));


		IsOkFirstCheck = 0;
	}else {
		IsOkFirstCheck = 1;
	}

	/* First Check */
	pHostInfo = &(pDupHostInfo->HostInfoOdd);

	xixcore_md5digest_metadata((xc_uint8 *)pHostInfo, ( sizeof(XIDISK_HOST_INFO)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pHostInfo->Digest);
	
	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5HostInfo pHostInfo(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pHostInfo,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL ODD checkMd5HostInfo pHostInfo(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pHostInfo,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));


		
		if(IsOkFirstCheck == 1) {
			*ppHostInfo = &(pDupHostInfo->HostInfoEven);
			return XCCODE_SUCCESS;
		}
		/*
		xixcore_DebugPrintf("FaIL ALL checkMd5HostInfo pHostInfo(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
			pHostInfo,
			out[0],out[1],out[2],out[3],
			digest[0], digest[1], digest[2], digest[3]);
		*/
		return XCCODE_CRCERROR;
	}else {
		if(IsOkFirstCheck == 1 ) {
			if( compareSequence(
					pDupHostInfo->HostInfoEven.SequenceNum, 
					pDupHostInfo->HostInfoOdd.SequenceNum 
					)  >= 0 )
			{
				*ppHostInfo = &(pDupHostInfo->HostInfoEven);
				return XCCODE_SUCCESS;
			}else{
				*ppHostInfo = &(pDupHostInfo->HostInfoOdd);
				xixcore_IncreaseBufferOffset(xbuf, XIDISK_HOST_INFO_SIZE);
				return XCCODE_SUCCESS;
			}
		}else {
			*ppHostInfo = &(pDupHostInfo->HostInfoOdd);
			xixcore_IncreaseBufferOffset(xbuf, XIDISK_HOST_INFO_SIZE);
			return XCCODE_SUCCESS;
		}
	}

}


static 
void
makeMd5HostInfo(
	IN PXIDISK_HOST_INFO	pHostInfo
)
{
	xc_uint32	*digest;


	xixcore_md5digest_metadata((xc_uint8 *)pHostInfo, ( sizeof(XIDISK_HOST_INFO) - XIXCORE_MD5DIGEST_SIZE), pHostInfo->Digest);
	digest = (xc_uint32 *)pHostInfo->Digest;

	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL,
		("makeMd5HostInfo pHostInfo(0x%p) lotnumber(0x%x%x%x%x)\n", 
		pHostInfo,digest[0],digest[1],digest[2],digest[3]));
}

int
xixcore_EndianSafeReadHostInfo(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_HOST_INFO	pLotHeader = NULL;

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);
	
	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadHostInfo Status(0x%x)\n", RC));

		return RC;
	}

	RC = checkMd5HostInfo(xbuf, &pLotHeader);
	
	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error checkMd5HostInfo Status(0x%x)\n", RC));

		return RC;
	}

#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeHostInfo(pLotHeader);
#endif

	return RC;
}






int
xixcore_EndianSafeWriteHostInfo(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_HOST_INFO	pLotHeader = NULL;	
	xc_sector_t newStartsector = 0;

	pLotHeader = (PXIDISK_HOST_INFO)xixcore_GetDataBufferWithOffset(xbuf);
	pLotHeader->SequenceNum ++;
	
	if(pLotHeader->SequenceNum % 2) {
		newStartsector = startsector + SECTOR_ALIGNED_COUNT(sectorsizeBit, XIDISK_HOST_INFO_SIZE); 
	}else{
		newStartsector = startsector;
	}


	*Reason = 0;


	

#if defined(__BIG_ENDIAN_BITFIELD)	
	writeChangeHostInfo(pLotHeader);
#endif

	makeMd5HostInfo(pLotHeader);

	RC = xixcore_DeviceIoSync(
			bdev,
			newStartsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);
	
	if(RC < 0){

#if defined(__BIG_ENDIAN_BITFIELD)			
		readChangeHostInfo(pLotHeader);
#endif

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeWriteHostInfo Status(0x%x)\n", RC));

		return RC;
	}
	
#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeHostInfo(pLotHeader);
#endif

	return RC;
}






/*
 *	Read and Write Register Host Record
 */
static void
readChangeHostRecord(
	IN PXIDISK_HOST_RECORD	pHostRecord
)
{

	// Host Rcord
	XIXCORE_LE2CPU32p(&(pHostRecord->HostState));

	XIXCORE_LE2CPU64p(&(pHostRecord->HostMountTime));
	XIXCORE_LE2CPU64p(&(pHostRecord->HostCheckOutLotMapIndex));
	XIXCORE_LE2CPU64p(&(pHostRecord->HostUsedLotMapIndex));
	XIXCORE_LE2CPU64p(&(pHostRecord->HostUnusedLotMapIndex));
	XIXCORE_LE2CPU64p(&(pHostRecord->LogFileIndex));
	XIXCORE_LE2CPU64p(&(pHostRecord->SequenceNum));
	return;
}

static void
writeChangeHostRecord(
	IN PXIDISK_HOST_RECORD	pHostRecord
)
{
	// Host Rcord
	XIXCORE_CPU2LE32p(&(pHostRecord->HostState));

	XIXCORE_CPU2LE64p(&(pHostRecord->HostMountTime));
	XIXCORE_CPU2LE64p(&(pHostRecord->HostCheckOutLotMapIndex));
	XIXCORE_CPU2LE64p(&(pHostRecord->HostUsedLotMapIndex));
	XIXCORE_CPU2LE64p(&(pHostRecord->HostUnusedLotMapIndex));
	XIXCORE_CPU2LE64p(&(pHostRecord->LogFileIndex));
	XIXCORE_CPU2LE64p(&(pHostRecord->SequenceNum));
	return;
}



static void
readCV_HostRecord(
	PXIDISK_HOST_RECORD	pHostRecord
)
{
	pHostRecord->HostMountTime = xixcore_GetTime(pHostRecord->HostMountTime);

	return;
}


static void
writeCV_HostRecord(
	PXIDISK_HOST_RECORD	pHostRecord
)
{
	pHostRecord->HostMountTime = xixcore_SetTime(pHostRecord->HostMountTime);

	return;
}

static
int 
checkMd5HostRecord(
	IN	PXIXCORE_BUFFER xbuf,
	OUT PXIDISK_HOST_RECORD	*ppHostRecord
)
{
	PXIDISK_DUP_HOST_RECORD	pDupHostRecord = NULL;
	PXIDISK_HOST_RECORD	pHostRecord = NULL;
	xc_uint32	out[4];
	xc_uint32	*digest;
	xc_uint32	IsOkFirstCheck = 0;
	

	pDupHostRecord = (PXIDISK_DUP_HOST_RECORD)xixcore_GetDataBuffer(xbuf);

	/* First check */
	pHostRecord = &(pDupHostRecord->HostRecordEven);

	xixcore_md5digest_metadata((xc_uint8 *)pHostRecord, ( sizeof(XIDISK_HOST_RECORD)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pHostRecord->Digest);
	


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5HostRecord pHostRecord(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pHostRecord,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL EVEN checkMd5HostRecord pHostRecord(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pHostRecord,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));


		IsOkFirstCheck = 0;
	}else {
		IsOkFirstCheck = 1;
	}

	/* Second check */
	pHostRecord = &(pDupHostRecord->HostRecordOdd);

	xixcore_md5digest_metadata((xc_uint8 *)pHostRecord, ( sizeof(XIDISK_HOST_RECORD)-XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pHostRecord->Digest);
	


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5HostRecord pHostRecord(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pHostRecord,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL checkMd5HostRecord pHostRecord(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pHostRecord,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));



		if(IsOkFirstCheck == 1 ) {
			*ppHostRecord = &(pDupHostRecord->HostRecordEven);
			return XCCODE_SUCCESS;
		}
		/*
		xixcore_DebugPrintf("FaIL ALL checkMd5HostRecord pHostRecord(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
			pHostRecord,
			out[0],out[1],out[2],out[3],
			digest[0], digest[1], digest[2], digest[3]);
		*/
		return XCCODE_CRCERROR;
	}else {
		if(IsOkFirstCheck == 1 ) {
			if( compareSequence(
					pDupHostRecord->HostRecordEven.SequenceNum, 
					pDupHostRecord->HostRecordOdd.SequenceNum 
					)  >= 0 )
			{
				*ppHostRecord = &(pDupHostRecord->HostRecordEven);
				return XCCODE_SUCCESS;
			}else{
				*ppHostRecord = &(pDupHostRecord->HostRecordOdd);
				xixcore_IncreaseBufferOffset(xbuf, XIDISK_HOST_RECORD_SIZE);
				return XCCODE_SUCCESS;
			}
		}else {
			*ppHostRecord = &(pDupHostRecord->HostRecordOdd);
			xixcore_IncreaseBufferOffset(xbuf, XIDISK_HOST_RECORD_SIZE);
			return XCCODE_SUCCESS;
		}
	}

}


static 
void
makeMd5HostRecord(
	IN PXIDISK_HOST_RECORD	pHostRecord
)
{
	xc_uint32	*digest;


	xixcore_md5digest_metadata((xc_uint8 *)pHostRecord, ( sizeof(XIDISK_HOST_RECORD) - XIXCORE_MD5DIGEST_SIZE), pHostRecord->Digest);
	digest = (xc_uint32 *)pHostRecord->Digest;



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL,
		("makeMd5HostRecord pHostRecord(0x%p) lotnumber(0x%x%x%x%x)\n", 
		pHostRecord,digest[0],digest[1],digest[2],digest[3]));

}

int
xixcore_EndianSafeReadHostRecord(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_HOST_RECORD	pLotHeader = NULL;
	*Reason = 0;	

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);
		
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadHostRecord Status(0x%x)\n", RC));

		return RC;
	}
	xixcore_DebugPrintf("Host start sector 0x%I64x\n", startsector);
	RC = checkMd5HostRecord(xbuf, &pLotHeader);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error checkMd5HostRecord Status(0x%x)\n", RC));

		return RC;
	}

#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeHostRecord(pLotHeader);
#endif		

	readCV_HostRecord(pLotHeader);
	return RC;
}




int
xixcore_EndianSafeWriteHostRecord(
	PXIXCORE_BLOCK_DEVICE bdev, 
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_int32 sectorsize, 
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER xbuf,
	xc_int32	*Reason
)
{
	int RC = 0;
	PXIDISK_HOST_RECORD	pLotHeader = NULL;
	xc_sector_t newStartsector = 0;

	pLotHeader = (PXIDISK_HOST_RECORD)xixcore_GetDataBufferWithOffset(xbuf);
	pLotHeader->SequenceNum ++;
	
	if(pLotHeader->SequenceNum % 2) {
		newStartsector = startsector + SECTOR_ALIGNED_COUNT(sectorsizeBit, XIDISK_HOST_RECORD_SIZE); 
	}else{
		newStartsector = startsector;
	}


	*Reason = 0;



	writeCV_HostRecord(pLotHeader);
#if defined(__BIG_ENDIAN_BITFIELD)	
	writeChangeHostRecord(pLotHeader);
#endif

	xixcore_DebugPrintf("Host start sector 0x%I64x\n", newStartsector);
	makeMd5HostRecord(pLotHeader);

	RC = xixcore_DeviceIoSync(
			bdev,
			newStartsector,
			size,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);
		
	if( RC < 0 ){

#if defined(__BIG_ENDIAN_BITFIELD)			
		readChangeHostRecord(pLotHeader);
#endif
		readCV_HostRecord(pLotHeader);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeWriteHostRecord Status(0x%x)\n", RC));

		return RC;
	}
	
#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeHostRecord(pLotHeader);
#endif		

	readCV_HostRecord(pLotHeader);
	return RC;
}





/*
 *	Read and Write BITMAPDATA
 */
static void
readChangeBitmapData(
	IN PXIDISK_BITMAP_DATA	pBitmapData
)
{
	XIXCORE_LE2CPU64p(&(pBitmapData->SequenceNum));
	return;
}

static void
writeChangeBitmapData(
	IN PXIDISK_BITMAP_DATA	pBitmapData
)
{
	XIXCORE_CPU2LE64p(&(pBitmapData->SequenceNum));
	return;
}

static
int 
checkMd5BitmapData(
	IN	PXIXCORE_BUFFER			xbuf,
	IN	xc_uint32				DataSize,			
	OUT PXIDISK_BITMAP_DATA		*ppBitmapData
)
{
	PXIDISK_BITMAP_DATA		pBitmapDataEven = NULL;
	PXIDISK_BITMAP_DATA		pBitmapDataOdd = NULL;
	xc_uint32	out[4];
	xc_uint32	*digest;
	xc_uint32	IsOkFirstCheck = 0;
	

	/* First check */
	pBitmapDataEven = (PXIDISK_BITMAP_DATA)xixcore_GetDataBuffer(xbuf);

	xixcore_md5digest_metadata((xc_uint8 *)&(pBitmapDataEven->SequenceNum), ( DataSize - XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pBitmapDataEven->Digest);
	


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5BitmapData pBitmapDataEven(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pBitmapDataEven,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL EVEN checkMd5BitmapData pBitmapDataEven(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pBitmapDataEven,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));


		IsOkFirstCheck = 0;
	}else {
		IsOkFirstCheck = 1;
	}

	/* Second check */
	pBitmapDataOdd = (PXIDISK_BITMAP_DATA) ((xc_uint8 *)xixcore_GetDataBuffer(xbuf) + DataSize);

	xixcore_md5digest_metadata((xc_uint8 *)&(pBitmapDataOdd->SequenceNum), ( DataSize - XIXCORE_MD5DIGEST_SIZE ), (xc_uint8 *)out);
	digest =(xc_uint32 *) (pBitmapDataOdd->Digest);
	


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL, 
		("checkMd5BitmapData pBitmapDataOdd(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pBitmapDataOdd,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));

	if(memcmp((xc_uint8 *)out, (xc_uint8 *)digest, XIXCORE_MD5DIGEST_SIZE) != 0 )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("FaIL checkMd5BitmapData pBitmapDataOdd(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
		pBitmapDataOdd,
		out[0],out[1],out[2],out[3],
		digest[0], digest[1], digest[2], digest[3]));



		if(IsOkFirstCheck == 1 ) {
			*ppBitmapData = pBitmapDataEven;
			return XCCODE_SUCCESS;
		}
		/*
		xixcore_DebugPrintf("FaIL ALL checkMd5HostRecord pHostRecord(0x%p) calced digest(0x%x%x%x%x) saved digest(0x%x%x%x%x)\n", 
			pHostRecord,
			out[0],out[1],out[2],out[3],
			digest[0], digest[1], digest[2], digest[3]);
		*/
		return XCCODE_CRCERROR;
	}else {
		if(IsOkFirstCheck == 1 ) {
			if( compareSequence(
					pBitmapDataEven->SequenceNum, 
					pBitmapDataOdd->SequenceNum 
					)  >= 0 )
			{
				*ppBitmapData = pBitmapDataEven;
				return XCCODE_SUCCESS;
			}else{
				*ppBitmapData = pBitmapDataOdd;
				xixcore_IncreaseBufferOffset(xbuf, DataSize);
				return XCCODE_SUCCESS;
			}
		}else {
			*ppBitmapData = pBitmapDataOdd;
			xixcore_IncreaseBufferOffset(xbuf, DataSize);
			return XCCODE_SUCCESS;
		}
	}

}


static 
void
makeMd5BitmapData(
	IN PXIDISK_BITMAP_DATA		pBitmapData,
	IN	xc_uint32				DataSize
)
{
	xc_uint32	*digest;


	xixcore_md5digest_metadata((xc_uint8 *)&(pBitmapData->SequenceNum), ( DataSize - XIXCORE_MD5DIGEST_SIZE), pBitmapData->Digest);
	digest = (xc_uint32 *)pBitmapData->Digest;



	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_DEVCTL,
		("makeMd5BitmapData pBitmapData(0x%p) lotnumber(0x%x%x%x%x)\n", 
		pBitmapData,digest[0],digest[1],digest[2],digest[3]));

}

int
xixcore_EndianSafeReadBitmapData(
	PXIXCORE_BLOCK_DEVICE	bdev, 
	xc_sector_t				startsector, 
	xc_uint32				DataSize,	
	xc_int32				sectorsize, 
	xc_uint32				sectorsizeBit,
	PXIXCORE_BUFFER			xbuf,
	xc_int32				*Reason
)
{
	int RC = 0;
	PXIDISK_BITMAP_DATA	pLotHeader = NULL;
	*Reason = 0;	

	RC = xixcore_DeviceIoSync(
			bdev,
			startsector,
			(DataSize * 2),
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_READ,
			Reason
			);
		
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeReadBitmapData Status(0x%x)\n", RC));

		return RC;
	}
	
	RC = checkMd5BitmapData(xbuf, DataSize, &pLotHeader);
	
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error checkMd5BitmapData Status(0x%x)\n", RC));

		return RC;
	}

#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeBitmapData(pLotHeader);
#endif		

	return RC;
}




int
xixcore_EndianSafeWriteBitmapData(
	PXIXCORE_BLOCK_DEVICE	bdev, 
	xc_sector_t				startsector, 
	xc_uint32				DataSize,	
	xc_int32				sectorsize, 
	xc_uint32				sectorsizeBit,
	PXIXCORE_BUFFER			xbuf,
	xc_int32				*Reason
)
{
	int RC = 0;
	PXIDISK_BITMAP_DATA	pLotHeader = NULL;
	xc_sector_t newStartsector = 0;

	pLotHeader = (PXIDISK_BITMAP_DATA)xixcore_GetDataBufferWithOffset(xbuf);
	pLotHeader->SequenceNum ++;
	
	if(pLotHeader->SequenceNum % 2) {
		newStartsector = startsector + SECTOR_ALIGNED_COUNT(sectorsizeBit, DataSize); 
	}else{
		newStartsector = startsector;
	}


	*Reason = 0;



#if defined(__BIG_ENDIAN_BITFIELD)	
	writeChangeBitmapData(pLotHeader);
#endif

	makeMd5BitmapData(pLotHeader, DataSize);

	RC = xixcore_DeviceIoSync(
			bdev,
			newStartsector,
			DataSize,
			sectorsize, 
			sectorsizeBit,
			xbuf,
			XIXCORE_WRITE,
			Reason
			);
		
	if( RC < 0 ){

#if defined(__BIG_ENDIAN_BITFIELD)			
		readChangeBitmapData(pLotHeader);
#endif
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Error xixcore_EndianSafeWriteBitmapData Status(0x%x)\n", RC));

		return RC;
	}
	
#if defined(__BIG_ENDIAN_BITFIELD)	
	readChangeBitmapData(pLotHeader);
#endif		

	return RC;
}





