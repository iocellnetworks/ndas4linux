/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
#include <stdio.h>
#include <thbase.h>
#include "sal/types.h"
#include "sal/thread.h"
#include "sal/time.h"
#include "sal/debug.h"

int 
sal_thread_create(
    sal_thread_id* tid, char* name, int prio, int stacksize,
    sal_thread_func f, void* arg)
{
    struct _iop_thread param;
    int th;
    
//    sal_debug_print("Creating thread: %s\n", name);
    
    param.attr         = TH_C;
    param.thread       = (void*)f;
    param.priority       = 41 - prio; /* In PS2 lower priority number is high priority */
    if (stacksize==0)
        param.stacksize    = 0x4000;
    else
        param.stacksize = stacksize;
    param.option      = 0;

    th = CreateThread(&param);

    
    if (th > 0)
    {
        if (tid)
            *tid = (sal_thread_id)th;
        StartThread(th, arg);
        return 0;
    }
    else
        return 1;
}

int
sal_thread_yield(void)
{
    sal_msleep(1);
    return 0;
}

sal_thread_id 
sal_thread_self(void)
{
    return (sal_thread_id)GetThreadId();
}

void
sal_thread_exit(int ret)
{
    ExitDeleteThread();
}
void sal_thread_block_signals() {}

void sal_thread_flush_signals() {}

void sal_thread_daemonize(const char* format,...) {}

