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

#include <ntddk.h>

#include "xcsystem/debug.h"
#include "xcsystem/errinfo.h"
#include "xcsystem/system.h"
#include "xcsystem/winnt/xcsysdep.h"


/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XCSYSTEM"

//
// winnt-dependent implementation for XIXFS core.
//

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

/*
 * 64 bit operations
 */
xc_uint32
xixcore_call
xixcore_Divide64(xc_uint64 *number, xc_uint32 base) {
	xc_uint32 remain;
	remain = (xc_uint32)(*number % base);
	*number /= base;
	return remain;
}

/*
 * Synchronization primitives
 */
void
xixcore_call
xixcore_InitializeSpinLock(
	PXIXCORE_SPINLOCK Spinlock
	)
{
	ExInitializeFastMutex(&Spinlock->impl);
}

void
xixcore_call
xixcore_AcquireSpinLock(
	PXIXCORE_SPINLOCK Spinlock,
	PXIXCORE_IRQL OldIrql
	)
{
	UNREFERENCED_PARAMETER(OldIrql);
	ExAcquireFastMutexUnsafe(&Spinlock->impl);
}

void
xixcore_call
xixcore_ReleaseSpinLock(
	PXIXCORE_SPINLOCK Spinlock,
	XIXCORE_IRQL OldIrql
	)
{
	UNREFERENCED_PARAMETER(OldIrql);
	ExReleaseFastMutexUnsafe(&Spinlock->impl);
}

void
xixcore_call
xixcore_InitializeSemaphore(
	PXIXCORE_SEMAPHORE	Semaphore,
	int					Value
)
{
	//
	// The caller also must specify a Limit for the semaphore,
	// which can be either of the following:
	//
	// 	Limit = 1
	// When this semaphore is set to the Signaled state, a single thread
	// waiting on the semaphore becomes eligible for execution and can
	// access whatever resource is protected by the semaphore.
	//
	// This type of semaphore is also called a binary semaphore
	// because a thread either has or does not have exclusive access
	// to the semaphore-protected resource.
	//
	// Limit > 1
	// When this semaphore is set to the Signaled state, some number of threads
	// waiting on the semaphore object become eligible for execution and
	// can access whatever resource is protected by the semaphore.
	//
	// This type of semaphore is called a counting semaphore
	// because the routine that sets the semaphore to the Signaled state
	// also specifies how many waiting threads can have their states
	// changed from waiting to ready. The number of such waiting threads
	// can be the Limit set when the semaphore was initialized or
	// some number less than this preset Limit.
	//

	//KeInitializeSemaphore(&Semaphore->impl, Value, Value>1?Value:1);
	UNREFERENCED_PARAMETER(Value);
	ExInitializeFastMutex(&Semaphore->impl);
}

void
xixcore_call
xixcore_IncreaseSemaphore(
	PXIXCORE_SEMAPHORE Semaphore
){
	
	//KeReleaseSemaphore(&Semaphore->impl, IO_NO_INCREMENT,
	//	1, FALSE);
	ExReleaseFastMutexUnsafe(&Semaphore->impl);
}

void
xixcore_call
xixcore_DecreaseSemaphore(
	PXIXCORE_SEMAPHORE Semaphore
)
{
	 //NTSTATUS        status;

	 //XIXCORE_ASSERT(KeGetCurrentIrql() <= PASSIVE_LEVEL);

	 //status = KeWaitForSingleObject(&Semaphore->impl, Executive, KernelMode, FALSE, NULL);
	//if(!NT_SUCCESS(status)) {
	//	XIXCORE_ASSERT(FALSE);
	//}
	ExAcquireFastMutexUnsafe(&Semaphore->impl);
}

int
xixcore_call
xixcore_ReadAtomic(
	XIXCORE_ATOMIC * AtomicVar
){
	return AtomicVar->value;
}

void
xixcore_call
xixcore_SetAtomic(
	XIXCORE_ATOMIC * AtomicVar,
	int value
	)
{
	InterlockedExchange(&AtomicVar->value, value);
}

void
xixcore_call
xixcore_IncreaseAtomic(
	XIXCORE_ATOMIC * AtomicVar
	)
{
	InterlockedIncrement(&AtomicVar->value);
}

void
xixcore_call
xixcore_DecreaseAtomic(
	XIXCORE_ATOMIC * AtomicVar
	)
{
	InterlockedDecrement(&AtomicVar->value);
}

/*
 * Time
 */

xc_uint64
xixcore_call
xixcore_GetCurrentTime64(void)
{
	LARGE_INTEGER Time;

	KeQuerySystemTime(&Time);
	return Time.QuadPart;
}

/*
 * Debug
 */


void
xixcore_call
xixcore_DebugPrintf(
	const char * format,
	...)
{
	static char printk_buf[1024];
	int printed_len;
	va_list ap;

	va_start(ap, format);
	printed_len = _vsnprintf(printk_buf, sizeof(printk_buf), format, ap);
	va_end(ap);
	DbgPrint(printk_buf);
}

#if defined(XIXCORE_DEBUG)

void
xixcore_call
xixcore_Assert(void * condition, char * message)
{
	RtlAssert(condition, "", 0, message);
}




int 
xixcore_call
xixcore_DebugMaskAndPrefix(xc_uint32 Level, xc_uint32 Target)
{
    if ((Target & XixcoreDebugTarget)  && (XixcoreDebugLevel & Level) ) {
		DbgPrint("tid[%p]:", KeGetCurrentThread());
		if (Level > 0) {
			DbgPrint("%*s", (int)Level, "" );
		}
        return 1;
    } else {
        return 0;
    }
}

#endif

/*
 * Error code conversion
 */

XCCODE XixcoreErrorCodeLookup[XCCODE_MAX] = {
		STATUS_SUCCESS,			// 0
		STATUS_ACCESS_DENIED,
		STATUS_NO_MORE_FILES,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNEXPECTED_IO_ERROR,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,	// 10
		STATUS_UNSUCCESSFUL,
		STATUS_INSUFFICIENT_RESOURCES,
		STATUS_ACCESS_DENIED,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,	// 20
		STATUS_UNSUCCESSFUL,
		STATUS_INVALID_PARAMETER,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_DISK_FULL,
		STATUS_UNSUCCESSFUL,
		STATUS_MEDIA_WRITE_PROTECTED,	// 30
		STATUS_TOO_MANY_LINKS,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,
		STATUS_UNSUCCESSFUL,		// 35
		STATUS_CRC_ERROR
};


/*
 * Memory
 */

void *
xixcore_call
xixcore_AllocateMem(
	xc_uint32 Size,
	xc_uint32 MemAttr,
	xc_uint32 MemTag
){
	int poolType;

	poolType = 0;
	if(MemAttr & XCMEMATTR_PAGED) {
		poolType = PagedPool;
	} else {
		poolType = NonPagedPool;
	}

	return ExAllocatePoolWithTag(poolType, Size, MemTag);
}


#if WINVER < 0x0501
#define ExFreePoolWithTag(a,b) ExFreePool(a)
#endif // WINVER < 0x0501

void
xixcore_call
xixcore_FreeMem(
	void *Mem,
	xc_uint32 MemTag
){
#if WINVER < 0x0501
	UNREFERENCED_PARAMETER(MemTag);
#endif // WINVER < 0x0501
	ExFreePoolWithTag(Mem, MemTag);
}

void *
xixcore_call
xixcore_AllocateLargeMem(
	xc_size_t Size,
	xc_uint32 MemAttr,
	xc_uint32 MemTag
) {
	int poolType;

	poolType = 0;
	if(MemAttr & XCMEMATTR_PAGED) {
		poolType = PagedPool;
	} else {
		poolType = NonPagedPool;
	}

	return ExAllocatePoolWithTag(poolType, Size, MemTag);
}

void
xixcore_call
xixcore_FreeLargeMem(
	void *Mem,
	xc_uint32 MemTag
){
#if WINVER < 0x0501
	UNREFERENCED_PARAMETER(MemTag);
#endif // WINVER < 0x0501
	ExFreePoolWithTag(Mem, MemTag);
}
