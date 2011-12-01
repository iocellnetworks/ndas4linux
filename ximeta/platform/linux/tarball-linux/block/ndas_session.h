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

#ifndef _NDAS_SESSION_H_
#define _NDAS_SESSION_H_

#include <ndas_dev.h>
#include <ndas_lpx.h>

#include "ndas_request.h"

#include "ndasdib.h"

#if   defined(__LITTLE_ENDIAN)

#define __LITTLE_ENDIAN_BYTEORDER
#define __LITTLE_ENDIAN__

#elif defined(__BIG_ENDIAN)

#define __BIG_ENDIAN_BYTEORDER
#define __BIG_ENDIAN__

#else
#error "byte endian is not specified"
#endif 

#include <lspx/lsp_type.h>
#include <lspx/lsp.h>

#include "ndasflowcontrol.h"

#define NDDEV_LPX_PORT_NUMBER     	10000


#define NDAS_MAX_CONNECTION_COUNT   64
#define NDAS_RECONNECT_TIMEOUT  	msecs_to_jiffies(180*MSEC_PER_SEC)	// 180 second

// status codes for uconn_s.err

#define CONN_STATUS_INIT            0x0001
#define CONN_STATUS_CONNECTING      0x0010
#define CONN_STATUS_CONNECTED       0x0020
#define CONN_STATUS_SHUTING_DOWN    0x1000 

typedef enum ndas_conn_mode_s {

    NDAS_CONN_MODE_EXCLUSIVE_WRITE, 
    NDAS_CONN_MODE_WRITE_SHARE, /* Log in write-share mode(actually read-only connection for ~2.0) and use lock */
    NDAS_CONN_MODE_LOCKED_READ, /* Use lock for increase aggregated performance in multiple connection case */
    NDAS_CONN_MODE_READ_ONLY,
    NDAS_CONN_MODE_DISCOVER, /* No IO. Log in for status checking/discovery */
    NDAS_CONN_MODE_SUPERVISOR

} ndas_conn_mode_t;

typedef __u64 TEXT_TARGET_DATA, *PTEXT_TARGET_DATA;

typedef struct uconn_s {

    int unit;  // Unit number in device. 0 for primary, 1 for secondary.

    ndas_conn_mode_t mode;

	struct socket		*socket;
    struct sockaddr_lpx lpx_addr;

    void		 	*lsp_session_buffer;
    lsp_handle_t 	lsp_handle;
    lsp_status_t	lsp_status;

    const lsp_hardware_data_t* hwdata;

    char *encrypt_buffer;

    unsigned int	status; // CONN_STATUS_xxxx
    unsigned long	reconnect_timeout;

    int 	max_transfer_size;
    int 	max_split_bytes;

	bool	ndas_locked[NDAS_LOCK_COUNT];

	NDAS_FC_STATISTICS	SendNdasFcStatistics;
	NDAS_FC_STATISTICS	RecvNdasFcStatistics;

} uconn_t;


void conn_clean(uconn_t *conn);

ndas_error_t conn_init(uconn_t *conn, __u8* node, __u32 unit, ndas_conn_mode_t mode);
ndas_error_t conn_reinit(uconn_t *conn);

bool conn_is_connected(uconn_t *conn);

ndas_error_t conn_connect(uconn_t *conn, unsigned long jiffies_timeout, int try_connection);

ndas_error_t conn_handshake(uconn_t *conn);
void conn_logout(uconn_t *conn);

void conn_disconnect(uconn_t *conn);


ndas_error_t conn_text_target_data(uconn_t *conn, bool is_set, TEXT_TARGET_DATA* data);
ndas_error_t conn_text_target_list(uconn_t *conn, lsp_text_target_list_t* list, int size);

ndas_error_t conn_lock_operation(uconn_t *conn, NDAS_LOCK_OP op, int lockid, void* data, void* opt);

ndas_error_t conn_rw_vector(uconn_t *conn, int cmd, sector_t start_sec, unsigned long num_sec, struct iovec *iov, size_t iov_num);
ndas_error_t conn_read_dib(uconn_t *conn, __u64 sector_count, NDAS_DIB_V2 *pDIB_V2, __u32 *pnDIBSize);

#ifdef XPLAT_RAID
ndas_error_t conn_read_bitmap(uconn_t *conn, __u64 sector_count, bitmap_t *bitmap);
#endif

#endif /* _NDAS_SESSION_H_ */

