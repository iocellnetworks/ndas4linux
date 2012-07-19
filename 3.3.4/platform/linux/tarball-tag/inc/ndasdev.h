/*
 -------------------------------------------------------------------------
 Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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
 may be distributed under the terms of the GNU General Public License (GPL v2),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#ifndef _NDAS_IOCTL_H
#define _NDAS_IOCTL_H

#include <linux/socket.h>
#include "ndas_id.h"
#include "ndasuser/ndaserr.h"
#include "ndasuser/info.h"
#include "sal/types.h"

#ifdef __KERNEL__
#include "linux_ver.h"
#ifndef NDAS_MAX_SLOT
#if LINUX_VERSION_25_ABOVE
#define NDAS_MAX_SLOT        (1<<4) 
#else
#define NDAS_MAX_SLOT        (1<<5)
#endif /* LINUX_VERSION_25_ABOVE */
#endif /* NDAS_MAX_SLOT */
#endif /* __KERNEL__ */

#define NDAS_BLK_MAJOR  60
#define NDAS_CHR_DEV_MAJOR  60
#define NDAS_CHR_DEV_MINOR 0

#define NDAS_MAX_SLOT_NR ((NDAS_MAX_SLOT) + (NDAS_FIRST_SLOT_NR))

#define _NDAS_IO(_index_)                 _IO(60, _index_)

#define IOCTL_NDCMD_START            _NDAS_IO (0x50)
#define IOCTL_NDCMD_STOP            _NDAS_IO (0x51)
#define IOCTL_NDCMD_REGISTER        _NDAS_IO (0x60)
#define IOCTL_NDCMD_UNREGISTER        _NDAS_IO (0x61)
#define IOCTL_NDCMD_ENABLE            _NDAS_IO (0x62)
#define IOCTL_NDCMD_DISABLE            _NDAS_IO (0x63)
#define IOCTL_NDCMD_EDIT            _NDAS_IO (0x64)
#define IOCTL_NDCMD_QUERY            _NDAS_IO (0x67)
#define IOCTL_NDCMD_REQUEST            _NDAS_IO (0x68)
#define IOCTL_NDCMD_PROBE            _NDAS_IO (0x69)
#define IOCTL_NDCMD_PROBE_BY_ID            _NDAS_IO (0x6a)

#ifdef NDAS_MSHARE
#define IOCTL_NDCMD_MS_SETUP_WRITE		_NDAS_IO(0x80)		//write start
#define IOCTL_NDCMD_MS_END_WRITE			_NDAS_IO(0x81)		//write end
#define IOCTL_NDCMD_MS_SETUP_READ		_NDAS_IO(0x82)		//read start
#define IOCTL_NDCMD_MS_END_READ			_NDAS_IO(0x83)		//read end
#define IOCTL_NDCMD_MS_DEL_DISC			_NDAS_IO(0x84)		//delete disc
#define IOCTL_NDCMD_MS_VALIDATE_DISC		_NDAS_IO(0x85)		//validate disc
#define IOCTL_NDCMD_MS_FORMAT_DISK		_NDAS_IO(0x86)		//format disc
#define IOCTL_NDCMD_MS_GET_DISK_INFO		_NDAS_IO(0x87)		//get disk info --> discs info
#define IOCTL_NDCMD_MS_GET_DISC_INFO		_NDAS_IO(0x88)		//get_disc_info --> disc specified info
#endif


#ifdef XPLAT_XIXFS_EVENT
#define IOCTL_XIXFS_GET_DEVINFO	_NDAS_IO(0x90)		//get dev status
#define IOCTL_XIXFS_GLOCK			_NDAS_IO(0x91)		//set/release global lock
#if !LINUX_VERSION_25_ABOVE
#define IOCTL_XIXFS_GET_PARTINFO   _NDAS_IO(0x92)		//get partition info
#define IOCTL_XIXFS_DIRECT_IO		_NDAS_IO(0x93)
#endif
#endif //#ifdef XPLAT_XIXFS_EVENT


#define IN
#define OUT

typedef struct _ndas_ioctl_register {
    IN char ndas_idserial[NDAS_ID_LENGTH + 1];  /* ID or serial */
    IN char ndas_key[NDAS_KEY_LENGTH + 1];
    IN char name[NDAS_MAX_NAME_LENGTH];
    IN char use_serial; /* 1 if register by serial */
    IN char discover; /* 1 if discover is required after registration */    
    OUT ndas_error_t error;
} ndas_ioctl_register_t, *ndas_ioctl_register_ptr;

typedef struct _ndas_ioctl_unregister {
    IN  char name[NDAS_MAX_NAME_LENGTH];
    OUT ndas_error_t error;
} ndas_ioctl_unregister_t, *ndas_ioctl_unregister_ptr;

typedef struct _ndas_ioctl_enable {
    IN int slot;
    IN int write_mode;
    OUT ndas_error_t error;
} ndas_ioctl_enable_t, *ndas_ioctl_enable_ptr;

typedef struct _ndas_ioctl_arg_disable {
    IN int slot;
    OUT ndas_error_t error;
} ndas_ioctl_arg_disable_t, *ndas_ioctl_arg_disable_ptr;

typedef struct _ndas_ioctl_request {
    IN int slot;
    OUT ndas_error_t error;
} ndas_ioctl_request_t, *ndas_ioctl_request_ptr;

typedef struct _ndas_ioctl_edit {
    IN int slot;
    IN char name[NDAS_MAX_NAME_LENGTH];
    IN char newname[NDAS_MAX_NAME_LENGTH];
    OUT ndas_error_t error;
} ndas_ioctl_edit_t, *ndas_ioctl_edit_ptr;

/* List up the slots registered */
typedef struct _ndas_ioctl_query {
    IN    char name[NDAS_MAX_NAME_LENGTH];
    OUT int nr_unit;
    OUT int slots[NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE];
    OUT ndas_error_t error;
} ndas_ioctl_query_t, *ndas_ioctl_query_ptr;

/* To probe the NDAS device in the local network */      
typedef struct _ndas_ioctl_probe {      
    /* array to fetch the array of the NDAS ids      
     * if set NULL, only nr_ndasid is return      
     */      
    IN char *ndasids;      
    /* size of the array to be passed */      
    IN int  sz_array;      
    /* # of NDAS id probed from the local network */      
    OUT int nr_ndasid;      
    OUT ndas_error_t error;
} ndas_ioctl_probe_t, *ndas_ioctl_probe_ptr;
 
#ifdef NDAS_MSHARE

typedef struct _ndas_ioctl_juke_write_start{
    IN	int			slot;
    IN	unsigned int	selected_disc;		// selected disc index
    IN	unsigned int	nr_sectors;			// size of disc
    IN	unsigned int	encrypted;			// need encrypt
    IN	unsigned char HINT[32];			// encrypt hint
    OUT 	int	partnum;			// allocated stream id --> linux version allow 8 stream
    OUT 	ndas_error_t 	error;				// error
}ndas_ioctl_juke_write_start_t, *ndas_ioctl_juke_write_start_ptr;


typedef struct 	_DISC_INFO
{
	unsigned char		title_name[NDAS_MAX_NAME_LENGTH];
	unsigned char		additional_information[NDAS_MAX_NAME_LENGTH];
	unsigned char		key[NDAS_MAX_NAME_LENGTH];
	unsigned char		title_info[NDAS_MAX_NAME_LENGTH];
}DISC_INFO, *PDISC_INFO;

typedef struct _ndas_ioctl_juke_write_end{
    IN	int			slot;	
    IN	int			partnum;
    IN	unsigned int	selected_disc;		// selected disc index
    IN	PDISC_INFO	discInfo;
    OUT 	ndas_error_t 	error;				// error
}ndas_ioctl_juke_write_end_t, *ndas_ioctl_juke_write_end_ptr;

typedef struct _ndas_ioctl_juke_read_start{
    IN	int			slot;	
    IN	unsigned int selected_disc;
    OUT 	int	partnum;			// allocated stream id --> linux version allow 8 stream
    OUT 	ndas_error_t 	error;				// error
}ndas_ioctl_juke_read_start_t, *ndas_ioctl_juke_read_start_ptr;

typedef struct _ndas_ioctl_juke_read_end{
    IN	int			slot;	
    IN	int			partnum;
    IN	unsigned int	selected_disc;		// selected disc index
    OUT 	ndas_error_t 	error;				// error
}ndas_ioctl_juke_read_end_t, *ndas_ioctl_juke_read_end_ptr;

typedef struct _ndas_ioctl_juke_del{
    IN	int			slot;	
    IN	unsigned int	selected_disc;		// selected disc index
    OUT 	ndas_error_t 	error;
}ndas_ioctl_juke_del_t, * ndas_ioctl_juke_del_ptr;

typedef struct _ndas_ioctl_juke_validate {
    IN	int			slot;	
    IN	unsigned int	selected_disc;		// selected disc index
    OUT 	ndas_error_t 	error;	
}ndas_ioctl_juke_validate_t, *ndas_ioctl_juke_validate_ptr;

typedef struct _ndas_ioctl_juke_format {
    IN	int			slot;
   OUT 	ndas_error_t 	error;	    
}ndas_ioctl_juke_format_t, * ndas_ioctl_juke_format_ptr;

#define MAX_DISC_INFO 120
typedef struct _DISK_information {
    short partinfo[8];
    int     discinfo[MAX_DISC_INFO];
    int     count;
}DISK_information, *PDISK_information;

typedef struct _ndas_ioctl_juke_disk_info {
    IN	int				slot;
    IN       int 				queryType;
    IN	PDISK_information	diskInfo;
    OUT    int 				diskType;
    OUT 	ndas_error_t 		error;	    	
}ndas_ioctl_juke_disk_info_t, * ndas_ioctl_juke_disk_info_ptr;

typedef struct _ndas_ioctl_juke_disc_info {
    IN	int			slot;
    IN	int			selected_disc;
    IN	PDISC_INFO	discInfo;
    OUT 	ndas_error_t 	error;	    	
}ndas_ioctl_juke_disc_info_t, *ndas_ioctl_juke_disc_info_ptr;

#endif

// Allow xixfs support only in kernel mode.
#ifndef __KERNEL__
#undef XPLAT_XIXFS_EVENT
#endif

#ifdef XPLAT_XIXFS_EVENT
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))	
#include <linux/workqueue.h>
#endif
#include <linux/completion.h>
#include <linux/fs.h>

typedef struct _ndas_xixfs_devinfo {
	int dev_usermode; // read = 0 write =1
	int dev_sharedwrite; // shared write  = 1
	int status;			// error <0 
}ndas_xixfs_devinfo, *pndas_xixfs_devinfo;

#define XIXFS_GLOBAL_LOCK_INDEX	2

typedef struct _ndas_xixfs_global_lock {
#if 0
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))	
	struct work_struct		lockwork;
#else
	struct tq_struct		lockwork;
#endif
	struct block_device	*bdev;
	
#endif //if 0
#if LINUX_VERSION_25_ABOVE
	struct gendisk * rdev;
#else
	kdev_t 	rdev;
#endif
	int lock_type; // release == 0, set == 1
#if 0
	int lock_status; // error <0 
	struct completion *lockComplete;
#endif //#if 0
}ndas_xixfs_global_lock, *pndas_xixfs_global_lock;

NDASUSER_API
int ndas_block_xixfs_lock(pndas_xixfs_global_lock plockinfo);


#if !LINUX_VERSION_25_ABOVE
typedef struct _ndas_xixfs_partinfo {
	unsigned long start_sect;
	unsigned long nr_sects;
	unsigned long partNumber;
}ndas_xixfs_partinfo, *pndas_xixfs_partinfo;




typedef struct _ndas_xixfs_direct_io {
#if 0	
	struct block_device	*bdev;
	struct tq_struct		diowork;	
#endif //#if 0
	kdev_t rdev;
	int cmd;
	unsigned long start_sect;
	unsigned long len;
	void	*buff;
	int 	status;
#if 0	
	struct completion *dioComplete;
	//used by block driver don't touch this
	struct completion *endioComplete;
#endif //#if 0
}ndas_xixfs_direct_io, *pndas_xixfs_direct_io;

NDASUSER_API
int ndas_block_xixfs_direct_io(pndas_xixfs_direct_io ndas_d_io);

#endif
#endif //#ifdef XPLAT_XIXFS_EVENT



#endif //_NDAS_IOCTL_H
