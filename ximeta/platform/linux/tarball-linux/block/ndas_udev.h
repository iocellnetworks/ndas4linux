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

#ifndef _NETDISK_UDEV_H_
#define _NETDISK_UDEV_H_


#include "ndas_request.h"

#include <lspx/lsp_type.h>


#ifdef PS2DEV
#define ND_MAX_BLOCK_SIZE       (16*1024)
#else
#define ND_MAX_BLOCK_SIZE       (64*1024)
#endif

#include "ndasdib.h"
#include "ndas_session.h"

#ifdef DEBUG
#define UDEV_MAGIC 0x11ff5544
#endif 

typedef struct _unit_info {

	bool	disk_capacity_set;

	unsigned long 	hard_sector_size;
    __u64 			hard_sectors;    // Disk capacity in sector count

    unsigned char  headers_per_disk;
    unsigned char  sectors_per_cylinder;
    unsigned short cylinders_per_header;

    char serialno[NDAS_MAX_DEVICE_SERIAL_LENGTH+1];
    char model[NDAS_DISK_MODEL_NAME_LENGTH+1];    	// Model name of disk
    char fwrev[NDAS_DISK_FMREV_LENGTH+1]; 			// firmware revision

    int type;        // One of NDAS_UNIT_DISK_TYPE_xxx

#if 1 // to do: move this to seperate structure
    int rwhost;      // Number of read-write hosts that are connected to this unit disk 
    int rohost;     // Number of read-only hosts that are connected to this unit 
#endif

    /* Following members are meaningful only in RAID */
    int raid_slot;    /* Not NDAS_INVALID_SLOT if this unit is member of RAID */
    int defective;  /* Set to non 0 value if this disk is defective in RAID */

	int	unit;

	int	slot;

} ndas_unit_info_t;


struct ndas_slot_info_s;
struct ndev_s;


typedef struct udev_s {

#ifdef DEBUG    
    int		magic;
#endif

    ndas_unit_info_t uinfo;

    struct ndev_s	 *ndev; 
	struct sdev_s	 *sdev;

    uconn_t			 conn;
	int 			 connect_flags;

	pid_t			 udeviod_pid;

	ndas_error_t	 err;


    spinlock_t    		req_queue_lock;
    struct list_head    req_queue;
	wait_queue_head_t	req_queue_wait;	

#ifdef NDAS_HIX
    struct semaphore hix_sema;
    void             *hix_result;
#endif

} udev_t;

extern int udev_max_xfer_unit;

struct unit_ops_s;

extern struct unit_ops_s udev_ops;

udev_t *udev_create(struct ndev_s *ndev, int unit);
void udev_cleanup(udev_t *udev);

void   udev_set_disk_information(udev_t *udev, const lsp_ata_handshake_data_t* hsdata);
void   udev_set_max_xfer_unit(int io_buffer_size);

#endif
