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
*/
#ifndef _NDAS_DEV_H
#define _NDAS_DEV_H

#include <ndas_errno.h>

#include <ndas/def.h>

#include <ndas_id.h>

#ifdef __KERNEL__

#include <linux/version.h>
#include <linux/module.h> 

#endif /* __KERNEL__ */

#define NDAS_BLK_MAJOR  60
#define NDAS_CHR_DEV_MAJOR  60
#define NDAS_CHR_DEV_MINOR 0


#define _NDAS_IO(_index_)          	_IO(60, _index_)

#define IOCTL_NDCMD_START           _NDAS_IO (0x50)
#define IOCTL_NDCMD_STOP            _NDAS_IO (0x51)
#define IOCTL_NDCMD_REGISTER        _NDAS_IO (0x60)
#define IOCTL_NDCMD_UNREGISTER      _NDAS_IO (0x61)
#define IOCTL_NDCMD_ENABLE          _NDAS_IO (0x62)
#define IOCTL_NDCMD_DISABLE         _NDAS_IO (0x63)
#define IOCTL_NDCMD_EDIT            _NDAS_IO (0x64)
#define IOCTL_NDCMD_QUERY           _NDAS_IO (0x67)
#define IOCTL_NDCMD_REQUEST         _NDAS_IO (0x68)
#define IOCTL_NDCMD_PROBE           _NDAS_IO (0x69)
#define IOCTL_NDCMD_PROBE_BY_ID     _NDAS_IO (0x6a)

#define IOCTL_NDAS_LOCK				_NDAS_IO (0x70)

#define IN
#define OUT

typedef struct _ndas_ioctl_register {

    IN  char ndas_idserial[NDAS_ID_LENGTH + 1];  /* ID or serial */
    IN  char ndas_key[NDAS_KEY_LENGTH + 1];
    IN  char name[NDAS_MAX_NAME_LENGTH];
    IN  char use_serial; /* 1 if register by serial */
    IN  char discover; /* 1 if discover is required after registration */    
    OUT int  error;

} ndas_ioctl_register_t, *ndas_ioctl_register_ptr;

typedef struct _ndas_ioctl_unregister {

    IN  char name[NDAS_MAX_NAME_LENGTH];
    OUT int error;

} ndas_ioctl_unregister_t, *ndas_ioctl_unregister_ptr;

typedef struct _ndas_ioctl_enable {

    IN  int slot;
    IN  int write_mode;
    OUT int error;

} ndas_ioctl_enable_t, *ndas_ioctl_enable_ptr;

typedef struct _ndas_ioctl_arg_disable {

    IN  int slot;
    OUT int error;

} ndas_ioctl_arg_disable_t, *ndas_ioctl_arg_disable_ptr;

typedef struct _ndas_ioctl_request {

    IN  int slot;
    OUT int error;

} ndas_ioctl_request_t, *ndas_ioctl_request_ptr;

typedef struct _ndas_ioctl_edit {

    IN  int  slot;
    IN  char name[NDAS_MAX_NAME_LENGTH];
    IN  char newname[NDAS_MAX_NAME_LENGTH];
    OUT int  error;

} ndas_ioctl_edit_t, *ndas_ioctl_edit_ptr;

/* List up the slots registered */

typedef struct _ndas_ioctl_query {

    IN  char name[NDAS_MAX_NAME_LENGTH];
    OUT int  nr_unit;
    OUT int  slots[NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE];
    OUT int  error;

} ndas_ioctl_query_t, *ndas_ioctl_query_ptr;

/* To probe the NDAS device in the local network */      
typedef struct _ndas_ioctl_probe {      

    /* array to fetch the array of the NDAS ids      
     * if set NULL, only nr_ndasid is return      
     */      
    IN  char *ndasids;      
    /* size of the array to be passed */      
    IN  int  sz_array;      
    /* # of NDAS id probed from the local network */      
    OUT int  nr_ndasid;      
    OUT int  error;

} ndas_ioctl_probe_t, *ndas_ioctl_probe_ptr;


#define NDAS_LOCK_COUNT	4

#define NDAS_LOCK_0	0
#define NDAS_LOCK_1	1
#define NDAS_LOCK_2	2
#define NDAS_LOCK_3	3

typedef struct _ndas_lock {

	int 			lock_type; // release == 0, set == 1
	ndas_error_t 	lock_status;

} ndas_lock, *pndas_lock;

#if defined(__KERNEL__)

ndas_error_t ndas_emu_start(char* devfile, int version, int max_transfer_size);
ndas_error_t ndas_emu_stop(void);
ndas_error_t ndas_get_version(int* major, int* minor, int *build);

ndas_error_t ndas_get_string_error(ndas_error_t code, char* buf, int buflen); 

#define NDAS_GET_STRING_ERROR(code) ({\
        static char buf[1024]; \
        ndas_get_string_error(code,buf,1024); \
        (const char*) buf; \
    })

#endif

#endif //_NDAS_DEV_H
