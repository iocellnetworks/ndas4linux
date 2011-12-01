#include <ucos_ii.h>
#include "sal/types.h"
#include "sal/time.h"
#include "syabas.h"
#include "sal/debug.h"

sal_msec
sal_time_msec(void)
{
    INT32U tick;
    sal_msec msec;
    tick = OSTimeGet();
    msec =(sal_msec)((unsigned long long) tick * 1000 / OS_TICKS_PER_SEC);
    return msec;
}

void
sal_msleep(sal_msec msec)
{    
    INT16U tick;
    if (TCPIP_TASK_PRIO == OSTCBCur->OSTCBPrio) {
        sal_debug_print("***Warning: sleep in tcpip task***\r\n");
    }

    if (msec==0)
        return;
    tick = msec * OS_TICKS_PER_SEC / 1000;
    if (tick==0)
        tick = 1;
    OSTimeDly(tick);
}

