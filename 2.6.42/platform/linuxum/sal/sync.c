#include "xplatcfg.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#include <pthread.h>
#include "sal/sync.h"
#include "sal/thread.h"
#include "sal/time.h"
#include "sal/mem.h"
#include "sal/debug.h"
#include "sal/types.h"

#undef TRACE_SYNC_COUNT
#ifdef TRACE_SYNC_COUNT
static int _sal_mutex_count = 0;
static int _sal_sema_count = 0;
#endif
struct mutex_impl {
    pthread_mutex_t mutex;
    const char* desc;
};

NDAS_SAL_API
int 
sal_spinlock_create(const char *desc, sal_spinlock* lock)
{
    int r;
    struct mutex_impl *sm = sal_malloc(sizeof(struct mutex_impl));
    if ( !sm ) return SAL_INVALID_MUTEX;
    sm->desc = desc;
//    sal_debug_print("mutex create:%s\n", desc);
    r = pthread_mutex_init(&sm->mutex,NULL);
    if ( r == 0 ) {
	*lock = sm;
        return 1;
    }
    sal_free(sm);
    return 0;
}

void
sal_spinlock_destroy(sal_spinlock m)
{
    struct mutex_impl *sm = (struct mutex_impl *)m;
    sal_assert(m!=SAL_INVALID_MUTEX);
    pthread_mutex_destroy(&sm->mutex);
//    sal_debug_print("mutex destroy:%s\n", sm->desc);
    sal_free(sm);
}

void
sal_spinlock_take_irqsave(sal_spinlock m, unsigned long* flags)
{
    struct mutex_impl *sm = (struct mutex_impl *)m;
    sal_assert(m!=SAL_INVALID_MUTEX);    
    pthread_mutex_lock(&sm->mutex);
//    v_mutex_lock_count++;
}

void
sal_spinlock_give_irqrestore(sal_spinlock m, unsigned long* flags)
{
    struct mutex_impl *sm = (struct mutex_impl *)m;
    sal_assert(m!=SAL_INVALID_MUTEX);    
//    v_mutex_lock_count--;
    pthread_mutex_unlock(&sm->mutex);
}

NDAS_SAL_API void        sal_spinlock_take(sal_spinlock m)
{
	sal_spinlock_take_irqsave(m, NULL);	
}

NDAS_SAL_API void        sal_spinlock_give(sal_spinlock m)
{
	sal_spinlock_give_irqrestore(m, NULL);
}

sal_semaphore
sal_semaphore_create(const char *desc, int max_count, int initial_count)
{
    sem_t        *s;
#ifdef TRACE_SYNC_COUNT
    _sal_sema_count ++;
    if (strcmp(desc,"debug_sema")!=0)
        sal_debug_print("sema_create %s(%dth)\n", desc, _sal_sema_count);
#endif
    /* Currently support binary semaphore only */ 
    sal_assert(max_count==1);
    sal_assert(initial_count>=0 && initial_count<=max_count);

    if ((s = sal_malloc(sizeof (sem_t)+16)) != 0) {
        int ret;
        ret = sem_init(s, 0, initial_count);
        if (ret!=0) {
            sal_debug_print("sem_init failed\n");
        }
        strncpy(((char*)s)+sizeof(sem_t), desc, 16);
        (((char*)s)+sizeof(sem_t))[15] = 0;
    }

    return (sal_semaphore) s;
}

void
sal_semaphore_destroy(sal_semaphore b)
{
    sem_t        *s = (sem_t *) b;

#ifdef TRACE_SYNC_COUNT
    _sal_sema_count--;
    sal_debug_print("sema_destroy %d remain\n", _sal_sema_count);
#endif
    sal_assert(b!=SAL_INVALID_SEMAPHORE);
    if (sal_semaphore_getvalue(b)==0) {
        sal_debug_print("Semaphore %p is destroyed while taken: %s\n", (void*)s, ((char*)s)+sizeof(sem_t));
    }
    sem_destroy(s);

    sal_free(s);
}


int
sal_semaphore_take(sal_semaphore b, sal_tick tick)
{
    sem_t        *s = (sem_t *) b;
    int            err=0;
//    if (v_mutex_lock_count) {
//        sal_assert(0);
//    }
    sal_assert(b!=SAL_INVALID_SEMAPHORE);
    if (tick == SAL_SYNC_FOREVER) {
        err = sem_wait(s);
    } else {
        sal_tick    time_wait = 1;

        /* Retry algorithm with exponential backoff */

        for (;;) {
            if (sem_trywait(s) == 0) {
                err = 0;
                break;
            }

            if (errno != EAGAIN) {
                err = errno;
                break;
            }

            if (time_wait > tick) {
                time_wait = tick;
            }
            sal_msleep(time_wait * 1000 / SAL_TICKS_PER_SEC);

            tick -= time_wait;

            if (tick == 0) {
                err = ETIMEDOUT;
                break;
            }

            if ((time_wait *= 2) > 100) {
                time_wait = SAL_TICKS_PER_SEC/20; /* MAX 50ms */
            }
        }
    }

    return err ? SAL_SYNC_TIMEOUT : SAL_SYNC_OK;
}

int
sal_semaphore_give(sal_semaphore b)
{
    sem_t        *s = (sem_t *) b;
    int            err;
    sal_assert(b!=SAL_INVALID_SEMAPHORE);
    err = sem_post(s);

    return err ? SAL_SYNC_ERR : SAL_SYNC_OK;
}

int 
sal_semaphore_getvalue(sal_semaphore b)
{
    sem_t    *s = (sem_t*) b;
    int err;
    int val;
    sal_assert(b!=SAL_INVALID_SEMAPHORE);    
    err = sem_getvalue(s, &val);
    if (err!=0) {
        sal_assert(0);
    }
    return val;
}
