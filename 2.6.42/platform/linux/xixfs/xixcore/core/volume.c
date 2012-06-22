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
#include "xixcore/dir.h"
#include "xixcore/volume.h"

/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCVOL"

#if defined(XIXCORE_DEBUG)
long XixcoreDebugLevel;
long XixcoreDebugTarget;
#endif

XIXCORE_GLOBAL xixcore_global;

int
xixcore_call
xixcore_IntializeGlobalData(
	xc_uint8 uuid[],
	PXIXCORE_SPINLOCK AuxLockListSpinLock
)
{
	memset((void *)&xixcore_global, 0, sizeof(XIXCORE_GLOBAL));

	xixcore_global.tmp_lot_lock_list_lock = AuxLockListSpinLock;
	xixcore_InitializeSpinLock(xixcore_global.tmp_lot_lock_list_lock);
	xixcore_InitializeListHead(&(xixcore_global.tmp_lot_lock_list));

	xixcore_global.xixcore_case_table_length = XIXCORE_DEFAULT_UPCASE_NAME_LEN;
	xixcore_global.xixcore_case_table = xixcore_GenerateDefaultUpcaseTable();
	if(xixcore_global.xixcore_case_table == NULL) {
		return XCCODE_UNSUCCESS;
	}
	memcpy(xixcore_global.HostId, uuid, 16);
	xixcore_global.IsInitialized = 1;
	
	return XCCODE_SUCCESS;
}

/*
 * Initialize Xixcore VCB
 */

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
	)
{
	xc_uint32			i = 0;
	memset((void *)XixcoreVcb, 0, sizeof(XIXCORE_VCB));

	XixcoreVcb->NodeType.Type = XIXCORE_NODE_TYPE_VCB;
	XixcoreVcb->NodeType.Size = sizeof(XIXCORE_VCB);

	XixcoreVcb->IsVolumeWriteProtected = IsReadOnly;
	XixcoreVcb->SectorSize = SectorSize;
	XixcoreVcb->SectorSizeBit = SectorSizeBit;
	XixcoreVcb->AddrLotSize = AddrLotSize;
	// Set block device to Xixcore VCB
	XixcoreVcb->XixcoreBlockDevice = XixcoreBlockDevice;

	XixcoreVcb->childCacheCount = 0;
	XixcoreVcb->childEntryCacheSpinlock = XixcoreChidcacheSpinLock; 
	xixcore_InitializeSpinLock(XixcoreVcb->childEntryCacheSpinlock);
	xixcore_InitializeListHead(&(XixcoreVcb->LRUchildEntryCacheHeader));
	
	for(i = 0; i< 10; i++){
		xixcore_InitializeListHead(&(XixcoreVcb->HASHchildEntryCacheHeader[i]));
	}

	/*
	 * Host information
	 */
	memcpy(XixcoreVcb->HostId, xixcore_global.HostId, 16);
	memcpy(XixcoreVcb->HostMac, xixcore_global.HostMac, 32);
	memcpy(XixcoreVcb->VolumeId, VolumeId, 16);
}


void
xixcore_call
xixcore_InitializeMetaContext(
	PXIXCORE_META_CTX MetaContext,
	PXIXCORE_VCB XixcoreVcb,
	PXIXCORE_SPINLOCK MetaLock
	)
{
	memset((void *)MetaContext, 0, sizeof(XIXCORE_META_CTX));
	MetaContext->XixcoreVCB = XixcoreVcb;
	MetaContext->MetaLock = MetaLock;

	xixcore_InitializeSpinLock(MetaLock);
}

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
	)
{
	int 					RC = 0;
	PXIDISK_COMMON_LOT_HEADER	pLotHeader = NULL;
	PXIDISK_VOLUME_INFO		pVolumeInfo = NULL;
	xc_int32				reason = 0;


	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_VOLINFO, 
			("Enter xixcore_checkVolume .\n" ));

	XIXCORE_ASSERT(VolumeInfo);
	XIXCORE_ASSERT(LotHeader);
	XIXCORE_ASSERT(xixcore_GetBufferSizeWithOffset(VolumeInfo) >= XIDISK_DUP_VOLUME_INFO_SIZE);
	XIXCORE_ASSERT(xixcore_GetBufferSizeWithOffset(LotHeader) >= XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	memset(xixcore_GetDataBuffer(VolumeInfo),0,XIDISK_DUP_VOLUME_INFO_SIZE);
	memset(xixcore_GetDataBuffer(LotHeader),0,XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	RC = xixcore_RawReadLotHeader(
							xixBlkDev,
							LotSize,
							sectorsize, 
							sectorsizeBit,
							LotNumber,
							LotHeader, 
							&reason
							);  


	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("FAIL  xixcore_checkVolume : xixcore_RawReadLotHeader %x .\n", RC ));	
		goto error_out;
	}

	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(LotHeader);


	if(pLotHeader->LotInfo.Type  != LOT_INFO_TYPE_VOLUME)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail(0x%x)  Is Not LOT_INFO_TYPE_VOLUME .\n", 
			pLotHeader->LotInfo.Type));
		RC = XCCODE_EINVAL;
		goto error_out;
	}


	
	RC = xixcore_RawReadVolumeHeader(xixBlkDev,
									LotSize,
									sectorsize, 
									sectorsizeBit,
									LotNumber,
									VolumeInfo, 
									&reason
									);  


	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("FAIL  xixcore_checkVolume : xixcore_RawReadVolumeHeader %x .\n", RC ));	
		goto error_out;
	}


	pVolumeInfo = (PXIDISK_VOLUME_INFO)xixcore_GetDataBufferWithOffset(VolumeInfo);
	


	if(pVolumeInfo->VolumeSignature != XIFS_VOLUME_SIGNATURE)
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail (0x%llx)  Is XIFS_VOLUME_SIGNATURE .\n", 
			pVolumeInfo->VolumeSignature));
		RC = XCCODE_EINVAL;
		goto error_out;
	}

	if((pVolumeInfo->XifsVesion > XIFS_CURRENT_VERSION))
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail(0x%x)  Is XIFS_CURRENT_VERSION .\n",
			pVolumeInfo->XifsVesion ));
		RC = XCCODE_EINVAL;
		goto error_out;		
	}

	memcpy(VolumeId, pVolumeInfo->VolumeId, 16);
	RC = 0;


error_out:
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_VOLINFO, 
			("End xixcore_checkVolume %x.\n", RC ));


	return RC;
}


int 
xixcore_call
xixcore_GetSuperBlockInformation(
	PXIXCORE_VCB pVCB,
	xc_uint32		LotSize,
	xc_sector_t		VolumeIndex
	)
{
	int 					RC = 0;
	PXIDISK_VOLUME_INFO	VolInfo = NULL;
	PXIXCORE_BUFFER			xbuf = NULL;
	xc_int32				reason = 0;

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_VOLINFO, 
			("Enter xixcore_GetSuperBlockInformation .\n" ));


	xbuf = xixcore_AllocateBuffer(XIDISK_DUP_VOLUME_INFO_SIZE);
	if(!xbuf){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("FAIL xixcore_GetSuperBlockInformation : can't allocat xbuf .\n" ));	
		return XCCODE_ENOMEM;
	}

	memset(xixcore_GetDataBuffer(xbuf), 0, XIDISK_DUP_VOLUME_INFO_SIZE);
	
	RC = xixcore_RawReadVolumeHeader(pVCB->XixcoreBlockDevice,
											LotSize,
											pVCB->SectorSize,
											pVCB->SectorSizeBit,
											VolumeIndex,
											xbuf, 
											&reason
											);  


	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("FAIL  xixcore_checkVolume : xixcore_RawReadVolumeHeader %x .\n", RC ));	
		goto error_out;
	}	

	VolInfo = (PXIDISK_VOLUME_INFO)xixcore_GetDataBufferWithOffset(xbuf);

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("VolInfo HostRegLotMap %lld:RootLotMap %lld: LotSize: %d : TotalLotNumber %lld .\n",
			VolInfo->HostRegLotMapIndex, VolInfo->RootDirectoryLotIndex, VolInfo->LotSize, VolInfo->NumLots));

	pVCB->MetaContext.HostRegLotMapIndex = VolInfo->HostRegLotMapIndex;
	pVCB->RootDirectoryLotIndex = VolInfo->RootDirectoryLotIndex;
	pVCB->NumLots = VolInfo->NumLots;
	pVCB->LotSize = VolInfo->LotSize;
	pVCB->VolumeLotSignature = VolInfo->LotSignature;

	// Changed by ILGU HONG
	memcpy(pVCB->VolumeId, VolInfo->VolumeId,16);

	pVCB->VolCreateTime = VolInfo->VolCreationTime;
	pVCB->VolSerialNumber = VolInfo->VolSerialNumber;
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("VCB HostRegLotMap %lld:RootLotMap %lld: LotSize: %d : TotalLotNumber %lld .\n",
			pVCB->MetaContext.HostRegLotMapIndex, pVCB->RootDirectoryLotIndex, pVCB->LotSize, pVCB->NumLots));

	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("VCB Volume Signature (0x%x).\n",
			pVCB->VolumeLotSignature));
	
	if(pVCB->VolumeName != NULL) {
		xixcore_FreeMem(pVCB->VolumeName, XCTAG_VOLNAME);
	}


	if(VolInfo->VolLabelLength > 0 ) {

		if(pVCB->VolumeName) {
			if(VolInfo->VolLabelLength > SECTORALIGNSIZE_512(pVCB->VolumeNameLength) ) {
				xixcore_FreeMem(pVCB->VolumeName, XCTAG_VOLNAME);

				pVCB->VolumeName = xixcore_AllocateMem(SECTORALIGNSIZE_512( VolInfo->VolLabelLength),0,XCTAG_VOLNAME);
				if(pVCB->VolumeName  == NULL) {
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
							("FAIL  xixcore_checkVolume : can't allocate volume name.\n"));	
					RC = XCCODE_ENOMEM;
					goto error_out;
				}
			}
		}else {
			pVCB->VolumeName = xixcore_AllocateMem(SECTORALIGNSIZE_512(VolInfo->VolLabelLength), 0, XCTAG_VOLNAME);
			if(pVCB->VolumeName  == NULL) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
						("FAIL  xixcore_checkVolume : can't allocate volume name.\n"));	
				RC = XCCODE_ENOMEM;
				goto error_out;
			}
		}

		pVCB->VolumeNameLength = VolInfo->VolLabelLength;
		memset(pVCB->VolumeName, 0, SECTORALIGNSIZE_512(pVCB->VolumeNameLength));
		memcpy(pVCB->VolumeName, VolInfo->VolLabel, pVCB->VolumeNameLength);

		
	}
	
	
	
error_out:
	xixcore_FreeBuffer(xbuf);
	return RC;
}

