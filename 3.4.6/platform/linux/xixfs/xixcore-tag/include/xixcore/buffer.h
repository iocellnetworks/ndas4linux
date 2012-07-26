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
#ifndef __XIXCORE_BUFFER_H__
#define __XIXCORE_BUFFER_H__

#include "xixcore/xctypes.h"

/*
 * Xixcore buffer
 */


#define XIXCORE_BUFF_ALLOC_F_POOL		(0x00000000)
#define XIXCORE_BUFF_ALLOC_F_MEMORY	(0x00000001)

typedef struct _XIXCORE_BUFFER {
	xc_uint8				*xcb_data;
	xc_size_t		xcb_size;
	xc_size_t		xcb_offset;
	xc_uint32		xcb_flags;
} XIXCORE_BUFFER, *PXIXCORE_BUFFER;

/*
 * Initializw Xixcore biffer
 */

XIXCORE_INLINE
void xixcore_InitializeBuffer(
	PXIXCORE_BUFFER XixcoreBuffer,
	void *memory,
	xc_size_t size,
	xc_size_t offset
	)
{
	XixcoreBuffer->xcb_data = memory;
	XixcoreBuffer->xcb_size = size;
	XixcoreBuffer->xcb_offset = offset;
}


/*
 * Locate the buffer's current head in the Xixcore buffer.
 */

XIXCORE_INLINE
void * xixcore_GetDataBuffer(
	PXIXCORE_BUFFER XixcoreBuffer
	)
{
	return XixcoreBuffer->xcb_data;	
}

/*
 * Locate the buffer's current head plus offset in the Xixcore buffer.
 */

XIXCORE_INLINE
void * xixcore_GetDataBufferWithOffset(
	PXIXCORE_BUFFER XixcoreBuffer
	)
{
	return (void *)((xc_uint8 *)XixcoreBuffer->xcb_data + XixcoreBuffer->xcb_offset);	
}


/*
 * Calculate the current buffer size in the Xixcore buffer.
 */

XIXCORE_INLINE
xc_size_t xixcore_GetBufferSize(
	PXIXCORE_BUFFER XixcoreBuffer
	)
{
	return (XixcoreBuffer->xcb_size);
}

/*
 * Calculate the current buffer size minus offset in the Xixcore buffer.
 */

XIXCORE_INLINE
xc_size_t xixcore_GetBufferSizeWithOffset(
	PXIXCORE_BUFFER XixcoreBuffer
	)
{
	if(XixcoreBuffer->xcb_size <= XixcoreBuffer->xcb_offset)
		return 0;

	return (XixcoreBuffer->xcb_size - XixcoreBuffer->xcb_offset);
}

/*
 * Fill the Xixcore buffer.
 */

XIXCORE_INLINE
void * xixcore_FillBuffer(
	PXIXCORE_BUFFER XixcoreBuffer,
	int fill
	)
{
	return memset(XixcoreBuffer->xcb_data, fill, XixcoreBuffer->xcb_size);
}

/*
 * Reset the offset in the Xixcore buffer.
 */

XIXCORE_INLINE
void xixcore_ZeroBufferOffset(
	PXIXCORE_BUFFER XixcoreBuffer
) {
	XixcoreBuffer->xcb_offset = 0;
}

/*
 * Increase the offset in the Xixcore buffer.
 */

XIXCORE_INLINE
int xixcore_IncreaseBufferOffset(
	PXIXCORE_BUFFER XixcoreBuffer,
	xc_uint32			offset_inc
) {
	if(XixcoreBuffer->xcb_size <= XixcoreBuffer->xcb_offset) {
		return -1;
	}

	XixcoreBuffer->xcb_offset += offset_inc;
	return 0;
}

/*
 * Decrease the offset in the Xixcore buffer.
 */

XIXCORE_INLINE
int xixcore_DecreaseBufferOffset(
	PXIXCORE_BUFFER XixcoreBuffer,
	xc_uint32			offset_dec
) {
	if(0 >= XixcoreBuffer->xcb_offset)
		return -1;

	XixcoreBuffer->xcb_offset -= offset_dec;
	return 0;
}


#endif //  __XIXCORE_BUFFER_H__
