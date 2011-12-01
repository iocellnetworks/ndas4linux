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
#ifndef __XIXCORE_FILEADDR_H__
#define __XIXCORE_FILEADDR_H__

#include "xixcore/xctypes.h"
#include "xixcore/file.h"
#include "xixcore/bitmap.h"
#include "xixcore/lotinfo.h"

int
xixcore_call
xixcore_GetAddressInfoForOffset(
	PXIXCORE_VCB			pVCB,
	PXIXCORE_FCB			pFCB,
	xc_uint64				Offset,
	PXIXCORE_BUFFER			 Addr,
	xc_uint32				AddrSize,
	xc_uint32				* AddrStartSecIndex,
	xc_int32				* Reason,
	PXIXCORE_IO_LOT_INFO	pAddress,
	xc_uint32					CanWait
);

int
xixcore_call
xixcore_AddNewLot(
	PXIXCORE_VCB 	pVCB, 
	PXIXCORE_FCB 	pFCB, 
	xc_uint32		RequestStatIndex, 
	xc_uint32		LotCount,
	xc_uint32		*AddedLotCount,
	PXIXCORE_BUFFER 	Addr,
	xc_uint32		AddrSize,
	xc_uint32		* AddrStartSecIndex
);


int
xixcore_call
xixcore_DeleteFileLotAddress(
	IN PXIXCORE_VCB pVCB,
	IN PXIXCORE_FCB pFCB
);

#endif //#ifndef __XIXCORE_FILEADDR_H__

