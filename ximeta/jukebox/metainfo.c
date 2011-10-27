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
#include "mediajukedisk.h"
#include <jukebox/diskmgr.h>
#include "metaop.h"
#include "metainfo.h"

#define DEBUG_LEVEL_MSHARE_METAINFO  5

#define    debug_metainfo(l, x)    do {\
    if(l <= DEBUG_LEVEL_MSHARE_METAINFO) {     \
        sal_debug_print("UD|%d|%s|",l,__FUNCTION__); \
        sal_debug_println x;    \
    } \
} while(0)    

/********************************************************************

	Disc Meta Manager function

*********************************************************************/
int findFreeMapCount(unsigned int bitmap_count, PBITMAP bitmap)
{
	unsigned int i;
	unsigned int count = 0;

	sal_assert(bitmap);
	for(i = 0; i< bitmap_count; i++)
	{
		if(!test_bit(i, bitmap) )
		{
			count ++;
		}
	}

	return count;
}


int findDirtyMapCount(unsigned int bitmap_count, PBITMAP bitmap)
{
	unsigned int i;
	unsigned int count = 0;

	sal_assert(bitmap);
	for(i = 0; i< bitmap_count; i++)
	{
		if(test_bit(i, bitmap) )
		{
			count ++;
		}
	}

	return count;
}





void setDiscAddr_diskFreeMap(unsigned int bitmap_count, unsigned int request_alloc, PBITMAP diskfreemap, PON_DISC_ENTRY entry)
{
	unsigned int i;
	unsigned int alloc = 0;
	sal_assert(diskfreemap);
	sal_assert(entry);
	
	for( i = 0; i < bitmap_count; i++)
	{
		if(alloc >= request_alloc) break;
		
		if(!test_bit(i, diskfreemap))
		{
			test_and_set_bit(i, diskfreemap);
			entry->Addr[alloc] = (unsigned short) i;
			alloc++;
		}
	}	
}


void setDiscAddrtoLogData(PON_DISC_ENTRY entry, PON_DISK_LOG logdata)
{
	unsigned int i = 0;
	sal_assert(logdata);
	
	logdata->nr_DiscCluster = entry->nr_DiscCluster;

	for(i = 0; i <entry->nr_DiscCluster ; i++)
	{
		logdata->Addr[i] = entry->Addr[i];	
	}
}


void freeClusterMap (unsigned int index , PBITMAP DiscMap)
{	
	sal_assert(DiscMap);
	test_and_clear_bit(index, DiscMap);

	return;
}


void setClusterMap (unsigned int index, PBITMAP DiscMap)
{
	sal_assert(DiscMap);
	test_and_set_bit(index, DiscMap);

	return;
}


void setClusterMapFromEntry(PBITMAP clustermap, PON_DISC_ENTRY entry)
{
	unsigned int i;
	sal_assert(clustermap);
	sal_assert(entry);
	
	for(i = 0; i < entry->nr_DiscCluster; i++)
	{
		setClusterMap(entry->Addr[i], clustermap);
	}
}

void freeClusterMapFromEntry(PBITMAP clustermap, PON_DISC_ENTRY entry)
{
	unsigned int i;
	sal_assert(clustermap);
	sal_assert(entry);
	
	for(i = 0; i < entry->nr_DiscCluster; i++)
	{
		freeClusterMap(entry->Addr[i], clustermap);
	}
}


void setAllClusterMapFromEntry(PBITMAP freeclustermap, PBITMAP dirtyclustermap, PON_DISC_ENTRY entry)
{
	unsigned int i;
	sal_assert(freeclustermap);
	sal_assert(dirtyclustermap);
	sal_assert(entry);
	
	for(i = 0; i < entry->nr_DiscCluster; i++)
	{
		setClusterMap(entry->Addr[i], freeclustermap);
		setClusterMap(entry->Addr[i], dirtyclustermap);
	}
}

void freeAllClusterMapFromEntry(PBITMAP freeclustermap, PBITMAP dirtyclustermap, PON_DISC_ENTRY entry)
{
	unsigned int i;
	sal_assert(freeclustermap);
	sal_assert(dirtyclustermap);
	sal_assert(entry);
	
	for(i = 0; i < entry->nr_DiscCluster; i++)
	{
		freeClusterMap(entry->Addr[i], freeclustermap);
		freeClusterMap(entry->Addr[i], dirtyclustermap);
	}
}



void freeDiscMapbyLogData(PBITMAP freeMap, PBITMAP dirtyMap, PON_DISK_LOG logdata)
{
	unsigned int i = 0;
	sal_assert(freeMap);
	sal_assert(dirtyMap);
	sal_assert(logdata);
	
	for(i = 0; i < logdata->nr_DiscCluster; i++)
	{
		test_and_clear_bit(logdata->Addr[i], freeMap);
		test_and_clear_bit(logdata->Addr[i], dirtyMap);
	}
}

void freeDiscInfoMap (unsigned int index , PBITMAP DiscMap)
{	
	sal_assert(DiscMap);
	test_and_clear_bit(index, DiscMap);

	return;
}


void setDiscInfoMap (unsigned int index, PBITMAP DiscMap)
{
	sal_assert(DiscMap);
	test_and_set_bit(index, DiscMap);

	return;
}

/*
typedef struct  _ON_DISK_META
{
	unsigned int	MAGIC_NUMBER;					
	unsigned int	VERSION;								// offset 4
	unsigned int	age;									// offset 8
	unsigned int	nr_Enable_disc;							// offset 12
	unsigned int	nr_DiscInfo;								// offset 16
	unsigned int	nr_DiscInfo_byte;						// offset 20
	unsigned int	nr_DiscCluster;							// offset 24
	unsigned int	nr_AvailableCluster;						// offset 28
	unsigned int	nr_DiscCluster_byte;						// offset 32
	unsigned int	RESERVED1[23];							// offset 36
	unsigned int	RESERVED2[16];							// offset 128
	unsigned char	DiscInfo_bitmap[64];						// offset 192
	unsigned int	RESERVED3[16];							// offset 256
	unsigned char FreeCluster_bitmap[64];					// offset 320
	unsigned int	RESERVED4[16];							// offset 384
	unsigned char DirtyCluster_bitmap[64];					// offset 448
}ON_DISK_META, *PON_DISK_META;


typedef struct _ON_DISK_LOG_HEADER
{
	unsigned int latest_age;		
	unsigned int latest_log_action;		// offset	4
	unsigned int latest_log_host;		// offset 	8
	unsigned int latest_history;		// offset 	12
	DISK_HISTORY	history[62];		// offset 	16
}ON_DISK_LOG_HEADER, *PON_DISK_LOG_HEADER;
*/



void SetDiskMetaInformation(logunit_t * sdev, PON_DISK_META buf, unsigned int update)
{

	int		discs;
	int 		disc_info_count;
	int 		i;

	PBITMAP		bitmap;
	PON_DISK_META	hdisk;
	struct 		nd_diskmgr_struct * diskmgr = NULL;
	struct 		media_disc_map* Mdiscinfo = NULL;
	udev_t *	udev;
	udev = SDEV2UDEV(sdev);
	sal_assert(udev);
	diskmgr = udev->Disk_info;
	sal_assert(diskmgr != NULL);
	
	debug_metainfo(2,("enter SetDiskMetaInformation\n"));
	sal_assert(buf);
	
	hdisk = (PON_DISK_META)buf;
	hdisk->nr_AvailableCluster = findFreeMapCount(hdisk->nr_DiscCluster, hdisk->FreeCluster_bitmap);
	hdisk->nr_Enable_disc = findDirtyMapCount(hdisk->nr_DiscInfo, hdisk->DiscInfo_bitmap);
	debug_metainfo(2,(" Enable disc (%d) Avaiable Cluster(%d)\n", hdisk->nr_Enable_disc, hdisk->nr_AvailableCluster));
	discs = hdisk->nr_Enable_disc + (hdisk->nr_AvailableCluster/DEFAULT_DISC_CLUSTER_COUNT);
	
	if(discs > MAX_DISC_LIST) discs = MAX_DISC_LIST;
	disc_info_count = hdisk->nr_DiscInfo;
	

	Mdiscinfo = (struct media_disc_map *)diskmgr->mediainfo;
	sal_assert(Mdiscinfo != NULL);

	bitmap = (PBITMAP)(hdisk->DiscInfo_bitmap);
	Mdiscinfo->count = discs;

	disc_info_count = MAX_DISC_LIST;

	debug_metainfo(2,("DISC INFO : ENABLED_DISC (%d) AVAIABLE_DISC(%d), TOTAL_DISCS_INFO(%d)\n",
		hdisk->nr_Enable_disc, discs, disc_info_count)); 

	
	for( i = 0 ; i < disc_info_count; i++)
	{
		if(test_bit(i,bitmap))
		{
			debug_metainfo(2,("ASSIGNED DISC (%d)\n", i));
			Mdiscinfo->discinfo[i].valid &= ~DISC_LIST_EMPTY;
			Mdiscinfo->discinfo[i].valid |= DISC_LIST_ASSIGNED;
			
		}else{
			Mdiscinfo->discinfo[i].valid &= ~DISC_LIST_ASSIGNED;
			Mdiscinfo->discinfo[i].valid |= DISC_LIST_EMPTY;
		}
		Mdiscinfo->discinfo[i].pt_loc = i*MEDIA_DISC_INFO_SIZE_SECTOR +  MEDIA_DISC_INFO_START_ADDR_SECTOR; 
		if(Mdiscinfo->discinfo[i].valid & DISC_LIST_ASSIGNED)
			debug_metainfo(2,("ASSIGNED  Disc(%d) Loc (%d)\n", i, Mdiscinfo->discinfo[i].pt_loc));
	}

}

int SetDiscAddr(logunit_t *sdev, struct media_addrmap * amap, int disc)
{
	unsigned char * buf = NULL;
	struct nd_diskmgr_struct *diskmgr = NULL;
	struct media_disc_map * Mdiscinfo = NULL;
	udev_t * udev;
	
	sal_assert(amap);
	udev = SDEV2UDEV(sdev);
	sal_assert(udev);
	
	diskmgr = udev->Disk_info;
	sal_assert(diskmgr != NULL);

	buf = (unsigned char *)sal_malloc(4096);
	if(!buf) {
		debug_metainfo(2,("ERROR get buff\n"));
		return -1;
	}

	sal_memset(buf, 0, 4096);
	
	Mdiscinfo = (struct media_disc_map *)diskmgr->mediainfo;
	sal_assert(Mdiscinfo != NULL);
	
	if(! GetDiscMeta(sdev, &Mdiscinfo->discinfo[disc], buf, DISC_META_SECTOR_COUNT))
	{
		debug_metainfo(2,("ERROR _GetDiscMeta\n"));
		sal_free(buf);
		return -1;
	}
	
	AllocAddrEntities(amap, (PON_DISC_ENTRY)buf);
	sal_free(buf);
	return 0;	
		
}

void AllocAddrEntities(struct media_addrmap * amap, PON_DISC_ENTRY entry)
{
	int i;
	
	int lo_start_sec;
	int start_sec;
	unsigned int sector_count;
	unsigned int index = 0;
	unsigned int total_sector_count = 0;
	unsigned short next_addr;

	sal_assert(amap);
	sal_assert(entry);
	
	sector_count = 0;
	start_sec = 0;
	lo_start_sec = 0;

	next_addr = entry->Addr[0];
	lo_start_sec = total_sector_count;
	start_sec = entry->Addr[0] * MEDIA_CLUSTER_SIZE_SECTOR_COUNT + MEDIA_DISC_DATA_START_SECTOR;
	
	debug_metainfo(2,(" AllocAddrEntities\n"));

	for( i = 0 ; (unsigned int)i < entry->nr_DiscCluster; i++)
	{
		
		if(next_addr == entry->Addr[i]){
			sector_count +=  MEDIA_CLUSTER_SIZE_SECTOR_COUNT;
		}else{
			debug_metainfo(2,("ADDR MAP INDEX(%d), LG_S_ADDR(%d) S_ADDR(%d) NR_SEC(%d)\n", index, lo_start_sec,start_sec, sector_count));
			amap->addrMap[index].StartSector = start_sec;
			amap->addrMap[index].nr_SecCount = sector_count;
			amap->addrMap[index].Lg_StartSector = lo_start_sec;
			total_sector_count+= sector_count;

			lo_start_sec = total_sector_count;
			start_sec =entry->Addr[i] *  MEDIA_CLUSTER_SIZE_SECTOR_COUNT + MEDIA_DISC_DATA_START_SECTOR;
			sector_count = MEDIA_CLUSTER_SIZE_SECTOR_COUNT;
			index ++;
			
		}
		
		next_addr = entry->Addr[i] + 1;
		debug_metainfo(2,("ENTRY->ADDR[%d] : %d\n", i, entry->Addr[i]));	

	}

	debug_metainfo(2,("ADDR MAP INDEX(%d), LG_S_ADDR(%d) S_ADDR(%d) NR_SEC(%d)\n", index, lo_start_sec,start_sec, sector_count));

	amap->addrMap[index].StartSector = start_sec;
	amap->addrMap[index].nr_SecCount = sector_count;
	amap->addrMap[index].Lg_StartSector = lo_start_sec;
	
	total_sector_count+= sector_count;
	index++;	
	amap->count = index;
//	amap->disc = entry->index;
}




void InitDiscEntry(
	logunit_t* sdev, 
	unsigned int cluster_size, 
	unsigned int index, 
	unsigned int loc, 
	PON_DISC_ENTRY buf)
{
	PON_DISC_ENTRY	 entry;

	sal_assert(buf);
	debug_metainfo(2,("enter _InitDiscEntry---\n"));
	entry = (PON_DISC_ENTRY) buf;
	
	sal_memset(buf, 0, DISC_META_SECTOR_COUNT* 512);
	entry->MAGIC_NUMBER = MEDIA_DISK_MAGIC;
	entry->index = index;
	entry->loc = loc;
	entry->status = DISC_STATUS_INVALID;
	entry->age = 0;
	entry->time = 0;
	entry->action = ACTION_DEFAULT;
	entry->nr_DiscSector = 0;
	entry->nr_DiscCluster = 0;
	entry->nr_DiscCluster_bitmap = cluster_size;
	entry->pt_title = loc + DISC_TITLE_START;
	entry->pt_information = loc + DISC_ADD_INFO_START;
	entry->pt_key = loc + DISC_KEY_START;
	debug_metainfo(2,("---end _InitDiscEntry\n"));
}


void SetDiscEntryInformation (logunit_t * sdev, PON_DISC_ENTRY buf, unsigned int update)
{
	PON_DISC_ENTRY	 entry;
	struct nd_diskmgr_struct * diskmgr = NULL;
	struct 		media_disc_map * Mdiscinfo = NULL;
	udev_t * udev = NULL;
	int		index;
	debug_metainfo(2,("enter SetDiscEntryInformation---\n"));

	udev = SDEV2UDEV(sdev);
	sal_assert(udev);
	diskmgr = udev->Disk_info;
	sal_assert(diskmgr != NULL);
	
	entry = (PON_DISC_ENTRY)buf;
	sal_assert(buf != NULL);
	
	index = entry->index;
	debug_metainfo(2,("index of entry %ld\n", index));
	
	Mdiscinfo = (struct media_disc_map *)diskmgr->mediainfo;
	sal_assert(Mdiscinfo);
	
	if((index > Mdiscinfo->count) || (index < 0)) {
		debug_metainfo(2,(" index(%d) : entry->status(0x%x): entry->enrypt(0x%x) : entry->nr_DiscSector (%d)\n",
			index,	entry->status, entry->encrypt, entry->nr_DiscSector));
		debug_metainfo(2,("Invalid index of entry %ld\n", index));
		debug_metainfo(2,("Start Address entry %p\n", entry));
		debug_metainfo(2,("Start Address entry->index %p\n", &entry->index));
		debug_metainfo(2,("Start Address entry->status %p\n", &entry->status));
		debug_metainfo(2,("Start Address entry->encrypt %p\n", &entry->encrypt));
		return;
	}
	Mdiscinfo->discinfo[index].valid = ((Mdiscinfo->discinfo[index].valid & 0x000f0000) | entry->status);
	Mdiscinfo->discinfo[index].encrypt = entry->encrypt;
	Mdiscinfo->discinfo[index].sector_count = entry->nr_DiscSector;
		
	debug_metainfo(2,(" index(%d) : entry->status(0x%x): entry->enrypt(0x%x) : entry->nr_DiscSector (%d)\n",
			index,	entry->status, entry->encrypt, entry->nr_DiscSector));
/*
	if((entry->encrypt == 1) & (entry->index < MAX_HINT))
	{
		EHINT * HEnc;
//		int i;
//		unsigned char * buf;

		Mencrypt = (struct media_encryptID *)diskmgr->encryptInfo;
		HEnc = &Mencrypt->hint[index];
		sal_memset(HEnc->_data, 0 , 32);
		sal_memcpy(HEnc->_data, entry->HINT, 32);
//
//		buf = (unsigned char *)HEnc->_data;

//		printk("Disk is %d\n",index);
//		for(i = 0; i< 2; i++)
//		{
//			printk("0x%p:0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
//				&buf[i*16],buf[i*16],buf[i*16 +1],buf[i*16 +2],buf[i*16 +3],buf[i*16 +4],buf[i*16 +5],buf[i*16 +6],buf[i*16 +7],
//				buf[i*16 +8],buf[i*16 +9],buf[i*16 +10],buf[i*16 +11],buf[i*16 +12],buf[i*16 +13],buf[i*16 +14],buf[i*16 +15]);
//		}
//
	}
*/
	debug_metainfo(2,("---end SetDiscEntryInformation\n"));
}

#endif //#ifdef XPLAT_JUKEBOX
