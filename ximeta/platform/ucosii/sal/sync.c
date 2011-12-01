#include <ucos_ii.h>
#include "sal/types.h"
#include "sal/sync.h"
#include "sal/debug.h"
#include "sal/thread.h"
#include "sal/libc.h"
#include "proc.h" /* in sdk/tcpip/include */
#include "syabas.h"

#define DEBUG_LEVEL_SAL_SYNC 0
#ifdef DEBUG
#define debug_sync(l, x...) do { \
    if ( l <= DEBUG_LEVEL_SAL_SYNC ) { \
        sal_debug_print("SY|%d|%s|",l,__FUNCTION__);\
        sal_debug_println(x);\
    }\
} while(0)
#else
#define debug_sync(l, x...) do {} while(0)
#endif

static int v_sal_sema_count = 0;

void sal_sync_init(void)
{
}

struct _sal_semaphore {
    OS_EVENT* evt;
    int max_count;
    int count;
    struct proc* p;
    volatile int in_tcpip_thread;
    char desc[16];
};

sal_semaphore
sal_semaphore_create(const char *desc, int max_count, int initial_count)
{
    sal_semaphore sema = (sal_semaphore)sal_malloc(sizeof(struct _sal_semaphore));
    ++v_sal_sema_count;
    debug_sync(0, "sal_semaphore_create %s(total count=%d)\n", desc, v_sal_sema_count);
    sema->evt = OSSemCreate(initial_count);
    sema->in_tcpip_thread = 0;
    sema->max_count = max_count;
    sema->count = 0;
    sema->p = NULL;
    sal_strcpy(sema->desc, desc);
    sal_assert(sema->evt);
    if (sema->evt) {
        return (sal_semaphore)sema;
    } else {
        return SAL_INVALID_SEMAPHORE;
    }
}

void
sal_semaphore_destroy(sal_semaphore b)
{
    INT8U err;
    OS_EVENT* ret;
    sal_assert(b!=SAL_INVALID_SEMAPHORE);
    v_sal_sema_count--;
    if (b->in_tcpip_thread) {
        ksignal((void*)b, 0);
    }
//    sal_debug_print("sal_semaphore_destroy %x\r\n", b);
    debug_sync(0, "sal_semaphore_destroy %s(total count=%d)\n", b->desc, v_sal_sema_count);

    ret = OSSemDel( b->evt, OS_DEL_ALWAYS, &err);
    if (err==OS_NO_ERR && ret==NULL) {
        sal_free(b);
        return;
    }
    sal_assert(0);
}

extern OS_EVENT* sem_vod;
extern INT32 vod_play_state;
extern INT32 vod_command;
/* Return -1 for timeout, 0 for successful semaphore taking */
int
sal_semaphore_take(sal_semaphore b, sal_tick tick)
{
    INT8U err;
    INT16U cnt;
    int timeout = sal_tick_add(sal_get_tick(), tick);
    sal_assert(b!=SAL_INVALID_SEMAPHORE);
    debug_sync(2, "take %s, prio=%d", b->desc, OSPrioCur);

    if (TCPIP_TASK_PRIO == OSTCBCur->OSTCBPrio) {
        while(1) {
            cnt = OSSemAccept((OS_EVENT*)b->evt);
            if (cnt==0) {
                if (SAL_SYNC_FOREVER !=tick && sal_tick_sub(sal_get_tick(), timeout)>0) {
                    debug_sync(1, "timeout in tcpip task %s, prio=%d, proc=%p", b->desc, OSPrioCur, Curproc);
                    return -1;
                }
                b->in_tcpip_thread++;
                b->p = Curproc;
                if (SAL_SYNC_FOREVER !=tick) {
                    debug_sync(1, "ppause in tcpip task %s, prio=%d, proc=%p, event=%p", b->desc, OSPrioCur, Curproc, (void*)b);
                    ppause(tick * 1000 / OS_TICKS_PER_SEC);
                    debug_sync(1, "ppause returned in tcpip task %s, prio=%d, proc=%p", b->desc, OSPrioCur, Curproc);
                } else {
                    debug_sync(1, "kwait in tcpip task %s, prio=%d, proc=%p, event=%p", b->desc, OSPrioCur, Curproc, (void*)b);
                    kwait((void*)b);
                    debug_sync(1, "kwait returned in tcpip task %s, prio=%d, proc=%p", b->desc, OSPrioCur, Curproc);
                }
                continue;
            } else {
                debug_sync(1, "wakeup in tcpip task %s, prio=%d, proc=%p", b->desc, OSPrioCur, Curproc);
                b->count--;/* Decreased the semaphore count successfully */
                return 0;
            }
        }
    } else {
        if(sem_vod->OSEventCnt == 0)
        {
            OSTimeDly(2);
            OSSemPost(sem_vod);
        }
        OSSemPend((OS_EVENT*)b->evt, tick, &err);
        if (err == OS_NO_ERR) {
            b->count--;
            debug_sync(2, "wakeup in %s, prio=%d", b->desc, OSPrioCur);
            return 0;
        } else if (err == OS_TIMEOUT) {
            debug_sync(2, "timeout in %s, prio=%d", b->desc, OSPrioCur);
            return -1;
        } else {
            debug_sync(2, "sal_semaphore_take failed: %d\n", (int)err);
            return -1;
        }
    }
}

int sal_semaphore_give(sal_semaphore b)
{
    INT8U ret;
    sal_assert(b!=SAL_INVALID_SEMAPHORE);

    if (b->count>=b->max_count)
        return 0;
    b->count++;

    if (b->in_tcpip_thread>0) {
        debug_sync(1, "give to tcpip task(%s) from prio %d, proc=%p", b->desc, OSPrioCur, Curproc);
        b->in_tcpip_thread--;
        NOS_alert(b->p, EALARM);
    }
    ret = OSSemPost((OS_EVENT*) b->evt);
    debug_sync(2, "posted(%s) from prio %d", b->desc, OSPrioCur);

    if (ret==OS_NO_ERR) {
        return 0;
    } else {
        debug_sync(2, "sal_semaphore_give failed: %d", (int)ret);
        return -1;
    }
}


int sal_semaphore_getvalue(sal_semaphore b)
{
    INT8U ret;
    OS_SEM_DATA data;
    sal_assert(b!=SAL_INVALID_SEMAPHORE);

    ret = OSSemQuery((OS_EVENT*) b->evt, &data);

    if (ret==OS_NO_ERR) {
        return (int)data.OSCnt;
    } else {
        sal_assert(0);
        return -1;
    }
}

