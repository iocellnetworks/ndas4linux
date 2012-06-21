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
#include <linux/fs.h> // blkdev_ioctl
#include <linux/buffer_head.h> // file_fsync
#include <linux/genhd.h>  // rescan_parition
#include <linux/workqueue.h>  // 
#else
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
#endif
#include <linux/netdevice.h> // dev_base dev_base_lock
#include <linux/slab.h> // kmalloc, kfree
#include <linux/blkpg.h> // BLKPG
#include <linux/module.h> // module_put for 2.6, MOD_INC/DEC for 2.4
#include <asm/uaccess.h> // VERIFY_WRITE
#if LINUX_VERSION_AVOID_CFQ_SCHEDULER
#include <linux/sysfs.h>
#endif

#if !LINUX_VERSION_DEVFS_REMOVED_COMPLETELY
#ifdef NDAS_DEVFS
#ifndef CONFIG_DEVFS_FS
#error This kernel config does not support devfs.
#endif
#else
#include <linux/devfs_fs_kernel.h>
#endif
#endif

#include <sal/debug.h>
#include <sal/types.h>
#include <sal/sync.h>
#include <sal/libc.h>
#include <ndasuser/ndasuser.h>
#include <ndasuser/write.h>

#ifdef XPLAT_XIXFS_EVENT
#include <xixfsevent/xixfs_event.h>
#endif //#ifdef XPLAT_XIXFS_EVENT


#include "block.h"
#include "ops.h"
#include "ndasdev.h"
#include "procfs.h"

#ifdef NDAS_MSHARE
#include <linux/cdrom.h>
#include <ndasuser/mediaop.h>


#define IOCTL_IN(arg, type, in)                         \
		if(copy_from_user(&(in), (type *) (arg), sizeof (in)))\
		return -EFAULT;
#define IOCTL_OUT(arg, type, out)                       \
		if(copy_to_user((type *) (arg), &(out), sizeof (out)))\
		return -EFAULT;
		
#define CM_READ 0
#define CM_WRITE 1
#endif //#ifdef NDAS_MSHARE


#ifdef DEBUG
#define LOCAL
#else
#define LOCAL static
#endif

#define OPEN_WRITE_BIT 16

#define DEBUG_GENDISK(g) \
({ \
    static char __buf__[1024];\
    if ( g ) \
        snprintf(__buf__,sizeof(__buf__), \
            "%p={major=%d,first_minor=%d,minors=0x%x,disk_name=%s," \
            "private_data=%p,capacity=%lld}", \
            g,\
            g->major, \
            g->first_minor,\
            g->minors,\
            g->disk_name, \
            g->private_data, \
            (long long)g->capacity \
        ); \
    else \
        snprintf(__buf__,sizeof(__buf__), "NULL"); \
    (const char*) __buf__; \
})

struct ndas_slot* g_ndas_dev[NDAS_MAX_SLOT_NR] = {0};

#if LINUX_VERSION_25_ABOVE

#else

static
struct gendisk ndas_gd = {
    .major =        NDAS_BLK_MAJOR,
    .major_name =    "nd",
    .minor_shift =    PARTN_BITS, 
    .max_p =        (1<<PARTN_BITS),
    .part =        ndas_hd,
    .sizes =        ndas_sizes,
    .fops =        &ndas_fops,
};

void ndas_ops_invalidate_slot(int slot) 
{
    int i;
    int start;

    start = (slot - NDAS_FIRST_SLOT_NR) << ndas_gd.minor_shift;
    for(i = ndas_gd.max_p - 1; i>=0; i--) 
    {
            int minor = start + i;
            invalidate_device(MKDEV(NDAS_BLK_MAJOR, minor), 0);
            ndas_gd.part[minor].start_sect = 0;
            ndas_gd.part[minor].nr_sects = 0 ;
    }
}

ndas_error_t ndas_ops_read_partition(int slot) 
{
    int start,i;
    long sectors;

    dbgl_blk(3, "slot=%d",slot );

    start = (slot - NDAS_FIRST_SLOT_NR) << ndas_gd.minor_shift;

    for(i = ndas_gd.max_p - 1; i>=0; i--) 
    {
        int minor = start + i;
        invalidate_device(MKDEV(NDAS_BLK_MAJOR, minor), 1);

        ndas_gd.part[minor].start_sect = 0;
        ndas_gd.part[minor].nr_sects = 0 ;
    }

    sectors = NDAS_GET_SLOT_DEV(slot)->info.sectors;
#ifdef NDAS_MSHARE
   if(NDAS_GET_SLOT_DEV(slot)->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
   {
#endif

    dbgl_blk(3, "sectors=%ld",sectors);
    grok_partitions(
        &ndas_gd, 
        (slot - NDAS_FIRST_SLOT_NR), 
        1<<PARTN_BITS, 
        sectors
    );

    for(i=0; i<NR_PARTITION; i++) {
        dbgl_blk(4, " Part %d: (%ld,%ld) ", 
            i,
            ndas_hd[((slot - NDAS_FIRST_SLOT_NR)<<PARTN_BITS)+i].start_sect, 
            ndas_hd[((slot - NDAS_FIRST_SLOT_NR)<<PARTN_BITS)+i].nr_sects);
    }
    
#ifdef NDAS_MSHARE
   }else{

   grok_partitions(
        &ndas_gd, 
        (slot - NDAS_FIRST_SLOT_NR), 
        1<<PARTN_BITS, 
        sectors
        );

   	for(i = ndas_gd.max_p - 1; i >= 0; i--)
   	{
   		int minor = start + i;
   		ndas_gd.part[minor].start_sect = 0;
   		ndas_gd.part[minor].nr_sects = (1024*1024*8*2);
   	}
   }
#endif
    dbgl_blk(3, "ed");
    return NDAS_OK;

}
#endif

/**
 * Open the NDAS device file.
 */
int ndop_open(struct inode *inode, struct file *filp)
{
#if !LINUX_VERSION_25_ABOVE
    int part;
#endif
    int                slot;
    ndas_error_t error;
    
    dbgl_blk(3, "ing inode=%p",inode);

    if(!filp) {
        dbgl_blk(1,"Weird, open called with filp = 0\n");
        return -EIO;
    }

    MOD_INC_USE_COUNT;
    
    slot = SLOT_I(inode);
    dbgl_blk(2,"slot =0x%x", slot);
    dbgl_blk(2,"dev_t=0x%x", inode->i_bdev->bd_dev);
    
    if (slot > NDAS_MAX_SLOT_NR) {
        printk("ndas: minor too big.\n"); 
        ndop_release(inode, filp);
        return -ENXIO;
    }

    if ( filp->f_flags & O_NDELAY ) {
        dbgl_blk(3, "ed O_NDELAY");
        return 0;
    }
    
    if ( NDAS_GET_SLOT_DEV(slot) == NULL ) 
        return -ENODEV;
    if ( NDAS_GET_SLOT_DEV(slot)->enabled == 0 ) {
        dbgl_blk(3, "ed not enabled");
        printk("ndas: device is not enabled\n");
        ndop_release(inode, filp);
        return -ENODEV;
    }

    error = ndas_query_slot(slot, &g_ndas_dev[slot - NDAS_FIRST_SLOT_NR]->info);
    if ( !NDAS_SUCCESS(NDAS_OK) ) {
        dbgl_blk(3, "ed can't get info");
        printk("ndas: device is not reachable\n");
        ndop_release(inode, filp);
        return -ENODEV;
    }
    
#if !LINUX_VERSION_25_ABOVE
    part = PARTITION(inode->i_rdev);
    if(NDAS_GET_SLOT_DEV(slot)->ref[part] == -1
        || (NDAS_GET_SLOT_DEV(slot)->ref[part] && (filp->f_flags & O_EXCL))) 
    {
        dbgl_blk(2, "2:adapter_state->nd_ref[%d] = %d\n", part, 
                NDAS_GET_SLOT_DEV(slot)->ref[part]);
        ndop_release(inode, filp);
        printk("ndas: device is not exclusively opened\n");
        return -EBUSY;
    }

    if(filp->f_flags & O_EXCL)
        NDAS_GET_SLOT_DEV(slot)->ref[part] = -1;
    else
        NDAS_GET_SLOT_DEV(slot)->ref[part] ++;

    if (filp->f_flags & O_NDELAY) {
        dbgl_blk(3, "adapter_state->nd_ref[%d] = %d\n", part, 
                NDAS_GET_SLOT_DEV(slot)->ref[part]);
        return 0;
    }
#endif
    dbgl_blk(3, "filp->f_mode=%x",filp->f_mode); 
    dbgl_blk(3, "filp->f_flags=%x",filp->f_flags); 
    
    if (filp->f_mode & (O_ACCMODE)) {
#if !LINUX_VERSION_25_ABOVE
        ndop_check_netdisk_change(inode->i_rdev);
#endif
    /*  if(g_ndas_dev[slot - 1].flags[part] & ND_CHANGED) {
            dbgl_blk(3,"netdisk changed !!\n");
            goto error_out;
        */
    }
    if ( (filp->f_mode & 2) 
        && !NDAS_GET_SLOT_DEV(slot)->info.writable) 
    { 
        dbgl_blk(3, "ed read-only");
        ndop_release(inode, filp);
        return -EROFS;
    }

#if !LINUX_VERSION_25_ABOVE
    dbgl_blk(3,"g_ndas_dev->nd_ref[%d] = %d, flags = %x, mode = %x", part, 
            NDAS_GET_SLOT_DEV(slot)->ref[part], filp ? filp->f_flags : 0, filp ? filp->f_mode : 0);
#endif
    dbgl_blk(3, "ed open");
    return 0;
}

/* close the NDAS device file */
int ndop_release(struct inode *inode, struct file *filp)
{
#if !LINUX_VERSION_25_ABOVE
    int                part;
#endif
    int                slot;

    dbgl_blk(5, "inode=%p",inode);
    dbgl_blk(5, "filp=%p",filp);
    
    if(!inode) {
        printk("Weird, open called with inode = 0\n");
        return -ENODEV;
    }

    slot = SLOT_I(inode);
    dbgl_blk(2,"slot =%d", slot);
    if (slot > NDAS_MAX_SLOT_NR) {
        printk("Minor too big.\n"); 
        return -ENODEV;
    }

    if(filp && (filp->f_mode & (2 | OPEN_WRITE_BIT)))
        file_fsync(filp, filp->f_dentry, 0);
    if ( g_ndas_dev[slot - NDAS_FIRST_SLOT_NR] == NULL ) { //
        MOD_DEC_USE_COUNT;
        return 0;
    }

#if !LINUX_VERSION_25_ABOVE
    part = PARTITION(inode->i_rdev);
    dbgl_blk(3,"slot=%d, part=%d", slot, part);
    if(g_ndas_dev[slot - NDAS_FIRST_SLOT_NR]->ref[part] == -1)
        g_ndas_dev[slot - NDAS_FIRST_SLOT_NR]->ref[part] = 0;
    else if(!g_ndas_dev[slot - NDAS_FIRST_SLOT_NR]->ref[part])
        dbgl_blk(3,"nd_ref[%d] = 0", part);
    else
         g_ndas_dev[slot - NDAS_FIRST_SLOT_NR]->ref[part] --;
    
#endif
    dbgl_blk(3,"ed");
    MOD_DEC_USE_COUNT;
    return 0;
}

#ifdef NDAS_MSHARE
typedef struct _MY_READ_DVD_STRUCTURE_REPLY{
	unsigned char	ReadDvdStructLenMsb;
	unsigned char	ReadDvdStructLenLsb;
	unsigned char	Reserved1;
	unsigned char	Reserved2;
	unsigned char	PerVersion	:4;
	unsigned char	BookType	:4;
	unsigned char	MaxRate		:4;
	unsigned char	DiscSize	:4;
	unsigned char	LayerType	:4;
	unsigned char	TrackPath	:1;
	unsigned char	NumOfLayer	:2;
	unsigned char	Reserved	:1;
	unsigned char	TrackDensity	:4;
	unsigned char	LinearDensity	:4;
	unsigned char	StartPhySecNumDataArea0;
	unsigned char	StartPhySecNumDataArea1;
	unsigned char	StartPhySecNumDataArea2;
	unsigned char	StartPhySecNumDataArea3;
	unsigned char	EndPhySecNumDataArea0;
	unsigned char	EndPhySecNumDataArea1;
	unsigned char	EndPhySecNumDataArea2;
	unsigned char	EndPhySecNumDataArea3;
	unsigned char	EndSecNumInLayer0;
	unsigned char	EndSecNumInLayer1;
	unsigned char	EndSecNumInLayer2;
	unsigned char	EndSecNumInLayer3;
	unsigned char	Reserved3	:7;
	unsigned char	BCA			:1;
	unsigned char	MediaSpecific[1];
}
MY_READ_DVD_STRUCTURE_REPLY, *PMY_READ_DVD_STRUCTURE_REPLY;

typedef struct _MY_CDVD_REPORT_ASF_DATA {
    unsigned char Reserved1[3];
    unsigned char Success : 1;
    unsigned char Reserved2 : 7;
} 
MY_CDVD_REPORT_ASF_DATA, *PMY_CDVD_REPORT_ASF_DATA;



struct MY_ATAPI_CAPA_PAGE {
	struct mode_page_header header;
#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned char SAVBABLE : 1;
	unsigned char RESERVED1           : 1;
	unsigned char PAGECODE           : 6;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char PAGECODE           : 6;
	unsigned char RESERVED1           : 1;
	unsigned char SAVBABLE : 1;
#else
#endif
	unsigned char PAGELENGTH;
#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned char RESERVED2           : 2;
	unsigned char DVD_RAM_R       : 1;
	unsigned char DVD_R_R          : 1;
	unsigned char DVD_ROM             : 1;
	unsigned char METHOD2             : 1; /* reserved in 1.2 */
	unsigned char CD_RW_R		 : 1; /* reserved in 1.2 */
	unsigned char CD_R_R           : 1; /* reserved in 1.2 */
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char CD_R_R           : 1; /* reserved in 1.2 */
	unsigned char CD_RW_R          : 1; /* reserved in 1.2 */
	unsigned char METHOD2             : 1;
	unsigned char DVD_ROM             : 1;
	unsigned char DVD_R_R          : 1;
	unsigned char DVD_RAM_R        : 1;
	unsigned char RESERVED2		 : 2;
#else
#endif

#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned char RESERVED3           : 2;
	unsigned char DVD_RAM_W       : 1;
	unsigned char DVD_R_W         : 1;
	unsigned char RESERVED4          : 1;
	unsigned char TEST_W          : 1;
	unsigned char CD_RW_W	 : 1; /* reserved in 1.2 */
	unsigned char CD_R_W          : 1; /* reserved in 1.2 */
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char CD_R_W          : 1; /* reserved in 1.2 */
	unsigned char CD_RW_W	 : 1; /* reserved in 1.2 */
	unsigned char TEST_W          : 1;
	unsigned char RESERVED4          : 1;
	unsigned char DVD_R_W         : 1;
	unsigned char DVD_RAM_W       : 1;
	unsigned char RESERVED3           : 2;
#else
#endif

#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned char RESERVED5           : 1;
	unsigned char M_SESSION        : 1;
	unsigned char MODE2FORM2         : 1;
	unsigned char MODE2FORM1         : 1;
	unsigned char D_PORT2            : 1;
	unsigned char D_PORT1            : 1;
	unsigned char COMPOSITE           : 1;
	unsigned char AUDIOPLAY          : 1;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char AUDIOPLAY          : 1;
	unsigned char COMPOSITE           : 1;
	unsigned char D_PORT1            : 1;
	unsigned char D_PORT2            : 1;
	unsigned char MODE2FORM1         : 1;
	unsigned char MODE2FORM2         : 1;
	unsigned char M_SESSION        : 1;
	unsigned char RESERVED5           : 1;
#else
#endif

#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned char RESERVED6          : 1;
	unsigned char UPC                 : 1;
	unsigned char ISRC                : 1;
	unsigned char C2POINTERS         : 1;
	unsigned char RWCORR             : 1;
	unsigned char RWSUP        : 1;
	unsigned char CDDA_ACUR       : 1;
	unsigned char CDDA                : 1;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char CDDA                : 1;
	unsigned char CDDA_ACUR       : 1;
	unsigned char RWSUP        : 1;
	unsigned char RWCORR             : 1;
	unsigned char C2POINTERS         : 1;
	unsigned char ISRC                : 1;
	unsigned char UPC                  : 1;
	unsigned char RESERVED6           : 1;
#else
#endif

#if defined(__BIG_ENDIAN_BITFIELD)
	mechtype_t MTYPE	 : 3;
	unsigned char RESERVED7           : 1;
	unsigned char EJECT               : 1;
	unsigned char PREVENT_J      : 1;
	unsigned char LOCKSTATE          : 1;
	unsigned char LOCK                : 1;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char LOCK                : 1;
	unsigned char LOCKSTATE          : 1;
	unsigned char PREVENT_J       : 1;
	unsigned char EJECT               : 1;
	unsigned char RESERVED7          : 1;
	mechtype_t MTYPE	 : 3;
#else
#endif

#if defined(__BIG_ENDIAN_BITFIELD)
	unsigned char RESERVED8           : 4;
	unsigned char SSS                : 1;  /* reserved in 1.2 */
	unsigned char DISC_IN        : 1;  /* reserved in 1.2 */
	unsigned char SEP_MUTE       : 1;
	unsigned char SEP_VOLUME     : 1;
#elif defined(__LITTLE_ENDIAN_BITFIELD)
	unsigned char SEP_VOLUME     : 1;
	unsigned char SEP_MUTE       : 1;
	unsigned char DISC_IN        : 1;  /* reserved in 1.2 */
	unsigned char SSS                 : 1;  /* reserved in 1.2 */
	unsigned char RESERVED8           : 4;
#else
#endif
	unsigned short MAXSPPED;
	unsigned short NUM_VOL_LEVELS;
	unsigned short BUF_SIZE;
	unsigned short CUR_SPEED;
	char pad[4];
};


	//
	// 	cdrom emulate code
	//
int ACT_ATAPI_CMD(
	int slot,
	int partnum,
	kdev_t	dev, 
	struct cdrom_generic_command * cgc
	)
{
	int result = -EINVAL;
	int status;

	dbgl_blk(4,"enter ACT_ATAPI_CMD---\n");	
	if(cgc->sense)
		memset(cgc->sense, 0, sizeof(struct request_sense));

	status = ndas_GetDiscStatus(slot, partnum);
	
	if( !(status & (DISC_STATUS_VALID | DISC_STATUS_VALID_END)) )
	{
		struct request_sense  * sense;
		
		sense = cgc->sense;
 		sense->error_code = 0x70;
		sense->valid = 1;
		//sense->segment_number;
		sense->sense_key = 0x02; //NOT_READ
		//sense->ili
		//sense->information[0];
		//sense->information[3];
		sense->add_sense_len =  0xb;
		//sense->command_info[0];
		//sense->command_info[3];
		sense->asc = 0x3a; // INVALIDE_CCB;
		sense->ascq = 0x0;
		//sense->fruc;
		//sense->sks[0];
		//sense->sks[3];
		//sense->asb[0];
		//sense->asb[45];
		result = -EINVAL;
		return result;	
	}


	switch(cgc->cmd[0])
	{
		case 0x5a:
		{
			if(cgc->cmd[2] == 0x2a)
			{
				struct MY_ATAPI_CAPA_PAGE  * page;
				dbgl_blk(4,"in 0x2a\n");
				dbgl_blk(4,"buflen %d\n", cgc->buflen);
				memset(cgc->buffer,0,cgc->buflen);
				page = (struct MY_ATAPI_CAPA_PAGE *)(cgc->buffer);
				page->header.mode_data_length = 0x20;
				page->header.medium_type = 0;// 0x70;
				page->PAGECODE = 0x2a;
				page->PAGELENGTH = 0x18;
        
				page->CD_R_R = 1;
        
				page->CD_RW_R = 1;
       
				page->METHOD2 = 1;  

				page->DVD_ROM = 1;
       
				page->DVD_R_R = 1;
        
				page->DVD_RAM_R = 1;
       
				page->CD_R_W = 0;
        
				page->CD_RW_W = 0;
        
				page->TEST_W = 0;
       
				page->DVD_R_W = 0;
       
				page->DVD_RAM_W = 0;
        
				page->AUDIOPLAY = 0;
        
				page->COMPOSITE = 0;
      
				page->D_PORT1 = 0;
     
				page->D_PORT2 = 0;
      
				page->MODE2FORM1 = 0;
     
				page->MODE2FORM2 = 0;
   
				page->M_SESSION = 1;
				
				page->CDDA = 1;
  
				page->CDDA_ACUR = 1;
        
				page->RWSUP = 0;
     
				page->RWCORR  = 0;

				page->C2POINTERS = 0;
       
				page->ISRC = 0;
       
				page->UPC = 0;
     
				page->LOCK = 0;
				page->LOCKSTATE = 0;
				page->PREVENT_J = 0;
				
    
				page->EJECT = 1;
   
				page->MTYPE = 1;
    
				page->SEP_VOLUME = 1;
     
				page->SEP_MUTE = 1;
      
				page->DISC_IN = 0;
   
				page->SSS = 0;


				//page->MAXSPPED;
  
				page->NUM_VOL_LEVELS = 0xff;
     
				page->BUF_SIZE = 0x02;

				//page->CUR_SPEED;
								
				result = 0;
			
			}else{
				struct request_sense  * sense;
				dbgl_blk(4,"in (0x%02x)\n",cgc->cmd[2]);
				sense = cgc->sense;
 				sense->error_code = 0x70;
				sense->valid = 1;
				//sense->segment_number;
				sense->sense_key = 0x05; //ILLEGAL_REQ
				//sense->ili
				//sense->information[0];
				//sense->information[3];
				sense->add_sense_len =  0xb;
				//sense->command_info[0];
				//sense->command_info[3];
				sense->asc = 0x24; // INVALIDE_CCB;
				sense->ascq = 0x0;
				//sense->fruc;
				//sense->sks[0];
				//sense->sks[3];
				//sense->asb[0];
				//sense->asb[45];
				result = -EINVAL;

			}
		}
		break;
		case 0x28:
		case 0xa8:
		{
			unsigned int	logicalBlockAddress;
			unsigned int 	transferBlock;
			unsigned int startSec;
			unsigned int org_req_sectors;

			if(cgc->cmd[0] == 0x28){
				logicalBlockAddress = ( cgc->cmd[5]
							| cgc->cmd[4] << 8
							| cgc->cmd[3] << 16
							| cgc->cmd[2] << 24 );
				transferBlock = ( cgc->cmd[8]
							| cgc->cmd[7] << 8);							
			}else {
				logicalBlockAddress = ( cgc->cmd[5]
							| cgc->cmd[4] << 8
							| cgc->cmd[3] << 16
							| cgc->cmd[2] << 24 );
				transferBlock = ( cgc->cmd[9]
							| cgc->cmd[8] << 8
							| cgc->cmd[7] << 16
							| cgc->cmd[6] << 24);					
			}
		

			//
			//	check addres :  application direct ioctl 
			//		unit sector :  may be 2048			
			//	
			
	
			if(cgc->buflen < (unsigned int)(transferBlock * 2048) )
			{
				printk("buffer size (%d) is too small than req block(%d)\n", 
					cgc->buflen,transferBlock );
				result = -EINVAL;
				break;
			}	


			// address translation send command	
			startSec = (unsigned int)(logicalBlockAddress << 2);
			org_req_sectors = (unsigned int)(transferBlock << 2);
			dbgl_blk(4,"StartSec (%d): org_req_sector(%d)\n",startSec,org_req_sectors);	
			//printk("StartSec (%d): org_req_sector(%d)\n",startSec,org_req_sectors);
			if(ndas_ATAPIRequest(slot,partnum, dev, CM_READ, startSec, org_req_sectors, cgc->buffer) != NDAS_OK)
			{
				printk("Fail SendATA_CMD\n");
				result = -EINVAL;	
				return result;
			}
			return 0;

		}
		break;
		case 0x0:
			result = 0;
		break;
		case 0xad:
		{
				if(cgc->cmd[7] == 0x00){
					MY_READ_DVD_STRUCTURE_REPLY Reply;
					unsigned int					StartPhySec;
					unsigned int					EndPhySec;
					unsigned int					EndSecNumL0;
					memset((char *)&Reply,0,sizeof(MY_READ_DVD_STRUCTURE_REPLY));
					Reply.ReadDvdStructLenLsb = (unsigned char)((sizeof(MY_READ_DVD_STRUCTURE_REPLY) -2) & 0xff);
					Reply.BookType = 0;
					Reply.PerVersion = 1;
					Reply.DiscSize = 0;
					Reply.MaxRate = 2;
					Reply.NumOfLayer = 1;
					Reply.TrackPath = 1;
					Reply.LayerType = 1;
					Reply.LinearDensity = 1;
					Reply.TrackDensity = 0;
					StartPhySec = 0x30000;
					Reply.StartPhySecNumDataArea3 = (unsigned char)(StartPhySec & 0xff);
					Reply.StartPhySecNumDataArea2 = (unsigned char)((StartPhySec >> 8) & 0xff);
					Reply.StartPhySecNumDataArea1 = (unsigned char)((StartPhySec >> 16) & 0xff);
					Reply.StartPhySecNumDataArea0 = (unsigned char)((StartPhySec >> 24) & 0xff);
					
					EndPhySec = 0xfcf4f0;
					Reply.EndPhySecNumDataArea3 = (unsigned char)(EndPhySec & 0xff);
					Reply.EndPhySecNumDataArea2 = (unsigned char)((EndPhySec >> 8) & 0xff);
					Reply.EndPhySecNumDataArea1 = (unsigned char)((EndPhySec >> 16) & 0xff);
					Reply.EndPhySecNumDataArea0 = (unsigned char)((EndPhySec >> 24) & 0xff);					
					
					EndSecNumL0 = 0x22c28f;
					Reply.EndSecNumInLayer3 = (unsigned char)(EndSecNumL0  & 0xff);
					Reply.EndSecNumInLayer2 = (unsigned char)((EndSecNumL0  >> 8) & 0xff);
					Reply.EndSecNumInLayer1 = (unsigned char)((EndSecNumL0  >> 16) & 0xff);
					Reply.EndSecNumInLayer0 = (unsigned char)((EndSecNumL0  >> 24) & 0xff);	
					
					
					if(cgc->buflen < sizeof(MY_READ_DVD_STRUCTURE_REPLY)){
						memcpy((char *)cgc->buffer, (char *)&Reply, cgc->buflen);
					}else{
						memcpy((char *)cgc->buffer, (char *)&Reply, sizeof(MY_READ_DVD_STRUCTURE_REPLY));
					}
					result = 0;
				}else {
					result = -1;
				}
		}
		break;

		case 0xa4:
		{
			switch(cgc->cmd[10] & 0x3f){
			case 0x0:
			case 0x1:
			case 0x2:
			case 0x4:
			case 0x8:
			case 0x3f:
			{
				result = 0;
			}
			break;
			case 0x5:
			{
					MY_CDVD_REPORT_ASF_DATA ASF_DATA;
					ASF_DATA.Success = 0;
					memset(&ASF_DATA,0,sizeof(ASF_DATA));
					memcpy(cgc->buffer,&ASF_DATA,sizeof(MY_CDVD_REPORT_ASF_DATA));
					if(cgc->sense != NULL) {
						struct request_sense  * sense;
						sense = cgc->sense;
		 				sense->error_code = 0x70;
						sense->valid = 1;
						//sense->segment_number;
						sense->sense_key = 0x05; //ILLEGAL_REQ
						//sense->ili
						//sense->information[0];
						//sense->information[3];
						sense->add_sense_len =  0xb;
						//sense->command_info[0];
						//sense->command_info[3];
						sense->asc = 0x6f;
						sense->ascq = 0x0;
						//sense->fruc;
						//sense->sks[0];
						//sense->sks[3];
						//sense->asb[0];
						//sense->asb[45];
					}
					result = -EINVAL;

			}
			break;
			default:
			{
				if(cgc->sense != NULL) {
					struct request_sense  * sense;
					sense = cgc->sense;
	 				sense->error_code = 0x70;
					sense->valid = 1;
					//sense->segment_number;
					sense->sense_key = 0x05; //ILLEGAL_REQ
					//sense->ili
					//sense->information[0];
					//sense->information[3];
					sense->add_sense_len =  0xb;
					//sense->command_info[0];
					//sense->command_info[3];
					sense->asc = 0x6f;
					sense->ascq = 0x0;
					//sense->fruc;
					//sense->sks[0];
					//sense->sks[3];
					//sense->asb[0];
					//sense->asb[45];
				}
				result = -EINVAL;	
			}
			break;
			}			
		}
		break;
		default:
			//printk("command 0x%02x\n", cgc->cmd[0]);
			result = 0;
			break;
		break;
	}
	dbgl_blk(4,"---end ACT_ATAPI_CMD\n");	
	return result;
}


int Do_ATAPI_CMD(
	int slot,
	int partnum, 
	kdev_t	dev,
	struct cdrom_generic_command * cgc
	)
{
	struct ndas_slot *sd = NULL;
	struct request_sense *usense, sense;
	unsigned char *tbuf;
	int ret;

	sd = NDAS_GET_SLOT_DEV(slot);
	
	dbgl_blk(4, "enter Do_ATAPI_CMD---\n");	

	if(cgc->data_direction == CGC_DATA_UNKNOWN)
	{
		printk("Do_ATAPI_CMD: error CGC_DATA_UNKOWN\n");
		return NDAS_ERROR_INVALID_PARAMETER ;
	}

	if(cgc->buflen < 0 || cgc->buflen >= (sd->info.max_sectors_per_request * 1024))
	{
		printk("Do_ATAPI_CMD: error buffer is too big\n");
		return NDAS_ERROR_INVALID_PARAMETER ;
	}
	
	// buffer accessibility check
	if((tbuf = cgc->buffer)){
		cgc->buffer = kmalloc(cgc->buflen, GFP_KERNEL);
		
		if(cgc->buffer == NULL) {
			printk("Do_ATAPI_CMD: error allocating buffer\n");
			return NDAS_ERROR_INVALID_PARAMETER ;
		}
	}

	usense = cgc->sense;
	cgc->sense = &sense;
	memset(&sense,0,sizeof(struct request_sense));


	if(!access_ok(VERIFY_WRITE, usense, sizeof(*cgc->sense)))
	{
		printk("Do_ATAPI_CMD: error accessing sense buffer\n");
		kfree(cgc->buffer);
		return NDAS_ERROR_INVALID_PARAMETER;
	}

	if(cgc->data_direction == CGC_DATA_READ){
		
		if(!access_ok(VERIFY_READ, tbuf, cgc->buflen))
		{
			printk("Do_ATAPI_CMD; error accessing read buffer\n");
			kfree(cgc->buffer);
			return NDAS_ERROR_INVALID_PARAMETER ;
		}

	} else if(cgc->data_direction == CGC_DATA_WRITE) {

		if(copy_from_user(cgc->buffer, tbuf, cgc->buflen)){
			kfree(cgc->buffer);
			printk("Do_ATAPI_CMD: error accessing write buffer\n");
			return NDAS_ERROR_INVALID_PARAMETER;
		}
	}
	
	dbgl_blk(4,"CALL ACT_ATAPI_CMD disc \n");
	ret = ACT_ATAPI_CMD(slot, partnum, dev, cgc);

	if(ret < 0)	
	{
		__copy_to_user(usense, cgc->sense, sizeof(*usense));
		dbgl_blk(1,"Do_ATAPI_CMD; error processing ACT_ATAPI_CMD\n");
	}

	if(!ret && cgc->data_direction == CGC_DATA_READ) {
/*
		if(cgc->cmd[0] == 0x28){
			int i = 0;
			int count = 0;
			char * buf = NULL;
			unsigned long startAddr = 0;
			unsigned short Nsec = 0;
			buf = cgc->buffer;
			count = cgc->buflen/16;
			if(count > 10) count = 8;
			startAddr = ( cgc->cmd[5]
					| cgc->cmd[4] << 8
					| cgc->cmd[3] << 16
					| cgc->cmd[2] << 24 );

			Nsec = ( cgc->cmd[8]
					| cgc->cmd[7] << 8);

			printk("Req: StarSec[%d]:SecCount[%d] buff[%p]\n",startAddr,Nsec,buf);

			for(i = 0; i<count; i++)
			{
				printk("[0x%x]%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",i,
				buf[i*16+0],buf[i*16+1],buf[i*16+2],buf[i*16+3],buf[i*16+4],buf[i*16+5],buf[i*16+7],buf[i*16+7],	
				buf[i*16+8],buf[i*16+9],buf[i*16+10],buf[i*16+11],buf[i*16+12],buf[i*16+13],buf[i*16+14],buf[i*16+15]
				);
			}

		}
*/		
	__copy_to_user(tbuf,cgc->buffer, cgc->buflen);
	}
	kfree(cgc->buffer);
	dbgl_blk(4,"---end Do_ATAPI_CMD---\n");	
	return ret;
}



int juke_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    int            slot;
    struct ndas_slot *sd = NULL;
   
    unsigned int partnum = 0;
    unsigned int        result = -1;
    
    
    switch(cmd) { 	
    	case IOCTL_NDCMD_MS_SETUP_WRITE:
	{

		ndas_ioctl_juke_write_start_t write_start_info;
		int partid;
		int start = 0;

		IOCTL_IN(arg, struct ndas_ioctl_juke_write_start_t, write_start_info);	

		slot = write_start_info.slot;
		sd = NDAS_GET_SLOT_DEV(slot);
		
              if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
              {
			write_start_info.error =   NDAS_ERROR_JUKE_DISK_NOT_SET;	
			IOCTL_OUT(arg, ndas_ioctl_juke_write_start_t, write_start_info);
			return -1;				 
              }		

		if(sd->info.writable != 1)
		{
			write_start_info.error =   NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
			IOCTL_OUT(arg, ndas_ioctl_juke_write_start_t, write_start_info);
			return -1;
		}

		if(slot != write_start_info.slot)
		{
			write_start_info.error =   NDAS_ERROR_INVALID_SLOT_NUMBER ;
			IOCTL_OUT(arg, ndas_ioctl_juke_write_start_t, write_start_info);
			return -1;
		}

		if(! ndas_BurnStartCurrentDisc(slot, write_start_info.nr_sectors, write_start_info.selected_disc, write_start_info.encrypted, write_start_info.HINT))
		{
			write_start_info.error = NDAS_ERROR_JUKE_DISC_WRITE_S_FAIL;
			IOCTL_OUT(arg,  ndas_ioctl_juke_write_start_t, write_start_info);
			return -1;
		}

		partid = ndas_allocate_part_id(slot, write_start_info.selected_disc);
		if(partid < 0)
		{
			write_start_info.error = NDAS_ERROR_JUKE_DISC_WRITE_S_FAIL;
			IOCTL_OUT(arg,  ndas_ioctl_juke_write_start_t, write_start_info);
			return -1;
		}

		write_start_info.error = 0;
		write_start_info.partnum = partid;
		start = (slot - NDAS_FIRST_SLOT_NR) << ndas_gd.minor_shift;
#if	(LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22))
		invalidate_device(MKDEV(NDAS_BLK_MAJOR , (start+partid)), 1);
#endif
		ndas_gd.part[start + partid].start_sect = 0;
		ndas_gd.part[start + partid].nr_sects = (1024*8*2);
		IOCTL_OUT(arg,  ndas_ioctl_juke_write_start_t, write_start_info);
		result = 0;
		
	

	}
	break;
	case IOCTL_NDCMD_MS_END_WRITE	:
	{

		ndas_ioctl_juke_write_end_t write_end_info;
		PDISC_INFO pdiscinfo, tdiscinfo;
		int start = 0;
		
		IOCTL_IN(arg,  ndas_ioctl_juke_write_end_t, write_end_info);

		slot = write_end_info.slot;
		sd = NDAS_GET_SLOT_DEV(slot);
		
              if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
              {
			write_end_info.error =   NDAS_ERROR_JUKE_DISK_NOT_SET;	
			IOCTL_OUT(arg, ndas_ioctl_juke_write_end_t, write_end_info);
			return -1;				 
              }		

		
		if(sd->info.writable != 1){
			write_end_info.error =   NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
			IOCTL_OUT(arg, ndas_ioctl_juke_write_end_t, write_end_info);
			return -1;
		}

		if(slot != write_end_info.slot)
		{
			write_end_info.error =   NDAS_ERROR_INVALID_SLOT_NUMBER ;
			IOCTL_OUT(arg, ndas_ioctl_juke_write_end_t, write_end_info);
			return -1;
		}


		pdiscinfo = write_end_info.discInfo;
		if(!access_ok(VERIFY_READ, pdiscinfo, sizeof(DISC_INFO)))
		{
			write_end_info.error =  NDAS_ERROR_INVALID_PARAMETER;
			IOCTL_OUT(arg,  ndas_ioctl_juke_write_end_t, write_end_info);
			return -1;			
		}

		tdiscinfo = (PDISC_INFO)kmalloc(sizeof(DISC_INFO), GFP_KERNEL);
		if(!tdiscinfo){
			write_end_info.error =  NDAS_ERROR_BUFFER_OVERFLOW;;
			IOCTL_OUT(arg,  ndas_ioctl_juke_write_end_t, write_end_info);	
			return -1;
		}

		copy_from_user((PDISC_INFO)tdiscinfo, pdiscinfo, sizeof(DISC_INFO));

		if(!ndas_BurnEndCurrentDisc(slot, write_end_info.selected_disc, (struct disc_add_info *)tdiscinfo))
		{
			write_end_info.error = NDAS_ERROR_JUKE_DISC_WRITE_E_FAIL;
			IOCTL_OUT(arg,  ndas_ioctl_juke_write_end_t, write_end_info);
			return -1;
		}
		
		if(ndas_deallocate_part_id(slot, write_end_info.partnum) < 0)
		{
			write_end_info.error = NDAS_ERROR_JUKE_DISC_WRITE_E_FAIL;
			IOCTL_OUT(arg,  ndas_ioctl_juke_write_end_t, write_end_info);
			return -1;
		}

		write_end_info.error = 0;
		ndas_gd.part[start + write_end_info.partnum].start_sect = 0;
		ndas_gd.part[start + write_end_info.partnum].nr_sects = 0;
		IOCTL_OUT(arg,  ndas_ioctl_juke_write_end_t, write_end_info);
		result = 0;
		
	}
	break;	
	case IOCTL_NDCMD_MS_SETUP_READ:
	{
		ndas_ioctl_juke_read_start_t read_start_info;
		int partid;
		int start = 0;
		
		IOCTL_IN(arg,  ndas_ioctl_juke_read_start_t, read_start_info);	

		slot = read_start_info.slot;
		sd = NDAS_GET_SLOT_DEV(slot);
		
              if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
              {
			read_start_info.error =   NDAS_ERROR_JUKE_DISK_NOT_SET;	
			IOCTL_OUT(arg, ndas_ioctl_juke_read_start_t, read_start_info);
			return -1;				 
              }		


		if(slot != read_start_info.slot)
		{
			read_start_info.error =   NDAS_ERROR_INVALID_SLOT_NUMBER ;
			IOCTL_OUT(arg, ndas_ioctl_juke_read_start_t, read_start_info);
			return -1;
		}

		if(!ndas_CheckDiscValidity(slot, read_start_info.selected_disc))
		{
			read_start_info.error =  NDAS_ERROR_JUKE_DISC_VALIDATE_FAIL;
			IOCTL_OUT(arg, ndas_ioctl_juke_read_start_t, read_start_info);
			return -1;			
		}
		
		
		partid = ndas_allocate_part_id(slot, read_start_info.selected_disc);
		if(partid < 0)
		{
			read_start_info.error = NDAS_ERROR_JUKE_DISC_READ_S_FAIL;
			IOCTL_OUT(arg, ndas_ioctl_juke_read_start_t, read_start_info);
			return -1;
		}

		read_start_info.partnum = partid;
		read_start_info.error = 0;
#if	(LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22))
		invalidate_device(MKDEV(NDAS_BLK_MAJOR , (start+partid)), 1);
#endif
		ndas_gd.part[start + partid].start_sect = 0;
		ndas_gd.part[start + partid].nr_sects = (1024*1024*8*2);		
		IOCTL_OUT(arg, ndas_ioctl_juke_read_start_t, read_start_info);\

		result = 0;	
	}
	break;		
	case IOCTL_NDCMD_MS_END_READ:
	{
		ndas_ioctl_juke_read_end_t read_end_info;
		int start = 0;
		
		IOCTL_IN(arg,  ndas_ioctl_juke_read_end_t, read_end_info);
		
		slot = read_end_info.slot;
		sd = NDAS_GET_SLOT_DEV(slot);
		
              if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
              {
			read_end_info.error =   NDAS_ERROR_JUKE_DISK_NOT_SET;	
			IOCTL_OUT(arg, ndas_ioctl_juke_read_end_t, read_end_info);
			return -1;				 
              }		

		 
		if(slot != read_end_info.slot)
		{
			read_end_info.error =   NDAS_ERROR_INVALID_SLOT_NUMBER ;
			IOCTL_OUT(arg, ndas_ioctl_juke_read_end_t, read_end_info);
			return -1;
		}

		if(ndas_deallocate_part_id(slot, read_end_info.partnum) < 0)
		{
			read_end_info.error = NDAS_ERROR_JUKE_DISC_WRITE_S_FAIL;
			IOCTL_OUT(arg,  ndas_ioctl_juke_read_end_t, read_end_info);
			return -1;
		}

		read_end_info.error = 0;
		ndas_gd.part[start + read_end_info.partnum].start_sect = 0;
		ndas_gd.part[start + read_end_info.partnum].nr_sects = (1024*1024*8*2);
		IOCTL_OUT(arg,  ndas_ioctl_juke_read_end_t, read_end_info);
		result = 0;		
		
	}
	break;		
	case IOCTL_NDCMD_MS_DEL_DISC:
	{
		ndas_ioctl_juke_del_t del_info;
		IOCTL_IN(arg, ndas_ioctl_juke_del_t, del_info);

		slot = del_info.slot;
		sd = NDAS_GET_SLOT_DEV(slot);
		
              if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
              {
			del_info.error =   NDAS_ERROR_JUKE_DISK_NOT_SET;	
			IOCTL_OUT(arg, ndas_ioctl_juke_del_t, del_info);
			return -1;				 
              }		

		if(slot != del_info.slot)
		{
			del_info.error =   NDAS_ERROR_INVALID_SLOT_NUMBER ;
			IOCTL_OUT(arg, ndas_ioctl_juke_del_t, del_info);
			return -1;
		}

		if(sd->info.writable != 1){
			del_info.error =   NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
			IOCTL_OUT(arg,  ndas_ioctl_juke_del_t, del_info);
			return -1;
		}
		
		if(!ndas_DeleteCurrentDisc(slot, del_info.selected_disc) )
		{
		 	del_info.error = NDAS_ERROR_JUKE_DISC_DEL_FAIL;
		 	IOCTL_OUT(arg,  ndas_ioctl_juke_del_t, del_info);
		 	return -1;
		}

		del_info.error = 0;
		IOCTL_OUT(arg,  ndas_ioctl_juke_del_t, del_info);
		result = 0;

	}
	break;		
	case IOCTL_NDCMD_MS_VALIDATE_DISC:
	{
		ndas_ioctl_juke_validate_t validate_info;
		IOCTL_IN(arg,  ndas_ioctl_juke_validate_t, validate_info);

		slot = validate_info.slot;
		sd = NDAS_GET_SLOT_DEV(slot);
		
              if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
              {
			validate_info.error =   NDAS_ERROR_JUKE_DISK_NOT_SET;	
			IOCTL_OUT(arg,  ndas_ioctl_juke_validate_t, validate_info);
			return -1;				 
              }		

		if(sd->info.writable != 1){
			validate_info.error =   NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
			IOCTL_OUT(arg,  ndas_ioctl_juke_validate_t, validate_info);
			return -1;
		}

		if(slot != validate_info.slot)
		{
			validate_info.error =   NDAS_ERROR_INVALID_SLOT_NUMBER ;
			IOCTL_OUT(arg, ndas_ioctl_juke_validate_t, validate_info);
			return -1;
		}

		if(!ndas_ValidateDisc(slot, validate_info.selected_disc) )
		{
			validate_info.error = NDAS_ERROR_JUKE_DISC_VALIDATE_FAIL;
		 	IOCTL_OUT(arg,  ndas_ioctl_juke_validate_t, validate_info);
		 	return -1;
		}

		validate_info.error = 0;
		IOCTL_OUT(arg, ndas_ioctl_juke_validate_t, validate_info);
		result = 0;
		
		
	}
	break;
	case IOCTL_NDCMD_MS_FORMAT_DISK:
	{
		ndas_ioctl_juke_format_t	format_info;
		IOCTL_IN(arg, ndas_ioctl_juke_format_t, format_info);

		slot = format_info.slot;
		sd = NDAS_GET_SLOT_DEV(slot);
		
              if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
              {
			format_info.error =   NDAS_ERROR_JUKE_DISK_NOT_SET;	
			IOCTL_OUT(arg,  ndas_ioctl_juke_format_t, format_info);
			return -1;				 
              }		


		if(sd->info.writable != 1){
			format_info.error = NDAS_ERROR_NO_WRITE_ACCESS_RIGHT;
			IOCTL_OUT(arg,  ndas_ioctl_juke_format_t, format_info);
			return -1;
		}

		if(slot != format_info.slot)
		{
			format_info.error =   NDAS_ERROR_INVALID_SLOT_NUMBER ;
			IOCTL_OUT(arg, ndas_ioctl_juke_format_t, format_info);
			return -1;
		}


		if(!ndas_DISK_FORMAT(slot, sd->info.sectors) )
		{
			format_info.error = NDAS_ERROR_JUKE_DISK_FORMAT_FAIL;
			IOCTL_OUT(arg,  ndas_ioctl_juke_format_t, format_info);		
			return -1;
		}
		
		format_info.error = 0;
		IOCTL_OUT(arg,  ndas_ioctl_juke_format_t, format_info);		
		result = 0;
	}
	break;
	case IOCTL_NDCMD_MS_GET_DISK_INFO:
	{

		ndas_ioctl_juke_disk_info_t disk_info;
		PDISK_information pdiskinfo,tdiskinfo;
		
 		IOCTL_IN(arg, ndas_ioctl_juke_disk_info_t, disk_info);	

		slot = disk_info.slot;
		sd = NDAS_GET_SLOT_DEV(slot);

		
		
 
		if(slot != disk_info.slot)
		{
			disk_info.error =   NDAS_ERROR_INVALID_SLOT_NUMBER ;
			IOCTL_OUT(arg, ndas_ioctl_juke_disk_info_t, disk_info);
			return -1;
		}

		if(disk_info.queryType == 1){
			if(sd->info.mode == NDAS_DISK_MODE_MEDIAJUKE)
			{
				disk_info.diskType = 1;
			}else{
				disk_info.diskType = 0;
			}
			disk_info.error = 0;
			IOCTL_OUT(arg, ndas_ioctl_juke_disk_info_t, disk_info);
			return 0;
		}


             if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
              {
			disk_info.error =   NDAS_ERROR_JUKE_DISK_NOT_SET;	
			IOCTL_OUT(arg, ndas_ioctl_juke_disk_info_t, disk_info);
			return -1;				 
              }		
		
		disk_info.diskType = 1;

		pdiskinfo = disk_info.diskInfo;
		if(!pdiskinfo || !access_ok(VERIFY_WRITE, pdiskinfo , sizeof(DISK_information )))
		{
			disk_info.error =  NDAS_ERROR_INVALID_PARAMETER;
			IOCTL_OUT(arg,  ndas_ioctl_juke_disk_info_t, disk_info);
			return -1;			
		}


		dbgl_blk(0,"IOCTL_NDCMD_MS_GET_DISK_INFO : POSE of diskinfo %p\n", pdiskinfo);
		

		tdiskinfo = (PDISK_information)kmalloc(sizeof(DISK_information), GFP_KERNEL);
		if(!tdiskinfo) {
			disk_info.error = NDAS_ERROR_BUFFER_OVERFLOW;
			IOCTL_OUT(arg, ndas_ioctl_juke_disk_info_t, disk_info);
			return -1;
			
		}

		memset(tdiskinfo, 0, sizeof(DISK_information));
				
		if(!ndas_GetCurrentDiskInfo( slot, (struct disk_add_info *)tdiskinfo) )
		{
			disk_info.error = NDAS_ERROR_JUKE_DISC_INFO_FAIL;
			kfree(tdiskinfo);
			IOCTL_OUT(arg, ndas_ioctl_juke_disk_info_t, disk_info);
			return -1;		
		}
		
		
		disk_info.error = 0;
		copy_to_user((PDISK_information )pdiskinfo, tdiskinfo, sizeof(DISK_information));
		IOCTL_OUT(arg, ndas_ioctl_juke_disk_info_t, disk_info);
		kfree(tdiskinfo);
		result = 0;
		
		
	}
	break;
	case IOCTL_NDCMD_MS_GET_DISC_INFO:
	{

		ndas_ioctl_juke_disc_info_t 	disc_info;
		PDISC_INFO				pdiscInfo, tdiscinfo;
		
		IOCTL_IN(arg, ndas_ioctl_juke_disc_info_t, disc_info);	

		slot = disc_info.slot;
		sd = NDAS_GET_SLOT_DEV(slot);
		
              if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
              {
			disc_info.error =   NDAS_ERROR_JUKE_DISK_NOT_SET;	
			IOCTL_OUT(arg, ndas_ioctl_juke_disc_info_t, disc_info);
			return -1;				 
              }		

		if(slot != disc_info.slot)
		{
			disc_info.error =   NDAS_ERROR_INVALID_SLOT_NUMBER ;
			IOCTL_OUT(arg, ndas_ioctl_juke_disc_info_t, disc_info);
			return -1;
		}	
		
		pdiscInfo = disc_info.discInfo;
		
		if(!pdiscInfo  ||  !access_ok(VERIFY_WRITE, pdiscInfo , sizeof(DISC_INFO)))
		{
			disc_info.error =  NDAS_ERROR_INVALID_PARAMETER;
			IOCTL_OUT(arg,  ndas_ioctl_juke_disc_info_t, disc_info);
			return -1;			
		}

		tdiscinfo = (PDISC_INFO)kmalloc(sizeof(DISC_INFO), GFP_KERNEL);
		if(!tdiscinfo){
			disc_info.error =  NDAS_ERROR_BUFFER_OVERFLOW;
			IOCTL_OUT(arg,  ndas_ioctl_juke_disc_info_t, disc_info);
			return -1;			
		}
		
		memset(tdiscinfo, 0, sizeof(DISC_INFO));

		if(!ndas_GetCurrentDiscInfo(slot, disc_info.selected_disc, (struct disc_add_info * )tdiscinfo))
		{
			disc_info.error =  NDAS_ERROR_JUKE_DISC_INFO_FAIL;
			kfree(tdiscinfo);
			IOCTL_OUT(arg,  ndas_ioctl_juke_disc_info_t, disc_info);
			return -1;					
		}

		disc_info.error =  0;
		copy_to_user(pdiscInfo, tdiscinfo, sizeof(DISC_INFO));
		IOCTL_OUT(arg,  ndas_ioctl_juke_disc_info_t, disc_info);
		kfree(tdiscinfo);
		result = 0;
		
	}
	break;
	case CDROMREADTOCHDR:	
	{
		struct cdrom_tochdr header;

              slot = SLOT_I(inode);
              sd = NDAS_GET_SLOT_DEV(slot);
              partnum = PARTITION(inode->i_rdev);
			  
             if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
             {
                return NDAS_ERROR_JUKE_DISK_NOT_SET;	
             }
	
		 if( partnum != 0 && partnum < NR_PARTITION){
				if(!ndas_IsDiscSet(slot, partnum) ) {
    					return NDAS_ERROR_JUKE_DISC_NOT_SET;
				}	
    		}
		
		IOCTL_IN(arg, struct cdrom_tochdr, header);
		dbgl_blk(4,"CDROMREADTOCHDR\n");
		header.cdth_trk0 = 0x01;
		header.cdth_trk1 = 0x01;
		IOCTL_OUT(arg, struct cdrom_tochdr, header);
		result = 0; //return 0;		
	}
	break;
	case CDROMREADTOCENTRY:
	{
		struct cdrom_tocentry entry;

              slot = SLOT_I(inode);
              sd = NDAS_GET_SLOT_DEV(slot);
              partnum = PARTITION(inode->i_rdev);
			  
             if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
             {
                return NDAS_ERROR_JUKE_DISK_NOT_SET;	
             }
			 
		dbgl_blk(4,"CDROMREADTOCENTRY\n");
		
		 if( partnum != 0 && partnum < NR_PARTITION){
				if(!ndas_IsDiscSet(slot, partnum) ) {
    					return NDAS_ERROR_JUKE_DISC_NOT_SET;
				}	
    		}
		 
		IOCTL_IN(arg, struct cdrom_tocentry, entry);
		
		entry.cdte_track = 0x01;
		entry.cdte_adr = 0x1;
		entry.cdte_ctrl= 0x04;
		entry.cdte_addr.lba = 0x00000000;
		IOCTL_OUT(arg, struct cdrom_tocentry, entry);
		result = 0 ; //return 0;		
	}
	break;
	case DVD_READ_STRUCT:
	{

              slot = SLOT_I(inode);
              sd = NDAS_GET_SLOT_DEV(slot);
              partnum = PARTITION(inode->i_rdev);
			  
             if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
             {
                return NDAS_ERROR_JUKE_DISK_NOT_SET;	
             }
			 
		dbgl_blk(4,"DVD_READ_STRUCT\n");
		printk("DVD_READ_STRUCT\n");
		 if( partnum != 0 && partnum < NR_PARTITION){
				if(!ndas_IsDiscSet(slot, partnum) ) {
    					return NDAS_ERROR_JUKE_DISC_NOT_SET;
				}	
    		}
		
		result = 0 ; //return 0;		
	}
	break;
	case DVD_AUTH:
	{
		dvd_authinfo ai;

              slot = SLOT_I(inode);
              sd = NDAS_GET_SLOT_DEV(slot);
              partnum = PARTITION(inode->i_rdev);
			  
             if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
             {
                return NDAS_ERROR_JUKE_DISK_NOT_SET;	
             }
		
		dbgl_blk(4,"DVD_AUTH\n");
		
		 if( partnum != 0 && partnum < NR_PARTITION){
				if(!ndas_IsDiscSet(slot, partnum) ) {
    					return NDAS_ERROR_JUKE_DISC_NOT_SET;
				}	
    		}
		IOCTL_IN(arg, dvd_authinfo, ai);
		
		switch(ai.type){
		case  DVD_LU_SEND_AGID:
			dbgl_blk(4,"DVD_LU_SEND_AGID\n");
                        result = 0 ; //return 0;
               		break;
		case  DVD_LU_SEND_KEY1:
			dbgl_blk(4,"DVD_LU_SEND_KEY1\n");
			result = 0 ; //return 0;
                        break;
              case DVD_LU_SEND_CHALLENGE:
                     dbgl_blk(4,"DVD_LU_SEND_CHALLENGE\n");
                     result = 0 ; //return 0;
                     break;
		case DVD_LU_SEND_TITLE_KEY:
			dbgl_blk(4,"DVD_LU_SEND_TITLE_KEY\n");
			result = 0 ; //return 0;
			break;
               	case DVD_LU_SEND_ASF:
			dbgl_blk(4,"DVD_LU_SEND_ASF\n");
			ai.lsasf.asf = 0;
			result = -ENOSYS; //return -ENOSYS;
			break;
		case DVD_HOST_SEND_CHALLENGE:
			dbgl_blk(4,"DVD_HOST_SEND_CHALLENGE\n");
			result = 0 ; //return 0;
			break;
		case DVD_HOST_SEND_KEY2:
			dbgl_blk(4,"DVD_HOST_SEND_KEY2\n");
			result = 0 ; //return 0;
			break;
		case DVD_INVALIDATE_AGID:
			dbgl_blk(4,"DVD_INVALIDATE_AGID\n");
			result = 0 ; //return 0;
			break;
		case DVD_LU_SEND_RPC_STATE:
			dbgl_blk(4,"DVD_LU_SEND_RPC_STATE\n");
			result = 0 ; //return 0;
			break;
		case DVD_HOST_SEND_RPC_STATE:
			dbgl_blk(4,"DVD_HOST_SEND_RPC_STATE\n");
			result = 0 ; //return 0;
			break;
                }
		IOCTL_OUT(arg, dvd_authinfo, ai);		
	}
	break;
	case CDROM_SEND_PACKET:
	{
		struct cdrom_generic_command cgc;

              slot = SLOT_I(inode);
              sd = NDAS_GET_SLOT_DEV(slot);
              partnum = PARTITION(inode->i_rdev);
			  
             if(sd->info.mode != NDAS_DISK_MODE_MEDIAJUKE)
             {
                return NDAS_ERROR_JUKE_DISK_NOT_SET;	
             }

		dbgl_blk(4,"CDROM_SEND_PACKET\n");
		
		 if( partnum != 0 && partnum < NR_PARTITION){
				if(!ndas_IsDiscSet(slot, partnum) ) {
    					return NDAS_ERROR_JUKE_DISC_NOT_SET;
				}	
    		}
		
		IOCTL_IN(arg, struct cdrom_generic_command, cgc);
		dbgl_blk(4,"CDROM_SEND_PACKET 0x%02x\n", cgc.cmd[0]);
//		printk("CDROM_SEND_PACKET 0x%02x\n", cgc.cmd[0]);
		result = Do_ATAPI_CMD(slot,partnum,inode->i_rdev,&cgc);
		return result;
		
	}
	break;
	default:
		result = -1;
	break;
     }

     return result;
}

#endif //#ifdef NDSA_MSHARE

#ifdef XPLAT_XIXFS_EVENT
#if !LINUX_VERSION_25_ABOVE

void ndas_direct_end_io(int slot, ndas_error_t err, void* user_arg)
{
    
    struct nbio *bio = (struct nbio *)user_arg;
    pndas_xixfs_direct_io dio = NULL;	
    int uptodate = NDAS_SUCCESS(err) ? 1 : 0;

	//printk("ndas_direct_end_io nbio%x\n", bio);
	//printk("ndas_direct_end_io user_arg %x\n", user_arg);
    if(!bio) {
	 dbgl_blk(1, "ndas_direct_end_io step 1\n");
	return;
    }
	
    dio = (pndas_xixfs_direct_io) bio->req;
	
    if(!dio) {
	kmem_cache_free(nblk_kmem_cache ,bio);
	dbgl_blk(1, "ndas_direct_end_io step 2\n");
	return;		
    }
	
	
    dbgl_blk(5,"ing err=%d, dio=%p len=%d start sector=%d", err, dio, dio->len, dio->start_sect);
    
    if ( !uptodate ) {
	dbgl_blk(1, "ndasdevice: IO error %s occurred for sector %lu\n", 
	NDAS_GET_STRING_ERROR(err), 
	dio->start_sect+ ndas_hd[MINOR(dio->rdev)].start_sect);
	dio->status = -EIO;
    }

    kmem_cache_free(nblk_kmem_cache ,bio);
/*
#ifdef NDAS_SYNC_IO
#else
    complete(dio->endioComplete);
#endif
*/
   // printk("ndas_direct_end_io step 3\n");
    dbgl_blk(5,"ed");
}





static ndas_error_t ndas_direct_io(int slot, pndas_xixfs_direct_io dio)
{
 	ndas_error_t err;
    	ndas_io_request request;
	int minor = MINOR(dio->rdev);
	struct ndas_slot *sd = NDAS_GET_SLOT_DEV(slot);
	struct nbio *bio = (struct nbio *)nblk_mem_alloc();
	unsigned int max_sectors = NDAS_GET_SLOT_DEV(slot)->info.max_sectors_per_request;
/*
#ifdef NDAS_SYNC_IO
#else
   	DECLARE_COMPLETION(wait);
	dio->endioComplete = &wait;
#endif
*/
	memset(&request, 0, sizeof(ndas_io_request));


	printk("ndas_direct_io dio %x\n", dio);
	if(!bio){
		return NDAS_ERROR_OUT_OF_MEMORY;
	}
	
       if (!sd->enabled) { 
	   	kmem_cache_free(nblk_kmem_cache ,bio);
        	return NDAS_ERROR_NOT_CONNECTED;
      	}
   
	if( dio->len % 512) {
		kmem_cache_free(nblk_kmem_cache ,bio);
		return NDAS_ERROR_INVALID_PARAMETER;
	}



	bio->nr_blocks = 0;
	bio->dev = NDAS_GET_SLOT_DEV(slot);

	if(dio->len  > max_sectors*1024) {
        	dbgl_blk(1, "ed len=%d/%d nr_uio=%d", dio->len, max_sectors*1024 , bio->nr_blocks);
       	 kmem_cache_free(nblk_kmem_cache ,bio);
		return NDAS_ERROR_INVALID_PARAMETER;
        }
	
	if ( sd->info.io_splits == 1){
		
		bio->nr_blocks = 1;
		bio->blocks[0].len = dio->len;
		bio->blocks[0].ptr = dio->buff;
		bio->nr_sectors = (dio->len >>9);
	}else{
		int size = dio->len;
		char *data = (char *)dio->buff;
    		int chunk_size = 1 << 9;
	
		while ( size > 0 ) 
		{
		    int idx = SPLIT_IDX_SLOT(bio->nr_blocks, (dio->len >>9), slot);
		    bio->blocks[idx].len = chunk_size;
		    bio->blocks[idx].ptr = data;
		    size -= chunk_size;
		    data += chunk_size;
		    bio->nr_blocks++;
		}
		bio->nr_sectors = (dio->len >>9);
	}

	bio->req = (struct request *)dio;


	
	request.uio = bio->blocks;
	request.buf = NULL;
	request.nr_uio = bio->nr_blocks;
	request.num_sec = bio->nr_sectors ;// << __ffs(sd->info.io_splits);
	request.start_sec = dio->start_sect+ ndas_hd[minor].start_sect;
//#ifdef NDAS_SYNC_IO
    	request.done = NULL; // Synchrounos IO : sjcho temp
/*
#else
   	request.done = (ndas_io_done) ndas_direct_end_io;
    	request.done_arg = (void *)bio;
	printk("ndas_direct_io nbio %x\n", bio);
	printk("ndas_direct_io nbio %x\n", request.done_arg);	
#endif
*/


    if (dio->start_sect+ request.num_sec > ndas_hd[minor].nr_sects) {
       dbgl_blk(1, "ndas: tried to access area that exceeds the partition boundary\n");
	kmem_cache_free(nblk_kmem_cache ,bio);
       return  NDAS_ERROR_INVALID_OPERATION;
    } else {
	    switch ( dio->cmd) {
	    case READ:
	    case READA:
	        err = ndas_read(slot,&request);
	        break;
	    case WRITE:
	        err = ndas_write(slot,&request);
	        break;
	    default:
	        err = NDAS_ERROR_INVALID_OPERATION;
	        dbgl_blk(1, "ndas: operation %d is not supported.\n", dio->cmd);
	    }    
    }

//#ifdef NDAS_SYNC_IO
    ndas_direct_end_io(slot, err, (void *)bio);
/*
#else
    if ( !NDAS_SUCCESS(err) ) {
	printk("ERROR ndas_direct_io\n");
        kmem_cache_free(nblk_kmem_cache ,bio);
    }else{
    	wait_for_completion(&wait);
    }
#endif
*/
    return err;    
}
#endif


#endif //#ifdef XPLAT_XIXFS_EVENT

int ndop_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    int            slot;
    struct ndas_slot *sd;
    unsigned int        result = 0;

    dbgl_blk(3,"ing inode=%p, filp=%p, cmd=0x%x, arg=%p", inode, filp, cmd, (void*) arg);

    if(!capable(CAP_SYS_ADMIN))
        return -EPERM;

    if(!inode) {
        printk("ndas: open called with inode = NULL\n");
        return -EIO;
    }

    slot = SLOT_I(inode);
    
    dbgl_blk(2,"slot =%d", slot);
    if (slot > NDAS_MAX_SLOT_NR) {
        printk("ndas: Minor too big.\n"); // Probably can not happen
        return -ENXIO;
    }

    sd = NDAS_GET_SLOT_DEV(slot);

    dbgl_blk(3,"slot=0x%x", slot);
    
    switch(cmd) {

    case 0x301: /* command 0x301 */
    {
        struct struct_0x301 {
            xuint8 headers_per_disk;
            xuint8 sectors_per_cylinder;
            xuint8 cylinders_per_header;
            xuint32 start_sect;
        };
        struct struct_0x301 *x301 = (struct struct_0x301 *)arg;
        ndas_slot_info_t info;
        ndas_error_t error;
        int start_sect = 0;
        dbgl_blk(3,"0x301");
        
        if(!arg) return -EINVAL;
            
        result = access_ok(VERIFY_WRITE, x301, sizeof(*x301)) == TRUE  ? 0 : -EFAULT;
        
        error = ndas_query_slot(slot,&info);
        if ( error != NDAS_OK ) {
            dbgl_blk(1, "ndas_query_slot err=%s", NDAS_GET_STRING_ERROR(error));
            return -EINVAL;
        }
#if !LINUX_VERSION_25_ABOVE
        start_sect = ndas_gd.part[MINOR(inode->i_rdev)].start_sect;
#endif

        //put_user(info.cylinders_per_header, &x301->cylinders_per_header);
        //put_user(info.headers_per_disk,  &x301->headers_per_disk);    
        //put_user(info.sectors_per_cylinder, &x301->sectors_per_cylinder);    
        put_user(start_sect, &x301->start_sect); // TODO: for partition    
    }
    break;
#if !LINUX_VERSION_25_ABOVE
    case BLKGETSIZE:
    {
        ndas_slot_info_t info;
        ndas_error_t error;
        dbgl_blk(3,"BLKGETSIZE");
        
        error = ndas_query_slot(slot,&info);
        if ( error != NDAS_OK ) {
            dbgl_blk(1, "ndas_query_slot err=%s", NDAS_GET_STRING_ERROR(error));
            result = -EINVAL;
            break;
        }

        result = put_user(ndas_gd.part[MINOR(inode->i_rdev)].nr_sects, (long*)arg);
        dbgl_blk(3,"#sectors=%ld", ndas_gd.part[MINOR(inode->i_rdev)].nr_sects);

    }
    break;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
    case BLKGETSIZE64:
    {
        ndas_slot_info_t info;
        ndas_error_t error;
        xuint64 size;
        dbgl_blk(3,"BLKGETSIZE64");
        
        error = ndas_query_slot(slot,&info);
        if ( error != NDAS_OK ) {
            dbgl_blk(1, "ndas_query_slot err=%s", NDAS_GET_STRING_ERROR(error));
            return -EINVAL;
        }
	if (MINOR(inode->i_rdev) ==0)
		size = info.capacity;
	else
		size = ((xuint64)ndas_gd.part[MINOR(inode->i_rdev)].nr_sects) * 512;
        result = put_user(size, (xuint64*)arg);
    }
    break;
#endif
    case BLKRRPART:
        dbgl_blk(3,"BLKRRPART");
        result = ndas_ops_read_partition(slot) == NDAS_OK ? 0 : -EINVAL;
        break;
    case BLKROSET:
    case BLKROGET:
    case BLKRASET:
    case BLKRAGET:
    case BLKFLSBUF:
    case BLKSSZGET:
    case BLKELVGET:
    case BLKELVSET:
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
    case BLKBSZGET:
    case BLKBSZSET:
#endif
#if LINUX_VERSION_23_ABOVE
    case BLKPG:
        result = blk_ioctl(inode->i_rdev, cmd, arg);
#endif

        break;
#endif // 2.4.x    

#ifdef NDAS_MSHARE
    case IOCTL_NDCMD_MS_SETUP_WRITE:
    case IOCTL_NDCMD_MS_END_WRITE	:
    case IOCTL_NDCMD_MS_SETUP_READ:
    case IOCTL_NDCMD_MS_END_READ:
    case IOCTL_NDCMD_MS_DEL_DISC:
    case IOCTL_NDCMD_MS_VALIDATE_DISC:
    case IOCTL_NDCMD_MS_FORMAT_DISK:
    case IOCTL_NDCMD_MS_GET_DISK_INFO:
    case IOCTL_NDCMD_MS_GET_DISC_INFO:
    case CDROMREADTOCHDR:	
    case CDROMREADTOCENTRY:
    case DVD_READ_STRUCT:
    case DVD_AUTH:
    case CDROM_SEND_PACKET:	
{
	return juke_ioctl(inode, filp, cmd, arg);
}
#endif //#ifdef NDAS_MSHARE

#ifdef XPLAT_XIXFS_EVENT
#if !LINUX_VERSION_25_ABOVE
    case IOCTL_XIXFS_GET_PARTINFO:
    {
	 pndas_xixfs_partinfo part_info = (pndas_xixfs_partinfo)arg;
        ndas_slot_info_t info;
        ndas_error_t error;
  
        dbgl_blk(3,"IOCTL_XIXFS_GET_PARTINFO");
        
        if(!arg) return -EINVAL;
        
        error = ndas_query_slot(slot,&info);
        if ( error != NDAS_OK ) {
            dbgl_blk(1, "ndas_query_slot err=%s", NDAS_GET_STRING_ERROR(error));
            return -EINVAL;
        }
		
	part_info->start_sect = ndas_gd.part[MINOR(inode->i_rdev)].start_sect;
	part_info->nr_sects = ndas_gd.part[MINOR(inode->i_rdev)].nr_sects;
	part_info->partNumber = MINOR(inode->i_rdev);
	result = 0;

    }
    break;
#if 0
    case IOCTL_XIXFS_DIRECT_IO:
    {
	pndas_xixfs_direct_io ndas_d_io = (pndas_xixfs_direct_io)arg;
	ndas_error_t error;
	//printk("IOCTL_XIXFS_DIRECT_IO\n");
	ndas_d_io->rdev = inode->i_rdev;
	
	error = ndas_direct_io(slot,ndas_d_io);
	if ( error != NDAS_OK ) {
	    //printk("ERROR ndas_direct_io\n");
            dbgl_blk(1, "ndas_direct_io=%s", NDAS_GET_STRING_ERROR(error));
	     ndas_d_io->status = -EIO;
            return -EINVAL;
       }
	
	//printk("IOCTL_XIXFS_DIRECT_IO END\n");
	result = 0;
	
    }
    break;
#endif // #if 0
#endif
	case IOCTL_XIXFS_GET_DEVINFO:
	{
		pndas_xixfs_devinfo pbdevInfo;
		ndas_slot_info_t info;
        	ndas_error_t error;
		
		
		//printk("IOCTL_XIXFS_GET_DEVINFO 1\n");

		pbdevInfo = (pndas_xixfs_devinfo)arg;

		error = ndas_query_slot(slot,&info);
		
        	if ( error != NDAS_OK ) {
			dbgl_blk(1, "error :  IOCTL_XIXFS_GET_DEVINFO: ndas_query_slot");
            		dbgl_blk(1, "ndas_query_slot err=%s", NDAS_GET_STRING_ERROR(error));
            		return -EINVAL;
        	}


		if( info.writable || info.writeshared) {
			//printk("IOCTL_XIXFS_GET_DEVINFO writable mode \n");
			pbdevInfo->dev_usermode = 1;
		}else{
			//printk("IOCTL_XIXFS_GET_DEVINFO read only mode writable %d \n", info.writable);
			pbdevInfo->dev_usermode = 0;
		}

		//printk("IOCTL_XIXFS_GET_DEVINFO 2\n");

		if(sd->info.writeshared == TRUE) {
			//printk("IOCTL_XIXFS_GET_DEVINFO allow share write \n");
			pbdevInfo->dev_sharedwrite = 1;
		}else {
			//printk("IOCTL_XIXFS_GET_DEVINFO not allow shared write \n");
			pbdevInfo->dev_sharedwrite = 0;
		}

		pbdevInfo->status = 0;
		

		result = 0;
		
	}
	break;
#if 0
	case IOCTL_XIXFS_GLOCK	:
	{
		pndas_xixfs_global_lock plockinfo;
		ndas_error_t error;

		//printk("IOCTL_XIXFS_GLOCK 1\n");

	       plockinfo = (pndas_xixfs_global_lock) arg;

		//printk("IOCTL_XIXFS_GLOCK 2\n");
		if( plockinfo->lock_type == 1) {
			error = ndas_lock_operation(slot, NDAS_LOCK_TAKE, XIXFS_GLOBAL_LOCK_INDEX, NULL); 
			//printk("IOCTL_XIXFS_GLOCK 2-1\n");
		}else if ( plockinfo->lock_type == 0 ) {
			error = ndas_lock_operation(slot, NDAS_LOCK_GIVE, XIXFS_GLOBAL_LOCK_INDEX, NULL); 
			//printk("IOCTL_XIXFS_GLOCK 2-2\n");
		}else {
			ndas_lock_operation(slot, NDAS_LOCK_BREAK_TAKE, XIXFS_GLOBAL_LOCK_INDEX, NULL); 
			printk("error : invalid GLOCK operation\n");
			return -EINVAL;
		}


		//printk("IOCTL_XIXFS_GLOCK 3\n");
		plockinfo->lock_status = NDAS_SUCCESS(error) ? 0 : -EINVAL;


		result = NDAS_SUCCESS(error) ? 0 : -EINVAL;
		//printk("IOCTL_XIXFS_GLOCK Status(%d)\n", plockinfo->lock_status);
		
	}
	break;	
#endif // #if 0
#endif //#ifdef XPLAT_XIXFS_EVENT

    default:
        return -EINVAL;
    }

    dbgl_blk(3,"ed cmd=%x result=%d", cmd, result);
    return result;
}



#ifdef XPLAT_XIXFS_EVENT
NDASUSER_API
int ndas_block_xixfs_lock(pndas_xixfs_global_lock plockinfo)
{ 
		int            slot;
		int		 result = 0;
		ndas_error_t error = NDAS_OK;
		
		slot = SLOT(plockinfo->rdev);

		printk("ndas_block_xixfs_lock ENTER\n");
		if( plockinfo->lock_type == 1) {
			error = ndas_lock_operation(slot, NDAS_LOCK_TAKE, XIXFS_GLOBAL_LOCK_INDEX, NULL, NULL); 
			printk("ndas_block_xixfs_lock TAKE\n");
		}else if ( plockinfo->lock_type == 0 ) {
			error = ndas_lock_operation(slot, NDAS_LOCK_GIVE, XIXFS_GLOBAL_LOCK_INDEX, NULL, NULL); 
			printk("ndas_block_xixfs_lock GIVE\n");
		}else {
			ndas_lock_operation(slot, NDAS_LOCK_BREAK, XIXFS_GLOBAL_LOCK_INDEX, NULL, NULL); 
			printk("error : invalid GLOCK operation\n");
			return - EINVAL;
		}
		
		 result = NDAS_SUCCESS(error) ? 0 : -EINVAL;
		 printk("ndas_block_xixfs_lock END\n");
		return result;
}



#if !LINUX_VERSION_25_ABOVE
NDASUSER_API
int ndas_block_xixfs_direct_io(pndas_xixfs_direct_io ndas_d_io)
{
	ndas_error_t error;
	int slot = SLOT(ndas_d_io->rdev);
	printk("ndas_block_xixfs_direct_io ENTER\n");
	
	error = ndas_direct_io(slot,ndas_d_io);
	if ( error != NDAS_OK ) {
	    //printk("ERROR ndas_direct_io\n");
            dbgl_blk(1, "ndas_direct_io=%s", NDAS_GET_STRING_ERROR(error));
            return -EIO;
       }
	printk("ndas_block_xixfs_direct_io END\n");

	return ndas_d_io->status;
	
}

#endif //#if !LINUX_VERSION_25_ABOVE


#endif //#ifdef XPLAT_XIXFS_EVENT





#if LINUX_VERSION_25_ABOVE
int ndop_media_changed (struct gendisk *g) 
{
    dbgl_blk(3,"gendisk=%s", DEBUG_GENDISK(g));
    return 1;
}
int ndop_revalidate_disk(struct gendisk *g)
{
    int slot;
    dbgl_blk(8,"gendisk=%s", DEBUG_GENDISK(g));

    slot = SLOT(g);
    if (g->major != NDAS_BLK_MAJOR) {
        dbgl_blk(1,"not a netdisk");
        return 0;
    }
    
    if (slot > NDAS_MAX_SLOT_NR) {
        dbgl_blk(1,"Minor too big.\n"); 
        return 0;
    }
    dbgl_blk(8,"ed");
    return 0;
}
#else
int ndop_check_netdisk_change(kdev_t rdev)
{
    dbgl_blk(3, "kdev_t=%d",rdev);

    return 1;
}

int ndop_revalidate(kdev_t rdev)
{
    int slot;

    dbgl_blk(3, "kdev_t=%d",rdev);

    slot = SLOT(rdev);
    if (MAJOR(rdev) != NDAS_BLK_MAJOR) {
        dbgl_blk(1,"not a netdisk");
        return 0;
    }
    
    if (slot > NDAS_MAX_SLOT_NR) {
        dbgl_blk(1,"Minor too big.\n"); 
        return 0;
    }
    if ( PARTITION(rdev) != 0 ) {
        dbgl_blk(1,"not a disk\n"); 
        return 0;
    }
    return ndas_ops_read_partition(slot) == NDAS_OK ? 0 : 0;
}
#endif

