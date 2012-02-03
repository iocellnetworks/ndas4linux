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
#ifndef __XIXCORE_LOT_LOCK_H__
#define __XIXCORE_LOT_LOCK_H__

#include "xixcore/xctypes.h"
#include "xixcore/volume.h"

typedef struct _XIXCORE_AUXI_LOT_LOCK_INFO{
	XIXCORE_LISTENTRY	AuxLink;
	XIXCORE_ATOMIC	RefCount;
	PXIXCORE_VCB	pVCB;
	xc_uint8		VolumeId[16];
	xc_uint8		HostMac[32];
	xc_uint8		HostId[16];
	xc_uint64		LotNumber;
	xc_uint32		HasLock;
} XIXCORE_AUXI_LOT_LOCK_INFO, *PXIXCORE_AUXI_LOT_LOCK_INFO;


int
xixcore_call
xixcore_LotLock(
	PXIXCORE_VCB	pVCB,
	xc_uint64		LotNumber,
	xc_uint32		*status,
	xc_uint32		bCheck,
	xc_uint32		bRetry
	);

int 
xixcore_call
xixcore_LotUnLock(
	PXIXCORE_VCB	pVCB,
	xc_uint64		LotNumber,
	xc_uint32		*status
);

typedef
int
(*PXIXCORE_LOCKEDFUNCTION) (
	xc_uint8		*arg
    );

int 
xixcore_call
xixcore_LockedOp(
	PXIXCORE_VCB	pVCB,
	xc_uint64		LotNumber,
	xc_uint32		*status,
	xc_uint32		bCheck,
	PXIXCORE_LOCKEDFUNCTION func,
	xc_uint8		*arg
	);

int
xixcore_call
xixcore_AuxLotLock(
	PXIXCORE_VCB		pVCB,
	xc_uint64			LotNumber,
	xc_uint32			bCheck,
	xc_uint32			bRetry
);

int
xixcore_call
xixcore_AuxLotUnLock(
	PXIXCORE_VCB		pVCB,
	xc_uint64			LotNumber
);

void
xixcore_call
xixcore_CleanUpAuxLotLockInfo(
		PXIXCORE_VCB		pVCB
);

void
xixcore_call
xixcore_CleanUpAllAuxLotLockInfo(
	void
);

void
xixcore_call
xixcore_RefAuxLotLock(
	PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo
);

void
xixcore_call
xixcore_DeRefAuxLotLock(
	PXIXCORE_AUXI_LOT_LOCK_INFO	AuxLotInfo
);

#endif //#ifndef __XIXCORE_LOT_LOCK_H__
