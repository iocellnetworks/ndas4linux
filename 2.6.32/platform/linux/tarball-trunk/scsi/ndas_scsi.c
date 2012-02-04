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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/genhd.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/moduleparam.h>
#include <linux/scatterlist.h>
#include <linux/blkdev.h>
#include <scsi/scsi_host.h>
#include <scsi/scsicam.h>
#include <linux/stat.h>
#include "ndas_scsi.h"

struct ndas_scsi_dev {
	struct ndas_slot * ndas_dev;
};

struct ndas_slot* g_ndas_dev[NDAS_MAX_SLOT_NR] = {0};

static struct ndas_slot * host_to_ndas(struct Scsi_Host *hpnt)
{
	struct ndas_scsi_dev * ndev = NULL;
	ndev = (struct ndas_scsi_dev *)hpnt->hostdata;
	return ndev->ndas_dev;
}

#define NDAS_DEV_TAGGED_QUEUING	0
#define NDAS_SCSI_CANQUEUE  64 	/* needs to be >= 1 */


KMEM_CACHE* ndas_scsi_kmem_cache;

/* definition of virtual ndas_scsi_root */
static void ndas_scsi_0_release(struct device * dev)
{
    dbgl_scsi(1, "call  ndas_scsi_0_release");
}

static struct device ndas_scsi_root = {
    .bus_id		= "ndas_scsi_0",
    .release	= ndas_scsi_0_release,
};

/*definition of ndas_scsi host template */
char ndas_dbg_info[256];
static const char * ndas_scsi_host_info(struct Scsi_Host * shp)
{

    struct ndas_slot * ndas_dev = (struct ndas_slot *)host_to_ndas(shp);
	dbgl_scsi(3, "call  ndas_scsi_host_info");
	   
    sprintf(ndas_dbg_info, "ndas_scsi_info, dev name [%s], "
    	"ndas slot number=%d", ndas_dev->devname, ndas_dev->info.slot);
    return ndas_dbg_info;
}

static int ndas_scsi_slave_configure(struct scsi_device * sdp)
{
	struct ndas_slot * ndas_dev = NULL;
	int type = 0;
	
	dbgl_scsi(3, "call  ndas_scsi_slave_configure");
	
	ndas_dev = (struct ndas_slot *)host_to_ndas(sdp->host);
	type = ndas_dev->info.mode;

	if((ndas_dev->info.writable == 0) && (ndas_dev->info.writeshared == 0)) {
		sdp->writeable = 0;
	}
	sdp->use_10_for_ms = 1;
	ndas_dev->sdp = sdp;
	scsi_adjust_queue_depth(sdp, NDAS_DEV_TAGGED_QUEUING, sdp->host->cmd_per_lun);
	blk_queue_max_segment_size(sdp->request_queue, 64 * 1024);
	return 0;
}

static int ndas_scsi_ioctl(struct scsi_device *dev, int cmd, void __user *arg)
{
	dbgl_scsi(3, "call  ndas_scsi_ioct IOCTL CMD 0x%x", cmd);
	return -EINVAL;
}

static void ndas_scsi_set_sensedata(
	unsigned char *sensebuf,
	unsigned char sensekey,
	unsigned char additionalSenseCode,
	unsigned char additionalSenseCodeQaulifier
	)
{
	
	memset(sensebuf, 32, 0);
	sensebuf[0] = 0x70;  /* fixed, current */
	sensebuf[2] = sensekey;
	sensebuf[7] = 0xa;	  /* implies 18 byte sense buffer */
	sensebuf[12] = additionalSenseCode;
	sensebuf[13] = additionalSenseCodeQaulifier;	
}

static void ndas_scsi_set_result(int *result, unsigned char stat, unsigned char message , unsigned char host, unsigned char driver)
{
	int status = (stat || (message <<8) || (host << 16) || (driver << 24) );
	*result = status;
}



static void ndas_scsi_add_pc_to_slot(struct ndas_scsi_pc * pc,  struct ndas_slot * ndas_dev)
{
	unsigned long flags;

	dbgl_scsi(3, "call  ndas_scsi_add_pc_to_slot ");
	
	spin_lock_irqsave(&ndas_dev->q_list_lock, flags);
	list_add(&pc->q_entry, &ndas_dev->q_list_head);
	spin_unlock_irqrestore(&ndas_dev->q_list_lock, flags);
}


static void ndas_scsi_dell_pc_from_slot(struct ndas_scsi_pc * pc, struct ndas_slot * ndas_dev)
{
	unsigned long flags;
	dbgl_scsi(3, "call  ndas_scsi_dell_pc_to_slot ");
	spin_lock_irqsave(&ndas_dev->q_list_lock, flags);
	list_del(&pc->q_entry);
	spin_unlock_irqrestore(&ndas_dev->q_list_lock, flags);	
}

static void ndas_scsi_avort_pc_from_slot(struct scsi_cmnd *SCpnt, struct ndas_slot * ndas_dev)
{

	struct ndas_scsi_pc * pc = NULL;
	unsigned long flags;
	unsigned int dowait = 0;
	
	dbgl_scsi(3, "call  ndas_scsi_avort_pc_from_slot 0x%02x", SCpnt->cmnd[0]);
	spin_lock_irqsave(&ndas_dev->q_list_lock, flags);
	list_for_each_entry(pc, &ndas_dev->q_list_head, q_entry) {
		if(pc->org_cmd == SCpnt) {
			pc->avorted = 1;
			atomic_inc(&ndas_dev->wait_count);
			dowait =1;
		}
	}
	spin_unlock_irqrestore(&ndas_dev->q_list_lock, flags);

	if(dowait) {
		wait_event_interruptible(ndas_dev->wait_abort, 0);
	}
	
}


static void ndas_scsi_avort_all_pc_from_slot(struct ndas_slot * ndas_dev)
{

	struct ndas_scsi_pc * pc = NULL;
	unsigned long flags;
	unsigned int dowait = 0;

	dbgl_scsi(3, "call  ndas_scsi_avort_all_pc_from_slot");
	spin_lock_irqsave(&ndas_dev->q_list_lock, flags);
	list_for_each_entry(pc, &ndas_dev->q_list_head, q_entry) {
		pc->avorted = 1;
		atomic_inc(&ndas_dev->wait_count);
		dowait =1;
	}
	spin_unlock_irqrestore(&ndas_dev->q_list_lock, flags);

	if(dowait) {
		wait_event_interruptible(ndas_dev->wait_abort, atomic_read(&ndas_dev->wait_count) == 0);
	}
}



static struct ndas_scsi_pc * ndas_scsi_make_pc(struct scsi_cmnd * SCpnt)
{
	struct ndas_scsi_pc * req = NULL;
	nads_io_pc_request  * pcReq = NULL;
	ndas_io_request       *ndas_io_req = NULL;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25))
	struct scsi_data_buffer * sdb = NULL;
	sdb = scsi_in(SCpnt);
#endif
	dbgl_scsi(3, "call  ndas_scsi_make_pc 0x%02x", SCpnt->cmnd[0]);
	req = kmem_cache_alloc(ndas_scsi_kmem_cache, GFP_ATOMIC);
	if(!req) {
		printk(KERN_INFO "ndas_scsi_make_pc: err fail to alloc req\n"); 
		return NULL;
	}

	
	memset(req, 0, MAX_NDAS_SCSI_BIO_SIZE);
	INIT_LIST_HEAD(&req->q_entry);
	

	/*initialize ndas_io_pc_request */
	pcReq = &(req->ndas_add_req_block);
	memcpy(pcReq->cmd, SCpnt->cmnd, SCpnt->cmd_len);
	pcReq->cmdlen = SCpnt->cmd_len;
	pcReq->status = 0;

	switch(SCpnt->sc_data_direction){
		case DMA_TO_DEVICE:
			pcReq->direction = DATA_TO_DEV;
			break;
		case DMA_FROM_DEVICE:
			pcReq->direction = DATA_FROM_DEV;
			break;
		default:
			pcReq->direction = DATA_NONE;
			break;
	}



	/*set buffer */
	ndas_io_req = &(req->ndas_req);
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )

	ndas_io_req->nr_uio = scsi_sg_count(SCpnt);

	if (scsi_sg_count(SCpnt) > 0) {
		struct scatterlist *sg = NULL;
		int i = 0;
		struct page * pPage = NULL;
		char * data = NULL;
		
		for_each_sg(sdb->table.sgl, sg, sdb->table.nents, i) {
			pPage = sg_page(sg);
#ifdef 	CONFIG_HIGHMEM
			if( is_highmem(page_zone(pPage)) ) {
				req->blocks[i].private = pPage;
				req->blocks[i].ptr = kmap(pPage) + sg->offset;
				req->blocks[i].len = sg->length;
			}else {
#endif //#ifdef 	CONFIG_HIGHMEM			
				data = page_address(pPage) + sg->offset;
#ifdef 	CONFIG_HIGHMEM
				req->blocks[i].private = NULL;
#endif //#ifdef 	CONFIG_HIGHMEM
				req->blocks[i].ptr= data;
				req->blocks[i].len = sg->length;
#ifdef 	CONFIG_HIGHMEM
			}
#endif //#ifdef 	CONFIG_HIGHMEM			
		}

		ndas_io_req->uio = req->blocks;
	}

	ndas_io_req->reqlen = scsi_bufflen(SCpnt);
	
#else //#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )

	
	ndas_io_req->nr_uio = SCpnt->use_sg;
	if(ndas_io_req->nr_uio) {
		int i = 0;
		struct scatterlist *sg = NULL;
		char * data = NULL;
		
		
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
		struct page * pPage = NULL;
		sg = (struct scatterlist *)SCpnt->request_buffer;
		for(i = 0; i< ndas_io_req->nr_uio; i++)
		{
			pPage = (struct page *)(sg[i].page_link & ~0x3);
#ifdef 	CONFIG_HIGHMEM
			if(is_highmem(page_zone(pPage))) {
		
				req->blocks[i].private = pPage;
				req->blocks[i].ptr = kmap(pPage) + sg[i].offset;
				req->blocks[i].len = sg[i].length;
			}else {
#endif //#ifdef 	CONFIG_HIGHMEM
				data = page_address(pPage) + sg[i].offset;
#ifdef 	CONFIG_HIGHMEM
				req->blocks[i].private = NULL;
#endif //#ifdef 	CONFIG_HIGHMEM
				req->blocks[i].ptr= data;
				req->blocks[i].len = sg[i].length;
#ifdef 	CONFIG_HIGHMEM
			}
#endif //#ifdef 	CONFIG_HIGHMEM
		}
#else //#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))

		sg = (struct scatterlist *)SCpnt->request_buffer;
		for(i = 0; i< ndas_io_req->nr_uio; i++)
		{
#ifdef 	CONFIG_HIGHMEM
			if(is_highmem(page_zone(sg[i].page))) {
		
				req->blocks[i].private = sg[i].page;
				req->blocks[i].ptr = kmap(sg[i].page) + sg[i].offset;
				req->blocks[i].len = sg[i].length;
			}else {
#endif //#ifdef 	CONFIG_HIGHMEM
				data = page_address(sg[i].page) + sg[i].offset;
#ifdef 	CONFIG_HIGHMEM
				req->blocks[i].private = NULL;
#endif //#ifdef 	CONFIG_HIGHMEM
				req->blocks[i].ptr= data;
				req->blocks[i].len = sg[i].length;
#ifdef 	CONFIG_HIGHMEM
			}
#endif //#ifdef 	CONFIG_HIGHMEM
		}
#endif //#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))

		ndas_io_req->uio = req->blocks;
	}else {;
		ndas_io_req->buf = SCpnt->request_buffer;
	}

	
	ndas_io_req->reqlen = SCpnt->request_bufflen;
#endif //#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
	
	ndas_io_req->additionalInfo = (void *)pcReq;
	req->org_cmd = SCpnt;
	return req;
}




static void ndas_scsi_end_io(int slot, ndas_error_t err, struct ndas_scsi_pc *ndas_scsi_req)
{
	struct ndas_slot * ndas_dev = NULL;
	struct Scsi_Host *host = NULL;
	ndas_io_request * ndas_io_req = &(ndas_scsi_req->ndas_req);
	nads_io_pc_request  * pcReq = &(ndas_scsi_req->ndas_add_req_block);

	unsigned char driver = 0;
	struct scsi_cmnd * SCpnt = NULL;
	void (*done)(struct scsi_cmnd *);

	ndas_dev = ndas_scsi_req->org_dev;	
	ndas_scsi_dell_pc_from_slot(ndas_scsi_req, ndas_dev);


	if(ndas_io_req->nr_uio){
	
#ifdef CONFIG_HIGHMEM
		int i;
		for (i = 0; i < ndas_io_req->nr_uio; i++) {
			if ( ndas_scsi_req->blocks[i].private ) 
				kunmap((struct page *)ndas_scsi_req->blocks[i].private);
		}
#endif
	
	}	

	SCpnt = ndas_scsi_req->org_cmd;
		done = ndas_scsi_req->done;
		host =  SCpnt->device->host;
		
	if(ndas_scsi_req->avorted == 0) {
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
		struct scsi_data_buffer * sdb = NULL;
		sdb = scsi_in(SCpnt);
#endif
		if(pcReq->avaliableSenseData) {
			memcpy(SCpnt->sense_buffer, pcReq->sensedata, 32);
			driver = DRIVER_SENSE;
		}

		if(pcReq->requestSenseData) {
			driver |= SUGGEST_SENSE;
		}


		if(pcReq->resid) {
#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
			sdb->resid = pcReq->resid;
#else
			SCpnt->resid = pcReq->resid;
#endif
		}
		switch(pcReq->status) {
			case NDAS_SCSI_STATUS_SUCCESS:
			{	
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_GOOD,
										0 ,
										DID_OK ,
										0);
			}
			break;
			case NDAS_SCSI_STATUS_BUSY:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_GOOD,
										0 ,
										DID_BUS_BUSY ,
										driver);
			}
			break;
			case NDAS_SCSI_STATUS_STOP:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_GOOD,
										0 ,
										DID_NO_CONNECT,
										driver);								
			}
			break;
			case NDAS_SCSI_STATUS_NOT_EXIST:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_GOOD,
										0 ,
										DID_NO_CONNECT ,
										driver);				
			}
			break;
			case NDAS_SCSI_STATUS_INVALID_COMMAND:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_GOOD,
										0 ,
										DID_ERROR,
										driver);								
			}
			break;
			case NDAS_SCSI_STATUS_DATA_OVERRUN:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_GOOD,
										0 ,
										DID_OK ,
										driver);								
			}
			break;
			case NDAS_SCSI_STATUS_LOST_LOCK:
			case NDAS_SCSI_STATUS_COMMAND_FAILED:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_CHECK_CONDITION,
										0 ,
										DID_ERROR ,
										driver);
			}
			break;
			case NDAS_SCSI_STATUS_RESET:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_GOOD,
										0 ,
										DID_ERROR,
										driver);								
			}
			break;
			case NDAS_SCSI_STATUS_COMMUNICATION_ERROR:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_CHECK_CONDITION,
										0 ,
										DID_BAD_TARGET ,
										driver);								
			}
			break;
			case NDAS_SCSI_STATUS_COMMMAND_DONE_SENSE:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_CHECK_CONDITION,
										0 ,
										DID_OK ,
										driver);				
			}
			break;
 			case NDAS_SCSI_STATUS_COMMMAND_DONE_SENSE2:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_CHECK_CONDITION,
										0 ,
										DID_ERROR ,
										driver);				
			}
			break;      			
			case NDAS_SCSI_STATUS_NO_ACCESS:
			{
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_GOOD,
										0 ,
										DID_ERROR,
										driver);								
			}
			break;
			default:
				ndas_scsi_set_result(&SCpnt->result, 
										SAM_STAT_GOOD,
										0 ,
										DID_ERROR,
										driver);						
			break;
		}
		
		dbgl_scsi(3, "result ndas_scsi_end_io Cmd:0x%02x result:%d", SCpnt->cmnd[0], SCpnt->result);
		dbgl_scsi(3,"\n");
		kmem_cache_free(ndas_scsi_kmem_cache ,ndas_scsi_req);
		
		done(SCpnt);
		return;
	}else {
		ndas_scsi_set_result(&SCpnt->result, 
							SAM_STAT_GOOD,
							0,
							DID_ABORT,
							0
							);
							
		kmem_cache_free(ndas_scsi_kmem_cache ,ndas_scsi_req);
		atomic_dec(&ndas_dev->wait_count);
		dbgl_scsi(3, "aborted cmd ndas_scsi_end_io Cmd:0x%02x result:%d", SCpnt->cmnd[0], SCpnt->result);
		dbgl_scsi(3,"\n");
		wake_up(&ndas_dev->wait_abort);
		return;
	}
	
}


static void ndas_scsi_real_end_io(int slot, ndas_error_t err, struct ndas_scsi_pc * ndas_scsi_req)
{
		struct scsi_cmnd * SCpnt = NULL;
		struct Scsi_Host *host = NULL;		
		struct ndas_slot * ndas_dev = NULL;
		
		
		ndas_dev = ndas_scsi_req->org_dev;
		
		
		SCpnt = ndas_scsi_req->org_cmd;
		host = SCpnt->device->host;
		dbgl_scsi(3, "ndas_scsi_real_end_io 0x%02x", 
				SCpnt->cmnd[0]);
		
		spin_lock_irq(host->host_lock);
		ndas_scsi_end_io(slot, err, ndas_scsi_req);
		spin_unlock_irq(host->host_lock);
		
		return;
}


static int ndas_scsi_queuecommand(struct scsi_cmnd * SCpnt, void (*done)(struct scsi_cmnd *))
{

	struct ndas_slot * ndas_dev = NULL;
	unsigned char *cmd = NULL;
	unsigned char cmdlen = 0;
	struct ndas_scsi_pc *ndas_scsi_req = NULL;
	ndas_io_request        *ndas_io_req = NULL;
	struct Scsi_Host *host = SCpnt->device->host;
	
	int ret = 0;
	int slot = 0;

	if (done == NULL)
		return 0;	


	if(SCpnt->request) {
		if(SCpnt->request->rq_disk) {
			dbgl_scsi(3, "call ndas_scsi_queuecommand CMD:0x%02x CMDLEN[%d] DEV[%s]", 
				SCpnt->cmnd[0], SCpnt->cmd_len, SCpnt->request->rq_disk->disk_name);
		}else {
			dbgl_scsi(3, "call ndas_scsi_queuecommand CMD:0x%02x CMDLEN[%d]", SCpnt->cmnd[0], SCpnt->cmd_len);
		}
	}else{
		dbgl_scsi(3, "call ndas_scsi_queuecommand CMD:0x%02x CMDLEN [%d]", SCpnt->cmnd[0], SCpnt->cmd_len);
	}
	
	ndas_dev = (struct ndas_slot *)host_to_ndas(host);
	slot = ndas_dev->slot;
	cmd = (unsigned char *)SCpnt->cmnd;
	cmdlen = SCpnt->cmd_len;


	if( ndas_dev->info.mode == NDAS_DISK_MODE_ATAPI) {
		if(cmdlen > 12) {
			printk(KERN_INFO "ERR: ndas_scsi_queuecommand unsupported command len %d\n", cmdlen);
			ndas_scsi_set_sensedata(SCpnt->sense_buffer, SENSE_ILLEGAL_REQUEST, ADSENSE_ILLEGAL_COMMAND, 0);
			ndas_scsi_set_result(&SCpnt->result, SAM_STAT_GOOD,0 ,DID_ABORT ,0);
			done(SCpnt);
			return 0;
		}
	}else {
		if(cmdlen > MAX_SUPPORTED_COM_SIZE) {
			printk(KERN_INFO "ERR: ndas_scsi_queuecommand unsupported command len %d\n", cmdlen);
			ndas_scsi_set_sensedata(SCpnt->sense_buffer, SENSE_ILLEGAL_REQUEST, ADSENSE_ILLEGAL_COMMAND, 0);
			ndas_scsi_set_result(&SCpnt->result, SAM_STAT_GOOD,0 ,DID_ABORT ,0);
			done(SCpnt);
			return 0;
		}
	}


#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,25) )
	if( scsi_sg_count(SCpnt) > MAS_NDAS_SG_SEGMENT) {
		printk(KERN_INFO "ERR: ndas_scsi_queuecommand unsupported SGMEMNT SIZE %d\n", scsi_sg_count(SCpnt));
#else
	if(SCpnt->use_sg > MAS_NDAS_SG_SEGMENT) {
		printk(KERN_INFO "ERR: ndas_scsi_queuecommand unsupported SGMEMNT SIZE %d\n", SCpnt->use_sg);
#endif		
		ndas_scsi_set_result(&SCpnt->result, SAM_STAT_GOOD,0 ,DID_ERROR ,0);
		done(SCpnt);
		return 0;
	}

	if(SCpnt->sc_data_direction == DMA_BIDIRECTIONAL){
		printk(KERN_INFO "ERR: ndas_scsi_queuecommand unsupported data direction%d\n", SCpnt->sc_data_direction);
		ndas_scsi_set_sensedata(SCpnt->sense_buffer, SENSE_ILLEGAL_REQUEST, ADSENSE_ILLEGAL_COMMAND, 0);
		ndas_scsi_set_result(&SCpnt->result, SAM_STAT_GOOD,0 ,DID_ABORT ,0);
		done(SCpnt);
		return 0;		
	}

	switch(SCpnt->cmnd[0]){
		case 0xf5:
		{
			dbgl_scsi(3, "Unsupported CMD:0x%02x", SCpnt->cmnd[0]);
			ndas_scsi_set_sensedata(SCpnt->sense_buffer, SENSE_ILLEGAL_REQUEST, ADSENSE_ILLEGAL_COMMAND, 0);
			ndas_scsi_set_result(&SCpnt->result, SAM_STAT_GOOD,0 ,DID_ABORT ,0);
			done(SCpnt);
			return 0;								
		}
		break;
		case ATA_16:
		{
			if(ndas_dev->info.mode == NDAS_DISK_MODE_ATAPI) {
				dbgl_scsi(3, "Unsupported CMD:0x%02x", SCpnt->cmnd[0]);
				ndas_scsi_set_sensedata(SCpnt->sense_buffer, SENSE_ILLEGAL_REQUEST, ADSENSE_ILLEGAL_COMMAND, 0);
				ndas_scsi_set_result(&SCpnt->result, SAM_STAT_GOOD,0 ,DID_ABORT ,0);
				done(SCpnt);
				return 0;								
			}else {
				dbgl_scsi(3, "SAT: CMD:0x%02x RL CMD:0x%02x", SCpnt->cmnd[0], SCpnt->cmnd[0]);
			}
		}
		break;
		default:
		break;	
	}


	SCpnt->scsi_done = done;

	
	
	ndas_scsi_req = ndas_scsi_make_pc(SCpnt);
	if(!ndas_scsi_req) {
		printk(KERN_INFO "ERR: ndas_scsi_queuecommand Fail allocation ndas_scsi req\n");
		ndas_scsi_set_sensedata(SCpnt->sense_buffer, SENSE_ILLEGAL_REQUEST, ADSENSE_ILLEGAL_COMMAND, 0);
		ndas_scsi_set_result(&SCpnt->result, SAM_STAT_GOOD,0 ,DID_BUS_BUSY,0);
		done(SCpnt);
		return 0;
	}

	
	ndas_scsi_req->done = done;
	ndas_scsi_req->org_cmd = SCpnt;
	ndas_scsi_req->org_dev = ndas_dev;
	
	ndas_io_req = (ndas_io_request  *)&(ndas_scsi_req->ndas_req);
#ifndef NDAS_SYNC_IO
    	ndas_io_req->done = (ndas_io_done) ndas_scsi_real_end_io;
    	ndas_io_req->done_arg = ndas_scsi_req;
	
#else
    	ndas_io_req->done = NULL;
    	ndas_io_req->done_arg = NULL;
#endif

	
	ndas_scsi_add_pc_to_slot(ndas_scsi_req, ndas_dev);
	

	spin_unlock_irq(host->host_lock);
	ret = ndas_packetcmd(slot, ndas_io_req);
	spin_lock_irq(host->host_lock);


#ifndef NDAS_SYNC_IO
    	if (NDAS_MORE_PROCESSING_RQ == ret) {
		return 0;
	}else {
		ndas_scsi_end_io(slot, ret, ndas_scsi_req);
		
    	}
#else
    	ndas_scsi_end_io(slot, ret, ndas_scsi_req);
#endif
	return 0;
}


static int ndas_scsi_abort(struct scsi_cmnd * SCpnt)
{
	struct ndas_slot * ndas_dev = NULL;
	dbgl_scsi(3, "call ndas_scsi_abort");	
	if(!SCpnt) return SUCCESS;
	ndas_dev = (struct ndas_slot *)host_to_ndas(SCpnt->device->host);
	ndas_scsi_avort_pc_from_slot(SCpnt, ndas_dev);
	SCpnt->scsi_done(SCpnt);
	return SUCCESS;
}

static int ndas_scsi_host_reset(struct scsi_cmnd * SCpnt)
{
	struct ndas_slot * ndas_dev = NULL;
	dbgl_scsi(3, "call ndas_scsi_host_reset");	
	ndas_dev = (struct ndas_slot *)host_to_ndas(SCpnt->device->host);
	ndas_scsi_avort_all_pc_from_slot(ndas_dev);
	SCpnt->scsi_done(SCpnt);
	return SUCCESS;
}


static struct scsi_host_template ndas_scsi_host_template = {
    .module                 = THIS_MODULE,
	.name =			"ndas_scsi",
	.info =			ndas_scsi_host_info,
	.slave_configure =	ndas_scsi_slave_configure,
	.ioctl =			ndas_scsi_ioctl,
	.queuecommand =		ndas_scsi_queuecommand,
	.eh_abort_handler =	ndas_scsi_abort,
	.eh_host_reset_handler = ndas_scsi_host_reset,
	.can_queue =			NDAS_SCSI_CANQUEUE,
	.this_id =		-1,
	.sg_tablesize =		MAS_NDAS_SG_SEGMENT,
	.cmd_per_lun =		1,
	.max_sectors =		128,
	.unchecked_isa_dma = 	0,
	.use_clustering = 	DISABLE_CLUSTERING,
};

/* definition of virtual ndas_scsi_bus type */
static int ndas_scsi_bus_match(struct device *dev,
                          struct device_driver *dev_driver)
{
        return 1;
}



static int ndas_scsi_bus_probe(struct device * dev)
{
	int error = 0;
	struct ndas_slot * ndas_dev;
	struct Scsi_Host *hpnt;
	struct ndas_scsi_dev *ndev = NULL;
	
	ndas_dev = dev_to_ndas_slot(dev);
	dbgl_scsi(3, "call ndas_scsi_bus_probe");	
	dbgl_scsi(3, "ndas_slot address=0x%p OK", ndas_dev);

	if(ndas_dev->info.mode == NDAS_DISK_MODE_ATAPI){
		ndas_scsi_host_template.max_sectors = 32;
	}

	
	hpnt = scsi_host_alloc(&ndas_scsi_host_template, sizeof(struct ndas_scsi_dev));
	if (NULL == hpnt) {
		printk(KERN_ERR "%s: scsi_register failed\n", __FUNCTION__);
		error = -ENODEV;
		return error;
	}
	
	ndas_dev->shost = hpnt;
	ndev = (struct ndas_scsi_dev *)hpnt->hostdata;
	ndev->ndas_dev = ndas_dev;
/* 
*	do something more configuration device based on ndas_slot info : disk: odd: media juke:.....
*/
	hpnt->max_id =1;
	hpnt->max_lun = 1;
	hpnt->max_channel = 0;
		
	if ( ndas_dev->info.mode == NDAS_DISK_MODE_ATAPI) {
		hpnt->max_cmd_len = 12;
	}else {
		hpnt->max_cmd_len = MAX_SUPPORTED_COM_SIZE;
	}


	error = scsi_add_host(hpnt, &ndas_dev->dev);
	if (error) {
		printk(KERN_ERR "%s: scsi_add_host failed\n", __FUNCTION__);
		error = -ENODEV;
		scsi_host_put(hpnt);
	} else
		scsi_scan_host(hpnt);
	return error;
}



static int ndas_scsi_bus_remove(struct device * dev)
{
    struct ndas_slot *ndas_dev;

	dbgl_scsi(3, "call ndas_scsi_bus_remove");	
    ndas_dev = dev_to_ndas_slot(dev);

    if (!ndas_dev) {
        printk(KERN_ERR "%s: Unable to locate host info\n",
               __FUNCTION__);
        return -ENODEV;
    }
 
    scsi_remove_host(ndas_dev->shost);

    /* do something more */

    scsi_host_put(ndas_dev->shost);
    return 0;
}


static struct bus_type ndas_scsi_bus = {
    .name = "ndas_scsi",
    .match = ndas_scsi_bus_match,
#if LINUX_VERSION_UPDATE_DRIVER    
    .probe = ndas_scsi_bus_probe,
    .remove = ndas_scsi_bus_remove,
#else
#endif    
};

static char ndas_scsi_driver_name[] = "ndas_scsi_d";

static struct device_driver ndas_scsi_driver = {
	.name 		= ndas_scsi_driver_name,
	.bus		= &ndas_scsi_bus,
#if LINUX_VERSION_UPDATE_DRIVER
#else
	.probe = ndas_scsi_bus_probe,
	.remove = ndas_scsi_bus_remove,	
#endif
};



static void ndas_scsi_release_adapter(struct device * dev)
{
	//struct ndas_slot * ndas_dev = dev_to_ndas_slot(dev);
	dbgl_scsi(3, "call ndas_scsi_release_adapter\n");
	printk(KERN_INFO "COMPLETE REMOVE NDAS_SCSI ADAPTOR!\n");
	
	return ;
	
}

int ndas_scsi_add_adapter(struct device *dev)
{
        struct ndas_slot * ndas_dev = dev_to_ndas_slot(dev);
	 int error = 0;

	memset(dev, 0, sizeof(struct device));
	spin_lock_init(&ndas_dev->q_list_lock);
	INIT_LIST_HEAD(&ndas_dev->q_list_head);
	init_waitqueue_head(&ndas_dev->wait_abort);
	atomic_set(&ndas_dev->wait_count, 0);
		
	 dbgl_scsi(3, "call ndas_scsi_add_adapter");
	 dbgl_scsi(3, "ndas_slot address=0x%p OK", ndas_dev);
        dev->bus = &ndas_scsi_bus;
        dev->parent = &ndas_scsi_root;
        dev->release = &ndas_scsi_release_adapter;
        sprintf(dev->bus_id, "adapter%d", ndas_dev->slot);
        error = device_register(dev);
        if (error)
		goto clean;
        return error;
clean: 
        return error;
}


int ndas_scsi_can_remove(struct device *dev)
{
	struct ndas_slot *ndas_dev = dev_to_ndas_slot(dev);
	struct Scsi_Host *hpnt = NULL;
	int ret = 0;
	
	
	down(&ndas_dev->mutex);
	if ( !ndas_dev->enabled ) {
		dbgl_scsi(1, "ed slot=%d is already disabled",ndas_dev->slot);    
		up(&ndas_dev->mutex);
		goto out;
	}
	


	hpnt = ndas_dev->shost;
 	scsi_block_requests(hpnt);
	spin_lock_irq(hpnt ->host_lock);
	ret = (hpnt->host_busy > 0) ? 0 : 1; 
	spin_unlock_irq(hpnt ->host_lock);

	if(!ret) {
		printk(KERN_INFO "NDAS_SCSI FAIL STOP: DEVICE BUSY! \n");
		scsi_unblock_requests(hpnt);
	}else {
		struct scsi_device * sdp = NULL;

		sdp = ndas_dev->sdp;
		if(sdp->lockable && sdp->locked) {
			printk(KERN_INFO "NDAS_SCSI FAIL STOP: media is diabled to remove! \n");
			printk(KERN_INFO "check mounted media and eject media \n");
			scsi_unblock_requests(hpnt);
			ret = 0;
		}else {
			scsi_unblock_requests(hpnt);
		}
	}

	up(&ndas_dev->mutex);
	return ret;
out:
	return 1;
}


void ndas_scsi_release_all_req_from_slot(struct ndas_slot * ndas_dev)
{

	struct ndas_scsi_pc * pc = NULL;
	unsigned long flags;
	struct scsi_cmnd * SCpnt = NULL;
	
	dbgl_scsi(3, "call  ndas_scsi_avort_all_pc_from_slot");
	spin_lock_irqsave(&ndas_dev->q_list_lock, flags);


	while (!list_empty(&ndas_dev->q_list_head)) {
		pc = list_first_entry(&ndas_dev->q_list_head, struct ndas_scsi_pc, q_entry);
		
		list_del(&pc->q_entry);
		SCpnt = pc->org_cmd;
		dbgl_scsi(3, "bug in resource free pc(0x%p) cmd[0x%02x]\n", pc, SCpnt->cmnd[0]);
		ndas_scsi_set_sensedata(SCpnt->sense_buffer, SENSE_HARDWARE_ERROR, ADSENSE_LUN_COMMUNICATION   , 0);
		ndas_scsi_set_result(&SCpnt->result, SAM_STAT_TASK_ABORTED,0 ,DID_NO_CONNECT ,0);							
		

		if(atomic_read(&ndas_dev->wait_count) > 0) {
			atomic_dec(&ndas_dev->wait_count);
			wake_up(&ndas_dev->wait_abort);
		}else {
			SCpnt->scsi_done(SCpnt);
		}
		kmem_cache_free(ndas_scsi_kmem_cache ,pc);
	}
	spin_unlock_irqrestore(&ndas_dev->q_list_lock, flags);
}


void ndas_scsi_remove_adapter(struct device *dev)
{
	struct ndas_slot * ndas_dev = dev_to_ndas_slot(dev);
	printk(KERN_INFO "REMOVE NDAS_SCSI ADAPTOR! sd->enabled=%d \n", ndas_dev->enabled);
	
	device_unregister(dev);
	ndas_scsi_release_all_req_from_slot(ndas_dev);
	return;	
}





int ndas_scsi_init(void)
{
	int ret;

	dbgl_scsi(3, "call ndas_scsi_init");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23))
	ndas_scsi_kmem_cache = kmem_cache_create("ndas_scsi_req", MAX_NDAS_SCSI_BIO_SIZE, 0, SLAB_HWCACHE_ALIGN,
	    NULL);
#else
	ndas_scsi_kmem_cache = kmem_cache_create("ndas_scsi_req", MAX_NDAS_SCSI_BIO_SIZE, 0, SLAB_HWCACHE_ALIGN,
	    NULL, NULL);
#endif


	if (!ndas_scsi_kmem_cache) {
	    printk(KERN_ERR "ndas_scsi: unable to create kmem cache\n");
	    ret = -ENOMEM;
	    return ret;
	}


	
	ret = device_register(&ndas_scsi_root);
	if (ret < 0) {
		printk(KERN_WARNING "ndas_scsi: device_register error: %d\n",
			ret);
		goto dev_free_cache;
	}
	ret = bus_register(&ndas_scsi_bus);
	if (ret < 0) {
		printk(KERN_WARNING "ndas_scsi: bus_register error: %d\n",
			ret);
		goto dev_unreg;
	}

	ret = driver_register(&ndas_scsi_driver);	
	if(ret < 0) {
		printk(KERN_WARNING "ndas_scsi: driver_register error: %d\n",
			ret);
		goto dev_bus_unreg;
	}
	return ret;

dev_bus_unreg:
	bus_unregister(&ndas_scsi_bus);
dev_unreg:
	device_unregister(&ndas_scsi_root);
dev_free_cache:
	if (ndas_scsi_kmem_cache) {
		kmem_cache_destroy(ndas_scsi_kmem_cache);
		ndas_scsi_kmem_cache = NULL;
	}
	return ret;
}

void ndas_scsi_exit(void)
{
	dbgl_scsi(3, "call ndas_scsi_exit");
	driver_unregister(&ndas_scsi_driver);	
	bus_unregister(&ndas_scsi_bus);
	device_unregister(&ndas_scsi_root);
	if (ndas_scsi_kmem_cache) {
		kmem_cache_destroy(ndas_scsi_kmem_cache);
		ndas_scsi_kmem_cache = NULL;
	}
}
