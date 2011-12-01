#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <ucos_ii.h>
#include "syabas.h"
#include "sal/types.h"
#include "sal/thread.h"
#include "sal/time.h"
#include "sal/debug.h"
#include "sal/mem.h"

#define NETDISK_PRIO_HIGHEST    NETDISK_TASK_PRIO
#define NETDISK_PRIO_LOWEST     NETDISK_TASK_PRIO
#define MAX_NO_OF_THREAD (NETDISK_PRIO_LOWEST - NETDISK_PRIO_HIGHEST +1)

static OS_STK* v_thread_stack_table[MAX_NO_OF_THREAD] = {0};


/* Currently netdisk use single thread
    Map this priority to reasonable value 
*/
int 
sal_thread_create(
    sal_thread_id* tid, char* name, int prio, int stacksize,
        sal_thread_func f, void* arg)
{
    INT8U    ret;
    OS_STK* stack;
    OS_STK* ptos;
    INT8U    ucos_prio;
    sal_assert(-2<=prio && prio<=2);
    ucos_prio = NETDISK_PRIO_HIGHEST ;
        
    if (stacksize==0)
        stacksize = 4 * 1024;
    stack = (OS_STK*) sal_malloc(stacksize);
    if (OS_STK_GROWTH==1) {
        ptos =stack+stacksize/sizeof(OS_STK)-1;
    } else {
        ptos = stack;
    }
    while(1) {
        ret = OSTaskCreate((void(*)(void*)) f, arg, ptos, ucos_prio);
        if (ret==OS_NO_ERR) {
            sal_debug_print("OSTaskCreate %s with priority %d->%d\r\n", name, prio, ucos_prio);
            break;
        } else if (ret == OS_PRIO_EXIST) {
            ucos_prio++;
        } else {
            sal_assert(0);
            sal_free(ptos);
            return -1;
        }
        if (ucos_prio > NETDISK_PRIO_LOWEST) {
            sal_assert(0);
            sal_free(ptos);
            return -1;
        }
    }
    *tid = (sal_thread_id)(xuint32)ucos_prio;
    if (v_thread_stack_table[ucos_prio-NETDISK_PRIO_HIGHEST]!=0) {
        sal_free((char*)v_thread_stack_table[ucos_prio-NETDISK_PRIO_HIGHEST]);
    }
    v_thread_stack_table[ucos_prio-NETDISK_PRIO_HIGHEST] = stack;
    return 0;
}

int
sal_thread_yield(void)
{
    return 0;
}

sal_thread_id 
sal_thread_self(void)
{
    return (sal_thread_id)(xuint32)OSTCBCur->OSTCBPrio;
}

/* No way to free stack memory. Free stack memory when the thread is reused */
void
sal_thread_exit(int ret)
{
    INT8U err;
    err = OSTaskDel(OS_PRIO_SELF);
    if (err!=OS_NO_ERR)
        sal_assert(0);
}

