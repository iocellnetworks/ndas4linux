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

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#include <asm/div64.h>
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>

#include "xcsystem/debug.h"
#include "xcsystem/errinfo.h"
#include "xcsystem/system.h"
#include "xcsystem/linux/xcsysdep.h"


/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XCSYSTEM"

//
// Linux-dependent implementation for XIXFS core.
//

/*
 * 64 bit operations
 */
xc_uint32
xixcore_call
xixcore_Divide64(xc_uint64 *number, xc_uint32 base) {
	return do_div(*number, base);
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
	spin_lock_init(&Spinlock->impl);
}

void
xixcore_call
xixcore_AcquireSpinLock(
	PXIXCORE_SPINLOCK Spinlock,
	PXIXCORE_IRQL OldIrql
	)
{
	spin_lock(&Spinlock->impl);
}

void
xixcore_call
xixcore_ReleaseSpinLock(
	PXIXCORE_SPINLOCK Spinlock,
	XIXCORE_IRQL OldIrql
	)
{
	spin_unlock(&Spinlock->impl);
}

void
xixcore_call
xixcore_InitializeSemaphore(
	PXIXCORE_SEMAPHORE	Semaphore,
	int					Value
)
{
	sema_init(&Semaphore->impl, Value);
}

void
xixcore_call
xixcore_IncreaseSemaphore(
	PXIXCORE_SEMAPHORE Semaphore
){
	up(&Semaphore->impl);
}

void
xixcore_call
xixcore_DecreaseSemaphore(
	PXIXCORE_SEMAPHORE Semaphore
)
{
	down(&Semaphore->impl);
}

int
xixcore_call
xixcore_ReadAtomic(
	XIXCORE_ATOMIC * AtomicVar
){
	return atomic_read((atomic_t *)AtomicVar);
}

void
xixcore_call
xixcore_SetAtomic(
	XIXCORE_ATOMIC * AtomicVar,
	int value
	)
{
	atomic_set((atomic_t *)AtomicVar, value);
}

void
xixcore_call
xixcore_IncreaseAtomic(
	XIXCORE_ATOMIC * AtomicVar
	)
{
	atomic_inc((atomic_t *)AtomicVar);
}

void
xixcore_call
xixcore_DecreaseAtomic(
	XIXCORE_ATOMIC * AtomicVar
	)
{
	atomic_dec((atomic_t *)AtomicVar);
}

/*
 * Time
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0))
XIXCORE_INLINE
struct timespec current_kernel_time(void)
{
       struct timespec now;	
	now.tv_sec= xtime.tv_sec;
	now.tv_nsec = xtime.tv_usec;
	return now; 
}
#endif

xc_uint64
xixcore_call
xixcore_GetCurrentTime64(void)
{
	struct timespec ts = current_kernel_time();
	xc_uint64 t = 0;
	t = (xc_uint64) ((xc_uint64)ts.tv_sec * 10000000 + (xc_uint64)(ts.tv_nsec / 100));
	return t;
}

/*
 * Debug
 */

#if defined(XIXCORE_DEBUG)

void
xixcore_call
xixcore_Assert(void * condition, char * message)
{
#ifdef CONFIG_KERNEL_ASSERTS

	KERNEL_ASSERT(message, condition);

#else

	if (!condition) {
		printk(KERN_CRIT "BUG at %s:%d assert(%s)\n",
			__FILE__, __LINE__, message);
		BUG();
	}

#endif //#ifdef CONFIG_KERNEL_ASSERTS
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)

void
xixcore_call
xixcore_DebugPrintf(
	const char * format,
	...)
{
	va_list ap;
	va_start(ap, format);
	vprintk(format, ap);
	va_end(ap);
}

#else

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
	printed_len = vsnprintf(printk_buf, sizeof(printk_buf), format, ap);
	va_end(ap);
	printk(printk_buf);
}
#endif

int 
xixcore_call
xixcore_DebugMaskAndPrefix(xc_uint32 Level, xc_uint32 Target)
{
    if ((Target & XixcoreDebugTarget)  && (XixcoreDebugLevel & Level) ) {
        printk( KERN_CRIT "pid[%d]:", current->pid);
        if (Level > 0) {
			printk("%*s", (int)Level, "" );
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
#define ECRCERROR	100

XCCODE XixcoreErrorCodeLookup[XCCODE_MAX] = {
		0,			// 0
		-EPERM,
		-ENOENT,
		-ESRCH,
		-EINTR,
		-EIO,
		-ENXIO,
		-E2BIG,
		-ENOEXEC,
		-EBADF,
		-ECHILD,	// 10
		-EAGAIN,
		-ENOMEM,
		-EACCES,
		-EFAULT,
		-ENOTBLK,
		-EBUSY,
		-EEXIST,
		-EXDEV,
		-ENODEV,
		-ENOTDIR,	// 20
		-EISDIR,
		-EINVAL,
		-ENFILE,
		-EMFILE,
		-ENOTTY,
		-ETXTBSY,
		-EFBIG,
		-ENOSPC,
		-ESPIPE,
		-EROFS,		// 30
		-EMLINK,
		-EPIPE,
		-EDOM,
		-ERANGE,
		-1,			// 35
		-ECRCERROR
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
	)
{
	return kmalloc(Size, GFP_KERNEL);
}

void
xixcore_call
xixcore_FreeMem(
	void *Mem,
	xc_uint32 MemTag
	)
{
	kfree(Mem);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#if 0
#define __bitwise__ __attribute__((bitwise))
#else
#define __bitwise__
#endif
typedef unsigned __bitwise__ gfp_t;
#endif

XIXCORE_INLINE
void *
__xixcore_LargeMalloc(unsigned long size, gfp_t gfp_mask)
{
	if (likely(size <= PAGE_SIZE)) {
		BUG_ON(!size);
		/* kmalloc() has per-CPU caches so is faster for now. */
		return kmalloc(PAGE_SIZE, gfp_mask & ~__GFP_HIGHMEM);
		/* return (void *)__get_free_page(gfp_mask); */
	}
	if (likely(size >> PAGE_SHIFT < num_physpages))
		return __vmalloc(size, gfp_mask, PAGE_KERNEL);
	return NULL;
}


XIXCORE_INLINE
void
__xixcore_LargeFree(void *addr)
{
	if (likely(((unsigned long)addr < VMALLOC_START) ||
			((unsigned long)addr >= VMALLOC_END ))) {
		kfree(addr);
		/* free_page((unsigned long)addr); */
		return;
	}
	vfree(addr);
}

void *
xixcore_call
xixcore_AllocateLargeMem(
	xc_size_t Size,
	xc_uint32 MemAttr,
	xc_uint32 MemTag
) {
	return __xixcore_LargeMalloc(Size, GFP_KERNEL);
}

void
xixcore_call
xixcore_FreeLargeMem(
	void *Mem,
	xc_uint32 MemTag
) {
	__xixcore_LargeFree(Mem);
}

