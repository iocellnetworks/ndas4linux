#ifdef XPLAT_JUKEBOX

#include "xplatcfg.h"
#include <sal/thread.h>
#include <sal/time.h>
#include <sal/sync.h>
#include <sal/mem.h>

#include <xlib/dpc.h>
#include <xlib/xbuf.h>

#include <netdisk/netdiskid.h>
#include <netdisk/ndasdib.h>
#include <netdisk/sdev.h>
#include <netdisk/conn.h>
#include <netdisk/ndasdib.h>
#include <netdisk/scrc32.h>
#include <ndasuser/def.h>

#include "mediajukedisk.h"
#include <jukebox/diskmgr.h>
#include <ndasuser/mediaop.h>
#include "metaop.h"
#include "metainfo.h"
#include "medialog.h"
#include "cipher.h"
#define DEBUG_LEVEL_MSHARE_MEDIAOP  5

#define    debug_mediaOp(l, x)    do {\
    if(l <= DEBUG_LEVEL_MSHARE_MEDIAOP) {     \
        sal_debug_print("UD|%d|%s|",l,__FUNCTION__); \
        sal_debug_println x;    \
    } \
} while(0)    



int get_stream_id(struct nd_diskmgr_struct * diskmgr)
{
	int i = 0;
	for(i = 1; i< MAX_PART; i++)
	{
		if(diskmgr->disc_part_map[i] == -1){
			return  i;
		}
	}
	return -1;
}


int check_stream_id(struct nd_diskmgr_struct * diskmgr, int disc_num)
{
	int i = 0;
	for(i = 1; i < MAX_PART; i++)
	{
		if(diskmgr->disc_part_map[i] == disc_num) return i;
	}

	return -1;
}


NDASUSER_API
int ndas_deallocate_part_id(int slot, unsigned int partid)
{
	logunit_t * sdev = NULL;
	udev_t * udev = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;

	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);

	udev = SDEV2UDEV(sdev);
	sal_assert(udev);

	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);

	if(partid == 0 || partid >= MAX_PART) return -1;

	if(diskmgr->Disc_info[partid] != NULL && diskmgr->disc_part_map[partid] == -1) {
		goto free_space;
	}

	if(diskmgr->Disc_info[partid] == NULL ) {
		diskmgr->disc_part_map[partid] = -1;
		return 0;
	}

	diskmgr->disc_part_map[partid] = -1;

free_space:
	if(diskmgr->Disc_info[partid]->disc_key)
		sal_free(diskmgr->Disc_info[partid]->disc_key);
	if(diskmgr->Disc_info[partid]->disc_info)
		sal_free(diskmgr->Disc_info[partid]->disc_info);
	if(diskmgr->Disc_info[partid])
		sal_free(diskmgr->Disc_info[partid]);
	diskmgr->Disc_info[partid] = NULL;
	return 0;
}



NDASUSER_API
int ndas_allocate_part_id(int slot, int selected_disc)
{
	int partid = -1;
	char * buf = NULL;
	int result = -1;
	logunit_t * sdev =NULL;
	udev_t * udev = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct media_disc_map * mediainfo = NULL;
	PDISC_LIST	disc_list = NULL;
	PON_DISC_ENTRY entry = NULL;
	unsigned char * pKey = NULL;
	struct Disc * disc = NULL;

	sdev = sdev_lookup_byslot( slot);
	sal_assert(sdev);

	udev = SDEV2UDEV(sdev);
	sal_assert(udev);

	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);

	mediainfo = diskmgr->mediainfo;
	sal_assert(mediainfo);

	if(selected_disc >= mediainfo->count) {
		sal_debug_println("disc is too big!\n");
		return -1;
	}

	disc_list = &mediainfo->discinfo[selected_disc];
	
	partid = get_stream_id(diskmgr);
	
	if(partid == -1) return -1;


	disc = (struct Disc *)sal_malloc(sizeof(struct Disc));
	if(!disc){
		sal_debug_println("allocate_part_id can't alloc disc!\n");
		goto error_out;
	}
	sal_memset(disc,0,sizeof(struct Disc));



	disc->disc_info = (struct media_disc_info *)sal_malloc(sizeof(struct media_disc_info));
	if(!disc->disc_info){
		sal_debug_println("allocate_part_id can't alloc disc_key!\n");
		goto error_out;
	}
	sal_memset(disc->disc_info, 0, sizeof(struct media_disc_info));

	buf = (unsigned char *)sal_malloc(4096);
	if(!buf)
	{
		sal_debug_println("allocate_part_id can't alloc buf!\n");
		goto error_out;
	}
	sal_memset(buf, 0, 4096);

	if(!GetDiscMeta(sdev, disc_list, buf, DISC_META_SECTOR_COUNT))
	{
		sal_debug_println("allocate_part_id Error GetDiskMeta\n");
		goto error_out;		
	}
	
	entry = (PON_DISC_ENTRY)buf;

	disc->dev_num =(  diskmgr->dev_num |partid); 
	disc->disc_num = entry->index;
	disc->disc_info->status = entry->status;
	disc->disc_info->pt_loc = entry->loc;
	disc->disc_info->nr_clusters = entry->nr_DiscCluster;
	disc->disc_info->encrypt = entry->encrypt;
	AllocAddrEntities(&disc->disc_info->address, entry);

	if(entry->encrypt){
		disc->disc_key = (struct media_key *)sal_malloc(sizeof(struct media_key));
		if(!disc->disc_key){
			sal_debug_println("allocate_part_id can't alloc disc_key!\n");
			goto error_out;
		}
		sal_memset(disc->disc_key, 0, sizeof(struct media_key));
		
		pKey = entry->HINT;	
		if(!CreateCipher(
				(PNCIPHER_INSTANCE)&disc->disc_key->cipherInstance,	
				NDAS_CIPHER_AES,
				NCIPHER_MODE_ECB,
				HASH_KEY_LENGTH,
				(unsigned char *)NULL
				)
		)
		{
			sal_debug_println("can't set Keyinstance  Media Key\n");
			goto error_out;
			
		}

		if(!CreateCipherKey(
				(PNCIPHER_KEY)disc->disc_key->cipherKey,
				(PNCIPHER_INSTANCE)disc->disc_key->cipherInstance,	
				NDAS_CIPHER_AES_LENGTH,
				pKey
				)
		)
 		{
			sal_debug_println("can't set Key  Media Key\n");
			goto error_out;
		}		
			
	}
//	set information
	diskmgr->disc_part_map[partid] = entry->index;
	diskmgr->Disc_info[partid] = disc ;

	result = partid;
	sal_free(buf);
	return result;
error_out:
	if(disc->disc_info) sal_free(disc->disc_info);
	if(disc->disc_key) sal_free(disc->disc_key);
	if(disc) sal_free(disc);
	if(buf) sal_free(buf);
	return -1;

}




int translate(
				struct Disc * disc, 
				unsigned int req_startsec, 
				unsigned int req_sectors, 
				unsigned int *start_sec, 
				unsigned int *sectors
			)
{
	struct media_addrmap * map = NULL;
	PDISC_MAP_LIST	addr = NULL;
	unsigned int	end_sector =  0;	
	unsigned int 	map_count = 0, i= 0;
	
	sal_assert(disc->disc_info);
	map = &disc->disc_info->address;


	end_sector = req_startsec + req_sectors -1 ;
	map_count = map->count;
	for(i = 0; i < map_count; i++)
	{
		addr = &(map->addrMap[i]);
		
		if( (addr->Lg_StartSector  + addr->nr_SecCount -1) > end_sector )
		{
			if(addr->Lg_StartSector <= req_startsec  )
			{
				*start_sec = addr->StartSector + ( req_startsec - addr->Lg_StartSector) ;
				*sectors = req_sectors;
			}else{

				if(i == 0)
				{
					return 0;
				}

				addr = &(map->addrMap[i-1]);
				*start_sec = addr->StartSector + ( req_startsec - addr->Lg_StartSector);
				*sectors = addr->nr_SecCount - (req_startsec - addr->Lg_StartSector);
			}	
			break;
		}
	}

	if(i >= map_count) return 0;
	return 1;	
}

NDASUSER_API
int ndas_TranslateAddr(
				int slot,
				unsigned int partnum,
				unsigned int req_startsec, 
				unsigned int req_sectors, 
				unsigned int *start_sec, 
				unsigned int *sectors
				)
{

	logunit_t * sdev = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct Disc * disc = NULL;
	

	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);

	diskmgr = SDEV2UDEV(sdev)->Disk_info;
	sal_assert(diskmgr);

	disc = diskmgr->Disc_info[partnum];
	if(disc == NULL) {
		sal_debug_println("can't find Disc information\n");
		return 0;
	}

	return translate(disc, req_startsec, req_sectors, start_sec, sectors);
}

NDASUSER_API
void ndas_freeDisc(struct Disc * disc)
{
	if(disc->disc_key)
		sal_free(disc->disc_key);
	if(disc->disc_info)
		sal_free(disc->disc_info);
	sal_free(disc);
}



NDASUSER_API
int ndas_IsDiscSet(int slot, int partnum)
{
	logunit_t * sdev = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct Disc * disc = NULL;
	
	sdev = sdev_lookup_byslot( slot);
	sal_assert(sdev);

	diskmgr = SDEV2UDEV(sdev)->Disk_info;
	sal_assert(diskmgr);	

	disc = diskmgr->Disc_info[partnum];
	//error check
	if( (diskmgr->disc_part_map[partnum] == -1) &&  (diskmgr->Disc_info[partnum] != NULL))
	{
		ndas_freeDisc(diskmgr->Disc_info[partnum]);
		diskmgr->Disc_info[partnum] = NULL;

	}

	if((diskmgr->disc_part_map[partnum] != -1) && (diskmgr->Disc_info[partnum] == NULL))
	{
		diskmgr->disc_part_map[partnum] = -1;
	}

	if(diskmgr->disc_part_map[partnum] == -1) return 0;
	return 1;
	
}

NDASUSER_API
int ndas_GetDiscStatus(int slot, int partnum)
{
	logunit_t * sdev = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct Disc * disc = NULL;

	sdev = sdev_lookup_byslot( slot);
	sal_assert(sdev);

	diskmgr = SDEV2UDEV(sdev)->Disk_info;
	sal_assert(diskmgr);	

	disc = diskmgr->Disc_info[partnum];
	sal_assert(disc);

	return disc->disc_info->status;
}


NDASUSER_API
void * ndas_getkey(int slot, int partnum)
{
	logunit_t * sdev = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct Disc * disc = NULL;
	struct media_disc_info * disc_info = NULL;
	sdev = sdev_lookup_byslot( slot);
	sal_assert(sdev);

	diskmgr = SDEV2UDEV(sdev)->Disk_info;
	sal_assert(diskmgr);

	disc = diskmgr->Disc_info[partnum];
	sal_assert(disc);

	disc_info = disc->disc_info;
	sal_assert(disc->disc_info);

	if(disc_info->encrypt == 1) return (void *)disc->disc_key;
	return (void *)NULL;
}


NDASUSER_API
void ndas_encrypt(void * key, char * inbuf, char * outbuf, unsigned int size)
{
	struct media_key * Mkey = (struct media_key *)key;
	EncryptBlock(
			(PNCIPHER_INSTANCE)Mkey->cipherInstance,
			(PNCIPHER_KEY)Mkey->cipherKey,
			size,
			(unsigned char*) inbuf, 
			(unsigned char*) outbuf
			);
	return;
}

NDASUSER_API
void ndas_decrypt(void * key, char * inbuf,  char * outbuf, unsigned int size)
{
	struct media_key * Mkey = (struct media_key *)key;
	DecryptBlock(
			(PNCIPHER_INSTANCE)Mkey->cipherInstance,
			(PNCIPHER_KEY)Mkey->cipherKey,
			size,
			(unsigned char*) inbuf, 
			(unsigned char*) outbuf
			);
	return;	
}

//#define REQ_MAX_WAIT_TIME	(SAL_TICKS_PER_SEC*10)

NDASUSER_API
int ndas_ATAPIRequest(
	int slot,
	int partnum,
	unsigned short dev, 
	int command, 
	int start_sector, 
	int nr_sectors, 
	char * buffer
	)
{
	
#ifdef XPLAT_ASYNC_IO
    struct udev_request_block *tir = NULL;
    struct juke_io_blocking_done_arg darg;
#endif
    logunit_t * sdev = NULL;	
    udev_t * udev = NULL;
    ndas_error_t err = NDAS_OK;
    ndas_io_request *ioreq;
    struct nd_diskmgr_struct * diskmgr = NULL;
    struct media_key * MKey = NULL;
    struct Disc * disc = NULL;
    int ret;
//    sal_tick wait_time = REQ_MAX_WAIT_TIME;

 
    int processed_sectors = 0;
    int req_sectors = nr_sectors;
    int req_start_sec = start_sector;
    int sectors = 0;
    int s_sector = 0;

    sdev = sdev_lookup_byslot(slot);
    sal_assert(sdev);
    udev = SDEV2UDEV(sdev);	
    sal_assert(udev);
    	
    diskmgr= udev->Disk_info;
    sal_assert(diskmgr);

    disc = diskmgr->Disc_info[partnum];
    sal_assert(disc);

    sal_assert(disc->disc_info);
    if(disc->disc_info->encrypt == 1)
    {
   	    MKey = disc->disc_key;
	    sal_assert(MKey);
	
	     if(command == ND_CMD_WRITE)
	    {

		EncryptBlock(
			(PNCIPHER_INSTANCE)MKey->cipherInstance,
			(PNCIPHER_KEY)MKey->cipherKey,
			nr_sectors*SECTOR_SIZE,
			(xuint8*) buffer, 
			(xuint8*) buffer
			);

	    }
    }

   while(req_sectors > 0)
   {
           if(!translate(disc, req_start_sec, req_sectors, &s_sector, &sectors) )
           {
           		sal_error_print("Trnaslation Fail\n");
           		return err;
           }

	   
#ifdef XPLAT_ASYNC_IO
        tir = tir_alloc(command, NULL);
        tir->func = NULL;
        tir->arg = 0;

        ioreq = tir->req;
	    ioreq->buf = (xint8 *)(buffer + processed_sectors * MS_SECTOR_SIZE);
	    ioreq->start_sec = s_sector;
	    ioreq->num_sec = sectors;
        ioreq->done = juke_io_blocking_done;
        ioreq->done_arg = &darg;

	    darg.done_sema = sal_semaphore_create("ndas_ioctl_done", 1, 0);  
	    sdev->ops->io(sdev, tir, FALSE);

            do {
        	    ret = sal_semaphore_take(darg.done_sema, SAL_SYNC_FOREVER);
            } while (ret == SAL_SYNC_INTERRUPTED);

	    sal_semaphore_give(darg.done_sema);
	    sal_semaphore_destroy(darg.done_sema);
	    debug_mediaOp(5, ("ed err=%d", darg.err));
	    err = darg.err;
#else
	   sal_assert(udev);

	    if (udev->conn.writeshare_mode) {
	        err = nwl_take(udev, 2000);
	        if (!NDAS_SUCCESS(err)) 
	            goto io_out;
	    }
	    err = conn_rw(&udev->conn, command,
                s_sector, sectors, (xint8 *)(buffer + processed_sectors * MS_SECTOR_SIZE);

	io_out:
	    if (udev->conn.writeshare_mode) {
	        nwl_give(udev);
	    }	
#endif

	    if (!NDAS_SUCCESS(err)) {
	        sal_error_print("ndas_ATAPIRequest: %d", err);
	        return err;
	    }

	    req_sectors -= sectors;
	    processed_sectors += sectors;
	    req_start_sec += sectors;
	    if(req_sectors > 0)  sal_error_print("request_splited!!!!!\n");
   }

   if(disc->disc_info->encrypt == 1)
    {
   	    MKey = disc->disc_key;
	    sal_assert(MKey);
	
	     if(command == ND_CMD_READ)
	    {
		debug_mediaOp(2,("Encrypted Media\n"));
		DecryptBlock(
			(PNCIPHER_INSTANCE)MKey->cipherInstance,
			(PNCIPHER_KEY)MKey->cipherKey,
			nr_sectors*SECTOR_SIZE,
			(xuint8*) buffer, 
			(xuint8*) buffer
			);

	     }
   }
   
    return NDAS_OK;
}





int MakeDiscView(logunit_t *sdev)
{
	unsigned char * buf = NULL;
	struct nd_diskmgr_struct *diskmgr = NULL;
	struct media_disc_map * Mdiscinfo = NULL;
	int result = 0;
	int i = 0;
	udev_t * udev;
	udev = SDEV2UDEV(sdev);
	sal_assert(udev);
	
	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);
	
	
	debug_mediaOp(2,("enter diskmgr_MakeDiscView---\n"));
	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT)
	{
		sal_debug_println("DMGR_FLAG_NEED_FORMAT\n");
		debug_mediaOp(2,("POSE diskmgr->mediainfo %p", diskmgr->mediainfo));
		Mdiscinfo = (struct media_disc_map *)diskmgr->mediainfo;
		sal_memset(Mdiscinfo, 0, sizeof(struct media_disc_info));
		Mdiscinfo->count = 0;
		return 0;
	}
	
	buf = (unsigned char *)sal_malloc(4096);
	if(!buf)
	{
		sal_debug_println("fail get ubuf\n");
		return 0;
	}
	sal_memset(buf, 0, 4096);

	// get Disk Meta
	if(!GetDiskMeta(sdev, buf, DISK_META_READ_SECTOR_COUNT))
	{
		sal_debug_println("fail GetDiskMeta\n");
		result = 0;
		goto error_out;
	}	
	SetDiskMetaInformation(sdev, (PON_DISK_META)buf, 0);

	if(!GetDiskMeta(sdev, buf, DISK_META_READ_SECTOR_COUNT))
	{
		sal_debug_println("fail GetDiskMeta\n");
		result = 0;
		goto error_out;
	}	

	Mdiscinfo = (struct media_disc_map *)diskmgr->mediainfo;
	sal_assert(Mdiscinfo);
	
	for(i = 0; i< Mdiscinfo->count; i++)
	{
		sal_memset(buf, 0, 4096);
		if(Mdiscinfo->discinfo[i].valid & DISC_LIST_ASSIGNED)
		{
			if(!GetDiscMeta(sdev, 
					&Mdiscinfo->discinfo[i], 
					buf, 
					DISC_META_SECTOR_COUNT))
			{
				sal_debug_println("fail GetDiscMeta\n");
				result = 0;
				goto error_out;
			}
			SetDiscEntryInformation(sdev, (PON_DISC_ENTRY)buf, 0);
		}
	}		
	debug_mediaOp(2,("---end diskmgr_MakeDiscView\n"));
	result = 1;

error_out:
	sal_free(buf);
	return  result;
}


int MakeView(int slot)
{
	logunit_t * sdev;
	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);
	return MakeDiscView(sdev);
}

NDASUSER_API
// general interface for wirting start
int ndas_BurnStartCurrentDisc ( 
				int slot,
				unsigned int	Size_sector,
				unsigned int	selected_disc,
				unsigned int	encrypted,
				unsigned char * HINT
						  )
{	
	unsigned int currentDisc;
	unsigned int sector_count = Size_sector;		
	PON_DISK_META	hdisk;
	PON_DISC_ENTRY  entry;
	PDISC_LIST	disc_list;
	int		result;
	unsigned char *buf = NULL;
	unsigned char *buf2 = NULL;
	logunit_t * sdev = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct media_disc_map * Mdiscinfo = NULL;
	udev_t * udev;
	unsigned int hostid;
	
	sdev = sdev_lookup_byslot( slot);
	sal_assert(slot);
	udev = SDEV2UDEV(sdev);
	sal_assert(udev);
	
	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);
	sal_assert(HINT);

	hostid = diskmgr->hostid;
	
	Mdiscinfo = (struct media_disc_map *) diskmgr->mediainfo;
	sal_assert(Mdiscinfo);
	
	currentDisc = selected_disc;

	debug_mediaOp(2,("enter BurnStartCurrentDisc\n"));

	if(currentDisc >= (unsigned int)Mdiscinfo->count)
	{
		sal_debug_println("BurnStartCurrentDisc : invalid burn->disc(%d) Mdiscinfo->count(%d)!!\n",
				currentDisc, Mdiscinfo->count);
		return 0;
	}
	
	if(currentDisc < 0){
		
		sal_debug_println("BurnStartCurrentDisc invalid disc (%d)\n", currentDisc);
	}

	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT){
		sal_debug_println("BurnStartCurrentDisc Error NEED FORMAT!!\n");
		return 0;
	}

	buf = (unsigned char *)sal_malloc(4096);
	if(!buf)
	{
		sal_debug_println("BurnStartCurrentDisc can't alloc ubuf1!\n");
		return 0;
	}
	sal_memset(buf, 0, 4096);

	buf2 = (unsigned char *)sal_malloc(4096);
	if(!buf2)
	{
		sal_debug_println("BurnStartCurrentDisc can't alloc ubuf2!\n");
		result = 0;
		goto error_out;
	}
	sal_memset(buf2, 0, 4096);

	disc_list = Mdiscinfo->discinfo;

	// update Disk meta
	if((result = _CheckUpdateDisk(sdev, 1, hostid ))== 0 )
	{
		sal_debug_println("BurnStartCurrentDisc Error _CheckUpdateDisk \n");
		result= 0;
		goto error_out;		
	}
	
RECHECK:
	if(( result = _CheckUpdateSelectedDisk( sdev, 1, hostid,currentDisc)) == 0)
	{
		sal_debug_println("BurnStartCurrentDisc Error _CheckUpdateSelectedDisk 3\n");
		result = 0;
		goto error_out;
	}

	
	if(!GetDiskMeta(sdev, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		sal_debug_println("BurnStartCurrentDisc Error _GetDiskMeta\n");
		result = 0;
		goto error_out;
	}
	
	SetDiskMetaInformation(sdev, (PON_DISK_META)buf, 1);

	hdisk = (PON_DISK_META)(buf);
	
	// check current dics status
	if(disc_list[currentDisc].valid & DISC_LIST_ASSIGNED)
	{
		sal_debug_println("BurnStartCurrentDisc Error already alloc\n");
		result = 0;
		goto error_out;
	}

	if(!GetDiscMeta(sdev, &disc_list[currentDisc], buf2, DISC_META_SECTOR_COUNT))
	{
		sal_debug_println("BurnStartCurrentDisc Error GetDiskMeta\n");
		result = 0;
		goto error_out;		
	}
	
	entry = (PON_DISC_ENTRY) buf2;

	if(entry->status != DISC_STATUS_INVALID) 
	{
			// un reachable code			
		if(!_InvalidateDisc (sdev, buf, buf2, hostid))
		{
			sal_debug_println("BurnStartCurrentDisc Error _InvalidateDisc\n");
			result = 0;
			goto error_out;
		}
		goto RECHECK;			
	}

	if(encrypted == 1)
	{
		sal_memset(entry->HINT,0,32);
		sal_memcpy(entry->HINT,HINT,32);		
	}


	// update disk and disc information
	if(!_WriteAndLogStart(sdev, hdisk,entry, hostid,currentDisc,sector_count, encrypted))
	{	
		sal_debug_println("BurnStartCurrentDisc Error: WriteAdnLogStart\n");
		result = 0;
		goto error_out;
	}

	SetDiskMetaInformation( sdev, (PON_DISK_META)buf, 1);
	SetDiscEntryInformation(sdev, (PON_DISC_ENTRY)buf2, 1);
	
	result = 1;

error_out:
	if(buf) sal_free(buf);			
	if(buf2) sal_free(buf2);
	return result;			
	
}




NDASUSER_API
int ndas_BurnEndCurrentDisc( 
			int slot,
			unsigned int  selected_disc, 
			struct disc_add_info	* info
			)
{
	unsigned int currentDisc;
	int result;
	logunit_t * sdev = NULL;
	PON_DISK_META	hdisk = NULL;
	PON_DISC_ENTRY  entry;
	PDISC_LIST	disc_list = NULL;
	
	unsigned char *	p;	

	unsigned char *buf = NULL;
	unsigned char *buf2 = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct media_disc_map * Mdiscinfo = NULL;
	udev_t * udev = NULL;
	unsigned int hostid;
	
	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);
	
	udev = SDEV2UDEV(sdev);
	sal_assert(udev);
	
	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);

	hostid = diskmgr->hostid;
	
	debug_mediaOp(2,("enter BurnEndCurrentDisc\n"));
	
	Mdiscinfo = (struct media_disc_map *) diskmgr->mediainfo;
	sal_assert(Mdiscinfo);
	
	currentDisc = selected_disc;
	disc_list = Mdiscinfo->discinfo;
		
	if(currentDisc >= (unsigned int)Mdiscinfo->count)
	{
		sal_debug_println("BurnEndCurrentDisc : invalid burn->disc(%d) Mdiscinfo->count(%d)!!\n",
				currentDisc, Mdiscinfo->count);
		return 0;
	}
	
	if(currentDisc < 0){
		
		sal_debug_println("BurnEndCurrentDisc invalid disc (%d)\n", currentDisc);
	}

	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT){
		sal_debug_println("BurnEndCurrentDisc Error NEED FORMAT!!\n");
		return 0;
	}
	
	buf = (unsigned char *)sal_malloc(4096);
	if(!buf)
	{
		sal_debug_println("BurnEndCurrentDisc can't alloc ubuf1!\n");
		return 0;
	}
	sal_memset(buf, 0, 4096);


	buf2 = (unsigned char *)sal_malloc(4096);
	if(!buf2)
	{
		sal_debug_println("BurnEndCurrentDisc can't alloc ubuf2!\n");
		result = 0;
		goto error_out;
	}
	sal_memset(buf2, 0, 4096);

	if((result = _CheckUpdateDisk(sdev, 1, hostid )) == 0 )
	{
		sal_debug_println("BurnEndCurrentDisc Error _CheckUpdateDisk\n");
		result = 0;
		goto error_out;		
	}
	
	if(( result = _CheckUpdateSelectedDisk( sdev, 1, hostid,currentDisc)) == 0)
	{
		sal_debug_println("BurnEndCurrentDisc Error _CheckUpdateSelectedDisk\n");
		result = 0;
		goto error_out;
	}
debug_mediaOp(2,("enter BurnEndCurrentDisc 6\n"));
	if(!GetDiskMeta(sdev, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		sal_debug_println("BurnEndCurrentDisc Error _GetDiskMeta\n");
		result = 0;
		goto error_out;
	}

	SetDiskMetaInformation(sdev, (PON_DISK_META)buf, 1);		

	hdisk = (PON_DISK_META)(buf);


	if(!GetDiscMeta(sdev, &disc_list[currentDisc], buf2, DISC_META_SECTOR_COUNT))
	{
		sal_debug_println("BurnEndCurrentDisc Error _GetDiscMeta\n");
		result = 0;
		goto error_out;		
	}


	
	entry = (PON_DISC_ENTRY) buf2;

	if(entry->status != DISC_STATUS_WRITING) 
	{
		if(!_InvalidateDisc (sdev, buf, buf2, hostid))
		{
			sal_debug_println("BurnEndCurrentDisc Error InvalidateDisc\n");
			result = 0;
			goto error_out;
		}
		
		result = 0;
		goto error_out;	
	}


	
	// update disk and disc information
	debug_mediaOp(2,("enter BurnEndCurrentDisc 9\n"));	
	// disc title
	p =(unsigned char *) ( buf2 + 	DISC_TITLE_START * SECTOR_SIZE) ;
	sal_memset(p, 0, 512);
	sal_memcpy(p, info->title_name, 128);
	p[127] = '\0';
	p += 128;
	sal_memcpy(p, info->title_info, 128);
	p[127] = '\0';		
		// disc additional info
	p =(unsigned char *) ( buf2 + 	DISC_ADD_INFO_START * SECTOR_SIZE) ;
	sal_memset(p, 0, 512);
	sal_memcpy(p, info->additional_infomation, 128);
	p[127] = '\0';
		// disc key info
	p =(unsigned char *) ( buf2 + 	DISC_KEY_START* SECTOR_SIZE) ;
	sal_memset(p,0,512);
	sal_memcpy(p, info->key, 128);		
	p[127] = '\0';
	
	if(!_WriteAndLogEnd( sdev, hdisk,entry, hostid, currentDisc))
	{
		result = 0;
		goto error_out;	
	}
	
	debug_mediaOp(2,("enter BurnEndCurrentDisc 5\n"));
	SetDiskMetaInformation( sdev, (PON_DISK_META)buf, 1);
	SetDiscEntryInformation(sdev, (PON_DISC_ENTRY)buf2, 1);

	result =  1;
error_out:
	if(buf) sal_free(buf);		
	if(buf2) sal_free(buf2);
	return result;		
}

NDASUSER_API
int ndas_CheckDiscValidity(
				int slot, 
				unsigned int select_disc
				)
{
	unsigned int currentDisc;
	PON_DISC_ENTRY  entry = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct media_disc_map * Mdiscinfo = NULL;	
	unsigned char *buf = NULL;
	int result = 0;
	int hostid;

	PDISC_LIST	disc_list = NULL;
	udev_t * udev;
	logunit_t * sdev;
	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);
	
	udev = SDEV2UDEV(sdev);
	sal_assert(udev);

	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);

	hostid = diskmgr->hostid;
	
	Mdiscinfo = (struct media_disc_map *) diskmgr->mediainfo;
	sal_assert(diskmgr);
	
	disc_list = Mdiscinfo->discinfo;
	
	currentDisc = select_disc;	

	buf = (unsigned char *)sal_malloc(4096);
	if(!buf)
	{
		sal_debug_println("CheckDiscValidity can't alloc ubuf!\n");
		result = 0;
		goto error_out;
	}
	sal_memset(buf, 0, 4096);

	if(!GetDiscMeta(sdev, &disc_list[currentDisc], buf, DISC_META_SECTOR_COUNT))
	{
		sal_debug_println("DeleteCurrentDisc Error _GetDiscMeta\n");
		result = 0;
		goto error_out;		
	}
	
	entry = (PON_DISC_ENTRY) buf;

	if((entry->status != DISC_STATUS_VALID) && (entry->status != DISC_STATUS_VALID_END))
	{
		result = 0;
		goto error_out;
	}
	
	if(entry->status == DISC_STATUS_VALID)
	{
		int ret = 0;
#ifdef GTIME
		sal_msec tv;
		unsigned int sec;
		unsigned int savedsec ;
		tv = sal_time_msec();

		sec = tv;
		savedsec = (unsigned int)(entry->time);

		debug_mediaOp(2,("VALIDITY CHECK Set time (%d) : Current time (%ld)\n", savedsec, sec));
		if( (sec -savedsec) > 604800 )  // 7 days
		{
			sal_debug_println("NEED VALIDATION PROCESS (%d)!!!!\n",entry->status);
			result =  -1;
			goto error_out;
		}
#else
			
		ret = _CheckValidateCount(sdev, currentDisc);
		if(ret == 0){
			result = 0;
			goto error_out;
		}else if(ret == -1){
			
			sal_debug_println("NEED VALIDATION PROCESS (%d)!!!!\n",entry->status);
			result =  -1;
			goto error_out;
		}
#endif	
	}
			
	result= 1;
error_out:
	if(buf) sal_free(buf);
	return result;
}

NDASUSER_API
int ndas_DeleteCurrentDisc( 
				int slot, 
				unsigned int selected_disc
				)
{
	unsigned int currentDisc;
	int result;
	
	PON_DISK_META	hdisk;
	PON_DISC_ENTRY  entry;
	PDISC_LIST	disc_list = NULL;
	
	unsigned char *buf = NULL;
	unsigned char *buf2 = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct media_disc_map * Mdiscinfo = NULL;
	udev_t * udev = NULL;
	logunit_t * sdev = NULL;
	unsigned int hostid;
	
	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);
	
	udev = SDEV2UDEV(sdev);
	sal_assert(udev);
	
	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);

	hostid = diskmgr->hostid;
	
	Mdiscinfo = (struct media_disc_map *) diskmgr->mediainfo;
	sal_assert(Mdiscinfo);
	
	currentDisc = selected_disc;
	disc_list = Mdiscinfo->discinfo;

	debug_mediaOp(2,("enter DeleteCurrentDisc\n"));
	


	if(currentDisc >= (unsigned int)Mdiscinfo->count)
	{
		sal_debug_println("DeleteCurrentDisc : invalid currentDisc(%d) Mdiscinfo->count(%d)!!\n",
				currentDisc, Mdiscinfo->count);
		return 0;
	}
	
	if(currentDisc < 0){
		
		sal_debug_println("DeleteCurrentDisc invalid disc (%d)\n", currentDisc);
	}

	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT){
		sal_debug_println("DeleteCurrentDisc Error NEED FORMAT!!\n");
		return 0;
	}

	buf = (unsigned char *)sal_malloc(4096);
	if(!buf)
	{
		sal_debug_println("DeleteCurrentDisc can't alloc buf!\n");
		return 0;
	}
	sal_memset(buf,0, 4096);


	buf2 = (unsigned char *)sal_malloc(4096);
	if(!buf2)
	{
		sal_debug_println("DeleteCurrentDisc can't alloc buf2!\n");
		result = 0;
		goto error_out;
	}
	sal_memset(buf2, 0, 4096);
	
	// update Disk meta
	if((result = _CheckUpdateDisk(sdev, 1, hostid ))== 0 )
	{
		sal_debug_println("DeleteCurrentDisc Error _CheckUpdateDisk\n");
		result = 0;
		goto error_out;		
	}
	
	if(( result = _CheckUpdateSelectedDisk( sdev, 1, hostid,currentDisc)) == 0)
	{
		sal_debug_println("DeleteCurrentDisc Error _CheckUpdateSelectedDisk\n");
		result = 0;
		goto error_out;
	}

	if(!GetDiskMeta(sdev, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		sal_debug_println("DeleteCurrentDisc Error _GetDiskMeta\n");
		result = 0;
		goto error_out;
	}
		


	hdisk = (PON_DISK_META)(buf);
	

	if(!GetDiscMeta(sdev, &disc_list[currentDisc], buf2, DISC_META_SECTOR_COUNT))
	{
		sal_debug_println("DeleteCurrentDisc Error _GetDiscMeta\n");
		result = 0;
		goto error_out;		
	}
	
	entry = (PON_DISC_ENTRY) buf2;

	

	if(entry->status ==  DISC_STATUS_WRITING)
	{
		int ret = 0;
		ret = _CheckWriteFail(sdev, hdisk, entry, currentDisc, hostid);
		if(( ret == 0) || (ret == 1)){
			sal_debug_println("DeleteCurrentDisc Error _CheckWriteFail\n");
			result = 0;
			goto error_out;
		}
		
		if(!GetDiskMeta(sdev, buf, DISK_META_READ_SECTOR_COUNT)) 
		{
			sal_debug_println("DeleteCurrentDisc Error GetDiskMeta\n");
			result = 0;
			goto error_out ;
		}
	
		hdisk = (PON_DISK_META)(buf);
	

		if(!GetDiscMeta(sdev, &disc_list[currentDisc], buf2, DISC_META_SECTOR_COUNT))
		{
			sal_debug_println("DeleteCurrentDisc Error _GetDiscMeta\n");
			result = 0;
			goto error_out;		
		}
	
		entry = (PON_DISC_ENTRY) buf2;

		SetDiskMetaInformation( sdev, (PON_DISK_META)buf, 1);
		SetDiscEntryInformation(sdev, (PON_DISC_ENTRY)buf2, 1);

		result = 1;
		goto error_out;
	}


	if((entry->status == DISC_STATUS_VALID)
		|| (entry->status == DISC_STATUS_VALID_END))
	{
		int refcount = 0;
		refcount =  _GetDiscRefCount(sdev, currentDisc);
		if(refcount == -1){
			
			sal_debug_println("DeleteCurrentDisc Error _GetDiscRefCount\n");
			result = 0;
			goto error_out;				
		}		
		if(refcount  <= 1){

			if(!_CheckHostOwner( sdev, currentDisc, hostid))
			{
				sal_debug_println("DeleteCurrentDisc Is not ownde\n");
				result = 0;
				goto error_out;			
			}

			if( !_InvalidateDisc(sdev, buf, buf2, hostid) )
			{
				sal_debug_println("DeleteCurrentDisc Error InvalidateDisc\n");
				result = 0;
				goto error_out;				
			}
			
		}else {
	
			if(!_DeleteAndLog( sdev, hdisk, entry, hostid, currentDisc))
			{
				 
				sal_debug_println("DeleteCurrentDisc Error DeleteAndLog\n");
				result = 0;
				goto error_out;				
			}
		}
		SetDiskMetaInformation( sdev, (PON_DISK_META)buf, 1);
		SetDiscEntryInformation(sdev, (PON_DISC_ENTRY)buf2, 1);
	}

	result= 1;
error_out:
	if(buf) sal_free(buf);
	if(buf2) sal_free(buf2);
	return result;

}


NDASUSER_API
int ndas_GetCurrentDiskInfo(
			int slot,
			struct disk_add_info * DiskInfo
			)
{
	struct nd_diskmgr_struct * diskmgr = NULL;
	udev_t * udev = NULL;
	logunit_t * sdev = NULL;
	int max_discs = 0;
	int i = 0;
	
	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);

	udev = SDEV2UDEV(sdev);
	sal_assert(udev);
	
	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);

	for(i = 0; i < MAX_PART; i++)
	{
		DiskInfo->partinfo[i] = diskmgr->disc_part_map[i];
		sal_debug_println("ndas_GetCurrentDiskInfo : partinfo (%d) --> (%d)\n",i, diskmgr->disc_part_map[i]);
	}

	max_discs = diskmgr->mediainfo->count;
	if(max_discs > MAX_DISC_LIST) max_discs = MAX_DISC_LIST;
	
	for(i = 0; i < max_discs; i++)
	{
		DiskInfo->discinfo[i] = diskmgr->mediainfo->discinfo[i].valid;
		sal_debug_println("ndas_GetCurrentDiskInfo : discinfo (%d) --> (%d)\n",i, DiskInfo->discinfo[i]);
	}

	DiskInfo->count = max_discs;

	return 1;
}


NDASUSER_API
int ndas_GetCurrentDiscInfo( 
				int slot, 
				unsigned int selected_disc,
				struct disc_add_info	*  DiscInfo
				)
{
	unsigned char		*disk_buf = NULL;
	unsigned char 		*disc_buf = NULL;
	PON_DISC_ENTRY		DiscEntry;
	PDISC_LIST		disclist;
	unsigned char 			* source;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct media_disc_map * Mdiscinfo = NULL;
	int result = 0;
	udev_t * udev;
	logunit_t * sdev;
	
	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);

	udev = SDEV2UDEV(sdev);
	sal_assert(udev);
	
	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);
	
	Mdiscinfo = (struct media_disc_map *) diskmgr->mediainfo;
	sal_assert(Mdiscinfo);
	
	disclist = &Mdiscinfo->discinfo[selected_disc];
	
	debug_mediaOp(2,("enter GetCurrentDiscInfo\n"));


	if(selected_disc >= (unsigned int)Mdiscinfo->count)
	{
		sal_debug_println("GetCurrentDiscInfo : invalid selected_disc(%d) Mdiscinfo->count(%d)!!\n",
				selected_disc, Mdiscinfo->count);
		return 0;
	}
	
	if(selected_disc < 0){
		
		sal_debug_println("GetCurrentDiscInfo : invalid disc (%d)\n", selected_disc);
	}

	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT){
		sal_debug_println("GetCurrentDiscInfo : Error NEED FORMAT!!\n");
		return 0;
	}


	disk_buf = (unsigned char *)sal_malloc(4096);
	if(!disk_buf)
	{
		sal_debug_println("GetCurrentDiscInfo : can't alloc disk_buf!\n");
		return 0;
	}
	sal_memset(disk_buf, 0, 4096);

	disc_buf = (unsigned char *)sal_malloc(4096);
	if(!disc_buf)
	{
		sal_debug_println("GetCurrentCurrentDisc : can't alloc disc_buf!\n");
		result = 0;
		goto error_out;
	}
	sal_memset(disc_buf,0, 4096);


	debug_mediaOp(2,("GetCurrentDiscInfo %p selected_disc %d\n", diskmgr, selected_disc));
	


	if(!GetDiskMeta(sdev, disk_buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		sal_debug_println("GetCurrentDiscInfo  Error can't read DISK META!!\n");
		result= 0;
		goto error_out;
	}

	SetDiskMetaInformation( sdev, (PON_DISK_META)disk_buf, 1);


	if(!GetDiscMeta(sdev, disclist, disc_buf, DISC_META_SECTOR_COUNT))
	{
		sal_debug_println("GetCurrentDiscInfo  Error can't read DISC META!!\n");
		result= 0;
		goto error_out;
	}
	

		

	DiscEntry = (PON_DISC_ENTRY) disc_buf;
	
	if(!(DiscEntry->status & (DISC_STATUS_VALID | DISC_STATUS_VALID_END)))
	{
		sal_debug_println("GetCurrentDiscInfo Error : Invalid status (entry->status (%d)\n", DiscEntry->status);
		result = 1;
		goto error_out;
	}

	source = disc_buf + DISC_TITLE_START * SECTOR_SIZE;
	sal_memcpy(DiscInfo->title_name, source, 128);
	sal_memcpy(DiscInfo->title_info, source + 128, 128);
	source = disc_buf + DISC_ADD_INFO_START * SECTOR_SIZE;
	sal_memcpy(DiscInfo->additional_infomation, source,128);

	source = disc_buf + DISC_KEY_START* SECTOR_SIZE;
	sal_memcpy(DiscInfo->key, source, 128);

	result= 1;

error_out:
	if(disk_buf) sal_free(disk_buf);	
	if(disc_buf) sal_free(disc_buf);
	return result;	
		
}




/****************************************************************
*
*		disk structure
*		# Cluster size Parameter
*		| 8M (G meta) | 16k (Media meta)| ... | 16k| Media cluseter 
*					.... Media cluster | Ximeta Information block (2k)|
*
*		G meta : 		0				??
*		Media meta : 	8M 				(8M )		[4M(disk Info)][4M(disc info mirror)]
*		Data		    :	16M		 		(total - 18M)   
*		XimetaInfo   :  total - 2M		2M
****************************************************************/

NDASUSER_API
int ndas_DISK_FORMAT( int slot, unsigned long long sector_count)
{
	unsigned char 		*buf;
	PON_DISK_META		hdisk;
	unsigned long long		total_available_sector;
	unsigned int		total_available_cluster;
	unsigned int		i;
	unsigned int		max_discs;
	PNDAS_DIB_V2		DIBV2;
	struct nd_diskmgr_struct * diskmgr = NULL;
	int result = 0;
	udev_t * udev;
	logunit_t * sdev;
	
	sdev =sdev_lookup_byslot(slot);
	sal_assert(sdev);
	
	udev = SDEV2UDEV(sdev);
	sal_assert(udev);

	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);
	
	debug_mediaOp(2,("enter DISK_FORMAT\n"));


	if( sector_count  < (unsigned long long)( MEDIA_META_INFO_SECTOR_COUNT ) )
	{
		sal_debug_println("DISK FORMAT Error Sector size is too small \n");
		return 0;
	}


	buf = (unsigned char *)sal_malloc(4096);
	if(!buf){
		sal_debug_println("DISK FORMAT Error can't alloc buf\n");
		return 0;
	}	
	sal_memset(buf,0, 4096);


	
	max_discs = MEDIA_MAX_DISC_COUNT;
	total_available_sector = (sector_count  - MEDIA_META_INFO_SECTOR_COUNT) ;
	total_available_cluster = (unsigned int)((total_available_sector ) /MEDIA_CLUSTER_SIZE_SECTOR_COUNT);

	for(i = 0; i< max_discs; i++)
	{
		DISC_LIST list; 
		sal_memset(buf,0,DISC_META_SIZE);
		list.pt_loc = i*MEDIA_DISC_INFO_SIZE_SECTOR  + MEDIA_DISC_INFO_START_ADDR_SECTOR;
		InitDiscEntry(sdev, total_available_cluster, i, list.pt_loc, (PON_DISC_ENTRY)buf);
		if(!SetDiscMeta(sdev, &list, buf, DISC_META_SECTOR_COUNT))
		{
			sal_debug_println("DISK FORMAT Error SetDiscMeta\n");
			result = 0;
			goto error_out;		
		}

		if(!SetDiscMetaMirror(sdev, &list, buf, DISC_META_SECTOR_COUNT))
		{
			sal_debug_println("DISK FORMAT Error _SetDiscMetaMirror\n");
			result = 0;
			goto error_out;				
		}

		SetDiscEntryInformation(sdev, (PON_DISC_ENTRY)buf, 0);
		
	}


	// logdata setting

	for(i = 0; i < max_discs; i++)
	{
		unsigned int index = i;
		sal_memset(buf, 0, SECTOR_SIZE*DISK_LOG_DATA_SECTOR_COUNT);
		if(!SetDiskLogData(sdev, buf, index, DISK_LOG_DATA_READ_SECTOR_COUNT))
		{
			sal_debug_println("DISK FORMAT Error !_SetDisckLogData\n");
			result = 0;
			goto error_out;						
		}
	}

	// loghead
	sal_memset(buf,0, SECTOR_SIZE);
	if(!SetDiskLogHead(sdev, buf, DISK_LOG_HEAD_READ_SECTOR_COUNT))
	{
		sal_debug_println("DISK FORMAT Error SetDiscLogHead\n");
		result = 0;
		goto error_out;					
	}
	
	sal_memset(buf,0,SECTOR_SIZE*DISK_META_READ_SECTOR_COUNT);
	hdisk = (PON_DISK_META)buf;
	hdisk->MAGIC_NUMBER = MEDIA_DISK_MAGIC;
	hdisk->VERSION = MEDIA_DISK_VERSION;
	hdisk->age = 0;
	hdisk->nr_Enable_disc =0 ;
	hdisk->nr_DiscInfo = max_discs;
	hdisk->nr_DiscInfo_byte = (unsigned int)((hdisk->nr_DiscInfo + 7) / 8);
	hdisk->nr_DiscCluster = total_available_cluster ;
	hdisk->nr_DiscCluster_byte =(unsigned int)( (hdisk->nr_DiscCluster + 7)/ 8) ;
	hdisk->nr_AvailableCluster = hdisk->nr_DiscCluster;


	if(!SetDiskMeta(sdev, buf, DISK_META_WRITE_SECTOR_COUNT))
	{
		sal_debug_println("DISK FORMAT Error SetDiskMeta\n");
		result = 0;
		goto error_out;		
	}

	if(!SetDiskMetaMirror(sdev, buf, DISK_META_WRITE_SECTOR_COUNT))
	{
		sal_debug_println("DISK FORMAT Error 8\n");
		goto error_out;		
	}

	SetDiskMetaInformation( sdev, (PON_DISK_META)buf, 0);


	//add disk signal
	sal_memset(buf,0, SECTOR_SIZE);
	DIBV2 = (PNDAS_DIB_V2)(buf);
	sal_memcpy(DIBV2->u.s.Signature, NDAS_DIB_V2_SIGNATURE, 8);
        DIBV2->u.s.MajorVersion = NDAS_DIB_VERSION_MAJOR_V2;
        DIBV2->u.s.MinorVersion = NDAS_DIB_VERSION_MINOR_V2;
        DIBV2->u.s.nDiskCount = 1;
        DIBV2->u.s.iMediaType = NMT_MEDIAJUKE;
        DIBV2->crc32 = crc32_calc((unsigned char *)DIBV2, sizeof(DIBV2->u.bytes_248));
        
	if(RawDiskThreadOp(sdev,ND_CMD_WRITE,(sector_count + XIMETA_MEDIA_INFO_SECTOR_COUNT -2),1,buf) != NDAS_OK)
	{
		sal_debug_println("Error Writing signature \n");
		result = 0;
		goto error_out;		
	}

	// update vmap table information
	if(diskmgr->flags & DMGR_FLAG_NEED_FORMAT)
	{
		diskmgr->flags &= ~DMGR_FLAG_NEED_FORMAT;
		diskmgr->flags |= DMGR_FLAG_FORMATED;															
	}
	
	result = 1;
error_out:
	if(buf) sal_free(buf);
	return result;
}



NDASUSER_API
int ndas_ValidateDisc(
				int slot, 
				unsigned int selected_disc
				)
{
	PON_DISC_ENTRY		entry;
	unsigned char *		disk_buf = NULL;
	unsigned char *		disc_buf = NULL;
	PDISC_LIST		disclist = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct media_disc_map * Mdiscinfo = NULL;
	
	int result = 0;
	udev_t * udev;
	logunit_t * sdev;
	unsigned int hostid;
	
#ifdef GTIME
	sal_msec tv;
	unsigned int sec;
	unsigned int savedsec;
	tv = sal_time_msec();
	sec = tv;
#endif

	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);

	udev = SDEV2UDEV(sdev);
	sal_assert(udev);
	
	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);

	hostid = diskmgr->hostid;

	Mdiscinfo = (struct media_disc_map *) diskmgr->mediainfo;
	sal_assert(Mdiscinfo);
	

	if(selected_disc > (unsigned int)Mdiscinfo->count) 
	{
		sal_debug_println("ValidateDisc Error : selected_disc (%d) is too big\n", selected_disc);
		return 0;
	}
	
	disk_buf = (unsigned char *)sal_malloc(4096);
	if(!disk_buf){
		sal_debug_println("ValidateDisc  Error can't alloc disk_buf!!\n");
		return 0;
	}
	sal_memset(disk_buf, 0, 4096);


	disc_buf = (unsigned char *)sal_malloc(4096);
	if(!disc_buf){
		sal_debug_println("ValidateDisc  Error can't alloc disc_buf!!\n");
		result = 0;
		goto error_out;
	}
	sal_memset(disc_buf, 0, 4096);

	if(!GetDiskMeta(sdev, disk_buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		sal_debug_println("ValidateDisc  Error can't read DISK META!!\n");
		result = 0;
		goto error_out;
	}

	SetDiskMetaInformation( sdev, (PON_DISK_META)disk_buf, 1);

	disclist = &Mdiscinfo->discinfo[selected_disc];	
	
	if(!GetDiscMeta(sdev, disclist, disc_buf, DISC_META_SECTOR_COUNT))
	{
		sal_debug_println("ValidateDisc  Error can't read DISC META!!\n");
		result = 0;
		goto error_out;
	}

	entry = (PON_DISC_ENTRY) disc_buf;

	if(entry->status != DISC_STATUS_VALID)
	{
		sal_debug_println("ValidateDisc Error Not VALID DISC (%d)\n",entry->status);
		result = 0;
		goto error_out;
	}

	if(!_CheckHostOwner( sdev, selected_disc, hostid))
	{
		sal_debug_println("ValidateDisc Is not owned 8\n");
		result = 0;
		goto error_out;			
	}


#ifdef GTIME
	savedsec = (unsigned int)(entry->time );
	debug_mediaOp(2,("VALIDITY CHECK Set time (%d) : Current time (%ld)\n", savedsec, sec));
	if( (sec - savedsec) < 604800 )  // 7 days
	{
		sal_debug_println("TOO EARLY TRY VALIDATION  PROCESS !!!!\n");
		result = 0;
		goto error_out;
	}	
#else
	if(_CheckValidateCount(sdev, selected_disc) != -1)
	{
		sal_debug_println("TOO EARLY TRY VALIDATION PROCESS !!!!\n");
		result = 0;
		goto error_out;
	}	
#endif	
			
	
	entry->status = DISC_STATUS_VALID_END;
	
	if( ! SetDiscMeta(sdev, disclist, disc_buf, DISC_META_SECTOR_COUNT))
	{
		sal_debug_println("DeleteCurrentDisc Error 11\n");
		result = 0;
		goto error_out;
	}

	SetDiscEntryInformation(sdev, (PON_DISC_ENTRY)disc_buf, 1);
	result = 1;
error_out:
	if(disk_buf) sal_free(disk_buf);
	if(disc_buf) sal_free(disc_buf);
	return result;
}


int is_check_jukebox_format(int slot)
{
	PON_DISK_META hdisk = NULL;
	unsigned char *	buf = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	udev_t * udev = NULL;
	logunit_t * sdev = NULL;
	int result = 0;
	
	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);
	udev = SDEV2UDEV(sdev);
	sal_assert(udev);

	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);
	
	buf = (unsigned char *)sal_malloc(4096);
	if(!buf)
	{
		sal_debug_println("CheckFormat Error : can't alloc buf\n");
		return 0;	
	}

	sal_memset(buf,0,4096);

	debug_mediaOp(2,("enter CheckFormat---\n"));


		
	if(!GetDiskMeta(sdev, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		sal_debug_println("CheckFormat  Error can't read DISK META!!\n");
		result = 0;
		goto error_out;
	}

	hdisk = (PON_DISK_META)buf;
	if((hdisk->MAGIC_NUMBER != MEDIA_DISK_MAGIC)
			|| (hdisk->VERSION != MEDIA_DISK_VERSION))
	{
		debug_mediaOp(2,("DMGR_FLAG_NEED_FORMAT\n"));
		diskmgr->flags &= ~DMGR_FLAG_FORMATED;
		diskmgr->flags |= DMGR_FLAG_NEED_FORMAT;
		result = 0;
	}else{
		debug_mediaOp(2,("DMGR_FLAG_FORMATED\n"));
		diskmgr->flags &= ~DMGR_FLAG_NEED_FORMAT;
		diskmgr->flags |= DMGR_FLAG_FORMATED;
		result = 1;
	}
		
error_out:
	sal_free(buf);
	debug_mediaOp(2,("---end CheckFormat\n"));
	return result;	
}


int MakeMetaKey(struct nd_diskmgr_struct * diskmgr)
{
#ifdef META_ENCRYPT
		unsigned long long Password = METAKEY;
		unsigned char Key[32];
		struct media_key * MKey;
		sal_memset(Key,0,32);
		sal_memcpy(Key,METASTRING,16);

		MKey = (struct media_key *)diskmgr->MetaKey;	
		sal_memset(MKey, 0, sizeof(struct media_key));
		if(!CreateCipher(
				(PNCIPHER_INSTANCE)MKey->cipherInstance,	
				NDAS_CIPHER_AES,
				NCIPHER_MODE_ECB,
				HASH_KEY_LENGTH,
				(unsigned char *)&Password
				)
		)
		{
			debug_mediaOp(0,( "can't set Keyinstance  disk->MetaKey\n"));
			return 0;
		
		}

		if(!CreateCipherKey(
				(PNCIPHER_KEY)MKey->cipherKey,
				(PNCIPHER_INSTANCE)MKey->cipherInstance,	
				NDAS_CIPHER_AES_LENGTH,
				Key
				)
		)
			{
			debug_mediaOp(0,( "can't set Key disk->MetaKey\n"));
			return 0;
		}
#endif
	return 1;
}



NDASUSER_API
int ndas_CheckFormat(int slot)
{
	PON_DISK_META hdisk = NULL;
	unsigned char *	buf = NULL;
	struct nd_diskmgr_struct * diskmgr = NULL;
	udev_t * udev = NULL;
	logunit_t * sdev = NULL;
	int ret = 0;

	sdev = sdev_lookup_byslot(slot);
	sal_assert(sdev);

	if((SDEV2UDEV(sdev))->info->mode != NDAS_DISK_MODE_MEDIAJUKE)
	{
	       diskmgr->flags &= ~DMGR_FLAG_FORMATED;
		diskmgr->flags |= DMGR_FLAG_NEED_FORMAT;
		ret = -1;
		goto error_out;
	}
	
	udev = SDEV2UDEV(sdev);
	sal_assert(udev);

	diskmgr = udev->Disk_info;
	sal_assert(diskmgr);


	
	buf = (unsigned char *)sal_malloc(4096);
	if(!buf)
	{
		sal_debug_println("CheckFormat Error : can't alloc buf\n");
		ret = -1;
		goto error_out;	
	}

	sal_memset(buf,0,4096);

	debug_mediaOp(2,("enter CheckFormat---\n"));


	
	if(!GetDiskMeta(sdev, buf, DISK_META_READ_SECTOR_COUNT)) 
	{
		sal_debug_println("CheckFormat  Error can't read DISK META!!\n");
		ret = -1;
		goto error_out;
	}

	hdisk = (PON_DISK_META)buf;
	if((hdisk->MAGIC_NUMBER != MEDIA_DISK_MAGIC)
			|| (hdisk->VERSION != MEDIA_DISK_VERSION))
	{
		debug_mediaOp(2,("DMGR_FLAG_NEED_FORMAT\n"));
		diskmgr->flags &= ~DMGR_FLAG_FORMATED;
		diskmgr->flags |= DMGR_FLAG_NEED_FORMAT;
	}else{
		debug_mediaOp(2,("DMGR_FLAG_FORMATED\n"));
		diskmgr->flags &= ~DMGR_FLAG_NEED_FORMAT;
		diskmgr->flags |= DMGR_FLAG_FORMATED;
	
		if(!MakeView(slot))
	   	{
	        	sal_debug_println("NDAS_ERROR_JUKE_DISK_NOT_FORMATED");
	        	diskmgr->flags &= ~DMGR_FLAG_FORMATED;
			diskmgr->flags |= DMGR_FLAG_NEED_FORMAT;
	   	}
	   	
	}
	
error_out:
	if(buf) sal_free(buf);
	
	debug_mediaOp(2,("---end CheckFormat\n"));
	return ret;
}
#endif //#ifdef XPLAT_JUKEBOX
