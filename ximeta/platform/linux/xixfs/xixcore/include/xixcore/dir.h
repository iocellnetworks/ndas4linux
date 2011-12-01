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
#ifndef __XIXCORE_DIR_H__
#define __XIXCORE_DIR_H__


#include "xixcore/xctypes.h"
#include "xixcore/file.h"
#include "xixcore/layouts.h"


typedef struct _XIXCORE_CHILD_CACHE_INFO{
	xc_uint64			ParentDirLotIndex;
	xc_uint32			Type;
	xc_uint32			State;						//	delete/create/linked/...
	xc_uint32			NameSize;
	xc_uint32			ChildIndex;
	xc_uint32			HashValue;
	xc_uint32			FileAttribute;
	xc_uint64			StartLotIndex;
	xc_uint64			FileSize;
	xc_uint64			AllocationSize;	
	xc_uint64			Modified_time;
	xc_uint64			Change_time;
	xc_uint64			Access_time;
	xc_uint64			Create_time;
	xc_uint8			Name[1];
}XIXCORE_CHILD_CACHE_INFO, *PXIXCORE_CHILD_CACHE_INFO;


// Added by ILGU HONG
typedef struct _XIXCORE_CHILD_CACHE_ENTRY{
	XIXCORE_LISTENTRY	LRUChildCacheLink;
	XIXCORE_LISTENTRY	HASHChildCacheLink;	
	XIXCORE_ATOMIC		RefCount;
	xc_uint64			RefTime;
	XIXCORE_CHILD_CACHE_INFO	ChildCacheInfo;
} XIXCORE_CHILD_CACHE_ENTRY, *PXIXCORE_CHILD_CACHE_ENTRY;

void
xixcore_call
xixcore_RefChildCacheEntry(
	IN PXIXCORE_CHILD_CACHE_ENTRY	pChildCacheEntry
);

void
xixcore_call
xixcore_DeRefChildCacheEntry(
	IN PXIXCORE_CHILD_CACHE_ENTRY	pChildCacheEntry
);


void
xixcore_call
xixcore_RemoveLRUCacheEntry(
	IN PXIXCORE_VCB 	pVCB,
	IN xc_uint32		count
);


int
xixcore_call
xixcore_CreateChildCacheEntry(
	IN PXIXCORE_VCB 	pVCB,
	IN xc_uint64		ParentDirLotIndex,
	IN PXIDISK_CHILD_INFORMATION	pChildEntry,
	IN PXIDISK_FILE_INFO			pFileInfo
);

int
xixcore_call
xixcore_FindChildCacheEntry(
	IN PXIXCORE_VCB 	pVCB,
	IN xc_uint64		ParentDirLotIndex,
	IN xc_uint32		ChildIndex,
	OUT PXIXCORE_CHILD_CACHE_ENTRY *ppChildCacheEntry
);


int
xixcore_call
xixcore_UpdateChildCacheEntry(
	IN PXIXCORE_VCB 	pVCB,
	IN xc_uint64		ParentDirLotIndex,
	IN xc_uint64		FileLotIndex,
	IN xc_uint32		FileAttribute,
	IN xc_uint64		FileSize,
	IN xc_uint64		AllocationSize,	
	IN xc_uint64		Modified_time,
	IN xc_uint64		Change_time,
	IN xc_uint64		Access_time,
	IN xc_uint64		Create_time
);

void
xixcore_call
xixcore_DeleteSpecialChildCacheEntry(
	IN PXIXCORE_VCB 	pVCB,
	IN xc_uint64		ParentDirLotIndex,
	IN xc_uint32		ChildIndex
);

void
xixcore_call
xixcore_DeleteALLChildCacheEntry(
	IN PXIXCORE_VCB 	pVCB
);

// Added by ILGU HONG END

typedef struct _XIXCORE_DIR_EMUL_CONTEXT{
	xc_uint8		*AddrMap;
	PXIXCORE_BUFFER	AddrMapBuff;
	xc_uint32		AddrMapSize;
	xc_uint32		AddrIndexNumber;

	xc_uint8		*ChildBitMap;
	//		Added by ILGU HONG 12082006
	PXIXCORE_BUFFER		ChildHashTable;
	//		Added by ILGU HONG 12082006 END	
	PXIXCORE_BUFFER		ChildEntryBuff;
	
	xc_uint64			LotNumber; // Dir Fcb lot number
	xc_uint32			FileType;
	xc_uint8			*ChildName;
	xc_uint32			ChildNameLength;
	
	xc_uint32			Type;
	xc_uint32			State;						
	xc_uint32			NameSize;
	xc_uint32			ChildIndex;
	xc_uint64			StartLotIndex;
	xc_uint32			HashValue;
	xc_uint8 			*Name;



	PXIXCORE_FCB		pFCB;
	PXIXCORE_VCB		pVCB;
	xc_uint64			VirtualDirIndex;
	xc_int64			RealDirIndex;
	
	xc_uint64			SearchedVirtualDirIndex;
	xc_uint64			SearchedRealDirIndex;

	xc_uint32					IsChildCache;
	xc_uint32					IsVirtual;
}XIXCORE_DIR_EMUL_CONTEXT, *PXIXCORE_DIR_EMUL_CONTEXT;


int
xixcore_call
xixcore_RawReadDirEntry(
	PXIXCORE_VCB 	pVCB,
	PXIXCORE_FCB 	pFCB,
	xc_uint32		Index,
	PXIXCORE_BUFFER 	buffer,
	xc_uint32		bufferSize,
	PXIXCORE_BUFFER	AddrBuffer,
	xc_uint32		AddrBufferSize,
	xc_uint32		*AddrStartSecIndex
);

int
xixcore_call
xixcore_RawWriteDirEntry(
	PXIXCORE_VCB 	pVCB,
	PXIXCORE_FCB 	pFCB,
	xc_uint32		Index,
	PXIXCORE_BUFFER 	buffer,
	xc_uint32		bufferSize,
	PXIXCORE_BUFFER	AddrBuffer,
	xc_uint32		AddrBufferSize,
	xc_uint32		*AddrStartSecIndex
);

int
xixcore_call
xixcore_InitializeDirContext(
	PXIXCORE_VCB		pVCB,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext
);

void
xixcore_call
xixcore_CleanupDirContext(
    PXIXCORE_DIR_EMUL_CONTEXT DirContext
);

int
xixcore_call
xixcore_LookupInitialDirEntry(
	PXIXCORE_VCB	pVCB,
	PXIXCORE_FCB 	pFCB,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext,
	xc_uint32 InitIndex
);

/*
return value 
	0 --> find
	-1 --> error
	-2 --> no more file
*/
int
xixcore_call
xixcore_UpdateDirNames(
	PXIXCORE_DIR_EMUL_CONTEXT DirContext
);

int
xixcore_call
xixcore_LookupSetFileIndex(
	PXIXCORE_DIR_EMUL_CONTEXT DirContext,
	xc_uint64 InitialIndex
);

int
xixcore_call
xixcore_FindDirEntryByLotNumber(
    PXIXCORE_VCB	pVCB,
    PXIXCORE_FCB 	pFCB,
    xc_uint64	LotNumber,
    PXIXCORE_DIR_EMUL_CONTEXT DirContext,
    xc_uint64	*EntryIndex
);

int
xixcore_call
xixcore_FindDirEntry (
	PXIXCORE_VCB	pVCB,
	PXIXCORE_FCB 	pFCB,
	xc_uint8		*Name,
	xc_uint32		NameLength,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext,
	xc_uint64					*EntryIndex,
	xc_uint32					bIgnoreCase
    );

int
xixcore_call
xixcore_AddChildToDir(
	PXIXCORE_VCB					pVCB,
	PXIXCORE_FCB					pDir,
	xc_uint64						ChildLotNumber,
	xc_uint32						Type,
	xc_uint8 						*ChildName,
	xc_uint32						ChildNameLength,
	PXIXCORE_DIR_EMUL_CONTEXT	DirContext,
	xc_uint64 *						ChildIndex
);

int
xixcore_call
xixcore_DeleteChildFromDir(
	PXIXCORE_VCB 				pVCB,
	PXIXCORE_FCB				pDir,
	xc_uint64					ChildIndex,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext
);

int
xixcore_call
xixcore_UpdateChildFromDir(
	PXIXCORE_VCB		pVCB,
	PXIXCORE_FCB		pDir,
	xc_uint64			ChildLotNumber,
	xc_uint32			Type,
	xc_uint32			State,
	xc_uint8			*ChildName,
	xc_uint32			ChildNameLength,
	xc_uint64			ChildIndex,
	PXIXCORE_DIR_EMUL_CONTEXT DirContext
);


/*
 * Unicode manipulation
 */

#define XIXCORE_DEFAULT_UPCASE_NAME_LEN 	(0x10000)
#define XIXCORE_MAX_NAME_LEN				1000

xc_uint32 
xixcore_call
xixcore_CompareUcs(
	const xc_le16 *s1,
	const xc_le16 *s2,
	xc_size_t n);

xc_uint32
xixcore_call
xixcore_CompareUcsNCase(
		const xc_le16 *s1, 
		const xc_le16 *s2, 
		xc_size_t n,
		const xc_le16 *upcase, 
		const xc_uint32 upcase_size
);

xc_uint32 
xixcore_call
xixcore_IsNamesEqual(
	const xc_le16 		*s1, 
	xc_size_t 			s1_len,
	const xc_le16 		*s2, 
	xc_size_t 			s2_len, 
	xc_uint32 			bIgnoreCase,
	const xc_le16 		*upcase, 
	const xc_uint32 		upcase_size
);

void 
xixcore_call
xixcore_UpcaseUcsName(
	xc_le16 *name, 
	xc_uint32 name_len, 
	const xc_le16 *upcase,
	const xc_uint32 upcase_len
);

xc_le16 *
xixcore_call
xixcore_GenerateDefaultUpcaseTable(void);

#endif //#ifndef __XIXCORE_DIR_H__
