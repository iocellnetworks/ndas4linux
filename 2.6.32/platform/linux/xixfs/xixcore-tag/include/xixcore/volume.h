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
#ifndef __XIXCORE_VOLUME_H__
#define __XIXCORE_VOLUME_H__

#include "xixcore/xctypes.h"
#include "xcsystem/systypes.h"
#include "xixcore/buffer.h"

typedef struct _XIXCORE_GLOBAL {
	
	// Access control temp lot lock
	PXIXCORE_SPINLOCK	tmp_lot_lock_list_lock;
	XIXCORE_LISTENTRY	tmp_lot_lock_list;

	xc_uint8			HostMac[32];
	xc_uint8			HostId[16];
	xc_uint32			IsInitialized;

	xc_le16	*xixcore_case_table;
	xc_uint32	xixcore_case_table_length;

}XIXCORE_GLOBAL, *PXIXCORE_GLOBAL;

extern XIXCORE_GLOBAL xixcore_global;

/*
 * Structure node type
 */
#define	XIXCORE_NODE_TYPE_VCB	(0x1901)
#define	XIXCORE_NODE_TYPE_FCB	(0x1902)

typedef struct _XIXCORE_NODE_TYPE {
	xc_uint16	Type;
	xc_uint16	Size;
} XIXCORE_NODE_TYPE, *PXIXCORE_NODE_TYPE ;

#if defined(XIXCORE_DEBUG)

#define XIXCORE_GET_NODE_TYPE(P)	( ((PXIXCORE_NODE_TYPE)(P))->Type )
#define XIXCORE_GET_NODE_SIZE(P)	( ((PXIXCORE_NODE_TYPE)(P))->Size )
#define XIXCORE_ASSERT_STRUCT(S, T, SZ)	XIXCORE_ASSERT( \
		XIXCORE_GET_NODE_TYPE(S) == (T) && \
		XIXCORE_GET_NODE_SIZE(S) == (SZ) )
#define XIXCORE_ASSERT_VCB(F)		XIXCORE_ASSERT_STRUCT((F), XIXCORE_NODE_TYPE_VCB, sizeof(XIXCORE_VCB))
#define XIXCORE_ASSERT_FCB(F)		XIXCORE_ASSERT_STRUCT((F), XIXCORE_NODE_TYPE_FCB, sizeof(XIXCORE_FCB))

#else

#define XIXCORE_GET_NODE_TYPE(P)	do{;}while(0)
#define XIXCORE_GET_NODE_SIZE(P)	do{;}while(0)
#define XIXCORE_ASSERT_STRUCT(S, T)	do{;}while(0)
#define XIXCORE_ASSERT_FCB(p)		do{;}while(0)
#define XIXCORE_ASSERT_VCB(p)		do{;}while(0)

#endif //#if defined(XIXCORE_DEBUG)


#define XIXCORE_META_FLAGS_RECHECK_RESOURCES			(0x00000001)
#define XIXCORE_META_FLAGS_INSUFFICIENT_RESOURCES		(0x00000002)
#define XIXCORE_META_FLAGS_KILL_THREAD					(0x00000004)
#define XIXCORE_META_FLAGS_UPDATE						(0x00000008)
#define XIXCORE_META_FLAGS_TIMEOUT						(0x00000010)
#define XIXCORE_META_FLAGS_MASK							(0x0000001F)

#define XIXCORE_META_RESOURCE_NEED_UPDATE				(0x00000001)

typedef struct _XIXCORE_META_CTX {

	// volume related
	xc_uint64			AllocatedLotMapIndex;
	xc_uint64			FreeLotMapIndex;
	xc_uint64			CheckOutLotMapIndex;
	xc_uint64			HostRegLotMapIndex;
	xc_uint32			HostRegLotMapLockStatus;

	xc_uint32			HostRecordIndex;

	// host related
	
	xc_uint64			HostCheckOutLotMapIndex;
	xc_uint64			HostUsedLotMapIndex;
	xc_uint64			HostUnUsedLotMapIndex;

	PXIXCORE_SPINLOCK	MetaLock;

	PXIXCORE_LOT_MAP	HostFreeLotMap;
	PXIXCORE_LOT_MAP	HostDirtyLotMap;
	PXIXCORE_LOT_MAP	VolumeFreeMap;

	xc_uint32			VCBMetaFlags;
	xc_uint32			ResourceFlag;

	struct _XIXCORE_VCB	*XixcoreVCB;

} XIXCORE_META_CTX, *PXIXCORE_META_CTX;


#define XIXCORE_MAX_CHILD_CACHE	512
#define XIXCORE_DEFAULT_REMOVE	128
typedef struct _XIXCORE_VCB {
	XIXCORE_NODE_TYPE NodeType;

	int			IsVolumeWriteProtected;
	xc_uint8	HostId[16];
	xc_uint8	HostMac[32];
	xc_uint8	VolumeId[16];

	xc_uint64	NumLots;
	xc_uint32	LotSize;

	xc_uint32	SectorSize;
	xc_uint32	SectorSizeBit;
	xc_uint32	AddrLotSize;
	xc_uint32	VolumeLotSignature;
	xc_uint64	VolCreateTime;
	xc_uint32	VolSerialNumber;

	xc_uint8	*VolumeName;
	xc_uint32	VolumeNameLength;

	xc_uint64	RootDirectoryLotIndex;

	PXIXCORE_BLOCK_DEVICE	XixcoreBlockDevice;

	XIXCORE_META_CTX	MetaContext;
	//	Added by ILGU HONG
	xc_uint32			childCacheCount;
	PXIXCORE_SPINLOCK	childEntryCacheSpinlock;
	XIXCORE_LISTENTRY	LRUchildEntryCacheHeader;
	XIXCORE_LISTENTRY	HASHchildEntryCacheHeader[10];
	//	Added by ILGU HONG END
} XIXCORE_VCB, *PXIXCORE_VCB;


int
xixcore_call
xixcore_IntializeGlobalData(
	xc_uint8 uuid[],
	PXIXCORE_SPINLOCK AuxLockListSpinLock
);

void
xixcore_call
xixcore_InitializeVolume(
	PXIXCORE_VCB			XixcoreVcb,
	PXIXCORE_BLOCK_DEVICE	XixcoreBlockDevice,
	PXIXCORE_SPINLOCK		XixcoreChidcacheSpinLock,
	xc_uint8				IsReadOnly,
	xc_uint16				SectorSize,
	xc_uint16				SectorSizeBit,
	xc_uint32				AddrLotSize,
	xc_uint8				VolumeId[]
	);

void
xixcore_call
xixcore_InitializeMetaContext(
	PXIXCORE_META_CTX MetaContext,
	PXIXCORE_VCB pVCB,
	PXIXCORE_SPINLOCK MetaLock
	);

int 
xixcore_call
xixcore_checkVolume(
	PXIXCORE_BLOCK_DEVICE	xixBlkDev,
	xc_uint32				sectorsize,
	xc_uint32				sectorsizeBit,
	xc_uint32				LotSize,
	xc_sector_t				LotNumber,
	PXIXCORE_BUFFER			VolumeInfo,
	PXIXCORE_BUFFER			LotHeader,
	xc_uint8				*VolumeId
	);

int 
xixcore_call
xixcore_GetSuperBlockInformation(
	PXIXCORE_VCB pVCB,
	xc_uint32		LotSize,
	xc_sector_t		VolumeIndex
	);


#endif // #ifndef __XIXCORE_VOLUME_H__

