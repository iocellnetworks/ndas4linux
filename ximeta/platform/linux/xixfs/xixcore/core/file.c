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
#include "xcsystem/system.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/buffer.h"
#include "xixcore/ondisk.h"
#include "xixcore/lotlock.h"
#include "xixcore/bitmap.h"
#include "xixcore/lotinfo.h"
#include "xixcore/dir.h"
#include "xixcore/file.h"
#include "xixcore/md5.h"

/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCFILE"

xc_int32
xixcore_call
xixcore_InitializeFCB(
	PXIXCORE_FCB		XixcoreFCB,
	PXIXCORE_VCB		XixcoreVCB,
	PXIXCORE_SEMAPHORE	XixcoreSemaphore
	)
{
	memset((void *)XixcoreFCB, 0, sizeof(XIXCORE_FCB));
	XixcoreFCB->NodeType.Type = XIXCORE_NODE_TYPE_FCB;
	XixcoreFCB->NodeType.Size = sizeof(XIXCORE_FCB);
	XixcoreFCB->AddrLotSize = XixcoreVCB->AddrLotSize;
	XixcoreFCB->AddrStartSecIndex = 0;
	XixcoreFCB->WriteStartOffset = -1;
	XixcoreFCB->AddrLock = XixcoreSemaphore;
	xixcore_InitializeSemaphore(XixcoreFCB->AddrLock, 1);
	
	XixcoreFCB->AddrLot  = xixcore_AllocateBuffer(XixcoreFCB->AddrLotSize);
	if (XixcoreFCB->AddrLot  ==  NULL) {
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("xixfs_alloc_inode: can't allocate AddrLot\n"));
		return XCCODE_ENOMEM;
	}

	return XCCODE_SUCCESS;
}

void
xixcore_call
xixcore_DestroyFCB(
	PXIXCORE_FCB		XixcoreFCB
	)
{
	if(XixcoreFCB->AddrLot) {
		xixcore_FreeBuffer(XixcoreFCB->AddrLot );
		XixcoreFCB->AddrLot = NULL;
	}

	if(XixcoreFCB->FCBName) {
		xixcore_FreeMem(XixcoreFCB->FCBName, XCTAG_FCBNAME);
		XixcoreFCB->FCBName = NULL;
		XixcoreFCB->FCBNameLength = 0;
	}
	
}

int
xixcore_call
xixcore_CheckFileDirFromBitMapContext(
	PXIXCORE_VCB VCB,
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
)
{
	int							RC = 0;
	PXIDISK_COMMON_LOT_HEADER	pCommonLotHeader = NULL;
	PXIXCORE_LOT_MAP			HostFreeMap = NULL;
	PXIXCORE_LOT_MAP			HostDirtyMap = NULL;
	PXIXCORE_LOT_MAP			HostCheckOutMap = NULL;
	xc_int64					SearchIndex = -1;
	PXIXCORE_BUFFER				xbuf = NULL;
	PXIXCORE_BUFFER				LotHeader = NULL;
	xc_uint32					Size = 0;
	int							Reason = 0;
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixcore_CheckFileDirFromBitMapContext .\n"));	

	

	HostFreeMap = (PXIXCORE_LOT_MAP)&(BitmapEmulCtx->UnusedBitmap);
	HostDirtyMap = (PXIXCORE_LOT_MAP)&(BitmapEmulCtx->UsedBitmap);
	HostCheckOutMap = (PXIXCORE_LOT_MAP)&(BitmapEmulCtx->CheckOutBitmap);

	if(VCB->SectorSize > XIDISK_FILE_INFO_SIZE){
		Size = VCB->SectorSize;
	}else{
		Size = XIDISK_FILE_INFO_SIZE;
	}

	xbuf = xixcore_AllocateBuffer(XIDISK_DUP_FILE_INFO_SIZE);
	if(!xbuf) {
		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
			("Fail xixcore_CheckFileDirFromBitMapContext->Allocate xBuff\n"));
		return XCCODE_ENOMEM;
	}



	LotHeader  = xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
	if(!LotHeader) {
		xixcore_FreeBuffer(xbuf);
		DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
			("Fail xixcore_CheckFileDirFromBitMapContext->Allocate xBuff\n"));
		return XCCODE_ENOMEM;
	}

	
	// Check Start with Host Dirty map
	while(1)
	{
		SearchIndex = xixcore_FindSetBit(
						VCB->NumLots, 
						SearchIndex, 
						xixcore_GetDataBufferOfBitMap(HostDirtyMap->Data)
						);

		if((xc_uint64)SearchIndex >= VCB->NumLots){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail xixcore_CheckFileDirFromBitMapContext-->xixcore_FindSetBit End of Lot.\n"));
			break;
		}
		
		if(SearchIndex < 24){
			continue;
		}


		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Searched Index (%lld).\n", SearchIndex));

		xixcore_ZeroBufferOffset(LotHeader);
		memset(xixcore_GetDataBuffer(LotHeader), 0,XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

		// Read Lot map info
		RC = xixcore_RawReadLotHeader(
					VCB->XixcoreBlockDevice, 
					VCB->LotSize, 
					VCB->SectorSize,
					VCB->SectorSizeBit,
					SearchIndex, 
					LotHeader,
					&Reason
					);

		if( RC < 0){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail(0x%x) (Reason %x)xixcore_CheckFileDirFromBitMapContext-->RawReadLot .\n", RC, Reason));
			if(RC == XCCODE_CRCERROR){
				pCommonLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(LotHeader);
				goto invalidate_lot;
			}else{
				goto error_out;
			}
		}					
		// Check Lot map info
		

		pCommonLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(LotHeader);

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Lot (%lld) Type(0x%x) Flags(0x%x).\n", 
				SearchIndex, pCommonLotHeader->LotInfo.Type, pCommonLotHeader->LotInfo.Flags));

		if( (pCommonLotHeader->LotInfo.Type & (LOT_INFO_TYPE_INVALID|LOT_INFO_TYPE_FILE|LOT_INFO_TYPE_DIRECTORY))
			&& (pCommonLotHeader->LotInfo.LotSignature == VCB->VolumeLotSignature))
		{

			


			if((pCommonLotHeader->LotInfo.Type == LOT_INFO_TYPE_INVALID) 
				|| (pCommonLotHeader->LotInfo.Flags == LOT_FLAG_INVALID ))
			{



					DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
						("!!!!! Find Null bitmap information Lot %lld !!!!!!\n", SearchIndex));


					xixcore_InitializeCommonLotHeader(
							pCommonLotHeader,
							VCB->VolumeLotSignature,
							LOT_INFO_TYPE_INVALID,
							LOT_FLAG_INVALID,
							SearchIndex,
							SearchIndex,
							0,
							0,
							0,
							0,
							0
							);	


					RC = xixcore_RawWriteLotHeader(
								VCB->XixcoreBlockDevice, 
								VCB->LotSize, 
								VCB->SectorSize,
								VCB->SectorSizeBit,
								SearchIndex, 
								LotHeader,
								&Reason
								);

					if( RC < 0){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
							("Fail(0x%x) (Reason %x)xixcore_CheckFileDirFromBitMapContext-->WriteReadLot .\n", RC, Reason));
						goto error_out;
					}					

					// Update Lot map
					xixcore_ClearBit(SearchIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(HostDirtyMap->Data));
					xixcore_SetBit(SearchIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(HostFreeMap->Data));						


			}else if((pCommonLotHeader->LotInfo.Type == LOT_INFO_TYPE_FILE) 
						&& (pCommonLotHeader->LotInfo.Flags == LOT_FLAG_BEGIN)){

					PXIDISK_FILE_INFO		pFileHeader;
					xc_uint32				i = 0;
					xc_uint32				AddrStartIndex = 0;
					xc_uint32				AddrSavedIndex = 0;
					xc_uint64				AdditionalAddrLot = 0;
					xc_uint32				bDelete = 0;
					xc_uint32				bStop = 0;
					xc_uint64				*Addr = NULL;
					xc_uint32				DefaultSecPerLotAddr = (xc_uint32)(VCB->AddrLotSize/sizeof(xc_uint64));
					DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
						("!!!!! Find a Valid Information Lot %lld !!!!!!\n", SearchIndex));
					
				
					RC = xixcore_AuxLotLock(
								VCB,
								SearchIndex,
								0,
								0
								);
				

					if( RC < 0){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
							("Fail(0x%x)xixcore_CheckFileDirFromBitMapContext--> xixcore_AuxLotLock .\n", RC));
						continue;
					}


					xixcore_ZeroBufferOffset(xbuf);
					memset(xixcore_GetDataBuffer(xbuf), 0,XIDISK_DUP_FILE_INFO_SIZE);

					RC = xixcore_RawReadFileHeader(
								VCB->XixcoreBlockDevice, 
								VCB->LotSize, 
								VCB->SectorSize,
								VCB->SectorSizeBit,
								SearchIndex, 
								xbuf,
								&Reason
								);

					if( RC < 0){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
							("Fail(0x%x) (Reason %x)xixcore_CheckFileDirFromBitMapContext-->xixcore_RawReadLotAndFileHeader .\n", RC, Reason));
						
						
						xixcore_AuxLotUnLock(
								VCB,
								SearchIndex
								);						
						
						if(RC == XCCODE_CRCERROR){
							pCommonLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(LotHeader);
							goto invalidate_lot;
						}else{
							goto error_out;
						}
					}					



					
					AddrStartIndex = 0;
					i = 0;

					pFileHeader = (PXIDISK_FILE_INFO)xixcore_GetDataBufferWithOffset(xbuf);

					if(pFileHeader->State == XIFS_FD_STATE_DELETED){
						bDelete = 1;
					}

					AdditionalAddrLot = pFileHeader->AddressMapIndex;
					
					
					do{
						xixcore_ZeroBufferOffset(xbuf);
						memset(xixcore_GetDataBuffer(xbuf), 0,XIDISK_DUP_FILE_INFO_SIZE);

						RC = xixcore_RawReadAddressOfFile(
										VCB->XixcoreBlockDevice, 
										VCB->LotSize,
										VCB->SectorSize,
										VCB->SectorSizeBit,
										SearchIndex,
										AdditionalAddrLot,
										VCB->AddrLotSize,
										&AddrSavedIndex,
										AddrStartIndex,
										xbuf,
										&Reason
										);
										
						if( RC < 0){
							DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
								("Fail(0x%x) (Reason %x)xixcore_CheckFileDirFromBitMapContext-->xixcore_RawReadAddressOfFile .\n", RC, Reason));
							bStop = 1;
							break;
						}					
					

						Addr = (xc_uint64 *)xixcore_GetDataBufferWithOffset(xbuf);

						for(i = 0; i<DefaultSecPerLotAddr; i++){
							if((Addr[i] != 0) && (Addr[i] >= 24) &&  (Addr[i] < VCB->NumLots)){
								

								if(xixcore_TestBit(Addr[i] , (unsigned long *)xixcore_GetDataBufferOfBitMap(HostCheckOutMap->Data))){

									DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
											("Update Addr Info Lo(%lld)\n", Addr[i]));

									if(bDelete == 1){
										xixcore_ClearBit(Addr[i], (unsigned long *)xixcore_GetDataBufferOfBitMap(HostDirtyMap->Data));
										xixcore_SetBit(Addr[i], (unsigned long *)xixcore_GetDataBufferOfBitMap(HostFreeMap->Data));	
									}else{
										xixcore_ClearBit(Addr[i], (unsigned long *)xixcore_GetDataBufferOfBitMap(HostFreeMap->Data));
										xixcore_SetBit(Addr[i], (unsigned long *)xixcore_GetDataBufferOfBitMap(HostDirtyMap->Data));											
									}
								}



							}else{
								bStop = 1;
								break;
							}
						}
						
						AddrStartIndex ++;

					}while(bStop == 0);

					xixcore_AuxLotUnLock(
								VCB,
								SearchIndex
								);
			}
		}else{
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Failed Lot (%lld) Type(0x%x) Flags(0x%x).\n", 
				SearchIndex, pCommonLotHeader->LotInfo.Type, pCommonLotHeader->LotInfo.Flags));


			if(pCommonLotHeader->LotInfo.LotSignature != VCB->VolumeLotSignature){
invalidate_lot:
					DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
						("!!!!! Find a inaccurate bitmap information Lot %lld !!!!!!\n", SearchIndex));

					xixcore_InitializeCommonLotHeader(
							pCommonLotHeader,
							VCB->VolumeLotSignature,
							LOT_INFO_TYPE_INVALID,
							LOT_FLAG_INVALID,
							SearchIndex,
							SearchIndex,
							0,
							0,
							0,
							0,
							0
							);	


					RC = xixcore_RawWriteLotHeader(
							VCB->XixcoreBlockDevice, 
							VCB->LotSize, 
							VCB->SectorSize,
							VCB->SectorSizeBit,
							SearchIndex, 
							LotHeader,
							&Reason
							);
					
					if( RC < 0){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
							("Fail(0x%x) (Reason %x)xixcore_CheckFileDirFromBitMapContext-->xixcore_RawWriteLotHeader .\n", RC, Reason));
						goto error_out;
					}					
					
					xixcore_ClearBit(SearchIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(HostDirtyMap->Data));
					xixcore_SetBit(SearchIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(HostFreeMap->Data));	

			}
		}

	}

	SearchIndex = -1;

	// Check Start with Host Free map
	while(1)
	{
		SearchIndex = xixcore_FindSetBit(VCB->NumLots, SearchIndex, xixcore_GetDataBufferOfBitMap(HostFreeMap->Data));

		if((xc_uint64)SearchIndex >= VCB->NumLots){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail xixcore_CheckFileDirFromBitMapContext-->xixcore_FindSetBit End of Lot.\n"));
			break;
		}
		
		if(SearchIndex < 24){
			continue;
		}


		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Searched Index (%lld).\n", SearchIndex));

		xixcore_ZeroBufferOffset(LotHeader);
		memset(xixcore_GetDataBuffer(LotHeader), 0,XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

		// Read Lot map info
		RC = xixcore_RawReadLotHeader(
				VCB->XixcoreBlockDevice, 
				VCB->LotSize, 
				VCB->SectorSize,
				VCB->SectorSizeBit,
				SearchIndex, 
				LotHeader,
				&Reason
				);

		if( RC < 0){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("Fail(0x%x) (Reason %x)xixcore_CheckFileDirFromBitMapContext-->xixcore_RawReasdLotHeader .\n", RC, Reason));
				if(RC == XCCODE_CRCERROR){
					continue;
				}else{
					goto error_out;
				}
		}
		
		pCommonLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(LotHeader);

		// Check Lot map info
		
		if( pCommonLotHeader->LotInfo.Type & (LOT_INFO_TYPE_INVALID | LOT_INFO_TYPE_FILE | LOT_INFO_TYPE_DIRECTORY))
		{
			if(	(pCommonLotHeader->LotInfo.Type != LOT_INFO_TYPE_INVALID) 
				&& (pCommonLotHeader->LotInfo.Flags != LOT_FLAG_INVALID )
				&& (pCommonLotHeader->LotInfo.LotSignature == VCB->VolumeLotSignature)) 
			{

					DebugTrace(DEBUG_LEVEL_ALL, DEBUG_TARGET_ALL,
						("!!!!! Find a Used bitmap information Lot %lld !!!!!!\n", SearchIndex));

					// Update Lot map
					xixcore_ClearBit(SearchIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(HostFreeMap->Data));
					xixcore_SetBit(SearchIndex, (unsigned long *)xixcore_GetDataBufferOfBitMap(HostDirtyMap->Data));						
			}
		}

	}

error_out:	
	
	if(xbuf) {
		xixcore_FreeBuffer(xbuf);
	}

	if(LotHeader){
		xixcore_FreeBuffer(LotHeader);
	}
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit xixcore_CheckFileDirFromBitMapContext Status(0x%x).\n", RC));	

	return RC;
}

int
xixcore_call
xixcore_CreateNewFile(
	PXIXCORE_VCB 					pVCB, 
	PXIXCORE_FCB 					pDir,
	xc_uint32						bIsFile,
	xc_uint8 *						Name, 
	xc_uint32 						NameLength, 
	xc_uint32 						FileAttribute,
	PXIXCORE_DIR_EMUL_CONTEXT 		DirContext,
	xc_uint64						*NewFileId
)
{
	int 			RC = 0;
	PXIXCORE_BUFFER	xbuf = NULL;
	PXIXCORE_BUFFER	LotHeader = NULL;
	xc_int64 		BeginingLotIndex = 0;
	xc_uint32		bLotAlloc = 0;
	xc_uint32		IsDirSet = 0;
	xc_uint32		ChildType = 0;
	xc_uint64 		ChildIndex = 0;
	xc_int32		Reason = 0;
	
	PXIDISK_COMMON_LOT_HEADER		pLotHeader = NULL;
	PXIDISK_FILE_INFO 				pFileInfo = NULL;
	PXIDISK_DIR_INFO 				pDirInfo = NULL;
	PXIDISK_ADDR_MAP				pAddressMap = NULL;
	xc_uint32							AddrStartSectorIndex = 0;
	xc_uint64						EntryIndex = 0;
	
	
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Enter xixcore_CreateNewFile \n"));
		

	XIXCORE_ASSERT_FCB(pDir);
	XIXCORE_ASSERT_VCB(pVCB);
	
	
	// Changed by ILGU HONG for readonly 09052006
	if(pVCB->IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixcore_CreateNewFile : is read only .\n"));	
		RC = XCCODE_EPERM;
		return RC;
	}
	// Changed by ILGU HONG for readonly end

	

	ChildType = (bIsFile)?(XIFS_FD_TYPE_FILE):(XIFS_FD_TYPE_DIRECTORY);

	if(NameLength > (XIFS_MAX_FILE_NAME_LENGTH * sizeof(xc_le16) )){
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
			("FileNameLen(%d).\n", NameLength));	
		NameLength = XIFS_MAX_FILE_NAME_LENGTH * sizeof(xc_le16);
	}


	xbuf = xixcore_AllocateBuffer(XIDISK_DUP_FILE_INFO_SIZE);

	if(xbuf == NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixcore_CreateNewFile : can't allocate xbuf .\n"));	
		RC = XCCODE_ENOMEM;		
		return RC;
	}

	LotHeader = xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if(LotHeader == NULL) {
		xixcore_FreeBuffer(xbuf);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixcore_CreateNewFile : can't allocate LotHeader .\n"));	
		RC = XCCODE_ENOMEM;		
		return RC;
	}



	RC = xixcore_FindDirEntry (
							pVCB,
							pDir,
							(xc_uint8 *)Name,
							NameLength,
							DirContext,
							&EntryIndex,
							1
							);
	if(RC >= 0){
		RC = XCCODE_EPERM;
		goto error_out;		
	}




	

	BeginingLotIndex = xixcore_AllocVCBLot(pVCB);
	if(BeginingLotIndex < 0 ){

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixcore_CreateNewFile : can't allocate lot.\n"));	
	
		RC =XCCODE_ENOSPC;
		goto error_out;
	}

	bLotAlloc = 1;
	
		RC = xixcore_AddChildToDir(
							pVCB,
							pDir,
							BeginingLotIndex,
							ChildType,
							Name,
							NameLength,
							DirContext,
							&ChildIndex
							);
	

		if( RC < 0 ){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x) xixcore_CreateNewFile : xixcore_AddChildToDir.\n", 
					RC));	
		
			goto error_out;
		}	
		




		IsDirSet =1;


		xixcore_ZeroBufferOffset(xbuf);
		memset(xixcore_GetDataBuffer(xbuf), 0,XIDISK_DUP_FILE_INFO_SIZE);

		xixcore_ZeroBufferOffset(LotHeader);
		memset(xixcore_GetDataBuffer(LotHeader), 0,XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
		
		
		DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO), 
			("Make File Info.\n"));


		RC = xixcore_RawReadLotHeader(
						pVCB->XixcoreBlockDevice,
						pVCB->LotSize,
						pVCB->SectorSize,
						pVCB->SectorSizeBit,
						BeginingLotIndex,
						LotHeader,
						&Reason
						);

		if(RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x) Reason(0x%x) xixcore_CreateNewFile : xixcore_RawWriteLotAndFileHeader.\n", 
					RC, Reason));	


			if(RC == XCCODE_CRCERROR){
				xixcore_ZeroBufferOffset(LotHeader);
			}else{
				goto error_out;
			}
		}




		RC = xixcore_RawReadFileHeader(
						pVCB->XixcoreBlockDevice,
						pVCB->LotSize,
						pVCB->SectorSize,
						pVCB->SectorSizeBit,
						BeginingLotIndex,
						xbuf,
						&Reason
						);

		if(RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x) Reason(0x%x) xixcore_CreateNewFile : xixcore_RawWriteFileHeader.\n", 
					RC, Reason));	


			if(RC == XCCODE_CRCERROR){
				xixcore_ZeroBufferOffset(xbuf);
			}else{
				goto error_out;
			}
		}


		if(ChildType == XIFS_FD_TYPE_FILE){
			
			pFileInfo = (PXIDISK_FILE_INFO)xixcore_GetDataBufferWithOffset(xbuf);
			pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(LotHeader);

			
			xixcore_InitializeCommonLotHeader(	
				pLotHeader,
				pVCB->VolumeLotSignature,
				LOT_INFO_TYPE_FILE,
				LOT_FLAG_BEGIN,
				BeginingLotIndex,
				BeginingLotIndex,
				0,
				0,
				0,
				sizeof(XIDISK_FILE_HEADER),
				pVCB->LotSize - sizeof(XIDISK_FILE_HEADER)
				);



			pLotHeader->Lock.LockState =  XIDISK_LOCK_RELEASED;
			pLotHeader->Lock.LockAcquireTime=0;
			memcpy(pLotHeader->Lock.LockHostSignature, pVCB->HostId , 16);
			// Changed by ILGU HONG
			//	chesung suggest
			//RtlCopyMemory(pLotHeader->Lock.LockHostMacAddress, PtrVCB->HostMac, 6);
			memcpy(pLotHeader->Lock.LockHostMacAddress, pVCB->HostMac, 32);
			
			
				
			memset(pFileInfo, 0, (XIDISK_COMMON_LOT_HEADER_SIZE - XIXCORE_MD5DIGEST_AND_SEQSIZE)); 

			pFileInfo->Access_time = xixcore_GetCurrentTime64();
			pFileInfo->Change_time = xixcore_GetCurrentTime64();
			pFileInfo->Create_time = xixcore_GetCurrentTime64();
			pFileInfo->Modified_time = xixcore_GetCurrentTime64();
			memcpy(pFileInfo->OwnHostId , pVCB->HostId, 16);
			pFileInfo->ParentDirLotIndex = pDir->LotNumber;
			pFileInfo->LotIndex = BeginingLotIndex;
			pFileInfo->State = XIFS_FD_STATE_CREATE;
			pFileInfo->ACLState = 0;
			pFileInfo->FileAttribute = (FileAttribute|FILE_ATTRIBUTE_ARCHIVE);
			pFileInfo->AccessFlags = FILE_SHARE_READ;
			pFileInfo->FileSize = 0;
			pFileInfo->AllocationSize =	pVCB->LotSize - XIDISK_FILE_HEADER_SIZE;
			pFileInfo->AddressMapIndex = 0;
			pFileInfo->LinkCount = 1;
			pFileInfo->NameSize = NameLength;
			pFileInfo->AddressMapIndex = 0;
			XIXCORE_SET_FLAGS(pFileInfo->Type, XIFS_FD_TYPE_FILE);

			memcpy(pFileInfo->Name, Name, NameLength);
			pFileInfo->AddressMapIndex = 0;

			
		}else{
			
			pDirInfo = (PXIDISK_DIR_INFO)xixcore_GetDataBufferWithOffset(xbuf);
			pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(LotHeader);
		
			xixcore_InitializeCommonLotHeader(	
				pLotHeader,
				pVCB->VolumeLotSignature,
				LOT_INFO_TYPE_DIRECTORY,
				LOT_FLAG_BEGIN,
				BeginingLotIndex,
				BeginingLotIndex,
				0,
				0,
				0,
				sizeof(XIDISK_FILE_HEADER_LOT),
				pVCB->LotSize - sizeof(XIDISK_DIR_HEADER_LOT)
				);

			pLotHeader->Lock.LockState=  XIDISK_LOCK_RELEASED;
			pLotHeader->Lock.LockAcquireTime= 0;;
			memcpy(pLotHeader->Lock.LockHostSignature, pVCB->HostId , 16);
			// Changed by ILGU HONG
			//	chesung suggest
			//RtlCopyMemory(pLotHeader->Lock.LockHostMacAddress, PtrVCB->HostMac, 6);
			memcpy(pLotHeader->Lock.LockHostMacAddress, pVCB->HostMac, 32);

			

			memset(pDirInfo, 0, (XIDISK_COMMON_LOT_HEADER_SIZE - XIXCORE_MD5DIGEST_AND_SEQSIZE)); 
			pDirInfo->Access_time = xixcore_GetCurrentTime64();
			pDirInfo->Change_time = xixcore_GetCurrentTime64();
			pDirInfo->Create_time = xixcore_GetCurrentTime64();
			pDirInfo->Modified_time = xixcore_GetCurrentTime64();
			
			memcpy(pDirInfo->OwnHostId ,pVCB->HostId, 16);
			pDirInfo->ParentDirLotIndex = pDir->LotNumber;
			pDirInfo->LotIndex = BeginingLotIndex;
			pDirInfo->State = XIFS_FD_STATE_CREATE;
			pDirInfo->ACLState = 0;
			pDirInfo->FileAttribute = (FileAttribute | FILE_ATTRIBUTE_DIRECTORY);
			pDirInfo->AccessFlags = FILE_SHARE_READ;
			pDirInfo->FileSize = 0;
			pDirInfo->AllocationSize = pVCB->LotSize - XIDISK_DIR_HEADER_SIZE;
			pDirInfo->LinkCount = 1;
			XIXCORE_SET_FLAGS(pDirInfo->Type, XIFS_FD_TYPE_DIRECTORY);
			pDirInfo->NameSize = NameLength;

		
			memcpy(pDirInfo->Name, Name, NameLength);	
			pDirInfo->AddressMapIndex = 0;

		
		}


		RC = xixcore_RawWriteFileHeader(
						pVCB->XixcoreBlockDevice,
						pVCB->LotSize,
						pVCB->SectorSize,
						pVCB->SectorSizeBit,
						BeginingLotIndex,
						xbuf,
						&Reason
						);

		if(RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x) Reason(0x%x) xixcore_CreateNewFile : xixcore_RawWriteFileHeader.\n", 
					RC, Reason));	


			goto error_out;
		}


		RC = xixcore_RawWriteLotHeader(
						pVCB->XixcoreBlockDevice,
						pVCB->LotSize,
						pVCB->SectorSize,
						pVCB->SectorSizeBit,
						BeginingLotIndex,
						LotHeader,
						&Reason
						);

		if(RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x) Reason(0x%x) xixcore_CreateNewFile : xixcore_RawWriteLotAndFileHeader.\n", 
					RC, Reason));	


			goto error_out;
		}

		xixcore_ZeroBufferOffset(xbuf);
		memset(xixcore_GetDataBuffer(xbuf), 0, XIDISK_DUP_FILE_INFO_SIZE);
		pAddressMap = (PXIDISK_ADDR_MAP)xixcore_GetDataBufferWithOffset(xbuf);
		pAddressMap->LotNumber[0] = BeginingLotIndex;
			
		RC = xixcore_RawWriteAddressOfFile(
						pVCB->XixcoreBlockDevice,
						pVCB->LotSize,
						pVCB->SectorSize,
						pVCB->SectorSizeBit,
						BeginingLotIndex, 
						0, 
						pDir->AddrLotSize,
						&AddrStartSectorIndex, 
						0,
						xbuf,
						&Reason);
		

		if(RC < 0 ){
			
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR(0x%x) Reason(0x%x) xixcore_CreateNewFile : xixcore_RawWriteAddressOfFile.\n", 
					RC, Reason));	


			goto error_out;
		}


		if(ChildType == XIFS_FD_TYPE_DIRECTORY)
		{
			PXIDISK_HASH_VALUE_TABLE pTable = NULL;
			
			xixcore_ZeroBufferOffset(xbuf);
			memset(xixcore_GetDataBuffer(xbuf), 0, XIDISK_DUP_FILE_INFO_SIZE);

			RC = xixcore_RawReadDirEntryHashValueTable(
							pVCB->XixcoreBlockDevice,
							pVCB->LotSize,
							pVCB->SectorSize,
							pVCB->SectorSizeBit,
							BeginingLotIndex, 
							xbuf,
							&Reason
							);

			if(RC < 0 ){
				
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR(0x%x) Reason(0x%x) xixcore_CreateNewFile : xixcore_RawWriteDirEntryHashValueTable.\n", 
						RC, Reason));	


				if(RC == XCCODE_CRCERROR){
					xixcore_ZeroBufferOffset(xbuf);
				}else{
					goto error_out;
				}
			}

			pTable = (PXIDISK_HASH_VALUE_TABLE)xixcore_GetDataBufferWithOffset(xbuf);
			memset(pTable, 0, (XIDISK_HASH_VALUE_TABLE_SIZE - XIXCORE_MD5DIGEST_AND_SEQSIZE));

			RC = xixcore_RawWriteDirEntryHashValueTable(
							pVCB->XixcoreBlockDevice,
							pVCB->LotSize,
							pVCB->SectorSize,
							pVCB->SectorSizeBit,
							BeginingLotIndex, 
							xbuf,
							&Reason
							);

			if(RC < 0 ){
				
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("ERROR(0x%x) Reason(0x%x) xixcore_CreateNewFile : xixcore_RawWriteDirEntryHashValueTable.\n", 
						RC, Reason));	


				goto error_out;
			}
		}



		*NewFileId = BeginingLotIndex;

		//xixcore_NotifyChange(pVCB, XIXCORE_META_FLAGS_UPDATE);

		RC = XCCODE_SUCCESS;
		

error_out:
		
		if( RC < 0 ) {
			
			if(IsDirSet == 1){

				xixcore_DeleteChildFromDir(
							pVCB, 
							pDir, 
							ChildIndex,
							DirContext
							);




			}

			if(bLotAlloc == 1) { 
				xixcore_FreeVCBLot(pVCB, BeginingLotIndex);
			}
		}


		if(xbuf) {
			xixcore_FreeBuffer(xbuf);
		}

		if(LotHeader){
			xixcore_FreeBuffer(LotHeader);
		}
	

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_DIRINFO|DEBUG_TARGET_FCB|DEBUG_TARGET_FILEINFO),
		("Exit (0x%x)  xixcore_CreateNewFile \n", 
			RC));
	
	return RC;
}



