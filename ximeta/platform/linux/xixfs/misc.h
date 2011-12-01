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
#ifndef __XIXFS_MISC_H__
#define __XIXFS_MISC_H__

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <xixfsevent/xixfs_event.h>
#include "xixfs_types.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"
#include "xixfs_event_op.h"


int
xixfs_IHaveLotLock(
	uint8	* HostMac,
	uint64	 LotNumber,
	uint8	* VolumeId
);

int
xixfs_SendRenameLinkBC(
	uint32		SubCommand,
	uint8		* HostMac,
	uint64		LotNumber,
	uint8		* VolumeId,
	uint64		OldParentLotNumber,
	uint64		NewParentLotNumber
);

int
xixfs_SendFileChangeRC(
	uint8		* HostMac,
	uint64		LotNumber,
	uint8		* VolumeId,
	uint64		FileLength,
	uint64		AllocationLength,
	uint64		UpdateStartOffset
);

int
xixfs_CheckLockRequest(
	PXIXFS_SERVICE_CTX	serviceCtx,
	PXIXFSDG_PKT		Pkt
);


int
xixfs_CheckLockRequest(
	PXIXFS_SERVICE_CTX	serviceCtx,
	PXIXFSDG_PKT		Pkt
);

#if LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE
void
xixfs_ProcessLockRequest(struct work_struct *work);

void
xixfs_ProcessLockBroadCast(struct work_struct *work);

void
xixfs_ProcessFlushBroadCast(struct work_struct *work);

void
xixfs_ProcessFileLenChange(struct work_struct *work);

void
xixfs_ProcessFileChange(struct work_struct *work);

void
xixfs_ProcessDirChange(struct work_struct *work);
#else
void
xixfs_ProcessLockRequest(void * data);

void
xixfs_ProcessLockBroadCast(void * data);

void
xixfs_ProcessFlushBroadCast(void * data);

void
xixfs_ProcessFileLenChange(void * data);

void
xixfs_ProcessFileChange(void * data);

void
xixfs_ProcessDirChange(void * data);
#endif //LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE

void 
xixfs_init_wait_ctx(
	PXIXFS_WAIT_CTX wait
);


void 
xixfs_add_resource_wait_list(
	PXIXFS_WAIT_CTX wait,
	PXIXFS_LINUX_META_CTX ctx
);

void 
xixfs_remove_resource_wait_list(
	PXIXFS_WAIT_CTX wait,
	PXIXFS_LINUX_META_CTX ctx
);

void 
xixfs_wakeup_resource_waiter(
		PXIXFS_LINUX_META_CTX ctx
);


#if !LINUX_VERSION_25_ABOVE

void 
xixfs_metaevent_timeouthandler(unsigned long data);

void 
xixfs_clievent_timeouthandler(unsigned long data);

void 
xixfs_add_metaThread_stop_wait_list(
	PXIXFS_WAIT_CTX wait,
	PXIXFS_LINUX_META_CTX ctx
);

void 
xixfs_wakeup_metaThread_stop_waiter(
		PXIXFS_LINUX_META_CTX ctx
);
#endif

	
#endif //#ifndef __XIXFS_MISC_H__
