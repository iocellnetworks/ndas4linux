#ifndef _XLIB_DPC_H
#define _XLIB_DPC_H

#include <sal/time.h>
#include <sal/sync.h>

#define DPC_INVALID_ID ((dpc_id)0)

#define DPC_PRIO_NORMAL 1
#define DPC_PRIO_HIGH 0

#define DPC_MAX_PRIO 2

typedef int (*dpc_func)(void *, void *);
typedef void* dpc_id;


/* 
 * initialize the dpc facility with the given size for the dpc pool 
 */

int dpc_start(void);

/*
 * destroy the dpc facility.
 */

void dpc_stop(void);

/*
 * Create a DPC
 */
#define DPCFLAG_DO_NOT_FREE 0x00001

dpc_id dpc_create(int priority, dpc_func func, void *p1, void *p2, sal_event comp_event, int flags);

/*
 * Destroy a DPC
 */

void dpc_destroy(dpc_id dpcid);

/* 
 * Queue the func in the given tick
 */
int dpc_queue(dpc_id dpcid, sal_tick tick);

/* 
 * Invoke the func immediately
 * Returns DPC ID or DPC_INVALID_ID if error 
 */
int dpc_invoke(dpc_id dpcid);

/*
 * Cancel a queued DPC
 * Canceling is allowed only for a non-auto-free DPC.
 *
 * Return values:
 *    NDAS_OK: cancellation is successful.
 *    1 : cancellation is defered. Do not free the DPC by the caller.
 */

int dpc_cancel(dpc_id dpcid);

#ifdef XPLAT_BPC
/* 
 * initialize the B-DPC facility with the given size for the dpc pool 
 */

int bpc_start(void);

/*
 * destroy the B-DPC facility.
 */

void bpc_stop(void);

/*
 * Create a B-DPC
 */

dpc_id bpc_create(dpc_func func, void *p1, void *p2, sal_event comp_event, int flags);

/*
 * Destroy a B-DPC
 */

void bpc_destroy(dpc_id dpcid);

/* 
 * Queue the func in the given tick in the blocking procedure call queue.
 */
int bpc_queue(dpc_id dpcid, sal_tick tick);

/* 
 * Invoke the func immediately
 * Returns NDAS_OK or DPC_INVALID_ID if error 
 */

int bpc_invoke(dpc_id dpcid);

/*
 * Cancel a queued B-DPC
 * Canceling is allowed only for a non-auto-free DPC.
 *
 * Return values:
 *    NDAS_OK: cancellation is successful.
 *    < 0: cancellation fails.
 */

int bpc_cancel(dpc_id dpcid);

#endif

#endif    /* !_XLIB_DPC_H */

