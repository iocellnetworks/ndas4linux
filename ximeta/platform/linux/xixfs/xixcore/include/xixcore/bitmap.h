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
#ifndef __XIXCORE_BITMAP_H__
#define __XIXCORE_BITMAP_H__

#include "xixcore/xctypes.h"
#include "xixcore/volume.h"
#include "xixcore/buffer.h"

typedef struct _XIXCORE_BITMAP_EMUL_CONTEXT{
	PXIXCORE_VCB		VCB;
	xc_uint32  			BitMapBytes;
	xc_uint64			UsedBitmapIndex;
	xc_uint64			UnusedBitmapIndex;
	xc_uint64			CheckOutBitmapIndex;
	PXIXCORE_BUFFER LotHeader;
	PXIXCORE_BUFFER	BitmapLotHeader;
	XIXCORE_LOT_MAP	UsedBitmap;
	XIXCORE_LOT_MAP	UnusedBitmap;
	XIXCORE_LOT_MAP	CheckOutBitmap;
} XIXCORE_BITMAP_EMUL_CONTEXT, *PXIXCORE_BITMAP_EMUL_CONTEXT;

/********************************************************************

	Bitmap Operation

*********************************************************************/
XIXCORE_INLINE
xc_uint32 xixcore_TestBit(xc_uint64 nr, volatile void * addr)
{
    return ((unsigned char *) addr)[nr >> 3] & (1U << (nr & 7));
}	

XIXCORE_INLINE
void xixcore_SetBit(xc_uint64 nr, volatile void *addr)
{
	((unsigned char *) addr)[nr >> 3] |= (1U << (nr & 7));
}

XIXCORE_INLINE
void xixcore_ClearBit(xc_uint64 nr, volatile void *addr)
{
	((unsigned char *) addr)[nr >> 3] &= ~(1U << (nr & 7));
}

XIXCORE_INLINE
void xixcore_ToggleBit(xc_uint64 nr, volatile void *addr)
{
	((unsigned char *) addr)[nr >> 3] ^= (1U << (nr & 7));
}

XIXCORE_INLINE
xc_uint32 xixcore_TestAndSetBit(xc_uint64 nr, volatile void *addr)
{
	unsigned int mask = 1 << (nr & 7);
	unsigned int oldval;

	oldval = ((unsigned char *) addr)[nr >> 3];
	((unsigned char *) addr)[nr >> 3] = oldval | mask;
	return oldval & mask;
}

XIXCORE_INLINE
xc_uint32 xixcore_TestAndClearBit(xc_uint64 nr, volatile void *addr)
{
	unsigned int mask = 1 << (nr & 7);
	unsigned int oldval;

	oldval = ((unsigned char *) addr)[nr >> 3];
	((unsigned char *) addr)[nr >> 3] = oldval & ~mask;
	return oldval & mask;
}

XIXCORE_INLINE
int xixcore_TestAndToggleBit(xc_uint64 nr, volatile void *addr)
{
	unsigned int mask = 1 << (nr & 7);
	unsigned int oldval;

	oldval = ((unsigned char *) addr)[nr >> 3];
	((unsigned char *) addr)[nr >> 3] = oldval ^ mask;
	return oldval & mask;
}


void *
xixcore_call
xixcore_GetDataBufferOfBitMap(
	IN PXIXCORE_BUFFER Buffer
);


xc_int64 
xixcore_call
xixcore_FindSetBit(
	IN		xc_int64 bitmap_count, 
	IN		xc_int64 bitmap_hint, 
	IN OUT	volatile void * Mpa
);

xc_uint64 
xixcore_call
xixcore_FindFreeBit(
	IN		xc_uint64 bitmap_count, 
	IN		xc_uint64 bitmap_hint, 
	IN OUT	volatile void * Mpa
);

xc_uint64 
xixcore_call
xixcore_FindSetBitCount(
	IN		xc_uint64 bitmap_count, 
	IN OUT	volatile void *LotMapData
);

xc_uint64
xixcore_call
xixcore_AllocLotMapFromFreeLotMap(
	IN	xc_uint64 bitmap_count, 
	IN	xc_uint64 request,  
	IN	volatile void * FreeLotMapData, 
	IN	volatile void * CheckOutLotMapData,
	IN	xc_uint64 * 	AllocatedCount
);

int
xixcore_call
xixcore_ReadBitMapWithBuffer(
	PXIXCORE_VCB VCB,
	xc_sector_t BitMapLotNumber,
	PXIXCORE_LOT_MAP	Bitmap,	
	PXIXCORE_BUFFER 		LotHeader,
	PXIXCORE_BUFFER 		BitmapHeader_xbuf 
);

int
xixcore_call
xixcore_WriteBitMapWithBuffer(
	PXIXCORE_VCB VCB,
	xc_sector_t BitMapLotNumber,
	PXIXCORE_LOT_MAP	Bitmap,	
	PXIXCORE_BUFFER 		LotHeader,
	PXIXCORE_BUFFER 		BitmapHeader_xbuf,
	xc_uint32		Initialize
);

int
xixcore_call
xixcore_InvalidateLotBitMapWithBuffer(
	PXIXCORE_VCB VCB,
	xc_sector_t BitMapLotNumber,
	PXIXCORE_BUFFER LotHeader
);

int
xixcore_call
xixcore_ReadAndAllocBitMap(	
	PXIXCORE_VCB VCB,
	xc_sector_t LotMapIndex,
	PXIXCORE_LOT_MAP *ppLotMap
);

int
xixcore_call
xixcore_WriteBitMap(
	PXIXCORE_VCB VCB,
	xc_sector_t LotMapIndex,
	PXIXCORE_LOT_MAP pLotMap,
	xc_uint32		Initialize
);

int
xixcore_call
xixcore_SetCheckOutLotMap(
	PXIXCORE_LOT_MAP	FreeLotMap,
	PXIXCORE_LOT_MAP	CheckOutLotMap,
	xc_int32 			HostCount
);

int
xixcore_call
xixcore_InitializeBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx,
	PXIXCORE_VCB	pVCB,
	xc_uint64 UsedBitmapIndex,
	xc_uint64 UnusedBitmapIndex,
	xc_uint64 CheckOutBitmapIndex
);

int
xixcore_call
xixcore_CleanupBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

int
xixcore_call
xixcore_ReadDirmapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

int
xixcore_call
xixcore_WriteDirmapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

int
xixcore_call
xixcore_ReadFreemapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

int
xixcore_call
xixcore_WriteFreemapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

int
xixcore_call
xixcore_ReadCheckoutmapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

int
xixcore_call
xixcore_WriteCheckoutmapFromBitmapContext(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

int
xixcore_call
xixcore_InvalidateDirtyBitMap(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

int
xixcore_call
xixcore_InvalidateFreeBitMap(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

int
xixcore_call
xixcore_InvalidateCheckOutBitMap(
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

void
xixcore_call
xixcore_ORMap(
	PXIXCORE_LOT_MAP		pDestMap,
	PXIXCORE_LOT_MAP		pSourceMap
	);

void
xixcore_call
xixcore_XORMap(
	PXIXCORE_LOT_MAP		pDestMap,
	PXIXCORE_LOT_MAP		pSourceMap
);

/*
 * Allocate a VCB lot
 */

xc_int64
xixcore_call
xixcore_AllocVCBLot(
	PXIXCORE_VCB	XixcoreVCB
);

/*
 * Free a VCB lot
 */

void
xixcore_call
xixcore_FreeVCBLot(
	PXIXCORE_VCB XixcoreVCB,
	xc_uint64 LotIndex
);


#endif //#ifndef __XIXCORE_BITMAP_H__
