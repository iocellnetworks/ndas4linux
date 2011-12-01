/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
#include <tamtypes.h>
#include <thbase.h>
#include "sal/types.h"
#include "sal/time.h"
#include <timer.h>

xuint32 SAL_TICKS_PER_SEC = 1000;

sal_msec
sal_time_msec(void)
{
    struct timestamp    t;
    int usec, sec;
    sal_msec msec;
    
    GetSystemTime(&t);
    SysClock2USec(&t, &sec, &usec);
    msec = ((sal_msec)sec * 1000) + (usec/1000);
    return msec;    
}

void
sal_msleep(sal_msec msec)
{
    DelayThread(msec * 1000);
}

NDAS_SAL_API sal_tick sal_get_tick(void)
{
    struct timestamp    t;
    int usec, sec;
    sal_msec msec;
    
    GetSystemTime(&t);
    SysClock2USec(&t, &sec, &usec);
    msec = ((sal_msec)sec * 1000) + (usec/1000);
    return msec;
}

