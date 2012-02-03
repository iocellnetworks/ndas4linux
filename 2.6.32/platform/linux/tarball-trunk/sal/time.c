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
#include <linux/version.h> // version
#include <linux/module.h> // EXPORT_SYMBOL
#include <linux/sched.h> // jiffies
#include <linux/interrupt.h> // in_interrupt
#include <linux/wait.h> // wait_queue_head_t
#include <linux/errno.h> //wait_event_interruptible_timeout require ERESTARTSYS
#include "sal/types.h"
#include "sal/time.h"
#include "sal/debug.h" // sal_debug_print
#include "linux_ver.h" // SAL_HZ

#ifdef DEBUG
#ifndef DEBUG_LEVEL_SAL_TIME
#define DEBUG_LEVEL_SAL_TIME 1
#endif
#define dbgl_saltime(l,x...) do { \
    if ( l <= DEBUG_LEVEL_SAL_TIME ) {\
        sal_debug_print("ST|%d|%s|",l, __FUNCTION__); \
        sal_debug_println(x); \
    }\
} while(0) 
#else
#define dbgl_saltime(l,x...) do {} while(0)
#endif

#ifndef XPLAT_ALLOW_LINUX_HEADER
#if LINUX_VERSION_NON_CONSTANT_HZ
xuint32 SAL_TICKS_PER_SEC = USER_HZ;
#else
xuint32 SAL_TICKS_PER_SEC = HZ;
#endif

EXPORT_SYMBOL(SAL_TICKS_PER_SEC);

NDAS_SAL_API
sal_tick
sal_get_tick(void)
{
    return jiffies;
}
EXPORT_SYMBOL(sal_get_tick);
#endif /* XPLAT_ENABLE_INLINE_OPT */

#if LINUX_VERSION_NON_CONSTANT_HZ 
#define TICK2MSEC(tick) ((HZ>1000)?((tick)/(HZ/1000)):((tick)*(1000/HZ)))
#define MSEC2TICK(msec) ((HZ>1000)?((msec)*(HZ/1000)):((msec)/(1000/HZ)))
#else
#if (HZ>1000)
#define TICK2MSEC(tick) ((tick)/(HZ/1000))
#define MSEC2TICK(msec) ((msec)*(HZ/1000))
#else
#define TICK2MSEC(tick) ((tick)*(1000/HZ))
#define MSEC2TICK(msec) ((msec)/(1000/HZ))
#endif
#endif

NDAS_SAL_API
sal_msec
sal_time_msec(void)
{
    return TICK2MSEC(jiffies);
}

EXPORT_SYMBOL(sal_time_msec);

NDAS_SAL_API
void
sal_msleep(sal_msec msec)
{
    long jit;
#ifdef DEBUG
    if (in_interrupt()) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20))
        dump_stack();
#endif
    }
#endif
    // TODO early overflow
    jit = MSEC2TICK(msec);
    
    if ( msec >= HZ/2 )
        dbgl_saltime(9,"interruptible_sleep_on_timeout-ing for %d ms,jiffies=%ld",msec,jiffies);

    while(1) {
        long waited;
        long begin = jiffies;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
        DECLARE_WAIT_QUEUE_HEAD(wait);
        wait_event_interruptible_timeout(wait, 0, jit);
#else
        wait_queue_head_t wait;
        init_waitqueue_head (&wait);
        interruptible_sleep_on_timeout(&wait, jit);
#endif
        waited = (jiffies - begin);
        if ( msec >= HZ/2 )
            dbgl_saltime(9, "waited=%ld", waited);
        /* somehow when there are many thread active,
          sleep_on_timeout doesn't sleep for the requested time.
          so check if slept for the requested time,
          if 10  mili-sec less, do it again.
         */
        if ( waited + MSEC2TICK(10) >= jit )  
            break;
                
        dbgl_saltime(9,"jit=%ld but waited only %ld msec=%d", 
            jit, (jiffies-begin), msec);
        jit -= waited;
    }

    if ( msec >= HZ/2 )
        dbgl_saltime(9,"interruptible_sleep_on_timeout-ed for %d,jiffies=%ld",msec,jiffies);
}
EXPORT_SYMBOL(sal_msleep);
