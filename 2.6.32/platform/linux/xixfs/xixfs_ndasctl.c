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
#include "linux_ver.h"
#if LINUX_VERSION_25_ABOVE
#include <linux/fs.h> // blkdev_ioctl , register file system
#include <linux/buffer_head.h> // file_fsync
#include <linux/genhd.h>  // rescan_parition
#include <linux/workqueue.h>  // 
#else
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h>

#include "xcsystem/debug.h"
#include "xixcore/layouts.h"
#include "xcsystem/system.h"
#include "xixcore/ondisk.h"

#include "xixfs_global.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"
#include "ndasdev.h"



/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSNDASCTL"



int
xixfs_ndas_closeEvent(
	PXIXFS_EVENT_CTX	pEventCtx
)
{
	int RC = 0;

	
	XIXCORE_ASSERT(pEventCtx);

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ("Enter xixfs_closeEvent\n" ) );

	RC = xixfs_Event_clenup(pEventCtx);
	
	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("fail :xixfs_closeEvent ->xixfs_Event_clenup %x\n", RC ));
		
	}

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ("Exit (%x) xixfs_closeEvent\n", RC ));
	return RC;
}


int
xixfs_ndas_createEvent(
	PXIXFS_EVENT_CTX	pEventCtx
)
{
	int RC = 0;

	
	XIXCORE_ASSERT(pEventCtx);
	
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ("Enter xixfs_createEvent\n" ) );

	if(pEventCtx->error_handler == NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("fail :xixfs_createEvent -> No error handler is set\n"));
		return -EINVAL;
	}


	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM,
			(" Type (%d) \n", pEventCtx->IsSrvType));


	if(pEventCtx->IsSrvType) {
		if(pEventCtx->packet_handler == NULL) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("fail :xixfs_createEvent ->No packet handler is set\n"));

			return -EINVAL;
		}
	}

	RC = xixfs_Event_init(pEventCtx);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("fail :xixfs_createEvent ->xixfs_Event_init %x\n", RC ));
		
	}

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ("Exit (%x) xixfs_createEvent\n", RC ));
	return RC;
	
}




int
xixfs_ndas_sendEvent(
	PXIXFS_EVENT_CTX	pEventCtx
)
{

	int RC = 0;
	
	
	XIXCORE_ASSERT(pEventCtx);	

	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ("Enter xixfs_sendEvent\n" ));

	if(pEventCtx->running != 1) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("fail :xixfs_sendEvent ->Invalid status\n" ));

		return -EINVAL;
	}

	RC = xixfs_Event_Send(pEventCtx);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("fail :xixfs_sendEvent ->xixfs_Event_Send %x\n", RC ));
		
	}
	DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_HOSTCOM, ("Exit (%x) xixfs_sendEvent\n" , RC));

	return RC;
	
}



int 
xixfs_checkNdas(
	struct super_block *sb,
	int				*IsReadOnly
	)
{
	int RC = 0;
	ndas_xixfs_devinfo NdasDevInfo;

	*IsReadOnly = 0;
	
	memset((void *)&NdasDevInfo, 0, sizeof(ndas_xixfs_devinfo));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("DEVINFO REQ\n"));

	printk(KERN_DEBUG "PS[%d] DEVINFO REQ\n", current->pid);
	
	RC = ioctl_by_bdev(sb->s_bdev,  IOCTL_XIXFS_GET_DEVINFO ,(unsigned long)&(NdasDevInfo));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("END DEVINFO  REQ\n"));

	printk(KERN_DEBUG "PS[%d] END DEVINFO REQ\n", current->pid);


	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixfs_checkNdas %x\n", RC ));
		goto error_out;
	}

	if(NdasDevInfo.status < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixfs_checkNdas  IOCTL_XIXFS_GET_DEVINFO with failed %x\n", NdasDevInfo.status));	
		RC = -EINVAL;
		goto error_out;
	}
	

	if( NdasDevInfo.dev_usermode == 0) {
		DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_VOLINFO, 
			("xixfs_checkNdas  READ ONLY DEVICE\n"));
		
		*IsReadOnly = 1;
		RC = 0; 
	}else if(NdasDevInfo.dev_usermode == 1) {
		
		if(NdasDevInfo.dev_sharedwrite == 1) {
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_VOLINFO, 
				("xixfs_checkNdas  SHARED WRITE DEVICE\n"));	
			*IsReadOnly = 0;
		}else {
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_VOLINFO, 
				("xixfs_checkNdas  READ ONLY DEVICE\n"));	
			*IsReadOnly = 1;
		}
		
		RC = 0;
	}else {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixfs_checkNdas  Invalid parameter userM(%x) sharedW(%x)\n",
						NdasDevInfo.dev_usermode, NdasDevInfo.dev_sharedwrite));	
		RC = -EINVAL;
		goto error_out;
	}
	
	
error_out:	
	return RC;
}


#if !LINUX_VERSION_25_ABOVE
int 
xixfs_getPartiton(
	struct super_block *sb,
	pndas_xixfs_partinfo	 partinfo
	)
{
	int RC = 0;
	ndas_xixfs_partinfo NdasPartInfo;

	
	
	memset((void *)&NdasPartInfo, 0, sizeof(ndas_xixfs_partinfo));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("PartINFO REQ\n"));

	printk(KERN_DEBUG "PS[%d] PartINFO REQ\n", current->pid);
	
	RC = ioctl_by_bdev(sb->s_bdev,  IOCTL_XIXFS_GET_PARTINFO ,(unsigned long)&(NdasPartInfo));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("END DEVINFO  REQ\n"));

	printk(KERN_DEBUG "PS[%d] END DEVINFO REQ\n", current->pid);


	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixfs_getPartiton %x\n", RC ));
		goto error_out;
	}

	partinfo->start_sect = NdasPartInfo.start_sect;
	partinfo->nr_sects = NdasPartInfo.nr_sects;
	partinfo->partNumber = NdasPartInfo.partNumber;
	
	
error_out:	
	return RC;
}


#if 0
void xixfs_direct_io_op(void *parameter)
{
	pndas_xixfs_direct_io dio = (pndas_xixfs_direct_io)parameter;
	int RC = 0;
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("xixfs_direct_io_op REQ\n"));


	RC = ioctl_by_bdev(dio->bdev,  IOCTL_XIXFS_DIRECT_IO ,(unsigned long)dio);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixfs_direct_io_op %x\n", RC ));
		goto error_out;
	}	
error_out:
	dio->status = RC;
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("end xixfs_direct_io_op REQ\n"));
	
	complete(dio->dioComplete);
	return;
}



int
xixfs_direct_block_ios(
	struct block_device *bdev, 
	sector_t startsector, 
	int32 size, 
	PXIX_BUF xbuf, 
	int32 rw
)
{
	ndas_xixfs_direct_io dio;
	DECLARE_COMPLETION(wait);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("xixfs_direct_block_ios REQ\n"));

	memset(&dio, 0, sizeof(ndas_xixfs_direct_io));
	dio.bdev = bdev;
	dio.diowork.routine = xixfs_direct_io_op;
	dio.diowork.data = (void *)&dio;
	dio.rdev = bdev->bd_dev;
	dio.cmd = rw;
	dio.start_sect = startsector;
	dio.len = size;
	dio.buff = xbuf->xix_data + xbuf->xix_offset;
	dio.dioComplete = &wait;
	schedule_task(&(dio.diowork));

	wait_for_completion(&wait);

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("end xixfs_direct_block_ios REQ\n"));

	return dio.status;
}


#if LINUX_VERSION_25_ABOVE

#define SLOT_I(_inode_) SLOT((_inode_)->i_bdev->bd_disk)
#define SLOT_R(_request_) SLOT((_request_)->rq_disk)
#define SLOT(_gendisk_) \
({ \
    struct gendisk *__g__ = _gendisk_;\
    (((__g__->first_minor) >> PARTN_BITS) + NDAS_FIRST_SLOT_NR);\
})

#else
#define SLOT_I(inode) SLOT((inode)->i_rdev)
#define SLOT_R(_request_) SLOT((_request_)->rq_dev)
#define SLOT(dev)            (DEVICE_NR(dev) + NDAS_FIRST_SLOT_NR)
#define PARTITION(dev)            ((dev) & PARTN_MASK) 
#define TOMINOR(slot, part)        (((slot - NDAS_FIRST_SLOT_NR) << PARTN_BITS) | (part))
#endif
#endif //#if 0

int
xixfs_direct_block_ios(
	struct block_device *bdev, 
	sector_t startsector, 
	int32 size, 
	PXIX_BUF xbuf, 
	int32 rw
)
{
	ndas_xixfs_direct_io dio;
	int result = 0;
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("xixfs_direct_block_ios REQ\n"));	
	printk(KERN_DEBUG "xixfs_direct_block_ios REQ\n");

	memset(&dio, 0, sizeof(ndas_xixfs_direct_io));
	
	dio.rdev = bdev->bd_inode->i_rdev;
	dio.cmd = rw;
	dio.start_sect = startsector;
	dio.len = size;
	dio.buff = xbuf->xixcore_buffer.xcb_data + xbuf->xixcore_buffer.xcb_offset;

	result =  ndas_block_xixfs_direct_io(&dio);
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("end xixfs_direct_block_ios REQ\n"));
	printk(KERN_DEBUG "end xixfs_direct_block_ios REQ\n");
	return result;
}

#endif


/*
DECLARE_COMPLETION(wait);
wait_for_completion(&wait);
INIT_WORK(&(Pkt->Pktwork),xixfs_ProcessLockBroadCast,Pkt);
queue_work(xixfs_workqueue, &(Pkt->Pktwork));	
*/

#if 0
void xixfs_lock_op(void * parameter)
{
	pndas_xixfs_global_lock ndas_lock = (pndas_xixfs_global_lock)parameter;
	int RC = 0;	
	XIXCORE_ASSERT(parameter);

	
	RC = ioctl_by_bdev(ndas_lock->bdev,  IOCTL_XIXFS_GLOCK ,(unsigned long)(ndas_lock));
 	
	if( RC < 0 ) {
		ndas_lock->lock_status = -EIO;
	}

	complete(ndas_lock->lockComplete);
}



int 
xixfs_ndas_lock(
	struct block_device	*bdev
	)
{
	int RC = 0;
	ndas_xixfs_global_lock ndas_lock;
	DECLARE_COMPLETION(wait);
	
	memset((void *)&ndas_lock, 0, sizeof(ndas_xixfs_global_lock));

	ndas_lock.lock_type = 1;
	ndas_lock.bdev = bdev;
	ndas_lock.lockComplete = &wait;
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("LOCK REQ\n"));


	//printk(KERN_DEBUG "PS[%d] LOCK REQ\n", current->pid);
#if LINUX_VERSION_WORK_QUEUE
	INIT_WORK(&(ndas_lock.lockwork),xixfs_lock_op, &ndas_lock);
	queue_work(xixfs_workqueue, &(ndas_lock.lockwork));
#else
	memset(&(ndas_lock.lockwork), 0, sizeof(struct tq_struct));
	ndas_lock.lockwork.routine = xixfs_lock_op;
	ndas_lock.lockwork.data = &ndas_lock;
	schedule_task(&(ndas_lock.lockwork));
#endif
	
	/*
	RC = ioctl_by_bdev(bdev,  IOCTL_XIXFS_GLOCK ,(unsigned long)&(ndas_lock));
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("END LOCK REQ\n"));

	printk(KERN_DEBUG "PS[%d] END LOCK REQ\n", current->pid);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixfs_ndas_lock %x\n", RC ));
		goto error_out;
	}
	*/
	
	wait_for_completion(&wait);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("END LOCK REQ\n"));

	//printk(KERN_DEBUG "PS[%d] END LOCK REQ\n", current->pid);
	
	if(ndas_lock.lock_status < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixfs_ndas_lock  IOCTL_XIXFS_GLOCK with failed %x\n", ndas_lock.lock_status));	
		RC = -EINVAL;
		goto error_out;		
	}
	
error_out:
	return RC;	
	
}

int 
xixfs_ndas_unlock(
	struct block_device	*bdev
	)
{
	int RC = 0;
	ndas_xixfs_global_lock ndas_lock;
	DECLARE_COMPLETION(wait);
	
	memset((void *)&ndas_lock, 0, sizeof(ndas_xixfs_global_lock));

	ndas_lock.lock_type = 0;
	ndas_lock.bdev = bdev;
	ndas_lock.lockComplete = &wait;
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("UN LOCK REQ\n"));

	//printk(KERN_DEBUG "PS[%d] UNLOCK REQ\n", current->pid);
#if LINUX_VERSION_WORK_QUEUE
	INIT_WORK(&(ndas_lock.lockwork),xixfs_lock_op, &ndas_lock);
	queue_work(xixfs_workqueue, &(ndas_lock.lockwork));
#else
	memset(&(ndas_lock.lockwork), 0, sizeof(struct tq_struct));
	ndas_lock.lockwork.routine = xixfs_lock_op;
	ndas_lock.lockwork.data = &ndas_lock;
	schedule_task(&(ndas_lock.lockwork));
#endif


	
	/*
	RC = ioctl_by_bdev(bdev,  IOCTL_XIXFS_GLOCK ,(unsigned long)&(ndas_lock));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("END UN LOCK REQ\n"));

	printk(KERN_DEBUG "PS[%d] END UNLOCK REQ\n", current->pid);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixfs_ndas_unlock %x\n", RC ));
		goto error_out;
	}
	*/

	wait_for_completion(&wait);
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("END UN LOCK REQ\n"));

	//printk(KERN_DEBUG "PS[%d] END UNLOCK REQ\n", current->pid);

	if(ndas_lock.lock_status < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail:xixfs_ndas_unlock  IOCTL_XIXFS_GLOCK with failed %x\n", ndas_lock.lock_status));	
		RC = -EINVAL;
		goto error_out;		
	}
	
error_out:
	return RC;	
}
#endif //#if 0

