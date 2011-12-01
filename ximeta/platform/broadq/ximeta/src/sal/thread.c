/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
#include <stdio.h>
#include <kernel.h>
#include <thbase.h>
#include "sal/types.h"
#include "sal/thread.h"
#include "sal/time.h"
#include "sal/debug.h"

int
sal_thread_create(sal_thread_id* tid, char* name,
    int prio, int stacksize,
    sal_thread_func f, void* arg)
{
    struct t_thread param;
    int th;
    
    sal_debug_print("sal_thd: \"%s\", tid: %#x, ent: %#x, arg: %#x, pr: %d, stk: %#x\n", name, tid, (void*)f, arg, prio, stacksize);
    
prio = 20 - prio;
    param.type         = TH_C;
    param.function       = (void*)f;
    param.priority       = 41 - prio; /* In PS2 lower priority number is high priority */
    if (stacksize==0)
        param.stackSize    = 0x3000;
    else
        param.stackSize = stacksize;
param.stackSize    = 0x8000;

    th = CreateThread(&param);

    
    if (th > 0)
    {
        // tjd commented out 'cuz we are getting bogus pointers....
        if (tid)
    //if (0)
            *tid = (sal_thread_id)th;
        StartThread(th, arg);
        sal_debug_print("Thread \"%s\" started, id: %#x, pr: %d\n", name, th, param.priority);
        return 0;
    }
    else {
        sal_debug_print("Unable to start \"%s\", r: %d\n", name, th);
        return 1;
    }
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

