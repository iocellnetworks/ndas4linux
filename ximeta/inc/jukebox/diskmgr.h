#ifndef __DISK_MGR_H__
#define __DISK_MGR_H__
#ifdef XPLAT_JUKEBOX

#define MAX_ADDR_ALLOC_COUNT	40
#define MAX_PART		8
#define	MAX_DISC_LIST				200

struct media_key{
	char cipherInstance[1024];		// count 1024
	char cipherKey[1024];			// count 2048
} __attribute__((packed));

typedef struct _DISC_MAP_LIST
{
	unsigned int	StartSector;
	unsigned int	nr_SecCount;
	unsigned int	Lg_StartSector;
}__attribute__((packed)) DISC_MAP_LIST, *PDISC_MAP_LIST;

struct media_addrmap{	
	unsigned int count;					// count 4
	unsigned int ref;					// count 8
	DISC_MAP_LIST	addrMap[MAX_ADDR_ALLOC_COUNT];		// count 492
} __attribute__((packed)) ;

struct media_disc_info
{
	unsigned int		status;				// count 4
	unsigned int		pt_loc;				// count 8
	unsigned int		nr_clusters;			// count 12	
	unsigned int		nr_sectors;			// count 16
	unsigned int		encrypt;			// count 20
	struct media_addrmap 	address;			// count 512
} __attribute__((packed)) ;

struct Disc {
	unsigned short			dev_num;
	unsigned short			disc_num;	
	struct media_key * 		disc_key;
	struct media_disc_info * 	disc_info;
} __attribute__((packed));

typedef struct _DISC_LIST
{
	unsigned int	valid;
	unsigned int	pt_loc;
	unsigned int	sector_count;
	unsigned short	encrypt;
	short			minor;
}__attribute__((packed)) DISC_LIST, *PDISC_LIST;


struct media_disc_map {
	int count;
	DISC_LIST	discinfo[MAX_DISC_LIST];		//16*200
} __attribute__((packed));

#define DMGR_FLAG_FORMATED		0x00000100	//	formated for media juke
#define DMGR_FLAG_NEED_FORMAT	0x00000200	//	need format




struct nd_diskmgr_struct  {
	unsigned short			slot_number;
	unsigned short			dev_num;
	unsigned int				hostid;
	unsigned int				flags;
	short					disc_part_map[MAX_PART];	
	// media juke box support structure
	struct media_key * 		MetaKey;
	struct media_disc_map * 	mediainfo;
	// parallel operation support to noly NR_PARTITION;
	unsigned int 				nr_AllocDiscs;
	struct Disc *				Disc_info[MAX_PART];
} __attribute__((packed)) ;


int is_check_jukebox_format(int slot);
int MakeMetaKey(struct nd_diskmgr_struct * diskmgr);
int MakeView(int slot);

#endif //#ifdef XPLAT_JUKEBOX
#endif //#ifndef __DISK_MGR_H__
