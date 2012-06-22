#ifndef __NDAS_TYPES_H__
#define __NDAS_TYPES_H__

#include <asm/byteorder.h>

#if		defined(__LITTLE_ENDIAN)
#elif 	defined(__BIG_ENDIAN)
#else
#error "byte endian is not specified"
#endif 

#if (LINUX_VERSION_CODE <  KERNEL_VERSION(2,4,20))
#define BUG_ON(x)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))

#include <linux/stddef.h>
#include <linux/fs.h>

#else

typedef enum {

	false	= 0,
	true	= 1
} bool;

#include <linux/fs.h>

static inline void inc_nlink(struct inode *inode)
{
	inode->i_nlink++;
}

static inline void drop_nlink(struct inode *inode)
{
	inode->i_nlink--;
}

static inline void clear_nlink(struct inode *inode)
{
	inode->i_nlink = 0;
}

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
static inline unsigned iminor(const struct inode *inode)
{
    return minor(inode->i_rdev);
}
#else
static inline unsigned iminor(const struct inode *inode)
{
    return MINOR(inode->i_rdev);
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else
typedef unsigned long long sector_t;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
#include <linux/log2.h>
#else

static inline __attribute__((const))
bool is_power_of_2(unsigned long n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}

#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15))
#include <linux/timer.h>
#include <linux/slab.h>
#else

#include <linux/timer.h>

static inline void setup_timer(struct timer_list * timer,
				void (*function)(unsigned long),
				unsigned long data)
{
	timer->function = function;
	timer->data = data;
	init_timer(timer);
}

#include <linux/slab.h>
#define kmem_cache	kmem_cache_s

#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11))
#include <linux/timer.h>
#else

#include <linux/timer.h>
#define CURRENT_TIME_SEC	CURRENT_TIME

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

#include <linux/module.h>
#include <linux/types.h>
#include <linux/list.h>

#else

#include <linux/module.h>
#include <linux/types.h>

#define __be16	__u16
#define __be32	__u32

#define __le16	__u16
#define __le32	__u32

#define container_of(ptr, type, member) ({			\
	const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
	(type *)( (char *)__mptr - offsetof(type,member) );})

#define typecheck(type,x) \
({	type __dummy; \
	typeof(x) __dummy2; \
	(void)(&__dummy == &__dummy2); \
	1; \
})

#endif

#include <linux/list.h>

#ifndef list_for_each_entry

#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		     prefetch(pos->member.next);			\
	     &pos->member != (head); 					\
	     pos = list_entry(pos->member.next, typeof(*pos), member),	\
		     prefetch(pos->member.next))

#endif

#ifndef list_for_each_entry_safe

#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

#include <linux/time.h>
#include <linux/delay.h>
#include <linux/sched.h>

#else

#define MSEC_PER_SEC	1000L

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0) || LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,27))

static unsigned int inline jiffies_to_msecs(const unsigned long j)
{
#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
	return (MSEC_PER_SEC / HZ) * j;
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
	return (j + (HZ / MSEC_PER_SEC) - 1)/(HZ / MSEC_PER_SEC);
#else
	return (j * MSEC_PER_SEC) / HZ;
#endif
}

static unsigned long msecs_to_jiffies(const unsigned int m)
{
	/*
	 * Negative value, means infinite timeout:
	 */
	if ((int)m < 0)
		return MAX_JIFFY_OFFSET;

#if HZ <= MSEC_PER_SEC && !(MSEC_PER_SEC % HZ)
	/*
	 * HZ is equal to or smaller than 1000, and 1000 is a nice
	 * round multiple of HZ, divide with the factor between them,
	 * but round upwards:
	 */
	return (m + (MSEC_PER_SEC / HZ) - 1) / (MSEC_PER_SEC / HZ);
#elif HZ > MSEC_PER_SEC && !(HZ % MSEC_PER_SEC)
	/*
	 * HZ is larger than 1000, and HZ is a nice round multiple of
	 * 1000 - simply multiply with the factor between them.
	 *
	 * But first make sure the multiplication result cannot
	 * overflow:
	 */
	if (m > jiffies_to_msecs(MAX_JIFFY_OFFSET))
		return MAX_JIFFY_OFFSET;

	return m * (HZ / MSEC_PER_SEC);
#else
	/*
	 * Generic case - multiply, round and divide. But first
	 * check that if we are doing a net multiplication, that
	 * we wouldnt overflow:
	 */
	if (HZ > MSEC_PER_SEC && m > jiffies_to_msecs(MAX_JIFFY_OFFSET))
		return MAX_JIFFY_OFFSET;

	return (m * HZ + MSEC_PER_SEC - 1) / MSEC_PER_SEC;
#endif
}

#include <linux/sched.h>

static inline void msleep(unsigned long msecs)
{
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(msecs_to_jiffies(msecs) + 1);
}

#endif

#endif


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
#include <linux/blkdev.h>
#else

#include <linux/blkdev.h>

struct req_iterator {
	int i;
	struct bio *bio;
};

/* This should not be used directly - use rq_for_each_segment */
#define __rq_for_each_bio(_bio, rq)	\
	if ((rq->bio))			\
		for (_bio = (rq)->bio; _bio; _bio = _bio->bi_next)

#define rq_for_each_segment(bvl, _rq, _iter)			\
	__rq_for_each_bio(_iter.bio, _rq)			\
		bio_for_each_segment(bvl, _iter.bio, _iter.i)

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20))
#else
#define totalram_pages num_physpages
#define nr_free_pages() 0
#endif

#endif // __NDAS_TYPES_H__
