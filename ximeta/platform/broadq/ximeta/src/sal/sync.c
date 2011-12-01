/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
#include <thsemap.h>
#include <intrman.h>
#include "sal/sync.h"
#include "sal/thread.h"
#include "sal/time.h"
#include "sal/mem.h"
#include "sal/debug.h"
#include "sal/types.h"

//    Swap the comments on the following two lines to enable/disable debugging printf's
//#define SYNC_DEBUG sal_debug_print
#define SYNC_DEBUG(a...)

typedef struct {
    unsigned int attr;
        unsigned int option;
        int init_count;
        int max_count;
        int count;
        int wait_threads;
} sema_info;


//    Simple check to ensure sema ids are "the tiniest bit" valid.
//    This needs to be a macro, so that trace error information from the assert is correct.

#define CHECK_SEMA(s) \
    sal_assert(((unsigned int)s  >= 0x1000000) && ((unsigned int)s  < 0x3f00000));


sal_mutex
sal_mutex_create(const char *desc)
{
    int r;
    /* ignore desc */
    struct t_sema sema;
    sema.attr = 0;
    sema.option = 0;
    sema.init_count = 1;
    sema.max_count = 1;
    r = CreateSema(&sema);
    SYNC_DEBUG("sal_mutex_create returned: %#x\n", r);
    CHECK_SEMA(r);
    return (sal_mutex)r;
}

void
sal_mutex_destroy(sal_mutex m)
{
    SYNC_DEBUG("sal_mutex_destroy: %#x\n", m);
    CHECK_SEMA(m);
    DeleteSema((int)m);
}

int
sal_mutex_take(sal_mutex m)
{
    return sal_semaphore_take((sal_semaphore)m, SAL_SYNC_FOREVER);
}

int
sal_mutex_give(sal_mutex m)
{
    SYNC_DEBUG("sal_mutex_give, id: %#x\n", m);
    CHECK_SEMA(m);

    if (QueryIntrContext()==1) {
        iSignalSema((int)m);
    } else {
        SignalSema((int)m);    
    }
    return 0;
}
int 
sal_mutex_getvalue(sal_mutex b)
{
    sema_info info;
    int ret;
    //SYNC_DEBUG("sal_semaphore_getvalue, id: %#x\n", (int)b);
    CHECK_SEMA(b);
    ret = ReferSemaStatus((int)b, &info);
    if (ret!=0) {
        sal_assert(0);
    }
    return info.count;
}
sal_semaphore
sal_semaphore_create(const char *desc, int max_count, int initial_count)
{
    struct t_sema    sema;
    int            semid;

    sal_assert(initial_count>=0 && initial_count<=max_count);
    
    sema.attr = 0;
    sema.option = 0;
    sema.max_count = max_count;
    sema.init_count = initial_count;

    semid=CreateSema(&sema);    
    SYNC_DEBUG("sal_semaphore_create, id: %#x", semid);
    CHECK_SEMA(semid);
    return (sal_semaphore)semid;
}

void
sal_semaphore_destroy(sal_semaphore b)
{
    SYNC_DEBUG("sal_semaphore_destroy, id: %#x\n", b);
    CHECK_SEMA(b);
    DeleteSema((int)b);    
}

/* Return -1 for timeout, 0 for successful semaphore taking */
int
sal_semaphore_take(sal_semaphore b, sal_tick tick)
{
       int            err;
    int semid=(int)b;
    sal_tick origMsec = tick; 
    SYNC_DEBUG("sal_mutex_take entered, id: %#x, timeout: %d\n", semid, tick);
    CHECK_SEMA(b);

    if (tick == SAL_SYNC_FOREVER) {
        err = WaitSema(semid);
    } else {
        int        time_wait = 1;
    
        /* Retry algorithm with exponential backoff */
    
        while (1) {
            err = PollSema(semid);
            if (err == 0) {
                break;
            }

            if (tick  == 0) {
                err = -1;
                break;
            }
    
            if (time_wait > tick) {
                time_wait = tick;
            }

            sal_msleep(time_wait);
    
            tick -= time_wait;
    
            if (tick == 0) {
                err = -1; /* timeout */
                break;
            }
    
            if ((time_wait *= 2) > 100) {
                time_wait = 100;
            }
        }
    }
    if (err) {
        SYNC_DEBUG("sal_mutex_take timed out, id: %#x\n", semid);
        return -1;
    }
    SYNC_DEBUG("sal_mutex_take ok, id: %#x, time needed: %d\n", semid, origMsec - tick);
    return 0;
}

int
sal_semaphore_give(sal_semaphore b)
{
    SYNC_DEBUG("sal_mutex_give, id: %#x\n", b);
    CHECK_SEMA(b);

    if (QueryIntrContext()==1) {
        iSignalSema((int)b);
    } else {
        SignalSema((int)b);    
    }
    return 0;
}

/* 
    return -1 if semaphore is taken by another one.
     return 0 if semaphore is free
*/
int 
sal_semaphore_trywait(sal_semaphore b)
{
    int ret;
    SYNC_DEBUG("sal_semaphore_trywait, id: %#x\n", (int)b);
    CHECK_SEMA(b);
    ret = PollSema((int)b);
    return ret?0:-1;
}

int 
sal_semaphore_getvalue(sal_semaphore b)
{
    sema_info info;
    int ret;
    //SYNC_DEBUG("sal_semaphore_getvalue, id: %#x\n", (int)b);
    CHECK_SEMA(b);
    ret = ReferSemaStatus((int)b, &info);
    if (ret!=0) {
        sal_assert(0);
    }
    return info.count;
}

