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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#include <linux/version.h>
#include <linux/module.h> 

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>


#include "ndas_udev.h"

#include "ndasdib.h"
#include "ndas_id.h"

#include "ndas_sdev.h"
#include "ndas_session.h"
#include "ndas_regist.h"
#include "ndas_ndev.h"

#ifdef DEBUG

int	dbg_level_udev = DBG_LEVEL_UDEV;

#define dbg_call(l,x...) do { 								\
    if ( l <= dbg_level_udev ) {							\
        printk("%s|%d|%d|",__FUNCTION__, __LINE__,l); 		\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif



// max buffer size. todo remove this buffer

int udev_max_xfer_unit = ND_MAX_BLOCK_SIZE;

static ndas_error_t udev_init(sdev_t *sdev, udev_t *udev);
static void 		udev_deinit(udev_t *udev);

static ndas_error_t udev_enable(udev_t *udev, int flags);
static void 		udev_disable(udev_t *udev, ndas_error_t err);

static void udev_io(udev_t *udev, ndas_request_t *ndas_req);

unit_ops_t udev_ops = {

    .init 		= udev_init,
    .deinit 	= udev_deinit,

    .enable 	= udev_enable,
    .disable 	= udev_disable,

    .io 		= udev_io,
};

void udev_set_max_xfer_unit(int io_buffer_size) {

    udev_max_xfer_unit = (io_buffer_size > 0) ? io_buffer_size : ND_MAX_BLOCK_SIZE;

    dbg_call( 1, "io_buffer_size=%d\n", io_buffer_size );
}


static ndas_request_t* udeviod_get_next_task(udev_t *udev, bool *urgent)
{
    ndas_request_t *ndas_req;
#ifdef NDAS_WRITE_BACK
	ndas_request_t *temp_ndas_req;
#endif
    unsigned long flags;

    dbg_call( 4, "ing slot=%d\n", udev->uinfo.slot );

    spin_lock_irqsave( &udev->req_queue_lock, flags );

    if (list_empty(&udev->req_queue)) {

        dbg_call( 3, "ed slot=%d udev->req_queue is no_pending io\n", udev->uinfo.slot );

		spin_unlock_irqrestore( &udev->req_queue_lock, flags );
    	return NULL;
    }

	*urgent = false;

#ifndef NDAS_WRITE_BACK

    ndas_req = list_entry(udev->req_queue.next, ndas_request_t, queue);

#else

	ndas_req = NULL;

	list_for_each_entry(temp_ndas_req, &udev->req_queue, queue) {

		if (ndas_req == NULL) {

			ndas_req = temp_ndas_req;

		} else if (temp_ndas_req->urgent == true) {

			*urgent = true;
			ndas_req = temp_ndas_req;

		} else if (temp_ndas_req->cmd == NDAS_CMD_FLUSH) {

			NDAS_BUG_ON(true);
			ndas_req = temp_ndas_req;

#if 0
		} else if (temp_ndas_req->cmd == NDAS_CMD_WRITE && temp_ndas_req->num_sec < 256) {

			ndas_req = temp_ndas_req;
#endif
		} else if (ndas_req->cmd == NDAS_CMD_READ && temp_ndas_req->cmd == NDAS_CMD_WRITE) {
		
		} else if (ndas_req->cmd == NDAS_CMD_WRITE && temp_ndas_req->cmd == NDAS_CMD_READ) {

			ndas_req = temp_ndas_req;

#if 0

		} else if (ndas_req->cmd == NDAS_CMD_READ && ndas_req->num_sec > temp_ndas_req->num_sec) {

			ndas_req = temp_ndas_req;	

		} else if (ndas_req->num_sec == 1024) {

		} else if (temp_ndas_req->num_sec == 1024) {

			ndas_req = temp_ndas_req;

		} else if (forword && ndas_req->start_sec > temp_ndas_req->start_sec) {

			ndas_req = temp_ndas_req;	

		} else if (ndas_req->start_sec < temp_ndas_req->start_sec) {

			ndas_req = temp_ndas_req;
#endif
		} 

		if (temp_ndas_req->urgent == true) {

			break;
		}
	}

	if (ndas_req->cmd == NDAS_CMD_READ) {

	 	list_for_each_entry(temp_ndas_req, &udev->req_queue, queue) {

			if (temp_ndas_req->cmd != NDAS_CMD_WRITE) {

				continue;
			}

			if (ndas_req->u.s.sector < temp_ndas_req->u.s.sector) {

				if (ndas_req->u.s.sector + ndas_req->u.s.nr_sectors > temp_ndas_req->u.s.sector) {

					dbg_call( 1, "ndas_req->cmd = %d, ndas_req->start_sec = %ld, ndas_req->nr_sectors = %ld "
								 "temp_ndas_req->cmd = %d, temp_ndas_req->start_sec = %ld, tem_ndas_req->nr_sectors = %ld\n",
								 ndas_req->cmd, (unsigned long)ndas_req->u.s.sector, ndas_req->u.s.nr_sectors,
								 temp_ndas_req->cmd, (unsigned long)temp_ndas_req->u.s.sector, temp_ndas_req->u.s.nr_sectors );

					ndas_req = temp_ndas_req;
					break;
				}
			}

			if (ndas_req->u.s.sector > temp_ndas_req->u.s.sector) {

				 if (temp_ndas_req->u.s.sector + temp_ndas_req->u.s.nr_sectors > ndas_req->u.s.sector) {

					dbg_call( 1, "ndas_req->cmd = %d, ndas_req->start_sec = %ld, ndas_req->nr_sectors = %ld "
								 "temp_ndas_req->cmd = %d, temp_ndas_req->start_sec = %ld, tem_ndas_req->nr_sectors = %ld\n",
								 ndas_req->cmd, (unsigned long)ndas_req->u.s.sector, ndas_req->u.s.nr_sectors,
								 temp_ndas_req->cmd, (unsigned long)temp_ndas_req->u.s.sector, temp_ndas_req->u.s.nr_sectors );

					ndas_req = temp_ndas_req;
					break;
				}
			}
		}
	}
	
	if (ndas_req->urgent == true) {

		NDAS_BUG_ON( ndas_req->cmd != NDAS_CMD_WRITE );

	 	list_for_each_entry(temp_ndas_req, &udev->req_queue, queue) {

			if (temp_ndas_req == ndas_req) {

				break;
			}

			if (temp_ndas_req->cmd != NDAS_CMD_WRITE) {

				continue;
			}

			if (ndas_req->u.s.sector < temp_ndas_req->u.s.sector) {

				if (ndas_req->u.s.sector + ndas_req->u.s.nr_sectors > temp_ndas_req->u.s.sector) {

					dbg_call( 1, "ndas_req->cmd = %d, ndas_req->sector = %ld, ndas_req->nr_sectors = %ld "
								 "temp_ndas_req->cmd = %d, temp_ndas_req->sector = %ld, tem_ndas_req->nr_sectors = %ld\n",
								 ndas_req->cmd, (unsigned long)ndas_req->u.s.sector, ndas_req->u.s.nr_sectors,
								 temp_ndas_req->cmd, (unsigned long)temp_ndas_req->u.s.sector, temp_ndas_req->u.s.nr_sectors );

					ndas_req = temp_ndas_req;
					break;
				}
			}

			if (ndas_req->u.s.sector > temp_ndas_req->u.s.sector) {

				 if (temp_ndas_req->u.s.sector + temp_ndas_req->u.s.nr_sectors > ndas_req->u.s.sector) {

					dbg_call( 1, "ndas_req->cmd = %d, ndas_req->sector = %ld, ndas_req->nr_sectors = %ld "
								 "temp_ndas_req->cmd = %d, temp_ndas_req->sector = %ld, tem_ndas_req->nr_sectors = %ld\n",
								 ndas_req->cmd, (unsigned long)ndas_req->u.s.sector, ndas_req->u.s.nr_sectors,
								 temp_ndas_req->cmd, (unsigned long)temp_ndas_req->u.s.sector, temp_ndas_req->u.s.nr_sectors );

					ndas_req = temp_ndas_req;
					break;
				}
			}
		}
	}
	
#endif

	spin_unlock_irqrestore( &udev->req_queue_lock, flags );

    dbg_call( 4, "ed slot=%d ndas_req=%p\n", udev->uinfo.slot, ndas_req );

    return ndas_req;
}

//////////////////////////////////////////////////////////////////////////
//
// Lock operations
//

//
// Acquire a general purpose lock.
// NOTE: With RetryWhenFailure is FALSE, be aware that the lock-cleanup
//        workaround will not be performed in this function.
//
// return values
//    NDAS_ERROR_ACQUIRE_LOCK_FAILED : Lock is owned by another session.
//                            This is not communication error.
//
// Opxxxx is calling conn_xxx functions and it is not multi-thread safe.Should be called in ndiod thread or single thread.
//

ndas_error_t
OpAcquireDevLock(
    uconn_t*  conn,
    __u32  LockIndex,
    unsigned char* LockData,
    __u32 TimeOut,
    bool            RetryWhenFailure
){
    ndas_error_t status = NDAS_ERROR_ACQUIRE_LOCK_FAILED;
    __u32 lockContention;
    __u32    startTime;
    __u32    maximumWaitTime;

	__u32 		buflockIdx0 = NDAS_LOCK_0;
	__u32		buflockIdx1 = NDAS_LOCK_1;
	__u32 		buflockIdx2 = NDAS_LOCK_2;
	__u32 		buflockIdx3 = NDAS_LOCK_3;

    lockContention = 0;
    startTime = jiffies_to_msecs(jiffies);

    if(TimeOut) {
        maximumWaitTime = TimeOut;
    } else {
        maximumWaitTime = 2 * 1000;
    }

    dbg_call( 4, "OpAcquireDevLock: TimeOut = %d, RetryWhenFailure=%d\n", TimeOut, RetryWhenFailure );

    if(conn->ndas_locked[buflockIdx3] == TRUE) 
        return NDAS_OK;
    if(conn->ndas_locked[buflockIdx0] == TRUE) 
        conn_lock_operation( conn, NDAS_LOCK_GIVE, buflockIdx0, NULL, NULL );
    if(conn->ndas_locked[buflockIdx1] == TRUE) 
        conn_lock_operation( conn, NDAS_LOCK_GIVE, buflockIdx1, NULL, NULL );
    if(conn->ndas_locked[buflockIdx2] == TRUE) 
        conn_lock_operation( conn, NDAS_LOCK_GIVE, buflockIdx2, NULL, NULL );

    while(time_before(jiffies, msecs_to_jiffies(startTime + maximumWaitTime))) {
            status = conn_lock_operation(conn, NDAS_LOCK_TAKE, LockIndex, LockData, NULL);
//        status = udev_lock_operation(udev, NDAS_LOCK_TAKE, LockIndex, LockData, NULL);

        if(status == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {
            dbg_call( 4, "LockIndex%u: Lock contention #%u!!!\n", LockIndex, lockContention );
    	     dbg_call( 0, "LockIndex%u: Lock contention #%u!!!\n", LockIndex, lockContention );
        } else if(NDAS_SUCCESS(status)) {
            break;
        } else {
            break;
        }
        lockContention ++;

        //
        //    Clean up the lock on the NDAS device.
        //

        if(lockContention != 0 && (lockContention % 10000 == 0)) {
            conn_lock_operation(conn, NDAS_LOCK_BREAK, LockIndex, NULL, NULL);
//            udev_lock_operation(udev, NDAS_LOCK_BREAK, LockIndex, NULL, NULL);
        }

        if(RetryWhenFailure == FALSE) {
            break;
        }
#ifdef NDAS_CRYPTO
	msleep( msecs_to_jiffies(10) );
#endif
    }
    if(status == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {
        dbg_call( 1, "Lock denied. idx=%u lock contention=%u\n", LockIndex, lockContention );
    } else if(!NDAS_SUCCESS(status)) {
        dbg_call( 1, "Failed to acquire lock idx=%u lock contention=%u\n", LockIndex, lockContention );
    } else {
        dbg_call( 4, "Acquired lock idx=%u lock contention=%u\n", LockIndex, lockContention );
        dbg_call( 1, "Acquired lock idx=%u lock contention=%u\n", LockIndex, lockContention );
    }

    return status;
}

ndas_error_t
OpReleaseDevLock(
    uconn_t*    conn,
    __u32            LockIndex,
    unsigned char *            LockData,
    __u32   TimeOut
){
    ndas_error_t    status;
	__u32 		buflockIdx3 = NDAS_LOCK_3;

    dbg_call( 4, "OpReleaseDevLock: TimeOut = %d\n", TimeOut );

    if(conn->ndas_locked[buflockIdx3] == FALSE) 
        return NDAS_OK;	
	
    status = conn_lock_operation(conn, NDAS_LOCK_GIVE, LockIndex, LockData, NULL);
//    status = udev_lock_operation(udev, NDAS_LOCK_GIVE, LockIndex, LockData, NULL);

    if(!NDAS_SUCCESS(status)) {
        dbg_call( 4, "Failed to release lock idx=%d\n", LockIndex );
        dbg_call( 1, "Failed to release lock idx=%d\n", LockIndex );
    } else {
        dbg_call( 4, "Release lock idx=%d\n", LockIndex );
        dbg_call( 1, "Release lock idx=%d\n", LockIndex );
    }
    return status;
}

static ndas_error_t udeviod_dispatch(udev_t *udev, ndas_request_t *ndas_req)
{
    ndas_error_t err;
#ifdef NDAS_CRYPTO
    int lock_data;
#endif

	NDAS_BUG_ON( ndas_req->next_done == 0 );
	NDAS_BUG_ON( udev->uinfo.slot != ndas_req->slot );

    dbg_call( 4,"waiting slot=%d\n", udev->uinfo.slot );

	NDAS_BUG_ON( udev->conn.status != CONN_STATUS_CONNECTED );

	switch (ndas_req->cmd) {
		
	case NDAS_CMD_LOCKOP:
//#if 0
#ifdef NDAS_CRYPTO
		if ( ndas_req->u.l.lock_op == NDAS_LOCK_TAKE ) {
			err = OpAcquireDevLock( &udev->conn, ndas_req->u.l.lock_id, (unsigned char *)&lock_data,  30 * 1000, TRUE );
		}
		else if ( ndas_req->u.l.lock_op == NDAS_LOCK_GIVE ) {
			err = OpReleaseDevLock( &udev->conn, ndas_req->u.l.lock_id, (unsigned char *)&lock_data,  30 * 1000 );
		}
		else {
			err = conn_lock_operation( &udev->conn, ndas_req->u.l.lock_op, ndas_req->u.l.lock_id, NULL, NULL );
		}
		
#else
		err = conn_lock_operation( &udev->conn, ndas_req->u.l.lock_op, ndas_req->u.l.lock_id, NULL, NULL );
#endif
		break;

	case NDAS_CMD_FLUSH:
    case NDAS_CMD_READ:
    case NDAS_CMD_WRITE:

		err = conn_rw_vector( &udev->conn, ndas_req->cmd, ndas_req->u.s.sector, 
							  ndas_req->u.s.nr_sectors, ndas_req->req_iov, ndas_req->req_iov_num );
		break;

	default:

        err = NDAS_ERROR_INTERNAL;
        NDAS_BUG_ON(true);

        break;
	}

    return err;
}

int udeviod_thread(void *arg)
{
	udev_t  *udev = (udev_t *)arg;

	char 		  name[32];
    unsigned long flags;

    bool 		   do_exit;
    ndas_request_t *ndas_req;

	__u32 		buflockIdx0 = NDAS_LOCK_0;
	__u32		buflockIdx1 = NDAS_LOCK_1;
	__u32 		buflockIdx2 = NDAS_LOCK_2;

	ndas_error_t	err;
	bool			urgent;
	unsigned long	acquire_jiffies = 0;


    snprintf( name, sizeof(name), "udeviod/%d", udev->uinfo.slot );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

	daemonize(name);
	set_user_nice( current, -20 );

#else

	daemonize();
	sprintf( current->comm, name );
    reparent_to_init(); // kernel over 2.5 don't need this because daemonize call this function.

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

    spin_lock_irqsave( &current->sighand->siglock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending();
    spin_unlock_irqrestore( &current->sighand->siglock, flags );

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,27))

    spin_lock_irqsave( &current->sigmask_lock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending(current);
    spin_unlock_irqrestore( &current->sigmask_lock, flags );

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,23))

    spin_lock_irqsave( &current->sighand->siglock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending();
    spin_unlock_irqrestore( &current->sighand->siglock, flags );

#else

    spin_lock_irqsave( &current->sigmask_lock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending(current);
    spin_unlock_irqrestore( &current->sigmask_lock, flags );

#endif

    dbg_call( 1,"Thread starting %s\n", name );

    do_exit = FALSE;
 
    do {

		int ret;

		ret = wait_event_interruptible( udev->req_queue_wait, (!list_empty(&udev->req_queue)) );

        while ((ndas_req = udeviod_get_next_task(udev, &urgent)) != NULL) {

			if (ndas_req->cmd == NDAS_CMD_STOP) {

			    dbg_call( 1, "NDAS_CMD_STOP\n" );
			
				if (udev->conn.ndas_locked[buflockIdx0] == true) {
			
					conn_lock_operation( &udev->conn, NDAS_LOCK_GIVE, buflockIdx0, NULL, NULL );
				}

			    spin_lock_irqsave( &udev->req_queue_lock, flags );
				list_del( &ndas_req->queue );
				spin_unlock_irqrestore( &udev->req_queue_lock, flags );

				ndas_req->next_done --;
				ndas_req->err = NDAS_OK;			

				ndas_req->done[ndas_req->next_done]( ndas_req, ndas_req->done_arg[ndas_req->next_done] );

				do_exit = true;

				break;
			}
				
			if (unlikely(!NDAS_SUCCESS(udev->err))) {

		        dbg_call( 1, "udev->err=%d\n", udev->err );
		
			    spin_lock_irqsave( &udev->req_queue_lock, flags );
				list_del( &ndas_req->queue );
				spin_unlock_irqrestore( &udev->req_queue_lock, flags );

				ndas_req->next_done --;
				ndas_req->err = udev->err;	

				ndas_req->done[ndas_req->next_done]( ndas_req, ndas_req->done_arg[ndas_req->next_done] );

				continue;
			}

			if (ndas_req->cmd != NDAS_CMD_READ && ndas_req->cmd != NDAS_CMD_WRITE) {

				if (ndas_req->cmd == NDAS_CMD_LOCKOP) {

					if (udev->conn.ndas_locked[buflockIdx0] == true) {
			
						conn_lock_operation( &udev->conn, NDAS_LOCK_GIVE, buflockIdx0, NULL, NULL );
					}
				}

				err = udeviod_dispatch( udev, ndas_req );

				goto reconnect;
			}

			NDAS_BUG_ON( ndas_req->cmd != NDAS_CMD_READ && ndas_req->cmd != NDAS_CMD_WRITE );

			if (udev->conn.ndas_locked[buflockIdx0] == true) {

				if (ndas_req->cmd != NDAS_CMD_READ) {

#ifdef NDAS_CRYPTO
					if (time_after(jiffies, acquire_jiffies + msecs_to_jiffies(400)))  
#else
					if (time_after(jiffies, acquire_jiffies + msecs_to_jiffies(200)))  
#endif
					{

						// for user program and the other hosts
						
						dbg_call( 1, "Timed: buffer lock release(%d ms)=%d\n", jiffies_to_msecs(jiffies), udev->err );

						conn_lock_operation( &udev->conn, NDAS_LOCK_GIVE, buflockIdx0, NULL, NULL );
						
						msleep( msecs_to_jiffies(10) );
					
					} /* else {

						if (time_after(jiffies, acquire_jiffies + msecs_to_jiffies(100))) {

							// for another host

							err = conn_lock_operation( &udev->conn, NDAS_LOCK_TAKE, buflockIdx1, NULL, NULL );
						
							if (NDAS_SUCCESS(err)) {

								conn_lock_operation( &udev->conn, NDAS_LOCK_GIVE, buflockIdx1, NULL, NULL );
						
							} else {

								conn_lock_operation( &udev->conn, NDAS_LOCK_GIVE, buflockIdx0, NULL, NULL );

								msleep( msecs_to_jiffies(10) );
							}
						}
					} */
				}
			}

			if (udev->conn.ndas_locked[buflockIdx0] == false) {

				bool firstlockfail;

				while (1) {

					err = conn_lock_operation( &udev->conn, NDAS_LOCK_TAKE, buflockIdx2, NULL, NULL );
						
					if (err == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {
						msleep( msecs_to_jiffies(5) );
						continue;
					}

					if (!NDAS_SUCCESS(err)) {

						goto reconnect;
					}

					break;
				}

				firstlockfail = true;

				while (1) {

					err = conn_lock_operation( &udev->conn, NDAS_LOCK_TAKE, buflockIdx1, NULL, NULL );
						
					if (err == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {

						if (firstlockfail == true) {

							msleep( msecs_to_jiffies(10) );
							firstlockfail = false;
							
						} else {

							msleep( msecs_to_jiffies(1) );
						}

						continue;
					}

					if (!NDAS_SUCCESS(err)) {

						goto reconnect;
					}

					err = conn_lock_operation( &udev->conn, NDAS_LOCK_GIVE, buflockIdx2, NULL, NULL );

					break;
				}

				firstlockfail = true;

				while (1) {

					err = conn_lock_operation( &udev->conn, NDAS_LOCK_TAKE, buflockIdx0, NULL, NULL );
						
					if (err == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {

						if (firstlockfail == true) {

							msleep( msecs_to_jiffies(10) );
							firstlockfail = false;
							
						} else {

							msleep( msecs_to_jiffies(1) );
						}

						continue;
					}

					if (!NDAS_SUCCESS(err)) {

						goto reconnect;
					}

					if (NDAS_SUCCESS(err)) {

						conn_lock_operation( &udev->conn, NDAS_LOCK_GIVE, buflockIdx1, NULL, NULL );
						dbg_call( 6, "buffer lock acquired(%d ms)=%d\n", jiffies_to_msecs(jiffies), udev->err );
						acquire_jiffies = jiffies;
					}

					break;
				}
			}
			
            err = udeviod_dispatch( udev, ndas_req );

reconnect:

			if (err != NDAS_OK && err != NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED && 
				err != NDAS_ERROR_ACQUIRE_LOCK_FAILED && !NDAS_SUCCESS(err)) {

				unsigned long	start_jiffies = jiffies;
				ndas_error_t	connet_err = NDAS_ERROR_NETWORK_FAIL;

			    dbg_call( 1, "ndas_req->cmd = %d err = %d\n", ndas_req->cmd, err );

				NDAS_BUG_ON( err != NDAS_ERROR_NETWORK_FAIL );

			    while (time_before(jiffies, start_jiffies + NDAS_RECONNECT_TIMEOUT)) {

				    if (udev->conn.socket > 0 ) {

					    conn_disconnect( &udev->conn );
    				}

		   			conn_reinit( &udev->conn );

		    		connet_err = conn_connect( &udev->conn, NDAS_RECONNECT_TIMEOUT, 1 );
	
					if (NDAS_SUCCESS(connet_err)) {

				    	connet_err  = conn_handshake( &udev->conn );
					}

					if (NDAS_SUCCESS(connet_err)) {

						break;
					}			
				}

				if (NDAS_SUCCESS(connet_err)) {

					int i;

					for (i=0; i<NDAS_LOCK_COUNT-1; i++) {

						udev->conn.ndas_locked[NDAS_LOCK_COUNT - i - 1] = false;
					}

					if (udev->conn.ndas_locked[NDAS_LOCK_COUNT - 1] == true) {

						while (1) {

							connet_err = conn_lock_operation( &udev->conn, NDAS_LOCK_TAKE, NDAS_LOCK_COUNT - 1, NULL, NULL );
						
							if (connet_err == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {

								continue;
							}

							break;
						}
					}

					continue;
				}
			}

			if (err != NDAS_OK && err != NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED && 
				err != NDAS_ERROR_ACQUIRE_LOCK_FAILED && !NDAS_SUCCESS(err)) {

				NDAS_BUG_ON( err != NDAS_ERROR_NETWORK_FAIL );

        		dbg_call( 1, "cmd=%d, err=%d\n", ndas_req->cmd, err );
				udev->err = err;

				ndev_pnp_changed( udev->ndev->info.ndas_id, udev->ndev->info.name, NDAS_DEV_STATUS_OFFLINE );
    		}

			dbg_call( 4, "ndas_req=%p\n", ndas_req );

		    spin_lock_irqsave( &udev->req_queue_lock, flags );
			list_del( &ndas_req->queue );
			spin_unlock_irqrestore( &udev->req_queue_lock, flags );

			ndas_req->next_done --;
			ndas_req->err = err;			

			ndas_req->done[ndas_req->next_done]( ndas_req, ndas_req->done_arg[ndas_req->next_done] );
        }

		if (udev->conn.ndas_locked[buflockIdx0] == true) {

			dbg_call( 6, "Idle: buffer lock release(%d ms)=%d\n", jiffies_to_msecs(jiffies), udev->err );
			conn_lock_operation( &udev->conn, NDAS_LOCK_GIVE, buflockIdx0, NULL, NULL );
		}

	} while (!do_exit);

    dbg_call( 1, "Terminating ndiod thread slot=%d\n", udev->uinfo.slot );
    dbg_call( 2, "Terminating ndiod thread conn->sock=%p\n", udev->conn.socket );

	return 0;
}

static void udev_queue_request(udev_t *udev, ndas_request_t *ndas_req)
{
	unsigned long flags;
   	
	dbg_call( 5, "ing slot=%d\n", udev->uinfo.slot );

    if (ndas_req->req_iov_num == 1) {

        dbg_call( 6, "slot=%d blocks=%p\n", udev->uinfo.slot, ndas_req->req_iov );
    
	} else {

        dbg_call( 6, "slot=%d nr_uio=%ld, blocks=%p\n", udev->uinfo.slot, (unsigned long)ndas_req->req_iov_num, ndas_req->req_iov );
    }

    spin_lock_irqsave( &udev->req_queue_lock, flags );

    if (ndas_req->urgent) {
 
        list_add( &ndas_req->queue, &udev->req_queue );
	
	} else {

        list_add_tail( &ndas_req->queue, &udev->req_queue );
	}

	spin_unlock_irqrestore( &udev->req_queue_lock, flags );

	wake_up( &udev->req_queue_wait );

    dbg_call( 5, "ed\n" );

    return;
}

static void  
udev_io(udev_t *udev, ndas_request_t *ndas_req)
{
    udev_queue_request(udev, ndas_req);
}

static ndas_error_t udev_enable(udev_t *udev, int flags)
{
    ndas_error_t err;

	ndas_conn_mode_t mode;

    bool writemode  = ENABLE_FLAG_WRITABLE(flags);
    bool writeshare = ENABLE_FLAG_WRITESHARE(flags);

    bool pnp 		= ENABLE_FLAG_PNP(flags);

    dbg_call( 1, "slot=%d conn=%p, unit=%d\n", udev->uinfo.slot, &udev->conn, udev->uinfo.unit );
    dbg_call( 1, "err=%d status=%x\n", udev->err, udev->conn.status );

	udev->connect_flags = flags;
	udev->err = 0;

    if (writeshare) {

        mode = NDAS_CONN_MODE_WRITE_SHARE;

    } else if (writemode) {

        mode = NDAS_CONN_MODE_EXCLUSIVE_WRITE;

    } else {

        mode = NDAS_CONN_MODE_READ_ONLY;
    }

    conn_init( &udev->conn, udev->ndev->network_id, udev->uinfo.unit, mode );

    err = conn_connect( &udev->conn, NDAS_RECONNECT_TIMEOUT, 5 );

#ifdef NDAS_HIX

    if (err == NDAS_ERROR_NO_WRITE_ACCESS_RIGHT && writemode) {

        // send the request for surrending the exclusive write permission

        dbg_call( 1, "someone else has the write permission already\n" );

        if (pnp) {

            err = NDAS_ERROR_TRY_AGAIN;
			goto out; 
        }

        err = NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
		goto out;
    }

#else

    if (err == NDAS_ERROR_NO_WRITE_ACCESS_RIGHT && writemode && pnp) {

		dbg_call( 1, "fail to connnect\n" );

        err = NDAS_ERROR_TRY_AGAIN;
		goto out;
    }

#endif

    if (!NDAS_SUCCESS(err)) {

        dbg_call( 1, "fail to conn_connect\n" );
        goto out1;
    }

    err  = conn_handshake(&udev->conn);

    if (!NDAS_SUCCESS(err)) {

		dbg_call( 1, "fail to connnect\n" );
        goto out1;
    }

	
    if (udev->ndev->info.version != udev->conn.hwdata->hardware_version) {

		dbg_call( 1, "udev->ndev->info.version = %d, udev->conn.hwdata->hardware_version = %d\n",
					 udev->ndev->info.version, udev->conn.hwdata->hardware_version );
		goto out1;
	}

    if (writeshare) {

        if (udev->ndev->info.version == NDAS_VERSION_1_0) {

            dbg_call( 1, "Hardware does not support shared-write\n" );
            err = NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION;
            goto out1;
        }
    }

    dbg_call( 1, "ed\n" );

    udev->udeviod_pid = kernel_thread( udeviod_thread, udev, 0 );

	if (udev->udeviod_pid <= 0) {

		NDAS_BUG_ON( udev->udeviod_pid == 0 );
		NDAS_BUG_ON(true);
        dbg_call( 1, "fail to create thread\n" );
        err = udev->udeviod_pid;
		goto out1;
    }

	return NDAS_OK;

out1:

    conn_logout( &udev->conn );
    conn_disconnect( &udev->conn );

out:
    conn_clean( &udev->conn );

	return err;
}

static void udev_invalidate_requests(udev_t *udev)
{
    ndas_request_t	*ndas_req;
	unsigned long 	flags;

    dbg_call( 2, "ing slot=%d\n", udev->uinfo.slot );

    spin_lock_irqsave( &udev->req_queue_lock, flags );

	while (!list_empty(&udev->req_queue)) {

		ndas_req = list_entry(udev->req_queue.next, ndas_request_t, queue);

	    NDAS_REQ_ASSERT(ndas_req);
    
        list_del( &ndas_req->queue );

		spin_unlock_irqrestore( &udev->req_queue_lock, flags );

		NDAS_BUG_ON( ndas_req->next_done == 0 );
		NDAS_BUG_ON( udev->uinfo.slot != ndas_req->slot );

		ndas_req->next_done --;

		ndas_req->err = udev->err;
		ndas_req->done[ndas_req->next_done]( ndas_req, ndas_req->done_arg[ndas_req->next_done] );

	    spin_lock_irqsave( &udev->req_queue_lock, flags );
	} 

	spin_unlock_irqrestore( &udev->req_queue_lock, flags );

    dbg_call( 2, "ed\n" );

	return;
}

static void v_udev_disable_done(ndas_request_t *ndas_req, void *arg)
{
	struct completion *completion = (struct completion *)arg;

	complete( completion );
}

static void v_udev_disable(udev_t *udev)
{
	ndas_request_t *ndas_req;

    dbg_call( 1, "ing slot=%d\n", udev->uinfo.slot );

	ndas_req = ndas_request_alloc();
	
	memset( ndas_req, 0, sizeof(ndas_request_t) );

#ifdef DEBUG
    ndas_req->magic = NDAS_REQ_MAGIC;
#endif

	ndas_req->slot = udev->uinfo.slot;

	INIT_LIST_HEAD( &ndas_req->queue );

    ndas_req->cmd = NDAS_CMD_STOP;

	init_completion( &ndas_req->completion );

    ndas_req->done[ndas_req->next_done]	    = v_udev_disable_done;
    ndas_req->done_arg[ndas_req->next_done] = &ndas_req->completion;
	ndas_req->next_done ++;

    udev_queue_request( udev, ndas_req );

	wait_for_completion( &ndas_req->completion );

	ndas_request_free(ndas_req);

    dbg_call( 1, "ed\n" );

	return;
}

static void udev_disable(udev_t *udev, ndas_error_t err)
{
    dbg_call( 1, "ing slot=%d\n", udev->uinfo.slot );

    NDAS_BUG_ON( udev->udeviod_pid <= 0 );

	if (udev->udeviod_pid > 0) {

	    v_udev_disable(udev);
		udev->udeviod_pid = 0;
	}

	udev->err = NDAS_ERROR_NO_DEVICE;

    udev_invalidate_requests(udev);

	if (udev->conn.socket != NULL) {

        conn_logout( &udev->conn );
        conn_disconnect( &udev->conn );
    }

    conn_clean( &udev->conn );

	return;
}

static ndas_error_t udev_init(sdev_t *sdev, udev_t *udev) 
{
    dbg_call( 1, "udev=%p\n", udev );

	udev->sdev 		 = sdev;
	udev->uinfo.slot = sdev->info.slot;
	udev->err	 	 = NDAS_OK;

    return NDAS_OK;
}

static void udev_deinit(udev_t *udev) 
{
    dbg_call( 1, "udev = %p\n", udev );
}

void udev_set_disk_information(udev_t *udev, const lsp_ata_handshake_data_t* hsdata)
{
	if (udev->uinfo.disk_capacity_set == false) {

        udev->uinfo.hard_sector_size = hsdata->logical_block_size;

		NDAS_BUG_ON( udev->uinfo.hard_sector_size != SECTOR_SIZE );

		udev->uinfo.hard_sectors = hsdata->lba_capacity.quad;

		NDAS_BUG_ON( hsdata->num_heads >= 256 );
		NDAS_BUG_ON( hsdata->num_sectors_per_track >= 256 );

   	    udev->uinfo.headers_per_disk = hsdata->num_heads;
       	udev->uinfo.cylinders_per_header = hsdata->num_cylinders;
       	udev->uinfo.sectors_per_cylinder = hsdata->num_sectors_per_track;

        memcpy( udev->uinfo.serialno, hsdata->serial_number, sizeof(udev->uinfo.serialno) );
    	memcpy( udev->uinfo.model, hsdata->model_number, sizeof(udev->uinfo.model) );
        memcpy( udev->uinfo.fwrev, hsdata->firmware_revision, sizeof(udev->uinfo.fwrev) );

		udev->uinfo.disk_capacity_set = true;
	}

	return;
}

udev_t *udev_create(ndev_t *ndev, int unit)
{
    udev_t *udev;

    dbg_call( 1, "ing node=%p unit=%d\n", ndev->network_id, unit );

    udev = ndas_kmalloc( sizeof(udev_t) );

    if (!udev) {
 
		NDAS_BUG_ON(true);
		return NULL;
	}

	memset( udev, 0, sizeof(udev_t) );

#ifdef DEBUG
    udev->magic = UDEV_MAGIC;
#endif 

    udev->uinfo.raid_slot = NDAS_FIRST_SLOT_NR - 1;

    udev->ndev  = ndev;

    INIT_LIST_HEAD( &udev->req_queue );
	spin_lock_init( &udev->req_queue_lock );
	init_waitqueue_head( &udev->req_queue_wait );

#ifdef NDAS_HIX
    sema_init( &udev->hix_sema, 0 );
#endif

    return udev;
}

void udev_cleanup(udev_t *udev) 
{
    ndas_kfree(udev);
	return;
}
