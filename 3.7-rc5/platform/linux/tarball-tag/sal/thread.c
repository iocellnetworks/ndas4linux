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
#include <linux/module.h> // EXPORT_SYMBOL
#include <linux/sched.h> // kernel_thread
#include <linux/errno.h> // /usr/include/errno.h , ENOMEM
#include "linux_ver.h"
#include "sal/types.h"
#include "sal/thread.h"
#include "sal/time.h"
#include "sal/debug.h"
#include "ndasuser/ndaserr.h"

NDAS_SAL_API
int 
sal_thread_create(
    sal_thread_id* tid, char* name, int prio, int stacksize,
    sal_thread_func f, void* arg)
{
    unsigned long flags = 0L; // ignored in 2.4 - 2.6
    pid_t ret = 0;
    if ((ret = kernel_thread((int (*)(void*))f, (void *)arg, flags)) < 0) {
        
        switch (ret) {
        case ENOMEM:
            printk("ndas: fail to create kernel thread: out of memory\n");
            return NDAS_ERROR_OUT_OF_MEMORY;
        case EAGAIN:
            // todo
        case EINVAL:
        default:
            break;
        }
        printk("ndas: fail to create kernel thread\n");
        return NDAS_ERROR_INTERNAL;
    }
    if (tid)
        *tid = (sal_thread_id)(long)ret;
    return NDAS_OK;
}
EXPORT_SYMBOL(sal_thread_create);

NDAS_SAL_API
int
sal_thread_yield(void)
{
    schedule();
    return 0;
}
EXPORT_SYMBOL(sal_thread_yield);

NDAS_SAL_API
sal_thread_id 
sal_thread_self(void)
{
   return (sal_thread_id) (long)(current->pid);
}
EXPORT_SYMBOL(sal_thread_self);

NDAS_SAL_API
void
sal_thread_exit(int ret)
{
	//	do_nothing
}
EXPORT_SYMBOL(sal_thread_exit);

NDAS_SAL_API
void sal_thread_block_signals()
{
    unsigned long flags;
// 2.4.20-8(RH9.0)
#if (!defined(NDAS_SUSE) && LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20)) || \
    (defined(NDAS_SUSE) && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,21))     ||\
    (defined(NDAS_MVISTA) && LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,20))) || \
	defined(NDAS_SIGPENDING_OLD) ||\
    (defined(NDAS_MSHARE) && LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,26)) ||\
    defined(NDAS_EM86XX)
    spin_lock_irqsave(&current->sigmask_lock, flags);
//    flush_signals(current);
//    siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGTERM));
    sigfillset(&current->blocked);
    recalc_sigpending(current);
    spin_unlock_irqrestore(&current->sigmask_lock, flags);
#else
    spin_lock_irqsave(&current->sighand->siglock, flags);
    //flush_signals(current);	
    //siginitsetinv(&current->blocked, sigmask(SIGKILL)|sigmask(SIGTERM));
    sigfillset(&current->blocked);
    recalc_sigpending();
    spin_unlock_irqrestore(&current->sighand->siglock, flags);	
#endif

}
EXPORT_SYMBOL(sal_thread_block_signals);

NDAS_SAL_API
void sal_thread_flush_signals()
{
    if(signal_pending(current)) {
        flush_signals(current);
    }
}
EXPORT_SYMBOL(sal_thread_flush_signals);

/* 16 for 2.4 and 2.6 */
#define SIZE_COMM sizeof(((struct task_struct*)0)->comm)
NDAS_SAL_API
void sal_thread_daemonize(const char* name, ...)
{
    va_list args;
#if LINUX_VERSION_25_ABOVE
    char buffer[SIZE_COMM];
    va_start(args, name);
    vsnprintf(buffer,SIZE_COMM,name,args);
    va_end(args);
    daemonize(buffer);
#else
    va_start(args, name);
    vsnprintf(current->comm,sizeof(current->comm),name, args);
    va_end(args);
    daemonize();
#if LINUX_VERSION_HAS_REPARENT_TO_INIT    
    reparent_to_init(); // kernel over 2.5 don't need this because daemonize call this function.
#endif    
#endif    
}
EXPORT_SYMBOL(sal_thread_daemonize);

NDAS_SAL_API
void sal_thread_nice(long nice)
{
#if LINUX_VERSION_25_ABOVE
	set_user_nice(current, nice);
#endif
}
EXPORT_SYMBOL(sal_thread_nice);
