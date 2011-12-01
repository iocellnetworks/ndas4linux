#ifdef NDAS_MSHARE
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <fcntl.h>
#include <syslog.h>
#include <stdlib.h>
#include <dirent.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <unistd.h>
#include <linux/major.h>
#include <linux/ioctl.h>
#include <ctype.h>
#include <asm/ioctl.h>
#include <sys/mount.h>
extern "C" {
#include "ndas_api.h"
}
#include "ndas_helper.h"

#define ND_ROOT "/tmp/ndas"
#define ND_MOUNT_POINT "/mnt"

typedef enum _NDAS_FS_TYPE {
	NDAS_FS_UNKN = 0,
	NDAS_FS_NTFS = 1,
	NDAS_FS_VFAT = 2,
	NDAS_FS_EXT3 = 3,
	NDAS_FS_EXT2 = 4,
} NDAS_FS_TYPE, *PNDAS_FS_TYPE;

NDASHelper::NDASHelper(char * DVDDevString)
{
	printf("[%s] DVDDevString = %s, MountingPoint = %s\n", __FUNCTION__, DVDDevString);
	m_Helper_status = NDAS_HELPER_STATUS_INIT;
	m_EnabledDisk.VDiscList = NULL;
	m_FDiskList = NULL;
	m_dvddev_name = strdup(DVDDevString);

	for(int i = 0; i < sizeof(m_mountedPartitions) / sizeof(m_mountedPartitions[0]); i++)
	{
		m_mountedPartitions[i].fs_type = NDAS_FS_UNKN;
	}

	m_rootpath = ND_ROOT;
}

NDASHelper::~NDASHelper()
{
	if(m_EnabledDisk.VDiscList ){
		 StrFree(m_EnabledDisk.VDiscList );
		 free(m_EnabledDisk.VDiscList) ; 	
	}
	if(m_FDiskList){
		 free(m_FDiskList);
	}

	m_rootpath = NULL;
}

void NDASHelper::recheck_list(NDAS_DISK_LIST_PTR ndas_disk_list, int nr_disk, char * ndasids)
{
	int i = 0;
	int j = 0;
	int check_count = 0;
	char * p = NULL;
	NDAS_DISK_PTR ndas_disk = NULL;

	for (i = 0; i < nr_disk; i++) {
	    p = ndasids + i * (ND_NDAS_ID_LENGTH + 1);
//	    printf("%s\n",p);
	    check_count = ndas_disk_list->count;
	    ndas_disk = ndas_disk_list->head;
	    for(j = 0; j <  check_count; j++)
	    {
	    	// Check this information is set
	    	if(!strncmp(ndas_disk->DiskStr, p, (ND_NDAS_ID_LENGTH + 1)))
	    	{
//	    		fprintf(stderr, "search entry %s\n", p);
	    		break;
	    	}
	    	ndas_disk = ndas_disk->next;
	    }

	    // Added new entry
	    if(j == check_count){
	    	ndas_disk = NULL;
	    	ndas_disk = (NDAS_DISK_PTR)malloc(sizeof(NDAS_DISK_T));
	    	if(!ndas_disk) {
	    		fprintf(stderr, "Fail alloc ndas_disk\n");
	    		break;
	    	}
	    	memset(ndas_disk, 0, sizeof(NDAS_DISK_T));
	    	memcpy(ndas_disk->DiskStr, p,(ND_NDAS_ID_LENGTH + 1));
	    	ndas_disk->next = ndas_disk_list->head;
	    	ndas_disk_list->head = ndas_disk;
	    	ndas_disk_list->count ++;
	    }
	}	
}

void NDASHelper::freelist(NDAS_DISK_LIST_PTR ndas_disk_list)
{
	NDAS_DISK_PTR p, q;
	p = ndas_disk_list->head;
	while(p){
		q = p;
		p = p->next;
		free(q);
	}
}

int NDASHelper::GetFirstLevelDiskList(PFIRST_L_DISK_LIST * ppFDiskList)
{
	PFIRST_L_DISK pFDisk= NULL;
	ndas_ioctl_probe_t arg;
	int err = 0;
	int i = 0;
	char * p = NULL;
	int nr_disk= 0;
	int size = 0;
	NDAS_DISK_LIST_T ndas_disk_list;
	NDAS_DISK_PTR ndas_disk = NULL;
	int max_loop = 3;
	
	memset(&ndas_disk_list, 0 , sizeof(NDAS_DISK_LIST_T));
	
	if(m_FDiskList != NULL) {
		free(m_FDiskList);
		m_FDiskList = NULL;
		m_Helper_status &= ~NDAS_HELPER_STATUS_FDISK_SET;
	}

	for(i = 0 ; i < max_loop; i++)
	{
		memset(&arg, 0, sizeof(ndas_ioctl_probe_t));
		
		arg.sz_array =0;
		arg.ndasids = NULL;

		err = do_ndc_probe(&arg);
		if(err < 0){
			fprintf(stderr,"GetFirstLevelDiskList : fail do_ndc_probe err %d\n", err);
			continue;
		}

		if(arg.nr_ndasid <= 0){
			fprintf(stderr,"GetFirstLevelDiskList : nr_ndasid is too small %d\n", arg.nr_ndasid);
			continue;
		}

		if(arg.nr_ndasid <= 0) continue;

		arg.ndasids = (char *)malloc((NDAS_ID_LENGTH + 1) * arg.nr_ndasid);
		arg.sz_array = arg.nr_ndasid;

		err = do_ndc_probe(&arg);
		if(err < 0){
			fprintf(stderr,"GetFirstLevelDiskList : fail  2nd do_ndc_probe err %d\n", err);
			free(arg.ndasids);
			continue;
		}

		if(arg.nr_ndasid <= 0){
			fprintf(stderr,"GetFirstLevelDiskList : 2nd nr_ndasid is too small %d\n", arg.nr_ndasid);
			free(arg.ndasids);
			continue;
		}
		
		nr_disk = (arg.nr_ndasid > arg.sz_array) ? arg.sz_array : arg.nr_ndasid;

		recheck_list(&ndas_disk_list, nr_disk, arg.ndasids);
		free(arg.ndasids);	
		sleep(1);
	}



	nr_disk = ndas_disk_list.count;

	if(nr_disk <=0) {
		fprintf(stderr,"Fail get list count is small\n");
		err = -1;
		goto error_out;
	}
	
	size = (nr_disk -1)*sizeof(FIRST_L_DISK) + sizeof(FIRST_L_DISK_LIST);
	m_FDiskList = (PFIRST_L_DISK_LIST)malloc(size);
	if(NULL == m_FDiskList){
		fprintf(stderr,"GetFirstLevelDiskList Fail alloc m_FDiskList\n");
		err =  -1;
		goto error_out;
	}

	memset((char *)m_FDiskList,0,size);		

	m_FDiskList->Count = nr_disk;
	pFDisk = &m_FDiskList->FDISKS[0];	
#ifdef DEBUG
	fprintf(stderr,"nr_disk(%d)\n",nr_disk);		

	//  ignore even if arg.sz_array != arg.nr_ndasid 
	printf("NDAS device\n");
	printf("--------------------\n");
#endif
	ndas_disk = ndas_disk_list.head;
	
	for (i = 0; i < nr_disk; i++) {
	    p = ndas_disk->DiskStr;
#ifdef DEBUG
	    printf("%s\n",p);
#endif
	    pFDisk[i].Index = i;
	    memcpy(pFDisk[i].DiskStr, p, ( ND_NDAS_ID_LENGTH + 1));
	    ndas_disk = ndas_disk->next;
	}
#ifdef DEBUG
	printf("--------------------\n");
#endif
	*ppFDiskList = m_FDiskList;
	m_Helper_status |= NDAS_HELPER_STATUS_FDISK_SET;
error_out:
	freelist(&ndas_disk_list);
	return err;
				
}

/*
int NDASHelper::GetFirstLevelDiskList(PFIRST_L_DISK_LIST *ppFDiskList)
{
	PFIRST_L_DISK pFDisk= NULL;
	ndas_ioctl_probe_t arg;
	int err = 0;
	int i = 0;
	char * p = NULL;
	int nr_disk= 0;
	int size = 0;

	if(m_FDiskList != NULL) {
		free(m_FDiskList);
		m_FDiskList = NULL;
		m_Helper_status &= ~NDAS_HELPER_STATUS_FDISK_SET;
	}
	
	memset(&arg, 0, sizeof(ndas_ioctl_probe_t));
	
	arg.sz_array =0;
	arg.ndasids = NULL;

	err = do_ndc_probe(&arg);
	if(err < 0){
		fprintf(stderr,"GetFirstLevelDiskList : fail do_ndc_probe err %d\n", err);
		return err;
	}

	if(arg.nr_ndasid <= 0){
		fprintf(stderr,"GetFirstLevelDiskList : nr_ndasid is too small %d\n", arg.nr_ndasid);
		return -1;
	}

	arg.ndasids = (char *)malloc((NDAS_ID_LENGTH + 1) * arg.nr_ndasid);
	arg.sz_array = arg.nr_ndasid;

	err = do_ndc_probe(&arg);
	if(err < 0){
		fprintf(stderr,"GetFirstLevelDiskList : fail  2nd do_ndc_probe err %d\n", err);
		goto error_out;
	}

	if(arg.nr_ndasid <= 0){
		fprintf(stderr,"GetFirstLevelDiskList : 2nd nr_ndasid is too small %d\n", arg.nr_ndasid);
		err =  -1;
		goto error_out;
	}


	nr_disk = (arg.nr_ndasid > arg.sz_array) ? arg.sz_array : arg.nr_ndasid;

	size = (nr_disk -1)*sizeof(FIRST_L_DISK) + sizeof(FIRST_L_DISK_LIST);
	m_FDiskList = (PFIRST_L_DISK_LIST)malloc(size);

	if(NULL == m_FDiskList){
		fprintf(stderr,"GetFirstLevelDiskList Fail alloc m_FDiskList\n");
		err =  -1;
		goto error_out;
	}

	memset((char *)m_FDiskList,0,size);		

	m_FDiskList->Count = nr_disk;
	pFDisk = &m_FDiskList->FDISKS[0];	
	fprintf(stderr,"nr_disk(%d)\n",nr_disk);		

	//  ignore even if arg.sz_array != arg.nr_ndasid 
	printf("NDAS device\n");
	printf("--------------------\n");
	for (i = 0; i < nr_disk; i++) {
	    int j;
	    p = arg.ndasids + i * (ND_NDAS_ID_LENGTH + 1);
	    printf("%s\n",p);
	    pFDisk[i].Index = i;
	    memcpy(pFDisk[i].DiskStr, p, ( ND_NDAS_ID_LENGTH + 1));
	}
	printf("--------------------\n");
	*ppFDiskList = m_FDiskList;
	m_Helper_status |= NDAS_HELPER_STATUS_FDISK_SET;
error_out:
	free(arg.ndasids);
	return err;
		

}
*/

int NDASHelper::StrFree(PVDVD_DISC_LIST pVDVDList)
{
	unsigned int count = 0;
	unsigned int i = 0;
	PVDVD_DISC pVDisk = NULL;
	
	count = pVDVDList->Count;
	pVDisk = &pVDVDList->VDiscs[0];

	for(i = 0; i< count; i++)
	{
		if(pVDisk[i].DiscStr) free(pVDisk[i].DiscStr);
		pVDisk[i].DiscStr = NULL; 					
	}
	return 0;
}

// Event Handler
int NDASHelper::EnableDisk(ENABLE_PTR enable_info, int * Diskslot)
{
	int ret = 0;
	ndas_ioctl_register_t arg;
	ndas_ioctl_enable_t arg2;
	ndas_ioctl_juke_disk_info_t arg3;
	PDISK_information	diskInfo;
	
	int slot = 0;	
	int retry_count = 0;
	int retry_max = 1;

	if(NDAS_HELPER_STATUS_DISK_ENABLED &  m_Helper_status){
		fprintf(stderr, "Fail DISK in already set\n");
		return -1;		
	}
	
retry:
	memset(&arg, 0, sizeof(ndas_ioctl_register_t));
	memcpy(arg.name, enable_info->Name, (ND_NDAS_MAX_NAME_LENGTH));
	memcpy(arg.ndas_idserial, enable_info->DiskStr, (ND_NDAS_ID_LENGTH + 1));
	memcpy(arg.ndas_key, enable_info->Key, (ND_NDAS_KEY_LENGTH +1));
	
	if((ret = do_ndc_register(&arg)) < 0)
	{
		fprintf(stderr, "Fail EnableDisk :  do_ndc_register\n");
		return ret;
	}

	memset(&arg2, 0, sizeof(ndas_ioctl_enable_t));
	if(get_slot_number(enable_info->Name, &slot) <0 )
	{
		fprintf(stderr, "Fail get slot number\n");
		goto clean_reg;
	}
	
	if(slot <= 0 || slot > 7) {
		fprintf(stderr, "Fail get_slot_number\n");
		goto clean_reg;	
	}

	arg2.slot = slot;
	arg2.write_mode = 0;

	sleep(5);

	if((ret = do_ndc_enable(&arg2)) < 0)
	{
clean_reg:	
		ndas_ioctl_unregister_t arg4;
		fprintf(stderr, "Fail EnableDisk :  do_ndc_enable\n");
				
		memset(&arg4, 0 ,sizeof(ndas_ioctl_unregister_t));
		memcpy(arg4.name,enable_info->Name,(ND_NDAS_MAX_NAME_LENGTH ));
		ret = do_ndc_unregister(&arg4);
		if(ret < 0){
			fprintf(stderr,"Fail EnableDisk :  do_ndc_unregister\n");
			return ret;
		}
		
		if(retry_count >= retry_max) {
			return -1;
		}else{
			retry_count ++;
			goto retry;
		}
	}

	
	memset(&arg3, 0, sizeof(ndas_ioctl_juke_disk_info_t));
	arg3.slot = slot;
	arg3.queryType =1;


	m_EnabledDisk.slot = slot;
	memcpy(m_EnabledDisk.Name, enable_info->Name, (ND_NDAS_MAX_NAME_LENGTH));
	memcpy(m_EnabledDisk.DiskStr, enable_info->DiskStr, (ND_NDAS_ID_LENGTH + 1));
	memcpy(m_EnabledDisk.Key, enable_info->Key, (ND_NDAS_KEY_LENGTH +1));
	m_Helper_status |= NDAS_HELPER_STATUS_DISK_ENABLED;
	
	if((ret = do_ndc_diskinfo(&arg3)) < 0){
		DisableDisk();
		return -1;
	}


	MakeNode(slot);
	
	if(arg3.diskType == 1){
		m_EnabledDisk.DiskType = NDAS_TYPE_MEDIAJUKE ;
		UpdateDVDList();
	}else{
		m_EnabledDisk.DiskType = NDAS_TYPE_DISK;
		// to do mouting disk
		if(MountXiDisk(slot) == 0)
		{
			DisableDisk();
			return -1;
		}
	}

	*Diskslot = slot;
	return m_EnabledDisk.DiskType;
}


void NDASHelper::MakeNode(int slot)
{
	dev_t devid;
	char path[50];
	char dc = 'a' + (slot -NDAS_FIRST_SLOT_NR );
	int i = 0;
	int minor = slot-NDAS_FIRST_SLOT_NR;
	memset(path,0,50);
	sprintf(path,"/dev/nd%c",dc);
	devid = ((dev_t)(60 << 8)|(dev_t)(minor<<3));

	fprintf(stderr,"mknod path=%s dev_num=%x\n",path, devid);
	if(mknod(path, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH|S_IFBLK, devid) < 0)
	{
		fprintf(stderr,"already created dev=%s\n",path);
	}

	for(i = 1; i < 8; i++)
	{
		memset(path,0,50);
		sprintf(path,"/dev/nd%c%d",dc,i);
		devid = ((dev_t)(60 << 8)|(dev_t)(minor<<3) |(dev_t) i);
		fprintf(stderr,"mknod path=%s dev_num=%x\n",path, devid);
		if(mknod(path, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH|S_IFBLK, devid) < 0)
		{
			fprintf(stderr,"already created dev=%s\n",path);
		}
	}
	
}

typedef struct _NDASMountInfo
{
	char *source;
	char *target;
} NDASMountInfo, *PNDASMountInfo;

NDASMountInfo g_MountPaths[] = {
	{"/dev/nda", ND_MOUNT_POINT "/nda/"},
	{"/dev/nda1", ND_MOUNT_POINT "/nda1/"},
	{"/dev/nda2", ND_MOUNT_POINT "/nda2/"},
	{"/dev/nda3", ND_MOUNT_POINT "/nda3/"},
	{"/dev/nda4", ND_MOUNT_POINT "/nda4/"},
	{"/dev/nda5", ND_MOUNT_POINT "/nda5/"},
	{"/dev/nda6", ND_MOUNT_POINT "/nda6/"},
	{"/dev/nda7", ND_MOUNT_POINT "/nda7/"},
};

typedef struct _NDASMountParam
{
	unsigned long fstype;
	char *filesystemtype;
	unsigned long mountflags;
	void *data;	
} NDASMountParam, *PNDASMountParam;

NDASMountParam MountParams[] = {
	{NDAS_FS_UNKN, "void", 0, NULL},
	{NDAS_FS_NTFS, "ntfs", MS_RDONLY | MS_NOSUID| MS_NODEV, (void *)"nls=utf8"},
	{NDAS_FS_VFAT, "vfat", MS_RDONLY | MS_NOSUID| MS_NODEV, (void *)"iocharset=utf8,codepage=949"},
	{NDAS_FS_EXT3, "ext3", MS_RDONLY | MS_NOSUID| MS_NODEV, (void *)NULL},	
	{NDAS_FS_EXT2, "ext2", MS_RDONLY | MS_NOSUID| MS_NODEV, (void *)NULL},	
};

const char *NDASHelper::GetMountedPathTarget(int idx_mounted)
{
	int part;
	part= MapIdxToPartition(idx_mounted);

	if(part < 0)
		return NULL;

	return g_MountPaths[part].target;
}

int NDASHelper::MapIdxToPartition(int idx_mounted)
{
	int i;
	int count = 0;
	for(i = 0; i < sizeof(g_MountPaths) / sizeof(g_MountPaths[0]); i++)
	{
		if(NDAS_FS_UNKN != m_partitions[i])
		{
			if(count == idx_mounted)
			{
				return i;
			}
			count++;
		}
	}

	return -1;
}

int NDASHelper::MountPartition(char *devname, char *mountpoint)
{
	int i;
	int result = 0;
	char deviceName[30];

	for(int i = 1; i < sizeof(MountParams) / sizeof(MountParams[0]); i++)
	{
		fprintf(stderr, "mount(%s, %s, %s, %X, %p)\n",
			devname, 
			mountpoint, 
			MountParams[i].filesystemtype, 
			MountParams[i].mountflags, 
			MountParams[i].data);
		result = mount(
			devname, 
			mountpoint, 
			MountParams[i].filesystemtype, 
			MountParams[i].mountflags, 
			MountParams[i].data);

		if(0 == result)
			return MountParams[i].fstype;
	}

	return NDAS_FS_UNKN;

}

int NDASHelper::GetMountedPartitionCount()
{
	int i;
	int mount_count = 0;
	for(mount_count = 0, i = 0; i < sizeof(g_MountPaths) / sizeof(g_MountPaths[0]); i++)
	{
		if(NDAS_FS_UNKN != m_partitions[i])
		{
			mount_count++;
		}
	}
	
	return mount_count;
}

int NDASHelper::GetMountedPartitionType(int idx_mounted)
{
	int part;
	part= MapIdxToPartition(idx_mounted);

	if(part < 0)
		return NDAS_FS_UNKN;

	return m_partitions[part];
}

char *NDASHelper::GetMountedPartitionString(int idx_mounted)
{
	switch(GetMountedPartitionType(idx_mounted))
	{
		case NDAS_FS_NTFS: return "NTFS";
		case NDAS_FS_VFAT: return "FAT 32";
		case NDAS_FS_EXT3: return "EXT3";
		case NDAS_FS_EXT2: return "EXT2";
		case NDAS_FS_UNKN:
		default:
			return "Unknown";
	}
}

int NDASHelper::MountXiDisk(int slot)
{

	char dc = 'a' + (slot -NDAS_FIRST_SLOT_NR );
	int count = 0;
	int i = 0, j;
	int mount_count = 0;
	int result = 0;
	char deviceName[30];
	unsigned long fstype;
	char mount_point[30];
	
	if(!(NDAS_HELPER_STATUS_DISK_ENABLED & m_Helper_status))
	{
		fprintf(stderr,"DisableDisk status is not ENABLED\n");
		return -1;
	}

	if(NDAS_TYPE_DISK != m_EnabledDisk.DiskType)
	{
		fprintf(stderr,"Disk type is not NDAS_TYPE_DISK\n");
		return -1;
	}

	// mount /dev/nda* -> /mnt/nda*
	for(mount_count = 0, i = 0; i < sizeof(g_MountPaths) / sizeof(g_MountPaths[0]); i++)
	{
		m_partitions[i] = MountPartition(
			g_MountPaths[i].source,
			g_MountPaths[i].target);
		
		if(NDAS_FS_UNKN != m_partitions[i])
		{
			mount_count++;
			fprintf(stdout, "======== device %s is mounted at %s as type of %d. mount_count(%d) ========\n",
				g_MountPaths[i].source,
				g_MountPaths[i].target,
				m_partitions[i],
				mount_count);
		}
	}

	// from here now use GetMountedPartitionCount() instead of mount_count

	// symbolic link
	switch(GetMountedPartitionCount())
	{
		case 0:
			// nothing was mounted
			return 0;
		case 1:
			// soft link /mnt/nda? -> ND_ROOT
			printf("single partitions mount : [%s] -> [%s]\n", GetMountedPathTarget(0), ND_ROOT);
			symlink(GetMountedPathTarget(0), ND_ROOT);
			break;
		default:
			// soft link /mnt/nda* -> ND_ROOT/partition?
			result = mkdir(ND_ROOT, 0x777);
			if(0 != result && EEXIST != errno)
			{
					return 0;
			}

			// reverse link for mediashare list sort order problem
			for(i = GetMountedPartitionCount() -1; i >= 0 ; i--)
			{
				sprintf(mount_point, ND_ROOT "/partition %d [%s]", i, GetMountedPartitionString(i));
				symlink(GetMountedPathTarget(i), mount_point);
			}
			break;
	}

	return GetMountedPartitionCount();
}

int NDASHelper::UnMountXiDisk()
{
	int result = 0;
	if(!(NDAS_HELPER_STATUS_DISK_ENABLED & m_Helper_status))
	{
		fprintf(stderr,"DisableDisk status is not ENABLED\n");
		return -1;
	}

	if(NDAS_TYPE_DISK != m_EnabledDisk.DiskType)
	{
		fprintf(stderr,"Disk type is not NDAS_TYPE_DISK\n");
		return -1;
	}
	
	int i;
	int mount_count = GetMountedPartitionCount();

	// unmount all the partitions
	for(i = 0; i < mount_count; i++)
	{
		result = umount(g_MountPaths[MapIdxToPartition(i)].target);
		if(result < 0)
		{
			fprintf(stderr, "FAILED to unmount %s\n", g_MountPaths[MapIdxToPartition(i)].target);
		}
	}

	switch(mount_count)
	{
		case 0:
			break;
		case 1:
			unlink(ND_ROOT);
			break;
		default:
			DIR *d;
			struct dirent *c;
			char buffer[256];

			d = opendir(ND_ROOT);
			if(d != NULL)
			{
				while((c = readdir(d)) != NULL)
				{
					sprintf(buffer, ND_ROOT "/%s", c->d_name);
					if(0 == strcmp(c->d_name, ".") || 0 == strcmp(c->d_name, ".."))
					{
						continue;
					}
					
					printf("removing %s\n", buffer);
					unlink(buffer);
					rmdir(buffer);
				}
			}
			closedir(d);

			rmdir(ND_ROOT);
			break;
	}

	// clear mounted partition infos
	for(i = 0; i < sizeof(m_mountedPartitions) / sizeof(m_mountedPartitions[0]); i++)
	{
		m_mountedPartitions[i].fs_type = NDAS_FS_UNKN;
	}
	
}

int NDASHelper::DisableDisk()
{
	PFIRST_L_DISK pFdisk = NULL;
	int ret = 0;
	ndas_ioctl_unregister_t arg;
	ndas_ioctl_arg_disable_t arg2;
	
	
	if(!(NDAS_HELPER_STATUS_DISK_ENABLED & m_Helper_status))
	{
		fprintf(stderr,"DisableDisk status is not ENABLED\n");
		return 0;
	}

	if(NDAS_TYPE_DISK == m_EnabledDisk.DiskType)
	{
		// to do unmounting disk
		UnMountXiDisk();	
	}

	memset(&arg2, 0, sizeof(ndas_ioctl_arg_disable_t ));
	arg2.slot = m_EnabledDisk.slot;

	if((ret = do_ndc_disable(&arg2)) < 0)
	{
		if(ret ==NDAS_ERROR_INVALID_SLOT_NUMBER ||
		 	ret == NDAS_ERROR_ALREADY_DISABLED_DEVICE){
		 	fprintf(stderr,"May have some problem\n");
		}else{
			fprintf(stderr,"Fail do_ndc_diable\n");
			return ret;
		}
	}


	memset(&arg, 0, sizeof(ndas_ioctl_unregister_t));
	memcpy(arg.name,m_EnabledDisk.Name,(NDAS_MAX_NAME_LENGTH));

	if((ret = do_ndc_unregister(&arg)) <0 )
	{
		fprintf(stderr,"Faildo_ndc_unregister\n");
		return ret;
	}


	if(m_EnabledDisk.VDiscList)
	{
		StrFree(m_EnabledDisk.VDiscList);
		free(m_EnabledDisk.VDiscList);
		m_EnabledDisk.VDiscList = NULL;
	}
	
	if(m_FDiskList)
	{
		free(m_FDiskList);
		m_FDiskList = NULL;
	}

	m_EnabledDisk.slot  = 0;
	m_EnabledDisk.DiskType = 0;
	memset(m_EnabledDisk.Name, 0, (ND_NDAS_MAX_NAME_LENGTH));
	memset(m_EnabledDisk.DiskStr, 0, (ND_NDAS_ID_LENGTH + 1));
	memset(m_EnabledDisk.Key, 0, (ND_NDAS_KEY_LENGTH +1));
	m_Helper_status &= ~NDAS_HELPER_STATUS_DISK_ENABLED; 

	return 0;			
}


int NDASHelper::GetDVDList(PVDVD_DISC_LIST *ppDVDList)
{
	if(!(NDAS_HELPER_STATUS_DISK_ENABLED &m_Helper_status))
	{
		fprintf(stderr,"Fail GetDVDList Invalid status\n");
		return -1;
	}

	if(NDAS_TYPE_MEDIAJUKE !=m_EnabledDisk.DiskType)
	{
		fprintf(stderr,"NOT MEDIAJUKE\n");
		return -1;
	}


	if(NULL == m_EnabledDisk.VDiscList){
		UpdateDVDList();
	}

	*ppDVDList = m_EnabledDisk.VDiscList;
	return 0;
}


int NDASHelper::SetDVDStr(PVDVD_DISC_LIST pDVDList , int slot)
{
	unsigned int i = 0;
	unsigned int max_loop = 0;
	ndas_ioctl_juke_disc_info_t arg;
	PDISC_INFO	pdiscInfo = NULL;
	PVDVD_DISC pVDisk = NULL;
	pVDisk = &pDVDList->VDiscs[0];
	

	pdiscInfo = (PDISC_INFO)malloc(sizeof(DISC_INFO));
	if(!pdiscInfo) {
		fprintf(stderr, "SetDVDStr : can't alloc DiscInfo buffer\n");
		return -1;
	}

	fprintf(stderr,"pDVDList->Count = %d\n", pDVDList->Count);
	max_loop = pDVDList->Count;
	
	for(i = 0; i < max_loop; i++)
	{
		fprintf(stderr,"Count %d\n", i);
		if((pVDisk[i].Status & DISC_STATUS_VALID) || 
			(pVDisk[i].Status & DISC_STATUS_VALID_END))
		{
			memset(&arg, 0, sizeof(ndas_ioctl_juke_disc_info_t));
			arg.slot = slot;
			arg.selected_disc = i;
			arg.discInfo = pdiscInfo;

			if(do_ndc_discinfo(&arg) < 0){
				char buff[50];
				fprintf(stderr, "Fail get DVD string\n");
				memset(buff,0,50);
				sprintf(buff,"unkonwn info%d\n",i);
				pVDisk[i].DiscStr = strdup(buff);
			}else{
				if(strlen((char *)pdiscInfo->title_name) > ND_NDAS_MAX_NAME_LENGTH){
					char buff[50];
					fprintf(stderr, "Fail get DVD string\n");
					memset(buff,0,50);
					sprintf(buff,"unkonwn info%d\n",i);
					pVDisk[i].DiscStr = strdup(buff);
				}else{
					pVDisk[i].DiscStr = strdup((char *)pdiscInfo->title_name);
					
					pVDisk[i].DiscStr = (char *)malloc(ND_NDAS_MAX_NAME_LENGTH);
					if(pVDisk[i].DiscStr)
					{
						memset(pVDisk[i].DiscStr,0,ND_NDAS_MAX_NAME_LENGTH);
						memcpy(pVDisk[i].DiscStr, pdiscInfo->title_name, ND_NDAS_MAX_NAME_LENGTH);
						pVDisk[i].DiscStr[127] = '\0';
					}else{
						pVDisk[i].DiscStr = strdup("unkonwn info");
					}	
				}
			}

		}
			
	}

	free(pdiscInfo);
	return 0;
}


int NDASHelper::UpdateDVDList()
{
	ndas_ioctl_juke_disk_info_t arg;
	PDISK_information pdiskinfo = NULL;
	int err = 0;
	PVDVD_DISC pVDisk = NULL;
	unsigned int size = 0;
	int ret = 0;
	unsigned int i = 0;

	if(!(NDAS_HELPER_STATUS_DISK_ENABLED &m_Helper_status))
	{
		fprintf(stderr,"UpdateDVDList Invalid status\n");
		return -1;
	}


	if(NDAS_TYPE_MEDIAJUKE !=m_EnabledDisk.DiskType)
	{
		fprintf(stderr,"NOT MEDIAJUKE\n");
		return -1;
	}

	
	// set AddrMap	
	if(NULL != m_EnabledDisk.VDiscList)
	{
		StrFree(m_EnabledDisk.VDiscList);
		free(m_EnabledDisk.VDiscList);
		m_EnabledDisk.VDiscList = NULL;
	}	

	memset(&arg,0, sizeof(ndas_ioctl_juke_disk_info_t));

	pdiskinfo =(PDISK_information) malloc(sizeof(DISK_information));
	if(!pdiskinfo){
		fprintf(stderr,"UpdateDVDList fail alloc diskinfo buff\n");
		return -1;
	}

	memset(pdiskinfo, 0, sizeof(DISK_information));
	
	arg.slot = m_EnabledDisk.slot;
	arg.diskInfo = pdiskinfo;

	err = do_ndc_diskinfo(&arg);
	if(err <0 ){
		fprintf(stderr, "UpdateDVDList fail do_ndc_diskinfo err=%d\n", err);
		goto error_out;
	}

	size = (pdiskinfo->count - 1)*sizeof(VDVD_DISC) + sizeof(VDVD_DISC_LIST);	

	m_EnabledDisk.VDiscList = (PVDVD_DISC_LIST)malloc(size);

	if(NULL == m_EnabledDisk.VDiscList)
	{
		fprintf(stderr,"Fail malloc m_VDiscList\n");
		err =  -1;
		goto error_out;
		
	}

	memset((char *)m_EnabledDisk.VDiscList,0,size);
	
	m_EnabledDisk.VDiscList->Count = pdiskinfo->count;
	pVDisk = &(m_EnabledDisk.VDiscList->VDiscs[0]);
	
	for(i = 0;  i< pdiskinfo->count; i++)
	{	
		pVDisk[i].index = i;
		pVDisk[i].partnum = -1;
		pVDisk[i].slot = (unsigned char)m_EnabledDisk.slot;
		pVDisk[i].Disc = (unsigned char)i;			
		pVDisk[i].Status = pdiskinfo->discinfo[i];				
		pVDisk[i].DiscStr = NULL;			
	}

	for(i = 0; i< 8; i++)
	{
		if(pdiskinfo->partinfo[i] != -1 &&  pdiskinfo->partinfo[i] != 0){
			if(pdiskinfo->partinfo[i] > 0){
				pVDisk[pdiskinfo->partinfo[i]].partnum = i;
			}
		}
	}
	
	SetDVDStr(m_EnabledDisk.VDiscList, m_EnabledDisk.slot);
error_out:
	if(pdiskinfo) free(pdiskinfo);
	return 0;
}


int NDASHelper::SelectDisc(int disc , int *partnum)
{
	int ret = 0;
	ndas_ioctl_juke_read_start_t arg;
	
	memset(&arg, 0, sizeof(ndas_ioctl_juke_read_start_t));
	
	if(!(NDAS_HELPER_STATUS_DISK_ENABLED &m_Helper_status))
	{
		fprintf(stderr,"Fail GetDVDList Invalid status\n");
		return -1;
	}
	
	if(NDAS_TYPE_MEDIAJUKE !=m_EnabledDisk.DiskType)
	{
		fprintf(stderr,"NOT MEDIAJUKE\n");
		return -1;
	}

	if(m_EnabledDisk.VDiscList == NULL){
		if((ret =UpdateDVDList()) < 0){
			fprintf(stderr,"Fail UpdateDVDList()\n");
			return ret;
		}
	}

	if(NULL == m_EnabledDisk.VDiscList){
		fprintf(stderr,"Fail m_EnabledDisk.VDiscList == NULL\n");
		return -1;
	}
	
	if(m_EnabledDisk.VDiscList->Count < disc){
		fprintf(stderr, "fail disc=%d is too big\n", disc);
			return -1;
	}

	


	arg.slot = m_EnabledDisk.slot;
	arg.selected_disc = disc;

	if((ret = do_ndc_seldisc(&arg)) < 0)
	{
		if(arg.error = NDAS_ERROR_JUKE_DISC_VALIDATE_FAIL){
			fprintf(stderr,"NEED validation\n");
			return 1;
		}else{
			fprintf(stderr,"Fail do_ndc_seldisc disc=%d\n", disc);
			return ret;
		}
	}

	m_EnabledDisk.VDiscList->VDiscs[disc].selected = 1;
	m_EnabledDisk.VDiscList->VDiscs[disc].partnum = (unsigned char)arg.partnum;
	*partnum = arg.partnum;
	
	return ret;
}


int NDASHelper::DeselectDisc(int disc , int partnum)
{
	int ret = 0;
	ndas_ioctl_juke_read_end_t arg;
	
	memset(&arg, 0, sizeof(ndas_ioctl_juke_read_end_t));
	
	if(!(NDAS_HELPER_STATUS_DISK_ENABLED &m_Helper_status))
	{
		fprintf(stderr,"Fail GetDVDList Invalid status\n");
		return -1;
	}

	if(NDAS_TYPE_MEDIAJUKE !=m_EnabledDisk.DiskType)
	{
		fprintf(stderr,"NOT MEDIAJUKE\n");
		return -1;
	}



	if(NULL == m_EnabledDisk.VDiscList){
		if((ret =UpdateDVDList()) < 0){
			fprintf(stderr,"Fail UpdateDVDList()\n");
			return ret;
		}
	}
	
	if(m_EnabledDisk.VDiscList->Count < disc){
		fprintf(stderr, "fail disc=%d is too big\n", disc);
			return -1;
	}


	arg.slot = m_EnabledDisk.slot;
	arg.selected_disc = disc;
	arg.partnum = partnum;


	if((ret = do_ndc_deseldisc(&arg)) < 0)
	{
	
		fprintf(stderr,"Fail do_ndc_deseldisc=%d\n", disc);
		return ret;
		
	}

	m_EnabledDisk.VDiscList->VDiscs[disc].selected =0;
	m_EnabledDisk.VDiscList->VDiscs[disc].partnum = -1;
	
	return ret;
}

#ifdef NDAS_MSHARE_WRITE
int NDASHelper::MFormat(int slot)
{
	int ret = 0;
	ndas_ioctl_juke_format_t arg;
	memset(&arg, 0, sizeof(ndas_ioctl_juke_format_t));
	
	if(!(NDAS_HELPER_STATUS_DISK_ENABLED &m_Helper_status))
	{
		fprintf(stderr,"Fail GetDVDList Invalid status\n");
		return -1;
	}

	if(NDAS_TYPE_MEDIAJUKE !=m_EnabledDisk.DiskType)
	{
		fprintf(stderr,"NOT MEDIAJUKE\n");
		return -1;
	}
	
	if(NULL == m_EnabledDisk.VDiscList){
		fprintf(stderr,"Fail m_EnabledDisk.VDiscList == NULL\n");
		return -1;
	}
	
	arg.slot = slot;
	if((ret = do_ndc_format(&arg)) < 0)
	{
		fprintf(stderr,"Fail do_ndc_formatn");
		return ret;
	}
	
	UpdateDVDList();

	return 0;
}


int NDASHelper::MCopy(int disc)
{
	int ret = 0;


	if(!(NDAS_HELPER_STATUS_DISK_ENABLED &m_Helper_status))
	{
		fprintf(stderr,"Fail GetDVDList Invalid status\n");
		return -1;
	}

	if(NDAS_TYPE_MEDIAJUKE !=m_EnabledDisk.DiskType)
	{
		fprintf(stderr,"NOT MEDIAJUKE\n");
		return -1;
	}
	
	if(NULL == m_EnabledDisk.VDiscList){
		fprintf(stderr,"Fail m_EnabledDisk.VDiscList == NULL\n");
		return -1;
	}
	

	if(m_EnabledDisk.VDiscList->Count < disc){
		fprintf(stderr, "fail disc=%d is too big\n", disc)
			return -1;
	}

	if((ret = do_ndc_rcopy(m_EnabledDisk.slot, disc, m_dvddev_name)) < 0)
	{
		fprintf(stderr,"Fail do_ndc_rcopy\n");
		return ret;
	}

	UpdateDVDList();
	return 0;

}

int NDASHelper::MValidate(int disc)
{
	int ret = 0;
	

	if(!(NDAS_HELPER_STATUS_DISK_ENABLED &m_Helper_status))
	{
		fprintf(stderr,"Fail GetDVDList Invalid status\n");
		return -1;
	}


	if(NDAS_TYPE_MEDIAJUKE !=m_EnabledDisk.DiskType)
	{
		fprintf(stderr,"NOT MEDIAJUKE\n");
		return -1;
	}
	
	if(NULL == m_EnabledDisk.VDiscList){
		fprintf(stderr,"Fail m_EnabledDisk.VDiscList == NULL\n");
		return -1;
	}
	
	if(m_EnabledDisk.VDiscList->Count < disc){
		fprintf(stderr, "fail disc=%d is too big\n", disc)
			return -1;
	}

	if((ret = do_ndc_validate(m_EnabledDisk.slot, disc, m_dvddev_name)) < 0)
	{
		fprintf(stderr,"do_ndc_validate\n");
		return ret;
	}


	UpdateDVDList();
	return 0;
		
}



int NDASHelper::MDel(int disc)
{
	int ret = 0;
	ndas_ioctl_juke_del_t arg;
	
	memset(&arg, 0, sizeof(ndas_ioctl_juke_del_t));

	if(!(NDAS_HELPER_STATUS_DISK_ENABLED &m_Helper_status))
	{
		fprintf(stderr,"Fail GetDVDList Invalid status\n");
		return -1;
	}

	if(NDAS_TYPE_MEDIAJUKE !=m_EnabledDisk.DiskType)
	{
		fprintf(stderr,"NOT MEDIAJUKE\n");
		return -1;
	}
	
	if(NULL == m_EnabledDisk.VDiscList){
		fprintf(stderr,"Fail m_EnabledDisk.VDiscList == NULL\n");
		return -1;
	}
	

	arg.slot = m_EnabledDisk.slot;
	arg.selected_disc = disc;
	
	if( (ret =do_ndc_del(&arg)) < 0)
	{
		fprintf(stderr,"do_ndc_del\n");
		return ret;	
	}


	UpdateDVDList();
	return 0;

}
#endif //NDAS_MSHARE_WRITE
#endif
