#ifndef _NETDISK_CONN_H_
#define _NETDISK_CONN_H_

#include <sal/types.h>
#include <sal/time.h>
#include <lpx/lpxutil.h>
#include "ndasuser/ndasuser.h"
#include "netdisk/ndasdib.h"
#include <lspx/lsp_type.h>
#include <lspx/lsp.h>
#ifdef XPLAT_RAID
#include "raid/bitmap.h"
#endif

#define NDAS_MAX_CONNECTION_COUNT    64

#define CONN_DEFAULT_IO_TIMEOUT (30 * 1000) /* 30 sec */

/* status codes for uconn_s.err */
#define CONN_STATUS_INIT            0x0001
#define CONN_STATUS_CONNECTING            0x0010
#define CONN_STATUS_CONNECTED            0x0020
#define CONN_STATUS_SHUTING_DOWN        0x1000    

typedef enum _ndas_conn_mode {
    NDAS_CONN_MODE_READ_ONLY = 0x0001,
    NDAS_CONN_MODE_EXCLUSIVE_WRITE = 0x0002,
    NDAS_CONN_MODE_LOCKED = 0x1000,
    NDAS_CONN_MODE_LOCKED_READ = NDAS_CONN_MODE_LOCKED | NDAS_CONN_MODE_READ_ONLY, /* Use lock for increase aggregated performance in multiple connection case */
    NDAS_CONN_MODE_WRITE_SHARE = NDAS_CONN_MODE_LOCKED | NDAS_CONN_MODE_EXCLUSIVE_WRITE, /* Log in write-share mode(actually read-only connection for ~2.0) and use lock */
    NDAS_CONN_MODE_DISCOVER = 0x2000, /* No IO. Log in for status checking/discovery */
    NDAS_CONN_MODE_SUPERVISOR = 0x4000
} ndas_conn_mode_t;

typedef xuint64 TEXT_TARGET_DATA, *PTEXT_TARGET_DATA;

struct _BUFFLOCK_CONTROL;
struct _NDAS_DEVLOCK_INFO;


#define CONN_IO_MAX_SEGMENT 32

/* NDAS connection to the NDAS unit device 
 * Please note that there can exist two or more connection to one NDAS device in the same time.
 */
typedef struct _uconn_t {
    int unit;  // Unit number in device. 0 for primary, 1 for secondary.
    
    /* lpx_sock */
    int sock;
    struct sockaddr_lpx    lpx_addr;

    void*             lsp_session_buffer;
    lsp_handle_t   lsp_handle;
    lsp_status_t    lsp_status;
    // 32 bytes

    const lsp_hardware_data_t* hwdata;

    /* error for this connection */
    ndas_error_t        err;

    char*    encrypt_buffer;

    unsigned int        status; // CONN_STATUS_xxxx
    
    // 48 bytes

 
#ifndef XPLAT_RECONNECT
    dpc_id                    timer;

    int                        timeout;
#endif
    int 			reconnect_timeout;
    
    // 80 bytes

    int max_transfer_size;
    int max_split_bytes;
#ifdef USE_FLOW_CONTROL
    int read_split_bytes;	// Used for slow down size increment.
    int write_split_bytes;	// Used for slow down size increment.    
#endif
	// 96 bytes

    ndas_conn_mode_t mode;

    struct _BUFFLOCK_CONTROL* BuffLockCtl;
    struct _NDAS_DEVLOCK_INFO*   LockInfo;
}uconn_t;

#define CONN_RECONN_INTERVAL_MSEC (5000)

ndas_error_t conn_init(
    uconn_t *conn, xuchar* node, xuint32 unit, ndas_conn_mode_t mode,
    struct _BUFFLOCK_CONTROL* BuffLockCtl,
    struct _NDAS_DEVLOCK_INFO*   LockInfo
);

ndas_error_t conn_reinit(uconn_t *conn);

xbool conn_is_connected(uconn_t *conn);

ndas_error_t conn_connect(uconn_t *conn, sal_msec timeout);
ndas_error_t conn_reconnect(uconn_t *conn);

ndas_error_t conn_handshake(uconn_t *conn);

void conn_logout(uconn_t *conn);

void conn_disconnect(uconn_t *conn);

void conn_clean(uconn_t *conn);

ndas_error_t
conn_text_target_data(
    uconn_t *conn, xbool is_set, TEXT_TARGET_DATA* data
);

ndas_error_t
conn_text_target_list(
    uconn_t *conn, lsp_text_target_list_t* list, int size
);

ndas_error_t 
conn_lock_operation(
    uconn_t *conn, NDAS_LOCK_OP op, int lockid, void* data, void* opt
);

/*
    To do:  Role of conn_set_error and conn_set_status is ambiguous. 
        Check all usages
*/
void conn_set_error(uconn_t *conn, ndas_error_t err);

inline static 
void conn_set_status(uconn_t *conn, int status)
{
    conn->status = status;
}
/*
 * basic i/o to the unit device via the uconn_s established. Blocking call.
 */
#define CONNIO_CMD_READ              0
#define CONNIO_CMD_WRITE             1
#define 		CONNIO_RW_CMD_LAST              	1
#define CONNIO_CMD_FLUSH             2
#define CONNIO_CMD_VERIFY					3
#define CONNIO_CMD_WV						4
#define CONNIO_CMD_PACKET					5
#define CONNIO_CMD_SET_FEATURE			6
#define CONNIO_CMD_HANDSHAKE				7
#define CONNIO_CMD_PASSTHOUGH			8
ndas_error_t 
conn_rw(
    uconn_t *conn, xint8 cmd, xuint64 start_sec, xuint32 num_sec, char* buf
);

ndas_error_t
conn_rw_vector(
    uconn_t *conn, xint8 cmd, xuint64 start_sec, xuint32 num_sec, int nr_uio, struct sal_mem_block *uio
);


ndas_error_t
conn_do_ata_rw(
	uconn_t *conn, 
	xint8 cmd, 
	ndas_io_request_ptr io_req
);

ndas_error_t
conn_do_ata_flush(
	uconn_t *conn,
	ndas_io_request_ptr io_req
);

ndas_error_t
conn_do_ata_verify(
	uconn_t *conn,
	ndas_io_request_ptr io_req
);

ndas_error_t
conn_do_ata_write_verify(
	uconn_t *conn,
	ndas_io_request_ptr io_req	
);

ndas_error_t
conn_do_ata_set_feature(
	uconn_t *conn,
	ndas_io_request_ptr io_req
);

ndas_error_t
conn_do_atapi_cmd(
	uconn_t *conn,
	ndas_io_request_ptr io_req
);


ndas_error_t
conn_do_atapi_req_sense(
	uconn_t *conn,
	ndas_io_request_ptr io_req
);

ndas_error_t
conn_do_ata_passthrough(
	uconn_t *conn,
	ndas_io_request_ptr io_req
);



/*
 conn_read_dib read the NDAS DIB information from the give sector
 conn:
 offset_sector: the sector offset to read the DIB information
 */
ndas_error_t
conn_read_dib(
    uconn_t *conn,
    xuint64 sector_count,
    NDAS_DIB_V2 *pDIB_V2,
    xuint32 *pnDIBSize);

#ifdef XPLAT_RAID
ndas_error_t
conn_read_bitmap(uconn_t *conn,
            xuint64 sector_count,
              bitmap_t *bitmap);
#endif

#endif /* _NETDISK_CONN_H_ */

