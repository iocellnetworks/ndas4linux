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
#ifndef _sal_TIME_H_
#define _sal_TIME_H_

#include "sal/sal.h"
#include "sal/types.h"

typedef xuint32 sal_msec;
typedef xuint32 sal_tick;
#define SAL_MSEC_PER_SEC 1000

/* Wrap-around is considered 
        as long as the gap between time is less than 0x7ffffff. 
    Must use sal_tick_sub whenever comparing sal_tick. */
#define sal_tick_sub(t1, t2) ((xint32)((t1)-(t2)))
#define sal_tick_add(t1, t2) ((t1)+(t2))

NDAS_SAL_API extern sal_msec sal_time_msec(void);
NDAS_SAL_API extern void sal_msleep(sal_msec msec);

#if defined(UCOSII)
#define sal_get_tick()  ((sal_tick)OSTimeGet())
#define SAL_TICKS_PER_SEC OS_TICKS_PER_SEC
#elif defined(XPLAT_ALLOW_LINUX_HEADER)

#else
NDAS_SAL_API extern sal_tick sal_get_tick(void);
extern xuint32 SAL_TICKS_PER_SEC;
#endif


#endif
