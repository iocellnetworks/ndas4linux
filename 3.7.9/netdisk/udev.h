#ifndef _NETDISK_UDEV_H_
#define _NETDISK_UDEV_H_

#include <sal/types.h>
#include <sal/thread.h>
#include <ndasuser/ndaserr.h>
#include "netdisk/ndasdib.h"
#include "netdisk/conn.h"
#include "binaryparameters.h" // FIRST_TARGET_RW_USER
#include "xlib/dpc.h"
#include "lockmgmt.h"

#ifdef XPLAT_RAID
#include "raid/bitmap.h"
#endif

extern struct ops_s udev_ops;

struct _ndev;

#ifdef DEBUG
#define UDEV_MAGIC 0x11ff5544
#endif 

struct ndiod {
    sal_thread_id    tid;    /* thread ID of ndiod */
    sal_event    idle;    /* wake up idle process to work */
};

/*
    Represent each unit device(single disk, CD drive).
*/
typedef struct _udev_t {
#ifdef DEBUG    
    int                 magic;
#endif

    ndas_slot_info_t*       info;     /* pointing to the _logdev.info */
    ndas_unit_info_t*      uinfo;

#ifdef NDAS_MSHARE
	struct nd_diskmgr_struct  * Disk_info;
#endif

    /* the network connect to the NDAS unit device */
    uconn_t      conn;
    int unit;
    /* NDAS device to which this NDAS unit device is attached */
    struct _ndev*            ndev; 

#ifdef XPLAT_ASYNC_IO
    struct ndiod        thread;
    /* lock for request head */
    sal_spinlock    lock;
    /* udev_request_block queue head */
    struct list_head    request_head;
#else
    sal_semaphore  io_lock;
#endif

#ifdef XPLAT_NDASHIX
    /* used for hix surrender request semaphore */
    sal_semaphore       hix_sema;
    void                *hix_result;
#endif
    void                *arg;

    NDAS_DEVLOCK_INFO   lock_info;
    BUFFLOCK_CONTROL BuffLockCtl;
}udev_t;

#ifdef DEBUG
#define TIR_MAGIC 0x0621222f
#define TIR_ASSERT(tir) sal_assert((tir)->magic == TIR_MAGIC)
#else
#define TIR_ASSERT(tir) do {} while(0)
#endif

typedef struct udev_request_block *urb_ptr;

typedef ndas_error_t (*tir_func)(udev_t *udev, urb_ptr);

#define ND_CMD_READ              CONNIO_CMD_READ
#define ND_CMD_WRITE             CONNIO_CMD_WRITE
#define ND_CMD_FLUSH             CONNIO_CMD_FLUSH
#define ND_CMD_VERIFY		 CONNIO_CMD_VERIFY
#define ND_CMD_WV			CONNIO_CMD_WV
#define ND_CMD_PACKET		CONNIO_CMD_PACKET
#define ND_CMD_SET_FEATURE             CONNIO_CMD_SET_FEATURE
#define ND_CMD_HANDSHAKE             CONNIO_CMD_HANDSHAKE
#define ND_CMD_PASSTHROUGH		CONNIO_CMD_PASSTHOUGH



#define ND_CMD_CONNECT           16
#define ND_CMD_DISCONNECT        17
#define ND_CMD_LOCKOP            18
#define ND_CMD_IO_IDLE           19    /* Used to notify IO idle event */

/* 
    This structure is used to request command to unit device.
    This command will be run in the context of unit device thread in ASYNC_IO mode.
*/
struct udev_request_block {
#ifdef DEBUG
    xint32                     magic;
#endif
    struct     list_head         queue;
    xint8                      cmd; /* ND_CMD_xxx */
    tir_func                func;       /* To do: remove this. cmd is matched to operation, this function is not required */
    void                    *arg; /* private argument */
    ndas_io_request         *req;
    ndas_io_request         int_req;
};

void udev_set_max_xfer_unit(int io_buffer_size);

ndas_error_t udev_write_dib(udev_t *udev,NDAS_DIB_V2 *dib);

ndas_error_t 
udev_io_idle_operation(udev_t *udev);

ndas_error_t 
udev_lock_operation(udev_t *udev, NDAS_LOCK_OP op, int lockid, void* lockdata, void* opt);

#if 0
ndas_error_t udev_query_unit(udev_t *udev, ndas_metadata_t* nmd);
#endif

void udev_shutdown(udev_t *udev);

#ifdef XPLAT_RAID

struct udev_request_block *
tir_clone(struct udev_request_block *tir);

#endif

struct udev_request_block *
tir_alloc(xint8 cmd, ndas_io_request_ptr req);


void tir_free(struct udev_request_block *tir);

#ifdef XPLAT_RAID
/* To do: change conn to uconn_s and move to conn.c/h */
ndas_error_t
udev_read_rmd(udev_t *conn,
              NDAS_RAID_META_DATA *rmd);
void
udev_queue_write_rmd(udev_t *udev,
            int  sector_offset, 
              NDAS_RAID_META_DATA *rmd);
#endif 


#endif
