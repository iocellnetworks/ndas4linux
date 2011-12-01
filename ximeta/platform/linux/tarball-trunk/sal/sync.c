/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
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
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#include <linux/version.h> // LINUX_VERSION
#include <linux/module.h> // EXPORT_SYMBOL
#include <linux/spinlock.h> // spinklock_t
#include <linux/sched.h> // jiffies
#include <asm/semaphore.h> // struct semaphore
#include <asm/atomic.h> // atomic
#include <linux/mm.h> //
#include <linux/slab.h> // 2.4 kmalloc/kfree
#include <linux/interrupt.h>

#include "sal/sync.h"
#include "sal/thread.h"
#include "sal/time.h"
#include "sal/mem.h"
#include "sal/debug.h"
#include "sal/types.h"
#include "linux_ver.h" // SAL_HZ

#ifdef DEBUG
#ifndef DEBUG_LEVEL_SAL_SYNC
#ifdef DEBUG_LEVEL
#define DEBUG_LEVEL_SAL_SYNC DEBUG_LEVEL
#else
#define DEBUG_LEVEL_SAL_SYNC 1
#endif
#endif
#define dbgl_salsync(l,x...) do {\
    if ( l <= DEBUG_LEVEL_SAL_SYNC ) {\
        sal_debug_print("SS|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x); \
    } \
} while(0) 
#else
#define dbgl_salsync(l,x...) do {} while(0)
#endif
#define SAL_MUTEX_MAGIC 0x9832abcd
#define SAL_SEMA_MAGIC 0x9832abcd
#define SAL_EVENT_MAGIC 0x9832abcd

/*
 * backport schedule_timeout() for linux 2.4
 */

#if !LINUX_VERSION_25_ABOVE

static void
_sal_process_timeout(unsigned long __data)
{
    wake_up_process((struct task_struct *)__data);
}

long schedule_timeout(signed long timeout)
{
    struct timer_list timer;
    unsigned long expire;

    switch (timeout)
    {
    case LONG_MAX:
        /*
        * These two special cases are useful to be comfortable
        * in the caller. Nothing more. We could take
        * MAX_SCHEDULE_TIMEOUT from one of the negative value
        * but I' d like to return a valid offset (>=0) to allow
        * the caller to do everything it want with the retval.
        */
        schedule();
        goto out;
    default:
        /*
        * Another bit of PARANOID. Note that the retval will be
        * 0 since no piece of kernel is supposed to do a check
        * for a negative retval of schedule_timeout() (since it
        * should never happens anyway). You just have the printk()
        * that will tell you if something is gone wrong and where.
        */
        if (timeout < 0) {
            printk(KERN_ERR "schedule_timeout: wrong timeout "
                    "value %lx\n", timeout);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20))
            dump_stack();
#endif
            current->state = TASK_RUNNING;
            goto out;
        }
    }

    expire = timeout + jiffies;

    init_timer(&timer);
    timer.data = (unsigned long)current;
    timer.function = _sal_process_timeout;
    mod_timer(&timer, expire);
    schedule();
    del_timer_sync(&timer);

    timeout = expire - jiffies;

out:
    return timeout < 0 ? 0 : timeout;
}
#endif

/*
 * Mutex ( spinlock in linux kernel )
 */

//#define DEBUG_TRACE_SEMAPHORE 1

#ifndef XPLAT_ALLOW_LINUX_HEADER
struct _sal_spinlock {
#undef MAGIC 
#ifdef MAGIC
    int magic;
#endif
    spinlock_t    mutex;
    const char        *desc;
};

/* Return 0 for error */
NDAS_SAL_API
int
sal_spinlock_create(const char *desc, sal_spinlock* lock)
{
    struct _sal_spinlock    *sm;

    if ((sm = kmalloc(sizeof (struct _sal_spinlock), GFP_ATOMIC)) == NULL) {
        return 0;
    }

    dbgl_salsync(5, "%p %s", sm, desc);
    
    sm->desc = desc;
#ifdef MAGIC
    sm->magic = SAL_MUTEX_MAGIC;
#endif
    spin_lock_init(&sm->mutex);
    *lock = sm;
    return 1;
}
EXPORT_SYMBOL(sal_spinlock_create);

NDAS_SAL_API
void
sal_spinlock_destroy(sal_spinlock m)
{
    struct _sal_spinlock    *sm;
    sm = (struct _sal_spinlock*) m;  
    sal_assert(m!=SAL_INVALID_MUTEX);
#ifdef    MAGIC
    sal_assert(m->magic == SAL_MUTEX_MAGIC);
#endif
#ifdef DEBUG
    /* check if locked and print debugging info */
    dbgl_salsync(5, "%s %p",sm->desc, sm);    
    sal_assert( !spin_is_locked(&sm->mutex) );
#endif
    kfree(sm);
}
EXPORT_SYMBOL(sal_spinlock_destroy);

/* return -1 for timeout, 0 for successful mutex taking */
NDAS_SAL_API
void
sal_spinlock_take_irqsave(sal_spinlock m, unsigned long* flags)
{
    struct _sal_spinlock    *sm = (struct _sal_spinlock*) m;
   
    sal_assert(m!=SAL_INVALID_MUTEX);
#ifdef MAGIC
    sal_assert(m->magic == SAL_MUTEX_MAGIC);
#endif

    dbgl_salsync(5, "ing(%s)",sm->desc);
    spin_lock_irqsave(&sm->mutex,*flags);
    dbgl_salsync(5, "ed(%s)",sm->desc);
}
EXPORT_SYMBOL(sal_spinlock_take_irqsave);

NDAS_SAL_API
void
sal_spinlock_give_irqrestore(sal_spinlock m, unsigned long* flags)
{
    struct _sal_spinlock    *sm = (struct _sal_spinlock*) m;
    while(0) {sm = sm;} /* sm may not used in some kernel configuration */

    sal_assert(m!=SAL_INVALID_MUTEX);
#ifdef MAGIC
    sal_assert(m->magic == SAL_MUTEX_MAGIC);
#endif
    dbgl_salsync(5, "ing (%s)",(sm->desc));
    spin_unlock_irqrestore(&sm->mutex, *flags);
    dbgl_salsync(5, "ed (%s)",(sm->desc));
}
EXPORT_SYMBOL(sal_spinlock_give_irqrestore);

/* Mostly used for synchronization between software interrupts.
 sal_spinlock_take_irqsave should be used for sharing data with interrupt context */
NDAS_SAL_API void        sal_spinlock_take_softirq(sal_spinlock m)
{
    struct _sal_spinlock    *sm = (struct _sal_spinlock*) m;
    spin_lock_bh(&sm->mutex);
}
EXPORT_SYMBOL(sal_spinlock_take_softirq);

NDAS_SAL_API void        sal_spinlock_give_softirq(sal_spinlock m)
{
    struct _sal_spinlock    *sm = (struct _sal_spinlock*) m;
    spin_unlock_bh(&sm->mutex);
}
EXPORT_SYMBOL(sal_spinlock_give_softirq);

/* Mostly used for synchronization between thread.
 sal_spinlock_take_irqsave should be used for sharing data with hardware interrupt context */
NDAS_SAL_API void        sal_spinlock_take(sal_spinlock m)
{
    struct _sal_spinlock    *sm = (struct _sal_spinlock*) m;
    spin_lock(&sm->mutex);
}
EXPORT_SYMBOL(sal_spinlock_take);

NDAS_SAL_API void        sal_spinlock_give(sal_spinlock m)
{
    struct _sal_spinlock    *sm = (struct _sal_spinlock*) m;
    spin_unlock(&sm->mutex);
}
EXPORT_SYMBOL(sal_spinlock_give);

#endif /* XPLAT_ALLOW_LINUX_HEADER */

struct _sal_semaphore {
    struct semaphore sema;
    int max_count;
    const char* desc;
};

NDAS_SAL_API
sal_semaphore
sal_semaphore_create(const char *desc, int max_count, int initial_count)
{
    struct _sal_semaphore        *s;

    sal_assert(initial_count<=max_count);

    if ((s = kmalloc(sizeof(struct _sal_semaphore),GFP_ATOMIC)) != 0) {
        sema_init(&s->sema, initial_count);
        s->max_count = max_count;
        s->desc = desc;
    }
    sal_assert(s != SAL_INVALID_SEMAPHORE);

    return (sal_semaphore) s;
}
EXPORT_SYMBOL(sal_semaphore_create);

NDAS_SAL_API
void
sal_semaphore_destroy(sal_semaphore b)
{
    struct _sal_semaphore        *s = (struct _sal_semaphore *) b;
    sal_assert(b!=SAL_INVALID_SEMAPHORE);
    kfree(s);
}
EXPORT_SYMBOL(sal_semaphore_destroy);

/*
 * Take one semaphore count.
 *
 * NOTE: Timeout support will be deprecated.
 *   If timeout is specified, the function will poll the semaphore.
 *   It will degrade system performance significantly, so do not use time-out.
 *
 * Returns:
 *   SAL_SYNC_OK for successful semaphore taking 
 *   SAL_SYNC_TIMEOUT for timeout
 *   SAL_SYNC_INTERRUPTED if wait function is interrupted by signal
 */

 #define SAL_SEMA_POLLING_PERIOD    10 // jiffies
 
NDAS_SAL_API
int
sal_semaphore_take(sal_semaphore b, sal_tick tick)
{
    struct _sal_semaphore   * s = (struct _sal_semaphore *) b;
    long target_jiffies;
    int ret, salret;

    sal_assert(b != SAL_INVALID_SEMAPHORE);
    sal_assert(b != (sal_semaphore) 0xc);
    sal_assert(tick == SAL_SYNC_NOWAIT  || !in_interrupt());

    // 1. Take the semaphore without a wait.
    if(tick == SAL_SYNC_NOWAIT) {
        ret = down_trylock(&s->sema);
        if(ret != 0) {
            return SAL_SYNC_ERR;
        }
        return SAL_SYNC_OK;

    // 2. Take the semaphore with a wait.
    } else if(tick == SAL_SYNC_FOREVER) {
        ret = down_interruptible(&s->sema);
        if(ret != 0) {
            return SAL_SYNC_INTERRUPTED;
        } else {
            return SAL_SYNC_OK;
        }
    }

    // 3. Take the semaphor with a timeout.
    //    Very slow. Will be deprecated.
    target_jiffies = jiffies + tick;
    salret = SAL_SYNC_OK;
    for(;;) {
        ret = down_trylock(&s->sema);
        if(ret != 0) {
            // If a signal is pending, exit with interrupted error.
            if(signal_pending(current)) {
                salret = SAL_SYNC_INTERRUPTED;
                break;
            }
            // Sleep for polling period.
            schedule_timeout(SAL_SEMA_POLLING_PERIOD);
            // Timeout check
            if(target_jiffies <= jiffies) {
                salret = SAL_SYNC_TIMEOUT;
                break;
            }
            // Try again.
            continue;
        }
        // semaphore down succeeds.
        break;
    }

    return salret;
}
EXPORT_SYMBOL(sal_semaphore_take);

NDAS_SAL_API int        sal_semaphore_give(sal_semaphore b)
{
    struct _sal_semaphore   * s = (struct _sal_semaphore *) b;
    up(&s->sema);
    return SAL_SYNC_OK;
}

EXPORT_SYMBOL(sal_semaphore_give);

NDAS_SAL_API int sal_semaphore_getvalue(sal_semaphore b)
{
    struct _sal_semaphore   * s = (struct _sal_semaphore *) b;

#if LINUX_VERSION_25_ABOVE || LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20)

#if LINUX_VERSION_NEW_SEMAPHORE 
    return s->sema.count;
#else
    return atomic_read(&s->sema.count);
#endif //#if LINUX_VERSION_NEW_SEMAPHORE

#else
    return sem_getcount(&s->sema);
#endif
}
EXPORT_SYMBOL(sal_semaphore_getvalue);

/*
 * Event
 */
 
 struct _sal_event {
    atomic_t        state;
    wait_queue_head_t queue;
    const char * desc;
#ifdef MAGIC
    int magic;
#endif
 };
 
NDAS_SAL_API
sal_event
sal_event_create(const char *desc)
{
    struct _sal_event    *sevent;

    if ((sevent = kmalloc(sizeof (struct _sal_event), GFP_ATOMIC)) == NULL) {
        return NULL;
    }

    dbgl_salsync(5, "%p %s", sevent, desc);
    
    sevent->desc = desc;
#ifdef MAGIC
    sevent->magic = SAL_MUTEX_MAGIC;
#endif
    init_waitqueue_head(&sevent->queue);
    atomic_set(&sevent->state, 0);

    return (sal_event)sevent;
}
EXPORT_SYMBOL(sal_event_create);

NDAS_SAL_API
void
sal_event_destroy(sal_event event)
{
    struct _sal_event    *sevent = (struct _sal_event*) event;
    sal_assert(sevent!=SAL_INVALID_EVENT);
#ifdef    MAGIC
    sal_assert(m->magic == SAL_EVENT_MAGIC);
#endif

    dbgl_salsync(5, "%s %p",sevent->desc, sevent);
    sal_assert( !waitqueue_active(&sevent->queue) );

    // check if locked and print debugging info
    kfree(sevent);
}
EXPORT_SYMBOL(sal_event_destroy);


/*
 * Implements notification event.
 * Caller should be aware of return by signal pending.
 * timeout: the number of jiffies to wait, not an absolute time value
 */

NDAS_SAL_API
int
sal_event_wait(sal_event event, sal_tick timeout)
{
    struct _sal_event    *sevent = (struct _sal_event*) event;
    long timeleft = timeout;
    int ret = SAL_SYNC_OK;
    wait_queue_t wait;

#if LINUX_VERSION_25_ABOVE
    if(in_interrupt() || in_atomic() || irqs_disabled()) {
#else
    if(in_interrupt()) {
#endif

#ifdef DEBUG
        *(char *)0 = 0;
#endif
        printk(KERN_ERR "Tried wait in interrupt context.\n");
        return SAL_SYNC_ERR;
    }

#if LINUX_VERSION_25_ABOVE
    // Initialze the wait.
    init_wait(&wait);
#else
    // Initialize the wait
    init_waitqueue_entry(&wait, current);
    // Add the wait to the queue
    add_wait_queue(&sevent->queue, &wait);
#endif

    for(;;)
    {
#if LINUX_VERSION_25_ABOVE
        // Add the wait to the queue if the wait is not added.
        // Set current process state to TASK_INTERRUPTIBLE
        prepare_to_wait(&sevent->queue, &wait, TASK_INTERRUPTIBLE);
#else
        // Set current process state to TASK_INTERRUPTIBLE
        set_current_state(TASK_INTERRUPTIBLE);
#endif
        // See if the state is set.
        if(atomic_read(&sevent->state))
            break;
        if(!signal_pending(current)) {
            if( timeout == SAL_SYNC_FOREVER) {
                schedule();
                continue;
            }
            else
            {
                timeleft = schedule_timeout(timeleft);
                // See if timeout.
                if(!timeleft) {
                    ret = SAL_SYNC_TIMEOUT;
                    break;
                }
                continue;
            }
        }
        ret = SAL_SYNC_INTERRUPTED;
        break;
    }
#if LINUX_VERSION_25_ABOVE
    // Set current process state to TASK_RUNNING
    // Remove the wait from the queue.
    finish_wait(&sevent->queue, &wait);
#else
    // Set current process state to TASK_RUNNING
    current->state = TASK_RUNNING;
    // Remove the wait from the queue.
    remove_wait_queue(&sevent->queue, &wait);
#endif

    return ret;
}

EXPORT_SYMBOL(sal_event_wait);


NDAS_SAL_API
void
sal_event_set(sal_event event)
{
    struct _sal_event    *sevent = (struct _sal_event*) event;
    atomic_set(&sevent->state, 1);
    wake_up_interruptible(&sevent->queue);
}

EXPORT_SYMBOL(sal_event_set);


NDAS_SAL_API
void
sal_event_reset(sal_event event)
{
    struct _sal_event    *sevent = (struct _sal_event*) event;
    atomic_set(&sevent->state, 0);
}

EXPORT_SYMBOL(sal_event_reset);

NDAS_SAL_API 
void 
sal_atomic_inc(sal_atomic *v)
{
    /* NDAS SAL uses same struct for atomic structure */
    atomic_inc((atomic_t*)v);
}
EXPORT_SYMBOL(sal_atomic_inc);
    
NDAS_SAL_API 
void 
sal_atomic_dec(sal_atomic *v)
{
    atomic_dec((atomic_t*)v);
}
EXPORT_SYMBOL(sal_atomic_dec);

/* Return 1 if counter is 0 after decremnt */
NDAS_SAL_API 
int 
sal_atomic_dec_and_test(sal_atomic *v)
{
    return atomic_dec_and_test((atomic_t*)v);
}
EXPORT_SYMBOL(sal_atomic_dec_and_test);

