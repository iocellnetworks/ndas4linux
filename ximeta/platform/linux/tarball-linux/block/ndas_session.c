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

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include "ndas_request.h"
#include "scrc32.h"
#include "ndas_session.h"

#if   defined(__LITTLE_ENDIAN)

#define __LITTLE_ENDIAN_BYTEORDER
#define __LITTLE_ENDIAN__

#elif defined(__BIG_ENDIAN)

#define __BIG_ENDIAN_BYTEORDER
#define __BIG_ENDIAN__

#else
#error "byte endian is not specified"
#endif 

#include <lspx/lsp_util.h>
#include <lspx/lsp.h>


#ifdef DEBUG

int	dbg_level_nss = DBG_LEVEL_NSS;

#define dbg_call(l,x...) do { 								\
    if (l <= dbg_level_nss) {								\
        printk("%s|%d|%d|",__FUNCTION__, __LINE__,l); 		\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif


#ifdef DEBUG

#define sal_assert(expr) do { NDAS_BUG_ON(!(expr)); } while(0)
#define sal_debug_print(x...)	do { printk(x); } while(0)
#define sal_debug_println(x...) do { printk(x); printk("\n"); } while (0)
#define sal_error_print(x...) do { printk(x); printk("\n"); } while(0)

#else

#define sal_assert(expr) do {} while(0)
#define sal_error_print(x...) do {} while(0)
#define sal_debug_println_impl(file, line, function, x...) do {} while(0)
#define sal_debug_println(x...) do { } while (0)
#define sal_debug_print(x...) do {} while(0)

#endif

C_ASSERT( 0, sizeof(unsigned long long) == 8 );


bool conn_is_connected(uconn_t *conn)
{
    if (conn->socket == NULL) {
 
		return FALSE;
	}

    return NDAS_SUCCESS(lpx_is_connected(conn->socket)) ? TRUE:FALSE;
}

static ndas_error_t conn_convert_lsp_status_to_ndas_error(lsp_status_t status)
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

		return NDAS_ERROR_INVALID_HANDLE;

	default: {

		lsp_status_detail_t detail;
        detail.status = status;

		dbg_call( 1, "lsp error code %x(R=%x,T=%x,S=%x,F=%x) not converted\n", status,
                     detail.detail.response, detail.detail.type, detail.detail.sequence, detail.detail.function );

		sal_assert(0);

		return NDAS_ERROR;
	}
    }
}

static ndas_error_t conn_transport_process_transfer(uconn_t* conn, struct iovec *iov, size_t iov_num)
{
    int  err;
    char *buffer;

    lsp_uint32_t len_buffer;
    lsp_status_t status;

    struct socket *sock = conn->socket;

    int i;
 

    for (status = conn->lsp_status; true; status = lsp_process_next(conn->lsp_handle)) {

		dbg_call( 5, "status = %d\n", status );

        conn->lsp_status = status;

        switch (status) {

            case LSP_REQUIRES_SEND_INTERNAL_DATA: {

            	dbg_call( 4, "LSP_REQUIRES_SEND_INTERNAL_DATA\n" );

				buffer = lsp_get_buffer_to_send( conn->lsp_handle, &len_buffer );

				dbg_call( 4,"Sending internal data %p, %d\n", buffer, len_buffer );

				err = lpx_sock_send( sock, buffer, len_buffer, 0 );

		    	if (len_buffer != err) {

					return NDAS_ERROR_NETWORK_FAIL;
            	}

                break;
            }

            case LSP_REQUIRES_SEND_USER_DATA: {

	            dbg_call( 4, "LSP_REQUIRES_SEND_USER_DATA\n" );

                buffer = lsp_get_buffer_to_send( conn->lsp_handle, &len_buffer );

                dbg_call( 4,"Sending user data %p, %d\n", buffer, len_buffer );

                if (conn->hwdata->data_encryption_algorithm) {

	                for ( i = 0; i < iov_num; i++) {

                        lsp_encrypt_send_data( conn->lsp_handle, conn->encrypt_buffer, iov[i].iov_base, iov[i].iov_len );

                        err = lpx_sock_send( sock, conn->encrypt_buffer, iov[i].iov_len, 0 );

	                    if (iov[i].iov_len != err) {

    	                    return NDAS_ERROR_NETWORK_FAIL;
        	            }
            	    }
				
				} else {

#if 1
	                for ( i = 0; i < iov_num; i++) {

                        err = lpx_sock_send( sock, iov[i].iov_base, iov[i].iov_len, 0 );

	                    if (iov[i].iov_len != err) {

    	                    return NDAS_ERROR_NETWORK_FAIL;
        	            }
            	    }
#else
					err = lpx_sock_sendmsg( sock, iov, iov_num, len_buffer, 0 );

    	            if (len_buffer != err) {

        	            dbg_call( 1,"NDAS_ERROR_NETWORK_FAIL\n" );
            	        return NDAS_ERROR_NETWORK_FAIL;
                	}
#endif
                }

                break;
            }

            case LSP_REQUIRES_RECEIVE_INTERNAL_DATA: {

	            dbg_call( 4, "LSP_REQUIRES_RECEIVE_INTERNAL_DATA\n" );

                buffer = lsp_get_buffer_to_receive( conn->lsp_handle, &len_buffer );

                dbg_call( 2,"Receiving internal data %p, %d\n", buffer, len_buffer );

                err = lpx_sock_recv( sock, buffer, len_buffer, 0 );

                if (len_buffer != err) {

                    dbg_call( 1,"NDAS_ERROR_NETWORK_FAIL\n" );
                    return NDAS_ERROR_NETWORK_FAIL;
                }

                break;
            }

            case LSP_REQUIRES_RECEIVE_USER_DATA: {

                dbg_call( 4, "LSP_REQUIRES_RECEIVE_USER_DATA\n" );

                buffer = lsp_get_buffer_to_receive( conn->lsp_handle, &len_buffer );

                if (conn->hwdata->data_encryption_algorithm) {

	                for ( i = 0; i < iov_num; i++) {

                        err = lpx_sock_recv( sock, conn->encrypt_buffer, iov[i].iov_len, 0 );

	                    if (iov[i].iov_len != err) {

    	                    return NDAS_ERROR_NETWORK_FAIL;
        	            }

						dbg_call( 2,"Receiving lsp_decrypt_recv_data %p, %d\n", buffer, len_buffer );

						lsp_decrypt_recv_data( conn->lsp_handle, iov[i].iov_base, conn->encrypt_buffer, iov[i].iov_len );
            	    }
				
				} else {

					err = lpx_sock_recvmsg( sock, iov, iov_num, len_buffer, 0 );

    	            if (len_buffer != err) {

        		    	return NDAS_ERROR_NETWORK_FAIL;
                	}
                }

                break;
            }

            case LSP_REQUIRES_DATA_DECODE: {

                dbg_call( 4, "LSP_REQUIRES_DATA_DECODE\n" );
                break;
            }

            case LSP_REQUIRES_DATA_ENCODE: {

                dbg_call( 4, "LSP_REQUIRES_DATA_ENCODE\n" );
                break;
            }

            case LSP_REQUIRES_SYNCHRONIZE: {

                dbg_call( 4, "LSP_REQUIRES_SYNCHRONIZE\n" );
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

                return conn_convert_lsp_status_to_ndas_error(status);
            }
        }
    }
}

ndas_error_t conn_rw_vector(uconn_t *conn, int cmd, sector_t start_sec, unsigned long num_sec, struct iovec *iov, size_t iov_num) 
{
    ndas_error_t err = NDAS_OK;

	size_t	iov_idx;
	size_t	iov_buf_offset;
	
    __u32	transfered_sectors = 0;
    __u32	split_sector_size;

	int		retrial;

	unsigned long	start_time;

#ifdef DEBUG
    int 	i;
#endif

	NDAS_BUG_ON( conn->max_split_bytes != 64*1024 && conn->max_split_bytes != 32*1024 );

	if (cmd == NDAS_CMD_WRITE) {
	
		split_sector_size = NdasFcChooseTransferSize( &conn->SendNdasFcStatistics, 
													  (num_sec * SECTOR_SIZE < conn->max_split_bytes) ? 
													  num_sec * SECTOR_SIZE : conn->max_split_bytes );

	} else if (cmd == NDAS_CMD_READ) {
	
		split_sector_size = NdasFcChooseTransferSize( &conn->RecvNdasFcStatistics, 
													  (num_sec * SECTOR_SIZE < conn->max_split_bytes) ? 
													  num_sec * SECTOR_SIZE : conn->max_split_bytes );
	
	} else {

		split_sector_size = 0;
	}

	NDAS_BUG_ON( split_sector_size > conn->max_split_bytes );

	split_sector_size /= SECTOR_SIZE;

	start_time = jiffies;

    dbg_call( 4, "ing conn=%p, start_sec=%llu, sec_len=%ld, split size=%d\n", 
    		  	  conn, (long long)start_sec, num_sec, split_sector_size ); 

#ifdef DEBUG
    for (i=0; i<iov_num; i++) {

        dbg_call( 4, "ioreq.uio[%d]: %p:%p\n", i, iov[i].iov_base, iov[i].iov_base+iov[i].iov_len );
    }
#endif

	retrial = 0;

retry:

    transfered_sectors = 0;

	iov_idx = 0;
	iov_buf_offset = 0;

    do {

		unsigned long to_transfer;

#define CONN_IO_MAX_SEGMENT 32

	    struct iovec temp_iov[CONN_IO_MAX_SEGMENT];
		size_t		 temp_iov_num;

		int i;
		int copied;
		int	total_copied;

        dbg_call( 4, "req->num_sec=%ld, split_size=%d\n", num_sec, split_sector_size );

        to_transfer = (num_sec - transfered_sectors <= split_sector_size) ? (num_sec - transfered_sectors) : split_sector_size;

		total_copied = 0;
		temp_iov_num = 0;
		
		for (i=iov_idx; i<iov_num; i++) {

			temp_iov[temp_iov_num].iov_base = (char *)(iov[i].iov_base) + iov_buf_offset;

			copied = iov[i].iov_len - iov_buf_offset;
			
			if ((total_copied + copied) > to_transfer*SECTOR_SIZE) {
		
				copied = to_transfer*SECTOR_SIZE - total_copied;

				iov_buf_offset += copied;

			} else {

				iov_buf_offset = 0;
			}

			total_copied += copied;
			
			temp_iov[temp_iov_num].iov_len = copied;

			temp_iov_num ++;

			if (total_copied == to_transfer*SECTOR_SIZE) {

				break;
			}
		}

		iov_idx = i;

		if (iov_buf_offset == 0) {

			iov_idx ++;
		}

		NDAS_BUG_ON( total_copied != to_transfer*SECTOR_SIZE );

        switch(cmd) {

        case NDAS_CMD_WRITE: {

            const lsp_large_integer_t lsp_start_sec = {

                .quad = start_sec + transfered_sectors
            };

			char	buf;

            dbg_call( 4, "Writing start=%llu, len=%ld sectors\n", (long long)start_sec + transfered_sectors, to_transfer ); 

            conn->lsp_status = lsp_ide_write( conn->lsp_handle, &lsp_start_sec, 
											  to_transfer, (void*)&buf, to_transfer * SECTOR_SIZE );

            break;
        }

        case NDAS_CMD_READ: {

            const lsp_large_integer_t lsp_start_sec = {

                .quad = start_sec + transfered_sectors
            };

			char	buf;

            dbg_call( 4, "Reading start=%llu, len=%ld\n", (long long)start_sec + transfered_sectors, to_transfer ); 

            conn->lsp_status = lsp_ide_read( conn->lsp_handle, &lsp_start_sec, to_transfer, (void*)&buf, to_transfer * SECTOR_SIZE );

            break;
        }
        case NDAS_CMD_FLUSH: {

            const lsp_large_integer_t lsp_start_sec = {

                .quad = 0
            };

            dbg_call( 1, "Flushing\n" );

            conn->lsp_status = lsp_ide_flush( conn->lsp_handle, &lsp_start_sec, 0, NULL, 0 );

            break;
        }

        default:

            sal_error_print( "connio: Invalid command 0x%x\n", cmd );
            break;
        }

		if (cmd == NDAS_CMD_FLUSH) {
	
	        err = conn_transport_process_transfer( conn, NULL, 0 );
		
		} else {

	        err = conn_transport_process_transfer( conn, temp_iov, temp_iov_num );
		}

        if (!NDAS_SUCCESS(err)) {

            dbg_call( 4, "conn_transport_process_transfer() failed. %d\n", err );
 
           if (err == NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED) {

                retrial++;

                if (retrial < 5) {

                    goto retry;
                }
            }

            dbg_call( 1, "Fatal error. Stopping IO\n" );

            break;
        }

        transfered_sectors += to_transfer;

    } while(transfered_sectors < num_sec);

	if (cmd == NDAS_CMD_WRITE) {
	
		NdasFcUpdateTrasnferSize( &conn->SendNdasFcStatistics,
								  split_sector_size * SECTOR_SIZE,
								  num_sec * SECTOR_SIZE,
								  start_time,
								  jiffies );
		
	} else if (cmd == NDAS_CMD_READ) {
	
		NdasFcUpdateTrasnferSize( &conn->RecvNdasFcStatistics,
								  split_sector_size * SECTOR_SIZE,
								  num_sec * SECTOR_SIZE,
								  start_time,
								  jiffies );
	
	}

    dbg_call( 3, "done sec_len=%ld\n", num_sec );

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
        dbg_call( 1, "HW does not support device lock\n" );
        return NDAS_ERROR_UNSUPPORTED_FEATURE;
    } else if (conn->hwdata->hardware_version != LSP_HARDWARE_VERSION_1_1 && 
        conn->hwdata->hardware_version != LSP_HARDWARE_VERSION_2_0) {
        dbg_call( 1, "unsupported SW version\n" );
        return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
    }

    request.type = LSP_REQUEST_VENDOR_COMMAND;
    vc_request = &request.u.vendor_command.request;
    vc_request->vendor_id = LSP_VENDOR_ID_XIMETA;
    vc_request->vop_ver = LSP_VENDOR_OP_VERSION_1_0;

#if   defined(__LITTLE_ENDIAN)
    param[4] = lockid;
#elif defined(__BIG_ENDIAN)
    param[3] = lockid;
#else
#error "byte endian is not specified"
#endif

    memcpy (vc_request->param, param, 8 );

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
                dbg_call( 4, "%d lock data=%x\n", lockid, *((int*) lockdata) );
//                dbg_call(1, "%d lock data=%x\n", lockid, *((int*) lockdata));		
            }
        }

		if (err == NDAS_OK) {

			conn->ndas_locked[lockid] = true;
		}

        dbg_call( 4, "%d LOCK TAKE returning %d\n", lockid, err );
//        dbg_call(1, "%d LOCK TAKE returning %d\n", lockid, err);
        break;
    case NDAS_LOCK_GIVE:
        vc_request->vop_code = LSP_VCMD_FREE_SEMA;
        conn->lsp_status = lsp_request(conn->lsp_handle, &request);
        err = conn_transport_process_transfer(conn, NULL, 0);
        if (err == NDAS_OK) {
            lsp_get_vendor_command_result(conn->lsp_handle, (lsp_uint8_t*) param, 8);
            if (lockdata) {
                *((int*) lockdata) = lsp_ntohl(*((int*)&param[4]));
                dbg_call(4, "%d lock data=%x\n", lockid, *((int*) lockdata));
                dbg_call(1, "%d lock data=%x\n", lockid, *((int*) lockdata));
            }
        }
        dbg_call(4, "%d LOCK GIVE returning %d\n", lockid, err);
//        dbg_call(1, "%d LOCK GIVE returning %d\n", lockid, err);

		conn->ndas_locked[lockid] = false;
        break;

    case NDAS_LOCK_GET_COUNT:
        vc_request->vop_code = LSP_VCMD_GET_SEMA;
        conn->lsp_status = lsp_request(conn->lsp_handle, &request);
        err = conn_transport_process_transfer(conn, NULL, 0);
        if (err == NDAS_OK) {
            lsp_get_vendor_command_result(conn->lsp_handle, (lsp_uint8_t*) param, 8);
            *(int*)lockdata = (((__u32)param[4])<<24)+ (((__u32)param[5])<<16)+ (((__u32)param[6])<<8) + (__u32)param[7];
        }
        break;

    case NDAS_LOCK_GET_OWNER:
        vc_request->vop_code = LSP_VCMD_GET_OWNER_SEMA;        
        conn->lsp_status = lsp_request(conn->lsp_handle, &request);
        err = conn_transport_process_transfer(conn, NULL, 0);
        if (err == NDAS_OK) {
            lsp_get_vendor_command_result(conn->lsp_handle, (lsp_uint8_t*) param, 8);
            memcpy(lockdata, &param[2], 6);
        } else {
            memset(lockdata, 0, 6);
        }
        break;

    case NDAS_LOCK_BREAK:
        {
            uconn_t* conn_array;
            uconn_t* current_conn;            
            dbg_call(1, "NDAS_LOCK_BREAK for lock %d\n", lockid);
            if (!(conn->hwdata->hardware_version == LSP_HARDWARE_VERSION_2_0 &&
                conn->hwdata->hardware_revision == LSP_HARDWARE_V20_REV_0)) {
                err = NDAS_ERROR_UNSUPPORTED_FEATURE;
                /* This lock break mechanism is workaround for 2.0 chip bug. This method does not work on other versions */
                break;
            }
            
            conn_array = ndas_kmalloc(sizeof(uconn_t) * (NDAS_MAX_CONNECTION_COUNT-1));
            if (!conn_array) {
                err = NDAS_ERROR_OUT_OF_MEMORY;
                break;
            }
            
            for (i=0;i<NDAS_MAX_CONNECTION_COUNT-1;i++) {
                current_conn = &(conn_array[i]);
                err  = conn_init(current_conn, current_conn->lpx_addr.slpx_node, 0, NDAS_CONN_MODE_READ_ONLY);
                if ( !NDAS_SUCCESS(err)) {
                    continue;
                }
                
                err = conn_connect(current_conn, NDAS_RECONNECT_TIMEOUT, 1);
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
conn_text_target_data(uconn_t *conn, bool is_set, TEXT_TARGET_DATA* data)
{
    ndas_error_t ret;
    lsp_text_target_data_t tdata;
    __u64 target_data;

#ifdef DEBUG
    if (is_set){
        dbg_call(4, "Setting target data: %llx\n", *data);
    }
#endif

    memset(&tdata, 0, sizeof(tdata));
    tdata.type = LSP_TEXT_BINPARAM_TYPE_TARGET_DATA;
    tdata.to_set = (lsp_uint8_t) is_set;
    // Host endian to network endian
    target_data = lsp_ntohll(*data);
    memcpy(tdata.target_data, &target_data, sizeof(TEXT_TARGET_DATA));
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
        memcpy(data, tdata.target_data, sizeof(TEXT_TARGET_DATA));
		// Network endian to host endian.
        *data = lsp_ntohll(*data);
#ifdef DEBUG
        dbg_call(4, "Got target data: %llx\n", *data);
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
        dbg_call( 1, "Buffer is not enough\n" );
        sal_assert(0);
        return NDAS_ERROR_BUFFER_OVERFLOW;
    }

    memset(list, 0, sizeof(list));
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
conn_rw(
    uconn_t *conn, int cmd, __u64 start_sec, __u32 num_sec, char* buf
) {
    struct iovec reqmem_block[1];
    ndas_error_t	err;

    //
    // Single chunk of data pointer is provided instead of memblock array.
    // Convert to memblock array.
    //
    dbg_call( 3, "buf=%p\n", buf ); 
    reqmem_block[0].iov_len = num_sec << 9;
    reqmem_block[0].iov_base = buf;

	err = conn_rw_vector( conn, cmd, start_sec, num_sec, reqmem_block, 1 );

	return err;
}

ndas_error_t conn_read_dib(uconn_t *conn, __u64 sector_count, NDAS_DIB_V2 *pDIB_V2, __u32*pnDIBSize)
{
    ndas_error_t 	err;
    NDAS_DIB* 		DevDIB_V1 = NULL;
    NDAS_DIB_V2* 	DevDIB_V2 = NULL;
    __u32 			nDIBSize;
    int 			unit;
    
    dbg_call( 1, "conn_read_dib conn=%p sector_count= %llu\n", conn, sector_count );

    unit = conn->unit;        // to do: get from sdev info

    DevDIB_V1 = ndas_kmalloc(sizeof(NDAS_DIB));

    if (DevDIB_V1 == NULL)  {

		NDAS_BUG_ON(true);
        err = NDAS_ERROR_OUT_OF_MEMORY;
        goto errout;
    }

    DevDIB_V2 = ndas_kmalloc(sizeof(NDAS_DIB_V2));

    if (DevDIB_V2==NULL)  {

		NDAS_BUG_ON(true);
        err = NDAS_ERROR_OUT_OF_MEMORY;
        goto errout;
    }

    sal_assert( sizeof(NDAS_DIB) == SECTOR_SIZE );
    sal_assert( sizeof(NDAS_DIB_V2) == SECTOR_SIZE );
    
    err = conn_rw( conn, NDAS_CMD_READ, sector_count + NDAS_BLOCK_LOCATION_DIB_V2, 1, (char*)DevDIB_V2 );

    if (!NDAS_SUCCESS(err)) {

		NDAS_BUG_ON(true);
        dbg_call( 1, "conn_read_dib failed: %d\n", err );
        goto errout;
    }

    // Check Signature, Version and CRC informations in NDAS_DIB_V2 and accept if all the informations are correct    

    if(memcmp(NDAS_DIB_V2_SIGNATURE, DevDIB_V2->u.s.Signature,8) != 0) {

		int i;
		
		for (i=0; i<8; i++) {

			dbg_call( 1, "DevDIB_V2->u.s.Signature[i] = %x\n ", DevDIB_V2->u.s.Signature[i] );
		}
			
        dbg_call( 1, "DIB V2 signature mismatch\n" );
        goto try_v1;
    }

    if (DevDIB_V2->crc32 != crc32_calc((unsigned char *)DevDIB_V2, sizeof(DevDIB_V2->u.bytes_248))) {

        goto try_v1;
	}

    if (DevDIB_V2->crc32_unitdisks != crc32_calc((unsigned char *)DevDIB_V2->UnitDisks,    sizeof(DevDIB_V2->UnitDisks))) {

        goto try_v1;
	}

    if (DevDIB_V2->u.s.iMediaType != NMT_MEDIAJUKE) {

	    if (NDAS_BLOCK_SIZE_XAREA != DevDIB_V2->u.s.sizeXArea &&
	        NDAS_BLOCK_SIZE_XAREA * SECTOR_SIZE != DevDIB_V2->u.s.sizeXArea){
	 
			NDAS_BUG_ON(true);
			dbg_call(1,"DIB V2 XArea mispatch!!!\n");    	
	        goto try_v1;
	    }

	    if(DevDIB_V2->u.s.sizeUserSpace > sector_count) { /* Invalid sector count is recorded?? */

	       dbg_call(1,"DIB V2 UserSpace!!!\n");    	
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
        memcpy(pDIB_V2, DevDIB_V2, sizeof(NDAS_DIB_V2));
        err= NDAS_OK;
        dbg_call(1,"NMT_MEDIAJUKE!!!\n");   
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
        dbg_call(1, "Unsupported DIB version %d.%d\n", DevDIB_V2->u.s.MajorVersion, DevDIB_V2->u.s.MinorVersion);
        err = NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;
        goto errout;
    }

    nDIBSize = (GET_TRAIL_SECTOR_COUNT_V2(DevDIB_V2->u.s.nDiskCount) + 1) 
        * sizeof(NDAS_DIB_V2);

    if(NULL == pnDIBSize) {// copy 1 sector only
        memcpy(pDIB_V2, DevDIB_V2, sizeof(NDAS_DIB_V2));
        err= NDAS_OK;
        goto errout;
    }

    if(*pnDIBSize < nDIBSize) { // make sure there is enough space
        *pnDIBSize = nDIBSize; // do not copy, possibly just asking size
        err= NDAS_OK;
        goto errout;
    }
    
    memcpy(pDIB_V2, DevDIB_V2, sizeof(NDAS_DIB_V2));

    // Read additional NDAS Device location informations at 
    // NDAS_BLOCK_LOCATION_ADD_BIND incase of more than 32 NDAS Unit devices exist 4. 
    // Read an NDAS_DIB_V1 information at NDAS_BLOCK_LOCATION_DIB_V1 
    // if  NDAS_DIB_V2 information is not acceptable
    if(nDIBSize > sizeof(NDAS_DIB_V2))
    {
        err = conn_rw(conn, NDAS_CMD_READ, sector_count + NDAS_BLOCK_LOCATION_ADD_BIND, 
            GET_TRAIL_SECTOR_COUNT_V2(DevDIB_V2->u.s.nDiskCount), (char*)(pDIB_V2 +1));
        if (!NDAS_SUCCESS(err)) {
            goto errout;
        }
    }

	if((NMT_SINGLE != DevDIB_V2->u.s.iMediaType) &&  (NMT_MEDIAJUKE != DevDIB_V2->u.s.iMediaType))
    {
        if(DevDIB_V2->u.s.nDiskCount <= DevDIB_V2->u.s.iSequence)
            goto try_v1;

        if(memcmp(DevDIB_V2->UnitDisks[DevDIB_V2->u.s.iSequence].MACAddr,
            conn->lpx_addr.slpx_node, sizeof(LPX_NODE_LEN)) ||
            DevDIB_V2->UnitDisks[DevDIB_V2->u.s.iSequence].UnitNumber != unit)
            goto try_v1;
    }

    err= NDAS_OK;
    goto errout;

try_v1:
    dbg_call( 1, "DIB v2 identification failed. Trying V1\n" );
    // Check Signature and Version informations in NDAS_DIB_V1 and translate the NDAS_DIB_V1 to an NDAS_DIB_V2

    // initialize DIB V1
    nDIBSize = sizeof(NDAS_DIB_V2); // maximum 2 disks

    // ensure buffer
    if (NULL == pDIB_V2 || *pnDIBSize < nDIBSize) {

        *pnDIBSize = nDIBSize; // set size needed
        dbg_call(1, "Unsufficient buffer size\n");        
        err = NDAS_ERROR_BUFFER_OVERFLOW;
        goto errout;
    }

    // write to pDIB_V2 directly
    memset( pDIB_V2, 0 , nDIBSize);

    err = conn_rw(conn, NDAS_CMD_READ, sector_count + NDAS_BLOCK_LOCATION_DIB_V1, 1, (char*)DevDIB_V1);

    if (!NDAS_SUCCESS(err)) {

        goto errout;
    }

    memcpy(pDIB_V2->u.s.Signature, NDAS_DIB_V2_SIGNATURE, sizeof(NDAS_DIB_V2_SIGNATURE));
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
            dbg_call(2, "DIB V1 is not found-Default to single disk mode\n");
        pDIB_V2->u.s.iMediaType = NMT_SINGLE;
        pDIB_V2->u.s.iSequence = 0;
        pDIB_V2->u.s.nDiskCount = 1;
        // only 1 unit
        memcpy(pDIB_V2->UnitDisks[0].MACAddr, conn->lpx_addr.slpx_node, LPX_NODE_LEN);
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
            memcpy(pUnitDiskLocation0->MACAddr, conn->lpx_addr.slpx_node, LPX_NODE_LEN);
        }
        else
        {
            memcpy(&pUnitDiskLocation0->MACAddr, DevDIB_V1->EtherAddress, 
                LPX_NODE_LEN);
            pUnitDiskLocation0->UnitNumber = DevDIB_V1->UnitNumber;
        }

       // 2nd unit
        memcpy(pUnitDiskLocation1->MACAddr, DevDIB_V1->PeerAddress, LPX_NODE_LEN);

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
            dbg_call(1, "Unsupported disk type in V1 DIB\n");
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
        ndas_kfree(DevDIB_V1);
    if (DevDIB_V2)
        ndas_kfree(DevDIB_V2);

    dbg_call( 1, "conn_read_dib ed\n" );

    return err;
}

ndas_error_t conn_handshake(uconn_t *conn)
{
    ndas_error_t err;
 
    conn->lsp_status = lsp_ata_handshake( conn->lsp_handle );
    err = conn_transport_process_transfer( conn, NULL, 0 );

    return err;
}

void conn_logout(uconn_t *conn) {

    dbg_call( 1, "sock=%p\n", conn->socket );

    NDAS_BUG_ON( conn->lsp_handle == NULL );

    conn->lsp_status = lsp_logout(conn->lsp_handle);
    conn_transport_process_transfer(conn, NULL, 0);
}

void conn_disconnect(uconn_t *conn) 
{
    dbg_call( 1, "sock=%p\n", conn->socket );

    if (conn->socket) {

        lpx_sock_close( conn->socket );
        conn->socket = NULL;
    }

    conn->status = CONN_STATUS_INIT;

    dbg_call( 2, "status = CONN_STATUS_INIT\n" );
}

extern int udev_max_xfer_unit;

ndas_error_t conn_connect(uconn_t *conn, unsigned long jiffies_timeout, int try_connection)
{
    ndas_error_t	err = NDAS_ERROR;

    lsp_login_info_t 	lsp_login_info;
    struct sockaddr_lpx bindaddr;

	int	i;

    dbg_call( 3, "conn=%p\n" , conn );
    dbg_call( 1, "Connecting via LANSCSI for %s:0x%x status=%d, unit=%d\n",
				 NDAS_DEBUG_HEXDUMP(conn->lpx_addr.slpx_node, 6),
        		 conn->lpx_addr.slpx_port, conn->status, conn->unit );

    NDAS_BUG_ON( conn->status != CONN_STATUS_INIT );
    NDAS_BUG_ON( try_connection == 0 );

	for (i=0; i<try_connection; i++) {

	    err = lpx_sock_create( LPX_TYPE_STREAM, 0, &conn->socket );

    	dbg_call( 1, "sock=%p\n", conn->socket );

 	   if (err < 0) {

			NDAS_BUG_ON(true);
        	goto errout;
		}

		memset( &bindaddr, 0, sizeof(bindaddr) ); 

		bindaddr.slpx_family = AF_LPX;
 
	    err = lpx_sock_bind( conn->socket, &bindaddr, sizeof(bindaddr) );

    	if (!NDAS_SUCCESS(err)) {
		
        	dbg_call( 1, "bind failed for sock %p:%d\n", conn->socket, err );
			NDAS_BUG_ON(true);
 
			goto errout;
    	}

		err = lpx_sock_connect( conn->socket, &conn->lpx_addr, sizeof(conn->lpx_addr) );

		if (NDAS_SUCCESS(err)) {

			break;
    	}

        conn_disconnect(conn);

		msleep( msecs_to_jiffies(2000) );
	}

	if (!NDAS_SUCCESS(err)) {

       	err = NDAS_ERROR_CONNECT_FAILED;
       	goto errout;
   	}

    conn->reconnect_timeout = jiffies_timeout;

    /* default login values */

    memset( &lsp_login_info, 0, sizeof(lsp_login_info) );

    if (conn->mode == NDAS_CONN_MODE_SUPERVISOR) {

        const lsp_uint8_t LSP_LOGIN_PASSWORD_DEFAULT_SUPERVISOR[8] = { 0x1E, 0x13, 0x50, 0x47, 0x1A, 0x32, 0x2B, 0x3E};

        memcpy( lsp_login_info.supervisor_password, 
                LSP_LOGIN_PASSWORD_DEFAULT_SUPERVISOR, sizeof(LSP_LOGIN_PASSWORD_DEFAULT_SUPERVISOR) );

    } else {

        memcpy(lsp_login_info.password, LSP_LOGIN_PASSWORD_ANY, sizeof(LSP_LOGIN_PASSWORD_ANY));
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

    conn->lsp_status = lsp_login( conn->lsp_handle, &lsp_login_info );

    err = conn_transport_process_transfer(conn, NULL, 0);

    dbg_call( 1, "lsp_login err = %d\n", err );

    if (NDAS_SUCCESS(err)) {

        conn->hwdata = lsp_get_hardware_data(conn->lsp_handle);

        conn->max_transfer_size = conn->hwdata->maximum_transfer_blocks * SECTOR_SIZE;

	    dbg_call( 1, "maximum_transfer_blocks = %d\n", conn->hwdata->maximum_transfer_blocks );

        if (conn->hwdata->data_encryption_algorithm) {

			NDAS_BUG_ON( conn->encrypt_buffer );

            conn->encrypt_buffer = ndas_kmalloc(conn->max_transfer_size);

            if (conn->encrypt_buffer == NULL) {

				NDAS_BUG_ON( true );
                err = NDAS_ERROR_OUT_OF_MEMORY;
                goto errout;
            }
        }

        if (conn->max_transfer_size > udev_max_xfer_unit) {

            conn->max_split_bytes = udev_max_xfer_unit;

        } else {

            conn->max_split_bytes = conn->max_transfer_size;
        }

		NDAS_BUG_ON( conn->max_split_bytes != 64*1024 && conn->max_split_bytes != 32*1024 );

        dbg_call( 1, "Using max split bytes: %dk\n", conn->max_split_bytes/1024 );

	    conn->status = CONN_STATUS_CONNECTED;

        return NDAS_OK;
    }

errout:

    if (conn->socket != NULL) {

        conn_disconnect(conn);
    }
    return err;
}

ndas_error_t conn_reinit(uconn_t *conn)
{
    dbg_call( 1, "conn=%p\n", conn );

    if (conn->encrypt_buffer) {

        // Encrypt buffer size may need to be changed. Free here and let it being created again

        ndas_kfree(conn->encrypt_buffer);
        conn->encrypt_buffer = NULL;
    }

    conn->lsp_handle = lsp_initialize_session(conn->lsp_session_buffer, LSP_SESSION_BUFFER_SIZE );

    lsp_set_options( conn->lsp_handle, 
					 LSP_SO_USE_EXTERNAL_DATA_ENCODE | LSP_SO_USE_EXTERNAL_DATA_DECODE | LSP_SO_USE_DISTINCT_SEND_RECEIVE );

	NdasFcInitialize( &conn->SendNdasFcStatistics );
	NdasFcInitialize( &conn->RecvNdasFcStatistics );

    conn->status = CONN_STATUS_INIT;

    return NDAS_OK;
}

ndas_error_t conn_init(uconn_t *conn, __u8 *node, __u32 unit, ndas_conn_mode_t mode)
{
    dbg_call( 1,"conn=%p\n", conn );

    memset( conn, 0, sizeof(uconn_t) );

    conn->lpx_addr.slpx_family = AF_LPX;

    memcpy( conn->lpx_addr.slpx_node, node, sizeof(conn->lpx_addr.slpx_node) );
    conn->lpx_addr.slpx_port = htons(NDDEV_LPX_PORT_NUMBER);

    conn->unit = unit;
    conn->mode = mode;

    conn->lsp_session_buffer = ndas_kmalloc(LSP_SESSION_BUFFER_SIZE);

    conn->lsp_handle = lsp_initialize_session( conn->lsp_session_buffer, LSP_SESSION_BUFFER_SIZE );

    lsp_set_options( conn->lsp_handle,
        			 LSP_SO_USE_EXTERNAL_DATA_ENCODE | LSP_SO_USE_EXTERNAL_DATA_DECODE | LSP_SO_USE_DISTINCT_SEND_RECEIVE );

	NdasFcInitialize( &conn->SendNdasFcStatistics );
	NdasFcInitialize( &conn->RecvNdasFcStatistics );

    conn->status = CONN_STATUS_INIT;

    return NDAS_OK;
}

void conn_clean(uconn_t *conn) 
{
    if (conn->lsp_session_buffer) {

        ndas_kfree( conn->lsp_session_buffer );
        conn->lsp_session_buffer = NULL;
        conn->lsp_handle = NULL;
    }

    if (conn->encrypt_buffer) {

        ndas_kfree( conn->encrypt_buffer );
        conn->encrypt_buffer = NULL;
    }

    conn->status = CONN_STATUS_INIT;
}
