/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
#include <tamtypes.h>
#include <thbase.h>
#include "sal/types.h"
#include "sal/time.h"

sal_msec
sal_time_msec(void)
{
    iop_sys_clock_t    t;
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

