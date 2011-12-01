#ifndef _SAL_LINUX_EMB_H_
#define _SAL_LINUX_EMB_H_

#ifndef __KERNEL__
#error Need __KERNEL__ define to enable xplat embedded primitives option.
#endif

/* For embedded system, inline function */

#include <linux/config.h>
#include <linux/version.h>
#include <linux/sched.h> /* for HZ, jiffies */
#include <linux/slab.h> /* for malloc */
#include <linux/wait.h>
#include <linux/mm.h> // kmalloc, GFP_ATOMIC

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#include <linux/interrupt.h>
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0)
#include <asm/softirq.h>
#else
#error Not support linux version.
#endif
#include <linux/spinlock.h>

#define XPLAT_ALLOW_LINUX_HEADER 1

typedef spinlock_t sal_spinlock;

static inline int sal_spinlock_create(const char *desc, sal_spinlock* lock) 
{
    spin_lock_init(lock);
    return 1;
}

#define sal_spinlock_destroy(m) do { } while(0)

#define sal_spinlock_take_irqsave(m, f) spin_lock_irqsave(&(m), *(f))
#define sal_spinlock_give_irqrestore(m, f) spin_unlock_irqrestore(&(m), *(f))

#define sal_spinlock_take_softirq(m) spin_lock_bh(&(m))
#define sal_spinlock_give_softirq(m) spin_unlock_bh(&(m))

#define sal_spinlock_take(m) spin_lock(&(m))
#define sal_spinlock_give(m) spin_unlock(&(m)) 

#define sal_get_tick() (jiffies)
#define SAL_TICKS_PER_SEC (HZ)

#define sal_atomic_inc(v) atomic_inc((atomic_t*)v)
#define sal_atomic_dec(v) atomic_dec((atomic_t*)v)
#define sal_atomic_dec_and_test(v) atomic_dec_and_test((atomic_t*)v)

static inline  void* sal_alloc_from_pool(void* pool, int size)
{
    int kmalloc_flags = 0;
    if (in_interrupt() ) {
        kmalloc_flags |= GFP_ATOMIC;
    } else {
#if LINUX_VERSION_25_ABOVE
        if (in_atomic() || irqs_disabled())
        {
            kmalloc_flags |= GFP_ATOMIC;
        }
        else
#endif
        {
            kmalloc_flags |= GFP_KERNEL;
        }
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
    return kmem_cache_alloc((struct kmem_cache*)pool, kmalloc_flags);
#else
    return kmem_cache_alloc((kmem_cache_t*)pool, kmalloc_flags);
#endif
}

static inline void sal_free_from_pool(void* pool, void* ptr)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
    kmem_cache_free((struct kmem_cache*)pool, ptr);
#else
    kmem_cache_free((kmem_cache_t*)pool, ptr);
#endif
}

#endif /* _SAL_LINUX_EMB_H_ */

