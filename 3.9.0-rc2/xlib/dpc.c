/*
    To do: 
        Use same code for dpc/bpc or remove bpc
        Use mempool for dpc/bpc
*/
#include "xplatcfg.h"
#include "sal/libc.h"
#include "sal/types.h"
#include "sal/time.h"
#include "sal/sync.h"
#include "sal/thread.h"
#include "sal/mem.h"
#include "sal/debug.h"
#include "xlib/dpc.h"
#include "netdisk/list.h"

#define XLIB_DPC_THREAD_PRIO    0 /* normal priority */

#define DPC_MAX_WAITING_TIME (SAL_TICKS_PER_SEC*30)
#define DPC_DEBUG_INTERVAL (30*SAL_TICKS_PER_SEC)

#ifdef DEBUG
#define debug_dpc(l,x...) \
do { \
    if ( l <= DEBUG_LEVEL_DPC ) { \
        sal_debug_print("DPC|%s|%d|",__FUNCTION__,l); \
        sal_debug_println(x); \
    }\
} while(0)
#define debug_bpc(l,x...) \
do { \
    if ( l <= DEBUG_LEVEL_BPC ) { \
        sal_debug_print("BPC|%s|%d|",__FUNCTION__,l); \
        sal_debug_println(x); \
    }\
} while(0)
#else
#define debug_dpc(l,x...) do {} while(0)
#define debug_bpc(l,x...) do {} while(0)
#endif

#define DPC_MAGIC (0x6fb3cc59)

#ifndef RELEASE
#define MAGIC
#endif


//
// DPC entry
//

struct dpc {
#ifdef MAGIC
    int                 magic;
#endif
#ifdef DEBUG
    int                 dpcq_id;
#endif
    struct list_head    dpc_list;
    sal_tick            time;        /* Time to be executed. */
    xint16              prio;
    /* 
        0 if not being used , 1 if being used 
        must be protected by mutex dpc_lock
    */
    xint8              in_queue:1;
    xint8              in_execution:1;
    xint8              reserved:6;

    xint8              do_not_free:1;
    xint8              reserved2:7;

    dpc_func            func;
    void                *p1;
    void                *p2;
    sal_event           user_comp_event;

};

//
// DPC queue
//

struct xlib_dpc_queue {
    /* Lock for DPC datas: DPC item lists, v_no_pending_dpc, dpc_exit_request */
    sal_spinlock        dpc_lock;
    xint16              dpc_exit_request; /* Guarded by dpc_lock */
    xint16              dpc_pending_count;
    sal_event           dpc_wait_event;
    void *              dpc_pool;

    struct list_head    dpc_queue[DPC_MAX_PRIO];

    sal_thread_id       dpc_thread_id;
    int                 dpcq_id;

#if defined(STAT_DPC) && !defined(RELEASE)
    sal_atomic          dpc_allocs;
    int                 dpc_max_pending_count;
#endif

    char poolname[32];
    char threadname[32];
};

static struct xlib_dpc_queue g_dpc_queue;
static struct xlib_dpc_queue g_bpc_queue;

#ifdef STAT_DPC

static void _print_dpc_queue(struct xlib_dpc_queue *dpcqueue)
{
    int i = 0;
    sal_error_print("dpc:%d/%d\n", dpcqueue->dpc_pending_count, dpcqueue->dpc_max_pending_count);
    for ( i = 0; i < DPC_MAX_PRIO; i++) {
        sal_error_print("dpc queue[%d]=%p\n",i, dpcqueue->dpc_queue[i]);
    }
}

// DPC dump
static void _dump_dpc(struct xlib_dpc_queue *dpcqueue, void* p1, void* p2) {
    _print_dpc(dpcqueue);
    if (!dpcqueue->dpc_exit_request)
        _dpc_queue(dpcqueue, DPC_DEBUG_INTERVAL, DPC_PRIO_NORMAL, _dump_dpc, NULL, NULL);
}

#endif


static void* _dpc_thread(void *p1);

static
inline
int
_dpc_start(struct xlib_dpc_queue *dpcq, int queue_id)
{
    int        i;
    int ret;

    debug_dpc(1, "DPC queue #%d starting", queue_id);

    if (dpcq->dpc_thread_id != SAL_INVALID_THREAD_ID) {
        /* Already running */
        return 0;
    }

    dpcq->dpc_exit_request = FALSE;
    dpcq->dpc_pending_count = 0;
    dpcq->dpc_thread_id = SAL_INVALID_THREAD_ID;
    dpcq->dpc_wait_event = SAL_INVALID_EVENT;
    dpcq->dpc_pool = NULL;
    dpcq->dpcq_id = queue_id;

#if defined(STAT_DPC) && !defined(RELEASE)
    sal_atomic dpc_allocs = sal_atomic_init(0);
    int dpc_max_pending_count;
#endif
    for(i = 0; i< DPC_MAX_PRIO; i++) {
        INIT_LIST_HEAD(&dpcq->dpc_queue[i]);
    }

    debug_dpc(6, "sal_spinlock_create-ing");

    ret = sal_spinlock_create("dpc_list", &dpcq->dpc_lock);
    if (!ret)
        goto out3;

    sal_snprintf(dpcq->poolname, 32, "ndas_dpc%d", queue_id);
    dpcq->dpc_pool = sal_create_mem_pool(dpcq->poolname, sizeof(struct dpc));
    if(dpcq->dpc_pool == NULL)
        goto out3;

    debug_dpc(6, "sal_event_create-ing");

    dpcq->dpc_wait_event = sal_event_create("dpc_wait");

    if ( dpcq->dpc_wait_event == SAL_INVALID_EVENT )
        goto out2;

    debug_dpc(6, "sal_malloc-ing");

    dpcq->dpc_exit_request = FALSE;

    debug_dpc(9, "sal_thread_create(%p,dpc,%d,0,0)", &dpcq->dpc_thread_id, XLIB_DPC_THREAD_PRIO);

    ret = sal_thread_create((sal_thread_id*)&dpcq->dpc_thread_id, 
        "dpc", XLIB_DPC_THREAD_PRIO,
        0, _dpc_thread, dpcq);

    if (ret !=0 ) 
        goto out1;

    debug_dpc(5, "dpc_thread_id=%p ed", dpcq->dpc_thread_id);

#ifdef STAT_DPC
    _dpc_queue(dpcq, DPC_DEBUG_INTERVAL,DPC_PRIO_NORMAL, _dump_dpc, NULL, NULL);
#endif

    return 0;
out1:
    sal_spinlock_destroy(dpcq->dpc_lock);
out2:
    sal_event_destroy(dpcq->dpc_wait_event);
out3:
    if(dpcq->dpc_pool) {
        sal_destroy_mem_pool(dpcq->dpc_pool);
        dpcq->dpc_pool = NULL;
    }
    return NDAS_ERROR_OUT_OF_MEMORY;
}

//
// Clean up the pending DPCs
// Requires DPC queue lock is held.
//
static
inline
void
_dpc_cleanup_pending_dpc(struct xlib_dpc_queue *dpcq)
{
    int i;
    struct dpc *d;
    struct list_head * dpc_entry, * dpc_next;

    for (i = 0;i < DPC_MAX_PRIO; i++)
    {
        dpc_entry = dpcq->dpc_queue[i].next;
        while(dpc_entry != &dpcq->dpc_queue[i]) {

            // Save the next entry before freeing the current entry.
            dpc_next = dpc_entry->next;

            // Free the DPC
            d = list_entry(dpc_entry, struct dpc, dpc_list);
            sal_assert(d->dpcq_id == dpcq->dpcq_id);
            list_del(&d->dpc_list);
            d->in_queue = 0;
            dpcq->dpc_pending_count --;
            if(!d->do_not_free) {
                if(d->user_comp_event) {
                    sal_event_set(d->user_comp_event);
                }
                sal_free_from_pool(dpcq->dpc_pool, d);
#if defined(STAT_DPC) && !defined(RELEASE)
                sal_atomic_dec(&dpcq->dpc_allocs);
#endif
            }

            // Move to the next
            dpc_entry = dpc_next;
        }
    }
}


static
inline
void
_dpc_stop(struct xlib_dpc_queue *dpcq)
{
    sal_spinlock_take_softirq(dpcq->dpc_lock);
    dpcq->dpc_exit_request = TRUE;

    debug_dpc(1, "DPC queue #%d stopping", dpcq->dpcq_id);

    //
    // Wait until the DPC thread ends.
    //
    while (dpcq->dpc_thread_id != SAL_INVALID_THREAD_ID) {
        sal_event_set(dpcq->dpc_wait_event);
        sal_spinlock_give_softirq(dpcq->dpc_lock);
        sal_msleep(50);
        sal_spinlock_take_softirq(dpcq->dpc_lock);
    }

    _dpc_cleanup_pending_dpc(dpcq);

    sal_spinlock_give_softirq(dpcq->dpc_lock);

    sal_spinlock_destroy(dpcq->dpc_lock);

    sal_event_destroy(dpcq->dpc_wait_event);
    dpcq->dpc_wait_event = NULL;

    sal_destroy_mem_pool(dpcq->dpc_pool);
    dpcq->dpc_pool = NULL;
}

static
inline
dpc_id
_dpc_create(struct xlib_dpc_queue * dpcq, int priority, dpc_func func, void *p1, void *p2, sal_event comp_event, int flags)
{
    struct dpc *new_dpc; /* new dpc item */

    if(priority >= DPC_MAX_PRIO) {
        return NULL;
    }

    new_dpc = (struct dpc *) sal_alloc_from_pool(dpcq->dpc_pool, sizeof(struct dpc));
    if(new_dpc == NULL) {
        debug_dpc(1,"failed to allocate new DPC");
        return NULL;
    }
    debug_dpc(4,"dpc%d: dpc=%p priority=%d func=%p p1=%p flg=%x", dpcq->dpcq_id, new_dpc, priority, func, p1, flags);

#ifdef MAGIC
    new_dpc->magic = DPC_MAGIC;
#endif
#ifdef DEBUG
    new_dpc->dpcq_id = dpcq->dpcq_id;
#endif
    INIT_LIST_HEAD(&new_dpc->dpc_list);
    new_dpc->time = 0;
    new_dpc->prio = priority;
    new_dpc->in_queue = 0;
    new_dpc->in_execution = 0;
    if(flags & DPCFLAG_DO_NOT_FREE) {
        new_dpc->do_not_free = 1;
    } else {
        new_dpc->do_not_free = 0;
    }

    new_dpc->func  = func;
    new_dpc->p1 = p1;
    new_dpc->p2 = p2;
    new_dpc->user_comp_event = comp_event;

#if defined(STAT_DPC) && !defined(RELEASE)
    sal_atomic_inc(&dpcq->dpc_allocs);
#endif

    return (dpc_id) new_dpc;
}

static
inline
void
_dpc_destroy(struct xlib_dpc_queue * dpcq, dpc_id dpcid, xbool qlock)
{
    struct dpc *d = (struct dpc *)dpcid; /* new dpc item */

    if(qlock) {
        sal_spinlock_take_softirq(dpcq->dpc_lock);
    }
    sal_assert(d->in_queue == 0);
    sal_assert(d->dpcq_id == dpcq->dpcq_id);

    // Defer freeing the DPC.
    if(d->in_execution) {
        d->do_not_free = 0;
        if(qlock) {
            sal_spinlock_give_softirq(dpcq->dpc_lock);
        }
        return;
    }
    if(qlock) {
        sal_spinlock_give_softirq(dpcq->dpc_lock);
    }

    sal_free_from_pool(dpcq->dpc_pool, dpcid);

#if defined(STAT_DPC) && !defined(RELEASE)
    sal_atomic_dec(&dpcq->dpc_allocs);
#endif
}


//
// Queue a DPC.
//
// Do not allow the DPC to be queued if the DPC is in execution and
// the caller specifies the comp event.
//
static
inline
int
_queue_dpc(struct xlib_dpc_queue * dpcq, dpc_id dpcid, sal_tick tick)
{
    struct dpc *new_dpc = (struct dpc *)dpcid; /* new dpc item */
    struct dpc *cur_dpc; /* current dpc item */
    struct list_head *cur_entry;
    int priority;
    int queue_again = 0;

    debug_dpc(4,"dpc%d: tick=%d", dpcq->dpcq_id, tick);

#ifdef MAGIC
    sal_assert(new_dpc->magic == DPC_MAGIC);
#endif
#ifdef DEBUG
    sal_assert(new_dpc->prio < DPC_MAX_PRIO);
    sal_assert(new_dpc->dpcq_id == dpcq->dpcq_id);
#endif

    sal_spinlock_take_softirq(dpcq->dpc_lock);

    if (dpcq->dpc_exit_request) { /* dpc is now stopping */
        sal_spinlock_give_softirq(dpcq->dpc_lock);
        sal_debug_print("dpc%d: thread shutting down\n", dpcq->dpcq_id);
        return NDAS_ERROR_SHUTDOWN;
    }
#ifdef DEBUG
    if (dpcq->dpc_thread_id == SAL_INVALID_THREAD_ID) {
        sal_spinlock_give_softirq(dpcq->dpc_lock);
        sal_debug_print("dpc%d: thread is not created\n", dpcq->dpcq_id);
        return NDAS_ERROR_SHUTDOWN;
    }
#endif

    //
    // If the DPC is already in the queue, queue again.
    //
    if(new_dpc->in_queue) {
        debug_dpc(4,"dpc %p queued again.", new_dpc);
        list_del(&new_dpc->dpc_list);
        new_dpc->in_queue = 0;
        dpcq->dpc_pending_count --;

        queue_again = 1;
    }

    sal_assert(new_dpc->in_queue == 0);
    priority = new_dpc->prio;
    new_dpc->time = sal_tick_add(sal_get_tick(), tick);
    for (cur_entry = dpcq->dpc_queue[priority].next;
         cur_entry!=&dpcq->dpc_queue[priority];
         cur_entry = cur_entry->next) {
        /* Find insertion point in the list sorted by callout time */
        debug_dpc(4,"cd=%p", cur_entry);
        cur_dpc = list_entry(cur_entry, struct dpc, dpc_list);
#ifdef MAGIC
        sal_assert(cur_dpc->magic == DPC_MAGIC);
#endif
        if (sal_tick_sub(new_dpc->time, cur_dpc->time) < 0) {
            debug_dpc(4,"after cd=%p", cur_entry);
            break;
        }
    }

#if defined(STAT_DPC) && !defined(RELEASE)
    if ( dpcq->dpc_pending_count > dpcq->dpc_max_pending_count )
        dpcq->dpc_max_pending_count = dpcq->dpc_pending_count;
#endif

    /*
     * lt is the head if insert first entry, OR, points to entry after which
     * we should insert.
     */
    /* Insert before the current entry.
       It is meant to insert to the tail if no early-timeout entry is found,
       or insert before the current entry if an entry is found.
        */
    list_add_tail(&new_dpc->dpc_list, cur_entry);
    new_dpc->in_queue = 1;

    dpcq->dpc_pending_count++;

    //
    //   Wake up the DPC thread when the new DPC is inserted in the first place.
    //
    if(new_dpc->dpc_list.prev == &dpcq->dpc_queue[priority])
        sal_event_set(dpcq->dpc_wait_event);

    sal_spinlock_give_softirq(dpcq->dpc_lock);

    debug_dpc(3,"ed id=%p",new_dpc);
    return NDAS_OK + queue_again;
}


/* 
    Return DPC_INVALID_ID if dpc is canceled
    or input dpc if not
*/
ndas_error_t
_dpc_cancel(struct xlib_dpc_queue *dpcq, dpc_id dpcid)
{
    struct dpc* d = (struct dpc*)dpcid;

    if(dpcid == DPC_INVALID_ID) {
        debug_dpc(1, "canceling already canceled DPC");
	return NDAS_ERROR_INVALID_OPERATION;
    }

    debug_dpc(4, "dpc%d: d=%p", dpcq->dpcq_id, d);

    sal_assert(d->dpcq_id == dpcq->dpcq_id);

    sal_spinlock_take_softirq(dpcq->dpc_lock);
    debug_dpc(4, "func=%p, in_queue=%d", d->func, (int)d->in_queue);
    // Canceling is allowed only for a non-auto-free DPC.
    if(!d->do_not_free) {
        sal_spinlock_give_softirq(dpcq->dpc_lock);
        debug_dpc(1, "not allowed for non-auto-free DPC");
        return NDAS_ERROR_INVALID_OPERATION;
    }
    if(d->in_queue) {
        list_del(&d->dpc_list);
        d->in_queue = 0;
        dpcq->dpc_pending_count--;

    } else {
        sal_spinlock_give_softirq(dpcq->dpc_lock);
        debug_dpc(4, "ed func=%p is not in the list", d->func);
        return NDAS_ERROR_INVALID_OPERATION;
    }

    if(d->user_comp_event) {
        sal_event_set(d->user_comp_event);
    }
    sal_spinlock_give_softirq(dpcq->dpc_lock);

    debug_dpc(3, "ed func=%p removed", d->func);

    return NDAS_OK;
}

static void*
_dpc_thread(void *p1)
{
    struct xlib_dpc_queue *dpcq = (struct xlib_dpc_queue *)p1;
    struct dpc *cur_dpc;
    int ret;

    sal_snprintf(dpcq->threadname, 32, "nd/dpcd%d", dpcq->dpcq_id);

    sal_thread_block_signals();
    sal_thread_daemonize(dpcq->threadname);
    sal_thread_nice(-20);
    debug_dpc(1,"starting..");
    sal_spinlock_take_softirq(dpcq->dpc_lock);
    while (dpcq->dpc_exit_request==FALSE) 
    {

        if (!dpcq->dpc_pending_count) {
            sal_spinlock_give_softirq(dpcq->dpc_lock);
            /* No dpc is pending */
            debug_dpc(4,"xlib-event_wait-ing");
            sal_event_wait(dpcq->dpc_wait_event, SAL_SYNC_FOREVER);
            sal_event_reset(dpcq->dpc_wait_event);

            sal_spinlock_take_softirq(dpcq->dpc_lock);
            continue;
        }
        {
            int idx_dpcq;
            struct list_head *dpc_entry;
            sal_tick now;
            int t;
            int dif;

            cur_dpc = NULL;
            dif = 0x7fffffff;
            now = sal_get_tick();

            for (idx_dpcq = 0; idx_dpcq < DPC_MAX_PRIO; idx_dpcq ++)
            {

                dpc_entry = &dpcq->dpc_queue[idx_dpcq];

                if ( list_empty(dpc_entry))
                    continue;

                cur_dpc = list_entry(dpc_entry->next, struct dpc, dpc_list);
                debug_dpc(4,"dpc_queue[%d]=%p", idx_dpcq, cur_dpc);

                sal_assert(cur_dpc->dpcq_id == dpcq->dpcq_id);

                if ( (t = sal_tick_sub(cur_dpc->time, now)) <= 0 ) {
                    dif = t;
                    break;
                }
                //
                // Save the lowest wait time.
                //
                if (t < dif) {
                    dif = t;
                }
            }
            if ( cur_dpc == NULL ) {
                sal_assert(dpcq->dpc_pending_count == 0);
                debug_dpc(1,"dpc=NULL, pending count=%d", dpcq->dpc_pending_count);
                break;
            }

            if ( dif > 0) {
                sal_spinlock_give_softirq(dpcq->dpc_lock);
                debug_dpc(4,"dpc sal_event_wait %d tick", dif); 

                ret = sal_event_wait(dpcq->dpc_wait_event, dif);
                sal_event_reset(dpcq->dpc_wait_event);

                sal_spinlock_take_softirq(dpcq->dpc_lock);
                // Get next DPC again. User could cancel the DPC in cur_dpc
		continue;
            }
            debug_dpc(4,"dpc%d prio=%d proc=%p at %u, now=%u, in_queue=%d",
                dpcq->dpcq_id, cur_dpc->prio, cur_dpc->func, cur_dpc->time, now, cur_dpc->in_queue);

            /* 1. Remove from pending queue */
            list_del(&cur_dpc->dpc_list);
            sal_assert(cur_dpc->in_queue);
            cur_dpc->in_queue = 0;
            cur_dpc->in_execution = 1;

            dpcq->dpc_pending_count --;

            debug_dpc(6, "dpc_queue[%d]=%p", idx_dpcq, dpcq->dpc_queue[idx_dpcq].next);
            sal_spinlock_give_softirq(dpcq->dpc_lock);

            /* 2. Call dpc func */
            debug_dpc(6,"call-ing func=%p dpc=%p", cur_dpc->func, cur_dpc);
#ifdef MAGIC
            sal_assert(cur_dpc->magic == DPC_MAGIC);
#endif
            cur_dpc->func(cur_dpc->p1, cur_dpc->p2);

            debug_dpc(6,"call-ed func=%p", cur_dpc->func);

            /* 3. Indicate the dpc is done. */
            sal_spinlock_take_softirq(dpcq->dpc_lock);
            cur_dpc->in_execution = 0;
            if ( cur_dpc->user_comp_event != SAL_INVALID_EVENT ) {
                debug_dpc(4,"set event %p", cur_dpc->user_comp_event);
                sal_event_set(cur_dpc->user_comp_event);
            }
            if(!cur_dpc->do_not_free) {
                // Remove the DPC from the queue.
                // This happens when DPC function re-queue the DPC again,
                // but the owner tried to destroy the DPC.
                if(cur_dpc->in_queue) {
	            debug_dpc(1,"removed dpc %p from the queue %d", cur_dpc, dpcq->dpcq_id);
                    list_del(&cur_dpc->dpc_list);
                    cur_dpc->in_queue = 0;
                    dpcq->dpc_pending_count--;
                }
                _dpc_destroy(dpcq, cur_dpc, FALSE);
            }
            /* Do not access the DPC afterward */
        }
    }
    dpcq->dpc_thread_id = SAL_INVALID_THREAD_ID;
    dpcq->dpc_exit_request = TRUE;
    sal_spinlock_give_softirq(dpcq->dpc_lock);
    sal_event_set(dpcq->dpc_wait_event);
    sal_thread_exit(0);
    debug_dpc(1, "_dpc_thread exiting..");
    return 0;
}


/*
 * DPC interface
 */
int
dpc_start(void)
{
    return _dpc_start(&g_dpc_queue, 0);
}

void
dpc_stop(void)
{
    _dpc_stop(&g_dpc_queue);
}

dpc_id
dpc_create(int priority, dpc_func func, void *p1, void *p2, sal_event comp_event, int flags)
{
    return _dpc_create(&g_dpc_queue, priority, func, p1, p2, comp_event, flags);
}

void
dpc_destroy(dpc_id dpcid)
{
    return _dpc_destroy(&g_dpc_queue, dpcid, TRUE);
}

int
dpc_queue(dpc_id dpcid, sal_tick tick)
{
    return _queue_dpc(&g_dpc_queue, dpcid, tick);
}

int
dpc_invoke(dpc_id dpcid)
{
    return _queue_dpc(&g_dpc_queue, dpcid, 0);
}

int
dpc_cancel(dpc_id dpcid)
{
    return _dpc_cancel(&g_dpc_queue, dpcid);
}

#ifdef XPLAT_BPC
/*
 * BPC interface
 */
int
bpc_start(void)
{
    return _dpc_start(&g_bpc_queue, 1);
}

void
bpc_stop(void)
{
    _dpc_stop(&g_bpc_queue);
}

dpc_id
bpc_create(dpc_func func, void *p1, void *p2, sal_event comp_event, int flags)
{
    return _dpc_create(&g_bpc_queue, DPC_PRIO_HIGH, func, p1, p2, comp_event, flags);
}

void
bpc_destroy(dpc_id dpcid)
{
    return _dpc_destroy(&g_bpc_queue, dpcid, TRUE);
}

int
bpc_queue(dpc_id dpcid, sal_tick tick)
{
    return _queue_dpc(&g_bpc_queue, dpcid, tick);
}

int
bpc_invoke(dpc_id dpcid)
{
    return _queue_dpc(&g_bpc_queue, dpcid, 0);
}

int
bpc_queue_and_wait(dpc_id dpcid, sal_tick tick)
{
    return _queue_dpc(&g_bpc_queue, dpcid, tick);
}

int
bpc_cancel(dpc_id dpcid)
{
    return _dpc_cancel(&g_bpc_queue, dpcid);
}


#endif

