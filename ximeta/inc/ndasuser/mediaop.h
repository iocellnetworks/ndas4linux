#ifndef _NDAS_MEDIAOP_H_
#define _NDAS_MEDIAOP_H_

#include <ndasuser/def.h>

#define DISC_STATUS_ERASING			0x00000001
#define DISC_STATUS_WRITING			0x00000002
#define DISC_STATUS_VALID				0x00000004
#define DISC_STATUS_VALID_END			0x00000008
#define DISC_STATUS_INVALID			0x00000000


struct 	disc_add_info
{
	unsigned char		title_name[NDAS_MAX_NAME_LENGTH];
	unsigned char		additional_infomation[NDAS_MAX_NAME_LENGTH];
	unsigned char		key[NDAS_MAX_NAME_LENGTH];
	unsigned char		title_info[NDAS_MAX_NAME_LENGTH];
};

#define MAX_PART 8
#define MAX_INFO 120

struct disk_add_info {
    short partinfo[MAX_PART];
    int     discinfo[MAX_INFO];
    int     count;
};


NDASUSER_API
int ndas_deallocate_part_id(int slot, unsigned int partid);

NDASUSER_API
int ndas_allocate_part_id(int slot, int selected_disc);

NDASUSER_API
int ndas_TranslateAddr(
				int slot,
				unsigned int partnum,
				unsigned int req_startsec, 
				unsigned int req_sectors, 
				unsigned int *start_sec, 
				unsigned int *sectors
				);

NDASUSER_API
int ndas_IsDiscSet(int slot, int partnum);

NDASUSER_API
int ndas_GetDiscStatus(int slot, int partnum);

struct Disc;

NDASUSER_API
void ndas_freeDisc(struct Disc * disc);

NDASUSER_API
void * ndas_getkey(int slot, int partnum);

NDASUSER_API
void ndas_encrypt(void * key, char * inbuf, char * outbuf, unsigned int size);

NDASUSER_API
void ndas_decrypt(void * key, char * inbuf,  char * outbuf, unsigned int size);

NDASUSER_API
int ndas_ATAPIRequest(
	int slot,
	int partnum,
	unsigned short dev, 
	int command, 
	int start_sector, 
	int nr_sectors, 
	char * buffer
	);

NDASUSER_API
int ndas_BurnStartCurrentDisc ( 
				int slot,
				unsigned int	Size_sector,
				unsigned int	selected_disc,
				unsigned int	encrypted,
				unsigned char * HINT
						  );

NDASUSER_API
int ndas_BurnEndCurrentDisc( 
			int slot,
			unsigned int  selected_disc, 
			struct disc_add_info	* info
			);

NDASUSER_API
int ndas_CheckDiscValidity(
				int slot, 
				unsigned int select_disc
				);


NDASUSER_API
int ndas_DeleteCurrentDisc( 
				int slot, 
				unsigned int selected_disc
				);


NDASUSER_API
int ndas_GetCurrentDiskInfo(
			int slot,
			struct disk_add_info * DiskInfo
			);
			
NDASUSER_API
int ndas_GetCurrentDiscInfo( 
				int slot, 
				unsigned int selected_disc,
				struct disc_add_info	*  DiscInfo
				);


NDASUSER_API
int ndas_DISK_FORMAT( int slot, unsigned long long sector_count);

NDASUSER_API
int ndas_ValidateDisc(
				int slot, 
				unsigned int selected_disc
				);

NDASUSER_API
int ndas_CheckFormat(int slot);





#endif //_NDAS_MEDIAOP_H_



