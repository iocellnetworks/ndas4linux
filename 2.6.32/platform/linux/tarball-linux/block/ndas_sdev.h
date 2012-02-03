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
 may be distributed under the terms of the GNU General Public License (GPL v2),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#ifndef _NDAS_SDEV_H_
#define _NDAS_SDEV_H_

#include <ndas_dev.h>

#include "ndas_request.h"
#include "ndasdib.h"

#include <lspx/lsp_type.h>

#include "ndas_udev.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#define NDAS_PARTN_BITS	4
#else
#define NDAS_PARTN_BITS 3 
#endif

#define NDAS_PARTN_MASK		((1<<NDAS_PARTN_BITS)-1)	/* a useful bit mask */
#define NDAS_NR_PARTITION	(1<<NDAS_PARTN_BITS)
#define NDAS_PARTITION(dev)	((dev) & NDAS_PARTN_MASK) 

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

#define SLOT_I(_inode_) 	SLOT((_inode_)->i_bdev->bd_disk)
#define SLOT_R(_request_) 	SLOT((_request_)->rq_disk)
#define SLOT_DEV(dev)		((MINOR(dev) >> NDAS_PARTN_BITS) + NDAS_FIRST_SLOT_NR)

#define SLOT(_gendisk_) \
({ 						\
    struct gendisk *__g__ = _gendisk_;\
    (((__g__->first_minor) >> NDAS_PARTN_BITS) + NDAS_FIRST_SLOT_NR);\
})

#else

#define SLOT_I(inode) 		SLOT((inode)->i_rdev)
#define SLOT_R(_request_) 	SLOT((_request_)->rq_dev)
#define SLOT(dev)           ((MINOR(dev) >> NDAS_PARTN_BITS) + NDAS_FIRST_SLOT_NR)
#define TOMINOR(slot, part) (((slot - NDAS_FIRST_SLOT_NR) << NDAS_PARTN_BITS) | (part))

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#else

extern struct hd_struct ndas_hd[];
extern int            	ndas_blk_size[];
extern int            	ndas_blksize_size[];
extern int            	ndas_hardsect_size[];
extern int            	ndas_max_sectors[];

extern struct gendisk 	ndas_gd;

ndas_error_t sdev_read_partition(int slot);

#endif

typedef struct ndas_metadata_s {

    NDAS_DIB_V2 dib;

} ndas_metadata_t;


#ifdef DEBUG
#define SDEV_MAGIC 0x77fe5531
#endif

#define REGDATA_BIT_DISABLED 		00
#define REGDATA_BIT_READ_ENABLED 	01
#define REGDATA_BIT_WRITE_ENABLED 	02
#define REGDATA_BIT_SHARE_ENABLED 	03
#define REGDATA_BIT_MASK 			03

typedef struct ndas_slot_info_s {

    __u8 writeshared	  :1;
    __u8 writable		  :1;
	__u8 disk_capacity_set:1;
    __u8 enabled		  :1;
    __u8 reserved		  :4;

    __u8 	mode;            // NDAS_DISK_MODE_SINGLE, NDAS_DISK_MODE_RAID0, NDAS_DISK_MODE_RAID1 NDAS_DISK_MODE_MEDIAJUKE
    int		slot;

    char	ndas_id[NDAS_ID_LENGTH+1];
    bool    has_key; // whether user has the NDAS key for this device

	unsigned long	logical_block_size;
    __u64 		 	logical_blocks;    	// Disk capacity in sector count

	unsigned char	nr_child;

    int 			child_slot[NDAS_MAX_UNITS_IN_BIND];
    int  			unit[NDAS_MAX_UNITS_IN_BIND];
    unsigned char	headers_per_disk[NDAS_MAX_UNITS_IN_BIND];
    unsigned char	sectors_per_cylinder[NDAS_MAX_UNITS_IN_BIND];
    unsigned short 	cylinders_per_header[NDAS_MAX_UNITS_IN_BIND];


    unsigned long conn_timeout; 		// connection timeout jiffies

} ndas_slot_info_t;

struct unit_ops_s;

typedef struct sdev_s {

#ifdef DEBUG
    int magic;
#endif 

    struct semaphore sdev_mutex;
    char 			 devname[32];

    ndas_slot_info_t info;

    struct unit_ops_s *ops;

	struct ndev_s   *ndev[NDAS_MAX_UNITS_IN_BIND];
	void 			*child[NDAS_MAX_UNITS_IN_BIND]; 
		
	ndas_error_t	io_err;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

    spinlock_t 	   disk_lock;
    struct gendisk *disk;

    // pointer to disk->queue, but will be deallocated on unregistration

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
    struct request_queue *queue;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    struct request_queue queue;
#endif
    unsigned long	queue_flags;   // NDAS_QUEUE_FLAG_xxx

#define NDAS_QUEUE_FLAG_SUSPENDED  (1<<0)

#else

    unsigned int flags[NDAS_NR_PARTITION];
    int          ref[NDAS_NR_PARTITION];

#endif

    struct proc_dir_entry *proc_dir;
    struct proc_dir_entry *proc_info;
    struct proc_dir_entry *proc_info2;
    struct proc_dir_entry *proc_unit;
    struct proc_dir_entry *proc_timeout;
    struct proc_dir_entry *proc_enabled;
    struct proc_dir_entry *proc_ndas_serial;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
    struct proc_dir_entry *proc_devname;
#endif

    struct proc_dir_entry *proc_sectors;
    struct list_head      proc_node;

#ifdef NDAS_HIX
	ndas_error_t (*surrender_request_handler) (int slot);
    void	*arg;
#endif

    ndas_metadata_t nmd;

} sdev_t;


struct ndev_s;

typedef struct _sdev_create_param {

	__u8	slot;
    int		mode;

    char	ndas_id[NDAS_ID_LENGTH+1];
    bool    has_key; // whether user has the NDAS key for this device

	unsigned long 	logical_block_size;
    __u64 			logical_blocks;    // Disk capacity in sector count

	unsigned char	nr_child;

	struct {

		__u8			slot;
		struct ndev_s   *ndev;
		udev_t			*udev;
		int				unit;

	    unsigned char	headers_per_disk;
		unsigned char	sectors_per_cylinder;
	    unsigned short 	cylinders_per_header;

	} child[NDAS_MAX_UNITS_IN_BIND];

} sdev_create_param_t;

typedef struct unit_ops_s {

	ndas_error_t (*init)(sdev_t *sdev, udev_t *udev);
	void 		 (*deinit)(udev_t *udev);

	ndas_error_t (*enable)(udev_t *udev, int flag);
	void 		 (*disable)(udev_t *udev, ndas_error_t err);

	void (*io)(udev_t *udev, ndas_request_t * ndas_req);

} unit_ops_t;

int	 sdev_get_slot(int slot);
void sdev_put_slot(int slot);
bool sdev_is_slot_allocated(int slot);

sdev_t *sdev_create(sdev_create_param_t* opt);
void sdev_cleanup(sdev_t *sdev, void *arg);

ndas_error_t sdev_query_slot_info(int slot, ndas_slot_info_t* info);
ndas_error_t sdev_query_unit(int slot, ndas_unit_info_t* info);

ndas_error_t sdev_set_slot_handlers(int slot, ndas_error_t (*write_func) (int slot), void* arg );

ndas_error_t sdev_set_encryption_mode(int slot,  bool headenc, bool dataenc);

ndas_error_t sdev_cmd_enable_writeshare(int slot);
ndas_error_t sdev_cmd_enable_exclusive_writable(int slot);
ndas_error_t sdev_cmd_enable_slot(int slot);

ndas_error_t sdev_disable_slot(int slot);

ndas_error_t sdev_io(int slot, ndas_request_t* req, bool bound_check);
sdev_t *sdev_lookup_byslot(int slot);


#endif // _NDAS_SDEV_H_

