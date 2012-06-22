/*
    to do: Change udev as unit device not single type logical unit. Need to create single type logical unit device types.
*/
#include "xplatcfg.h"
#include <sal/types.h>
#include <ndasuser/ndasuser.h>
#include "netdisk/netdiskid.h"
#include "netdisk/ndasdib.h"
#include "netdisk/sdev.h"
#include "raid/bitmap.h"
#include "udev.h"
#include "ndas_scsi_cmd_fmt.h"
#include "netdisk/conn.h"
#include "xlib/gtypes.h"
#include "registrar.h"
#include "netdisk/scrc32.h"
#include "lockmgmt.h"


#ifndef NDAS_NO_LANSCSI

#define NDAS_SET_ENC_TIMEOUT (15*1000)

#ifdef DEBUG
#define    debug_ndiod(l, x...)    do {\
    if(l <= DEBUG_LEVEL_NDIOD) {      \
        sal_debug_print("NDIOD|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x); \
    }    \
} while(0)
#define    debug_udev(l, x...)    do {\
    if(l <= DEBUG_LEVEL_UDEV) {     \
        sal_debug_print("UD|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)    
#define    debug_tir(l, x...)    do {\
    if(l <= DEBUG_LEVEL_TIR) {     \
        sal_debug_print("TR|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)    
#else
#define    debug_ndiod(l, x...)    do {} while(0)
#define    debug_udev(l, x...)    do {} while(0)
#define    debug_tir(l, x...)    do {} while(0)
#endif

#define MEMORY_FREE(ptr) if ( ptr ) { sal_free(ptr); ptr = NULL; }


/* max buffer size. todo remove this buffer */
int udev_max_xfer_unit = ND_MAX_BLOCK_SIZE;

LOCAL
NDAS_CALL void v_uop_enabled(int slot, ndas_error_t err, logunit_t* log_udev);

LOCAL
ndas_error_t uop_init(logunit_t *log_udev);

LOCAL
void uop_cleanup(logunit_t *log_udev);

LOCAL
void uop_enable(logunit_t * log_udev, int flag);

LOCAL
void uop_disable(logunit_t * log_udev, ndas_error_t err);
#ifdef XPLAT_ASYNC_IO
LOCAL 
void uop_io(logunit_t *log_udev, struct udev_request_block *tir, xbool urgent);

LOCAL void
udev_queue_request(udev_t *udev, urb_ptr tir, xbool urgent);

#endif

#if 0
LOCAL
ndas_error_t uop_writable(logunit_t *log_udev);
#endif

LOCAL 
int uop_status(logunit_t *log_udev) {
    return SDEV2UDEV(log_udev)->conn.status;
}

#if 0
LOCAL
ndas_error_t uop_query(logunit_t *log_udev, ndas_metadata_t *nmd);
#endif
LOCAL
ndas_error_t uop_create(logunit_t *log_udev, sdev_create_param_t* param);
LOCAL void uop_deinit(logunit_t *log_udev);

struct ops_s udev_ops = {
    .create_disks = (create_disks_func) uop_create,
    .destroy_disks = (destroy_disks_func) uop_cleanup,
    .init = (init_func) uop_init,
    .deinit = (deinit_func) uop_deinit,
    .enable = (enable_func) uop_enable,
    .disable = (disable_func) uop_disable,
//    .writable = (writable_func) uop_writable,
#ifdef XPLAT_ASYNC_IO    
    .io = (io_func) uop_io,
#else
    .io = NULL, /* Not used */
#endif
#if 0
    .query = (query_func) uop_query,
#endif    
    .status = (status_func) uop_status,
    .changed = NULL,
};


struct ndas_lock_io_blocking_done_arg {
    sal_semaphore done_sema;
    ndas_error_t err;
};


struct ndas_lock_oprand {
	NDAS_LOCK_OP op;
	int lockid;
	void* lockdata;    
	void* opt;
};

/*
    To do: integrate into lockmgmt.
*/
ndas_error_t
ndas_lock_aop_operation(udev_t *udev, urb_ptr tir)
{
    struct ndas_lock_oprand * oprand = NULL;

    if(tir == NULL) {
    	return NDAS_ERROR_INVALID_PARAMETER ;
    }

    if(tir->req->done_arg == NULL) {
    	return NDAS_ERROR_INVALID_PARAMETER ;
    }

    oprand = (struct ndas_lock_oprand *)tir->arg;


    if( udev == NULL) {
    	return NDAS_ERROR_NO_DEVICE;
    }

    if (oprand->op == NDAS_LOCK_TAKE) {
        return OpAcquireDevLock(&udev->conn, 
                oprand->lockid, NULL, 3 * 1000, TRUE);
    } else if (oprand->op == NDAS_LOCK_GIVE) {
        return OpReleaseDevLock(&udev->conn, oprand->lockid, NULL, 3 * 1000);
    } else {
        return conn_lock_operation(&(udev->conn), oprand->op, oprand->lockid, oprand->lockdata, oprand->opt);
    }
}

void ndas_lock_aop_done(int slot, ndas_error_t err, void* arg)
{
    struct ndas_lock_io_blocking_done_arg* darg = (struct ndas_lock_io_blocking_done_arg*)arg;
    darg->err = err;
    debug_udev(0,"CALL ndas_lock_aop_done!");
    sal_semaphore_give((sal_semaphore)darg->done_sema);
}

ndas_error_t 
udev_io_idle_operation(udev_t *udev)
{
#ifdef XPLAT_ASYNC_IO
    urb_ptr tir = NULL; 

    /* Send IO idle message */
    debug_udev(4,"udev_io_idle_operation");
    tir = tir_alloc(ND_CMD_IO_IDLE, NULL);
    tir->func = NULL;
    tir->arg = NULL;

    udev_queue_request(udev, tir, FALSE);
    return NDAS_OK;
#else
    return NDAS_OK;
#endif //#ifdef XPLAT_ASYNC_IO
}

/*
    Currently this is only used for xixfs lock. 
    
    to do: 
        do not use tir->func.
        Integrate into lockmgmt.
*/
ndas_error_t 
udev_lock_operation(udev_t *udev, NDAS_LOCK_OP op, int lockid, void* lockdata, void* opt)
{
#ifdef XPLAT_ASYNC_IO
   urb_ptr tir = NULL; 
   struct ndas_lock_io_blocking_done_arg  darg ;
   struct ndas_lock_oprand  oprand;
   int err;

    /* Currently udev_lock_operation is used only by xixfs */
    sal_assert(lockid == 2);
    
   oprand.lockid = lockid;
   oprand.op = op;
   oprand.lockdata = lockdata;
   oprand.opt = opt;
   
    debug_udev(4,"ed");

    tir = tir_alloc(ND_CMD_LOCKOP, NULL);
    tir->func = ndas_lock_aop_operation;
    tir->arg = &oprand;
    tir->req->done = ndas_lock_aop_done;
    tir->req->done_arg = &darg;
    darg.done_sema = sal_semaphore_create("udev_lock_io_done", 1, 0);  

    udev_queue_request(udev, tir, FALSE);

    do {
        err = sal_semaphore_take(darg.done_sema, SAL_SYNC_FOREVER);
    } while (err == SAL_SYNC_INTERRUPTED);

    sal_semaphore_give(darg.done_sema);
    sal_semaphore_destroy(darg.done_sema);
	
    return darg.err;
#else
    return conn_lock_operation(&(udev->conn), op, lockid, lockdata, opt);
#endif //#ifdef XPLAT_ASYNC_IO
}

/*
    To do: 
        Change this function to operation on unit device.
*/
NDASUSER_API ndas_error_t 
    ndas_lock_operation(int slot, NDAS_LOCK_OP op, int lockid, void* lockdata, void* opt)
{
    logunit_t * log_udev = sdev_lookup_byslot(slot);
    
    if (log_udev==NULL) {
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

    return udev_lock_operation(SDEV2UDEV(log_udev), op, lockid, lockdata, opt);
}


void udev_set_max_xfer_unit(int io_buffer_size) {
    udev_max_xfer_unit = (io_buffer_size > 0) ? io_buffer_size : ND_MAX_BLOCK_SIZE;
    debug_udev(1, "io_buffer_size=%d", io_buffer_size);
}

LOCAL
ndas_error_t uop_create(logunit_t *log_udev, sdev_create_param_t* param)
{
    udev_t *udev;
    ndas_error_t err = NDAS_ERROR_OUT_OF_MEMORY;
    debug_udev(1, "ing node=%p unit=%d", param->u.single.ndev->network_id, param->u.single.unit);
    
    udev = sal_malloc(sizeof(udev_t));
    if ( !udev ) return NDAS_ERROR_OUT_OF_MEMORY;

    sal_memset(udev, 0, sizeof(udev_t));
#ifdef DEBUG
    udev->magic = UDEV_MAGIC;
#endif 
    log_udev->info2 = sal_malloc(sizeof(ndas_unit_info_t));
    if ( !log_udev->info2 ) {
        sal_free(udev);
        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    sal_memset(log_udev->info2, 0, sizeof(ndas_unit_info_t));    
    SDEV_UNIT_INFO(log_udev)->raid_slot = NDAS_FIRST_SLOT_NR - 1;
        
    udev->ndev = param->u.single.ndev;
    udev->unit = param->u.single.unit;
    udev->info = &log_udev->info;
    udev->uinfo = log_udev->info2;
    debug_udev(3, "udev=%p", udev);
    debug_udev(3, "udev->info=%p", udev->info);
    debug_udev(3, "log_udev->info2=%p", log_udev->info2);
        
    udev->info->unit = param->u.single.unit;
    
#ifdef NDAS_MSHARE
     
    if(udev->Disk_info == NULL)
    {
	struct nd_diskmgr_struct * diskmgr;
    	diskmgr = sal_malloc(sizeof(struct nd_diskmgr_struct));
    	udev->Disk_info = diskmgr;
    	if(diskmgr) {
    		int i = 0;	
    		sal_memset(diskmgr, 0, sizeof(struct nd_diskmgr_struct));
    
    		for(i = 0; i < MAX_PART ; i++)
    		{
    			diskmgr->disc_part_map[i] = -1;
    		}
    		
    		diskmgr->MetaKey = sal_malloc(sizeof(struct media_key));
    		if(!diskmgr->MetaKey) {
    			debug_udev(1, "Can't alloc meida_key\n");
    			sal_debug_println("Can't alloc meida_key\n");
    			sal_free(diskmgr);
			sal_free(log_udev->info2);
    			udev->Disk_info = NULL;
    			goto out1;
    		}
    
    		sal_memset(diskmgr->MetaKey,0,sizeof(struct media_key));

		// Make Meta Key
		if(!MakeMetaKey(diskmgr))
		{
		    	debug_udev(1, "Can't Make MetaKey\n");
		    	sal_debug_println("Can't Make MetaKey\n");
    			sal_free(diskmgr->MetaKey);
    			sal_free(diskmgr);
			sal_free(log_udev->info2);
    			udev->Disk_info = NULL;
    			goto out1;
		}	
		
    		diskmgr->mediainfo = sal_malloc(sizeof(struct media_disc_map));
    		if(!diskmgr->mediainfo)
    		{
    			debug_udev(1, "Can't alloc media_disc_map\n");
    			sal_debug_println("Can't alloc media_disc_map\n");
    			sal_free(diskmgr->MetaKey);
    			sal_free(diskmgr);
			sal_free(log_udev->info2);
    			udev->Disk_info = NULL;
    			goto out1;
    		}
    		sal_memset(diskmgr->mediainfo, 0, sizeof(struct media_disc_map));
    			
    	}else{
    		sal_debug_println("Can't alloc diskmgr\n");
    		goto out1;
    	}
    }
 
#endif

#ifdef XPLAT_ASYNC_IO    
    INIT_LIST_HEAD(&udev->request_head);

    if (!sal_spinlock_create("uq", &udev->lock))
        goto out1;
#else
    udev->io_lock = sal_semaphore_create("udev-iolock", 1, 1);
    if(!udev->io_lock) {
        goto out1;
    }
#endif

#ifdef XPLAT_NDASHIX
    udev->hix_sema = sal_semaphore_create("hix", 1, 0);
    if ( udev->hix_sema == SAL_INVALID_SEMAPHORE )
        goto out2;
#endif

    // Give the reference of single NDAS device to the logical device.
    log_udev->disks = (logunit_t **) udev;

    return NDAS_OK;

#ifdef XPLAT_NDASHIX
out2:
    sal_semaphore_destroy(udev->hix_sema);
#endif
#ifdef XPLAT_ASYNC_IO
out1:
    sal_spinlock_destroy(udev->lock);
#else
out1:
    sal_semaphore_destroy(udev->io_lock);
#endif
    sal_free(udev);
    return err;
}

LOCAL
void uop_cleanup(logunit_t *log_udev) 
{
    udev_t *udev = SDEV2UDEV(log_udev);
    if ( !log_udev || !udev ) return;

#ifdef XPLAT_NDASHIX
    if ( udev->hix_sema != SAL_INVALID_SEMAPHORE)
        sal_semaphore_destroy(udev->hix_sema);
#endif        
#ifdef XPLAT_ASYNC_IO
    sal_spinlock_destroy(udev->lock);
#else
    sal_semaphore_destroy(udev->io_lock);
#endif

#ifdef  NDAS_MSHARE
{
    int i;
    struct nd_diskmgr_struct * diskmgr;

    diskmgr = udev->Disk_info;
    
    if(diskmgr->MetaKey) sal_free(diskmgr->MetaKey);
    if(diskmgr->mediainfo) sal_free(diskmgr->mediainfo);
    for(i = 0; i<8; i++){
    	if(diskmgr->Disc_info[i] != NULL)
    	{
	  	if(diskmgr->Disc_info[i]->disc_key)
			sal_free(diskmgr->Disc_info[i]->disc_key);
		if(diskmgr->Disc_info[i]->disc_info)
			sal_free(diskmgr->Disc_info[i]->disc_info);
		if(diskmgr->Disc_info[i])
			sal_free(diskmgr->Disc_info[i]);
		diskmgr->Disc_info[i] = NULL;  		
    	}
    }
    sal_free(udev->Disk_info);
}
#endif

    sal_free(udev);
    log_udev->disks = NULL;
}

#if 0
/* Called by ndas_query_slot and raid query functions
 */
LOCAL
ndas_error_t uop_query(logunit_t *log_udev, ndas_metadata_t *nmd) 
{
    ndas_error_t err;
    udev_t *udev = SDEV2UDEV(log_udev);
    debug_udev(3, "log_udev=%p", log_udev);
    debug_udev(5, "slot=%d conn=%p", log_udev->info.slot, &udev->conn);
    debug_udev(5, "slot=%d status=%x", log_udev->info.slot, udev->conn.status);
    
    err = udev_query_unit(udev, nmd);
    if ( !NDAS_SUCCESS(err) ) {
        debug_udev(1, "err=%d", err);
        return err;
    }
    return NDAS_OK;
}
#endif

LOCAL void uop_deinit(logunit_t *log_udev) 
{
    debug_udev(1, "slot=%d", log_udev->info.slot);
#ifdef XPLAT_RAID
    log_udev->private = NULL;
#endif
}
/*
 * called by sdev_create
 */
LOCAL
ndas_error_t uop_init(logunit_t *log_udev) 
{
    
    ndev_t* ndev = SDEV2UDEV(log_udev)->ndev;
    debug_udev(1, "udev=%p", SDEV2UDEV(log_udev));
#ifdef XPLAT_SERIAL    
    log_udev->has_key = TRUE;
#else
    log_udev->has_key = ndev->info.ndas_key[0] ? TRUE: FALSE;
#endif        
    sal_strncpy(log_udev->info.ndas_serial, ndev->info.ndas_serial, NDAS_SERIAL_LENGTH+1);
    
    ndev->info.slot[log_udev->info.unit] = log_udev->info.slot;
    log_udev->info.io_splits = 1;
    log_udev->info.mode = NDAS_DISK_MODE_SINGLE;
    log_udev->info.mode_role = 0;
//    log_udev->info.type = NDAS_UNIT_TYPE_HARD_DISK;
    log_udev->info.sector_size = 1 << 9;
    
    return NDAS_OK;
}

#if 0

LOCAL
ndas_error_t uop_writable(logunit_t *log_udev) 
{
    udev_t *udev = SDEV2UDEV(log_udev);

    return  udev->info->writable ==TRUE;
}
#endif

#ifdef XPLAT_ASYNC_IO

#ifdef XPLAT_RAID
struct udev_request_block *
tir_clone(urb_ptr tir) 
{
    urb_ptr ret = sal_alloc_from_pool(Urb_pool, struct udev_request_block));
    if ( !ret ) 
        return NULL;
    sal_memcpy(ret, tir, sizeof(struct udev_request_block));
    // If the URB uses external NDAS request, copy it to the new one.
    ret->req = &ret->int_req;
    if(tir->req != &tir->int_req) {
        sal_memcpy(ret->req, tir->req, sizeof(ret->int_req));
    }

    debug_tir(3, "clone ret=%p", ret);

    return ret;
}
#endif

/*
    To do: 
        use pool mem
        Don't use ndas_io_request_ptr. It is user interface structure.
*/
struct udev_request_block *
tir_alloc(xint8 cmd, ndas_io_request_ptr req) 
{
    urb_ptr tir = sal_alloc_from_pool(Urb_pool, sizeof(struct udev_request_block));

    if ( !tir ) return NULL;

#ifdef DEBUG
    tir->magic = TIR_MAGIC;
#endif
    INIT_LIST_HEAD(&tir->queue);
    tir->func = NULL;
    tir->cmd = cmd;
    // If the caller does not specify NDAS request, use the internal one.
    if(req) {
        tir->req = req;
    } else {
        sal_memset(&tir->int_req, 0, sizeof(ndas_io_request));
        tir->req = &tir->int_req;
    }
    tir->arg = NULL;

    debug_tir(3, "alloc tir=%p", tir);

    return tir;
}

void tir_free(urb_ptr tir) {
    debug_tir(3, "free tir=%p", tir);
    sal_free_from_pool(Urb_pool, tir);
}

LOCAL
ndas_error_t udev_shuting_down(udev_t *udev, urb_ptr tir)
{
    debug_udev(2, "ing slot=%d", udev->info->slot);
    return NDAS_OK;
}

/**
 * Shut down the unit device 
 * Called by 
 *  uop_disable(ndas_disable_slot) : user thread
 **/
LOCAL void udev_request_shutdown(udev_t *udev)
{
    urb_ptr tir;
    debug_udev(2, "ing slot=%d", udev->info->slot);
//    udev->thread.exit_requested = TRUE;

    tir = tir_alloc(ND_CMD_DISCONNECT, NULL);
    if ( !tir ) {
        sal_error_print("ndas: out of memory to shut down slot %d\n", udev->info->slot);   
        return;
    }
    tir->func = udev_shuting_down;
    tir->arg = NULL;
    udev_queue_request(udev, tir, FALSE);
}

LOCAL 
struct udev_request_block* ndiod_get_next_task(udev_t *udev)
{
    struct udev_request_block* treq;

    debug_ndiod(4, "ing slot=%d", udev->info->slot );
    sal_spinlock_take_softirq(udev->lock);

    if( list_empty(&udev->request_head)) 
    {
        debug_ndiod(3, "ed slot=%d udev->request_head is no_pending io", udev->info->slot);
        goto out;
    } 

    treq = list_entry(udev->request_head.next, struct udev_request_block, queue);
    list_del(&treq->queue);

    sal_spinlock_give_softirq(udev->lock);

    debug_ndiod(4, "ed slot=%d treq=%p", udev->info->slot, treq);
    return treq;
out:

    sal_spinlock_give_softirq(udev->lock);
    return NULL;
}

static
int
ndiod_dispatch(udev_t *udev, struct udev_request_block *tir)
{
    uconn_t            *conn = &udev->conn;
    ndas_error_t res;
    xbool locklost;
    xbool do_exit = FALSE;

    debug_ndiod(4,"waiting slot=%d", udev->info->slot);

    if ( conn->status != CONN_STATUS_CONNECTED ) 
    {
        if ( tir->cmd == ND_CMD_READ || tir->cmd == ND_CMD_WRITE ) 
        {
            debug_ndiod(1, "status=%d", conn->status);
            if ( tir->req->done ) 
                tir->req->done(udev->info->slot, NDAS_ERROR_SHUTDOWN_IN_PROGRESS, tir->req->done_arg);
            do_exit = TRUE;
            goto out;
        }
    }

#ifdef DEBUG
    if ( tir->cmd == ND_CMD_CONNECT ) {
        debug_ndiod(2, "slot=%d connecting", udev->info->slot);
    }
#endif

    if ( tir->func ) {
        res = tir->func(udev, tir);
    } else {
        switch(tir->cmd) {
        case ND_CMD_READ:
        case ND_CMD_WRITE:
	case ND_CMD_WV:
	{
		if(udev->BuffLockCtl.BufferLockConrol) {
                // Check to see if this IO is in the range of a lost device lock.
                	locklost = LockCacheCheckLostLockIORange(
					&udev->lock_info,
					tir->req->start_sec,
					tir->req->start_sec +  tir->req->num_sec -1
					);
			if (locklost) {
                    		debug_ndiod(1, "Lock lost. Failing operation");
                    		res = NDAS_ERROR_LOCK_LOST;
                    		break;
                	}

                	StopIoIdleTimer(&udev->BuffLockCtl);
            	}

			
	    	if(tir->cmd == ND_CMD_WV) {
			res = conn_do_ata_write_verify(conn, tir->req);
            	} else {
			res = conn_do_ata_rw(conn, tir->cmd, tir->req);
            	}
			
			
            	if(udev->BuffLockCtl.BufferLockConrol) {
                	StartIoIdleTimer(&udev->BuffLockCtl, udev);
            	}
	}
	break;
	case ND_CMD_FLUSH:
	{
		res = conn_do_ata_flush(conn, tir->req);
	}
	break;
	case ND_CMD_VERIFY:
	{
		res = conn_do_ata_verify(conn, tir->req);
	}
	break;
	case ND_CMD_PACKET:
	{		
		res = conn_do_atapi_cmd(conn,  tir->req);	
	}
	break;
	case ND_CMD_PASSTHROUGH:
	{
		res = conn_do_ata_passthrough(conn, tir->req);
	}
	break;
	case ND_CMD_SET_FEATURE:
	{
		res = conn_do_ata_set_feature(conn, tir->req);
	}
	break;
	case ND_CMD_HANDSHAKE:
	{
		res = conn_handshake(conn);
	}
	break;
        case ND_CMD_IO_IDLE:
            res = EnterBufferLockIoIdle( &udev->BuffLockCtl, conn, &udev->lock_info);
            if (!NDAS_SUCCESS(res)) {
                debug_ndiod(1, "EnterBufferLockIoIdle failed:%d\n", res);
                res = conn_reconnect(&udev->conn);
                LockCacheAllLocksLost(&udev->lock_info);
            }
            break;
        case ND_CMD_DISCONNECT:
            res = NDAS_OK;
            conn_set_error(conn, NDAS_ERROR_SHUTDOWN_IN_PROGRESS);
            break;
        default:
            res = NDAS_ERROR_INTERNAL;
            sal_assert(FALSE);
            break;
        }
        /* Mark this device alive while it response. We may miss PNP packet while IO */
#ifdef XPLAT_PNP
        udev->ndev->last_tick = sal_get_tick();
#endif
    }

#ifdef DEBUG
    if ( tir->cmd == ND_CMD_CONNECT ) {
        debug_ndiod(2, "slot=%d connect err=%d", udev->info->slot, res);
    }
#endif


	if(tir->cmd == ND_CMD_PACKET) {
		switch(res){
			case NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND :
			case NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED :
			case NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION:
			case NDAS_ERROR_IDE_REMOTE_AUTH_FAILED :
			case NDAS_ERROR_HARDWARE_DEFECT:
			case NDAS_ERROR_IDE_TARGET_BROKEN_DATA :
			case NDAS_ERROR_BAD_SECTOR:
			case NDAS_ERROR_IDE_VENDOR_SPECIFIC :
				conn_do_atapi_req_sense(conn, tir->req);
				goto  process;
				break;
			default:
				break;
		}
	}

    if(tir->cmd == ND_CMD_PASSTHROUGH){
        switch(res) {
    		case NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND :
    		case NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED :
    		case NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION:
    		case NDAS_ERROR_IDE_REMOTE_AUTH_FAILED :
    		case NDAS_ERROR_HARDWARE_DEFECT:
    		case NDAS_ERROR_IDE_TARGET_BROKEN_DATA :
    		case NDAS_ERROR_BAD_SECTOR:
    		case NDAS_ERROR_IDE_VENDOR_SPECIFIC :
                res = NDAS_OK;
              break;
              default:
                break;
        }
    }
 
	
    if ( !NDAS_SUCCESS(res) )
    {
        debug_ndiod(1, "cmd=%d, err=%d", tir->cmd, res);
        conn_set_status(conn, CONN_STATUS_SHUTING_DOWN);
        conn_set_error(conn, res);
        do_exit = TRUE;
    }
process:
    if ( tir->req->done ) {
        tir->req->done(udev->info->slot, res, tir->req->done_arg);
        debug_ndiod(3, "tir done finished");
    }

out:
    tir_free(tir);
    return do_exit;
}

/* nd i/o deamon - ndas io handling thread */
void* ndiod_thread(udev_t *udev)
{
    uconn_t            *conn = &udev->conn;
    char name[32];
    xbool do_exit;
    struct udev_request_block *urb;
#ifdef NDASIOD_YIELD
    sal_tick                pre_time;
    sal_tick                running_time = 0;
#endif

    sal_thread_block_signals();
    sal_snprintf(name, sizeof(name), "ndiod/%d", udev->info->slot);
    sal_thread_daemonize(name); // change the name of process
    sal_thread_nice(-20);
    debug_ndiod(1,"Thread starting %s", name);

    do_exit = FALSE;
    while(!do_exit) 
    {

        sal_event_wait(udev->thread.idle, SAL_SYNC_FOREVER);
        sal_event_reset(udev->thread.idle);

        if ( conn->status == CONN_STATUS_SHUTING_DOWN || !NDAS_SUCCESS(conn->err) ) { 
            debug_ndiod(2, "status=%d err=%d", conn->status,conn->err);
            do_exit = TRUE;
            break;
        }

#ifdef NDASIOD_YIELD
        running_time = 0;
        pre_time = sal_get_tick();
#endif
        while ((urb = ndiod_get_next_task(udev)) != NULL){

            do_exit = ndiod_dispatch(udev, urb);
            if(do_exit) {
                break;
            }
#ifdef NDASIOD_YIELD
            running_time = sal_tick_add(running_time, sal_tick_sub(sal_get_tick(),pre_time));
            pre_time = sal_get_tick();
            if(running_time > 3 * SAL_TICKS_PER_SEC) {
                running_time = 0;
                sal_thread_yield();
            }
#endif
        }
    }

    debug_ndiod(1, "Terminating ndiod thread slot=%d", udev->info->slot);
    debug_ndiod(2, "Terminating ndiod thread conn->sock=%d",conn->sock);
    
    udev_shutdown(udev);
    sal_thread_exit(0);
    return 0;
}

LOCAL ndas_error_t ndiod_create(udev_t *udev)
{
    ndas_error_t err = NDAS_OK;
    udev->thread.idle = sal_event_create("ndiod_idle");

    if ( udev->thread.idle == SAL_INVALID_EVENT )
        goto out;

    /* to do: register event handler(data_ready, state_change, write_space) to socket */
    
    err = sal_thread_create(&udev->thread.tid, "ndiod_thread", -1, 0,
                                (void*(*)(void*)) ndiod_thread, udev);
    if(err < 0) {
        udev->thread.tid = SAL_INVALID_THREAD_ID;
        debug_ndiod(1, "failed to create ndiod thread");
    } else {
        debug_ndiod(1, "ndiod thread created for slot %d", udev->info->slot);
    }
    return err;
out:
    sal_event_destroy(udev->thread.idle);
    udev->thread.idle = SAL_INVALID_EVENT;
    return NDAS_ERROR_OUT_OF_MEMORY;
}

LOCAL void
udev_queue_request(udev_t *udev, struct udev_request_block *tir, xbool urgent)
{

    sal_assert(udev);
    sal_assert(udev->info);

    debug_ndiod(5, "ing slot=%d", udev->info->slot);

#ifdef DEBUG
    if ( tir->req->nr_uio == 1 ) {
        debug_ndiod(6, "slot=%d blocks=%p", udev->info->slot, tir->req->uio);
    } else {
        debug_ndiod(6, "slot=%d nr_uio=%d, blocks=%p", udev->info->slot, tir->req->nr_uio, tir->req->uio);
    }
#endif

    sal_spinlock_take_softirq(udev->lock);
    sal_assert( udev->thread.idle != SAL_INVALID_EVENT );
    if ( (udev->conn.status & CONN_STATUS_SHUTING_DOWN )) 
    {
        sal_spinlock_give_softirq(udev->lock);
        debug_ndiod(1, "status=%x", udev->conn.status);
        if ( tir->req->done )
            tir->req->done(udev->info->slot, NDAS_ERROR_SHUTDOWN_IN_PROGRESS, tir->req->done_arg);
        return;
    }
    if ( urgent ) 
        list_add(&tir->queue, &udev->request_head);
    else
        list_add_tail(&tir->queue, &udev->request_head);

    sal_spinlock_give_softirq(udev->lock);

    sal_event_set(udev->thread.idle);

    /* To do: need to handle the case that new task is queued continuously.. */
    debug_ndiod(5, "ed");
    return;
}

LOCAL void  
uop_io(logunit_t *log_udev, struct udev_request_block *tir, xbool urgent)
{
    if ( !log_udev->accept_io ) {
        if ( tir->cmd != ND_CMD_CONNECT) {
            debug_ndiod(1, "log_udev is not connected status");
            if ( tir->req->done )
                tir->req->done(log_udev->info.slot, NDAS_ERROR_SHUTDOWN_IN_PROGRESS, tir->req->done_arg);
            return;
        }
    }
    udev_queue_request(SDEV2UDEV(log_udev), tir, urgent);
}
/**
 * old: udev_invalidte, nd_invalidate_driver, nd_invalidate_queue
 */
LOCAL
void udev_invalidate_requests(udev_t *udev)
{
    struct udev_request_block    *tir;
    debug_udev(2, "ing slot=%d", udev->info->slot);
    do {
        tir = NULL;
        sal_spinlock_take_softirq(udev->lock);
        if(!list_empty(&udev->request_head)) {
            tir = list_entry(udev->request_head.next, struct udev_request_block, queue);
            TIR_ASSERT(tir);
            list_del(&tir->queue);
            sal_spinlock_give_softirq(udev->lock);

            sal_assert(!NDAS_SUCCESS(udev->conn.err));
            if ( tir->cmd == ND_CMD_DISCONNECT )
                tir->func(udev,tir);
            if ( tir->req->done) 
                tir->req->done(udev->info->slot, udev->conn.err, tir->req->done_arg);// TODO
            tir_free(tir);
            continue;
        } 
        sal_spinlock_give_softirq(udev->lock);
    } while(tir);
    debug_udev(2, "ed");
}

LOCAL
void ndiod_cleanup(struct ndiod *thread) 
{
    debug_udev(2, "ing");
    sal_event_destroy(thread->idle);
    thread->idle = SAL_INVALID_EVENT;
    thread->tid = 0;
}

#endif // end of #ifdef XPLAT_ASYNC_IO


#ifdef XPLAT_RAID

#define BITMAP_IO_UNIT (32*1024)

ndas_error_t
conn_read_bitmap(uconn_t *conn,
            xuint64 sector_count, 
              bitmap_t *bitmap)
{
    int i, j;
    ndas_error_t err = NDAS_OK;
    xuint64 start_sec;
    xuint32 num_sec;
    int nr_uio;
    int bitmap_size;
    int nr_io;
    struct sal_mem_block *uio;
    uio = sal_malloc(sizeof(struct sal_mem_block) * (BITMAP_IO_UNIT / bitmap->chunk_size));
    
    if ( !uio ) 
        return NDAS_ERROR_OUT_OF_MEMORY;

    bitmap_size = bitmap->chunk_size * bitmap->nr_chunks;

    nr_io = bitmap_size / BITMAP_IO_UNIT;
    
    for ( j = 0; j < nr_io; j++) 
    {
        start_sec = sector_count + NDAS_BLOCK_SIZE_XAREA + NDAS_BLOCK_LOCATION_BITMAP
                            + (BITMAP_IO_UNIT/512 * j);
        num_sec = BITMAP_IO_UNIT / 512;
        nr_uio = BITMAP_IO_UNIT / bitmap->chunk_size;
        
        for ( i = 0; i < BITMAP_IO_UNIT / bitmap->chunk_size; i++ ) 
        {
            int idx = j * (BITMAP_IO_UNIT / bitmap->chunk_size) + i;
            if ( idx < bitmap->nr_chunks) 
            {
                uio[i].ptr = bitmap->chunks[idx];
                uio[i].len = bitmap->chunk_size;
            }
        }
        err = conn_rw_vector(conn, ND_CMD_READ, start_sec, num_sec, nr_uio, uio);
        if ( !NDAS_SUCCESS(err) )
            break;
    }
    sal_free(uio);
    return err;
}

#endif

#if 0
ndas_error_t conn_get_retransmission_time(uconn_t *conn, sal_msec* retx_time)
{
	ndas_error_t err;
	xuint32 param32[2];
	char* param = (char*)&(param32[0]);
	xuint32 retransval;

        if (conn->info.HWVersion <= NDAS_VERSION_1_0) {
            debug_udev(1, "Get retransmission time is not supported for NDAS 1.0");
            return NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION;
        }
	err = NdasVendorCommand(conn->sock, &conn->info, VENDOR_OP_GET_MAX_RET_TIME, param);

	if ( !NDAS_SUCCESS(err) ) {
		debug_udev(1, "Failed to get retransmission time:err=%d",err);
		return err;
	}
	retransval = g_ntohl(param32[1]) + 1;
	if (retransval < 10 || retransval > 10 * 1000) {
		debug_udev(1, "Suspected retransmission value: %d msec", retransval);
	}
	*retx_time = retransval;
	return NDAS_OK;
}

ndas_error_t conn_set_loss_detection_time(uconn_t *conn, sal_msec retx_time)
{
	return lpx_set_packet_loss_detection_time(conn->sock, retx_time);
}
#endif


#if 0
/*
 to do: conn_get_infos need changes in name and usage?
*/
ndas_error_t
conn_get_infos(uconn_t *conn, ndas_slot_info_t* sinfo, ndas_unit_info_t* uinfo, ndas_metadata_t *nmd) 
{
    ndas_error_t err;
    const lsp_ata_handshake_data_t* hsdata;
    xuint32 size =sizeof(NDAS_DIB_V2);
    debug_udev(5, "conn=%p", conn);
    
    /* 
     * Fill ndas_slot info and ndas_unit_info_t 
     */

    hsdata = lsp_get_ata_handshake_data(conn->lsp_handle);

    uinfo->sectors = hsdata->lba_capacity.quad;
    uinfo->capacity = hsdata->lba_capacity.quad * hsdata->logical_block_size;
        
    uinfo->headers_per_disk = hsdata->num_heads;
    uinfo->cylinders_per_header = hsdata->num_cylinders;
    uinfo->sectors_per_cylinder = hsdata->num_sectors_per_track;
        
    sal_memcpy(uinfo->serialno, hsdata->serial_number, sizeof(uinfo->serialno));
    sal_memcpy(uinfo->model, hsdata->model_number, sizeof(uinfo->model));
    sal_memcpy(uinfo->fwrev, hsdata->firmware_revision, sizeof(uinfo->fwrev));

    // Set data as if single disk. 
    // We will update these field later. 
    sinfo->sector_size = 1 << 9;     
    sinfo->capacity = uinfo->sectors * sinfo->sector_size;
    sinfo->sectors = uinfo->sectors;
    sinfo->max_sectors_per_request = 16 * 1024 * 1024 / 512;

    if (hsdata->device_type == 0) {
        /* ATA */
        uinfo->type = NDAS_UNIT_TYPE_HARD_DISK;
        err = conn_read_dib(conn, sinfo->sectors, nmd->dib, &size);
        
        if ( !NDAS_SUCCESS(err) ) {
            sinfo->mode = NDAS_DISK_MODE_UNKNOWN;
            debug_udev(1, "Error reading DIB %d : ignore", err);
            return err;
        }
        debug_udev(5, "mediaType=%u", nmd->dib->u.s.iMediaType);
        if ( nmd->dib->u.s.iMediaType == NMT_RAID0 ) 
        {
            sinfo->mode = NDAS_DISK_MODE_RAID0;
            sinfo->mode_role = nmd->dib->u.s.iSequence;
        } else if ( nmd->dib->u.s.iMediaType == NMT_RAID1 ) {
            sinfo->mode = NDAS_DISK_MODE_RAID1;
            sinfo->mode_role = nmd->dib->u.s.iSequence;
        } else if ( nmd->dib->u.s.iMediaType == NMT_RAID4 ) {
            sinfo->mode = NDAS_DISK_MODE_RAID4;
            sinfo->mode_role = nmd->dib->u.s.iSequence;
        } else if ( nmd->dib->u.s.iMediaType == NMT_AGGREGATE ) {
            sinfo->mode = NDAS_DISK_MODE_AGGREGATION;
            sinfo->mode_role = nmd->dib->u.s.iSequence;
        } else if ( nmd->dib->u.s.iMediaType == NMT_MIRROR ) {
            sinfo->mode = NDAS_DISK_MODE_MIRROR; // TODO convert into raid1
            sinfo->mode_role = nmd->dib->u.s.iSequence;
        } else if ( nmd->dib->u.s.iMediaType == NMT_MEDIAJUKE) {
            sal_debug_println("NDAS_DISK_MODE_MEDIAJUKE");
            sinfo->mode = NDAS_DISK_MODE_MEDIAJUKE; 
        } else {
            sinfo->mode = NDAS_DISK_MODE_SINGLE;
        }
    } else {
        debug_udev(0, "ATAPI detected");
        /* ATAPI */
        sinfo->mode = NDAS_DISK_MODE_ATAPI;
        uinfo->type = NDAS_UNIT_TYPE_ATAPI;
        err = NDAS_OK;
    }

    if ( !nmd ) 
        return NDAS_OK;
    
    if ( !nmd->dib ) 
        return NDAS_OK;


    debug_udev(2, "NDAS_DISK_TYPE %d", nmd->dib->u.s.iMediaType);
    return err;
}
#endif

ndas_error_t udev_connecting_func(udev_t *udev, urb_ptr tir);
LOCAL
ndas_error_t v_udev_connecting_func(udev_t *udev, int flags);

/*
 * Queue the tir to enable the slot.
 * Called by 
 *    v_udev_connecting_func (bpc thread) on retrial
 *    uop_enable(user thread) on user's request
 */ 
LOCAL int v_uop_enable(logunit_t *log_udev, void *arg)
{
    long _arg = (long)arg;
#ifdef XPLAT_ASYNC_IO
    urb_ptr tir = tir_alloc(ND_CMD_CONNECT, NULL);
    debug_udev(2, "slot=%d queueing ND_CMD_CONNECT", log_udev->info.slot);
    if ( !tir ) {
        v_uop_enabled(log_udev->info.slot, NDAS_ERROR_OUT_OF_MEMORY, log_udev);
        return 0;
    }
    tir->func = udev_connecting_func;
    tir->arg = (void*)(_arg);
    tir->req->done = (ndas_io_done) v_uop_enabled;
    tir->req->done_arg = log_udev;
    log_udev->ops->io(log_udev, tir, TRUE); // uop_io
#else
    int flags = (int)_arg;
    ndas_error_t err;
    err = v_udev_connecting_func(SDEV2UDEV(log_udev), flags);
    v_uop_enabled(log_udev->info.slot, err, log_udev);
#endif
    return 0;
}

#ifndef XPLAT_PNP

static inline void NDEV_INIT_STATUS(ndev_t *ndev) { ndev->info.status = NDAS_DEV_STATUS_UNKNOWN; } 

/*
 * NDAS device status manipulation funtions
 * If PNP is not enable, status is on, off, unknown.
 * If PNP is enabled, status is on, off.
 */
static inline int NDEV_STATUS(ndev_t *ndev) { return ndev->info.status; }

static inline void NDEV_SET_ONLINE(ndev_t *ndev) { 
    /*sal_debug_print("online %s\n", ndev->info.ndas_idserial);*/
    ndev->info.status = NDAS_DEV_STATUS_HEALTHY; 
} 
static inline void NDEV_SET_OFFLINE(ndev_t *ndev) { ndev->info.status = NDAS_DEV_STATUS_OFFLINE; } 
static inline int NDEV_IS_ONLINE(ndev_t *ndev) { return ndev->info.status == NDAS_DEV_STATUS_HEALTHY; }
static inline int NDEV_IS_OFFLINE(ndev_t *ndev) { return ndev->info.status == NDAS_DEV_STATUS_OFFLINE; }
static inline int NDEV_IS_UNKNOWN(ndev_t *ndev) { return ndev->info.status == NDAS_DEV_STATUS_UNKNOWN; }

#endif

/* 
 * 
 * called by udev_connecting_func
 */
LOCAL
ndas_error_t v_udev_connecting_func(udev_t *udev, int flags) 
{
    ndas_error_t err;
//    sal_msec retx_time;
    xbool writemode = ENABLE_FLAG_WRITABLE(flags);
    xbool writeshare = ENABLE_FLAG_WRITESHARE(flags);
    xbool bind = ENABLE_FLAG_RAID(flags);
    xbool pnp = ENABLE_FLAG_PNP(flags);
    logunit_t *log_udev = sdev_lookup_byslot(udev->info->slot);

    debug_udev(2, "ing slot=%d", udev->info->slot);
    sal_assert(log_udev);
        
    err = conn_connect(&udev->conn, udev->info->timeout);

#ifdef XPLAT_NDASHIX
    if ( err == NDAS_ERROR_NO_WRITE_ACCESS_RIGHT && writemode ) 
    {
        /* send the request for surrending the exclusive write permission */
        debug_udev(1, "someone else has the write permission already");
        if ( pnp ) {
            dpc_id dpcid;
            int ret;

            debug_udev(1, "try again 1 sec later");
            dpcid = bpc_create((dpc_func) v_uop_enable, log_udev, (void*)(long)(flags), NULL, 0);
            sal_assert(dpcid);
            if(dpcid) {
                ret = bpc_queue(dpcid, SAL_TICKS_PER_SEC);
                sal_assert(ret >= 0);
            }
		
            /* 
			 * special value to v_uop_enabled 
			 * means, it didn't fail but I will keep trying to connect 
			 */
            return NDAS_ERROR_TRY_AGAIN; 
        }
        err = NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
        goto out2;
    }
#else
    if ( err == NDAS_ERROR_NO_WRITE_ACCESS_RIGHT && writemode && pnp )
    {
        dpc_id dpcid;
        int ret;

        debug_udev(1, "try again 1 sec later");
        dpcid = bpc_create((dpc_func) v_uop_enable, log_udev, (void*)(flags), NULL, 0);
        sal_assert(dpcid);
        if(dpcid) {
            ret = bpc_queue(dpcid, SAL_TICKS_PER_SEC);
            sal_assert(ret >= 0);
        }

		/* 
		 * special return value to v_uop_enabled 
		 * means, it didn't fail but I will keep trying to connect 
		 */
        return NDAS_ERROR_TRY_AGAIN;
    }
#endif    

    if ( !NDAS_SUCCESS(err) ) {
        debug_udev(1, "fail to connnect");
        goto out2;
    }
    debug_udev(2, "blocking call of conn_get_infos");
    err  = conn_handshake(&udev->conn);
    if ( !NDAS_SUCCESS(err) ) {
        debug_udev(1, "fail to handshake");
        goto out1;
    }

    //
    // To do:  check device information has changed since last discover/connect
    //

#if 0   // Assume all device has same retransmission time(200ms)
    //
    // Get retramission time and set packet loss detection time
    //
    err = conn_get_retransmission_time(&udev->conn, &retx_time);
    if (!NDAS_SUCCESS(err)) {
        debug_udev(1, "fail to get retransmission time. Keep default value");
    } else {
    	debug_udev(1, "Setting packet loss detection time to %d msec", retx_time * 7/8);
    	// Set retransmission time 75% of the target device's retransmission time.
    	conn_set_loss_detection_time(&udev->conn, retx_time * 7/8);
    }
#endif

    if ( !bind && slot_is_the_raid(udev->info)) 
    {
	
        debug_udev(1, "Is a member of raid");
        err = NDAS_ERROR_UNSUPPORTED_DISK_MODE;
        goto out1;
    }
    
    udev->ndev->info.version = udev->conn.hwdata->hardware_version;
    
    if (writeshare) {
        if (udev->ndev->info.version == NDAS_VERSION_1_0) {
            debug_udev(1, "Hardware does not support shared-write");
            err = NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION;
            goto out1;
        }
        udev->info->writeshared = TRUE;
    } else {
        udev->info->writeshared = FALSE;
    }    
    udev->info->writable = (writemode || udev->info->writeshared);

    LMInitialize(&udev->conn, &udev->BuffLockCtl, udev->info->writeshared);
    
#ifndef XPLAT_PNP
    NDEV_SET_ONLINE(udev->ndev);
#endif


    err = xbuf_pool_inc(TRUE, udev->info->writable);
    if ( !NDAS_SUCCESS(err) ) {
        debug_udev(1, "fail to increase xbuf pool");
        goto out1;
    }   
    debug_udev(2, "ed");

    err = NDAS_OK;
    goto out2;
out1:
    conn_logout(&udev->conn);
    conn_disconnect(&udev->conn);
    conn_clean(&udev->conn);    
out2:
    return err;
}
/* 
 * To enable the slot
 * called by ndiod_thread (ndiod thread) 
 */
ndas_error_t udev_connecting_func(udev_t *udev, urb_ptr tir) 
{
#ifdef XPLAT_ASYNC_IO    
    int flags = (int)((long)tir->arg);
#else
    int flags = (int)tir;
#endif
    return v_udev_connecting_func(udev, flags);
}

/**
 * 
 * called by 
 *    1. v_uop_enable (user thread of 'ndas_enable_slot' or 'ndio-#' thread)
 */
LOCAL
NDAS_CALL void v_uop_enabled(int slot, ndas_error_t err, logunit_t* log_udev)
{
    debug_udev(2,"ing slot=%d err=%d", slot, err);

    if ( err == NDAS_ERROR_TRY_AGAIN) {
        debug_udev(2,"will try again 1 sec later");
        return;
    }
    if ( !NDAS_SUCCESS(err) ) {
        log_udev->notify_enable_result_on_disabled = TRUE;
    } else {
        sdev_notify_enable_result(log_udev, err);
    }
}

/* 
 * Enable the slot (unit device).
 * called by jbod_op_enable enable_internal rddc_op_enable
 */
LOCAL
void  uop_enable(logunit_t *log_udev, int flags)
{
    ndas_error_t err;
    udev_t *udev = SDEV2UDEV(log_udev);
    ndas_conn_mode_t mode;
    xbool writemode = ENABLE_FLAG_WRITABLE(flags);
    xbool writeshare = ENABLE_FLAG_WRITESHARE(flags);
    
    debug_udev(1, "slot=%d conn=%p, unit=%d", log_udev->info.slot, &udev->conn, udev->unit);
    debug_udev(2, "err=%d status=%x", udev->conn.err, udev->conn.status);
    
    udev->arg = log_udev;

    if (writeshare) {
        mode = NDAS_CONN_MODE_WRITE_SHARE;
    } else if (writemode) {
        mode = NDAS_CONN_MODE_EXCLUSIVE_WRITE;
    } else {
        mode = NDAS_CONN_MODE_READ_ONLY;
    }

    conn_init(&udev->conn, udev->ndev->network_id, udev->unit, mode, &udev->BuffLockCtl, &udev->lock_info);
    
#ifdef XPLAT_ASYNC_IO
    sal_assert(udev->conn.err == NDAS_OK);
    
    udev->thread.tid = 0;
    
    /* Create a thread for the slot (unit device) */
    err = ndiod_create(udev);
    if ( !NDAS_SUCCESS(err) ) {
        debug_udev(1, "fail to create thread");
        sdev_notify_enable_result(log_udev, err);
        return;
    }

    v_uop_enable(log_udev, (void *)(long)flags);
    return;
#else
    err = udev_connecting_func(udev, (void *)(long)flags);
    if ( !NDAS_SUCCESS(err) ) 
    {
        debug_udev(1,"Connecting error %d", err);
    }
    sdev_notify_enable_result(log_udev, err);
    return;
#endif
}

/*
 * Called by 
 */
LOCAL
void uop_disable(logunit_t *log_udev, ndas_error_t err)
{
    udev_t *udev = SDEV2UDEV(log_udev);
    sal_assert(log_udev->info.enabled);
    if ( !log_udev->info.enabled )
        return;
    conn_set_error(&udev->conn, err);

#ifdef XPLAT_ASYNC_IO
    /* In async i/o, request the thread to be shut down and wait */
    udev_request_shutdown(udev);
#else
    /* In block i/o, just shut the connection down */
    udev_shutdown(udev);
#endif    
}

/********************************************
    Netdisk management interface. 
        This interface and implementation is 
            only for temporary purpose.
        Need to be moved to ndasuser library and 
            be modified according to newer NDAS API.
        
*********************************************/



/*
 * Write the dib 
 * CRC will be calculated in it
 */
ndas_error_t udev_write_dib(udev_t *udev,NDAS_DIB_V2 *pDIB_V2)
{
    xuint64 sector;
    ndas_error_t err;
    ndas_io_request ioreq = NDAS_IO_REQUEST_INITIALIZER;
    
    ioreq.buf = (char*)pDIB_V2;
    sector = udev->info->sectors + NDAS_BLOCK_SIZE_XAREA + NDAS_BLOCK_LOCATION_DIB_V2;
    ioreq.start_sec = sector;
    ioreq.num_sec = 1;

    pDIB_V2->crc32 = crc32_calc((unsigned char *)pDIB_V2,
        sizeof(pDIB_V2->u.bytes_248));
    pDIB_V2->crc32_unitdisks = crc32_calc((unsigned char *)pDIB_V2->UnitDisks,
        sizeof(pDIB_V2->UnitDisks));
    
    err = conn_rw(&udev->conn, ND_CMD_WRITE, sector, 1, (char*) pDIB_V2);
    
    if (!NDAS_SUCCESS(err)) {
        debug_udev(1, "conn_read_dib failed: %d", err);
        return err;
    }
    
    return NDAS_OK;
}

#if 0
/*
  * To do: check usage of udev. Maybe remove this function??
  */
ndas_error_t 
udev_query_unit(udev_t *udev, ndas_metadata_t* nmd) 
{
    if (udev->uinfo->type == NDAS_UNIT_TYPE_HARD_DISK) {
        xuint32 dibsize = SECTOR_SIZE;
        return conn_read_dib(&udev->conn, udev->uinfo->sectors, nmd->dib, &dibsize);
    } else {
        sal_memset(nmd->dib, 0, SECTOR_SIZE);
        return NDAS_OK;
    }
}
#endif

/* Shut down the unit device explicitly
 * Called by 
 *  ndas_disable_slot(block i/o) : user thread
 *  ndas_io (block i/o) : user thread
 *  ndiod_thread (async i/o) 
 **/

void udev_shutdown(udev_t *udev)
{
    ndas_error_t err;

    sal_assert(udev);
    sal_assert(udev->info);
    
    err = udev->conn.err;
    
    debug_ndiod(2, "ing slot=%d err=%d", udev->info->slot, err);
    conn_set_status(&udev->conn,CONN_STATUS_SHUTING_DOWN);
	
#ifdef XPLAT_ASYNC_IO    
    udev_invalidate_requests(udev);
#endif

    if ( udev->conn.sock > 0 ) {
        conn_logout(&udev->conn);
        conn_disconnect(&udev->conn);
    }
	// TODO: fix another conn memory leak.
    conn_clean(&udev->conn);

    conn_set_status(&udev->conn, CONN_STATUS_INIT);
    LMDestroy(&udev->BuffLockCtl);
    udev->conn.err = NDAS_OK;
    
#ifdef XPLAT_ASYNC_IO        
    ndiod_cleanup(&udev->thread);
#endif 
    {
        logunit_t *log_udev = (logunit_t *) udev->arg;
        sal_assert(log_udev);
        sdev_dont_accept_io(log_udev);
        debug_ndiod(2, "log_udev->info.mode=%d log_udev->info.enabled=%d", log_udev->info.mode, log_udev->info.enabled);
        if ( log_udev->notify_enable_result_on_disabled && !NDAS_SUCCESS(err)) {
            sdev_notify_enable_result(log_udev, err);
        } else {
            sdev_notify_disable_result(log_udev, err);
        }
    }

    sal_assert(udev->info);
    xbuf_pool_dec(TRUE, udev->info->writable?TRUE:FALSE);
}




LOCAL NDAS_CALL
void ndas_pc_io_set_sensedata(
	unsigned char *sensebuf,
	unsigned char sensekey,
	unsigned char additionalSenseCode,
	unsigned char additionalSenseCodeQaulifier
	)
{
	
	PSENSE_DATA SenseBuffer = (PSENSE_DATA)sensebuf;
	sal_memset(sensebuf, 32, 0);
	SenseBuffer->ErrorCode = 0x70;
	SenseBuffer->SenseKey = sensekey;
	SenseBuffer->AdditionalSenseLength = 0x0a;
	SenseBuffer->AdditionalSenseCode = additionalSenseCode;
	SenseBuffer->AdditionalSenseCodeQualifier = additionalSenseCodeQaulifier;
}

LOCAL NDAS_CALL
xuint32 ndas_pc_io_write_to_sal_bio( void * buf, 
											ndas_io_request * req, 
											xuint32 offset, 
											xuint32 size, 
											xbool zero)
{
	xuint32 remain = 0, copyed = 0, copy = 0;
	xuint16 i = 0;
	xuint8 * data = NULL;
	
	remain = size;
	
	if(req->nr_uio) {

		if(zero) {
			for(i = 0; i< req->nr_uio; i++) 
				sal_memset(req->uio[i].ptr, 0, req->uio[i].len);
		}

		for(i = 0; (i< req->nr_uio) && remain; i++) {
			if(offset) {
				if(req->uio[i].len <= offset) {
					offset -= req->uio[i].len;
				}else {
					data = (xuint8 *)(req->uio[i].ptr ) + offset;
					copy = (req->uio[i].len - offset);
					if(copy > remain) {
						copy = remain;
					}
					sal_memcpy(data, buf + copyed,  copy);
					copyed += copy;
					remain  -= copy;
					offset = 0;
				}
			}else {
				data = (xuint8 *)req->uio[i].ptr;
				copy = req->uio[i].len;
				if (copy > remain) {
					copy = remain;
				}
				sal_memcpy(data, buf+copyed, copy);
				copyed += copy;
				remain -= copy;
			}
		}
	}else {

		if(zero) 
			sal_memset(req->buf, 0, req->reqlen);
		data = (xuint8 *)req->buf;
		copy = remain;
		if(copy > req->reqlen) {
			copy = req->reqlen;
		}
		
		sal_memcpy(data, buf, copy);
		copyed = copy;
	}
	return copyed;
}


LOCAL NDAS_CALL
xuint32 ndas_pc_io_read_from_sal_bio(void * buf, ndas_io_request * req, xuint32 offset, xuint32 size)
{
	xuint32 remain = 0, copyed = 0, copy = 0;
	xuint16 i = 0;
	xuint8 * data = NULL;

	remain = size;
	
	if(req->nr_uio) {
		for(i = 0; (i< req->nr_uio) && remain; i++) {
			if(offset) {
				if(req->uio[i].len <= offset) {
					offset -= req->uio[i].len;
				}else {
					data = (xuint8 *)(req->uio[i].ptr ) + offset;
					copy = (req->uio[i].len - offset);
					if(copy > remain) {
						copy = remain;
					}
					sal_memcpy(buf+copyed, data,  copy);
					copyed += copy;
					remain  -= copy;
					offset = 0;
				}
			}else {
				data = (xuint8 *)req->uio[i].ptr;
				copy = req->uio[i].len;
				if (copy > remain) {
					copy = remain;
				}
				sal_memcpy(buf+copyed, data, copy);
				copyed += copy;
				remain -= copy;
			}
		}
	}else {
		data = (xuint8 *)req->buf;
		copy = remain;
		if(copy > req->reqlen) {
			copy = req->reqlen;
		}
		sal_memcpy(buf, data, copy);
		copyed = copy;
	}
	return copyed;
}

LOCAL NDAS_CALL
ndas_error_t ndas_io_real_atapi_processing(logunit_t* log_udev, xint8 cmd, ndas_io_request * req);

#define MAX_AUX_MAX_PIO_BUFF_4096			(4096)
LOCAL NDAS_CALL
ndas_error_t ndas_io_atapi_process(logunit_t * l_udev, ndas_io_request * req){

	nads_io_pc_request_ptr pc_req = NULL;
	ndas_slot_info_t  * s_info = NULL;

	s_info = &(l_udev->info);
	pc_req = (nads_io_pc_request_ptr)req->additionalInfo;
	pc_req->AuxBufLen = req->reqlen;


	debug_udev(1, "CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x] reqlen[0x%x] dir[0x%02x]",
			pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
			pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
			pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11],
			req->reqlen,
			pc_req->direction);
	
	
	switch(pc_req->cmd[0])
	{
		case SCSIOP_TEST_UNIT_READY:
		case SCSIOP_REZERO_UNIT:	
		case SCSIOP_START_STOP_UNIT:
		case SCSIOP_MEDIUM_REMOVAL:
		case SCSIOP_SEEK:
		case SCSIOP_STOP_PLAY_SCAN:
		case SCSIOP_RESERVE_TRACK_RZONE:
		case SCSIOP_SET_CD_SPEED:
		{
			pc_req->AuxBufLen = 0;
		}
		break;
		case SCSIOP_REPAIR_TRACK:
		{	
			PCDB cdb = NULL;
			
			if(!s_info->writable &&  !s_info->writeshared) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
			cdb = (PCDB)pc_req->cmd;
			if(cdb->REPAIR_TRACK.Immediate == 0) {
				cdb->REPAIR_TRACK.Immediate = 1;
			}

			pc_req->AuxBufLen = 0;				
		}
		break;
		case SCSIOP_SYNCHRONIZE_CACHE16:
		{
	
			PCDB cdb = NULL;
			
			if(!s_info->writable &&  !s_info->writeshared) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
			cdb = (PCDB)pc_req->cmd;
			if(cdb->SYNCHRONIZE_CACHE16.Immediate == 0) {
				cdb->SYNCHRONIZE_CACHE16.Immediate = 1;
			}

			pc_req->AuxBufLen = 0;			

		}
		break;
		case SCSIOP_SYNCHRONIZE_CACHE:
		{

			PCDB cdb = NULL;
			
			if(!s_info->writable &&  !s_info->writeshared) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
			cdb = (PCDB)pc_req->cmd;
			if(cdb->SYNCHRONIZE_CACHE10.Immediate == 0) {
				cdb->SYNCHRONIZE_CACHE10.Immediate = 1;
			}

			pc_req->AuxBufLen = 0;			
		}
		break;
		case SCSIOP_CLOSE_TRACK_SESSION:
		{
			PCDB cdb = NULL;
			
			if(!s_info->writable &&  !s_info->writeshared) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
			cdb = (PCDB)pc_req->cmd;
			if(cdb->CLOSE_TRACK.Immediate == 0) {
				cdb->CLOSE_TRACK.Immediate = 1;
			}

			pc_req->AuxBufLen = 0;
		}
		break;
		case SCSIOP_ERASE:
		{
			PCDB cdb = NULL;
			
			if(!s_info->writable &&  !s_info->writeshared) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
			cdb = (PCDB)pc_req->cmd;
			if(cdb->ERASE.Immediate == 0) {
				cdb->ERASE.Immediate = 1;
			}

			pc_req->AuxBufLen = 0;			
		}
		break;
		case SCSIOP_BLANK:
		{

			PCDB cdb = NULL;
			
			if(!s_info->writable &&  !s_info->writeshared) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
			cdb = (PCDB)pc_req->cmd;
			if(cdb->BLANK_MEDIA.Immediate == 0) {
				cdb->BLANK_MEDIA.Immediate = 1;
			}

			pc_req->AuxBufLen = 0;
		}
		break;		
		case SCSIOP_REPORT_LUNS:
		{
			PLUN_LIST	lunlist = NULL;
			PCDB cdb = NULL;
			xuint32		luncount = 1;
			xuint32		lunsize = 0;
			xuint32 		transfer_size = 0;
			xuint32		size = sizeof(LUN_LIST) + 8;
			xuint8		buf[size];

			lunlist = (PLUN_LIST)buf;			
			sal_memset(buf, 0, size);
			
			cdb = (PCDB)pc_req->cmd;

			if (sizeof(LUN_LIST) > req->reqlen) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;			
			}

			lunsize = (xuint32)( ((xuint32)cdb->REPORT_LUNS.AllocationLength[0] << 24)
					+ ((xuint32)cdb->REPORT_LUNS.AllocationLength[1] << 16)
					+ ((xuint32)cdb->REPORT_LUNS.AllocationLength[2] << 8)
					+ (xuint32)cdb->REPORT_LUNS.AllocationLength[3]  );

			luncount = luncount * 8;

			lunlist->LunListLength[0] = ((luncount >> 24)  & 0xff);
			lunlist->LunListLength[1] = ((luncount >> 16)  & 0xff);
			lunlist->LunListLength[2] = ((luncount >> 8) & 0xff);
			lunlist->LunListLength[3] = ((luncount >> 0) & 0xff); 


			transfer_size = ndas_pc_io_write_to_sal_bio((void *)buf, req, 0, size, 1);
			if(transfer_size > req->reqlen) transfer_size = req->reqlen;
			pc_req->resid = req->reqlen - transfer_size;
			pc_req->status = NDAS_IO_STATUS_SUCCESS;
			return NDAS_OK;			
		}
		break;		
		case SCSIOP_REQUEST_SENSE:
		{
			xuint8 transferlen = pc_req->cmd[4];
			if(transferlen >  req->reqlen) {
				pc_req->AuxBufLen = transferlen;
				pc_req->useAuxBuf = 1;
			}
		}
		break;
		case SCSIOP_READ6:
		{
			PCDB cdb, tcdb;
			xuint8 	AuxCmd[16];
			xuint32	logicalBlock = 0;
			xuint32	transferBlockLength = 0;

			
			cdb = tcdb = NULL;
			sal_memset(AuxCmd, 0, 16);

			cdb = (PCDB)pc_req->cmd;
			tcdb = (PCDB)AuxCmd;

			tcdb->READ12.OperationCode = SCSIOP_READ12;
			tcdb->READ12.LogicalUnitNumber = cdb->CDB6READWRITE.LogicalUnitNumber;

			logicalBlock = (xuint32)(((xuint32)cdb->CDB6READWRITE.LogicalBlockMsb1 << 16)
						+ ((xuint32)cdb->CDB6READWRITE.LogicalBlockMsb0 << 8)
						+ (xuint32)cdb->CDB6READWRITE.LogicalBlockLsb );

			tcdb->READ12.LogicalBlock[0] = (logicalBlock >> 24) & 0xFF;
			tcdb->READ12.LogicalBlock[1] = (logicalBlock >> 16) & 0xFF;
			tcdb->READ12.LogicalBlock[2] = (logicalBlock >> 8) & 0xFF;
			tcdb->READ12.LogicalBlock[3] = (logicalBlock) & 0xFF;

			transferBlockLength= cdb->CDB6READWRITE.TransferBlocks;
			
			tcdb->READ12.TransferLength[0] = (transferBlockLength >> 24) & 0xFF;
			tcdb->READ12.TransferLength[1] = (transferBlockLength >> 16) & 0xFF;
			tcdb->READ12.TransferLength[2] = (transferBlockLength >> 8) & 0xFF;
			tcdb->READ12.TransferLength[3] = (transferBlockLength) & 0xFF;

			tcdb->READ12.Control = cdb->CDB6READWRITE.Control;
			pc_req->cmdlen = 12;
			sal_memcpy(pc_req->cmd, AuxCmd, pc_req->cmdlen);

			debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);

			
		}
		break;
		case SCSIOP_MODE_SENSE:
		{
			xuint16 AllocationLength = 0;
			PCDB cdb, tcdb;
			xuint8 	AuxCmd[16];

			cdb = tcdb = NULL;
			sal_memset(AuxCmd, 0, 16);
			
			cdb = (PCDB)pc_req->cmd;
			tcdb = (PCDB)AuxCmd;
			
			if(pc_req->AuxBufLen < 4) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;			
			}

			tcdb->MODE_SENSE10.OperationCode = SCSIOP_MODE_SENSE10;
			tcdb->MODE_SENSE10.Dbd = cdb->MODE_SENSE.Dbd;
			tcdb->MODE_SENSE10.LogicalUnitNumber = cdb->MODE_SENSE.LogicalUnitNumber;
			tcdb->MODE_SENSE10.PageCode = cdb->MODE_SENSE.PageCode;
			tcdb->MODE_SENSE10.Pc = cdb->MODE_SENSE.Pc;
				
			AllocationLength = cdb->MODE_SENSE.AllocationLength;
			AllocationLength += 4;
			
			tcdb->MODE_SENSE10.AllocationLength[0] = (xuint8)((AllocationLength >> 8) & 0xFF);
			tcdb->MODE_SENSE10.AllocationLength[1] = (xuint8)(AllocationLength & 0xFF);
			tcdb->MODE_SENSE10.Control = cdb->MODE_SENSE.Control;			

			pc_req->AuxBufLen = AllocationLength;
			pc_req->useAuxBuf = 1;

			pc_req->cmdlen = 10;
			sal_memcpy(pc_req->cmd, AuxCmd, pc_req->cmdlen);	

			debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
			
		}
		break;	
		case SCSIOP_MODE_SENSE10:
		{
			PCDB cdb = NULL;
			xuint16 parameterListLength = 0;

			cdb = (PCDB)pc_req->cmd;	

			parameterListLength = (xuint16)(((xuint16)cdb->MODE_SENSE10.AllocationLength[0] << 8)
								+ (xuint16)cdb->MODE_SENSE10.AllocationLength[1]);

			if(req->reqlen < 8) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;			
			}


			if(parameterListLength > req->reqlen) {
				if(parameterListLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					parameterListLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->MODE_SENSE10.AllocationLength[0] = (parameterListLength >> 8) & 0xFF;
					cdb->MODE_SENSE10.AllocationLength[1] = parameterListLength & 0xFF;
				}

				pc_req->AuxBufLen = parameterListLength;
				pc_req->useAuxBuf = 1;
				
				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}			
						
		}
		break;		
		case SCSIOP_SEEK6:
		{
			PCDB cdb, tcdb;
			xuint8 	AuxCmd[16];
			xuint32	logicalBlock = 0;
			

			
			cdb = tcdb = NULL;
			sal_memset(AuxCmd, 0, 16);

			cdb = (PCDB)pc_req->cmd;
			tcdb = (PCDB)AuxCmd;

			tcdb->SEEK.OperationCode = SCSIOP_SEEK;
			tcdb->SEEK.LogicalUnitNumber = cdb->CDB6GENERIC.LogicalUnitNumber;

			logicalBlock = (xuint32)(((xuint32)cdb->CDB6READWRITE.LogicalBlockMsb1 << 16)
						+ ((xuint32)cdb->CDB6READWRITE.LogicalBlockMsb0 << 8)
						+ (xuint32)cdb->CDB6READWRITE.LogicalBlockLsb );

			tcdb->SEEK.LogicalBlockAddress[0] = (logicalBlock >> 24) & 0xFF;
			tcdb->SEEK.LogicalBlockAddress[1] = (logicalBlock >> 16) & 0xFF;
			tcdb->SEEK.LogicalBlockAddress[2] = (logicalBlock >> 8) & 0xFF;
			tcdb->SEEK.LogicalBlockAddress[3] = (logicalBlock) & 0xFF;
			
			tcdb->SEEK.Control = cdb->CDB6READWRITE.Control;

			pc_req->AuxBufLen = 0;	
			pc_req->cmdlen = 10;
			sal_memcpy(pc_req->cmd, AuxCmd, pc_req->cmdlen);	
			
			debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
			
		}
		break;	
		case SCSIOP_INQUIRY:
		{
			PCDB cdb = NULL;
			xuint32 allocationLength = 0;

			cdb = (PCDB)pc_req->cmd;
			allocationLength = cdb->CDB6INQUIRY3.AllocationLength;

			if(allocationLength > pc_req->AuxBufLen) {
				pc_req->useAuxBuf = 1;
				pc_req->AuxBufLen = allocationLength;
			}
		}
		break;	
		case SCSIOP_READ_FORMATTED_CAPACITY:
		{
			xuint16 transferLen = 0;
			
			transferLen = ( (xuint16)(pc_req->cmd[7] << 8)
						+ (xuint16) pc_req->cmd[8] );

			if(transferLen > req->reqlen) {
				if(transferLen > MAX_AUX_MAX_PIO_BUFF_4096) {
					transferLen = MAX_AUX_MAX_PIO_BUFF_4096;
					pc_req->cmd[7] = (xuint8)((transferLen  >> 8) & 0xFF);
					pc_req->cmd[7] = (xuint8)(transferLen & 0xFF);
				}

				pc_req->AuxBufLen = transferLen;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
			}
		}
		break;
		case SCSIOP_READ_BUFFER_CAPACITY:
		{
			PCDB cdb = NULL;
			xuint16 allocationLength = 0;

			cdb = (PCDB)pc_req->cmd;	

			allocationLength = (xuint16)(((xuint16)cdb->READ_BUFFER_CAPACITY.AllocationLength[0] << 8)
							+ (xuint16)cdb->READ_BUFFER_CAPACITY.AllocationLength[1]);

			if(allocationLength > req->reqlen) {
				if(allocationLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					allocationLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->READ_BUFFER_CAPACITY.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
					cdb->READ_BUFFER_CAPACITY.AllocationLength[1] = allocationLength & 0xFF;
				}

				pc_req->AuxBufLen = allocationLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}						
		}
		break;	
		case SCSIOP_READ_DATA_BUFF:
		{
			PCDB cdb = NULL;
			xuint32 allocationLength = 0;
			xuint32 bufferoffset = 0;
			
			cdb = (PCDB)pc_req->cmd;	

			bufferoffset = (xuint32)(((xuint32)cdb->READ_BUFFER.BufferOffset[0] << 16)
							+ ((xuint32)cdb->READ_BUFFER.BufferOffset[1] << 8)
							+ (xuint32)cdb->READ_BUFFER.BufferOffset[2]);


			allocationLength = (xuint32)(((xuint32)cdb->READ_BUFFER.AllocationLength[0] << 16)
							+ ((xuint32)cdb->READ_BUFFER.AllocationLength[1] << 8)
							+ (xuint32)cdb->READ_BUFFER.AllocationLength[2]);


			debug_udev(1, "READ_DATA_BUFF BO[0x%x] AL[0x%x] MODE[0x%02x] BID[0x%02x] reqlen[0x%x]",
				bufferoffset, allocationLength, cdb->READ_BUFFER.Mode, cdb->READ_BUFFER.BufferId, req->reqlen);

			if(allocationLength > req->reqlen) {
				allocationLength = req->reqlen;
					
				cdb->READ_BUFFER.AllocationLength[0] = (allocationLength >> 16) & 0xFF;
				cdb->READ_BUFFER.AllocationLength[1] = (allocationLength >> 8) & 0xFF;
				cdb->READ_BUFFER.AllocationLength[2] = allocationLength & 0xFF;

				pc_req->AuxBufLen = allocationLength;
				
				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}else if(allocationLength < req->reqlen) {
				pc_req->AuxBufLen = allocationLength;
			}
		}
		break;
		case SCSIOP_READ_CAPACITY:
		{
			if(req->reqlen < 8) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;			
			}

			pc_req->AuxBufLen = 8;
		}
		break;
		case SCSIOP_REPORT_KEY:
		{
			PCDB cdb = NULL;
			xuint16 allocationLength = 0;

			cdb = (PCDB)pc_req->cmd;	

			allocationLength = (xuint16)(((xuint16)cdb->REPORT_KEY.AllocationLength[0] << 8)
							+ (xuint16)cdb->REPORT_KEY.AllocationLength[1]);

			if(allocationLength > req->reqlen) {
				if(allocationLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					allocationLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->REPORT_KEY.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
					cdb->REPORT_KEY.AllocationLength[1] = allocationLength & 0xFF;
				}

				pc_req->AuxBufLen = allocationLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}														
		}
		break;		
		case SCSIOP_READ_SUB_CHANNEL:
		{
			PCDB cdb = NULL;
			xuint16 allocationLength = 0;

			cdb = (PCDB)pc_req->cmd;
			
			allocationLength = (xuint16)(((xuint16)cdb->SUBCHANNEL.AllocationLength[0] << 8)
							+ (xuint16)cdb->SUBCHANNEL.AllocationLength[1]);
			
			if(allocationLength > req->reqlen) {
				if(allocationLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					allocationLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->SUBCHANNEL.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
					cdb->SUBCHANNEL.AllocationLength[1] = allocationLength & 0xFF;
				}

				pc_req->AuxBufLen = allocationLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}

		}
		break;
		case SCSIOP_READ_TOC:
		{
			PCDB cdb = NULL;
			xuint16 allocationLength = 0;

			cdb = (PCDB)pc_req->cmd;

			allocationLength = (xuint16)(((xuint16)cdb->READ_TOC.AllocationLength[0] << 8)
							+ (xuint16)cdb->READ_TOC.AllocationLength[1]);

			if(allocationLength > req->reqlen) {
				if(allocationLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					allocationLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->READ_TOC.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
					cdb->READ_TOC.AllocationLength[1] = allocationLength & 0xFF;
				}

				pc_req->AuxBufLen = allocationLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}
			
		}
		break;
		case SCSIOP_GET_PERFORMANCE:
		{
			PCDB cdb = NULL;
			xuint16 maximumNumberOfDescriptor = 0;

			cdb = (PCDB)pc_req->cmd;	

			maximumNumberOfDescriptor = (xuint16)(((xuint16)cdb->GET_PERFORMANCE.MaximumNumberOfDescriptors[0] << 8)
							+ (xuint16)cdb->GET_PERFORMANCE.MaximumNumberOfDescriptors[1]);

			if(maximumNumberOfDescriptor > req->reqlen) {
				if(maximumNumberOfDescriptor > MAX_AUX_MAX_PIO_BUFF_4096) {
					maximumNumberOfDescriptor = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->GET_PERFORMANCE.MaximumNumberOfDescriptors[0] = (maximumNumberOfDescriptor >> 8) & 0xFF;
					cdb->GET_PERFORMANCE.MaximumNumberOfDescriptors[1] = maximumNumberOfDescriptor & 0xFF;
				}

				pc_req->AuxBufLen = maximumNumberOfDescriptor;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}																	
		}
		break;		
		case SCSIOP_GET_CONFIGURATION:
		{
			PCDB cdb = NULL;
			xuint16 allocationLength = 0;

			cdb = (PCDB)pc_req->cmd;
			
			allocationLength = (xuint16)(((xuint16)cdb->GET_CONFIGURATION.AllocationLength[0] << 8)
							+ (xuint16)cdb->GET_CONFIGURATION.AllocationLength[1]);

			if(allocationLength > req->reqlen) {
				if(allocationLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					allocationLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->GET_CONFIGURATION.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
					cdb->GET_CONFIGURATION.AllocationLength[1] = allocationLength & 0xFF;
				}

				pc_req->AuxBufLen = allocationLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}
			
		}
		break;
		case SCSIOP_GET_EVENT_STATUS:
		{
			PCDB cdb = NULL;
			xuint16 eventListLength;	

			cdb = (PCDB)pc_req->cmd;

			eventListLength = (xuint16)(((xuint16)cdb->GET_EVENT_STATUS_NOTIFICATION.EventListLength[0] << 8)
							+ (xuint16)cdb->GET_EVENT_STATUS_NOTIFICATION.EventListLength[1]);

			if(eventListLength > req->reqlen) {
				if(eventListLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					eventListLength = MAX_AUX_MAX_PIO_BUFF_4096;

					cdb->GET_EVENT_STATUS_NOTIFICATION.EventListLength[0] = (eventListLength >> 8) & 0xFF;
					cdb->GET_EVENT_STATUS_NOTIFICATION.EventListLength[1] = eventListLength & 0xFF;
					
				}

				pc_req->AuxBufLen = eventListLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}
			
		}
		break;
		case SCSIOP_READ_DISC_INFORMATION:
		{
			PCDB cdb = NULL;
			xuint16 allocationLength = 0;

			cdb = (PCDB)pc_req->cmd;	

			allocationLength = (xuint16)(((xuint16)cdb->READ_DISK_INFORMATION.AllocationLength[0] << 8)
							+ (xuint16)cdb->READ_DISK_INFORMATION.AllocationLength[1]);


			if(allocationLength > req->reqlen) {
				if(allocationLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					allocationLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->READ_DISK_INFORMATION.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
					cdb->READ_DISK_INFORMATION.AllocationLength[1] = allocationLength & 0xFF;
				}

				pc_req->AuxBufLen = allocationLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}
			
		
		}
		break;
		case SCSIOP_READ_TRACK_INFORMATION:
		{
			PCDB cdb = NULL;
			xuint16 allocationLength = 0;

			cdb = (PCDB)pc_req->cmd;	

			allocationLength = (xuint16)(((xuint16)cdb->READ_TRACK_INFORMATION.AllocationLength[0] << 8)
							+ (xuint16)cdb->READ_TRACK_INFORMATION.AllocationLength[1]);

			if(allocationLength > req->reqlen) {
				if(allocationLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					allocationLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->READ_TRACK_INFORMATION.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
					cdb->READ_TRACK_INFORMATION.AllocationLength[1] = allocationLength & 0xFF;
				}

				pc_req->AuxBufLen = allocationLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}
						
		}
		break;
		case SCSIOP_WRITE6:
		{
			PCDB cdb, tcdb;
			xuint8 	AuxCmd[16];
			xuint32	logicalBlock = 0;
			xuint32	transferBlockLength = 0;

			
			cdb = tcdb = NULL;
			sal_memset(AuxCmd, 0, 16);

			cdb = (PCDB)pc_req->cmd;
			tcdb = (PCDB)AuxCmd;

			tcdb->WRITE12.OperationCode = SCSIOP_WRITE12;
			tcdb->WRITE12.LogicalUnitNumber = cdb->CDB6READWRITE.LogicalUnitNumber;

			logicalBlock = (xuint32)(((xuint32)cdb->CDB6READWRITE.LogicalBlockMsb1 << 16)
						+ ((xuint32)cdb->CDB6READWRITE.LogicalBlockMsb0 << 8)
						+ (xuint32)cdb->CDB6READWRITE.LogicalBlockLsb );

			tcdb->WRITE12.LogicalBlock[0] = (logicalBlock >> 24) & 0xFF;
			tcdb->WRITE12.LogicalBlock[1] = (logicalBlock >> 16) & 0xFF;
			tcdb->WRITE12.LogicalBlock[2] = (logicalBlock >> 8) & 0xFF;
			tcdb->WRITE12.LogicalBlock[3] = (logicalBlock) & 0xFF;

			transferBlockLength= cdb->CDB6READWRITE.TransferBlocks;
			
			tcdb->WRITE12.TransferLength[0] = (transferBlockLength >> 24) & 0xFF;
			tcdb->WRITE12.TransferLength[1] = (transferBlockLength >> 16) & 0xFF;
			tcdb->WRITE12.TransferLength[2] = (transferBlockLength >> 8) & 0xFF;
			tcdb->WRITE12.TransferLength[3] = (transferBlockLength) & 0xFF;

			tcdb->WRITE12.Control = cdb->CDB6READWRITE.Control;
			pc_req->cmdlen = 12;
			sal_memcpy(pc_req->cmd, AuxCmd, pc_req->cmdlen);

			debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
			
		}
		break;
		case SCSIOP_MODE_SELECT:
		{
			xuint16 parameterListLength = 0;
			PCDB cdb, tcdb;
			xuint8 	AuxCmd[16];

			cdb = tcdb = NULL;
			sal_memset(AuxCmd, 0, 16);
			
			cdb = (PCDB)pc_req->cmd;
			tcdb = (PCDB)AuxCmd;
			
			if(pc_req->AuxBufLen < 4) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;			
			}

			if(!s_info->writable &&  !s_info->writeshared) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}

			tcdb->MODE_SELECT10.OperationCode = SCSIOP_MODE_SELECT10;
			tcdb->MODE_SELECT10.SPBit = cdb->MODE_SELECT.SPBit;
			tcdb->MODE_SELECT10.PFBit = cdb->MODE_SELECT.PFBit;
			tcdb->MODE_SELECT10.LogicalUnitNumber = cdb->MODE_SELECT.LogicalUnitNumber;
				
			parameterListLength = cdb->MODE_SELECT.ParameterListLength;
			parameterListLength += 4;
			
			tcdb->MODE_SELECT10.ParameterListLength[0] = (xuint8)((parameterListLength >> 8) & 0xFF);
			tcdb->MODE_SELECT10.ParameterListLength[1] = (xuint8)(parameterListLength & 0xFF);
			tcdb->MODE_SELECT10.Control = cdb->MODE_SELECT.Control;			

			pc_req->AuxBufLen = parameterListLength;
			pc_req->useAuxBuf = 1;

			pc_req->cmdlen = 10;
			sal_memcpy(pc_req->cmd, AuxCmd, pc_req->cmdlen);	

			debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
			
		}
		break;
		case SCSIOP_SEND_OPC_INFORMATION:
		{
			PCDB cdb = NULL;
			xuint16 parameterListLength = 0;

			cdb = (PCDB)pc_req->cmd;	

			parameterListLength = (xuint16)(((xuint16)cdb->SEND_OPC_INFORMATION.ParameterListLength[0] << 8)
								+ (xuint16)cdb->SEND_OPC_INFORMATION.ParameterListLength[1]);
			
			if(parameterListLength > 0) {

				if(parameterListLength > req->reqlen) {
					if(parameterListLength > MAX_AUX_MAX_PIO_BUFF_4096) {
						parameterListLength = MAX_AUX_MAX_PIO_BUFF_4096;
						
						cdb->SEND_OPC_INFORMATION.ParameterListLength[0] = (parameterListLength >> 8) & 0xFF;
						cdb->SEND_OPC_INFORMATION.ParameterListLength[1] = parameterListLength & 0xFF;
					}

					pc_req->AuxBufLen = parameterListLength;
					pc_req->useAuxBuf = 1;

					debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
						pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
						pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
						pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
					
				}

			}else {
				pc_req->AuxBufLen = 0;
			}
		}
		break;
		case SCSIOP_MODE_SELECT10:
		{
			PCDB cdb = NULL;
			xuint16 parameterListLength = 0;

			cdb = (PCDB)pc_req->cmd;	

			if(!s_info->writable &&  !s_info->writeshared) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}

			parameterListLength = (xuint16)(((xuint16)cdb->MODE_SELECT10.ParameterListLength[0] << 8)
								+ (xuint16)cdb->MODE_SELECT10.ParameterListLength[1]);

			if(parameterListLength > req->reqlen) {
				if(parameterListLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					parameterListLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->MODE_SELECT10.ParameterListLength[0] = (parameterListLength >> 8) & 0xFF;
					cdb->MODE_SELECT10.ParameterListLength[1] = parameterListLength & 0xFF;
				}

				pc_req->AuxBufLen = parameterListLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}			
			
		}
		break;
		case SCSIOP_SEND_CUE_SHEET:
		{
			PCDB cdb = NULL;
			xuint32 cueSheetSize = 0;
			
			cdb = (PCDB)pc_req->cmd;

			cueSheetSize = (xuint32)(((xuint32)cdb->SEND_CUE_SHEET.CueSheetSize[0] << 16)
							+ ((xuint32)cdb->SEND_CUE_SHEET.CueSheetSize[1] << 8)
							+ (xuint32)cdb->SEND_CUE_SHEET.CueSheetSize[2]);

			if(cueSheetSize  > req->reqlen) {
				if(cueSheetSize  > MAX_AUX_MAX_PIO_BUFF_4096) {
					cueSheetSize  = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->SEND_CUE_SHEET.CueSheetSize[0] = (cueSheetSize >> 16) & 0xFF;
					cdb->SEND_CUE_SHEET.CueSheetSize[1] = (cueSheetSize >> 8) & 0xFF;
					cdb->SEND_CUE_SHEET.CueSheetSize[2] = cueSheetSize & 0xFF;
				}

				pc_req->AuxBufLen = cueSheetSize ;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}											
		}
		break;
		case SCSIOP_SEND_EVENT:
		{
			xuint16 parameterListLength = 0;


			parameterListLength = (xuint16)(((xuint16)pc_req->cmd[8] << 8)
								+ (xuint16)pc_req->cmd[9]);

			if(req->reqlen < 8) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;			
			}


			if(parameterListLength > req->reqlen) {
				if(parameterListLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					parameterListLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					pc_req->cmd[8] = (parameterListLength >> 8) & 0xFF;
					pc_req->cmd[9] = parameterListLength & 0xFF;
				}

				pc_req->AuxBufLen = parameterListLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}						
		}
		break;
		case SCSIOP_SEND_KEY:
		{
			PCDB cdb = NULL;
			xuint16 parameterListLength = 0;

			cdb = (PCDB)pc_req->cmd;	

			parameterListLength = (xuint16)(((xuint16)cdb->SEND_KEY.ParameterListLength[0] << 8)
								+ (xuint16)cdb->SEND_KEY.ParameterListLength[1]);

			if(req->reqlen < 8) {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;			
			}


			if(parameterListLength > req->reqlen) {
				if(parameterListLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					parameterListLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->SEND_KEY.ParameterListLength[0] = (parameterListLength >> 8) & 0xFF;
					cdb->SEND_KEY.ParameterListLength[1] = parameterListLength & 0xFF;
				}

				pc_req->AuxBufLen = parameterListLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}			
		}
		break;
		case SCSIOP_READ_DVD_STRUCTURE:
		{
			PCDB cdb = NULL;
			xuint16 allocationLength = 0;

			cdb = (PCDB)pc_req->cmd;	

			allocationLength = (xuint16)(((xuint16)cdb->READ_DVD_STRUCTURE.AllocationLength[0] << 8)
							+ (xuint16)cdb->READ_DVD_STRUCTURE.AllocationLength[1]);

			if(allocationLength > req->reqlen) {
				if(allocationLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					allocationLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					cdb->READ_DVD_STRUCTURE.AllocationLength[0] = (allocationLength >> 8) & 0xFF;
					cdb->READ_DVD_STRUCTURE.AllocationLength[1] = allocationLength & 0xFF;
				}

				pc_req->AuxBufLen = allocationLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}													
		}
		break;
		case SCSIOP_SET_STREAMING:
		{
			xuint16 parameterListLength = 0;


			parameterListLength = (xuint16)(((xuint16)pc_req->cmd[9] << 8)
								+ (xuint16)pc_req->cmd[10]);


			if(parameterListLength > req->reqlen) {
				if(parameterListLength > MAX_AUX_MAX_PIO_BUFF_4096) {
					parameterListLength = MAX_AUX_MAX_PIO_BUFF_4096;
					
					pc_req->cmd[9] = (parameterListLength >> 8) & 0xFF;
					pc_req->cmd[10] = parameterListLength & 0xFF;
				}

				pc_req->AuxBufLen = parameterListLength;
				pc_req->useAuxBuf = 1;

				debug_udev(1, "CHANGED CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
					pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
					pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
					pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11]);
				
			}									
		}
		break;
		case SCSIOP_ATA_PASSTHROUGH16:
		{
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_ILLEGAL_COMMAND, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;	
		}
		default:
		break;
	}
	return ndas_io_real_atapi_processing(l_udev, CONNIO_CMD_PACKET, req);
}

LOCAL NDAS_CALL
ndas_error_t
ndas_io_internal_block_op(logunit_t* log_udev, xint8 cmd, ndas_io_request * req);

LOCAL NDAS_CALL
ndas_error_t ndas_io_real_ata_processing(logunit_t* log_udev, xint8 cmd, ndas_io_request * req);


LOCAL NDAS_CALL
ndas_error_t ndas_io_ata_check_passthrough(logunit_t * l_udev, ndas_io_request * req)
{
	nads_io_pc_request_ptr pc_req = NULL;
	ndas_slot_info_t  * s_info = NULL;
	ndas_io_add_info_ptr padd_info_passthrough = NULL;
	xuint8 com = 0;

	
	s_info = &(l_udev->info);	
	pc_req = (nads_io_pc_request_ptr)req->additionalInfo;
	padd_info_passthrough = (ndas_io_add_info_ptr)(pc_req->sensedata);
	
	sal_memset(padd_info_passthrough, 0, sizeof(ndas_io_add_info));
	

	
	if( (pc_req->cmd[0] != SCSIOP_ATA_PASSTHROUGH12)  
		&& (pc_req->cmd[0] != SCSIOP_ATA_PASSTHROUGH16) ) {
			sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
				SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;
	}
	
	padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.protocol = ((pc_req->cmd[1] & 0x1e) >> 1);
       padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.check_cond = (pc_req->cmd[2] & 0x20);
       
	if( pc_req->cmd[0] == SCSIOP_ATA_PASSTHROUGH16 ) {
		if( pc_req->cmd[1] & 0x01) {
			padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.LBA48 = 1;
			padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_feature = pc_req->cmd[3];
			padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_nsec = pc_req->cmd[5];
			padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbal = pc_req->cmd[7];
			padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbam = pc_req->cmd[9];
			padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbah = pc_req->cmd[11];
		}else {
			padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.LBA48 = 0;
		}

		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.feature = pc_req->cmd[4];
		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec = pc_req->cmd[6];
		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal = pc_req->cmd[8];
		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam = pc_req->cmd[10];
		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbah = pc_req->cmd[12];

		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.command = pc_req->cmd[14];		
	}else {
		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.LBA48 = 0;
		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.feature = pc_req->cmd[3];
		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec = pc_req->cmd[4];
		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal = pc_req->cmd[5];
		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam = pc_req->cmd[6];
		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbah = pc_req->cmd[7];

		padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.command = pc_req->cmd[9];		
	}

	com = padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.command;

	switch(com) {
	case 0xE5: /* check power mode */
	
		/* 
			mandatory for not implementing the Packet command
			no data
			support
		*/
	case 0xE2: /* place in standby power mode */
		/* 
			mandatory for not implementing the Packet command
			sectorcount => time period value
			no data
			support
		*/	
	case  0xE3: /* place in idle power mode */
		/* 
			mandatory for not implementing the Packet command
			sectorcount => time period value
			no data
			support
		*/	
	case 0xE0: /* standby immediate */
	
		/*
			This command is mandatory for devices not implementing the PACKET Command feature set
			Power Management feature set is mandatory when power management is not implemented by
the PACKET command set implemented by the device.
			This command is mandatory when the Power Management feature set is implemented
			data : none
			support
		*/	
	case 0xE1: /* idle imediate */
		/*
			This command is mandatory for devices not implementing the PACKET Command feature set
			Power Management feature set is mandatory when power management is not implemented by
the PACKET command set implemented by the device
			This command is mandatory when the Power Management feature set is implemented
			The Unload Feature of the command is optional
			data: none
			support
		*/
	case 0xE6: /* sleep */
		/*
			This command is mandatory for devices not implementing the PACKET Command feature set.
			Power Management feature set is mandatory when power management is not implemented by
the PACKET command set implemented by the device.
			This command is mandatory when the Power Management feature set is implemented.
			data: none
			support
		*/
	case  0x90:	/* execute device diagnostic */
		/*
			mandatory for all devices
			no data
			support
		*/		
	case 0xC6 : /* set multiple mode */
		/*
			Mandatory for devices not implementing the PACKET Command feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data : none
			support
		*/	
	case 0x37: /* set max address ext */
		/*
			Mandatory when the Host Protected Area feature set and the 48-bit Address feature set are
implemented.
			Use prohibited when the Removable Media feature set is implemented.
			Use prohibited when PACKET Command feature set is implemented.
			data: none
			support
		*/
	case 0xF8 : /* read native max address */
		/*
			Mandatory when the Host Protected Area feature set is implemented
			Use prohibited when Removable feature set is implemented
			data: none
			support (check return register)
		*/
	case 0x27: /* read native max address ext */
		/*
			Mandatory when the Host Protected Area feature set and the 48-bit Address feature set are
implemented.
			Use prohibited when Removable feature set is implemented.
			Use prohibited when PACKET Command feature set is implemented
			data: none
			support ( check return register)
		*/
	case 0xF9: /* set max */
		/*
			Mandatory when the Host Protected Area feature set is implemented
			Use prohibited when the Removable feature set is implemented
			data: none
			support
		*/	
	case 0xF5 : /* security freeze lock */
		/*
			Mandatory when the Security Mode feature set is implemented
			data none
			support
		*/
	case 0xE7: /* flush cache */
		/*
			mandatory for not implementing the Packet command
			no data
			support		
		*/
	case 0xEA: /* flush cache ext */
		/*
			Mandatory for all devices implementing the 48-bit Address feature
			Prohibited for devices implementing the PACKET Command feature set
			no data
			support
		*/		
	{
			debug_udev(1, "proecess COM[%02x]", com); 	
			if(req->reqlen != 0) {
				debug_udev(1, "BUG: COM[0x%02x]  buffsize[%d]",com, req->reqlen); 
				req->reqlen = 0;
			}		
	}
        break;
        case 0xEF: /* set features */
        	/*
        		Mandatory for all devices
        		Set transfer mode subcommand is mandatory
        		Enable/disable write cache subcommands are mandatory when a write cache is implemented
        		Enable/disable Media Status Notification sub commands are mandatory if the Removable Media
        feature set is implemented
        		All other subcommands are optional
        		data : none
        		support
        	*/
            {
                debug_udev(1, "proecess COM[%02x]", com); 	
                
                if( padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.feature == 0x03) {
                    sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
                    ndas_pc_io_set_sensedata(pc_req->sensedata, 
                    	SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
                    pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
                    pc_req->avaliableSenseData = 1;
                    return NDAS_OK;
                }
                
                
                if(req->reqlen != 0) {
                    debug_udev(1, "BUG: COM[0x%02x]  buffsize[%d]",com, req->reqlen); 
                    req->reqlen = 0;
                }		        
            }
        break;
	case 0x2f: /* read log ext */
		/*
			Mandatory for devices implementing the General Purpose Logging feature set
			data: sectorcount * 512
			PIO in
			support
		*/
	{
		xuint32 sector_count = 0;
		xuint32 sector_offset = 0;

		sector_count = (xuint32)((xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec
						+ ((xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_nsec << 8));
		sector_offset = (xuint32)((xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam
						+ ((xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbam << 8));
		
		debug_udev(1, "READ LOG EXT LogAddr[0x0x%x] SEC_C[%d] SEC_OFFSET[%d]", 
			padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal,
			sector_count, sector_offset);

		if(sector_count  != (req->reqlen >> 9)) {
			debug_udev(1, "READ LOG EXT error buffsize[%d] sec[%d]", req->reqlen, sector_count);
			if(sector_count > (req->reqlen >> 9)) {
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}else {
				req->reqlen = (sector_count << 9);
			}
		}
	}
	break;	
	case 0xB1 : /* device configuation */
		/*
			Individual Device Configuration Overlay feature set commands are identified by the value placed in the
Features register
			feature reg:
				0xC0: DEVICE CONFIGURATION RESTORE
					data: none
				0xC1: DEVICE CONFIGURATION FREEZE LOCK
					data: none
				0xC2: DEVICE CONFIGURATION IDENTIFY
					data: 512 bytes
					PIO in
				0xC3: DEVICE CONFIGURATION SET
					data: 512 bytes
					PIO out
			support
		*/
	{
		switch(padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.feature){
		case 0xC0:
		{
			debug_udev(1, "DEVICE CONFIGURATION RESTORE"); 
		}
		break;
		case 0xC1:
		{
			debug_udev(1, "DEVICE CONFIGURATION FREEZE LOCK"); 
		}
		break;
		case 0xC2:
		{
			debug_udev(1, "DEVICE CONFIGURATION IDENTIFY"); 
			if(req->reqlen != 512) {
				debug_udev(1, "DEVICE CONFIGURATION IDENTIFY error buffsize[%d]", req->reqlen); 
				if( req->reqlen < 512) {
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}else {
					req->reqlen = 512;
				}
			}
		}
		break;
		case 0xC3:
		{
			debug_udev(1, "DEVICE CONFIGURATION SET"); 	
			if(req->reqlen != 512) {
				debug_udev(1, "DEVICE CONFIGURATION SET error buffsize[%d]", req->reqlen); 
				if( req->reqlen < 512) {
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}else {
					req->reqlen = 512;
				}
			}
		}
		break;
		default:
			debug_udev(1, "UNSUPPORTED DEVICE CONFIGURATION SUB COM[0x%02x]",
				padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.feature);
			sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
				SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;
		break;
		}
	}
	break;
	case 0xB0: /* smart command */
		/*
			Individual SMART commands are identified by the value placed in the Feature register
			feature:
				0xD0: SMART READ DATA
					- Optional when the SMART feature set is implemented.
					- Use prohibited when the PACKET Command feature set is implemented.
					data : 512 bytes
					PIO in
				0xD1: Obsolete
				0xD2: SMART ENABLE/DISABLE ATTRIBUTE AUTOSAVE
					- Mandatory when the SMART feature set is implemented.
					- Use prohibited when the PACKET Command feature set is implemented.
					data : none
				0xD3: Obsolete
				0xD4: SMART EXECUTE OFF-LINE IMMEDIATE
					- Optional when the SMART feature set is implemented.
					- Use prohibited when the PACKET Command feature set is implemented.
					data : none
				0xD5: SMART READ LOG
					- Optional when the SMART feature set is implemented.
					- Use prohibited when the PACKET Command feature set is implemented.
					data: sectorcount * 512
					PIO in
				0xD6: SMART WRITE LOG
					- Optional when the SMART feature set is implemented.
					- Use prohibited when the PACKET Command feature set is implemented.
					data: sectorount * 512
					PIO out
				0xD7: Obsolete
				0xD8: SMART ENABLE OPERATIONS
					- Mandatory when the SMART feature set is implemented.
					- Use prohibited when the PACKET Command feature set is implemented
					data : none
				0xD9: SMART DISABLE OPERATIONS
					- Mandatory when the SMART feature set is implemented.
					- Use prohibited when the PACKET Command feature set is implemented
					data: none
				0xDA: SMART RETURN STATUS
					- Mandatory when the SMART feature set is implemented.
					- Use prohibited when the PACKET Command feature set is implemented.
					data : none
				0xDB: Obsolete
		*/
	{

		if( (padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam != 0x4F) ||
			(padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbah != 0xC2) ) 
		{

			debug_udev(1, "INVALID FILED SMART CMD LBAM[0x%02x] LBAH[0x%02x]",
				padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam,
				padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbah);
			
			sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
				SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;
		}
	
		switch(padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.feature){
		case 0xD0:
		{
			debug_udev(1, "SMART READ DATA"); 	
			if(req->reqlen != 512) {
				debug_udev(1, "SMART READ DATA error buffsize[%d]", req->reqlen); 
				if( req->reqlen < 512) {
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}else {
					req->reqlen = 512;
				}
			}										
		}
		break;
              case 0xD1:
              {
			debug_udev(1, "SMART READ THREASHOLD :: Obsolete"); 	
			if(req->reqlen != 512) {
				debug_udev(1, "SMART READ THREASHOLD error buffsize[%d]", req->reqlen); 
				if( req->reqlen < 512) {
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}else {
					req->reqlen = 512;
				}
			}							                    
              }
              break;
		case 0xD2:
		{
			debug_udev(1, "SMART ENABLE/DISABLE ATTRIBUTE AUTOSAVE SUB CMD[0x%02x]", 
				padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal); 	
			if(req->reqlen != 0) {
				debug_udev(1, "SMART ENABLE/DISABLE ATTRIBUTE AUTOSAVE error buffsize[%d]", req->reqlen); 
				req->reqlen = 0;
			}											
		}
		break;
              case 0xD3:
              {
			debug_udev(1, "SMART AUTO SAVE :: Obsolete"); 	
			if(req->reqlen != 0) {
				debug_udev(1, "SMART ENABLE/DISABLE ATTRIBUTE AUTOSAVE error buffsize[%d]", req->reqlen); 
				req->reqlen = 0;
			}					                
              }
              break;
		case 0xD4:
		{
			debug_udev(1, "SMART ENABLE/DISABLE ATTRIBUTE AUTOSAVE"); 	
			if(req->reqlen != 0) {
				debug_udev(1, "SMART ENABLE/DISABLE ATTRIBUTE AUTOSAVE error buffsize[%d]", req->reqlen); 
				req->reqlen = 0;
			}											
		}
		break;
		case 0xD5:
		{
			xuint32 num_sector = 0;
			xuint32 log_address = 0;

			num_sector = (xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec;
			log_address = (xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal;

			debug_udev(1, "SMART READ LOG NumSec[%d] Log Addr[%d]", num_sector, log_address); 	
			if(num_sector  != (req->reqlen >> 9)) {
				debug_udev(1, "SMART READ LOG error buffsize[%d] sec[%d]", req->reqlen, num_sector);
				if(num_sector > (req->reqlen >> 9)) {
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}else {
					req->reqlen = (num_sector << 9);
				}
			}	
		}
		break;
		case 0xD6:
		{
			xuint32 num_sector = 0;
			xuint32 log_address = 0;

			num_sector = (xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec;
			log_address = (xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal;

			if(s_info->mode == NDAS_DISK_MODE_SINGLE){
				if(!s_info->writable &&  !s_info->writeshared) {
					debug_udev(1, "COM[0x%02x] SMART WRITE LOG with NO WRITE PERMISSION", com);
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}
			}else {
				if(!l_udev->has_key) {
					debug_udev(1, "COM[0x%02x] SMART WRITE LOG with NO WRITE PERMISSION", com);
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}
			}

			debug_udev(1, "SMART WRITE LOG NumSec[%d] Log Addr[%d]", num_sector, log_address); 	
			if(num_sector  != (req->reqlen >> 9)) {
				debug_udev(1, "SMART WRITE LOG error buffsize[%d] sec[%d]", req->reqlen, num_sector);
				if(num_sector > (req->reqlen >> 9)) {
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;;
				}else {
					req->reqlen = (num_sector << 9);
				}
			}			
		}
		break;
              case 0xD7:
              {
			debug_udev(1, "SMART WRITE THREASHOLD :: Obsolete"); 	

			if(s_info->mode == NDAS_DISK_MODE_SINGLE){
				if(!s_info->writable &&  !s_info->writeshared) {
					debug_udev(1, "COM[0x%02x] SMART WRITE THREASHOLD with NO WRITE PERMISSION", com);
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}
			}else {
				if(!l_udev->has_key) {
					debug_udev(1, "COM[0x%02x] SMART WRITE THREASHOLDwith NO WRITE PERMISSION", com);
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}
			}

            
			if(req->reqlen != 512) {
				debug_udev(1, "SMART WRITE THREASHOLD error buffsize[%d]", req->reqlen); 
				if( req->reqlen < 512) {
					sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}else {
					req->reqlen = 512;
				}
			}							                
              }
              break;
		case 0xD8:
		{
			debug_udev(1, "SMART EXECUTE OFF-LINE IMMEDIATE"); 	
			if(req->reqlen != 0) {
				debug_udev(1, "SMART EXECUTE OFF-LINE IMMEDIATE error buffsize[%d]", req->reqlen); 
				req->reqlen = 0;
			}										
		}
		break;
		case 0xD9:
		{
			
			debug_udev(1, "SMART DISABLE OPERATIONS"); 	
			if(req->reqlen != 0) {
				debug_udev(1, "SMART DISABLE OPERATIONS error buffsize[%d]", req->reqlen); 
				req->reqlen = 0;
			}									
		}
		break;
		case 0xDA:
		{
			debug_udev(1, "SMART RETURN STATUS"); 	
			if(req->reqlen != 0) {
				debug_udev(1, "SMART RETURN STATUS error buffsize[%d]", req->reqlen); 
				req->reqlen = 0;
			}									
		}
		break;
		default:
			debug_udev(1, "SMART UNSUPPORTED SUB COM[0x%02x]", 
				padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.feature); 
			
			sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
				SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;
			break;
		}
	}
	break;
	case 0xEC: /* identify device */
		/*
			Mandatory for all devices
			data : 256 word (512 bytes)
			PIO in
			support
		*/
	case 0xA1: /* indentify packet device */
		/*
			Use prohibited for devices not implementing the PACKET Command feature set
			Mandatory for devices implementing the PACKET Command feature set
			data : 256 word (512 bytes)
			PIO in
			return error	
		*/
	case 0xE4 : /* read buffer */
		/*
			Optional for devices not implementing the PACKET Command feature set.
			Use prohibited for devices implementing the PACKET Command feature set.
			data 512 bytes
			PIO in
			support
		*/					
	{

		debug_udev(1, "COM[0x%02x]", com); 	
		if(req->reqlen != 512) {
			debug_udev(1, "BUG : COM[0x%02x] buffsize[%d]", com, req->reqlen); 
			if( req->reqlen < 512) {
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}else {
				req->reqlen = 512;
			}
		}									
	}
	break;
	case 0xE8 : /* write buffer */
		/*
			Optional for devices not implementing the PACKET Command feature set.
			Use prohibited for devices implementing the PACKET Command feature set
			data 512 bytes
			PIO out
			support
		*/
	{

		if(s_info->mode == NDAS_DISK_MODE_SINGLE){
			if(!s_info->writable &&  !s_info->writeshared) {
				debug_udev(1, "COM[0x%02x] with NO WRITE PERMISSION", com);
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
		}else {
			if(!l_udev->has_key) {
				debug_udev(1, "COM[0x%02x] with NO WRITE PERMISSION", com);
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
		}
		debug_udev(1, "COM[0x%02x]", com); 	
		if(req->reqlen != 512) {
			debug_udev(1, "BUG : COM[0x%02x] buffsize[%d]", com, req->reqlen); 
			if( req->reqlen < 512) {
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}else {
				req->reqlen = 512;
			}
		}									
	}		
	break;	
	case 0xC8: /* read dma */
	
		/*
			Mandatory for devices not implementing the PACKET Command feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data : sectorcount * 512
			address check
			DMA
			support
		*/
	case 0x20: /* read sector */
	
		/*
			Mandatory for all devices
			data sectorcount * 512
			address check
			PIO in
			support
		*/
	case 0xC4: /* read multiple */
		/*
			Mandatory for devices not implementing the PACKET Command feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data sectorcount * 512
			address check
			PIO in
			support
		*/		
	{
		xuint32 nsec = 0;
		xuint64 lba = 0;
		xuint64 sectorCount = 0;
		
		nsec= (xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec;
		
		lba = (xuint64)((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam << 8)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbah << 16)) ;


		if(s_info->mode == NDAS_DISK_MODE_SINGLE){
			sectorCount = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
		}else {
			sectorCount = s_info->sectors;
		}
			
		
		debug_udev(1, "COM[0x%02x] NSEC[%d] LBA[0x%llx]", com, nsec, lba);


		if(nsec != (req->reqlen >> 9) ) {
			if( nsec > (req->reqlen >> 9) ) {
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;

			}else {
			  	req->reqlen = (nsec << 9);
			}
		}

		if( (xuint64)(nsec + lba) >= sectorCount ) {
			debug_udev(1, "BUG COM[0x%02x]  NSEC[%d] LBA[0x%llx] totalcount[0x%llx]", com, nsec, lba, sectorCount);
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;

		}
		
	}
	break;
	case 0x25: /* read dma ext*/
	
		/*
			Mandatory for devices not implementing the PACKET Command feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data : sectorcount (2) * 512
			address check
			DMA
			support		
		*/
	case 0x24: /* read sectors ext */
	
		/*
			Mandatory for all devices implementing the 48-bit Address feature set
			data sectorcount(2) * 512
			address check
			PIO in
			support		
		*/
	case 0x29: /* read multiple ext */
		/*
			Mandatory for all devices implementing the 48-bit Address feature set
			Use prohibited when the PACKET command feature set is implemented
			data sectorcount(2) * 512
			address check
			PIO in
			support
		*/
		
	{
		xuint32 nsec = 0;
		xuint64 lba = 0;
		xuint64 sectorCount = 0;


		if(padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.LBA48 == 0) {
			debug_udev(1, "BUG:  COM[0x%02x] LBA48 is not SET", com);
			return NDAS_ERROR;
		}
		
		nsec= (xuint32)((xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec
				+ ((xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_nsec << 8));
		
		lba = (xuint64)((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam << 8)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbah << 16)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbal << 24)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbam << 32)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbah << 40));


		if(s_info->mode == NDAS_DISK_MODE_SINGLE){
			sectorCount = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
		}else {
			sectorCount = s_info->sectors;
		}
			
		
		debug_udev(1, "COM[0x%02x] NSEC[%d] LBA[0x%llx]", com, nsec, lba);


		if(nsec != (req->reqlen >> 9) ) {
			if( nsec > (req->reqlen >> 9) ) {
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;

			}else {
			  	req->reqlen = (nsec << 9);
			}
		}

		if( (xuint64)(nsec + lba) >= sectorCount ) {
			debug_udev(1, "BUG: COM[0x%02x] NSEC[%d] LBA[0x%llx] totalcount[0x%llx]", com, nsec, lba, sectorCount);
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;

		}		
	}
	break;
	case 0xCA: /*write dma */
		/*
			Mandatory for devices not implementing the PACKET Command feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data: sectorcount * 512
			address check
			DMA
			support
		*/
	case 0x30: /* write sector */
		/*
			Mandatory for devices not implementing the PACKET Command feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data sectorcount * 512
			address check
			PIO out
			support
		*/
	case 0xC5: /* write multiple */
		/*
			Mandatory for devices not implementing the PACKET Command feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data sectorcount * 512
			address check
			PIO out
			support
		*/		
	{
		xuint32 nsec = 0;
		xuint64 lba = 0;
		xuint64 sectorCount = 0;


		if(s_info->mode == NDAS_DISK_MODE_SINGLE){
			if(!s_info->writable &&  !s_info->writeshared) {
				debug_udev(1, "COM[0x%02x] with NO WRITE PERMISSION", com);
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
		}else {
			if(!l_udev->has_key) {
				debug_udev(1, "COM[0x%02x] with NO WRITE PERMISSION", com);
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
		}
			
		
		nsec= (xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec;
		
		lba = (xuint64)((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam << 8)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbah << 16)) ;


		if(s_info->mode == NDAS_DISK_MODE_SINGLE){
			sectorCount = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
		}else {
			sectorCount = s_info->sectors;
		}
			
		
		debug_udev(1, "COM[0x%02x] NSEC[%u] LBA[0x%llx]", com, nsec, lba);


		if(nsec != (req->reqlen >> 9) ) {
			if( nsec > (req->reqlen >> 9) ) {
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
				    SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}else {
			  	req->reqlen = (nsec << 9);
			}
		}
                                                           
		if( (xuint64)(nsec + lba) >= sectorCount ) {
			debug_udev(1, "BUG: COM[0x%02x] NSEC[%d] LBA[0x%llx] totalcount[0x%llx]", com, nsec, lba, sectorCount);
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
				    SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
		}		
	}
	break;
	case 0x35: /* write dma ext */
		/*
			Mandatory for devices not implementing the PACKET Command feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data: sectorcount(2) * 512
			address check
			DMA
			support
		*/
	case 0x3D: /* write dma fua ext */
		/*
			Optional for devices implementing the 48-bit Address feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data: sectorcount(2) * 512
			address check
			DMA
			support
		*/
	case 0x34: /* write sector ext */
		/*
			Mandatory for devices implementing the 48-bit Address feature set
			Use prohibited for devices implementing the PACKET Command feature set.
			data sectorcount(2) * 512
			address check
			PIO out
			support
		*/	
	case 0x39: /* write multiple ext */
		/*
			Mandatory for devices implementing the 48-bit Address feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data sectorcount(2) * 512
			address check
			PIO out
			support
		*/
	case 0xCE: /* write multiple fua ext */
		/*
			Optional for devices implementing the 48-bit Address feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data sectorcount(2) * 512
			address check
			PIO out
			support
		*/		
	{
		xuint32 nsec = 0;
		xuint64 lba = 0;
		xuint64 sectorCount = 0;


		if(padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.LBA48 == 0) {
			debug_udev(1, "BUG: COM[0x%02x] LBA48 is not SET", com);
			return NDAS_ERROR;
		}


		if(s_info->mode == NDAS_DISK_MODE_SINGLE){
			if(!s_info->writable &&  !s_info->writeshared) {
				debug_udev(1, "BUG: COM[0x%02x] EXT with NO WRITE PERMISSION",com);
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
		}else {
			if(!l_udev->has_key) {
				debug_udev(1, "BUG: COM[0x%02x] with NO WRITE PERMISSION", com);
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}
		}
		
		nsec= (xuint32)((xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec
				+ ((xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_nsec << 8));
		
		lba = (xuint64)((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam << 8)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbah << 16)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbal << 24)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbam << 32)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbah << 40));


		if(s_info->mode == NDAS_DISK_MODE_SINGLE){
			sectorCount = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
		}else {
			sectorCount = s_info->sectors;
		}
			
		
		debug_udev(1, "COM[0x%02x] NSEC[%d] LBA[0x%llx]", com, nsec, lba);


		if(nsec != (req->reqlen >> 9) ) {
			if( nsec > (req->reqlen >> 9) ) {
				sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}else {
			  	req->reqlen = (nsec << 9);
			}
		}

		if( (xuint64)(nsec + lba) >= sectorCount ) {
			debug_udev(1, "BUG: COM[0x%02x]  NSEC[%d] LBA[0x%llx] totalcount[0x%llx]", com, nsec, lba, sectorCount);
			sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
				SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;
		}				
	}
	break;
	case 0x40:/* read verify sectors */
		/*
			Mandatory for all devices not implementing the PACKET Command feature set
			Use prohibited for devices implementing the PACKET Command feature set
			data : none
			address check
			support
		*/
	{
		xuint32 nsec = 0;
		xuint64 lba = 0;
		xuint64 sectorCount = 0;
		
		nsec= (xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec;
		
		lba = (xuint64)((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam << 8)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbah << 16)) ;


		debug_udev(1, "COM[0x%02x]",com); 	
		if(req->reqlen != 0) {
			debug_udev(1, "BUG: COM[0x%02x] buffsize[%d]", com, req->reqlen); 
			req->reqlen = 0;
		}						

		if(s_info->mode == NDAS_DISK_MODE_SINGLE){
			sectorCount = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
		}else {
			sectorCount = s_info->sectors;
		}
			
		
		debug_udev(1, "COM[0x%02x] NSEC[%d] LBA[0x%llx]", com, nsec, lba);

		if( (xuint64)(nsec + lba) >= sectorCount ) {
			debug_udev(1, "BUG COM[0x%02x]  NSEC[%d] LBA[0x%llx] totalcount[0x%llx]", com, nsec, lba, sectorCount);
			sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
				SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;
		}
		
	}
	break;
	case 0x42: /* read verify ext */
		/*
			Mandatory for all devices implementing the 48-bit Address feature set
			Use prohibited when the PACKET command feature set is implemented
			data : none
			address check
			support
		*/
	{
		xuint32 nsec = 0;
		xuint64 lba = 0;
		xuint64 sectorCount = 0;


		if(padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.LBA48 == 0) {
			debug_udev(1, "BUG:  COM[0x%02x] LBA48 is not SET", com);
			sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
				SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;
		}
		
		nsec= (xuint32)((xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.nsec
				+ ((xuint32)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_nsec << 8));
		
		lba = (xuint64)((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbal
						+((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbam << 8)
						+ ((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.lbah << 16)
						+((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbal << 24)
						+((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbam << 32)
						+((xuint64)padd_info_passthrough->ADD_INFO_ATA_PASSTHROUGH.hob_lbah << 40));


		debug_udev(1, "COM[0x%02x]",com); 	
		if(req->reqlen != 0) {
			debug_udev(1, "BUG: COM[0x%02x] buffsize[%d]", com, req->reqlen); 
			req->reqlen = 0;
		}						

		if(s_info->mode == NDAS_DISK_MODE_SINGLE){
			sectorCount = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
		}else {
			sectorCount = s_info->sectors;
		}
			
		
		debug_udev(1, "COM[0x%02x] NSEC[%d] LBA[0x%llx]", com, nsec, lba);


		if( (xuint64)(nsec + lba) >= sectorCount ) {
			debug_udev(1, "BUG: COM[0x%02x] NSEC[%d] LBA[0x%llx] totalcount[0x%llx]", com, nsec, lba, sectorCount);
			sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
				SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;
		}				
	}
	break;
	case 0x08: /* ATAPI device reset */
		/* 
			not supported by ATA device.
			no data
			not support
		*/
	case 0xA0: /* packet */
		/*
			Use prohibited for devices not implementing the PACKET Command feature set
			Mandatory for devices implementing the PACKET Command feature set
			not supported
		*/
	{
		debug_udev(1, "COM[0x%02x] is not supported", com);
		sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
		ndas_pc_io_set_sensedata(pc_req->sensedata, 
			SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
		pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
		pc_req->avaliableSenseData = 1;
		return NDAS_OK;
	}
	break;	
	default:
		debug_udev(1, "COM[0x%02x] is not supported", com);
		sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
		ndas_pc_io_set_sensedata(pc_req->sensedata, 
			SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
		pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
		pc_req->avaliableSenseData = 1;
		return NDAS_OK;
	break;
	}
	
	return ndas_io_real_ata_processing(l_udev, CONNIO_CMD_PASSTHOUGH, req);
}


/* 
        define  
*/
#define NDAS_RW_RECOVERY_MPAGE 0x1
#define NDAS_RW_RECOVERY_MPAGE_LEN 12
#define NDAS_CACHE_MPAGE 0x8
#define NDAS_CACHE_MPAGE_LEN 20
#define NDAS_CONTROL_MPAGE 0xa
#define NDAS_CONTROL_MPAGE_LEN 12
#define NDAS_ALL_MPAGES 0x3f


static const xuint8 ndas_ata_rw_recovery_mpage[NDAS_RW_RECOVERY_MPAGE_LEN] = {
	NDAS_RW_RECOVERY_MPAGE,
	NDAS_RW_RECOVERY_MPAGE_LEN - 2,
	(1 << 7),	/* AWRE */
	0,		/* read retry count */
	0, 0, 0, 0,
	0,		/* write retry count */
	0, 0, 0
};

static const xuint8 ndas_ata_cache_mpage[NDAS_CACHE_MPAGE_LEN] = {
	NDAS_CACHE_MPAGE,
	NDAS_CACHE_MPAGE_LEN - 2,
	0,		/* contains WCE, needs to be 0 for logic */
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0,		/* contains DRA, needs to be 0 for logic */
	0, 0, 0, 0, 0, 0, 0
};

static const xuint8 ndas_ata_control_mpage[NDAS_CONTROL_MPAGE_LEN] = {
	NDAS_CONTROL_MPAGE,
	NDAS_CONTROL_MPAGE_LEN - 2,
	2,	/* DSENSE=0, GLTSD=1 */
	0,	/* [QAM+QERR may be 1, see 05-359r1] */
	0, 0, 0, 0, 0xff, 0xff,
	0, 30	/* extended self test time, see 05-359r1 */
};




LOCAL NDAS_CALL
ndas_error_t ndas_io_ata_process(logunit_t * l_udev, ndas_io_request * req)
{
	nads_io_pc_request_ptr pc_req = NULL;
	ndas_slot_info_t  * s_info = NULL;
	xuint32 transfer_size;
	s_info = &(l_udev->info);	
	pc_req = (nads_io_pc_request_ptr)req->additionalInfo;
	
	debug_udev(1, "CMND[0x%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x::%02x:%02x:%02x:%02x]",
			pc_req->cmd[0],pc_req->cmd[1],pc_req->cmd[2],pc_req->cmd[3],
			pc_req->cmd[4],pc_req->cmd[5],pc_req->cmd[6],pc_req->cmd[7],
			pc_req->cmd[8],pc_req->cmd[9],pc_req->cmd[10],pc_req->cmd[11],
			pc_req->cmd[12],pc_req->cmd[13],pc_req->cmd[14],pc_req->cmd[15]);
	switch(pc_req->cmd[0]){
		case SCSIOP_INQUIRY:
		{
			INQUIRYDATA inquiry;
			debug_udev(1, "call SCSIOP_INQUIRY");	
			if(pc_req->cmd[1] & CDB_INQUIRY_EVPD) {
				debug_udev(1, "err: ndas_io_ata_process: SCSIOP_INQURY: INVALID CDB_INQUIRY_EVPD");	
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}

			sal_memset(&inquiry, 0, sizeof(INQUIRYDATA));
			
			inquiry.DeviceType = DIRECT_ACCESS_DEVICE;
			inquiry.DeviceTypeQualifier = DEVICE_QUALIFIER_ACTIVE;
			inquiry.RemovableMedia = 0;
			inquiry.u1.Versions = 5;
			inquiry.ResponseDataFormat = 2;
			inquiry.AdditionalLength = sizeof(INQUIRYDATA)-4;
			
			if(s_info->mode == NDAS_DISK_MODE_SINGLE) {
				sal_memcpy(inquiry.VendorId, NDAS_VENDOR_ID_ATA, strlen(NDAS_VENDOR_ID_ATA));
			}else {	
				sal_memcpy(inquiry.VendorId, NDAS_VENDOR_ID, strlen(NDAS_VENDOR_ID));
			}
			if((s_info->mode == NDAS_DISK_MODE_SINGLE) || (s_info->mode == NDAS_DISK_MODE_ATAPI)){
				ndas_unit_info_t * u_info = NULL;
				udev_t * udev = NULL;
			   	ndas_device_info * d_info = NULL;

	
				u_info = (ndas_unit_info_t *)l_udev->info2;
				udev = (udev_t *)l_udev->disks;
				d_info = (ndas_device_info *)udev->ndev;	
				
				sal_memcpy(inquiry.ProductId, u_info->model, 16);
				switch(d_info->version) {
					case NDAS_VERSION_1_0 :
						sal_memcpy(inquiry.ProductRevisionLevel, "1.0", 4);
						break;
					case NDAS_VERSION_1_1:
						sal_memcpy(inquiry.ProductRevisionLevel,  "1.1", 4);
						break;
					case NDAS_VERSION_2_0:
						sal_memcpy(inquiry.ProductRevisionLevel, "2.0", 4);
						break;
					default:
						sal_memcpy(inquiry.ProductRevisionLevel, "0.0", 4);
						break;
				}
				
			}else {
				sal_memcpy(inquiry.ProductId, "RAID", 5);
				
				if(s_info->mode == NDAS_DISK_MODE_RAID0) {
					sal_memcpy(inquiry.ProductRevisionLevel, "1.0", 4);
				}else if (s_info->mode == NDAS_DISK_MODE_RAID1) {
					sal_memcpy(inquiry.ProductRevisionLevel, "2.0", 4);
				}else if (s_info->mode == NDAS_DISK_MODE_RAID4) {
					sal_memcpy(inquiry.ProductRevisionLevel, "5.0", 4);
				}else if (s_info->mode == NDAS_DISK_MODE_AGGREGATION) {
					sal_memcpy(inquiry.ProductRevisionLevel, "1.1", 4);
				}else {
					sal_memcpy(inquiry.ProductRevisionLevel, "0.0", 4);
				}
			}

			
			transfer_size = ndas_pc_io_write_to_sal_bio((void *)&inquiry, req, 0, sizeof(INQUIRYDATA), 1);

			
			if(transfer_size > req->reqlen) transfer_size = req->reqlen;
			pc_req->resid = req->reqlen - transfer_size;
			pc_req->status = NDAS_IO_STATUS_SUCCESS;
			return NDAS_OK;
			
		}
		break;
		case SCSIOP_READ_CAPACITY: 
		{
			READ_CAPACITY_DATA readCapacityData;
			xuint64 sectorCount = 0;
			xuint32 logicalBlockAddress = 0;
			xuint32 blockSize = 0;

			debug_udev(1, "call SCSIOP_READ_CAPACITY");	

			if(s_info->mode == NDAS_DISK_MODE_SINGLE){
				sectorCount = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
			}else {
				sectorCount = s_info->sectors;
			}
			
			logicalBlockAddress = (xuint32)(sectorCount -1);

			if(logicalBlockAddress < 0xffffffff) {
				REVERSE_BYTES_32(&readCapacityData.LogicalBlockAddress,  &logicalBlockAddress);
				debug_udev(1, "SCSIOP_READ_CAPACITY blockaddress ORGINAL 0x%x", logicalBlockAddress);
				debug_udev(1, "SCSIOP_READ_CAPACITY blockaddress 0x%x", readCapacityData.LogicalBlockAddress);
					
			}else {
				readCapacityData.LogicalBlockAddress = 0xffffffff;
			}

			blockSize = s_info->sector_size;
			REVERSE_BYTES_32(&readCapacityData.BytesPerBlock, &blockSize);

			debug_udev(1, "SCSIOP_READ_CAPACITY block size ORGINAL 0x%x", blockSize);
			debug_udev(1, "SCSIOP_READ_CAPACITY blockaddress 0x%x", readCapacityData.BytesPerBlock);

			transfer_size = ndas_pc_io_write_to_sal_bio((void *)&readCapacityData, req, 0, sizeof(READ_CAPACITY_DATA), 1);
			if(transfer_size > req->reqlen) transfer_size = req->reqlen;
			pc_req->resid = req->reqlen - transfer_size;
			pc_req->status = NDAS_IO_STATUS_SUCCESS;
			return NDAS_OK;
			
		}
		break;
		case SCSIOP_READ_CAPACITY16:
		{
			READ_CAPACITY_DATA_EX readCapacityDataEx;
			xuint32	blockSize;
			xuint64	sectorCount;
			xuint64 	logicalBlockAddress;

			if(s_info->mode == NDAS_DISK_MODE_SINGLE){
				sectorCount = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
			}else {
				sectorCount = s_info->sectors;
			}
			
			logicalBlockAddress = sectorCount -1;

			REVERSE_BYTES_64(&readCapacityDataEx.LogicalBlockAddress, &logicalBlockAddress);

			debug_udev(1, "SCSIOP_READ_CAPACITYEX blockaddress ORGINAL 0x%llx", logicalBlockAddress);
			debug_udev(1, "SCSIOP_READ_CAPACITYEX blockaddress 0x%llx", readCapacityDataEx.LogicalBlockAddress);


			blockSize = s_info->sector_size;
			REVERSE_BYTES_32(&readCapacityDataEx.BytesPerBlock, &blockSize);
			debug_udev(1, "SCSIOP_READ_CAPACITY block size ORGINAL 0x%x", blockSize);
			debug_udev(1, "SCSIOP_READ_CAPACITY blockaddress 0x%x", readCapacityDataEx.BytesPerBlock);
			
			transfer_size = ndas_pc_io_write_to_sal_bio((void *)&readCapacityDataEx, req, 0, sizeof(READ_CAPACITY_DATA_EX), 1);
			if(transfer_size > req->reqlen) transfer_size = req->reqlen;
			pc_req->resid = req->reqlen - transfer_size;
			pc_req->status = NDAS_IO_STATUS_SUCCESS;
			return NDAS_OK;			
			
		}
		break;
		case SCSIOP_RESERVE_UNIT:
		{
			pc_req->status = NDAS_IO_STATUS_SUCCESS;
			return NDAS_OK;
		}
		break;
		case SCSIOP_RESERVE_UNIT10:
		{
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
				SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_ILLEGAL_COMMAND, 0);
			pc_req->status = NDAS_IO_STATUS_COMMAND_FAILED;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;			
		}
		break;
		case SCSIOP_RELEASE_UNIT:
		{
			pc_req->status = NDAS_IO_STATUS_SUCCESS;
			return NDAS_OK;				
		}
		break;
		case SCSIOP_RELEASE_UNIT10:
		{
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
				SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_ILLEGAL_COMMAND, 0);
			pc_req->status = NDAS_IO_STATUS_COMMAND_FAILED;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;						
		}
		break;
		case SCSIOP_TEST_UNIT_READY:
		{
			/* TODO : check power state */
			debug_udev(1, "call SCSIOP_TEST_UNIT_READY");	
			pc_req->status = NDAS_IO_STATUS_SUCCESS;
			return NDAS_OK;							
			
		}
		break;
		case SCSIOP_START_STOP_UNIT:
		{
			/* disable enable media command like eject/load */
			pc_req->status = NDAS_IO_STATUS_SUCCESS;
			return NDAS_OK;										
			
		}
		break;
		case SCSIOP_MODE_SENSE:
              case SCSIOP_MODE_SENSE10:
		{
                    	const xuint8 BLK_DESC[] = {
		                    0, 0, 0, 0,	/* number of blocks: sat unspecified */
		                    0,
		                    0, 0x2, 0x0	/* block length: 512 bytes */
	               };
                     xuint32 blk_desc_size = 8;
                     
                     xuint8  buf[512];         
                     xuint32 bufsize = 512;
                     xuint32 datalen = req->reqlen;
			xuint8 * p_current = NULL;
                     xuint8 * p_last = NULL;
                     
                     xuint8 six_byte = 0;
                     xuint8 ebd = 0;
		       xuint8 page_control = 0;
                     xuint8 pg = 0;
                     xuint8 spg = 0;
                     xuint8 dpofua = 0;
                     
	              xuint32 output_len = 0;
                     xuint32 alloc_len = 0;
                     xuint32 minlen = 0;
                                         
                    sal_memset(buf, 0, bufsize); 
                     if(pc_req->cmd[0] == SCSIOP_MODE_SENSE) {
                        six_byte = 1;
                     }

                    ebd = !(pc_req->cmd[1] & 0x8);
                    page_control = pc_req->cmd[2] >> 6;

                    switch(page_control){
                        case 0:
                        break;
                        case 3:
                        {
                            ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, 0x39, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
                            return NDAS_OK;
                        }
                        break;
                        default:
                        {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;                            
                        }
                        break;
                    }
                    
                    if(six_byte) {
                        output_len = 4 + (ebd?8:0);
                        alloc_len = pc_req->cmd[4];
                    }else {
                        output_len = 8 + (ebd?8:0);
                        alloc_len = ((xuint32)pc_req->cmd[7] << 8) + (xuint32)pc_req->cmd[8];
                    }

                    minlen = (alloc_len < datalen) ? alloc_len : datalen;
                    pg = pc_req->cmd[2] * 0x3f;
                    spg = pc_req->cmd[3];


                    p_current = buf+output_len;
                    p_last = buf+minlen -1;


                    if( spg && (spg != 0xff)){
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;                                                    
                    }

                    switch(pg) {
                        case NDAS_RW_RECOVERY_MPAGE:
                        {
                             if((p_current + NDAS_RW_RECOVERY_MPAGE_LEN -1) > p_last) {
                                    debug_udev(1, "MODE_SENSE PG:RW_RECOVERY: OVERRUN!");	
                             }else {
                                  sal_memcpy(p_current, ndas_ata_rw_recovery_mpage, NDAS_RW_RECOVERY_MPAGE_LEN);
                                  p_current += NDAS_RW_RECOVERY_MPAGE_LEN;
                             }
                             output_len += NDAS_RW_RECOVERY_MPAGE_LEN;
                             
                        }
                        break;
                        case NDAS_CACHE_MPAGE:
                        {
                             if((p_current + NDAS_CACHE_MPAGE_LEN -1) > p_last) {
                                     debug_udev(1, "MODE_SENSE PG:CACHE_MPAGE: OVERRUN!");	
                             }else {
                                  sal_memcpy(p_current, ndas_ata_cache_mpage, NDAS_CACHE_MPAGE_LEN);
                                  p_current += NDAS_CACHE_MPAGE_LEN;
                             }
                             output_len += NDAS_CACHE_MPAGE_LEN;                            
                        }
                        break;
                        case NDAS_CONTROL_MPAGE:
                        {
                             if((p_current + NDAS_CONTROL_MPAGE_LEN -1) > p_last) {
                                    debug_udev(1, "MODE_SENSE PG:CONTROL_MPAGE: OVERRUN!");	
                             }else {
                                  sal_memcpy(p_current, ndas_ata_control_mpage, NDAS_CONTROL_MPAGE_LEN);
                                  p_current += NDAS_CONTROL_MPAGE_LEN;
                             }
                             output_len += NDAS_CONTROL_MPAGE_LEN;                                       
                        }
                        break;
                        case NDAS_ALL_MPAGES:
                        {
                             if((p_current + NDAS_RW_RECOVERY_MPAGE_LEN -1) > p_last) {
                                    debug_udev(1, "MODE_SENSE PG:RW_RECOVERY: OVERRUN!");	
                             }else {
                                  sal_memcpy(p_current, ndas_ata_rw_recovery_mpage, NDAS_RW_RECOVERY_MPAGE_LEN);
                                  p_current += NDAS_RW_RECOVERY_MPAGE_LEN;
                             }
                             output_len += NDAS_RW_RECOVERY_MPAGE_LEN;   

                             if((p_current + NDAS_CACHE_MPAGE_LEN -1) > p_last) {
                                    debug_udev(1, "MODE_SENSE PG:CACHE_MPAGE: OVERRUN!");	
                             }else {
                                  sal_memcpy(p_current, ndas_ata_cache_mpage, NDAS_CACHE_MPAGE_LEN);
                                  p_current += NDAS_CACHE_MPAGE_LEN;
                             }
                             output_len += NDAS_CACHE_MPAGE_LEN;                            

                            if((p_current + NDAS_CONTROL_MPAGE_LEN -1) > p_last) {
                                    debug_udev(1, "MODE_SENSE PG:CONTROL_MPAGE: OVERRUN!");	      
                             }else {
                                  sal_memcpy(p_current, ndas_ata_control_mpage, NDAS_CONTROL_MPAGE_LEN);
                                  p_current += NDAS_CONTROL_MPAGE_LEN;
                             }
                             output_len += NDAS_CONTROL_MPAGE_LEN;                         
                             
                        }
                        break;
                        default:
                        {
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;                                                        
                        }
                        break;
                    }

                    if(minlen < 1) {
                            pc_req->status = NDAS_IO_STATUS_SUCCESS;
				return NDAS_OK;                                    
                    }

                   
                    dpofua =0;

                    if(six_byte) {
                        output_len --;
                        buf[0] = output_len;
                        if(minlen > 2) {
                            buf[2] |= dpofua;
                        }

                        if(ebd) {
                            if(minlen > 3) {
                                buf[3] = blk_desc_size;
                            }

                            if(minlen > 11) {
                                sal_memcpy(buf + 4, BLK_DESC, blk_desc_size);
                            }
                        }
                        
                    }else {
                        output_len -=2;
                        buf[0] = (output_len >> 8);
                        if(minlen > 1) {
                            buf[1] = output_len;
                        }
                        if(minlen > 3) {
                            buf[3] |= dpofua;
                        }

                        if(ebd) {
                            if(minlen > 7) {
                                buf[7] = blk_desc_size;
                                if(minlen > 15) {
                                     sal_memcpy(buf + 8, BLK_DESC, blk_desc_size);
                                }
                            }
                        }
                        
                    }
                    
			transfer_size = ndas_pc_io_write_to_sal_bio((void *)&buf, req, 0, minlen, 1);
			if(transfer_size > req->reqlen) transfer_size = req->reqlen;
			pc_req->resid = req->reqlen - transfer_size;
			pc_req->status = NDAS_IO_STATUS_SUCCESS;
			return NDAS_OK;
		}
		break;
		case SCSIOP_MODE_SELECT:
		{
			PCDB cdb = NULL;
			xuint32 datalen = req->reqlen;
			xuint32 reqlen = sizeof(MODE_PARAMETER_HEADER);
			xuint32 bufsize = sizeof(MODE_PARAMETER_BLOCK) + sizeof(MODE_PARAMETER_HEADER) + sizeof(MODE_CACHING_PAGE);
			xuint8  buf[bufsize];
			xuint8 *modepage = NULL;
			xuint8  parameterlen = 0;

	
			PMODE_PARAMETER_HEADER modeH = NULL;
			PMODE_PARAMETER_BLOCK modeD = NULL;
			PMODE_CACHING_PAGE	modeC = NULL;
			
			
			if(datalen < reqlen) {
				debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: too small buffer datalen (%d) reqlen(%d)", 
						datalen, reqlen);					
				pc_req->status = NDAS_IO_STATUS_DATA_OVERRUN;
				return NDAS_OK;
			}

			cdb = (PCDB)pc_req->cmd;

			reqlen += sizeof(MODE_PARAMETER_BLOCK);
			if(datalen < reqlen) {
				debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: too small buffer datalen (%d) reqlen(%d)", 
						datalen, reqlen);	
				pc_req->status = NDAS_IO_STATUS_DATA_OVERRUN;
				return NDAS_OK;
			}		

			if(cdb->MODE_SELECT.PFBit == 0) {
				debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: MODE_SELECT.PFBit == 0");
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;				
			}else if (cdb->MODE_SELECT.SPBit == 0) {
			       debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: MODE_SELECT.SPBit == 0");
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;				
			}

			if(!s_info->writable &&  !s_info->writeshared) {
				debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: not support write");
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				pc_req->avaliableSenseData = 1;
				return NDAS_OK;
			}

			sal_memset(buf, 0, bufsize);
			ndas_pc_io_read_from_sal_bio((void *)buf, req, 0, bufsize);
			
			modeH = (PMODE_PARAMETER_HEADER)buf;
			modeD = (PMODE_PARAMETER_BLOCK)(buf + sizeof(MODE_PARAMETER_HEADER));
			parameterlen = cdb->MODE_SELECT.ParameterListLength;
			modepage = (xuint8 *)modeD + sizeof(MODE_PARAMETER_BLOCK);

			if(  (*modepage & 0x3f) == MODE_PAGE_CACHING) {

				ndas_error_t err = NDAS_OK;
				const lsp_ata_handshake_data_t	* hsdata = NULL;

				
				reqlen += sizeof(MODE_CACHING_PAGE);
				if(datalen < reqlen) {
					debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: too small buffer datalen (%d) reqlen(%d)", 
						datalen, reqlen);	
					pc_req->status = NDAS_IO_STATUS_DATA_OVERRUN;
					return NDAS_OK;
				}
				
				modeC = (PMODE_CACHING_PAGE)((xuint8 *)modeD + sizeof(MODE_PARAMETER_BLOCK));

				if(modeC->WriteCacheEnable) {					
					if((s_info->mode == NDAS_DISK_MODE_SINGLE) || (s_info->mode == NDAS_DISK_MODE_ATAPI)){
						udev_t * udev = NULL;
					   	ndas_device_info * d_info = NULL;

						udev = (udev_t *)l_udev->disks;
						d_info = (ndas_device_info *)udev->ndev;	


						if( d_info->version > NDAS_VERSION_1_0 ) {
							ndas_io_request req;
							ndas_io_add_info info;

							sal_memset(&req, 0, sizeof(ndas_io_request));
							sal_memset(&info, 0, sizeof(ndas_io_add_info));
							req.additionalInfo = (void *)&info;

							info.ADD_INFO_SET_FEATURE.sub_com = LSP_IDE_SET_FEATURES_ENABLE_WRITE_CACHE;
							info.ADD_INFO_SET_FEATURE.sub_com_specific_0 = 0;
							info.ADD_INFO_SET_FEATURE.sub_com_specific_1 = 0;
							info.ADD_INFO_SET_FEATURE.sub_com_specific_2 = 0;
							info.ADD_INFO_SET_FEATURE.sub_com_specific_3 = 0;
							
							err = ndas_io_internal_block_op(l_udev, CONNIO_CMD_SET_FEATURE, &req);
							
							if(err == NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED || err == NDAS_ERROR ) {
								debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: fail CONNIO_CMD_SET_FEATURE");
								pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
								return NDAS_OK;
							}

							if(!NDAS_SUCCESS(err)) {
								debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: error CONNIO_CMD_SET_FEATURE");
								pc_req->status = NDAS_IO_STATUS_COMMUNICATION_ERROR;
								return err;
							}

							sal_memset(&req, 0, sizeof(ndas_io_request));
							err = ndas_io_internal_block_op(l_udev, CONNIO_CMD_HANDSHAKE, &req);

							if(err == NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED || err == NDAS_ERROR ) {
								debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: fail CONNIO_CMD_HANDSHAKE");
								pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
								return NDAS_OK;
							}

							if(!NDAS_SUCCESS(err)) {
								debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: error CONNIO_CMD_HANDSHAKE");
								pc_req->status = NDAS_IO_STATUS_COMMUNICATION_ERROR;
								return err;
							}

							hsdata = lsp_get_ata_handshake_data(udev->conn.lsp_handle);							
							if(hsdata->active.write_cache != 1) {
								ndas_pc_io_set_sensedata(pc_req->sensedata, 
									SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);
								pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
								pc_req->avaliableSenseData = 1;
								return NDAS_OK;								
							}
						}else {
							debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: not supported HW version %d", d_info->version);
							pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
							return NDAS_OK;
						}

					}else {
						debug_udev(1, "err: ndas_io_ata_process: SCSIOP_MODE_SELECT: not supported HW type %d", s_info->mode);
						pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
						return NDAS_OK;
					}
				
				}
			}

			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			return NDAS_OK;
		}
		break;
		case SCSIOP_READ6:
		case SCSIOP_READ:
		case SCSIOP_READ12:
		case SCSIOP_READ16:
		{
			xuint64 logicalBlockAddress = 0;
			xuint32 transferblocks = 0;
			xuint64 total_sector = 0;
			PCDB cdb = NULL;


			debug_udev(1, "CALL READ CMD 0x%02x CMDLEN %d", pc_req->cmd[0], pc_req->cmdlen);	
			cdb = (PCDB)pc_req->cmd;
			if(s_info->mode == NDAS_DISK_MODE_SINGLE){
				total_sector = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
			}else {
				total_sector = s_info->sectors;
			}			

			
			if(pc_req->cmd[0] == SCSIOP_READ6) {
				logicalBlockAddress = (xuint64)((xuint64)pc_req->cmd[3] 
									+ ((xuint64)pc_req->cmd[2] << 8) 
									+ (((xuint64)pc_req->cmd[1] & 0x1f) << 16));
				transferblocks =  pc_req->cmd[4];
				debug_udev(3, "CALL READ6 total sector %lldlogicalBlockAddress %lld tansferbloks %d buflen %d", 
													total_sector, logicalBlockAddress, transferblocks, req->reqlen);	
			
			}else if (pc_req->cmd[0] == SCSIOP_READ) {
				
				
				logicalBlockAddress = (xuint64)((xuint64)cdb->CDB10.LogicalBlockByte3 
									+  ((xuint64)cdb->CDB10.LogicalBlockByte2 << 8)
									+ ((xuint64)cdb->CDB10.LogicalBlockByte1 << 16) 
									+ ((xuint64)cdb->CDB10.LogicalBlockByte0 << 24));
				
				transferblocks =(xuint32) ((xuint32)cdb->CDB10.TransferBlocksLsb 
								+ ((xuint32)cdb->CDB10.TransferBlocksMsb << 8));

				debug_udev(3, "CALL READ total sector %lldlogicalBlockAddress %lld tansferbloks %d buflen %d", 
													total_sector, logicalBlockAddress, transferblocks, req->reqlen);
				
			}else if (pc_req->cmd[0] == SCSIOP_READ12){
				
				logicalBlockAddress = (xuint64)((xuint64)cdb->READ12.LogicalBlock[3]
									+ ((xuint64)cdb->READ12.LogicalBlock[2] << 8)
									+ ((xuint64)cdb->READ12.LogicalBlock[1] << 16)
									+ ((xuint64)cdb->READ12.LogicalBlock[0] << 24));
				transferblocks = (xuint32)((xuint32)cdb->READ12.TransferLength[3]
								+ ((xuint32)cdb->READ12.TransferLength[2] << 8)
								+ ((xuint32)cdb->READ12.TransferLength[1] << 16)
								+ ((xuint32)cdb->READ12.TransferLength[0] << 24));
				debug_udev(3, "CALL READ12 total sector %lldlogicalBlockAddress %lld tansferbloks %d buflen %d", 
													total_sector, logicalBlockAddress, transferblocks, req->reqlen);
			}else if (pc_req->cmd[0] == SCSIOP_READ16) {
				logicalBlockAddress = (xuint64)((xuint64)cdb->READ16.LogicalBlock[7]
									+ ((xuint64)cdb->READ16.LogicalBlock[6] << 8)
									+ ((xuint64)cdb->READ16.LogicalBlock[5] << 16)
									+ ((xuint64)cdb->READ16.LogicalBlock[4] << 24)
									+ ((xuint64)cdb->READ16.LogicalBlock[3] << 32)
									+ ((xuint64)cdb->READ16.LogicalBlock[2] << 40)
									+ ((xuint64)cdb->READ16.LogicalBlock[1] << 48)
									+ ((xuint64)cdb->READ16.LogicalBlock[0] << 56)
									);
				transferblocks = (xuint32)((xuint32)cdb->READ16.TransferLength[3]
								+ ((xuint32)cdb->READ16.TransferLength[2] << 8)
								+ ((xuint32)cdb->READ16.TransferLength[1] << 16)
								+ ((xuint32)cdb->READ16.TransferLength[0] << 24));
				debug_udev(3, "CALL READ12 total sector %lld logicalBlockAddress %lld tansferbloks %d buflen %d", 
													total_sector, logicalBlockAddress, transferblocks, req->reqlen);
			}
			

			if(transferblocks == 0) {
				pc_req->status = NDAS_IO_STATUS_SUCCESS;
				debug_udev(3, "trnasferlbock 0");
				return NDAS_OK;
			}	
				
			if( (logicalBlockAddress >= total_sector) 
				|| ((logicalBlockAddress + (xuint64)transferblocks) >= total_sector) ){

				debug_udev(3, "trnasferlbock block out of bound");
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				return NDAS_OK;
			}
				
			req->start_sec = logicalBlockAddress;
			req->num_sec = (xuint16)transferblocks;

			return ndas_io_real_ata_processing(l_udev, CONNIO_CMD_READ, req);
			/* do  disk read  */
		}
		break;
		case SCSIOP_WRITE6:
		case SCSIOP_WRITE:
		case SCSIOP_WRITE12:
		case SCSIOP_WRITE16:
		{
			xuint64 logicalBlockAddress = 0;
			xuint32 transferblocks = 0;
			xuint64 total_sector = 0;
			xbool forceUnitAccess = 0;
			PCDB cdb = NULL;

			cdb = (PCDB)pc_req->cmd;
			if(s_info->mode == NDAS_DISK_MODE_SINGLE){
				total_sector = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
			}else {
				total_sector = s_info->sectors;
			}			

			if(s_info->mode == NDAS_DISK_MODE_SINGLE){
				if(!s_info->writable &&  !s_info->writeshared) {
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}
			}else {
				if(!l_udev->has_key) {
					ndas_pc_io_set_sensedata(pc_req->sensedata, 
						SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
					pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
					pc_req->avaliableSenseData = 1;
					return NDAS_OK;
				}
			}
			
			
			if(pc_req->cmd[0] == SCSIOP_WRITE6) {
				logicalBlockAddress = (xuint64)((xuint64)pc_req->cmd[3] 
									+ ((xuint64)pc_req->cmd[2] << 8) 
									+ (((xuint64)pc_req->cmd[1] & 0x1f) << 16));
				transferblocks =  pc_req->cmd[4];
			
			}else if (pc_req->cmd[0] == SCSIOP_WRITE) {
				
				
				logicalBlockAddress = (xuint64)((xuint64)cdb->CDB10.LogicalBlockByte3 
									+  ((xuint64)cdb->CDB10.LogicalBlockByte2 << 8)
									+ ((xuint64)cdb->CDB10.LogicalBlockByte1 << 16) 
									+ ((xuint64)cdb->CDB10.LogicalBlockByte0 << 24));
				
				transferblocks =(xuint32) ((xuint32)cdb->CDB10.TransferBlocksLsb 
								+ ((xuint32)cdb->CDB10.TransferBlocksMsb << 8));

				forceUnitAccess = cdb->CDB10.ForceUnitAccess;
			}else if (pc_req->cmd[0] == SCSIOP_WRITE12){
				
				logicalBlockAddress = (xuint64)((xuint64)cdb->WRITE12.LogicalBlock[3]
									+ ((xuint64)cdb->WRITE12.LogicalBlock[2] << 8)
									+ ((xuint64)cdb->WRITE12.LogicalBlock[1] << 16)
									+ ((xuint64)cdb->WRITE12.LogicalBlock[0] << 24));
				
				transferblocks = (xuint32)((xuint32)cdb->WRITE12.TransferLength[3]
								+ ((xuint32)cdb->WRITE12.TransferLength[2] << 8)
								+ ((xuint32)cdb->WRITE12.TransferLength[1] << 16)
								+ ((xuint32)cdb->WRITE12.TransferLength[0] << 24));

				forceUnitAccess = cdb->WRITE12.ForceUnitAccess;
			}else if (pc_req->cmd[0] == SCSIOP_WRITE16) {
				logicalBlockAddress = (xuint64)((xuint64)cdb->WRITE16.LogicalBlock[7]
									+ ((xuint64)cdb->WRITE16.LogicalBlock[6] << 8)
									+ ((xuint64)cdb->WRITE16.LogicalBlock[5] << 16)
									+ ((xuint64)cdb->WRITE16.LogicalBlock[4] << 24)
									+ ((xuint64)cdb->WRITE16.LogicalBlock[3] << 32)
									+ ((xuint64)cdb->WRITE16.LogicalBlock[2] << 40)
									+ ((xuint64)cdb->WRITE16.LogicalBlock[1] << 48)
									+ ((xuint64)cdb->WRITE16.LogicalBlock[0] << 56)
									);
				
				transferblocks = (xuint32)((xuint32)cdb->WRITE16.TransferLength[3]
								+ ((xuint32)cdb->WRITE16.TransferLength[2] << 8)
								+ ((xuint32)cdb->WRITE16.TransferLength[1] << 16)
								+ ((xuint32)cdb->WRITE16.TransferLength[0] << 24));

				forceUnitAccess = cdb->WRITE16.ForceUnitAccess;
			}
			

			if(transferblocks == 0) {
				pc_req->status = NDAS_IO_STATUS_SUCCESS;
				return NDAS_OK;
			}
	
				
			if( (logicalBlockAddress >= total_sector) 
				|| ((logicalBlockAddress + (xuint64)transferblocks) >= total_sector) ){
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				return NDAS_OK;
			}
				
			req->start_sec = logicalBlockAddress;
			req->num_sec = transferblocks;
			if(forceUnitAccess) {
				return ndas_io_real_ata_processing(l_udev, CONNIO_CMD_WV, req);
			}else {
				return ndas_io_real_ata_processing(l_udev, CONNIO_CMD_WRITE, req);
			}
			/* do  disk read  */
		}
		break;
		case SCSIOP_VERIFY6:
		case SCSIOP_VERIFY:
		case SCSIOP_VERIFY12:
		case SCSIOP_VERIFY16:
		{
			xuint64 logicalBlockAddress = 0;
			xuint32 transferblocks = 0;
			xuint64 total_sector = 0;
			PCDB cdb = NULL;

			cdb = (PCDB)pc_req->cmd;
			if(s_info->mode == NDAS_DISK_MODE_SINGLE){
				total_sector = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
			}else {
				total_sector = s_info->sectors;
			}			

			
			if(pc_req->cmd[0] == SCSIOP_VERIFY6) {
				logicalBlockAddress = (xuint64)((xuint64)pc_req->cmd[3] 
									+ ((xuint64)pc_req->cmd[2] << 8) 
									+ ((xuint64)(pc_req->cmd[1] & 0x1f) << 16));
				transferblocks =  pc_req->cmd[4];
			
			}else if (pc_req->cmd[0] == SCSIOP_VERIFY) {
				logicalBlockAddress = (xuint64)((xuint64)cdb->CDB10.LogicalBlockByte3 
									+  ((xuint64)cdb->CDB10.LogicalBlockByte2 << 8)
									+ ((xuint64)cdb->CDB10.LogicalBlockByte2 << 16) 
									+ ((xuint64)cdb->CDB10.LogicalBlockByte0 << 24));
				
				transferblocks =(xuint32) ((xuint32)cdb->CDB10.TransferBlocksLsb 
								+ ((xuint32)cdb->CDB10.TransferBlocksMsb << 8));
				
			}else if (pc_req->cmd[0] == SCSIOP_VERIFY12){
				
				logicalBlockAddress = (xuint64)((xuint64)cdb->CDB12.LogicalBlock[3]
									+ ((xuint64)cdb->CDB12.LogicalBlock[2] << 8)
									+ ((xuint64)cdb->CDB12.LogicalBlock[1] << 16)
									+ ((xuint64)cdb->CDB12.LogicalBlock[0] << 24));
				
				transferblocks = (xuint32)((xuint32)cdb->CDB12.TransferLength[3]
								+ ((xuint32)cdb->CDB12.TransferLength[2] << 8)
								+ ((xuint32)cdb->CDB12.TransferLength[1] << 16)
								+ ((xuint32)cdb->CDB12.TransferLength[0] << 24));
			}else if (pc_req->cmd[0] == SCSIOP_VERIFY16) {
				logicalBlockAddress = (xuint64)((xuint64)cdb->VERIFY16.LogicalBlock[7]
									+ ((xuint64)cdb->VERIFY16.LogicalBlock[6] << 8)
									+ ((xuint64)cdb->VERIFY16.LogicalBlock[5] << 16)
									+ ((xuint64)cdb->VERIFY16.LogicalBlock[4] << 24)
									+ ((xuint64)cdb->VERIFY16.LogicalBlock[3] << 32)
									+ ((xuint64)cdb->VERIFY16.LogicalBlock[2] << 40)
									+ ((xuint64)cdb->VERIFY16.LogicalBlock[1] << 48)
									+ ((xuint64)cdb->VERIFY16.LogicalBlock[0] << 56)
									);
				
				transferblocks = (xuint32)((xuint32)cdb->VERIFY16.VerificationLength[3]
								+ ((xuint32)cdb->VERIFY16.VerificationLength[2] << 8)
								+ ((xuint32)cdb->VERIFY16.VerificationLength[1] << 16)
								+ ((xuint32)cdb->VERIFY16.VerificationLength[0] << 24));
			}
			

			if(transferblocks == 0) {
				pc_req->status = NDAS_IO_STATUS_SUCCESS;
				return NDAS_OK;
			}				
				
			if( (logicalBlockAddress >= total_sector) 
				|| ((logicalBlockAddress + (xuint64)transferblocks) >= total_sector) ){
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				return NDAS_OK;
			}
				
			req->start_sec = logicalBlockAddress;
			req->num_sec = transferblocks;	
			return ndas_io_real_ata_processing(l_udev, CONNIO_CMD_VERIFY, req);
			/* do  disk read  */
		}
		break;
		case SCSIOP_SYNCHRONIZE_CACHE:
		case SCSIOP_SYNCHRONIZE_CACHE16:
		{
			xuint64 logicalBlockAddress = 0;
			xuint32 transferblocks = 0;
			xuint64 total_sector = 0;
			PCDB cdb = NULL;

			cdb = (PCDB)pc_req->cmd;
			if(s_info->mode == NDAS_DISK_MODE_SINGLE){
				total_sector = s_info->sectors - NDAS_BLOCK_SIZE_XAREA;
			}else {
				total_sector = s_info->sectors;
			}			

			
			if (pc_req->cmd[0] == SCSIOP_SYNCHRONIZE_CACHE) {
				logicalBlockAddress = (xuint64)((xuint64)cdb->SYNCHRONIZE_CACHE10.LogicalBlockAddress[3] 
									+  ((xuint64)cdb->SYNCHRONIZE_CACHE10.LogicalBlockAddress[2] << 8)
									+ ((xuint64)cdb->SYNCHRONIZE_CACHE10.LogicalBlockAddress[1] << 16) 
									+ ((xuint64)cdb->SYNCHRONIZE_CACHE10.LogicalBlockAddress[0] << 24));
				
				transferblocks =(xuint32) ((xuint32)cdb->SYNCHRONIZE_CACHE10.BlockCount[2] 
								+ ((xuint32)cdb->SYNCHRONIZE_CACHE10.BlockCount[1] << 8));
				
			}else if(pc_req->cmd[0] == SCSIOP_SYNCHRONIZE_CACHE16) {
				logicalBlockAddress = (xuint64)((xuint64)cdb->SYNCHRONIZE_CACHE16.LogicalBlock[7]
									+ ((xuint64)cdb->SYNCHRONIZE_CACHE16.LogicalBlock[6] << 8)
									+ ((xuint64)cdb->SYNCHRONIZE_CACHE16.LogicalBlock[5] << 16)
									+ ((xuint64)cdb->SYNCHRONIZE_CACHE16.LogicalBlock[4] << 24)
									+ ((xuint64)cdb->SYNCHRONIZE_CACHE16.LogicalBlock[3] << 32)
									+ ((xuint64)cdb->SYNCHRONIZE_CACHE16.LogicalBlock[2] << 40)
									+ ((xuint64)cdb->SYNCHRONIZE_CACHE16.LogicalBlock[1] << 48)
									+ ((xuint64)cdb->SYNCHRONIZE_CACHE16.LogicalBlock[0] << 56)
									);
				
				transferblocks = (xuint32)((xuint32)cdb->SYNCHRONIZE_CACHE16.BlockCount[3]
								+ ((xuint32)cdb->SYNCHRONIZE_CACHE16.BlockCount[2] << 8)
								+ ((xuint32)cdb->SYNCHRONIZE_CACHE16.BlockCount[1] << 16)
								+ ((xuint32)cdb->SYNCHRONIZE_CACHE16.BlockCount[0] << 24));
			}
			

			if(transferblocks == 0) {
				pc_req->status = NDAS_IO_STATUS_SUCCESS;
				return NDAS_OK;
			}				
				
			if( (logicalBlockAddress >= total_sector) 
				|| ((logicalBlockAddress + (xuint64)transferblocks) >= total_sector) ){
				pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
				return NDAS_OK;
			}
				
			req->start_sec = logicalBlockAddress;
			req->num_sec = transferblocks;	
			return ndas_io_real_ata_processing(l_udev, CONNIO_CMD_FLUSH, req);
			/* do  disk read  */			
		}
		break;
		case SCSIOP_SEEK:
		{
			pc_req->status = NDAS_IO_STATUS_SUCCESS;
			return NDAS_OK;
		}
		break;
		case SCSIOP_ATA_PASSTHROUGH16:
		case SCSIOP_ATA_PASSTHROUGH12 :
		{
			if (pc_req->direction == DATA_TO_DEV) {
				if(s_info->mode == NDAS_DISK_MODE_SINGLE){
					if(!s_info->writable &&  !s_info->writeshared) {
						ndas_pc_io_set_sensedata(pc_req->sensedata, 
							SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
						pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
						pc_req->avaliableSenseData = 1;
						return NDAS_OK;
					}
				}else {
					if(!l_udev->has_key) {
						ndas_pc_io_set_sensedata(pc_req->sensedata, 
							SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_WRITE_PROTECT, 0);
						pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
						pc_req->avaliableSenseData = 1;
						return NDAS_OK;
					}
				}
			}

			return ndas_io_ata_check_passthrough(l_udev, req);
		}
		break;
		case SCSIOP_REASSIGN_BLOCKS:
		case SCSIOP_REQUEST_SENSE:
		case SCSIOP_FORMAT_UNIT:
		case SCSIOP_MEDIUM_REMOVAL:
		{
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_ILLEGAL_COMMAND, 0);			
			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;
		}
		break;
		default:
		{
			ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_ILLEGAL_COMMAND, 0);	
			pc_req->status = NDAS_IO_STATUS_INVALID_COMMAND;
			pc_req->avaliableSenseData = 1;
			return NDAS_OK;
		}
		break;
	}

	return NDAS_OK;
}


// added by ILGU HONG for scsi support
ndas_error_t ndas_io_pc(int slot,  ndas_io_request* req, xbool bound_check)
{
	logunit_t* log_udev = NULL;
	nads_io_pc_request_ptr pc_req = NULL;
	pc_req = (nads_io_pc_request_ptr)req->additionalInfo;

	
	
	log_udev = sdev_lookup_byslot(slot);
	
	if (log_udev == NULL) {
            	debug_udev(1, "ed invalid log_udev");	
		ndas_pc_io_set_sensedata(pc_req->sensedata, 
			SCSI_SENSE_HARDWARE_ERROR, SCSI_ADSENSE_LUN_COMMUNICATION, SCSI_SENSEQ_COMM_FAILURE);
		pc_req->status = NDAS_IO_STATUS_NOT_EXIST;	
		pc_req->avaliableSenseData = 1;
		return NDAS_OK;
	}
	

	if (!log_udev->info.enabled || !log_udev->accept_io) {
		debug_udev(1, "ed log_udev->info.enabled=%d log_udev->accept_io=%d", log_udev->info.enabled, log_udev->accept_io);
		ndas_pc_io_set_sensedata(pc_req->sensedata, 
			SCSI_SENSE_HARDWARE_ERROR, SCSI_ADSENSE_LUN_COMMUNICATION, SCSI_SENSEQ_COMM_FAILURE);
		pc_req->status = NDAS_IO_STATUS_NOT_EXIST;	
		pc_req->avaliableSenseData = 1;
		return NDAS_OK;
	}
	
		
	if (log_udev->ops->status(log_udev) & (CONN_STATUS_SHUTING_DOWN| CONN_STATUS_INIT)) {
		debug_udev(1, "ed status shutdown fail command 0x%02x", pc_req->cmd[0]);	
		ndas_pc_io_set_sensedata(pc_req->sensedata, 
			SCSI_SENSE_HARDWARE_ERROR, SCSI_ADSENSE_LUN_COMMUNICATION, SCSI_SENSEQ_COMM_FAILURE);
		pc_req->status = NDAS_IO_STATUS_NOT_EXIST;
		pc_req->avaliableSenseData = 1;
		return NDAS_OK;
	}
	

	
	if(log_udev->info.mode == NDAS_DISK_MODE_ATAPI) {
		return ndas_io_atapi_process(log_udev, req);
	}
	
	return ndas_io_ata_process(log_udev, req);
    
}

LOCAL NDAS_CALL 
void ndas_pc_ata_io_process_err(ndas_error_t err, nads_io_pc_request_ptr pc_req)
{

    if ( (pc_req->cmd[0] == SCSIOP_ATA_PASSTHROUGH12)
                || (pc_req->cmd[0] == SCSIOP_ATA_PASSTHROUGH16) )
    {
        if(NDAS_SUCCESS(err)) {

            if(pc_req->avaliableSenseData == 1) {
                pc_req->status = NDAS_IO_STATUS_COMMMAND_DONE_SENSE;
            }else {
                pc_req->status = NDAS_IO_STATUS_SUCCESS;
            }
            
        }else {

            if(pc_req->avaliableSenseData == 1) {
                pc_req->status = NDAS_IO_STATUS_COMMMAND_DONE_SENSE2;
            }else {
                if(err ==NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND
                    || err ==  NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED ) {

                    ndas_pc_io_set_sensedata(pc_req->sensedata, 
                    			SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);			
                    pc_req->avaliableSenseData = 1;	
                    pc_req->status = NDAS_IO_STATUS_COMMAND_FAILED;

                }else {
                    ndas_pc_io_set_sensedata(pc_req->sensedata, 
                    	SCSI_SENSE_NOT_READY, SCSI_ADSENSE_LUN_NOT_READY, 0);
                    pc_req->avaliableSenseData = 1;	
                    pc_req->status = NDAS_IO_STATUS_COMMUNICATION_ERROR;
                }                
            }
        }
    }else {
    
        if(NDAS_SUCCESS(err)){           
            pc_req->status = NDAS_IO_STATUS_SUCCESS;       
        }else {
            debug_udev(1, "ERROR ATA : original error CODE(%d)", err);
            if(err ==NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND
                || err ==  NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED ) {

                ndas_pc_io_set_sensedata(pc_req->sensedata, 
                			SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ADSENSE_INVALID_CDB, 0);			
                pc_req->avaliableSenseData = 1;	
                pc_req->status = NDAS_IO_STATUS_COMMAND_FAILED;

            }else {
                ndas_pc_io_set_sensedata(pc_req->sensedata, 
                	SCSI_SENSE_NOT_READY, SCSI_ADSENSE_LUN_NOT_READY, 0);
                pc_req->avaliableSenseData = 1;	
                pc_req->status = NDAS_IO_STATUS_COMMUNICATION_ERROR;
            }
        }
        
    }
}


LOCAL NDAS_CALL 
void ndas_pc_atapi_io_process_err(ndas_error_t err, nads_io_pc_request_ptr pc_req)
{
	if(NDAS_SUCCESS(err)){
		pc_req->status = NDAS_IO_STATUS_SUCCESS;
	}else {
		
		switch(err){
			case NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND:
			case NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED:
			case NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION:
			case NDAS_ERROR_IDE_REMOTE_AUTH_FAILED :
			case NDAS_ERROR_HARDWARE_DEFECT:
			case NDAS_ERROR_IDE_TARGET_BROKEN_DATA :
			case NDAS_ERROR_BAD_SECTOR:
			case NDAS_ERROR_IDE_VENDOR_SPECIFIC :
				{
#ifdef DEBUG
					PSENSE_DATA SenseBuffer = (PSENSE_DATA)pc_req->sensedata;
#endif
					debug_udev(1, "ERROR ATAPI : original error ErrCode[0x%02x] ASC[0x%02x] ASCQ[0x%02x]",
						SenseBuffer->ErrorCode, 
						SenseBuffer->AdditionalSenseCode, 
						SenseBuffer->AdditionalSenseCodeQualifier);
					
					pc_req->status = NDAS_IO_STATUS_COMMAND_FAILED;
				}
				break;
			default:
				debug_udev(1, "INVALID ERROR ATAPI : original error CODE(%d)", err);
				ndas_pc_io_set_sensedata(pc_req->sensedata, 
					SCSI_SENSE_NOT_READY, SCSI_ADSENSE_LUN_NOT_READY, 0);
				pc_req->avaliableSenseData = 1;	
				pc_req->status = NDAS_IO_STATUS_COMMUNICATION_ERROR;
				break;
				
		}
		
	}
}


// added by ILGU HONG for scsi support end
#ifdef XPLAT_ASYNC_IO

struct ndas_io_blocking_done_arg {
    sal_semaphore done_sema;
    ndas_error_t err;
};

/* This function is used for asynchronous IO ndas_io called without done handler */
LOCAL NDAS_CALL void ndas_io_blocking_done(int slot, ndas_error_t err, void* arg)
{
    struct ndas_io_blocking_done_arg* darg = (struct ndas_io_blocking_done_arg*)arg;
    darg->err = err;
    sal_semaphore_give((sal_semaphore)darg->done_sema);
}


LOCAL NDAS_CALL void ndas_pc_ata_io_accept_done(int slot, ndas_error_t err, void* arg)
{
	ndas_io_request *  req = (ndas_io_request *)arg;
	nads_io_pc_request * pc_req = (nads_io_pc_request *)req->additionalInfo;
	
	ndas_pc_ata_io_process_err(err, pc_req);	
	req->expand_done(slot, NDAS_OK, req->expand_done_arg);
	return;
}

LOCAL NDAS_CALL void ndas_pc_atapi_io_accept_done(int slot, ndas_error_t err, void* arg)
{
	ndas_io_request *  req = (ndas_io_request *)arg;
	nads_io_pc_request * pc_req = (nads_io_pc_request *)req->additionalInfo;
	
	ndas_pc_atapi_io_process_err(err, pc_req);	
	req->expand_done(slot, NDAS_OK, req->expand_done_arg);
	return;
}




/* if bound_check is TRUE, accessing out of disk range is prohibited */
ndas_error_t ndas_io(int slot, xint8 cmd, ndas_io_request* req, xbool bound_check)
{
    xuint64 total_sector;
    xuint64 reqed_sector;
    xbool do_sync_io;
    struct udev_request_block *tir;
    struct ndas_io_blocking_done_arg darg;
    logunit_t* log_udev = sdev_lookup_byslot(slot);

    if (log_udev == NULL) {
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

    if (!log_udev->info.enabled || !log_udev->accept_io) {
        debug_udev(1, "ed log_udev->info.enabled=%d log_udev->accept_io=%d", log_udev->info.enabled, log_udev->accept_io);
        return NDAS_ERROR_SHUTDOWN;
    }
    if (cmd == ND_CMD_WRITE 
		&& !log_udev->info.writable
		&& !log_udev->info.writeshared)
        return NDAS_ERROR_READONLY;
    if (log_udev->ops->status(log_udev) & (CONN_STATUS_SHUTING_DOWN| CONN_STATUS_INIT))
        return NDAS_ERROR_SHUTDOWN;

    if (bound_check) {
        total_sector = log_udev->info.sectors;
        reqed_sector = req->start_sec;
        if (reqed_sector >= total_sector || reqed_sector + req->num_sec > total_sector) {
            debug_udev(1, "reqed_sector=%llu total_sector=%llu req->num_sec=%u", 
                reqed_sector, total_sector, req->num_sec);
            sal_assert(reqed_sector + req->num_sec <= total_sector);
            return NDAS_ERROR_INVALID_RANGE_REQUEST;
        }
    }

    debug_udev(8, "ing start sector %llu, count %d", req->start_sec, req->num_sec);
    debug_udev(8, "req->done_arg=%p", req->done_arg);

    tir = tir_alloc(cmd, req);
    if ( !tir ) return NDAS_ERROR_OUT_OF_MEMORY;

    if (tir->req->done==NULL) {
        debug_udev(1, "darg %p", &darg);
        do_sync_io = TRUE;
        tir->req->done = ndas_io_blocking_done;
        tir->req->done_arg = &darg;
        darg.done_sema = sal_semaphore_create("ndas_io_done", 1, 0);
    } else {
        do_sync_io = FALSE;
    }

    log_udev->ops->io(log_udev, tir, FALSE); // uop_io, r0op_io, r1op_io

    // if no done-handler is provided, wait for completion
    if (do_sync_io) {
        int err;
        do {
            err = sal_semaphore_take(darg.done_sema, SAL_SYNC_FOREVER);
        } while (err == SAL_SYNC_INTERRUPTED);
        sal_semaphore_give(darg.done_sema);
        sal_semaphore_destroy(darg.done_sema);
        debug_udev(5, "ed err=%d", darg.err);
        return darg.err;
    }
    debug_udev(5, "ed");
    return NDAS_OK;
}

LOCAL NDAS_CALL
ndas_error_t
ndas_io_internal_block_op(logunit_t* log_udev, xint8 cmd, ndas_io_request * req)
{
	struct udev_request_block *tir = NULL;
	struct ndas_io_blocking_done_arg darg;
	int err;

	if (req->done) {
		return NDAS_ERROR;	
	}

	tir = tir_alloc(cmd, req);
	if ( !tir ) return NDAS_ERROR;

	tir->req->done = ndas_io_blocking_done;
	tir->req->done_arg = &darg;
	darg.done_sema = sal_semaphore_create("scsi_i_done", 1, 0);

	log_udev->ops->io(log_udev, tir, FALSE); // uop_io, r0op_io, r1op_io


	    
	do {
		err = sal_semaphore_take(darg.done_sema, SAL_SYNC_FOREVER);
	} while (err == SAL_SYNC_INTERRUPTED);
	sal_semaphore_give(darg.done_sema);
	sal_semaphore_destroy(darg.done_sema);
	debug_udev(5, "ed err=%d", darg.err);	
	return darg.err;	
	
}

LOCAL NDAS_CALL
ndas_error_t ndas_io_real_ata_processing(logunit_t* log_udev, xint8 cmd, ndas_io_request * req)
{
	struct udev_request_block *tir = NULL;
	struct ndas_io_blocking_done_arg darg;
	xbool do_sync_io = FALSE;

	debug_udev(3, "ndas_io_real_ata_processing");	

	if(req->done) {
		debug_udev(3, "req->doen 0x%p done_arg 0x%p", req->done, req->done_arg);
		req->expand_done = req->done;
		req->expand_done_arg = req->done_arg;
		req->done = ndas_pc_ata_io_accept_done;
		req->done_arg = (void *)req;
	}

	tir = tir_alloc(cmd, req);
	if ( !tir ) {
		debug_udev(3, "err out of memory ");	
		return NDAS_ERROR_OUT_OF_MEMORY;
	}



	if (tir->req->done==NULL) {
	    debug_udev(1, "darg %p", &darg);
	    do_sync_io = TRUE;
	    tir->req->done = ndas_io_blocking_done;
	    tir->req->done_arg = &darg;
	    darg.done_sema = sal_semaphore_create("scsi_io_done", 1, 0);
	} else {	
	    do_sync_io = FALSE;
	    debug_udev(3, "do async io");
	}

	log_udev->ops->io(log_udev, tir, FALSE); // uop_io, r0op_io, r1op_io

	// if no done-handler is provided, wait for completion
	if (do_sync_io) {
	    int err;
	    do {
	        err = sal_semaphore_take(darg.done_sema, SAL_SYNC_FOREVER);
	    } while (err == SAL_SYNC_INTERRUPTED);
	    sal_semaphore_give(darg.done_sema);
	    sal_semaphore_destroy(darg.done_sema);
	    debug_udev(5, "ed err=%d", darg.err);
		ndas_pc_ata_io_process_err(darg.err, (nads_io_pc_request_ptr)req->additionalInfo);	
	    return NDAS_OK;
	}
	debug_udev(5, "ed");
	return NDAS_MORE_PROCESSING_RQ;	
}

LOCAL NDAS_CALL
ndas_error_t ndas_io_real_atapi_processing(logunit_t* log_udev, xint8 cmd, ndas_io_request * req)
{
	struct udev_request_block *tir = NULL;
	struct ndas_io_blocking_done_arg darg;
	xbool do_sync_io = FALSE;

	if(req->done) {
		req->expand_done = req->done;
		req->expand_done_arg = req->done_arg;
		req->done = ndas_pc_atapi_io_accept_done;
		req->done_arg = (void *)req;
	}

	tir = tir_alloc(cmd, req);
	if ( !tir ) return NDAS_ERROR_OUT_OF_MEMORY;


	if (tir->req->done==NULL) {
	    debug_udev(1, "darg %p", &darg);
	    do_sync_io = TRUE;
	    tir->req->done = ndas_io_blocking_done;
	    tir->req->done_arg = &darg;
	    darg.done_sema = sal_semaphore_create("ndas_io_done", 1, 0);
	} else {	
	    do_sync_io = FALSE;
	}

	log_udev->ops->io(log_udev, tir, FALSE); // uop_io, r0op_io, r1op_io

	// if no done-handler is provided, wait for completion
	if (do_sync_io) {
	    int err;
	    do {
	        err = sal_semaphore_take(darg.done_sema, SAL_SYNC_FOREVER);
	    } while (err == SAL_SYNC_INTERRUPTED);
	    sal_semaphore_give(darg.done_sema);
	    sal_semaphore_destroy(darg.done_sema);
	    debug_udev(5, "ed err=%d", darg.err);
		ndas_pc_ata_io_process_err(darg.err, (nads_io_pc_request_ptr)req->additionalInfo);		
	    return NDAS_OK;
	}
	debug_udev(5, "ed");
	return NDAS_MORE_PROCESSING_RQ;	
}

#else 
/* !XPLAT_ASYNC_IO */

/* if bound_check is TRUE, accessing out of disk range is prohibited */
ndas_error_t ndas_io(int slot, xint8 cmd, ndas_io_request* req, xbool bound_check)
{
    xuint64 total_sector;
    xuint64 reqed_sector;
    logunit_t *log_udev = sdev_lookup_byslot(slot);
    ndas_error_t err = NDAS_OK;

//    debug_udev(7, "log_udev=%s", DEBUG_SLOT_DEVICE(log_udev));

    if (req->done) {
        debug_udev(1, "This binary is not compiled for asynchronous IO");
        return NDAS_ERROR_UNSUPPORTED_FEATURE;
    }
    if ( log_udev == NULL ) {
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }
    
    if ( !log_udev->info.enabled )
        return NDAS_ERROR_SHUTDOWN;

    if ( log_udev->ops->status(log_udev) & (CONN_STATUS_SHUTING_DOWN|CONN_STATUS_INIT) )
        return NDAS_ERROR_SHUTDOWN;
    
    if ( cmd == ND_CMD_WRITE && !log_udev->info.writable)
        return NDAS_ERROR_READONLY;

    if (bound_check) {
        total_sector = log_udev->info.sectors; 
        sal_assert(total_sector!=0);
        reqed_sector = req->start_sec;
        if (reqed_sector >= total_sector ||(reqed_sector + req->num_sec >= total_sector) ) {
            debug_udev(1, "ed out-of-range access: total_sector=%08x:%08x, requested=%08x:%08x, num_sec=%x",
                (xuint32)(total_sector>>32), (xuint32)(total_sector&0x0ffffffff),
                (xuint32)(reqed_sector>>32),(xuint32)(reqed_sector&0x0ffffffff),
                (xuint32)req->num_sec);
            sal_assert(0);
            return NDAS_ERROR_INVALID_RANGE_REQUEST;
        }
    }
    debug_udev(4, "ing start sector %ld, count %d", (long)req->start_sec, req->num_sec);
    debug_udev(8, "req->done_arg=%p", req->done_arg);

//    debug_udev(1, "taking ndas-io %p %p", SDEV2UDEV(log_udev), SDEV2UDEV(log_udev)->io_lock);
    sal_semaphore_take(SDEV2UDEV(log_udev)->io_lock, SAL_SYNC_FOREVER);
//    debug_udev(1, "took ndas-io %p %p", SDEV2UDEV(log_udev), SDEV2UDEV(log_udev)->io_lock);

	switch(cmd) {
		case CONNIO_CMD_READ:
		case CONNIO_CMD_WRITE:
		{
			err = conn_do_ata_rw(&SDEV2UDEV(log_udev)->conn, cmd, req);	
		}
		break;
		case CONNIO_CMD_FLUSH:
		{
			err = conn_do_ata_flush(&SDEV2UDEV(log_udev)->conn, req);
		}
		break;
		case CONNIO_CMD_VERIFY:
		{
			err = conn_do_ata_verify(&SDEV2UDEV(log_udev)->conn, req);
		}
		break;
		default:
		{
			sal_semaphore_give(SDEV2UDEV(log_udev)->io_lock);
			return NDAS_ERROR_INVALID_OPERATION;
		}
		break;
    }

    if ( !NDAS_SUCCESS(err)) {
        conn_set_status(&SDEV2UDEV(log_udev)->conn, CONN_STATUS_SHUTING_DOWN);
        conn_set_error(&SDEV2UDEV(log_udev)->conn, err);
    }


    if (!NDAS_SUCCESS(err)) {
        sal_debug_print("ndas_io failed %d\n", err);
        udev_shutdown(SDEV2UDEV(log_udev));
    }

//    debug_udev(1, "giving ndas-io %p %p", SDEV2UDEV(log_udev), SDEV2UDEV(log_udev)->io_lock);
    sal_semaphore_give(SDEV2UDEV(log_udev)->io_lock);

    debug_udev(5, "ed");
    return err;
}

LOCAL NDAS_CALL
ndas_error_t
ndas_io_internal_block_op(logunit_t* log_udev, xint8 cmd, ndas_io_request * req)
{
	ndas_error_t err = NDAS_OK;

	if (req->done) {
		return NDAS_ERROR;	
	}
	
	sal_semaphore_take(SDEV2UDEV(log_udev)->io_lock, SAL_SYNC_FOREVER);
	switch(cmd) {
		case CONNIO_CMD_SET_FEATURE:
		{
			err = conn_do_ata_set_feature(&SDEV2UDEV(log_udev)->conn, req);
		}
		break;
		case CONNIO_CMD_HANDSHAKE:
			err = conn_handshake(&SDEV2UDEV(log_udev)->conn);
		break;
		default:
			err = NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED;
	}
	sal_semaphore_give(SDEV2UDEV(log_udev)->io_lock);
}


LOCAL NDAS_CALL
ndas_error_t ndas_io_real_ata_processing(logunit_t* log_udev, xint8 cmd, ndas_io_request * req)
{
 	ndas_error_t err = NDAS_OK;
 
	sal_semaphore_take(SDEV2UDEV(log_udev)->io_lock, SAL_SYNC_FOREVER);


	switch(cmd) {
		case CONNIO_CMD_READ:
		case CONNIO_CMD_WRITE:
		{
			err = conn_do_ata_rw(&SDEV2UDEV(log_udev)->conn, cmd, req);
			
		}
		break;
		case CONNIO_CMD_FLUSH:
		{
			err = conn_do_ata_flush(&SDEV2UDEV(log_udev)->conn, req);
		}
		break;
		case CONNIO_CMD_VERIFY:
		{
			err = conn_do_ata_verify(&SDEV2UDEV(log_udev)->conn, req);
		}
		break;
		case CONNIO_CMD_WV:
		{
			err = conn_do_ata_write_verify(&SDEV2UDEV(log_udev)->conn, req);
		}
		break;
		case CONNIO_CMD_PASSTHOUGH:
            	{
                    	err = conn_do_ata_passthrough(&SDEV2UDEV(log_udev)->conn, req);
            	}	
		default:
		{
			err = NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND;
			ndas_pc_ata_io_process_err(err, (nads_io_pc_request_ptr)req->additionalInfo);	
		}
		break;
	}


	ndas_pc_ata_io_process_err(err, (nads_io_pc_request_ptr)req->additionalInfo);		
	if ( !NDAS_SUCCESS(err)) {

		if(err !=NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND
				&&  err !=  NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED ) {
			conn_set_status(&SDEV2UDEV(log_udev)->conn, CONN_STATUS_SHUTING_DOWN);
			conn_set_error(&SDEV2UDEV(log_udev)->conn, err);	
			sal_debug_print("ndas_io failed %d\n", err);
			udev_shutdown(SDEV2UDEV(log_udev));

		}		
	}

	//    debug_udev(1, "giving ndas-io %p %p", SDEV2UDEV(log_udev), SDEV2UDEV(log_udev)->io_lock);
	sal_semaphore_give(SDEV2UDEV(log_udev)->io_lock);

	return err;
}

LOCAL NDAS_CALL
ndas_error_t ndas_io_real_atapi_processing(logunit_t* log_udev, xint8 cmd, ndas_io_request * req)
{
 	ndas_error_t err = NDAS_OK;	
	sal_semaphore_take(SDEV2UDEV(log_udev)->io_lock, SAL_SYNC_FOREVER);

	err = conn_do_atapi_cmd(&SDEV2UDEV(log_udev)->conn, req);

	ndas_pc_ata_io_process_err(err, (nads_io_pc_request_ptr)req->additionalInfo);		
	if ( !NDAS_SUCCESS(err)) {

		if(err !=NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND
				&&  err !=  NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED ) {
			conn_set_status(&SDEV2UDEV(log_udev)->conn, CONN_STATUS_SHUTING_DOWN);
			conn_set_error(&SDEV2UDEV(log_udev)->conn, err);	
			sal_debug_print("ndas_io failed %d\n", err);
			udev_shutdown(SDEV2UDEV(log_udev));

		}		
	}

	//    debug_udev(1, "giving ndas-io %p %p", SDEV2UDEV(log_udev), SDEV2UDEV(log_udev)->io_lock);
	sal_semaphore_give(SDEV2UDEV(log_udev)->io_lock);

	return err;
}
#endif



NDASUSER_API ndas_error_t ndas_set_encryption_mode(int slot, 
    xbool headenc, xbool dataenc) 
{
    // Temporarily disabled.
    return NDAS_OK;
#if 0
    ndas_error_t err;
    logunit_t *log_udev  = sdev_lookup_byslot(slot);
    udev_t *udev = NULL;
    char param[8] = {0};
    uconn_t *conn;
    sal_msec timeout = NDAS_SET_ENC_TIMEOUT;
    lsp_hardware_data_t* hwdata;
    debug_udev(1, "ed");

    if ( log_udev ) {
        udev = SDEV2UDEV(log_udev);
    }
    
    if ( log_udev == NULL || udev == NULL) {
        debug_udev(1, "invalid slot=%d", slot);
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

    conn = sal_malloc(sizeof(uconn_t));
    if ( !conn ) {
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    sal_memset(conn, 0, sizeof(uconn_t));
    
    err = conn_init(conn, udev->ndev->network_id, udev->info->unit);

    if(!NDAS_SUCCESS(err)) {
        debug_udev(4, "can't init connection object:err=%d",err);
        goto err_out;
    }
    sal_memcpy(&conn->info, &udev->conn.info, sizeof(lanscsi_path));

    err = conn_connect(conn, 0, 0, timeout);

    if ( !NDAS_SUCCESS(err) ) {
        debug_udev(1, "fail to connnect");
        goto err_out;
    }

    hwdata = lsp_get_hardware_data(conn->lsp_handle);

    if (headenc == hwdata->header_encryption_algorithm &&
        dataenc == hwdata->data_encryption_algorithm) {
        
        debug_udev(1, "No encryption mode change required");
        err= NDAS_OK;
        goto err_out;
    }

    if (conn->info.HWVersion == LANSCSI_VERSION_1_0) {
        debug_udev(1, "Unsupported hardware version");
        err = NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION;
        goto err_out;
    }
    conn_logout(conn);
    conn_disconnect(conn);

    debug_udev(1, "Updating encryption option to (%d, %d)", headenc, dataenc);

    conn->info.use_given_pw = TRUE; // passwd may be set by previous connection.Use the password.

    err = conn_connect(conn, 0, CONN_OPT_SUPER_MODE, timeout);

    if (!NDAS_SUCCESS(err)) {
        debug_udev(1, "Login as super user failed: %d", err);
        goto err_out;
    }

    param[7] = (headenc<<1) + dataenc;
    
    err = NdasVendorCommand(conn->sock, &conn->info, VENDOR_OP_SET_ENC_OPT, param);
    debug_udev(1, "NdasVendorCommand returned %d", err);    

    err = NdasVendorCommand(conn->sock, &conn->info, VENDOR_OP_RESET, param);
    debug_udev(1, "NdasVendorCommand returned %d", err);    
err_out:
    conn_logout(conn);
    conn_disconnect(conn);
    conn_clean(conn);
    sal_free(conn);
    return err;
#endif
}

#ifdef XPLAT_RAID
LOCAL 
xbool 
rmd_is_valid(NDAS_RAID_META_DATA *rmd) 
{
#ifdef DEBUG
    debug_udev(7, "Signature=%Lx", rmd->u.s.magic);
    debug_udev(7, "Signature=%Lx", RMD_MAGIC);
    debug_udev(7, "crc32=%x", rmd->crc32);
    debug_udev(7, "crc32 calced=%x", crc32_calc((unsigned char *)rmd, sizeof(rmd->u.bytes_248)) );
    debug_udev(7, "crc32=%x", rmd->crc32_unitdisks);
    debug_udev(7, "crc32 calced=%x", crc32_calc((unsigned char *)rmd->UnitMetaData, sizeof(rmd->UnitMetaData)) );
#endif
    return 
        rmd->crc32 == crc32_calc((unsigned char *)rmd, sizeof(rmd->u.bytes_248))
        && rmd->crc32_unitdisks == crc32_calc((unsigned char *)rmd->UnitMetaData, sizeof(rmd->UnitMetaData))
        && rmd->u.s.magic == RMD_MAGIC;
}
ndas_error_t
udev_read_rmd(udev_t *udev, NDAS_RAID_META_DATA *rmd)
{
    ndas_error_t err;
    xbool rmd_2nd_valid = TRUE, rmd_1st_valid = TRUE;
//    ndas_io_request ioreq = NDAS_IO_REQUEST_INITIALIZER;
    NDAS_RAID_META_DATA *rmd_2nd;
    uconn_t conn = {0};

    err = conn_init(&conn, udev->ndev->network_id, udev->unit, NDAS_CONN_MODE_READ_ONLY, NULL, NULL);

    if(!NDAS_SUCCESS(err)) {
        debug_udev(4, "can't init connection:err=%d",err);
        return err;
    }
    sal_memcpy(&conn.info, &udev->conn.info, sizeof(lanscsi_path));

    err = conn_connect(&conn, FALSE, 0, udev->info->timeout);
    if(!NDAS_SUCCESS(err)) {
        debug_udev(4, "can't connect:err=%d",err);
        goto out1;
    }
    
    rmd_2nd = sal_malloc(sizeof(NDAS_RAID_META_DATA));
    if ( !rmd_2nd ) {
        err = NDAS_ERROR_OUT_OF_MEMORY;
        goto out2;
    }
    
    err = conn_rw(&conn, ND_CMD_READ,
            udev->info->sectors + NDAS_BLOCK_LOCATION_2ND_RMD, 1, (char*) rmd_2nd);

    if ( !NDAS_SUCCESS(err) ) 
        goto out3;

    if( !rmd_is_valid(rmd_2nd) ) {
        rmd_2nd_valid = FALSE;
    }

    err = conn_rw(&conn, ND_CMD_READ,
            udev->info->sectors + NDAS_BLOCK_LOCATION_1ST_RMD, 1, (char*) rmd);

    if ( !NDAS_SUCCESS(err) ) 
        goto out3;

    debug_udev(4, "Signature=%Lx", rmd->u.s.magic);
    debug_udev(4, "USN=%u", rmd->u.s.usn);
    debug_udev(4, "state=%d", rmd->u.s.state);
    {
        int i;
        for ( i = 0; i < 3; i++) {
            debug_udev(4, "[%d]Status=%d", i, rmd->UnitMetaData[i].UnitDeviceStatus);
        }
    }
    if( !rmd_is_valid(rmd) ) 
    {
        rmd_1st_valid = FALSE;
    }
    
    if ( rmd_2nd_valid ) {
        if ( rmd_1st_valid || rmd->u.s.usn != rmd_2nd->u.s.usn )
            sal_memcpy(rmd,rmd_2nd, sizeof(NDAS_RAID_META_DATA));
    } else if ( !rmd_1st_valid ) {
        err = NDAS_ERROR_INVALID_METADATA; // todo
        goto out3;
    }
    err = NDAS_OK;
    goto out2;
out3:
    rmd->u.s.magic = 0;
    rmd->u.s.usn = 0;
    rmd->u.s.state = RMD_STATE_MOUNTED;
    sal_free(rmd_2nd);
out2:    
    conn_logout(&conn);
    conn_disconnect(&conn);
out1:
    conn_clean(&conn);
    return err;
}

void
udev_queue_write_rmd(udev_t *udev,int sector_offset,
              NDAS_RAID_META_DATA *rmd)
{
    urb_ptr tir;
    
    debug_udev(3, "ing slot=%d rmd=%p", udev->info->slot, rmd);
    sal_assert( sector_offset == 0 || sector_offset == 1);

    tir = tir_alloc(ND_CMD_WRITE, NULL);
    if ( !tir ) {
        return;
    }
    tir->req->buf = (char*) rmd;
    tir->req->start_sec = udev->info->sectors/* + NDAS_BLOCK_SIZE_XAREA*/ + NDAS_BLOCK_LOCATION_1ST_RMD + sector_offset;
    tir->req->num_sec = 1;

    rmd->crc32 = crc32_calc((unsigned char *)rmd, sizeof(rmd->u.bytes_248));
    
    rmd->crc32_unitdisks == crc32_calc((unsigned char *)rmd->UnitMetaData, sizeof(rmd->UnitMetaData));

    debug_udev(4, "Signature=%Lx", rmd->u.s.magic);
    debug_udev(4, "USN=%u", rmd->u.s.usn);
    debug_udev(4, "state=%d", rmd->u.s.state);
    debug_udev(4, "crc32=%x", rmd->crc32);
    debug_udev(1, "crc32_unitdisks=%x", rmd->crc32_unitdisks);
    {
        int i;
        for ( i = 0; i < 3; i++) {
            debug_udev(4, "[%d]Status=%d", i, rmd->UnitMetaData[i].UnitDeviceStatus);
        }
    }
    udev_queue_request(udev, tir, FALSE);
}

#endif

#else /* #ifndef NDAS_NO_LANSCSI */

NDASUSER_API ndas_error_t ndas_set_encryption_mode(int slot, 
    xbool headenc, xbool dataenc) 
{
	return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
}

#endif /*#ifdef NDAS_NO_LANSCSI */

