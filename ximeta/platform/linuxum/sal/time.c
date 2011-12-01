#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include "sal/types.h"
#include "sal/time.h"
#include "sal/debug.h"

// Set 1 tick to 10ms 
xuint32 SAL_TICKS_PER_SEC = 100;

sal_tick 
sal_get_tick(void)
{
    struct timeval    ltv;
    unsigned long long lt;
    gettimeofday(&ltv, 0);
    lt = (unsigned long long)ltv.tv_sec * 100 + ltv.tv_usec/10000;
    return (sal_tick)lt;
}

sal_msec
sal_time_msec(void)
{
    struct timeval    ltv;
    unsigned long long lt;
    gettimeofday(&ltv, 0);
    lt = (unsigned long long)ltv.tv_sec * 1000 + ltv.tv_usec/1000;
    return (sal_msec)lt;    
}

void
sal_msleep(sal_msec msec)
{
    struct timeval tv;

    tv.tv_sec = (time_t) (msec / 1000);
    tv.tv_usec = (long) (msec % 1000)*1000;
    select(0, (fd_set *) 0, (fd_set *) 0, (fd_set *) 0, &tv);
}

