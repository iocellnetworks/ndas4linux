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
#include "xixfs_ndasctl.h"
#include "ndasdev.h"
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h>

#include "xcsystem/debug.h"
#include "xcsystem/errinfo.h"
#include "xcsystem/system.h"
#include "xcsystem/linux/xcsysdep.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"

#include "xixfs_xbuf.h"
#include "xixfs_name_cov.h"
#include "misc.h"


/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSCALLBACK"

//
// Linux-dependent implementation for XIXFS core.
//

/*
 * block IO
 */

#if LINUX_VERSION_25_ABOVE	

static
int 
end_bio_xbuf_io_async(
	struct bio * bio, 
	uint32 bytes_done, 
	int32 err
	)
{
	
	PXIX_BUF xbuf = bio->bi_private;
	if(bio->bi_size)
		return 1;

	if(err == -EOPNOTSUPP){
		set_bit(BIO_EOPNOTSUPP, &bio->bi_flags);
		set_bit(XIX_BUF_FLAGS_ERROR, &xbuf->xix_flags);
	}

	xbuf->xix_end_io(xbuf, test_bit(BIO_UPTODATE, &bio->bi_flags));
	bio_put(bio);
	return 0;
}


static
int  
xixfs_raw_submit(
	struct block_device *bdev, 
	sector_t startsector, 
	int32 size, 
	int32 sectorsize, 
	PXIX_BUF xbuf,
	int32 rw)
{
	struct bio  * bio;
	int ret = 0;

	bio = bio_alloc(GFP_NOIO, 1);
		
	bio->bi_sector = startsector;
	bio->bi_bdev = bdev;
	bio_add_page(bio, xbuf->xix_page, size, xbuf->xixcore_buffer.xcb_offset);
	bio->bi_private = (void *)xbuf;
	bio->bi_end_io = end_bio_xbuf_io_async;
	bio->bi_rw = rw;
	
	bio_get(bio);
	submit_bio(rw, bio);
	
	if(bio_flagged(bio, BIO_EOPNOTSUPP))
		ret = -EOPNOTSUPP;
	bio_put(bio);
	return ret;
}


int
xixcore_call
xixcore_DeviceIoSync(
	PXIXCORE_BLOCK_DEVICE xixfsBlkDev,
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_uint32 sectorsize,
	xc_uint32 sectorsizeBit, 
	PXIXCORE_BUFFER xixfsBuffer, 
	xc_int32 xixfsCoreRw,
	xc_int32 * Reason
	)
{
	struct block_device *bdev = &xixfsBlkDev->impl;
	PXIX_BUF xbuf = container_of(xixfsBuffer, XIX_BUF, xixcore_buffer);
	int32 rw;

	int32 ret = 0;
	*Reason = 0;
	
	XIXCORE_ASSERT(xbuf);
	XIXCORE_ASSERT(size <= (xbuf->xixcore_buffer.xcb_size - xbuf->xixcore_buffer.xcb_offset));
	XIXCORE_ASSERT(xbuf->xix_page != NULL);
	XIXCORE_ASSERT(xbuf->xixcore_buffer.xcb_data != NULL);

	//
	// Convert XIXFS core block operation code to linux code
	//
	switch(xixfsCoreRw) {
		case XIXCORE_READ:	rw = READ; break;
		case XIXCORE_WRITE: rw = WRITE; break;
		default:
			return -1;
	}

	if( size != SECTOR_ALIGNED_SIZE(sectorsizeBit, size) ) {
		int new_size = 0;
		
		new_size = SECTOR_ALIGNED_SIZE(sectorsizeBit, size);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_DEVCTL,
		("set new file size from %d to %d", size, new_size));
		size = new_size;
	}

	
	lock_xbuf(xbuf);
	xbuf->xix_end_io = xbuf_end_io;
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("SUBMIT REQ\n"));
	ret = xixfs_raw_submit(bdev, startsector, size, sectorsize, xbuf, rw);
	
	if( ret < 0 ){
		*Reason = XCREASON_RAWIO_ERROR_SUBMIT;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("ERROR SUBMIT REQ\n"));
		return ret;
	}
	wait_on_xbuf(xbuf);

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("END SUBMIT REQ\n"));

	if(test_bit(XIX_BUF_FLAGS_FINE, &xbuf->xix_flags)){
		return 0;
	}else{
		*Reason = XCREASON_RAWIO_ERROR_OP;
		return -EOPNOTSUPP;
	}
}
#else

int
xixcore_call
xixcore_DeviceIoSync(
	PXIXCORE_BLOCK_DEVICE xixfsBlkDev,
	xc_sector_t startsector, 
	xc_int32 size, 
	xc_uint32 sectorsize,
	xc_uint32 sectorsizeBit, 
	PXIXCORE_BUFFER xixfsBuffer, 
	xc_int32 rw,
	xc_int32 * Reason
	)
{
	struct block_device *bdev = &xixfsBlkDev->impl;
	PXIX_BUF xbuf = container_of(xixfsBuffer, XIX_BUF, xixcore_buffer);
	int32 ret = 0;

	*Reason = 0;
	
	XIXCORE_ASSERT(xbuf);
	XIXCORE_ASSERT(size <= (xixfsBuffer->xcb_size - xixfsBuffer->xcb_offset));
	XIXCORE_ASSERT(xixfsBuffer->xcb_data != NULL);
	XIXCORE_ASSERT(xbuf->xix_page != NULL);

	if( size != SECTOR_ALIGNED_SIZE(sectorsizeBit, size) ) {
		int new_size = 0;
		
		new_size = SECTOR_ALIGNED_SIZE(sectorsizeBit, size);
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_DEVCTL, ("set new file size from %d to %d", size, new_size));
		size = new_size;
	}

	ret = xixfs_direct_block_ios(bdev, startsector, size, xbuf, rw);

	if( ret < 0 ){
		*Reason = XCREASON_RAWIO_ERROR_SUBMIT;
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
			("ERROR SUBMIT REQ\n"));
		return ret;
	}
	
	
	return ret;
	
}
#endif


/*
 * Wait for meta resource
 */

int
xixcore_call
xixcore_WaitForMetaResource(
	PXIXCORE_VCB	XixcoreVCB
	)
{
	PXIXFS_LINUX_VCB	VCB = container_of(XixcoreVCB, XIXFS_LINUX_VCB, XixcoreVcb);
	PXIXFS_LINUX_META_CTX		pCtx = NULL;
	XIXFS_WAIT_CTX wait;
	int			RC = 0;
#if LINUX_VERSION_25_ABOVE		
	int			TimeOut;
#endif

	XIXFS_ASSERT_VCB(VCB);

	pCtx = &VCB->MetaCtx;

	xixfs_init_wait_ctx(&wait);
	xixfs_add_resource_wait_list(&wait, pCtx);
	
	wake_up(&(pCtx->VCBMetaEvent));

	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
		("WAKE UP from VCB RESOURCE EVENT 2 \n"));

	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("RESOURCE REQ\n"));

	printk(KERN_DEBUG "WAIT Resource\n");
		
#if LINUX_VERSION_25_ABOVE			
	TimeOut = DEFAULT_XIXFS_RECHECKRESOURCE;
	RC = wait_for_completion_timeout(&(wait.WaitCompletion),TimeOut);
#else
	wait_for_completion(&(wait.WaitCompletion));
	RC = 1;
#endif

	if(RC <= 0) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_THREAD,
			("wait_for_completion_timeout() failed.\n"));

		xixfs_remove_resource_wait_list(&wait, pCtx);
	}

	printk(KERN_DEBUG "END Wait Resource\n");
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("END RESOURCE REQ\n"));

	return RC;
}


/*
 * Xixcore buffer
 */

void
xixcore_call
xixcore_FreeBuffer(PXIXCORE_BUFFER xixfsBuffer)
{
	PXIX_BUF xbuf = container_of(xixfsBuffer, XIX_BUF, xixcore_buffer);

	if(xbuf->xix_page){
		free_pages(
			(unsigned long)xbuf->xixcore_buffer.xcb_data,
			 get_order(xbuf->xixcore_buffer.xcb_size));
		xbuf->xix_page = NULL;
		xbuf->xixcore_buffer.xcb_data = NULL;
	}

	if(XIXCORE_TEST_FLAGS(xixfsBuffer->xcb_flags,XIXCORE_BUFF_ALLOC_F_MEMORY)){
		kfree(xbuf);
	}else{
		kmem_cache_free(xbuf_cachep, xbuf);
	}
	
	return;
}

PXIXCORE_BUFFER
xixcore_call
xixcore_AllocateBuffer(xc_uint32 size)
{
	PXIX_BUF xbuf = NULL;
	struct page * page = NULL;
	int order = 0;
	
#if  LINUX_VERSION_KMEM_CACHE_T_DEPRECATED
	xbuf= kmem_cache_alloc(xbuf_cachep, GFP_KERNEL);
#else
	xbuf= kmem_cache_alloc(xbuf_cachep, SLAB_KERNEL);
#endif	
	
	if(!xbuf) {

		xbuf = kmalloc(sizeof(XIX_BUF), GFP_KERNEL);
		if( !xbuf) {
			return NULL;
		}
		memset(xbuf, 0, sizeof(XIX_BUF));
		XIXCORE_SET_FLAGS(xbuf->xixcore_buffer.xcb_flags, XIXCORE_BUFF_ALLOC_F_MEMORY);
		
	}else{
		memset(xbuf, 0, sizeof(XIX_BUF));
		XIXCORE_SET_FLAGS(xbuf->xixcore_buffer.xcb_flags, XIXCORE_BUFF_ALLOC_F_POOL);
	}

	order = get_order(size);
	
	page = alloc_pages(GFP_KERNEL, order);
	if(!page) {
		xixcore_FreeBuffer(&xbuf->xixcore_buffer);
		return NULL;
	}

	xixcore_InitializeBuffer(
			&xbuf->xixcore_buffer,
			page_address(page),
			(PAGE_SIZE << order),
			0);

	xbuf->xix_page = page;
	xbuf->xix_flags = 0;

	return &xbuf->xixcore_buffer;
}

/*
 * Upcase table
 */

xc_le16 *
xixcore_call
xixcore_AllocateUpcaseTable() {
	return xixfs_large_malloc( (XIXCORE_DEFAULT_UPCASE_NAME_LEN*sizeof(xc_le16)) , GFP_NOFS | __GFP_HIGHMEM);
}

void
xixcore_call
xixcore_FreeUpcaseTable(
	xc_le16 *upcase_table
	)
{
	xixfs_large_free(upcase_table);
}

/*
 * Device lock
 */


int 
xixcore_call
xixcore_AcquireDeviceLock(
	PXIXCORE_BLOCK_DEVICE	XixcoreBlockDevice
	)
{
	struct block_device	*bdev = (struct block_device*)XixcoreBlockDevice;
	int RC = 0;
	ndas_xixfs_global_lock ndas_lock;


	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("LOCK REQ\n"));
	
	memset((void *)&ndas_lock, 0, sizeof(ndas_xixfs_global_lock));
	ndas_lock.lock_type = 1;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	ndas_lock.rdev = bdev->bd_disk;
#else
	ndas_lock.rdev = bdev->bd_inode->i_rdev;
#endif
	printk(KERN_DEBUG "PS[%d] LOCK REQ\n", current->pid);

	RC = ndas_block_xixfs_lock(&ndas_lock);
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("END LOCK REQ\n"));

	printk(KERN_DEBUG "PS[%d] END LOCK REQ\n", current->pid);
	
	return RC;
	
	
}


int 
xixcore_call
xixcore_ReleaseDevice(
	PXIXCORE_BLOCK_DEVICE	XixcoreBlockDevice
	)
{
	int RC = 0;
	struct block_device	*bdev = (struct block_device*)XixcoreBlockDevice;
	ndas_xixfs_global_lock ndas_lock;


	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("LOCK REQ\n"));
	
	memset((void *)&ndas_lock, 0, sizeof(ndas_xixfs_global_lock));
	ndas_lock.lock_type = 0;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
	ndas_lock.rdev = bdev->bd_disk;
#else
	ndas_lock.rdev = bdev->bd_inode->i_rdev;
#endif
	printk(KERN_DEBUG "PS[%d] LOCK REQ\n", current->pid);

	RC = ndas_block_xixfs_lock(&ndas_lock);
	
	
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_THREAD,
		("END LOCK REQ\n"));

	printk(KERN_DEBUG "PS[%d] END LOCK REQ\n", current->pid);
	
	return RC;
}


/*
 * Host communication
 */

int
xixcore_call
xixcore_HaveLotLock(
	uint8		* HostMac,
	uint8		* LockOwnerMac,
	uint64		LotNumber,
	uint8		* VolumeId,
	uint8		* LockOwnerId
)
{
	// Request LotLock state to Lock Owner
	
	
	int							RC = 0;
	PXIXFSDG_PKT				pPacket = NULL;
	PXIXFS_LOCK_REQUEST		pPacketData = NULL;
	int							waitStatus = LOCK_INVALID;						
	DECLARE_COMPLETION(wait);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Enter xixfs_AreYouHaveLotLock \n"));

	
	 RC = xixfs_AllocDGPkt(
					&pPacket, 
					HostMac, 
					LockOwnerMac, 
					XIXFS_TYPE_LOCK_REQUEST
					) ;
	 
	if(RC < 0  )
	{
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Error (0x%x) xixfs_AreYouHaveLotLock:xixfs_AllocDGPkt  \n",
				RC));
		
		return RC;
	}

	

	// Changed by ILGU HONG
	//	chesung suggest
	
	DebugTrace(DEBUG_LEVEL_INFO, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
		("Packet Dest Info  [0x%02x:%02x:%02x:%02x:%02x:%02x]\n",
		LockOwnerMac[26], LockOwnerMac[27], LockOwnerMac[28],
		LockOwnerMac[29], LockOwnerMac[30], LockOwnerMac[31]));

	pPacketData = &(pPacket->RawDataDG.LockReq);
	memcpy(pPacketData->LotOwnerID, LockOwnerId, 6);
	// Changed by ILGU HONG
	//	chesung suggest
	memcpy(pPacketData->VolumeId, VolumeId, 16);
	memcpy(pPacketData->LotOwnerMac, LockOwnerMac, 32);
	pPacketData->LotNumber = XI_HTONLL(LotNumber);
	pPacketData->PacketNumber = XI_HTONL(atomic_read(&(xixfs_linux_global.EventCtx.PacketNumber)));
	atomic_inc(&(xixfs_linux_global.EventCtx.PacketNumber));

	pPacket->TimeOut = xixcore_GetCurrentTime64()+ DEFAULT_REQUEST_MAX_TIMEOUT;
	pPacket->PkCtl.PktCompletion = &wait;
	pPacket->PkCtl.status=&waitStatus;


	printk(KERN_DEBUG "WAIT lock request\n");
	xixfs_Pkt_add_sendqueue(
			&xixfs_linux_global.EventCtx,
			pPacket 
			);
	
	wait_for_completion(&wait);
	
	printk(KERN_DEBUG "END WAIT lock request\n");
	
	if(waitStatus == LOCK_OWNED_BY_OWNER){
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit xixfs_AreYouHaveLotLock Lock is realy acquired by other \n"));
		return 1;
	}else{
		DebugTrace(DEBUG_LEVEL_CRITICAL, (DEBUG_TARGET_RESOURCE| DEBUG_TARGET_FCB|DEBUG_TARGET_LOCK),
			("Exit XifsdAreYouHaveLotLock Lock is status(0x%x) \n", waitStatus));
		return 0;
	}
}


xixcore_call
void
xixcore_NotifyChange(
	PXIXCORE_VCB XixcoreVcb,
	uint32 VCBMetaFlags
	)
{
	PXIXFS_LINUX_VCB pVCB = container_of(XixcoreVcb, XIXFS_LINUX_VCB, XixcoreVcb);

	spin_lock(&(pVCB->MetaCtx.MetaLock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
				("spin_lock(&(pCtx->MetaLock)) pCtx(%p)\n", &pVCB->MetaCtx ));
	XIXCORE_SET_FLAGS(pVCB->XixcoreVcb.MetaContext.VCBMetaFlags, VCBMetaFlags); 
	spin_unlock(&(pVCB->MetaCtx.MetaLock));
	DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_CHECK,
				("spin_unlock(&(pCtx->MetaLock)) pCtx(%p)\n", &pVCB->MetaCtx ));

	wake_up(&(pVCB->MetaCtx.VCBMetaEvent));
}
