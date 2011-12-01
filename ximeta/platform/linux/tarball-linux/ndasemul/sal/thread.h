/*
 -------------------------------------------------------------------------
 Copyright (c) 2005, XIMETA, Inc., IRVINE, CA, USA.
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
#ifndef _SAL_THREAD_H_
#define _SAL_THREAD_H_

typedef void* sal_thread_id;
typedef void* (sal_thread_func)(void*);
#define SAL_INVALID_THREAD_ID (sal_thread_id)0

/* 
 * sal_thread_create - create a system thread(kernel thread).
 * @tid: Thread ID. Can be NULL if tid is not used
 * @name: Name of the thread. Used for debugging purpose.
 * @prio: priority of newly created thread. Can be integer between -2 and 2.
 *     Value 0 means normal priority, and 1,2 means above normal, high.
 *     Negative value is priority lower than normal thread.a
 * @stacksize: 0 for default stack size.
 * @return: 0 for success 
*/

NDAS_SAL_API extern int sal_thread_create(
    sal_thread_id* tid, char* name, int prio, int stacksize, 
    sal_thread_func f, void* arg);

NDAS_SAL_API extern int sal_thread_yield(void);

NDAS_SAL_API extern sal_thread_id sal_thread_self(void);
/* 
 * Exit current thread with return value ret 
 */
NDAS_SAL_API extern void sal_thread_exit(int ret);

#if defined(LINUX) && !defined(LINUXUM)
/*
 * block the signals that should be pended while IO processed
 * other implmentations can leave this function no_pending_dpc
 */
NDAS_SAL_API extern void sal_thread_block_signals(void);
/*
 * flush signals pended
 * other implmentations can leave this function no_pending_dpc
 */
NDAS_SAL_API extern void sal_thread_flush_signals(void);

/*
 * Make the thread as daemon.
 * Set the name of current thread.
 * currently only needed for Linux Kernel device driver.
 * other implmentations can leave this function no_pending_dpc
 */
NDAS_SAL_API extern void sal_thread_daemonize(const char*,...);

NDAS_SAL_API extern void sal_thread_nice(long nice);

#else
#define sal_thread_block_signals() do { } while(0)
#define sal_thread_flush_signals() do { } while(0)
#define sal_thread_daemonize(x,...) do { } while(0)
#endif

#endif
