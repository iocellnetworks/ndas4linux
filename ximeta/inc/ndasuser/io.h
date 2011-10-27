/*
 -------------------------------------------------------------------------
 Copyright (c) 2005, XIMETA, Inc., IRVINE, CA, USA.
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
*/
#ifndef _NDASUSER_IO_H_
#define _NDASUSER_IO_H_

#include "sal/sal.h"
#include "sal/mem.h"
#include "ndasuser/def.h"
#include "ndasuser/ndaserr.h"

#define NDAS_IO_REQUEST_INITIALIZER { \
    .start_sec = 0ULL,\
    .num_sec = 0U,\
    .done = NULL,\
    .done_arg = NULL,\
    .buf = NULL,\
    .nr_uio = 0,\
    .uio = NULL\
}

typedef NDAS_CALL void (*ndas_io_done)(int slot, ndas_error_t err,void* user_arg);





#define NDAS_MAX_PC_REQ 16
#define DEFAULT_SENSE_DATA 32

#define DATA_TO_DEV		0
#define DATA_FROM_DEV		1
#define DATA_NONE			2

typedef struct _io_pc_request {
	xuint8 	cmd[NDAS_MAX_PC_REQ];
	xuint16	cmdlen;
	xuint16 	status;
	xuint32	resid;
	xuint8	avaliableSenseData;
	xuint8	useAuxBuf;
	xuint8	direction;
	xuint8	requestSenseData;
	xuint8 	sensedata[DEFAULT_SENSE_DATA];
	xuint32	AuxBufLen;
}nads_io_pc_request, * nads_io_pc_request_ptr;

typedef union _io_add_info {
	struct _ADD_INFO_SET_FEATURE{
		xuint8 sub_com;
		xuint8 sub_com_specific_0;
		xuint8 sub_com_specific_1;
		xuint8 sub_com_specific_2;
		xuint8 sub_com_specific_3;		
	} ADD_INFO_SET_FEATURE;
	struct _ADD_INFO_ATA_PASSTHROUGH{
		xuint8   LBA48;
		xuint8   protocol;
		
		xuint8   hob_feature;
		xuint8   hob_nsec;
		xuint8   hob_lbal;
		xuint8   hob_lbam;
		xuint8   hob_lbah;
		
		xuint8   feature;
		xuint8   nsec;
		xuint8   lbal;
		xuint8   lbam;
		xuint8   lbah;

		xuint8   command;
              xuint8   check_cond;
		xuint8   reserved[2];
	} ADD_INFO_ATA_PASSTHROUGH;
	xuint8 bytes32size[DEFAULT_SENSE_DATA];
}ndas_io_add_info, *ndas_io_add_info_ptr;


typedef struct _io_request {
    xuint64 start_sec;
    xuint16 num_sec;    // Number of sector to copy. The size of one sector is 512
    xuint16 nr_uio;     /* number of array - uio */
	xuint32 reqlen; //added for scsi_io
	void * additionalInfo;
    struct sal_mem_block *uio;  // pointer to mem blocks
   char* buf;            // User supplied buffer. If buf is null, nr_uio and uio is used
    ndas_io_done done;  // signalled I/O, null for blocking call
    void* done_arg;
	ndas_io_done expand_done;
	void * expand_done_arg;
	xbool urgent;
} ndas_io_request, *ndas_io_request_ptr;

/* Description
 * Read raw data from the device identified by slot number
 *  
 * Parameters
 * slot :   Registered slot number.
 * ioreq :  pointer to ndas_io_request structure 
 *       if the done field of the ioreq is NULL, this function will block the thread.
 *       if the done field of the ioreq is not NULL, this function will return immediately,
 *         and the done function will be called when the I/O is completed.
 *       If buf field is not NULL, this function read to the address pointed by buf
 *       If buf field is NULL, nr_uio and uio should be initialized and this function 
 *        will read to addresses pointed by uio
 * 
 * Returns
 * NDAS_OK: Successfully queued the I/O request(signalled I/O), 
 *         or completed the I/O request(blocking I/O)
 * NDAS_ERROR_INVALID_SLOT_NUMBER : the slot number is invalid
 * NDAS_ERROR_MEDIA_CHANGED : Media Changed (Todo: implemented later)
 * NDAS_ERROR_INVALID_RANGE_REQUEST: The range exceeds the size of the device
 * NDAS_ERROR_TIME_OUT: The requested timed out (when blocking I/O)
 * NDAS_ERROR: Undefined error (Todo: defined later)
 */
NDASUSER_API ndas_error_t ndas_read(int slot, ndas_io_request_ptr ioreq);

/* Description
 * Write raw data to the device identified by slot number.
 * 
 * Parameters
 * slot :   Registered slot number.
 * ioreq :  pointer to ndas_io_request structure 
 *       if the done field of the ioreq is NULL, this function will block the thread.
 *       if the done field of the ioreq is not NULL, this function will return immediately,
 *         and the done function will be called when the I/O is completed.
 *       If buf field is not NULL, this function write data from the address pointed by buf.
 *       If buf field is NULL, nr_uio and uio should be initialized and this function 
 *        will data from addresses pointed by uio 
 * Returns
 * NDAS_OK: Successfully queued the I/O request(signalled I/O), 
 *         or completed the I/O request(blocking I/O)
 * NDAS_ERROR_INVALID_SLOT_NUMBER : the slot number is invalid
 * NDAS_ERROR_MEDIA_CHANGED : Media Changed (Todo: implemented later)
 * NDAS_ERROR_READONLY: Attempt to write on read-only enabled device
 * NDAS_ERROR_INVALID_RANGE_REQUEST: The range exceeds the size of the device
 * NDAS_ERROR_TIME_OUT: The requested timed out (when blocking I/O)
 * NDAS_ERROR: Undefined error (Todo: defined later)
 */
NDASUSER_API ndas_error_t ndas_write(int slot, ndas_io_request_ptr ioreq);

/* Description
 * Flush the cache of the device identified by slot number.
 * 
 * Parameters
 * slot :   Registered slot number.
 * ioreq :  pointer to ndas_io_request structure 
 *       if the done field of the ioreq is NULL, this function will block the thread.
 *       if the done field of the ioreq is not NULL, this function will return immediately,
 *         and the done function will be called when the I/O is completed.
 *       If buf field is not NULL, this function write data from the address pointed by buf.
 *       If buf field is NULL, nr_uio and uio should be initialized and this function 
 *        will data from addresses pointed by uio 
 * Returns
 * NDAS_OK: Successfully queued the I/O request(signalled I/O), 
 *         or completed the I/O request(blocking I/O)
 * NDAS_ERROR_INVALID_SLOT_NUMBER : the slot number is invalid
 * NDAS_ERROR_MEDIA_CHANGED : Media Changed (Todo: implemented later)
 * NDAS_ERROR_INVALID_RANGE_REQUEST: The range exceeds the size of the device
 * NDAS_ERROR_TIME_OUT: The requested timed out (when blocking I/O)
 * NDAS_ERROR: Undefined error (Todo: defined later)
 */
NDASUSER_API ndas_error_t ndas_flush(int slot, ndas_io_request_ptr ioreq);




/* Description
 * Flush the cache of the device identified by slot number.
 * 
 * Parameters
 * slot :   Registered slot number.
 * ioreq :  pointer to ndas_io_request structure 
 *       if the done field of the ioreq is NULL, this function will block the thread.
 *       if the done field of the ioreq is not NULL, this function will return immediately,
 *         and the done function will be called when the I/O is completed.
 *       If buf field is not NULL, this function write data from the address pointed by buf.
 *       If buf field is NULL, nr_uio and uio should be initialized and this function 
 *        will data from addresses pointed by uio 
 * Returns
 * NDAS_OK: Successfully queued the I/O request(signalled I/O), 
 *         or completed the I/O request(blocking I/O)
 * NDAS_ERROR_INVALID_SLOT_NUMBER : the slot number is invalid
 * NDAS_ERROR_MEDIA_CHANGED : Media Changed (Todo: implemented later)
 * NDAS_ERROR_INVALID_RANGE_REQUEST: The range exceeds the size of the device
 * NDAS_ERROR_TIME_OUT: The requested timed out (when blocking I/O)
 * NDAS_ERROR: Undefined error (Todo: defined later)
 */
NDASUSER_API ndas_error_t  ndas_packetcmd(int slot, ndas_io_request_ptr req);
#endif
