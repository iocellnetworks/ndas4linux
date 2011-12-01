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


sal_mutex
sal_mutex_create(const char *desc)
{
    /* ignore desc */
    iop_sema_t sema;
    sema.attr = 0;
    sema.option = 0;
    sema.initial = 1;
    sema.max = 1;
    return (sal_mutex)CreateSema(&sema);
}

void
sal_mutex_destroy(sal_mutex m)
{
    sal_assert(m!=SAL_INVALID_MUTEX);
    DeleteSema((int)m);
}

void
sal_mutex_take(sal_mutex m, unsigned long flags)
{
    return sal_semaphore_take((sal_semaphore)m, SAL_SYNC_FOREVER);
}

void
sal_mutex_give(sal_mutex m, unsigned long* flags)
{
    sal_assert(m!=SAL_INVALID_MUTEX);
    if (QueryIntrContext()==1) {
        iSignalSema((int)m);
    } else {
        SignalSema((int)m);    
    }
    return 0;
}
int     sal_mutex_getvalue(sal_mutex b) {
    iop_sema_info_t info;
    int ret;
    ret = ReferSemaStatus((int)b, &info);
    if (ret!=0) {
        sal_assert(0);
    }
    return info.current;
}
sal_semaphore
sal_semaphore_create(const char *desc, int max_count, int initial_count)
{
    iop_sema_t    sema;
    int            semid;

    sal_assert(initial_count>=0 && initial_count<=max_count);
    
    sema.attr = 0;
    sema.option = 0;
    sema.max = max_count;
    sema.initial = initial_count;

    semid=CreateSema(&sema);    
    return (sal_semaphore)semid;
}

void
sal_semaphore_destroy(sal_semaphore b)
{
    DeleteSema((int)b);    
}

/* Return -1 for timeout, 0 for successful semaphore taking */
int
sal_semaphore_take(sal_semaphore b, sal_tick msec)
{
   int            err;
    int semid=(int)m;
    sal_assert(m!=SAL_INVALID_MUTEX);

    if (msec == SAL_SYNC_FOREVER) {
        err = WaitSema(semid);
    } else {
        int        time_wait = 1;
    
        /* Retry algorithm with exponential backoff */
    
        while (1) {
            err = PollSema(semid);
            if (err == 0) {
                break;
            }

            if (msec == 0) {
                err = -1;
                break;
            }
    
            if (time_wait > msec) {
                time_wait = msec;
            }

            sal_msleep(time_wait);
    
            msec -= time_wait;
    
            if (msec == 0) {
                err = -1; /* timeout */
                break;
            }
    
            if ((time_wait *= 2) > 100) {
                time_wait = 100;
            }
        }
    }
    if (err) {
        return -1;
    }
    return 0;
}

int
sal_semaphore_give(sal_semaphore b)
{
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
    ret = PollSema((int)b);
    return ret?0:-1;
}

int 
sal_semaphore_getvalue(sal_semaphore b)
{
    iop_sema_info_t info;
    int ret;
    ret = ReferSemaStatus((int)b, &info);
    if (ret!=0) {
        sal_assert(0);
    }
    return info.current;
}

