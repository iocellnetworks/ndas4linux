/*
 -------------------------------------------------------------------------
 Copyright (C) 2011, IOCELL Networks Corp. Plainsboro, NJ, USA.
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
/*
    Abstracts synchronization mechanism of operating system

 */

#ifndef _SAL_SYNC_H
#define _SAL_SYNC_H
#include "sal/sal.h"
#include "sal/time.h"

#define SAL_SYNC_FOREVER    (-1)
#define SAL_SYNC_NOWAIT        0

#define SAL_INVALID_MUTEX    (sal_spinlock)(0)
#define SAL_INVALID_SEMAPHORE (sal_semaphore)(0)
#define SAL_INVALID_EVENT   (sal_event)(0)

#define SAL_SYNC_OK                     (0)
#define SAL_SYNC_ERR                    (-1)
#define SAL_SYNC_TIMEOUT            (-2)
#define SAL_SYNC_INTERRUPTED    (-3)

struct _sal_semaphore;
typedef struct _sal_semaphore* sal_semaphore;
struct _sal_event;
typedef struct _sal_event* sal_event;

typedef struct { volatile int counter; } sal_atomic;

#define sal_atomic_set(x, v)            do { (x)->counter = v; } while(0)
#define sal_atomic_read(x)                ((x)->counter)
#define sal_atomic_init(v)      {v}

#if defined(XPLAT_ALLOW_LINUX_HEADER)

#elif defined(UCOSII)
#include <ucos_ii.h>
#define sal_spinlock_create(desc) (sal_spinlock)(1)
#define sal_spinlock_destroy(m) do { } while(0)
#define sal_spinlock_take_irqsave(m,f) ({ OSDisableInt(); f; })
#define sal_spinlock_give_irqrestore(m,f) ({ OSEnableInt(); f; })
#define sal_spinlock_take_softirq(m) ({ OSDisableInt(); })
#define sal_spinlock_give_softirq(m) ({ OSEnableInt(); })
#else

struct _sal_spinlock;
typedef struct _sal_spinlock* sal_spinlock;

NDAS_SAL_API int     sal_spinlock_create(const char *desc, sal_spinlock* lock);
NDAS_SAL_API void        sal_spinlock_destroy(sal_spinlock m);
NDAS_SAL_API void        sal_spinlock_take_irqsave(sal_spinlock m, unsigned long* flags);
NDAS_SAL_API void        sal_spinlock_give_irqrestore(sal_spinlock m, unsigned long* flags);

NDAS_SAL_API void        sal_spinlock_take_softirq(sal_spinlock m);
NDAS_SAL_API void        sal_spinlock_give_softirq(sal_spinlock m);

NDAS_SAL_API void        sal_spinlock_take(sal_spinlock m);
NDAS_SAL_API void        sal_spinlock_give(sal_spinlock m);

NDAS_SAL_API void sal_atomic_inc(sal_atomic *v);
NDAS_SAL_API void sal_atomic_dec(sal_atomic *v);
/* Returns 1 if decreased value is 0 */
NDAS_SAL_API int sal_atomic_dec_and_test(sal_atomic *v);

#endif 

NDAS_SAL_API sal_semaphore    sal_semaphore_create(const char *desc, int max_count, int initial_count);
NDAS_SAL_API void        sal_semaphore_destroy(sal_semaphore b);

/*
 * Take one semaphore count.
 *
 * NOTE: Timeout support will be deprecated.
 *   If timeout is specified, the function will poll the semaphore.
 *   It will degrade system performance significantly, so do not use time-out.
 *
 * Returns:
 *   SAL_SYNC_OK for successful semaphore taking 
 *   SAL_SYNC_TIMEOUT for timeout
 *   SAL_SYNC_INTERRUPTED if wait function is interrupted by signal
 */
NDAS_SAL_API int        sal_semaphore_take(sal_semaphore b, sal_tick tick);
NDAS_SAL_API int        sal_semaphore_give(sal_semaphore b);

NDAS_SAL_API int        sal_semaphore_getvalue(sal_semaphore b);

NDAS_SAL_API sal_event sal_event_create(const char *desc);
NDAS_SAL_API void sal_event_destroy(sal_event event);
NDAS_SAL_API int sal_event_wait(sal_event event, sal_tick timeout);
NDAS_SAL_API void sal_event_set(sal_event event);
NDAS_SAL_API void sal_event_reset(sal_event event);

#endif    /* !_SAL_SYNC_H */

