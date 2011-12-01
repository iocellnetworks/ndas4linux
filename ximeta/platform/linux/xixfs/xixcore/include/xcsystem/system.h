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
#ifndef __XIXCORE_SYSTEM_H__
#define __XIXCORE_SYSTEM_H__

#include "xcsystem/systypes.h"
#include "xixcore/xctypes.h"

/*
 * Block operation code
 */

#define XIXCORE_READ 0
#define XIXCORE_WRITE 1

/*
 * System supplied variables
 */

/*
	define debug level
*/

#if defined(__XIXCORE_DEBUG__)
extern long XixcoreDebugLevel;
extern long XixcoreDebugTarget;
#endif

#define XCCODE_MAX			37  // Must match in errinfo.h
extern XCCODE XixcoreErrorCodeLookup[XCCODE_MAX];

/*
 * System supplied functions
 * Should be implemented in platform-dependent part of Xixcore.
 */

/*
 * pause execution and print the message.
 */

void
xixcore_call
xixcore_Assert(void * Condition, char * Message);

/*
 * Print debug message
 */

void
xixcore_call
xixcore_DebugPrintf(const char * Format, ...);

/*
 * match debug masks and add a prefix to the debug message.
 */

int
xixcore_call
xixcore_DebugMaskAndPrefix(xc_uint32 Level, xc_uint32 Target);

/*
 * Memory attributes
 */

#define XCMEMATTR_PAGED	0x00000001

/*
 * Allocate a memory block.
 */
void *
xixcore_call
xixcore_AllocateMem(
	xc_uint32 Size,
	xc_uint32 MemAttr,
	xc_uint32 MemTag
);

/*
 * Free a memory block.
 */
void
xixcore_call
xixcore_FreeMem(
	void *Mem,
	xc_uint32 MemTag
);

/*
 * Allocate a large memory block.
 */
void *
xixcore_call
xixcore_AllocateLargeMem(
	xc_size_t Size,
	xc_uint32 MemAttr,
	xc_uint32 MemTag
	);

/*
 * Free a large memory block.
 */
void
xixcore_call
xixcore_FreeLargeMem(
	void *Mem,
	xc_uint32 MemTag
	);

/*
 * Calculate the current buffer size in the Xixcore buffer.
 */
xc_uint32
xixcore_call
xixcore_Divide64(
	xc_uint64 *number,
	xc_uint32 base
	);


/*
 * Initialize spin lock
 */

void
xixcore_call
xixcore_InitializeSpinLock(
	PXIXCORE_SPINLOCK Spinlock
	);

/*
 * Acquire spin lock
 */

void
xixcore_call
xixcore_AcquireSpinLock(
	PXIXCORE_SPINLOCK Spinlock,
	PXIXCORE_IRQL OldIrql
	);

/*
 * Release spin lock
 */

void
xixcore_call
xixcore_ReleaseSpinLock(
	PXIXCORE_SPINLOCK Spinlock,
	XIXCORE_IRQL OldIrql
	);

/*
 * Initialize semaphore
 */
void
xixcore_call
xixcore_InitializeSemaphore(
	PXIXCORE_SEMAPHORE Semaphore,
	int Value
	);

/*
 * Increase semaphore
 */
void
xixcore_call
xixcore_IncreaseSemaphore(
	PXIXCORE_SEMAPHORE Semaphore
	);

/*
 * Decrease semaphore
 */
void
xixcore_call
xixcore_DecreaseSemaphore(
	PXIXCORE_SEMAPHORE Semaphore
	);

/*
 * Atomic read
 */
int
xixcore_call
xixcore_ReadAtomic(
	XIXCORE_ATOMIC * AtomicVar
);

/*
 * Atomic set
 */
void
xixcore_call
xixcore_SetAtomic(
	XIXCORE_ATOMIC * AtomicVar,
	int value
);

/*
 * Atomic increment
 */
void
xixcore_call
xixcore_IncreaseAtomic(
	XIXCORE_ATOMIC * AtomicVar
	);

/*
 * Atomic decrement
 */
void
xixcore_call
xixcore_DecreaseAtomic(
	XIXCORE_ATOMIC * AtomicVar
	);

/*
 * Get xc_uint64 value of current ttime
 */
xc_uint64
xixcore_call
xixcore_GetCurrentTime64(void);

#endif //#ifndef __XIXCORE_SYSTEM_H__

