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
#include "linux_ver.h"
#if LINUX_VERSION_25_ABOVE
#include <linux/fs.h> // blkdev_ioctl , register file system
#include <linux/buffer_head.h> // file_fsync
#include <linux/genhd.h>  // rescan_parition
#include <linux/workqueue.h>  // 
#include <linux/jiffies.h>
#else
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/wait.h>
#include <linux/completion.h>

#include <linux/delay.h>
#include <asm/uaccess.h>
#include <asm/div64.h>


#include "xcsystem/debug.h"
#include "xcsystem/system.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/buffer.h"
#include "xixcore/ondisk.h"
#include "xixcore/lotlock.h"
#include "xixcore/lotinfo.h"
#include "xixcore/lotaddr.h"
#include "xixcore/bitmap.h"

#include "xixfs_global.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"
#include "xixfs_name_cov.h"
#include "misc.h"

/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSBITMAP"

int
xixfs_GetMoreCheckOutLotMap(
	PXIXFS_LINUX_META_CTX		pCtx 
)
{
	int					RC = 0;
	PXIXFS_LINUX_VCB					pVCB = NULL;
	PXIXCORE_BITMAP_EMUL_CONTEXT	pDiskBitmapEmulCtx= NULL;
	PXIXCORE_LOT_MAP 				pTempFreeLotMap ;
	uint32						size = 0;
	uint32						BuffSize = 0;
	uint32						trycount = 0;
	PXIXCORE_META_CTX				xixcoreCtx = NULL;
	

	XIXCORE_ASSERT(pCtx);
	pVCB = pCtx->VCBCtx;
	XIXFS_ASSERT_VCB(pVCB);
	xixcoreCtx = &pVCB->XixcoreVcb.MetaContext;


	pDiskBitmapEmulCtx = kmalloc(sizeof(XIXCORE_BITMAP_EMUL_CONTEXT), GFP_KERNEL);

	if(!pDiskBitmapEmulCtx) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixfs_GetMoreCheckOutLotMap --> can't alloc bitmap emul ctx .\n"));
		return -1;		
	}
	memset(pDiskBitmapEmulCtx, 0, sizeof(XIXCORE_BITMAP_EMUL_CONTEXT));
	

	pTempFreeLotMap = kmalloc(sizeof(XIXCORE_LOT_MAP), GFP_KERNEL);

	if(!pTempFreeLotMap) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixfs_GetMoreCheckOutLotMap --> can't alloc pTempFreeLotMap .\n"));

		kfree(pDiskBitmapEmulCtx);
		return -1;		
	}

	memset(pDiskBitmapEmulCtx, 0, sizeof(XIXCORE_LOT_MAP));


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_GetMoreCheckOutLotMap.\n"));


retry:	
	RC = xixcore_LotLock(
		&pVCB->XixcoreVcb,
		xixcoreCtx->HostRegLotMapIndex,
		&xixcoreCtx->HostRegLotMapLockStatus,
		1,
		1
		);
		
	
	if( RC <0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(0x%x)xixfs_GetMoreCheckOutLotMap --> xixfs_LotLock .\n", RC));
		if(trycount > 3) {
			kfree(pDiskBitmapEmulCtx);
			kfree(pTempFreeLotMap);
			return RC;
		}
		trycount++;
		goto retry;
	}

	printk(KERN_DEBUG "Get HOT LOT LOCK \n");
	
	

	// Zero Bit map context;
	memset(pDiskBitmapEmulCtx, 0, sizeof(XIXCORE_BITMAP_EMUL_CONTEXT));


	// Read Disk Bitmap information
	RC = xixcore_InitializeBitmapContext(
							pDiskBitmapEmulCtx,
							&pVCB->XixcoreVcb,
							xixcoreCtx->HostCheckOutLotMapIndex,
							xixcoreCtx->FreeLotMapIndex,
							xixcoreCtx->CheckOutLotMapIndex
							);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixfs_GetMoreCheckOutLotMap --> xixfs_InitializeBitmapContext (0x%x) .\n", RC));

		goto error_out;
	}



	memset(pTempFreeLotMap, 0, sizeof(XIXCORE_LOT_MAP));

	
	size = SECTOR_ALIGNED_SIZE(pVCB->XixcoreVcb.SectorSizeBit, (uint32) ((pVCB->XixcoreVcb.NumLots + 7)/8 + sizeof(XIDISK_BITMAP_DATA) -1));
	BuffSize = size*2;

	pTempFreeLotMap->Data = xixcore_AllocateBuffer(BuffSize);

	if(!pTempFreeLotMap->Data){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixfs_GetMoreCheckOutLotMap -->Allocate TempFreeLotMap .\n"));
		RC =-ENOMEM;
		goto error_out;
	}

	memset(xixcore_GetDataBuffer(pTempFreeLotMap->Data),0, BuffSize);

	// Update Disk Free bitmap , dirty map  and Checkout Bitmap from free Bitmap
	
	RC = xixcore_ReadDirmapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixfs_GetMoreCheckOutLotMap --> xixfs_ReadDirmapFromBitmapContext (0x%x) .\n", RC));
		goto error_out;
	}

	/*	
	{
		uint32 i = 0;
		uint8 	*Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UsedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Host CheckOut BitMap \n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/



	RC = xixcore_ReadFreemapFromBitmapContext(pDiskBitmapEmulCtx);
	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixfs_GetMoreCheckOutLotMap --> xixfs_ReadFreemapFromBitmapContext(0x%x) .\n", RC));
		goto error_out;
	}

	/*
	{
		uint32 i = 0;
		uint8 *	Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UnusedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Disk Free Bitmap\n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/


	RC = xixcore_ReadCheckoutmapFromBitmapContext(pDiskBitmapEmulCtx);

	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixfs_GetMoreCheckOutLotMap -->  xixfs_ReadCheckoutmapFromBitmapContext(0x%x) .\n", RC));
		goto error_out;
	}

	/*
	{
		uint32 i = 0;
		uint8 *	Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->CheckOutBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Disk CheckOut Bitmap\n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}	
	*/



	// Get Real FreeMap without CheckOut
	xixcore_XORMap(&(pDiskBitmapEmulCtx->UnusedBitmap), &(pDiskBitmapEmulCtx->CheckOutBitmap));
		
	/*
	{
		uint32 i = 0;
		uint8 *	Data;
		Data = xixcore_GetDataBuffer(pDiskBitmapEmulCtx->UnusedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Disk Real free Bitmap\n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/

	RC = xixcore_SetCheckOutLotMap((PXIXCORE_LOT_MAP)&pDiskBitmapEmulCtx->UnusedBitmap, pTempFreeLotMap, xixcoreCtx->HostRecordIndex + 1);

	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail  SetCheckOutLotMap Status (0x%x) .\n", RC));
		goto error_out;
	}
	

	/*
	{
		uint32 i = 0;
		uint8 	*Data;
		Data = xixcore_GetDataBufferOfBitMap(pTempFreeLotMap->Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Allocated Bit Map\n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/




	//Update Host CheckOutLotMap
	xixcore_ORMap(&(pDiskBitmapEmulCtx->UsedBitmap), pTempFreeLotMap);

	/*
	{
		uint32 i = 0;
		uint8 	*Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UsedBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Allocate Host Checkout Bitmap\n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}



	{
		uint32 i = 0;
		uint8 *	Data;
		Data = xixcore_GetDataBufferOfBitMap(xixcoreCtx->HostFreeLotMap->Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("Before Allocate Host free Bitmap\n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}
	*/

	//Update Host FreeLotMap
	xixcore_ORMap(xixcoreCtx->HostFreeLotMap, pTempFreeLotMap);

	/*
	{
		uint32 i = 0;
		uint8 	*Data;
		Data = xixcore_GetDataBufferOfBitMap(xixcoreCtx->HostFreeLotMap->Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Allocate Host free Bitmap\n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}		
	*/

	//Update Disk CheckOut BitMap
	xixcore_ORMap(&(pDiskBitmapEmulCtx->CheckOutBitmap), pTempFreeLotMap);	
	
	/*
	{
		uint32 i = 0;
		uint8 *	Data;
		Data = xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->CheckOutBitmap.Data);
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,("After Allocate Disk CheckOut Bitmap\n"));
		for(i = 0; i<2; i++){
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_RESOURCE,
				("<%i>[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]:[%02x]\n",
				i*8,Data[i*8],Data[i*8 +1],Data[i*8 +2],Data[i*8 +3],
				Data[i*8 +4],Data[i*8 +5],Data[i*8 +6],Data[i*8 +7]));	
		}
	}		
	*/

DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
		("STEP 0 pDiskBitmapEmulCtx->BitmapLotHeader(%p)\n", pDiskBitmapEmulCtx->BitmapLotHeader));
	

	//	Added by ILGU HONG	2006 06 12
	xixcoreCtx->VolumeFreeMap->BitMapBytes = pDiskBitmapEmulCtx->UnusedBitmap.BitMapBytes;
	xixcoreCtx->VolumeFreeMap->MapType = pDiskBitmapEmulCtx->UnusedBitmap.MapType;
	xixcoreCtx->VolumeFreeMap->NumLots = pDiskBitmapEmulCtx->UnusedBitmap.NumLots;

	/*
	memcpy(xixcore_GetDataBufferOfBitMap(pCtx->VolumeFreeMap->Data), 
				xixcore_GetDataBufferOfBitMap(pDiskBitmapEmulCtx->UnusedBitmap.Data), 
				pCtx->VolumeFreeMap->BitMapBytes
				);
	*/
	//	Added by ILGU HONG	2006 06 12 End
	

DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
		("STEP 1 pDiskBitmapEmulCtx->BitmapLotHeader(%p)\n", pDiskBitmapEmulCtx->BitmapLotHeader));


	// Update Disk Information

	RC = xixcore_WriteCheckoutmapFromBitmapContext(pDiskBitmapEmulCtx);

DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
		("STEP 2 pDiskBitmapEmulCtx->BitmapLotHeader(%p)\n", pDiskBitmapEmulCtx->BitmapLotHeader));



	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixfs_GetMoreCheckOutLotMap --> xixfs_WriteCheckoutmapFromBitmapContext(0x%x) .\n", RC));
		goto error_out;
	}



	// Update Host Record Information
	RC = xixcore_WriteDirmapFromBitmapContext(pDiskBitmapEmulCtx);

DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
		("STEP 3 pDiskBitmapEmulCtx->BitmapLotHeader(%p)\n", pDiskBitmapEmulCtx->BitmapLotHeader));




	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixfs_GetMoreCheckOutLotMap --> xixfs_WriteDirmapFromBitmapContext(0x%x) .\n", RC));
		goto error_out;
	}


DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
		("STEP 4 pDiskBitmapEmulCtx->BitmapLotHeader(%p)\n", pDiskBitmapEmulCtx->BitmapLotHeader));



	RC = xixcore_WriteBitMapWithBuffer(
			&pVCB->XixcoreVcb,
			xixcoreCtx->HostUsedLotMapIndex, 
			xixcoreCtx->HostDirtyLotMap,
			pDiskBitmapEmulCtx->LotHeader,
			pDiskBitmapEmulCtx->BitmapLotHeader, 
			0
			);

DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
		("STEP 5 pDiskBitmapEmulCtx->BitmapLotHeader(%p)\n", pDiskBitmapEmulCtx->BitmapLotHeader));

	
	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixfs_GetMoreCheckOutLotMap -->xixfs_WriteBitMapWithBuffer(0x%x) .\n", RC));
		goto error_out;
	}



	RC = xixcore_WriteBitMapWithBuffer(
			&pVCB->XixcoreVcb,
			xixcoreCtx->HostUnUsedLotMapIndex, 
			xixcoreCtx->HostFreeLotMap, 
			pDiskBitmapEmulCtx->LotHeader,
			pDiskBitmapEmulCtx->BitmapLotHeader, 
			0
			);

DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
		("STEP 6 pDiskBitmapEmulCtx->BitmapLotHeader(%p)\n", pDiskBitmapEmulCtx->BitmapLotHeader));




	if( RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("FAIL  xixfs_GetMoreCheckOutLotMap -->xixfs_WriteBitMapWithBuffer(0x%x) .\n", RC));
		goto error_out;
	}


error_out:

	
	xixcore_LotUnLock(
		&pVCB->XixcoreVcb,
		xixcoreCtx->HostRegLotMapIndex,
		&(xixcoreCtx->HostRegLotMapLockStatus)
		);	
	

	printk(KERN_DEBUG "Free HOT LOT LOCK \n");

	if(pDiskBitmapEmulCtx) {

		xixcore_CleanupBitmapContext(pDiskBitmapEmulCtx);
	
		kfree(pDiskBitmapEmulCtx);
	}




	if(pTempFreeLotMap){
		if(pTempFreeLotMap->Data){
			xixcore_FreeBuffer(pTempFreeLotMap->Data);
		}
	
		kfree(pTempFreeLotMap);
	}



	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Exit XifsdGetMoreCheckOutLotMap Status (0x%x).\n", RC));

	return RC;
}


int
xixfs_UpdateMetaData(
	PXIXFS_LINUX_META_CTX		pCtx 
)
{
	int 				RC = 0;
	PXIX_BUF		tmpBuf = NULL;
	PXIX_BUF		tmpLotHeader = NULL;
	PXIXFS_LINUX_VCB		pVCB = NULL;
	PXIXCORE_META_CTX		xixcoreCtx;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ),
		("Enter xixfs_UpdateMetaData .\n"));


	XIXCORE_ASSERT(pCtx);
	pVCB = pCtx->VCBCtx;
	XIXFS_ASSERT_VCB(pVCB);
	xixcoreCtx = &pVCB->XixcoreVcb.MetaContext;



	tmpBuf = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_LOT_MAP_INFO_SIZE);

	if(!tmpBuf) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail allocate xbuf : xixfs_UpdateMetaData Host Dirty map .\n"));
		RC = - ENOMEM;
		return RC;
	}

	
	memset(xixcore_GetDataBuffer(&tmpBuf->xixcore_buffer), 0, XIDISK_DUP_LOT_MAP_INFO_SIZE);



	tmpLotHeader = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if(!tmpLotHeader) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail allocate xbuf : xixfs_UpdateMetaData Host Dirty map .\n"));
		RC = - ENOMEM;
		goto error_out;
	}

	
	memset(xixcore_GetDataBuffer(&tmpLotHeader->xixcore_buffer), 0, XIDISK_DUP_COMMON_LOT_HEADER_SIZE);




	RC = xixcore_WriteBitMapWithBuffer(&pVCB->XixcoreVcb, 
							xixcoreCtx->HostUsedLotMapIndex, 
							xixcoreCtx->HostDirtyLotMap,
							&tmpLotHeader->xixcore_buffer,
							&tmpBuf->xixcore_buffer, 
							0);
	if( RC < 0 ){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail Status(0x%x) xixfs_UpdateMetaData--> xixfs_WriteBitMapWithBuffer Host Dirty map .\n", RC));

		goto error_out;
	}


	memset(xixcore_GetDataBuffer(&tmpBuf->xixcore_buffer), 0, XIDISK_MAP_LOT_SIZE);

	RC = xixcore_WriteBitMapWithBuffer(&pVCB->XixcoreVcb, 
							xixcoreCtx->HostUnUsedLotMapIndex, 
							xixcoreCtx->HostFreeLotMap,
							&tmpLotHeader->xixcore_buffer,
							&tmpBuf->xixcore_buffer, 
							0);
	if(RC < 0){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
			("Fail Status(0x%x) xixfs_UpdateMetaData--> xixfs_WriteBitMapWithBuffer   Host Free map.\n", RC));

		goto error_out;
	}

error_out:


	if( tmpBuf ) {
		xixcore_FreeBuffer( &tmpBuf->xixcore_buffer);
	}

	if(tmpLotHeader) {
		xixcore_FreeBuffer(&tmpLotHeader->xixcore_buffer);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit Status(0x%x) xixfs_UpdateMetaData .\n", RC));

	return RC;

}




int
xixfs_ResourceThreadFunction(
		void 	*lpParameter
)
{
	PXIXFS_LINUX_VCB		pVCB = NULL;
	PXIXFS_LINUX_META_CTX		pCtx = NULL;
	PXIXCORE_META_CTX		xixcoreCtx = NULL;
	int					RC =0;
#if LINUX_VERSION_25_ABOVE			
	int					TimeOut;
#endif
	unsigned long flags;
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Enter xixfs_ResourceThreadFunction .\n"));

#if defined(NDAS_ORG2423) || defined(NDAS_SIGPENDING_OLD)
	spin_lock_irqsave(&current->sigmask_lock, flags);
	siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGTERM));
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);
#else
	spin_lock_irqsave(&current->sighand->siglock, flags);
    	siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGTERM));
    	recalc_sigpending();
    	spin_unlock_irqrestore(&current->sighand->siglock, flags);
#endif

#if LINUX_VERSION_25_ABOVE	
	daemonize("XixMetaThread");
#else
	daemonize();
#endif

	pCtx = (PXIXFS_LINUX_META_CTX)lpParameter;

	XIXCORE_ASSERT(pCtx);
	
	pVCB = pCtx->VCBCtx;
	XIXFS_ASSERT_VCB(pVCB);
	xixcoreCtx = &pVCB->XixcoreVcb.MetaContext;

	while(1){

		 if(signal_pending(current)) {
        		flush_signals(current);
    		 }

#if LINUX_VERSION_25_ABOVE			
		TimeOut = DEFAULT_XIXFS_UPDATEWAIT;
		RC = wait_event_timeout(pCtx->VCBMetaEvent,  
								XIXCORE_TEST_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_MASK),
								TimeOut);
#else
		mod_timer(&(pCtx->VCBMetaTimeOut), jiffies+ 180*HZ);
		wait_event(pCtx->VCBMetaEvent,  
								XIXCORE_TEST_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_MASK));
#endif


		DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("!!!!! Wake up HELLOE ResourceThreadFunction .\n"));
	
		//printk(KERN_DEBUG "!!!!! Wake UP HELLOE ResourceThreadFunction .\n");
		
		spin_lock(&(pCtx->MetaLock));
		//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
		//			("spin_lock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx ));
#if LINUX_VERSION_25_ABOVE			
		if(RC == 0 ) {
#else
 		if(XIXCORE_TEST_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_TIMEOUT)) {
			XIXCORE_CLEAR_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_TIMEOUT);
#endif
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
				("Request Call timeout : xixfs_ResourceThreadFunction .\n"));	


			spin_unlock(&(pCtx->MetaLock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx ));
			if(XIXCORE_TEST_FLAGS(xixcoreCtx->ResourceFlag, XIXCORE_META_RESOURCE_NEED_UPDATE)){
				XIXCORE_CLEAR_FLAGS(xixcoreCtx->ResourceFlag, XIXCORE_META_RESOURCE_NEED_UPDATE);
				RC = xixfs_UpdateMetaData(pCtx);

				if( RC <0 ) {
					DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
						("fail(0x%x) xixfs_ResourceThreadFunction --> xixfs_UpdateMetaData .\n", RC));	
				}
			}
			
#if LINUX_VERSION_25_ABOVE	
			continue;
		}else if(XIXCORE_TEST_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_UPDATE)) {
#else
		}
		
 		if(XIXCORE_TEST_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_UPDATE)) {
#endif
			XIXCORE_CLEAR_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_UPDATE);
			
			spin_unlock(&(pCtx->MetaLock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx ));
			if(XIXCORE_TEST_FLAGS(xixcoreCtx->ResourceFlag, XIXCORE_META_RESOURCE_NEED_UPDATE)){
				XIXCORE_CLEAR_FLAGS(xixcoreCtx->ResourceFlag, XIXCORE_META_RESOURCE_NEED_UPDATE);
				RC = xixfs_UpdateMetaData(pCtx);

				if( RC <0 ) {
					DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
						("fail(0x%x) xixfs_ResourceThreadFunction --> xixfs_UpdateMetaData .\n", RC));	
				}
			}
			xixfs_wakeup_resource_waiter(pCtx);

			continue;
		}else if(XIXCORE_TEST_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_KILL_THREAD)) {
			
			
			XIXCORE_CLEAR_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_RECHECK_RESOURCES);
			XIXCORE_SET_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_INSUFFICIENT_RESOURCES);
			spin_unlock(&(pCtx->MetaLock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx ));

			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
				("Stop Thread : xixfs_ResourceThreadFunction .\n"));	
			
			xixfs_wakeup_resource_waiter(pCtx);
#if LINUX_VERSION_25_ABOVE	
			complete_all(&(pCtx->VCBMetaThreadStopCompletion));
#else
			del_timer(&(pCtx->VCBMetaTimeOut));
			xixfs_wakeup_metaThread_stop_waiter(pCtx);
#endif
			break;
		}else if( XIXCORE_TEST_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_RECHECK_RESOURCES)){

			spin_unlock(&(pCtx->MetaLock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx ));
			
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
				("get more resource  : xixfs_ResourceThreadFunction .\n"));	
			
			RC = xixfs_GetMoreCheckOutLotMap(pCtx);

			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
				("End xixfs_GetMoreCheckOutLotMap .\n"));	

			if( RC <0 ) {
				DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
					("fail(0x%x) xixfs_ResourceThreadFunction --> xixfs_GetMoreCheckOutLotMap .\n", RC));	
			}else {

				spin_lock(&(pCtx->MetaLock));
				//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
				//	("spin_lock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx ));
				
				XIXCORE_CLEAR_FLAGS(xixcoreCtx->VCBMetaFlags, XIXCORE_META_FLAGS_RECHECK_RESOURCES);
				spin_unlock(&(pCtx->MetaLock));
				//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
				//	("spin_unlock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx ));

				DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
					("WAKE UP WAITING THREAD!! .\n"));	
				
				xixfs_wakeup_resource_waiter(pCtx);
			}

			continue;
			
		}else {
			DebugTrace(DEBUG_LEVEL_ALL, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO |DEBUG_TARGET_ALL), 
				("Request Call Unrecognized : xixfs_ResourceThreadFunction .\n"));	


			spin_unlock(&(pCtx->MetaLock));
			//DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
			//		("spin_unlock(&(pCtx->MetaLock)) pCtx(%p)\n", pCtx ));
		}
		
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FSCTL|DEBUG_TARGET_VOLINFO ), 
		("Exit xixfs_ResourceThreadFunction .\n"));

	return 0;
}

