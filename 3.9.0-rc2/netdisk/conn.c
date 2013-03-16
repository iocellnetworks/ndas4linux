/*
    Connection to unit device operations. 
    These functions should be called in single thread context 
    such as ndiod thread or blockable user function call
*/
#include "xplatcfg.h"
#include <sal/types.h>
#include "netdisk/conn.h"
#include <netdisk/scrc32.h>
#include <lspx/lsp_util.h>
#include <lspx/lsp.h>
#include <ndasuser/ndasuser.h>
#include "lockmgmt.h"

#ifndef NDAS_NO_LANSCSI

#ifdef DEBUG
#define    debug_conn(l, x...)    do {\
    if(l <= DEBUG_LEVEL_UCONN) {     \
        sal_debug_print("UN|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)    
#else
#define    debug_conn(l, x...)    do {} while(0)
#endif

/* To do: remove this */
extern int udev_max_xfer_unit;


#define CONN_SECTOR_SIZE        512
#define CONN_SECTOR_SIZE_SHIFT  9
#define CONN_SECTOR_TO_BYTE(SECTOR)    ((SECTOR) << CONN_SECTOR_SIZE_SHIFT)
#define CONN_BYTE_TO_SECTOR(BYTE)     ((BYTE) >> CONN_SECTOR_SIZE_SHIFT)

/* 
    Used to split and contain user IO vectors.
*/
typedef struct _lsp_transfer_param {
    lsp_large_integer_t start_sec;
    xuint32 num_sec;    // Number of sector to copy. The size of one sector is 512 for disk io
    xuint32 translen;	// used for general transfer	
    struct sal_mem_block *cur_uio;         /* cur_uio */
    int cur_uio_offset;
    int nr_uio;     /* number of array - uio */
    struct sal_mem_block *uio;         /* array of struct sal_mem_block that point the user supplied pointer to hold data */
    lpx_aio aio;
} lsp_transfer_param_t;

#define CONN_MEMBLK_INDEX_FROM_HEAD(HEAD_BLK, CUR_BLK) \
    ((CUR_BLK) - (HEAD_BLK))


int aio_ndio_copy(
    struct _lpx_aio * aio,
    void *recvdata,
    int ssz
)
{
    uconn_t* conn = (uconn_t *)aio->userparam;
    struct sal_mem_block* blocks = &aio->buf.blocks[aio->index];
    char* sptr = (char *)recvdata, *dptr;
    int dsz;
    int tocopy;
    debug_conn(4, "conn=%p nr_block=%d, blocks=%p offset=%d", conn, aio->nr_blocks, aio->buf.blocks, aio->offset);

    sal_assert( aio->nr_blocks != 0);
	sal_assert( ssz );
    dptr = blocks->ptr + aio->offset;
    dsz = blocks->len - aio->offset;

    debug_conn(6, "dptr=%p dsz=%d i=%d", dptr, dsz, (int)aio->index);
    debug_conn(6, "sptr=%p ssz=%d i=%d", sptr, ssz, (int)aio->index);

    while(ssz > 0) {
        if ( dsz < ssz ) {
            tocopy = dsz;
        } else {
            tocopy = ssz;
        }
        debug_conn(3, "%p(%d)<-%d-%p(%d)", dptr, dsz,tocopy, sptr, ssz);

        lsp_decrypt_recv_data(conn->lsp_handle, dptr, sptr, tocopy);

        debug_conn(3,"done:%p(%d)<-%d-%p(%d)", dptr, dsz,tocopy, sptr, ssz);

        dptr += tocopy;
        sptr += tocopy;
        dsz -= tocopy;
        ssz -= tocopy;

        if ( dsz == 0 ) {
            aio->index ++;
            aio->offset = 0;
            if (aio->index < aio->nr_blocks) {
                blocks ++;
                dptr = blocks->ptr;
                dsz = blocks->len;
                debug_conn(6, "blocks[%d %p]={%p %d}", aio->index, blocks, dptr, dsz);
            } else {
                if (ssz!=0) {
                    debug_conn(1, "We got already what we wanted ssz=%d", ssz);
                    // TODO: Remove copied data from xb and enqueue to header
                }
                break;
            }
        } else {
            sal_assert(dsz > 0);
            aio->offset = blocks->len - dsz;
        }
    }

    return NDAS_OK;
}

 ndas_error_t uconn_convert_lsp_status_to_ndas_error(lsp_status_t status)
{
    switch (status) {
        case LSP_STATUS_SUCCESS:
            return NDAS_OK;
        case LSP_STATUS_WRITE_ACCESS_DENIED:
            return NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
        case LSP_STATUS_LOCK_FAILED:
            return NDAS_ERROR_ACQUIRE_LOCK_FAILED;
        case LSP_STATUS_INVALID_SESSION:
            return NDAS_ERROR_INVALID_HANDLE;
        case LSP_STATUS_IDE_COMMAND_FAILED:
            return NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED;
        case LSP_STATUS_INVALID_PARAMETER:
            /* Connection is invalid or closed */
            return NDAS_ERROR_INVALID_HANDLE;
        default:
            {
                lsp_status_detail_t detail;

                detail.status  = status;
                debug_conn(1,"lsp error code %x(Respone=%x,Type=%x,Sequence=%x,Function=%x) not converted", detail.status,
                    detail.detail.response, detail.detail.type, detail.detail.sequence, detail.detail.function);
                sal_assert(0);
            }
            return NDAS_ERROR;
    }
#if 0 /* To do: translate ide error code to NDAS_ERROR_xxx */
LOCAL ndas_error_t 
translate_error_code(int response, int login_phase) 
{
    switch(response) {
    case LANSCSI_RESPONSE_SUCCESS:
        return NDAS_OK;
    case LANSCSI_RESPONSE_RI_NOT_EXIST:
        return NDAS_ERROR_IDE_REMOTE_INITIATOR_NOT_EXIST;
    case LANSCSI_RESPONSE_RI_BAD_COMMAND:
        return NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND;
    case LANSCSI_RESPONSE_RI_COMMAND_FAILED:
        return NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED;
    case LANSCSI_RESPONSE_RI_VERSION_MISMATCH:
        return NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION;
    case LANSCSI_RESPONSE_RI_AUTH_FAILED:
        return NDAS_ERROR_IDE_REMOTE_AUTH_FAILED;
    case LANSCSI_RESPONSE_T_NOT_EXIST:
        return NDAS_ERROR_IDE_TARGET_NOT_EXIST;
    case LANSCSI_RESPONSE_T_BAD_COMMAND:
        return NDAS_ERROR_HARDWARE_DEFECT; //NDAS_ERROR_IDE_TARGET_BAD_COMMAND;
    case LANSCSI_RESPONSE_T_COMMAND_FAILED:
        if (login_phase)
            return NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
        else
            return NDAS_ERROR_BAD_SECTOR;
    case LANSCSI_RESPONSE_T_BROKEN_DATA:
        return NDAS_ERROR_IDE_TARGET_BROKEN_DATA;
    }
    if ( response >= LANSCSI_RESPONSE_VENDOR_SPECIFIC_MIN &&
        response <= LANSCSI_RESPONSE_VENDOR_SPECIFIC_MAX )
        return NDAS_ERROR_IDE_VENDOR_SPECIFIC;
    return NDAS_ERROR;
}
#endif
}

LOCAL
ndas_error_t
conn_transport_process_transfer_send_userdata(
    uconn_t* context, lsp_transfer_param_t* const reqdesc)
{
    struct sal_mem_block* cur_uio = reqdesc->cur_uio;
    int cur_uio_offset = reqdesc->cur_uio_offset;
    //int remaining = CONN_SECTOR_TO_BYTE(reqdesc->num_sec);
    int remaining = reqdesc->translen;
    int to_send;
    int ret;

    debug_conn(4,"Sending user data %p, %d", cur_uio->ptr + cur_uio_offset, remaining);

    while(remaining > 0) {
        to_send = (cur_uio->len - cur_uio_offset ) <
                remaining ? ( cur_uio->len - cur_uio_offset ) : remaining;

        if (context->hwdata->data_encryption_algorithm) {
            lsp_encrypt_send_data(context->lsp_handle, 
                context->encrypt_buffer, 
                cur_uio->ptr + cur_uio_offset,
                to_send);
            ret = lpx_send(context->sock, context->encrypt_buffer, to_send, 0);
        } else {
            ret = lpx_send(context->sock, cur_uio->ptr + cur_uio_offset, to_send, 0);
        }
        if (to_send != ret)
        {
            return NDAS_ERROR_NETWORK_FAIL;
        }

        remaining -= to_send;
        if(cur_uio_offset + to_send == cur_uio->len) {
            cur_uio ++;
            cur_uio_offset = 0;
        } else {
            cur_uio_offset += to_send;
        }
    }

    // Move the current UIO pointer if success.
    reqdesc->cur_uio = cur_uio;
    reqdesc->cur_uio_offset = cur_uio_offset;
    return NDAS_OK;
}

ndas_error_t
conn_transport_process_transfer(
    uconn_t* context, lsp_transfer_param_t* reqdesc, int flags )
{
    int ret;
    char *buffer;
    lsp_uint32_t len_buffer;
    int sockfd = context->sock;
    lsp_status_t status;
    
    for (
        status = context->lsp_status;;
        status = lsp_process_next(context->lsp_handle))
    {
        context->lsp_status  = status;
        switch(status) {
            case LSP_REQUIRES_SEND_INTERNAL_DATA:
            {
            debug_conn(4, "LSP_REQUIRES_SEND_INTERNAL_DATA");
                buffer = lsp_get_buffer_to_send(
                    context->lsp_handle, &len_buffer);
                debug_conn(4,"Sending internal data %p:%d", buffer, len_buffer);
                ret = lpx_send(sockfd, buffer, len_buffer, 0);
                if (len_buffer != ret)
                {
                    return NDAS_ERROR_NETWORK_FAIL;
                }
                break;
            } 
            case LSP_REQUIRES_SEND_USER_DATA:
            {
                debug_conn(4, "LSP_REQUIRES_SEND_USER_DATA");
                ret = conn_transport_process_transfer_send_userdata(context, reqdesc);
                if(!NDAS_SUCCESS(ret)) {
                    return ret;
                }
                break;
            }
            case LSP_REQUIRES_RECEIVE_INTERNAL_DATA:
            {
            debug_conn(4, "LSP_REQUIRES_RECEIVE_INTERNAL_DATA");
                buffer = lsp_get_buffer_to_receive(
                    context->lsp_handle, &len_buffer);
                ret = lpx_recv(sockfd, buffer, len_buffer, flags);
                if (len_buffer != ret)
                {
                    debug_conn(1,"NDAS_ERROR_NETWORK_FAIL");
                    return NDAS_ERROR_NETWORK_FAIL;
                }
		 debug_conn(2,"Receiving internal data %p, %d", buffer, len_buffer);		
                break;
            }
            case LSP_REQUIRES_RECEIVE_USER_DATA:
            {
                debug_conn(4, "LSP_REQUIRES_RECEIVE_USER_DATA");

                /* lspx's call to receive */
                buffer = lsp_get_buffer_to_receive(
                    context->lsp_handle, &len_buffer);

                /* Receive to xbuf queue to remove data copy */
                debug_conn(4,"Receiving user data %p, %d offset=%d", buffer, len_buffer, reqdesc->cur_uio_offset);

                lpx_prepare_aio(
                    sockfd,
                    // Get the number of the remaining UIO entries
                    reqdesc->nr_uio - CONN_MEMBLK_INDEX_FROM_HEAD(reqdesc->uio, reqdesc->cur_uio),
                    (void *)reqdesc->cur_uio,
                    reqdesc->cur_uio_offset,
                    len_buffer,
                    flags,
                    &reqdesc->aio);
                lpx_set_user_copy_routine(aio_ndio_copy, &reqdesc->aio);
                lpx_set_userparam(context, &reqdesc->aio);
                ret = lpx_recv_aio(&reqdesc->aio);
                if (len_buffer != ret)
                {
                    return NDAS_ERROR_NETWORK_FAIL;
                }
                // Move the current UIO pointer if success.
                reqdesc->cur_uio += reqdesc->aio.index;
                reqdesc->cur_uio_offset = reqdesc->aio.offset;

                debug_conn(4,"UIO %p AIO index %d offset %d", reqdesc->cur_uio, reqdesc->aio.index, reqdesc->cur_uio_offset);
                break;
            }
            case LSP_REQUIRES_DATA_DECODE:
            {
                debug_conn(4, "LSP_REQUIRES_DATA_DECODE");
                    /* We already did encode at receive stage. */
                break;
            }
            case LSP_REQUIRES_DATA_ENCODE:
            {
                debug_conn(4, "LSP_REQUIRES_DATA_ENCODE");
                    /* We'll encode data when send. Do nothing right now. */
                break;
            }
            case LSP_REQUIRES_SYNCHRONIZE:
            {
                debug_conn(4, "LSP_REQUIRES_SYNCHRONIZE");
                /* wait until all pending tx/rx are completed */
                /* nothing to do on synchronous socket */
                break;
            }
            case LSP_REQUIRES_SEND: {
                sal_assert(0);
                break;
            }
            case LSP_REQUIRES_RECEIVE: {
                sal_assert(0);
                break;
            }
            default: {
                /* all other return values indicate the completion or
                error of the process */

                return uconn_convert_lsp_status_to_ndas_error(status);
            }
        }
    }
    /* unreachable here */
}


ndas_error_t
conn_handshake(uconn_t *conn)
{
    ndas_error_t ret;   
    conn->lsp_status = lsp_ata_handshake(conn->lsp_handle);
    ret = conn_transport_process_transfer(conn, NULL, 0);
    return ret;
}



ndas_error_t
conn_set_feature(uconn_t *conn, 
					xuint8 sub_com, 
					xuint8 sbu_com_specific_0,
					xuint8 sbu_com_specific_1,
					xuint8 sbu_com_specific_2,
					xuint8 sbu_com_specific_3
					)
{
	ndas_error_t ret;
	conn->lsp_status = lsp_ide_setfeatures(conn->lsp_handle, 
											sub_com, 
											sbu_com_specific_0, 
											sbu_com_specific_1, 
											sbu_com_specific_2, 
											sbu_com_specific_3
											);
	ret = conn_transport_process_transfer(conn, NULL, 0);
    return ret;
}


#ifdef XPLAT_RECONNECT
/* 
 *
 * Thread: i/O ndiod (user thread if block i/o)
 */
LOCAL
ndas_error_t do_reconnect(uconn_t *conn) 
{
    ndas_error_t err;

    if (conn->err == NDAS_ERROR_SHUTDOWN_IN_PROGRESS ) {
        debug_conn(1, "User want to stop reconnecting..");
        return NDAS_ERROR_SHUTDOWN_IN_PROGRESS;
    }

    if ( conn->sock > 0 ) {
        conn_disconnect(conn);
    }
    conn_reinit(conn);

    debug_conn(1, "connecting");
    err = conn_connect(conn, conn->reconnect_timeout);
    debug_conn(1, "connect return");
    
    if(!NDAS_SUCCESS(err)) {
        conn->sock = 0;
        debug_conn(1, "can't connect:err=%d",err);
        goto disconnect;
    }

    err  = conn_handshake(conn);
    if ( !NDAS_SUCCESS(err) ) {
        debug_conn(1, "fail to handshake");
        goto disconnect;
    }
    
    //
    // To do:  check device information has changed since last discover/connect
    //
    
    return NDAS_OK;
disconnect:    
    conn_disconnect(conn);
    return err;
}

ndas_error_t conn_reconnect(uconn_t *conn)
{
    int retry_count=0;
    ndas_error_t ret = conn->err;
    sal_msec start_msec = sal_time_msec();
    sal_msec timeout = conn->reconnect_timeout;
    if (ret == NDAS_ERROR_SHUTDOWN_IN_PROGRESS ) {
        debug_conn(1, "User wants to stop reconnecting..");
        return NDAS_ERROR_SHUTDOWN_IN_PROGRESS;
    }        
    for(retry_count=0; (sal_time_msec() - start_msec)<timeout ;retry_count++) {
        debug_conn(1,"Reconnecting trial %d", retry_count+1);
        ret = do_reconnect(conn);
        if ( ret == NDAS_ERROR_SHUTDOWN_IN_PROGRESS ) {
            return ret;
        }
        if (!NDAS_SUCCESS(ret)) {
            debug_conn(1,"Login failed err=%d", ret);
            sal_msleep(CONN_RECONN_INTERVAL_MSEC);
            continue;
        } 
        debug_conn(1,"Reconnected on  trial %d", retry_count+1);
        return NDAS_OK;
    }
    return ret;
}
#else
LOCAL
ndas_error_t conn_reconnect(uconn_t *conn)
{
    return NDAS_ERROR;
}
#endif

ndas_error_t conn_reinit(uconn_t *conn)
{
    debug_conn(3,"conn=%p", conn);

    if (conn->encrypt_buffer) {
        /* Encrypt buffer size may need to be changed. Free here and let it being created again */
        sal_free(conn->encrypt_buffer);
        conn->encrypt_buffer = NULL;
    }
    

    conn->lsp_handle =  lsp_initialize_session(
        conn->lsp_session_buffer,
        LSP_SESSION_BUFFER_SIZE
    );

    /* Use external encoder/decoder to handle scatter/gather buffer and xbuf */
    lsp_set_options(
        conn->lsp_handle, 
        LSP_SO_USE_EXTERNAL_DATA_ENCODE | 
        LSP_SO_USE_EXTERNAL_DATA_DECODE |
        LSP_SO_USE_DISTINCT_SEND_RECEIVE
    );

    conn_set_status(conn, CONN_STATUS_INIT);    
    return NDAS_OK;
}

/*
    Initialize conn members for connection.
*/
ndas_error_t
conn_init(
    uconn_t *conn,
    xuchar* node,
    xuint32 unit,
    ndas_conn_mode_t mode,
    BUFFLOCK_CONTROL* BuffLockCtl, 
    NDAS_DEVLOCK_INFO*   LockInfo
)
{

    debug_conn(3,"conn=%p", conn);

    sal_memset(conn, 0, sizeof(uconn_t));
    sal_memset(&conn->lpx_addr, 0, sizeof(conn->lpx_addr));
    conn->lpx_addr.slpx_family = AF_LPX;

    sal_memcpy(conn->lpx_addr.slpx_node, node, sizeof(conn->lpx_addr.slpx_node));
    conn->lpx_addr.slpx_port = lsp_htons(10000);

    conn->unit = unit;


    conn->encrypt_buffer = NULL;

    conn->mode = mode;

    conn->BuffLockCtl = BuffLockCtl;
    conn->LockInfo = LockInfo;

    conn->lsp_session_buffer = sal_malloc(LSP_SESSION_BUFFER_SIZE);
    conn->lsp_handle =  lsp_initialize_session(
        conn->lsp_session_buffer,
        LSP_SESSION_BUFFER_SIZE
    );

    /* Use external encoder/decoder to handle scatter/gather buffer and xbuf */
    lsp_set_options(
        conn->lsp_handle,
        LSP_SO_USE_EXTERNAL_DATA_ENCODE |
        LSP_SO_USE_EXTERNAL_DATA_DECODE |
        LSP_SO_USE_DISTINCT_SEND_RECEIVE
    );

    conn_set_status(conn, CONN_STATUS_INIT);
    return NDAS_OK;
}

void conn_clean(uconn_t *conn) 
{
    if (conn->lsp_session_buffer) {
        sal_free(conn->lsp_session_buffer);
        conn->lsp_session_buffer = NULL;
        conn->lsp_handle = NULL;
    }
    if (conn->encrypt_buffer) {
        sal_free(conn->encrypt_buffer);
        conn->encrypt_buffer = NULL;
    }
}

/* No return value. Okay to fail */
void conn_logout(uconn_t *conn) {
    debug_conn(1, "sock=%d", conn->sock);
    sal_assert(conn->lsp_handle !=NULL);
    conn->lsp_status = lsp_logout(conn->lsp_handle);
    conn_transport_process_transfer(conn, NULL, 0);
}

/* No return value. Okay to fail */
void conn_disconnect(uconn_t *conn) 
{
    debug_conn(1, "sock=%d", conn->sock);
    if (conn->sock) {
        lpx_close(conn->sock);
        conn->sock = 0;
    }
    conn_set_status(conn, CONN_STATUS_INIT);
    debug_conn(2, "status=CONN_STATUS_INIT");
}

void conn_set_error(uconn_t *conn, ndas_error_t err)
{
    debug_conn(2, "ing err = %d",  err);
    sal_assert(!NDAS_SUCCESS(err));
    conn->err = err;
}

xbool conn_is_connected(uconn_t *conn)
{
    if ( conn->sock <= 0 ) return FALSE;
    return NDAS_SUCCESS(lpx_is_connected(conn->sock)) ? TRUE:FALSE;
}

ndas_error_t conn_connect(uconn_t *conn, sal_msec timeout) 
{
    ndas_error_t err;
    int sock;
    lsp_login_info_t lsp_login_info;
    struct sockaddr_lpx bindaddr;
    debug_conn(3, "conn=%p" , conn);
    debug_conn(1, "Connecting via LANSCSI for %s:0x%x status=%d, unit=%d",
        SAL_DEBUG_HEXDUMP_S(conn->lpx_addr.slpx_node, 6),
        conn->lpx_addr.slpx_port,
        conn->status, conn->unit);

    sal_assert(conn->status == CONN_STATUS_INIT);
#ifdef DEBUG
    if ( conn->status != CONN_STATUS_INIT )
        debug_conn(1, "conn=%p conn->status=%x", conn, conn->status);
#endif        

    conn->sock = sock = lpx_socket(LPX_TYPE_STREAM, 0);
    debug_conn(1, "sock=%d", sock);
    if (sock < 0) {
        err = sock;
        goto errout;
    }

    /* Use any interface */
    sal_memset(&bindaddr, 0, sizeof(bindaddr));    
    bindaddr.slpx_family = AF_LPX;
    
    err = lpx_bind(sock, &bindaddr, sizeof(bindaddr));
    if (!NDAS_SUCCESS(err)) {
        debug_conn(1, "bind failed for sock %d:%d", sock, err);
        goto errout;
    }

    err = lpx_connect(conn->sock, &conn->lpx_addr, sizeof(conn->lpx_addr));
    if (!NDAS_SUCCESS(err)) {
        err = NDAS_ERROR_CONNECT_FAILED;
        goto errout;
    }

    err = lpx_set_rtimeout(conn->sock, timeout);
    if (!NDAS_SUCCESS(err)) {
        goto errout;
    }

    err = lpx_set_wtimeout(conn->sock, timeout);
    if (!NDAS_SUCCESS(err)) {
        goto errout;
    }

    conn_set_status(conn, CONN_STATUS_CONNECTING);
    
    conn->reconnect_timeout = timeout;
    
    /* default login values */
    sal_memset(&lsp_login_info, 0, sizeof(lsp_login_info));

    if (conn->mode == NDAS_CONN_MODE_SUPERVISOR) {
        const lsp_uint8_t LSP_LOGIN_PASSWORD_DEFAULT_SUPERVISOR[8] = { 0x1E, 0x13, 0x50, 0x47, 0x1A, 0x32, 0x2B, 0x3E};
        sal_memcpy(lsp_login_info.supervisor_password, 
               LSP_LOGIN_PASSWORD_DEFAULT_SUPERVISOR, sizeof(LSP_LOGIN_PASSWORD_DEFAULT_SUPERVISOR));
    } else {
        sal_memcpy(lsp_login_info.password, LSP_LOGIN_PASSWORD_ANY, sizeof(LSP_LOGIN_PASSWORD_ANY));
    }

    if (conn->mode == NDAS_CONN_MODE_DISCOVER) {
        lsp_login_info.login_type = LSP_LOGIN_TYPE_DISCOVER;
    } else {
        lsp_login_info.login_type = LSP_LOGIN_TYPE_NORMAL;
    }
    if (conn->mode == NDAS_CONN_MODE_EXCLUSIVE_WRITE) {
        lsp_login_info.write_access = 1;
    } else {
        lsp_login_info.write_access = 0;
    }

    lsp_login_info.unit_no = conn->unit;

    conn->lsp_status = lsp_login(conn->lsp_handle, &lsp_login_info);
    err = conn_transport_process_transfer(conn, NULL, 0);

    if (NDAS_SUCCESS(err)) {
        conn->hwdata = lsp_get_hardware_data(conn->lsp_handle);
        conn->max_transfer_size = conn->hwdata->maximum_transfer_blocks * 512;

        if (conn->hwdata->data_encryption_algorithm) {
            sal_assert(conn->encrypt_buffer == NULL);
            conn->encrypt_buffer = sal_malloc(conn->max_transfer_size);
            if (conn->encrypt_buffer == NULL) {
                err = NDAS_ERROR_OUT_OF_MEMORY;
                goto errout;
            }
        }

        if (conn->max_transfer_size > udev_max_xfer_unit) {
            conn->max_split_bytes = udev_max_xfer_unit;
        } else {
            conn->max_split_bytes = conn->max_transfer_size;
        }

        debug_conn(1, "Using max split bytes: %dk", conn->max_split_bytes/1024);

#ifdef USE_FLOW_CONTROL
        conn->read_split_bytes = conn->max_split_bytes;
        conn->write_split_bytes = conn->max_split_bytes;
#endif

        conn_set_status(conn, CONN_STATUS_CONNECTED);
        
        return NDAS_OK;
    }

errout:
    if (conn->sock>0) {
        conn_disconnect(conn);
    }
    return err;
}

void
inline
conn_adjust_read_split_size(uconn_t *conn, int rx_packet_loss)
{
#ifdef USE_FLOW_CONTROL	
    if (rx_packet_loss) {
        conn->read_split_bytes /=2;
        if (conn->read_split_bytes<4 * 1024) {
            conn->read_split_bytes = 4 * 1024;
        }
        sal_error_print("read split  = %d bytes\n", conn->read_split_bytes);
    } else {
        // Sector size will be increased after 128(=512/4) successful transfer.
        conn->read_split_bytes += 4;
        if (conn->read_split_bytes>conn->max_split_bytes) {
            conn->read_split_bytes = conn->max_split_bytes;
        }
    }
	debug_conn(4, "read split size: %d", conn->read_split_bytes);
#endif
}

void
inline
conn_adjust_write_split_size(uconn_t *conn, int tx_retransmit)
{
#ifdef USE_FLOW_CONTROL 
    if (tx_retransmit) {
        conn->write_split_bytes /=2;
        if (conn->write_split_bytes<4*1024) {
            conn->write_split_bytes = 4*1024;
        }
        sal_error_print("write split  = %d bytes\n", conn->write_split_bytes);
    } else {
        // Sector size will be increased after 128(=512/4) successful transfer.
        conn->write_split_bytes+=4;
        if (conn->write_split_bytes>conn->max_split_bytes) {
            conn->write_split_bytes = conn->max_split_bytes;
        }
    }
    debug_conn(4, "split size: %d, %d", conn->read_split_bytes, conn->write_split_bytes);
#endif
}

/*
 * Connection IO functions
 */
ndas_error_t
conn_read_func(
    uconn_t *conn,
    lsp_transfer_param_t *reqd,
    int *split_size
)
{
    ndas_error_t err, reterr;
    struct lpx_retransmits_count retxcnt;

    debug_conn(4, "Reading start=%llu, len=%d. %d %d", (long long unsigned int)reqd->start_sec.quad, reqd->num_sec,
    (int)CONN_MEMBLK_INDEX_FROM_HEAD(reqd->uio, reqd->cur_uio), reqd->cur_uio_offset);

    conn->lsp_status = lsp_ide_read(
        conn->lsp_handle,
        &reqd->start_sec,
        reqd->num_sec,
        (void*)reqd,
        CONN_SECTOR_TO_BYTE(reqd->num_sec));

    err = conn_transport_process_transfer(conn, reqd, 0);

#ifdef USE_FLOW_CONTROL
    reterr = lpx_get_retransmit_count(conn->sock, &retxcnt);
    if (reterr == NDAS_OK) {
        conn_adjust_read_split_size(conn, retxcnt.rx_packet_loss);
        *split_size = CONN_BYTE_TO_SECTOR(conn->read_split_bytes);
    }
#endif

    return err;
}



ndas_error_t
conn_write_func(
    uconn_t *conn,
    lsp_transfer_param_t *reqd,
    int *split_size
)
{
    ndas_error_t err, reterr;
    struct lpx_retransmits_count retxcnt;

    debug_conn(4, "Writing start=%llu, len=%d sectors", (long long unsigned int)reqd->start_sec.quad, reqd->num_sec);

    conn->lsp_status = lsp_ide_write(
        conn->lsp_handle,
        &reqd->start_sec,
        reqd->num_sec,
        (void*)reqd,
        CONN_SECTOR_TO_BYTE(reqd->num_sec));

    err = conn_transport_process_transfer(conn, reqd, 0);

#ifdef USE_FLOW_CONTROL
    reterr = lpx_get_retransmit_count(conn->sock, &retxcnt);
    if (reterr == NDAS_OK) {
        conn_adjust_write_split_size(conn, retxcnt.tx_retransmit);
        *split_size = CONN_BYTE_TO_SECTOR(conn->write_split_bytes);
    }
#endif

    return err;
}


ndas_error_t
conn_flush_func(
    uconn_t *conn,
    lsp_transfer_param_t *reqd
)
{
    debug_conn(1, "Flushing");

    conn->lsp_status = lsp_ide_flush(
        conn->lsp_handle, &reqd->start_sec, reqd->num_sec,  0, 0);

    return conn_transport_process_transfer(conn, reqd, 0);
}

ndas_error_t
conn_verify_func(
    uconn_t *conn,
    lsp_transfer_param_t *reqd
)
{
    debug_conn(1, "verifying");

    conn->lsp_status = lsp_ide_verify(
        conn->lsp_handle, &reqd->start_sec, reqd->num_sec,  (void*)reqd, 0);

    return conn_transport_process_transfer(conn, reqd, 0);
}


/*
 * Connection IO function type
 */

typedef ndas_error_t (* conn_io_func)(
        uconn_t *conn,
        lsp_transfer_param_t *reqd,
        int *split_size);

conn_io_func conn_io_vec[CONNIO_RW_CMD_LAST + 1] = {
    conn_read_func,
    conn_write_func
};

inline
ndas_error_t
conn_rw_io_execute(
    xint8 cmd,
    uconn_t *conn,
    lsp_transfer_param_t *reqd,
    int *split_size)
{
    sal_assert(cmd <= CONNIO_RW_CMD_LAST);
    return (conn_io_vec[(int)cmd])(conn, reqd, split_size);
}

/*
 * Execute vectored IO within the connection.
 */
ndas_error_t 
conn_rw_vector(
    uconn_t *conn,
    xint8 cmd,
    xuint64 start_sec,
    xuint32 num_sec,
    int nr_uio,
    struct sal_mem_block *uio
) {
    ndas_error_t err = NDAS_OK;
    lsp_transfer_param_t reqd;
    xuint32 transfered_sectors;
    xuint32 to_transfer;
    int split_size;
    int retrial = 0;

#if 0
    int i;
#endif

    if(cmd > CONNIO_RW_CMD_LAST) {
        return NDAS_ERROR_INVALID_PARAMETER;
    }
#ifdef USE_FLOW_CONTROL
    if (cmd == CONNIO_CMD_WRITE) {
    	split_size = conn->write_split_bytes/512;
    } else {
    	split_size = conn->read_split_bytes/512;
    }
#else 
    split_size = num_sec;
    if (split_size > udev_max_xfer_unit)
        split_size  = udev_max_xfer_unit;
#endif
    debug_conn(1, "ing conn=%p, sec=%llu, sec_len=%d, split size=%d nr_uio=%d", 
    	conn, (long long unsigned int)start_sec, num_sec, split_size, nr_uio);

#if 0
    for(i=0;i<nr_uio;i++) {
        debug_conn(4, "ioreq.uio[%d]: %p:%d", i, uio[i].ptr, uio[i].len);
    }
#endif

    transfered_sectors = 0;
    // Init LSP request
    reqd.nr_uio = nr_uio;
    reqd.uio = uio;
    reqd.cur_uio = uio;
    reqd.cur_uio_offset = 0;

    do {
        debug_conn(4, "req->num_sec=%d, split_size=%d", 
              num_sec, split_size);
        to_transfer = (num_sec - transfered_sectors <= split_size) ? (num_sec - transfered_sectors) : split_size;

        // Setup lsp_transfer_param_t structure
        reqd.num_sec = to_transfer;
	 reqd.translen = CONN_SECTOR_TO_BYTE(to_transfer);
        reqd.start_sec.quad = start_sec;

retry:
#ifdef USE_FLOW_CONTROL
        lpx_reset_retransmit_count(conn->sock);
#endif
        if (conn->mode & NDAS_CONN_MODE_LOCKED) {
            sal_assert(conn->BuffLockCtl);
            sal_assert(conn->LockInfo);
            debug_conn(4, "Acquiring buffer lock");
            err = NdasAcquireBufferLock(conn->BuffLockCtl, conn, conn->LockInfo, NULL, 0);
            if (err == NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION) {
                // This should not happen. Anyway ignore lock failure
                debug_conn(1, "Unsupported version");
            } else if (!NDAS_SUCCESS(err)) {
                debug_conn(1, "Failed to take buffer lock");
                break;
            }
        }

        // Execute the command at the remote.
        err = conn_rw_io_execute(cmd, conn, &reqd, &split_size);

        if (!NDAS_SUCCESS(err)) {
            debug_conn(4, "conn_transport_process_transfer() failed. %d", err);
            if (err == NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED) {
                retrial++;
                if (retrial < 5) {
                    debug_conn(1, "Retrying IO #%d", retrial);
                    reqd.num_sec = to_transfer;
		      reqd.translen = CONN_SECTOR_TO_BYTE(to_transfer);			
                    goto retry;
                }
            } else {
                err = conn_reconnect(conn);
            }

            if (NDAS_SUCCESS(err)) {
                // Recovered from error. Continue
                reqd.num_sec = to_transfer;
		  reqd.translen = CONN_SECTOR_TO_BYTE(to_transfer);			
                goto retry;
            }

            debug_conn(1, "Fatal error. Stopping IO");    
            break;
        } else {
            /* Succeeded in IO after retrial. Reset retrial count */
            retrial = 0;

            if (conn->mode & NDAS_CONN_MODE_LOCKED) {
                debug_conn(4, "Releasing buffer lock");
                err = NdasReleaseBufferLock(conn->BuffLockCtl, conn, conn->LockInfo, NULL, 0, FALSE, reqd.num_sec * 512);
                if (err == NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION) {
                    // This should not happen. Anyway ignore lock failure
                    debug_conn(1, "Unsupported version");
                } else if (!NDAS_SUCCESS(err)) {
                    debug_conn(1, "Failed to release buffer lock: err=%d", err);
                    break;
                }
            }
        }

        //
        // to do: get ide error code from recv_reply and handle it 
        //

        //
        // Transferable sector may be smaller than to_transfer size.
        //
        transfered_sectors += reqd.num_sec;
        start_sec += reqd.num_sec;

    } while(transfered_sectors < num_sec);

    debug_conn(3, "done sec_len=%d", num_sec);

    return err;
}

ndas_error_t 
conn_rw(
    uconn_t *conn, xint8 cmd, xuint64 start_sec, xuint32 num_sec, char* buf
) {
    struct sal_mem_block reqmem_block[1];
    //
    // Single chunk of data pointer is provided instead of memblock array.
    // Convert to memblock array.
    //
    debug_conn(3, "buf=%p", buf);    
    reqmem_block[0].len = num_sec << 9;
    reqmem_block[0].ptr = buf;
#ifndef NDAS_MEMBLK_NO_PRIVATE
    reqmem_block[0].private = NULL;
#endif
    return conn_rw_vector(conn, cmd, start_sec, num_sec, 1, reqmem_block);
}


ndas_error_t
conn_packet_cmd(
	uconn_t *conn,
	xuint8	*cdb,
	xuint8	data_to_dev,
	xuint32	reqlen,
	xuint32	nr_uio,
	struct sal_mem_block * uio
)
{
	int flags = 0;

	debug_conn(1, "packet cmd");
	switch(cdb[0]) {
		case 0x58 : //SCSIOP_REPAIR_TRACK:
		case 0x91: //SCSIOP_SYNCHRONIZE_CACHE16:
		case 0x35: //SCSIOP_SYNCHRONIZE_CACHE:
		case 0x5B: //SCSIOP_CLOSE_TRACK_SESSION:
		case 0x19: //SCSIOP_ERASE:
		case 0xA1: //SCSIOP_BLANK:
		case 0x54: //SCSIOP_SEND_OPC_INFORMATION:
		case 0x04: //SCSIOP_FORMAT_UNIT:	
		break;
			flags = AIO_FLAG_LOGTERM_IO;
		default:
			flags = AIO_FLAG_MEDTERM_IO;
		break;
	}


	
	if(nr_uio) {
		lsp_transfer_param_t reqd;
		
		sal_memset(&reqd, 0, sizeof(lsp_transfer_param_t));
		reqd.nr_uio = nr_uio;
		reqd.translen = reqlen;
		reqd.uio = uio;
		reqd.cur_uio = uio;
    		reqd.cur_uio_offset = 0;
			
		conn->lsp_status = lsp_ide_packet_cmd(
								conn->lsp_handle,  cdb, (void *)&reqd, reqlen, data_to_dev);
		return conn_transport_process_transfer(conn, &reqd, flags);
	}


	
	conn->lsp_status = lsp_ide_packet_cmd(
							conn->lsp_handle,  cdb, NULL, 0, 0);
	
	return conn_transport_process_transfer(conn, NULL, flags);	
}

ndas_error_t
conn_ata_passthrough(
        uconn_t *conn, 
        lsp_ide_register_param_t * idereg,
        xuint8	data_to_dev,
        xuint32	reqlen,
        xuint32	nr_uio,
        struct sal_mem_block * uio
)
{

    ndas_error_t ret;
    int flags = 0;
    lsp_io_data_buffer_t data_buf;

    sal_memset(&data_buf, 0, sizeof(lsp_io_data_buffer_t));
    
    if(nr_uio) {
        lsp_transfer_param_t reqd;

        sal_memset(&reqd, 0, sizeof(lsp_transfer_param_t));
        reqd.nr_uio = nr_uio;
        reqd.translen = reqlen;
        reqd.uio = uio;
        reqd.cur_uio = uio;
        reqd.cur_uio_offset = 0;	

        if(data_to_dev) {
            data_buf.send_buffer = (lsp_uint8_t *)uio;
            data_buf.send_size = reqlen;
        }else {
            data_buf.recv_buffer = (lsp_uint8_t *)uio;
            data_buf.recv_size = reqlen;
        }
        
        conn->lsp_status = lsp_ide_atapassthrough(
        						conn->lsp_handle,  idereg, &data_buf);
        ret =  conn_transport_process_transfer(conn, &reqd, flags);
        lsp_get_ide_command_output_register(conn->lsp_handle, idereg);
    }else {
        conn->lsp_status = lsp_ide_atapassthrough(
        					conn->lsp_handle,  idereg, &data_buf);
        ret = conn_transport_process_transfer(conn, NULL, flags);
        lsp_get_ide_command_output_register(conn->lsp_handle, idereg);
    }
    return ret;
}




ndas_error_t
conn_do_ata_rw(
	uconn_t *conn, 
	xint8 cmd, 
	ndas_io_request_ptr io_req
)
{
	if(io_req->nr_uio) {
		return conn_rw_vector(conn, cmd, io_req->start_sec, io_req->num_sec, io_req->nr_uio, io_req->uio);
	}
	return conn_rw(conn, cmd, io_req->start_sec, io_req->num_sec, io_req->buf);
	
}


ndas_error_t
conn_do_ata_flush(
	uconn_t *conn,
	ndas_io_request_ptr io_req
)
{
	ndas_error_t err = 0;
	lsp_transfer_param_t reqd;
retry:	
	reqd.num_sec = io_req->num_sec;
	reqd.start_sec.quad = io_req->start_sec;
	err = conn_flush_func(conn, &reqd);
	
	if (!NDAS_SUCCESS(err)) {
		if (err != NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED) {
			err = conn_reconnect(conn);
		} 
		
		if (NDAS_SUCCESS(err)) {			
		    goto retry;
		}
	} 
	return err;
}

ndas_error_t
conn_do_ata_verify(
	uconn_t *conn,
	ndas_io_request_ptr io_req
)
{
	ndas_error_t err = 0;
	lsp_transfer_param_t reqd;
retry:	
	
	reqd.num_sec = io_req->num_sec;
	reqd.start_sec.quad = io_req->start_sec;
	err =  conn_verify_func(conn, &reqd);

	if (!NDAS_SUCCESS(err)) {
		if (err != NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED) {
			err = conn_reconnect(conn);
		} 
		
		if (NDAS_SUCCESS(err)) {			
		    goto retry;
		}
	} 
	return err;	
}

ndas_error_t
conn_do_ata_write_verify(
	uconn_t *conn,
	ndas_io_request_ptr io_req	
)
{
	ndas_error_t err = NDAS_OK;
	err = conn_do_ata_rw(conn, CONNIO_CMD_WRITE, io_req);
	if(!NDAS_SUCCESS(err)){
		return err;
	}

	return conn_do_ata_verify(conn, io_req);
}



ndas_error_t
conn_do_ata_set_feature(
	uconn_t *conn,
	ndas_io_request_ptr io_req
)
{
	ndas_error_t err = 0;
	ndas_io_add_info_ptr addinfo = NULL;

	addinfo = (ndas_io_add_info_ptr)io_req->additionalInfo;
retry:		
	err = conn_set_feature(conn, 
						addinfo->ADD_INFO_SET_FEATURE.sub_com, 
						addinfo->ADD_INFO_SET_FEATURE.sub_com_specific_0,
						addinfo->ADD_INFO_SET_FEATURE.sub_com_specific_1,
						addinfo->ADD_INFO_SET_FEATURE.sub_com_specific_2,
						addinfo->ADD_INFO_SET_FEATURE.sub_com_specific_3
						);
	
	if (!NDAS_SUCCESS(err)) {
		if (err != NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED) {
			err = conn_reconnect(conn);
		} 
		
		if (NDAS_SUCCESS(err)) {			
		    goto retry;
		}
	} 
	return err;		
}

ndas_error_t
conn_do_atapi_cmd(
	uconn_t *conn,
	ndas_io_request_ptr io_req
)
{
	char * buffer = NULL;
	nads_io_pc_request_ptr pc_req = NULL;
	ndas_error_t err = NDAS_OK;	
	struct sal_mem_block uio;


	pc_req = (nads_io_pc_request_ptr)io_req->additionalInfo;

	if(pc_req->AuxBufLen) {
		if(pc_req->AuxBufLen % 4) {
			pc_req->AuxBufLen = ((pc_req->AuxBufLen + 3) / 4)*4;
			pc_req->useAuxBuf = 1;
		}
	}

	if(pc_req->useAuxBuf) {
		buffer = lsp_get_pkt_aux_buffer(conn->lsp_handle);
		if(io_req->nr_uio) {
			
			xuint32 delta = 0;
			uio.ptr = io_req->uio[io_req->nr_uio-1].ptr;
			uio.len = io_req->uio[io_req->nr_uio-1].len;
			io_req->uio[io_req->nr_uio-1].ptr = buffer;
			delta = (io_req->reqlen % 4);
			io_req->uio[io_req->nr_uio-1].len = uio.len + delta;
			sal_memset(buffer, 0, 4096);
			if(pc_req->direction == DATA_TO_DEV) {
				sal_memcpy(buffer, uio.ptr, uio.len);
			}
		}else {
			sal_memset(buffer, 0, 4096);
			if(pc_req->direction == DATA_TO_DEV){
				sal_memcpy(buffer, io_req->buf, io_req->reqlen);
			}
		}
	}else {
		if(io_req->nr_uio == 0) {
			buffer = io_req->buf;
		}
	}
	
retry:
	if(pc_req->AuxBufLen) {
		if(io_req->nr_uio) {
			err = conn_packet_cmd(conn, 
									pc_req->cmd, 
									(pc_req->direction == DATA_TO_DEV)?1:0,
									pc_req->AuxBufLen,
									io_req->nr_uio,
									io_req->uio
									);			
		}else {
			uio.ptr = buffer;
			uio.len = pc_req->AuxBufLen;

			err = conn_packet_cmd(conn, 
									pc_req->cmd, 
									(pc_req->direction == DATA_TO_DEV)?1:0,
									pc_req->AuxBufLen,
									1,
									&uio
									);						

		}
	}else {
		err = conn_packet_cmd(conn, 
								pc_req->cmd, 
								(pc_req->direction == DATA_TO_DEV)?1:0,
								pc_req->AuxBufLen,
								0,
								NULL
								);			
	}

	if (!NDAS_SUCCESS(err)) {
		switch(err) {
			case NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND :
			case NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED :
			case NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION:
			case NDAS_ERROR_IDE_REMOTE_AUTH_FAILED :
			case NDAS_ERROR_HARDWARE_DEFECT:
			case NDAS_ERROR_IDE_TARGET_BROKEN_DATA :
			case NDAS_ERROR_BAD_SECTOR:
			case NDAS_ERROR_IDE_VENDOR_SPECIFIC :
			break;
			default:
			{
				debug_conn(1, "device error try reconnect");
				err = conn_reconnect(conn);
				if (NDAS_SUCCESS(err)) {			
		    				goto retry;
				}
			}
			break;
				
		}
	}else {
		if(pc_req->AuxBufLen) {
			if(pc_req->useAuxBuf) {
				if(io_req->nr_uio) {
					if(pc_req->direction == DATA_FROM_DEV) {
						sal_memcpy(uio.ptr, io_req->uio[io_req->nr_uio -1].ptr, uio.len);
					}
					
					io_req->uio[io_req->nr_uio -1].ptr = uio.ptr;
					io_req->uio[io_req->nr_uio -1].len = uio.len;

					
				}else {
					if(pc_req->direction == DATA_FROM_DEV) {
						sal_memcpy(io_req->buf, buffer, io_req->reqlen);
					}					
				}
			}
		}
	}
	return err;	
		
}


ndas_error_t
conn_do_atapi_req_sense(
	uconn_t *conn,
	ndas_io_request_ptr io_req
)
{
	unsigned char sense_cmd[12];
	nads_io_pc_request_ptr pc_req = NULL;
	ndas_error_t err = NDAS_OK;
	struct sal_mem_block uio;

	
	pc_req = (nads_io_pc_request_ptr)io_req->additionalInfo;
	
	sal_memset(sense_cmd, 0, 12);
	sal_memset(pc_req->sensedata, 0, DEFAULT_SENSE_DATA);
	sense_cmd[0] = 0x03; //SCSIOP_REQUEST_SENSE;
	sense_cmd[4] =  32; //DEFAULT_SENSE_DATA

	uio.len = 32;
	uio.ptr = pc_req->sensedata;

retry:
	err = conn_packet_cmd(conn, 
							sense_cmd, 
							0,
							32,
							1,
							&uio
							);	
	
	if (!NDAS_SUCCESS(err)) {
		switch(err) {
			case NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND :
			case NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED :
			case NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION:
			case NDAS_ERROR_IDE_REMOTE_AUTH_FAILED :
			case NDAS_ERROR_HARDWARE_DEFECT:
			case NDAS_ERROR_IDE_TARGET_BROKEN_DATA :
			case NDAS_ERROR_BAD_SECTOR:
			case NDAS_ERROR_IDE_VENDOR_SPECIFIC :
			break;
			default:
			{
				err = conn_reconnect(conn);
				if (NDAS_SUCCESS(err)) {			
		    				goto retry;
				}
			}
			break;
				
		}
		
	}else {
		pc_req->avaliableSenseData = 1;
	}

	return err;
}


ndas_error_t
conn_do_ata_passthrough(
	uconn_t *conn,
	ndas_io_request_ptr io_req
)
{
    ndas_error_t err = NDAS_OK;	
    nads_io_pc_request_ptr pc_req = NULL;
    ndas_io_add_info_ptr pthrough_req = NULL;
    lsp_ide_register_param_t idereg;
    struct sal_mem_block uio;

retry:
    pc_req = (nads_io_pc_request_ptr)io_req->additionalInfo;
    pthrough_req = (ndas_io_add_info_ptr)pc_req->sensedata;

    sal_memset(&idereg, 0, sizeof(lsp_ide_register_param_t));


    if(pthrough_req->ADD_INFO_ATA_PASSTHROUGH.protocol == 15) {
        xuint8 * sensebuf = pc_req->sensedata;
        
        lsp_get_ide_command_output_register(conn->lsp_handle, &idereg);
         if(pthrough_req->ADD_INFO_ATA_PASSTHROUGH.LBA48) {
            sal_memset(sensebuf, 0, DEFAULT_SENSE_DATA);
            sensebuf[0] = 0x01;
            sensebuf[1] = 0x0C;
            sensebuf[2] = 0x01;
            sensebuf[3] = idereg.reg.ret.err.err_na;
            sensebuf[4] = idereg.reg.named_48.prev.sector_count;
            sensebuf[5] = idereg.reg.named_48.cur.sector_count;
            sensebuf[6] = idereg.reg.named_48.prev.lba_low;
            sensebuf[7] = idereg.reg.named_48.cur.lba_low;
            sensebuf[8] = idereg.reg.named_48.prev.lba_mid;
            sensebuf[9] = idereg.reg.named_48.cur.lba_mid;
            sensebuf[10] = idereg.reg.named_48.prev.lba_high;
            sensebuf[11] = idereg.reg.named_48.cur.lba_high;
            sensebuf[12] = idereg.device.device;
            sensebuf[13] = idereg.command.command;                        
        }else {
           sal_memset(sensebuf, 0, DEFAULT_SENSE_DATA);
            sensebuf[0] = 0x01;
            sensebuf[1] = 0x0C;
            sensebuf[3] = idereg.reg.ret.err.err_na;
            sensebuf[5] = idereg.reg.named_48.cur.sector_count;
            sensebuf[7] = idereg.reg.named_48.cur.lba_low;
            sensebuf[9] = idereg.reg.named_48.cur.lba_mid;
            sensebuf[11] = idereg.reg.named_48.cur.lba_high;
            sensebuf[12] = idereg.device.device;
            sensebuf[13] = idereg.command.command;                    
        }
        pc_req->avaliableSenseData = 1;       
        return NDAS_OK;
    }


    
    idereg.reg.named_48.cur.sector_count = pthrough_req->ADD_INFO_ATA_PASSTHROUGH.nsec;
    idereg.reg.named_48.cur.lba_low = pthrough_req->ADD_INFO_ATA_PASSTHROUGH.lbal;
    idereg.reg.named_48.cur.lba_mid = pthrough_req->ADD_INFO_ATA_PASSTHROUGH.lbam;
    idereg.reg.named_48.cur.lba_high = pthrough_req->ADD_INFO_ATA_PASSTHROUGH.lbah;
    
    idereg.reg.named_48.prev.sector_count = pthrough_req->ADD_INFO_ATA_PASSTHROUGH.hob_nsec;
    idereg.reg.named_48.prev.lba_low = pthrough_req->ADD_INFO_ATA_PASSTHROUGH.hob_lbal;
    idereg.reg.named_48.prev.lba_mid = pthrough_req->ADD_INFO_ATA_PASSTHROUGH.hob_lbam;
    idereg.reg.named_48.prev.lba_high = pthrough_req->ADD_INFO_ATA_PASSTHROUGH.hob_lbah;

    idereg.command.command = pthrough_req->ADD_INFO_ATA_PASSTHROUGH.command;


    if(io_req->reqlen) {
        if(io_req->nr_uio) {
            err = conn_ata_passthrough(conn, 
                                                            &idereg,
                                                            (pc_req->direction == DATA_TO_DEV)?1:0,
                                                            io_req->reqlen, 
                                                            io_req->nr_uio, 
                                                            io_req->uio
                                                            );
        }else {
            uio.len = io_req->reqlen;
            uio.ptr = io_req->buf;
            err = conn_ata_passthrough(conn, 
                                                            &idereg, 
                                                            (pc_req->direction == DATA_TO_DEV)?1:0,
                                                            io_req->reqlen, 
                                                            1, 
                                                            &uio
                                                            );
        }
    }else {
            err = conn_ata_passthrough(conn, 
                                                            &idereg, 
                                                            0,
                                                            0, 
                                                            0,
                                                            NULL
                                                            );
    }

    if (!NDAS_SUCCESS(err)) {
    	switch(err) {
    		case NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND :
    		case NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED :
    		case NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION:
    		case NDAS_ERROR_IDE_REMOTE_AUTH_FAILED :
    		case NDAS_ERROR_HARDWARE_DEFECT:
    		case NDAS_ERROR_IDE_TARGET_BROKEN_DATA :
    		case NDAS_ERROR_BAD_SECTOR:
    		case NDAS_ERROR_IDE_VENDOR_SPECIFIC :
              {
                    xuint8 * sensebuf = pc_req->sensedata;
                    if(pthrough_req->ADD_INFO_ATA_PASSTHROUGH.LBA48) {
                        sal_memset(sensebuf, 0, DEFAULT_SENSE_DATA);
                        sensebuf[0] = 0x09;
                        sensebuf[1] = 0x0C;
                        sensebuf[2] = 0x01;
                        sensebuf[3] = idereg.reg.ret.err.err_na;
                        sensebuf[4] = idereg.reg.named_48.prev.sector_count;
                        sensebuf[5] = idereg.reg.named_48.cur.sector_count;
                        sensebuf[6] = idereg.reg.named_48.prev.lba_low;
                        sensebuf[7] = idereg.reg.named_48.cur.lba_low;
                        sensebuf[8] = idereg.reg.named_48.prev.lba_mid;
                        sensebuf[9] = idereg.reg.named_48.cur.lba_mid;
                        sensebuf[10] = idereg.reg.named_48.prev.lba_high;
                        sensebuf[11] = idereg.reg.named_48.cur.lba_high;
                        sensebuf[12] = idereg.device.device;
                        sensebuf[13] = idereg.command.command;
                    }else {
                       sal_memset(sensebuf, 0, DEFAULT_SENSE_DATA);
                        sensebuf[0] = 0x09;
                        sensebuf[1] = 0x0C;
                        sensebuf[3] = idereg.reg.ret.err.err_na;
                        sensebuf[5] = idereg.reg.named_48.cur.sector_count;
                        sensebuf[7] = idereg.reg.named_48.cur.lba_low;
                        sensebuf[9] = idereg.reg.named_48.cur.lba_mid;
                        sensebuf[11] = idereg.reg.named_48.cur.lba_high;
                        sensebuf[12] = idereg.device.device;
                        sensebuf[13] = idereg.command.command;
                    }
                    pc_req->avaliableSenseData = 1;
              }
    		break;
    		default:
    		{
    			debug_conn(1, "device error try reconnect");
    			err = conn_reconnect(conn);
    			if (NDAS_SUCCESS(err)) {			
    	    				goto retry;
    			}
    		}
    		break;	
    	}
    }else {
    	        if(pthrough_req->ADD_INFO_ATA_PASSTHROUGH.check_cond) {
                    xuint8 * sensebuf = pc_req->sensedata;
                    
                    if(pthrough_req->ADD_INFO_ATA_PASSTHROUGH.LBA48) {
                        sal_memset(sensebuf, 0, DEFAULT_SENSE_DATA);
                        sensebuf[0] = 0x01;
                        sensebuf[1] = 0x0C;
                        sensebuf[2] = 0x01;
                        sensebuf[3] = idereg.reg.ret.err.err_na;
                        sensebuf[4] = idereg.reg.named_48.prev.sector_count;
                        sensebuf[5] = idereg.reg.named_48.cur.sector_count;
                        sensebuf[6] = idereg.reg.named_48.prev.lba_low;
                        sensebuf[7] = idereg.reg.named_48.cur.lba_low;
                        sensebuf[8] = idereg.reg.named_48.prev.lba_mid;
                        sensebuf[9] = idereg.reg.named_48.cur.lba_mid;
                        sensebuf[10] = idereg.reg.named_48.prev.lba_high;
                        sensebuf[11] = idereg.reg.named_48.cur.lba_high;
                        sensebuf[12] = idereg.device.device;
                        sensebuf[13] = idereg.command.command;                        
                    }else {
                       sal_memset(sensebuf, 0, DEFAULT_SENSE_DATA);
                        sensebuf[0] = 0x01;
                        sensebuf[1] = 0x0C;
                        sensebuf[3] = idereg.reg.ret.err.err_na;
                        sensebuf[5] = idereg.reg.named_48.cur.sector_count;
                        sensebuf[7] = idereg.reg.named_48.cur.lba_low;
                        sensebuf[9] = idereg.reg.named_48.cur.lba_mid;
                        sensebuf[11] = idereg.reg.named_48.cur.lba_high;
                        sensebuf[12] = idereg.device.device;
                        sensebuf[13] = idereg.command.command;                    
                    }
                    pc_req->avaliableSenseData = 1;
                }
    }

    return err;
}


/* Used for internal use, does not lock unit dev->io_lock */
/* Return NDAS_ERROR_ACQUIRE_LOCK_FAILED for lock failure
    NDAS_OK for success
    and rest for any other error case */
ndas_error_t
conn_lock_operation(uconn_t *conn, NDAS_LOCK_OP op, int lockid, void* lockdata, void* opt)
{
    char param[8] = {0};
    ndas_error_t err = NDAS_ERROR_INTERNAL;
    lsp_request_packet_t request = {0};
    lsp_vendor_command_request_t* vc_request;
    int i;
    
    /* Check version */
    if (conn->hwdata->hardware_version == LSP_HARDWARE_VERSION_1_0) {
        debug_conn(1, "HW does not support device lock");
        return NDAS_ERROR_UNSUPPORTED_FEATURE;
    } else if (conn->hwdata->hardware_version != LSP_HARDWARE_VERSION_1_1 && 
        conn->hwdata->hardware_version != LSP_HARDWARE_VERSION_2_0) {
        debug_conn(1, "unsupported SW version");
        return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
    }
    param[3] = lockid;

    request.type = LSP_REQUEST_VENDOR_COMMAND;
    vc_request = &request.u.vendor_command.request;
    vc_request->vendor_id = LSP_VENDOR_ID_XIMETA;
    vc_request->vop_ver = LSP_VENDOR_OP_VERSION_1_0;
    vc_request->param[0] = lockid;
    vc_request->param_length = 8;   /* Up to 2.0, always 8 */

    switch(op) {
    case NDAS_LOCK_TAKE:
        vc_request->vop_code = LSP_VCMD_SET_SEMA;
        conn->lsp_status = lsp_request(conn->lsp_handle, &request);
        err = conn_transport_process_transfer(conn, NULL, 0);
        if (err == NDAS_OK || err == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {
            lsp_get_vendor_command_result(conn->lsp_handle, (lsp_uint8_t*) param, 8);
            if (lockdata) {
                *((int*) lockdata) = lsp_ntohl(*((int*)&param[4]));
                debug_conn(4, "lock data=%x", *((int*) lockdata));
            }
        }
        debug_conn(4, "LOCK TAKE returning %d", err);
        break;
    case NDAS_LOCK_GIVE:
        vc_request->vop_code = LSP_VCMD_FREE_SEMA;        
        conn->lsp_status = lsp_request(conn->lsp_handle, &request);
        err = conn_transport_process_transfer(conn, NULL, 0);
        if (err == NDAS_OK) {
            lsp_get_vendor_command_result(conn->lsp_handle, (lsp_uint8_t*) param, 8);
            if (lockdata) {
                *((int*) lockdata) = lsp_ntohl(*((int*)&param[4]));
                debug_conn(4, "lock data=%x", *((int*) lockdata));
            }
        }
        debug_conn(4, "LOCK GIVE returning %d", err);
        break;

    case NDAS_LOCK_GET_COUNT:
        vc_request->vop_code = LSP_VCMD_GET_SEMA;
        conn->lsp_status = lsp_request(conn->lsp_handle, &request);
        err = conn_transport_process_transfer(conn, NULL, 0);
        if (err == NDAS_OK) {
            lsp_get_vendor_command_result(conn->lsp_handle, (lsp_uint8_t*) param, 8);
            *(int*)lockdata = (((xuint32)param[4])<<24)+ (((xuint32)param[5])<<16)+ (((xuint32)param[6])<<8) + (xuint32)param[7];
        }
        break;

    case NDAS_LOCK_GET_OWNER:
        vc_request->vop_code = LSP_VCMD_GET_OWNER_SEMA;        
        conn->lsp_status = lsp_request(conn->lsp_handle, &request);
        err = conn_transport_process_transfer(conn, NULL, 0);
        if (err == NDAS_OK) {
            lsp_get_vendor_command_result(conn->lsp_handle, (lsp_uint8_t*) param, 8);
            sal_memcpy(lockdata, &param[2], 6);
        } else {
            sal_memset(lockdata, 0, 6);
        }
        break;

    case NDAS_LOCK_BREAK:
        {
            uconn_t* conn_array;
            uconn_t* current_conn;            
            debug_conn(1, "NDAS_LOCK_BREAK for lock %d", lockid);
            if (!(conn->hwdata->hardware_version == LSP_HARDWARE_VERSION_2_0 &&
                conn->hwdata->hardware_revision == LSP_HARDWARE_V20_REV_0)) {
                err = NDAS_ERROR_UNSUPPORTED_FEATURE;
                /* This lock break mechanism is workaround for 2.0 chip bug. This method does not work on other versions */
                break;
            }
            
            conn_array = sal_malloc(sizeof(uconn_t) * (NDAS_MAX_CONNECTION_COUNT-1));
            if (!conn_array) {
                err = NDAS_ERROR_OUT_OF_MEMORY;
                break;
            }
            
            for (i=0;i<NDAS_MAX_CONNECTION_COUNT-1;i++) {
                current_conn = &(conn_array[i]);
                err  = conn_init(current_conn, current_conn->lpx_addr.slpx_node, 0, NDAS_CONN_MODE_READ_ONLY, NULL, NULL);
                if ( !NDAS_SUCCESS(err)) {
                    continue;
                }
                
                err = conn_connect(current_conn, CONN_DEFAULT_IO_TIMEOUT);
                if ( !NDAS_SUCCESS(err)) {
                    continue;
                }

                vc_request->vop_code = LSP_VCMD_SET_SEMA;
                current_conn->lsp_status = lsp_request(current_conn->lsp_handle, &request);
                err = conn_transport_process_transfer(current_conn, NULL, 0);

                vc_request->vop_code = LSP_VCMD_FREE_SEMA;        
                current_conn->lsp_status = lsp_request(current_conn->lsp_handle, &request);
                err = conn_transport_process_transfer(current_conn, NULL, 0);
            }
            
            for (i=0;i<NDAS_MAX_CONNECTION_COUNT-1;i++) {
                current_conn = &(conn_array[i]);
                conn_logout(current_conn);
                conn_disconnect(current_conn);
                conn_clean(current_conn);
            }
            sal_free(conn_array);
        }
        break;
    default:
        sal_assert(0);
        err = NDAS_ERROR_INTERNAL;
        break;
    }
    sal_assert(NDAS_ERROR_INTERNAL !=err);
    return err;
}

ndas_error_t
conn_text_target_data(uconn_t *conn, xbool is_set, TEXT_TARGET_DATA* data)
{
    ndas_error_t ret;
    lsp_text_target_data_t tdata;
    xuint64 target_data;

#ifdef DEBUG
    if (is_set){
        debug_conn(4, "Setting target data: 0x%llu", *data);
    }
#endif

    sal_memset(&tdata, 0, sizeof(tdata));
    tdata.type = LSP_TEXT_BINPARAM_TYPE_TARGET_DATA;
    tdata.to_set = (lsp_uint8_t) is_set;
    // Host endian to network endian
    target_data = lsp_ntohll(*data);
    sal_memcpy(tdata.target_data, &target_data, sizeof(TEXT_TARGET_DATA));
    tdata.target_id = conn->unit;

    conn->lsp_status = lsp_text_command(
    	conn->lsp_handle, 
    	0x01, /* text_type_binary */
    	0x00, /* text_version */
    	&tdata,
    	sizeof(tdata),
    	&tdata,
    	sizeof(tdata));

    ret = conn_transport_process_transfer(conn, NULL, 0);
    if (NDAS_SUCCESS(ret) && !is_set) {
        sal_memcpy(data, tdata.target_data, sizeof(TEXT_TARGET_DATA));
		// Network endian to host endian.
        *data = lsp_ntohll(*data);
#ifdef DEBUG
        debug_conn(4, "Got target data: 0x%llu", *data);
#endif
    }
    return ret;
}



ndas_error_t
conn_text_target_list(
    uconn_t *conn, lsp_text_target_list_t* list, int size
) {
    ndas_error_t ret;

    if (size < sizeof(lsp_text_target_list_t)) {
        debug_conn(1, "Buffer is not enough");
        sal_assert(0);
        return NDAS_ERROR_BUFFER_OVERFLOW;
    }

    sal_memset(list, 0, sizeof(list));
    list->type = LSP_TEXT_BINPARAM_TYPE_TARGET_LIST;

    conn->lsp_status = lsp_text_command(
		conn->lsp_handle, 
		0x01, /* text_type_binary */
		0x00, /* text_version */
		list,
		4,  /* Input buffer size */
		list,
		size);

    ret = conn_transport_process_transfer(conn, NULL, 0);

    return ret;
}


/*
To do: move DIB analyze to logdev layer.
1. Read an NDAS_DIB_V2 structure from the NDAS Device at NDAS_BLOCK_LOCATION_DIB_V2

2. Check Signature, Version and CRC informations in NDAS_DIB_V2 and 
    accept if all the informations are correct
    
3. Read additional NDAS Device location informations at NDAS_BLOCK_LOCATION_ADD_BIND 
  in case of more than 32 NDAS Unit devices exist 4. 
  Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 
  if NDAS_DIB_V2 information is not acceptable
  
5. Check Signature and Version informations in NDAS_DIB_V1 
and translate the NDAS_DIB_V1 to an NDAS_DIB_V2

6. Create an NDAS_DIB_V2 as single NDAS Disk Device 
if the NDAS_DIB_V1 is not acceptable either

*/

ndas_error_t
conn_read_dib(uconn_t *conn,
            xuint64 sector_count, 
              NDAS_DIB_V2 *pDIB_V2,
              xuint32*pnDIBSize)
{
    ndas_error_t err;
    NDAS_DIB* DevDIB_V1 = NULL;
    NDAS_DIB_V2* DevDIB_V2 = NULL;
    xuint32 nDIBSize;
    int unit;
    
    debug_conn(1, "conn_read_dib conn=%p", conn);
    unit = conn->unit;        // to do: get from sdev info

    DevDIB_V1 = sal_malloc(sizeof(NDAS_DIB));
    if (DevDIB_V1 == NULL)  {
        err = NDAS_ERROR_OUT_OF_MEMORY;
        goto errout;
    }
    DevDIB_V2 = sal_malloc(sizeof(NDAS_DIB_V2));
    if (DevDIB_V2==NULL)  {
        err = NDAS_ERROR_OUT_OF_MEMORY;
        goto errout;
    }

    sal_assert(sizeof(NDAS_DIB) == 512);
    sal_assert(sizeof(NDAS_DIB_V2) == 512);
    
    err = conn_rw(conn, CONNIO_CMD_READ, sector_count + NDAS_BLOCK_LOCATION_DIB_V2, 1, (char*)DevDIB_V2);

    if (!NDAS_SUCCESS(err)) {
        debug_conn(1, "conn_read_dib failed: %d", err);
        goto errout;
    }

    // Check Signature, Version and CRC informations in NDAS_DIB_V2 and accept if all the informations are correct    
    if(sal_memcmp(NDAS_DIB_V2_SIGNATURE, DevDIB_V2->u.s.Signature,8) != 0) {
        debug_conn(2, "DIB V2 signature mismatch");
        goto try_v1;
    }

    if(DevDIB_V2->crc32 != crc32_calc((unsigned char *)DevDIB_V2, sizeof(DevDIB_V2->u.bytes_248)))
        goto try_v1;

    if(DevDIB_V2->crc32_unitdisks != crc32_calc((unsigned char *)DevDIB_V2->UnitDisks,    sizeof(DevDIB_V2->UnitDisks)))
        goto try_v1;

    if(DevDIB_V2->u.s.iMediaType != NMT_MEDIAJUKE)
    	{
	    if(NDAS_BLOCK_SIZE_XAREA != DevDIB_V2->u.s.sizeXArea &&
	        NDAS_BLOCK_SIZE_XAREA * SECTOR_SIZE != DevDIB_V2->u.s.sizeXArea){
	        debug_conn(1,"DIB V2 XArea mispatch!!!");    	
	        goto try_v1;
	    }
	    if(DevDIB_V2->u.s.sizeUserSpace > sector_count) /* Invalid sector count is recorded?? */
	     {
	       debug_conn(1,"DIB V2 UserSpace!!!");    	
	        goto try_v1;
	     }
    	}

    // check DIB_V2.nDiskCount
    switch(DevDIB_V2->u.s.iMediaType)
    {
    case NMT_SINGLE:
        if(DevDIB_V2->u.s.nDiskCount)
            goto try_v1;
        break;
    case NMT_AGGREGATE:
        if(DevDIB_V2->u.s.nDiskCount < 2)
            goto try_v1;
        break;
    case NMT_RAID0:
        if(DevDIB_V2->u.s.nDiskCount != 2 &&
            DevDIB_V2->u.s.nDiskCount != 4 &&
            DevDIB_V2->u.s.nDiskCount != 8)
            goto try_v1;
        break;
    case NMT_RAID1:
        if(DevDIB_V2->u.s.nDiskCount % 2)
            goto try_v1;
        break;
    case NMT_RAID4:
        if(DevDIB_V2->u.s.nDiskCount != 3 &&
            DevDIB_V2->u.s.nDiskCount != 5 &&
            DevDIB_V2->u.s.nDiskCount != 9)
            goto try_v1;
        break;
    case NMT_MEDIAJUKE:
        sal_memcpy(pDIB_V2, DevDIB_V2, sizeof(NDAS_DIB_V2));
        err= NDAS_OK;
        debug_conn(1,"NMT_MEDIAJUKE!!!");   
        goto errout;
        break;
    default:
        goto try_v1;
        break;
    }

    if(DevDIB_V2->u.s.iSequence >= DevDIB_V2->u.s.nDiskCount)
        goto try_v1;

    if(DevDIB_V2->u.s.nDiskCount > 32 + 64 + 64) // AING : PROTECTION
        goto try_v1;

    // check done, copy DIB_V2 information from NDAS Device to pDIB_V2
    // code does not support if version in DIB_V2 is greater than the version defined
    if (NDAS_DIB_VERSION_MAJOR_V2 < DevDIB_V2->u.s.MajorVersion ||
        (NDAS_DIB_VERSION_MAJOR_V2 == DevDIB_V2->u.s.MajorVersion && 
        NDAS_DIB_VERSION_MINOR_V2 < DevDIB_V2->u.s.MinorVersion)) {
        debug_conn(1, "Unsupported DIB version %d.%d", DevDIB_V2->u.s.MajorVersion, DevDIB_V2->u.s.MinorVersion);
        err = NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
        goto errout;
    }

    nDIBSize = (GET_TRAIL_SECTOR_COUNT_V2(DevDIB_V2->u.s.nDiskCount) + 1) 
        * sizeof(NDAS_DIB_V2);

    if(NULL == pnDIBSize) {// copy 1 sector only
        sal_memcpy(pDIB_V2, DevDIB_V2, sizeof(NDAS_DIB_V2));
        err= NDAS_OK;
        goto errout;
    }

    if(*pnDIBSize < nDIBSize) { // make sure there is enough space
        *pnDIBSize = nDIBSize; // do not copy, possibly just asking size
        err= NDAS_OK;
        goto errout;
    }
    
    sal_memcpy(pDIB_V2, DevDIB_V2, sizeof(NDAS_DIB_V2));

    // Read additional NDAS Device location informations at 
    // NDAS_BLOCK_LOCATION_ADD_BIND incase of more than 32 NDAS Unit devices exist 4. 
    // Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 
    // if  NDAS_DIB_V2 information is not acceptable
    if(nDIBSize > sizeof(NDAS_DIB_V2))
    {
        err = conn_rw(conn, CONNIO_CMD_READ, sector_count + NDAS_BLOCK_LOCATION_ADD_BIND, 
            GET_TRAIL_SECTOR_COUNT_V2(DevDIB_V2->u.s.nDiskCount), (char*)(pDIB_V2 +1));
        if (!NDAS_SUCCESS(err)) {
            goto errout;
        }
    }

	if((NMT_SINGLE != DevDIB_V2->u.s.iMediaType) &&  (NMT_MEDIAJUKE != DevDIB_V2->u.s.iMediaType))
    {
        if(DevDIB_V2->u.s.nDiskCount <= DevDIB_V2->u.s.iSequence)
            goto try_v1;

        if(sal_memcmp(DevDIB_V2->UnitDisks[DevDIB_V2->u.s.iSequence].MACAddr,
            conn->lpx_addr.slpx_node, sizeof(LPX_NODE_LEN)) ||
            DevDIB_V2->UnitDisks[DevDIB_V2->u.s.iSequence].UnitNumber != unit)
            goto try_v1;
    }

    err= NDAS_OK;
    goto errout;

try_v1:
    debug_conn(2, "DIB v2 identification failed. Trying V1");
    // Check Signature and Version informations in NDAS_DIB_V1 and translate the NDAS_DIB_V1 to an NDAS_DIB_V2

    // initialize DIB V1
    nDIBSize = sizeof(NDAS_DIB_V2); // maximum 2 disks

    // ensure buffer
    if(NULL == pDIB_V2 || *pnDIBSize < nDIBSize)        
    {
        *pnDIBSize = nDIBSize; // set size needed
        debug_conn(1, "Unsufficient buffer size");        
        err = NDAS_ERROR_BUFFER_OVERFLOW;
        goto errout;
    }

    // write to pDIB_V2 directly
    sal_memset( pDIB_V2, 0 , nDIBSize);

    err = conn_rw(conn, CONNIO_CMD_READ, sector_count + NDAS_BLOCK_LOCATION_DIB_V1, 1, (char*)DevDIB_V1);

    if (!NDAS_SUCCESS(err)) {
        goto errout;
    }

    sal_memcpy(pDIB_V2->u.s.Signature, NDAS_DIB_V2_SIGNATURE, sizeof(NDAS_DIB_V2_SIGNATURE));
    pDIB_V2->u.s.MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
    pDIB_V2->u.s.MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
    pDIB_V2->u.s.sizeXArea = NDAS_BLOCK_SIZE_XAREA;
//    pDIB_V2->u.s.iSectorsPerBit = 0; // no backup information
    pDIB_V2->u.s.sizeUserSpace = sector_count; // - NDAS_BLOCK_SIZE_XAREA; // in case of mirror, use primary disk size

    // Create an NDAS_DIB_V2 as single NDAS Disk Device if the NDAS_DIB_V1 is not acceptable either

    if(IS_NDAS_DIBV1_WRONG_VERSION(*DevDIB_V1) || // no DIB information
        (NDAS_DIB_DISK_TYPE_MIRROR_MASTER != DevDIB_V1->DiskType &&
        NDAS_DIB_DISK_TYPE_MIRROR_SLAVE != DevDIB_V1->DiskType &&
        NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST != DevDIB_V1->DiskType &&
        NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND != DevDIB_V1->DiskType))
    {
       if (IS_NDAS_DIBV1_WRONG_VERSION(*DevDIB_V1))
            debug_conn(2, "DIB V1 is not found-Default to single disk mode");
        pDIB_V2->u.s.iMediaType = NMT_SINGLE;
        pDIB_V2->u.s.iSequence = 0;
        pDIB_V2->u.s.nDiskCount = 1;
        // only 1 unit
        sal_memcpy(pDIB_V2->UnitDisks[0].MACAddr, conn->lpx_addr.slpx_node, LPX_NODE_LEN);
    }
    else
    {
        // pair(2) disks (mirror, aggregation)
        UNIT_DISK_LOCATION *pUnitDiskLocation0, *pUnitDiskLocation1;
        if(NDAS_DIB_DISK_TYPE_MIRROR_MASTER == DevDIB_V1->DiskType ||
            NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST == DevDIB_V1->DiskType)
        {
            pUnitDiskLocation0 = &pDIB_V2->UnitDisks[0];
            pUnitDiskLocation1 = &pDIB_V2->UnitDisks[1];
        }
        else
        {
            pUnitDiskLocation0 = &pDIB_V2->UnitDisks[1];
            pUnitDiskLocation1 = &pDIB_V2->UnitDisks[0];
        }

        // 1st unit
        if(
            0x00 == DevDIB_V1->EtherAddress[0] &&
            0x00 == DevDIB_V1->EtherAddress[1] &&
            0x00 == DevDIB_V1->EtherAddress[2] &&
            0x00 == DevDIB_V1->EtherAddress[3] &&
            0x00 == DevDIB_V1->EtherAddress[4] &&
            0x00 == DevDIB_V1->EtherAddress[5]) 
        {
            // usually, there is no ehteraddress information
            sal_memcpy(pUnitDiskLocation0->MACAddr, conn->lpx_addr.slpx_node, LPX_NODE_LEN);
        }
        else
        {
            sal_memcpy(&pUnitDiskLocation0->MACAddr, DevDIB_V1->EtherAddress, 
                LPX_NODE_LEN);
            pUnitDiskLocation0->UnitNumber = DevDIB_V1->UnitNumber;
        }

       // 2nd unit
        sal_memcpy(pUnitDiskLocation1->MACAddr, DevDIB_V1->PeerAddress, LPX_NODE_LEN);

        pUnitDiskLocation1->UnitNumber = DevDIB_V1->UnitNumber;

        pDIB_V2->u.s.nDiskCount = 2;

        switch(DevDIB_V1->DiskType)
        {
        case NDAS_DIB_DISK_TYPE_MIRROR_MASTER:
            pDIB_V2->u.s.iMediaType = NMT_MIRROR;
            pDIB_V2->u.s.iSequence = 0;
            break;
        case NDAS_DIB_DISK_TYPE_MIRROR_SLAVE:
            pDIB_V2->u.s.iMediaType = NMT_MIRROR;
            pDIB_V2->u.s.iSequence = 1;
            break;
        case NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST:
            pDIB_V2->u.s.iMediaType = NMT_AGGREGATE;
            pDIB_V2->u.s.iSequence = 0;
            break;
        case NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND:
            pDIB_V2->u.s.iMediaType = NMT_AGGREGATE;
            pDIB_V2->u.s.iSequence = 1;
            break;
        default:
            debug_conn(1, "Unsupported disk type in V1 DIB");
            err=NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
            goto errout;
        }
    }

    *pnDIBSize = nDIBSize;

    pDIB_V2->crc32 = crc32_calc((unsigned char *)pDIB_V2,
        sizeof(pDIB_V2->u.bytes_248));
    pDIB_V2->crc32_unitdisks = crc32_calc((unsigned char *)pDIB_V2->UnitDisks,
        sizeof(pDIB_V2->UnitDisks));
//    debug_ndasop(1, "Completed to read V1 DIB info");
    err = NDAS_OK;
errout:
    if (DevDIB_V1)
        sal_free(DevDIB_V1);
    if (DevDIB_V2)
        sal_free(DevDIB_V2);
    return err;
}
#endif /* NDAS_NO_LANSCSI */
