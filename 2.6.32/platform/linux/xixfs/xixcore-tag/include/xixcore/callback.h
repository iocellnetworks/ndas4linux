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
#ifndef __XIXCORE_CALLBACK_H__
#define __XIXCORE_CALLBACK_H__


#include "xixcore/xctypes.h"
#include "xixcore/buffer.h"
#include "xixcore/volume.h"

/*
 * System supplied callback functions
 * Should be implemented in platform-dependent part of Xixcore.
 */

/*
 * Allocate a memory block for default upcase table.
 */
xc_le16 *
xixcore_call
xixcore_AllocateUpcaseTable(void);

/*
 * Free a memory block for default upcase table.
 */
void
xixcore_call
xixcore_FreeUpcaseTable(
	xc_le16 *UpcaseTable
	);

/*
 * Allocate an Xixcore buffer.
 */

PXIXCORE_BUFFER
xixcore_call
xixcore_AllocateBuffer(xc_uint32 Size);

/*
 * Free the Xixcore buffer.
 */

void
xixcore_call
xixcore_FreeBuffer(PXIXCORE_BUFFER XixcoreBuffer);

/*
 * Acquire the shared device lock
 */
int 
xixcore_call
xixcore_AcquireDeviceLock(
	PXIXCORE_BLOCK_DEVICE	XixcoreBlockDevice
	);

/*
 * Release the shared device lock
 */
int 
xixcore_call
xixcore_ReleaseDevice(
	PXIXCORE_BLOCK_DEVICE	XixcoreBlockDevice
	);

/*
 * Ask a host if it has the lot lock.
 */

int
xixcore_call
xixcore_HaveLotLock(
	xc_uint8		* HostMac,
	xc_uint8		* LockOwnerMac,
	xc_uint64		LotNumber,
	xc_uint8		* VolumeId,
	xc_uint8		* LockOwnerId
	);

/*
 * perform raw IO to a block device
 */

int
xixcore_call
xixcore_DeviceIoSync(
	PXIXCORE_BLOCK_DEVICE XixcoreBlkDev,
	xc_sector_t startsector,
	xc_int32 size, 
	xc_uint32 sectorsize,
	xc_uint32 sectorsizeBit,
	PXIXCORE_BUFFER XixcoreBuffer,
	xc_int32 rw,
	xc_int32 * Reason
	);

/*
 * Notify file changes
 */

void
xixcore_call
xixcore_NotifyChange(
	PXIXCORE_VCB XixcoreVcb,
	xc_uint32 VCBMetaFlags
	);

/*
 * Wait for meta resource event
 */

int
xixcore_call
xixcore_WaitForMetaResource(
	PXIXCORE_VCB	XixcoreVcb
	);

#endif //#ifndef __XIXCORE_CALLBACK_H__

