#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "sal/types.h"
#include "sal/thread.h"
#include "sal/time.h"
#include "sal/debug.h"


//#define USE_PROFILER 1

#ifdef USE_PROFILER 
struct thread_func_wrap_arg {
    sal_thread_func* f;
    void* arg;
};

extern void ProfilerRegisterThread(void);


void* sal_thread_func_wrapper(void* arg)
{
    struct thread_func_wrap_arg* warg = (struct thread_func_wrap_arg*) arg;
    void* ret;
//    ProfilerRegisterThread();
    ret = warg->f(warg->arg);
    free(arg);
    return ret;
}
#endif

int 
sal_thread_create(
    sal_thread_id* tid, char* name, int prio, int stacksize,
    sal_thread_func f, void* arg)
{
    pthread_attr_t    attr;
    pthread_t        id;
    if (pthread_attr_init(&attr)) {
        return -1;
    }
    if (prio!=0) {
        /* Not implemented. Just ignore */
    }

    if (stacksize!=0) {
        /* Not implemented. Just ignore */
    }

#ifdef USE_PROFILER 
    {
        struct thread_func_wrap_arg* warg = (struct thread_func_wrap_arg*)malloc(sizeof(struct thread_func_wrap_arg));
        warg->f = f;
        warg->arg = arg;
        if (pthread_create(&id, &attr, (void *)sal_thread_func_wrapper, (void *)warg)) {
            return -1;
        }
    }
#else
    if (pthread_create(&id, &attr, (void *)f, (void *)arg)) {
        return -1;
    }
#endif    
    sal_debug_print("Thread %s created with id %x\n", name, (int)id);
    if (tid) {
        *tid = (sal_thread_id)id;
    }
    return 0;
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
    return (sal_thread_id) pthread_self();
}

void
sal_thread_exit(int ret)
{
    pthread_exit((void *)((long)ret));
}

