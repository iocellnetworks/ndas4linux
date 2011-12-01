#ifndef __NDAS_HELPER_H__
#define __NDAS_HELPER_H__

#define ND_NDAS_ID_LENGTH 				20
#define ND_NDAS_KEY_LENGTH  			5
#define ND_NDAS_MAX_NAME_LENGTH 		128

#define NDAS_TYPE_MEDIAJUKE 1
#define NDAS_TYPE_DISK 0

typedef struct _FIRST_L_DISK{
	int Index;
	char DiskStr[ND_NDAS_ID_LENGTH + 1];
	char Key[ND_NDAS_KEY_LENGTH  +1];
}FIRST_L_DISK, *PFIRST_L_DISK;

typedef struct _FIRST_L_DISK_LIST{
	unsigned int Count;
	FIRST_L_DISK FDISKS[1];	
}FIRST_L_DISK_LIST,*PFIRST_L_DISK_LIST;

typedef struct _ENABLE {
	char DiskStr[ND_NDAS_ID_LENGTH + 1];
	char Key[ND_NDAS_KEY_LENGTH  +1];
	char Name[ND_NDAS_MAX_NAME_LENGTH];
}ENABLE_T, *ENABLE_PTR;

typedef struct _VDVD_DISC{
	unsigned char selected;
	signed char partnum;
	unsigned char slot;
	unsigned char Disc;
	int index;
	int Status;
	char * DiscStr;
}VDVD_DISC, *PVDVD_DISC;

typedef struct _VDVD_DISC_LIST{
	unsigned int Count;
	VDVD_DISC	VDiscs[1];
}VDVD_DISC_LIST, *PVDVD_DISC_LIST;



typedef struct _ENABLED_DISK{
	char DiskStr[ND_NDAS_ID_LENGTH + 1];
	char Key[ND_NDAS_KEY_LENGTH + 1];
	char Name[ND_NDAS_MAX_NAME_LENGTH];
	int DiskType;
	int slot;
	PVDVD_DISC_LIST	VDiscList;
}ENABLED_DISK, * PENABLED_DISK;


typedef struct _NDAS_DISK{
	_NDAS_DISK * next;
	char DiskStr[ND_NDAS_ID_LENGTH + 1];		
}NDAS_DISK_T, *NDAS_DISK_PTR;


typedef struct _NDAS_DISK_LIST{
	int count;
	NDAS_DISK_PTR head;
}NDAS_DISK_LIST_T, * NDAS_DISK_LIST_PTR;

#define NDAS_HELPER_STATUS_INIT		0x00000000
#define NDAS_HELPER_STATUS_FDISK_SET	0x00000001
#define NDAS_HELPER_STATUS_DISK_ENABLED 0x00000002
#define NDAS_HELPER_STATUS_PANIC	0x00000008

#define DISC_STATUS_ERASING			0x00000001
#define DISC_STATUS_WRITING			0x00000002
#define DISC_STATUS_VALID			0x00000004
#define DISC_STATUS_VALID_END		0x00000008

class NDASHelper
{
// member function
public:
	NDASHelper(char * DVDDevString);
	~NDASHelper();

	int EnableDisk(ENABLE_PTR enable_info, int * Diskslot);
	int DisableDisk();
	int GetFirstLevelDiskList(PFIRST_L_DISK_LIST *ppFDiskList);
	int GetDVDList(PVDVD_DISC_LIST *ppDVDList);
	int SetDVDStr(PVDVD_DISC_LIST pDVDList , int slot);
	int SelectDisc(int disc , int *partnum);
	int DeselectDisc(int disc , int partnum);
	int GetDiskType(int index);

	char *GetRootPath() { return m_rootpath; }
	
#ifdef NDAS_MSHARE_WRITE
	int MFormat(int slot);
	int MCopy(int disc);
	int MValidate(int disc);
	int MDel(int disc);
#endif	
private:
	void recheck_list(NDAS_DISK_LIST_PTR ndas_disk_list, int nr_disk, char * ndasids);
	void freelist(NDAS_DISK_LIST_PTR ndas_disk_list);
	void MakeNode(int slot);
	int MountXiDisk(int slot);
	int UnMountXiDisk();

	int MountPartition(char *devname, char *mountpoint);
	int MapIdxToPartition(int idx_mounted);
	const char *GetMountedPathTarget(int mount_point);
	int GetMountedPartitionCount();
	int GetMountedPartitionType(int idx_mounted);
	char *GetMountedPartitionString(int idx_mounted);

	int StrFree(PVDVD_DISC_LIST pVDVDList);
	int UpdateDVDList();
// member variable
	unsigned long m_Helper_status;
private:
	ENABLED_DISK m_EnabledDisk;
	PFIRST_L_DISK_LIST m_FDiskList;

	char * m_rootpath;
	char * m_dvddev_name;

	int m_partitions[8];
	struct PartitionInfo{
		int fs_type;
		char mount_point[1024];				
	} m_mountedPartitions[8];
};
#endif //#ifndef __NDAS_HELPER_H__
