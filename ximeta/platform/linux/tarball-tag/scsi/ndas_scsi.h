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

/*
 * Data stuctures and Functions to provide the NDAS block device in linux
 * 
 */
#ifndef __LINUX_NDAS_SCSI_H__
#define __LINUX_NDAS_SCSI_H__




#include <sal/mem.h>
#include <ndasuser/info.h> // ndas_slot_info_t
#include <ndasuser/io.h>
#include "linux_ver.h"
#include "ndasdev.h"
#include <scsi/scsi_cmnd.h>
#include <scsi/scsi_device.h>
#include <scsi/scsi_eh.h>
#include <scsi/scsi.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

struct Scsi_host;
struct scsi_cmnd;
struct scsi_device;
struct scsi_target;
struct scatterlist;


typedef struct scsi_cmnd Scsi_Cmnd;



#define DEBUG_LEVEL_SCSI 1

#ifdef DEBUG
#if !defined(DEBUG_LEVEL_SCSI) 
    #if defined(DEBUG_LEVEL)
    #define DEBUG_LEVEL_SCSI DEBUG_LEVEL
    #else
    #define DEBUG_LEVEL_SCSI 1
    #endif
#endif
#define dbgl_scsi(l,x...) do { \
    if ( l <= DEBUG_LEVEL_SCSI ) {\
        printk(KERN_INFO "NS|%d|%s|",l,__FUNCTION__); \
        printk(KERN_INFO x);\
        printk(KERN_INFO "\n");\
    } \
} while(0) 
#else
#define dbgl_scsi(l,fmt...) do {} while(0)
#endif


#define LINUX_VERSION_UPDATE_DRIVER (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))


#define MAX_SUPPORTED_COM_SIZE	12
struct ndas_scsi_pc {
	struct list_head		q_entry;
	ndas_io_request         ndas_req;
	nads_io_pc_request ndas_add_req_block;
	struct scsi_cmnd *org_cmd;
	struct ndas_slot * org_dev;
	unsigned int		avorted;
	void (*done)(struct scsi_cmnd *);
	struct sal_mem_block     blocks[0];
};

#define MAS_NDAS_SG_SEGMENT		(64*1024/PAGE_SIZE)

#define NSBIO_SIZE(nr_blocks) ( \
    sizeof(struct ndas_scsi_pc) + \
    (nr_blocks) * sizeof(struct sal_mem_block))

#define MAX_NDAS_SCSI_BIO_SIZE NSBIO_SIZE(MAS_NDAS_SG_SEGMENT)
/*
 * The data structure to maintain the information about the slot
 */
struct ndas_slot {
    int         enabled;     /* 1 if enabled */
    ndas_slot_info_t info;
    ndas_device_info dinfo;
    int            slot;


	
    struct semaphore mutex;
    ndas_error_t         enable_result;

    char devname[32];
    

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
    struct list_head proc_node;
    struct Scsi_Host *shost;
    struct scsi_device * sdp;	
    struct device dev;
    struct list_head q_list_head;
    spinlock_t	q_list_lock;
    wait_queue_head_t		wait_abort;
    atomic_t	wait_count;	
};

#define dev_to_ndas_slot(d) \
    container_of(d, struct ndas_slot, dev)



#define SENSE_NO_SENSE         0x00
#define SENSE_RECOVERED_ERROR  0x01
#define SENSE_NOT_READY        0x02
#define SENSE_MEDIUM_ERROR     0x03
#define SENSE_HARDWARE_ERROR   0x04
#define SENSE_ILLEGAL_REQUEST  0x05
#define SENSE_UNIT_ATTENTION   0x06
#define SENSE_DATA_PROTECT     0x07
#define SENSE_BLANK_CHECK      0x08
#define SENSE_UNIQUE           0x09
#define SENSE_COPY_ABORTED     0x0A
#define SENSE_ABORTED_COMMAND  0x0B
#define SENSE_EQUAL            0x0C
#define SENSE_VOL_OVERFLOW     0x0D
#define SENSE_MISCOMPARE       0x0E
#define SENSE_RESERVED         0x0F


#define ADSENSE_NO_SENSE                              0x00
#define ADSENSE_NO_SEEK_COMPLETE                      0x02
#define ADSENSE_LUN_NOT_READY                         0x04
#define ADSENSE_LUN_COMMUNICATION                     0x08
#define ADSENSE_WRITE_ERROR                           0x0C
#define ADSENSE_TRACK_ERROR                           0x14
#define ADSENSE_SEEK_ERROR                            0x15
#define ADSENSE_REC_DATA_NOECC                        0x17
#define ADSENSE_REC_DATA_ECC                          0x18
#define ADSENSE_PARAMETER_LIST_LENGTH                 0x1A
#define ADSENSE_ILLEGAL_COMMAND                       0x20
#define ADSENSE_ILLEGAL_BLOCK                         0x21
#define ADSENSE_INVALID_CDB                           0x24
#define ADSENSE_INVALID_LUN                           0x25
#define ADSENSE_INVALID_FIELD_PARAMETER_LIST          0x26
#define ADSENSE_WRITE_PROTECT                         0x27
#define ADSENSE_MEDIUM_CHANGED                        0x28
#define ADSENSE_BUS_RESET                             0x29
#define ADSENSE_PARAMETERS_CHANGED                    0x2A
#define ADSENSE_INSUFFICIENT_TIME_FOR_OPERATION       0x2E
#define ADSENSE_INVALID_MEDIA                         0x30
#define ADSENSE_NO_MEDIA_IN_DEVICE                    0x3a
#define ADSENSE_POSITION_ERROR                        0x3b
#define ADSENSE_OPERATING_CONDITIONS_CHANGED          0x3f
#define ADSENSE_OPERATOR_REQUEST                      0x5a // see below
#define ADSENSE_FAILURE_PREDICTION_THRESHOLD_EXCEEDED 0x5d
#define ADSENSE_ILLEGAL_MODE_FOR_THIS_TRACK           0x64
#define ADSENSE_COPY_PROTECTION_FAILURE               0x6f
#define ADSENSE_POWER_CALIBRATION_ERROR               0x73
#define ADSENSE_VENDOR_UNIQUE                         0x80 // and higher
#define ADSENSE_MUSIC_AREA                            0xA0
#define ADSENSE_DATA_AREA                             0xA1
#define ADSENSE_VOLUME_OVERFLOW                       0xA7



#define NDAS_SCSI_STATUS_SUCCESS					0x00	// CCB completed without error
#define NDAS_SCSI_STATUS_BUSY						0x05	// busy.
#define NDAS_SCSI_STATUS_STOP						0x23	// device stopped.
#define NDAS_SCSI_STATUS_UNKNOWN_STATUS			0x02
#define NDAS_SCSI_STATUS_NOT_EXIST				0x10    // No Target or Connection is disconnected.
#define NDAS_SCSI_STATUS_INVALID_COMMAND			0x11	// Invalid command, out of range, unknown command
#define NDAS_SCSI_STATUS_DATA_OVERRUN				0x12
#define NDAS_SCSI_STATUS_COMMAND_FAILED			0x13
#define NDAS_SCSI_STATUS_RESET					0x14
#define NDAS_SCSI_STATUS_COMMUNICATION_ERROR		0x21
#define NDAS_SCSI_STATUS_COMMMAND_DONE_SENSE		0x22
#define NDAS_SCSI_STATUS_COMMMAND_DONE_SENSE2		0x24
#define NDAS_SCSI_STATUS_NO_ACCESS				0x25	// No access right to execute this ccb operation.
#define NDAS_SCSI_STATUS_LOST_LOCK				0x30


int ndas_scsi_add_adapter(struct device *dev);
int ndas_scsi_can_remove(struct device *dev);
void ndas_scsi_remove_adapter(struct device *dev);
void ndas_scsi_release_all_req_from_slot(struct ndas_slot * ndas_dev);

extern struct ndas_slot* g_ndas_dev[];
#define NDAS_GET_SLOT_DEV(slot) (g_ndas_dev[(slot) - NDAS_FIRST_SLOT_NR])
#define NDAS_SET_SLOT_DEV(slot, dev)   do { g_ndas_dev[slot - NDAS_FIRST_SLOT_NR] = dev; } while(0)

#endif /*#ifndef __NDAS_SCSI_H__ */
