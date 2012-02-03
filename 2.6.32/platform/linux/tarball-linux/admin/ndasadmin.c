/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
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

#define _GNU_SOURCE

#include <stdio.h> // printf
#include <string.h> // strncmp strlen
#include <stdlib.h> // malloc, free
#include <fcntl.h> // open
#include <unistd.h> // close
#include <ctype.h> // isalnum
#include <sys/ioctl.h> // ioctl
#include <sys/types.h> // open
#include <sys/stat.h> // open
#include <errno.h> // EINVAL
#include <getopt.h> // getopt_long
#include <linux/limits.h> // PATH_MAX for linux
#include <sys/utsname.h> // uname

#include "ndasadmin.h"

#include <ndas_dev.h>

#ifdef NDAS_MSHARE
#include <time.h>
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

static int ndas_fd;

static int ndc_start(int argc, char* argv[]);
static int ndc_stop(int argc, char* argv[]);
static int ndc_register(int argc, char *argv[]);
static int ndc_unregister(int argc, char *argv[]);
static int ndc_enable(int argc, char *argv[]);
static int ndc_disable(int argc, char *argv[]);
static int ndc_edit(int argc, char *argv[]);
static int ndc_request(int argc, char *argv[]);
static int ndc_probe(int argc, char *argv[]);
static int ndc_help(int argc, char* argv[]);

#ifdef NDAS_MSHARE
static int ndc_diskinfo(int argc, char* argv[]);
static int ndc_discinfo(int argc, char* argv[]);
static int ndc_seldisc(int argc, char* argv[]);
static int ndc_deseldisc(int argc, char* argv[]);
static int ndc_rcopy(int argc, char* argv[]);
static int ndc_del(int argc, char* argv[]);
static int ndc_validate(int argc, char* argv[]);
static int ndc_format(int argc, char* argv[]);
#endif


struct _ndasadmin_cmd {
    char* str;
    int (*func)(int ,char* []);
    char* help;
} ndasadmin_cmd[] = {
    {"start", ndc_start, "\tStart NDAS driver. No operation is possible before start command\n"},
    {"stop", ndc_stop, "\tStop NDAS driver. Must stop the driver before unloading\n"},
    {"register", ndc_register,  
        "\tRegister a new netdisk with ID and name or serial number.\n"
        "\tIf registration is successful, a slot number is returned.\n"
        "\tOr you can find later when the NDAS disk is online, by cat /proc/ndas/devs.\n"
        "\tThe slot number  is used for enabling and disabling the device.\n"
        "\tWhen device is registered with serial, the device can be only used in read-only mode.\n"
        "\tAdd -b(or --background) option to run device detection process in background.\n"
        "\tUsage: ndasadmin register ID[write-key] -n|--name NDASName [-b|--background]\n"
        "\t       ex) ndasadmin register XXXXX-XXXXX-XXXXX-XXXXX-WWWWW -n NetDisk1\n"
        "\t             ndasadmin register XXXXXXXX -n NetDisk1\n"},
    {"unregister", ndc_unregister, 
        "\tRemove a registered netdisk with name\n"
        "\tUsage: ndasadmin unregister -n|--name NDASName\n" },
    {"edit", ndc_edit,"\tUsage: ndasadmin edit -o NDASDeviceName -n NewNDASDeviceName\n" },
    {"enable", ndc_enable,
        "\tEnable netdisk Remove a registered netdisk with slot number or block device.\n"
        "\tIf connected in shared write mode, multiple hosts can enable the same device in write mode.\n"
        "\tBut coordination between hosts are needed to prevent file system corruption.\n"
        "\tUsage: ndasadmin enable -s SlotNumber -o accessMode\n"
        "\t       - accessMode : \'r\'(read), \'w\'(read/write) or \'s\'(shared write).\n"
        "\tUsage: ndasadmin enable -f BlockDevice -o accessMode\n"
        "\t       - accessMode : \'r\'(read), \'w\'(read/write) or \'s\'(shared write).\n"
        },
    {"disable", ndc_disable,
        "\tDisable enabled device by slot number or block device\n"
        "\tUsage: ndasadmin disable -s SlotNumber\n"
        "\tUsage: ndasadmin disable -f BlockDevice\n"},
#ifdef NDAS_MULTIDISKS    
    {"config", ndc_config,
        "\tUsage: ndasadmin config -s SlotNumber -t a -ps PeerSlotNumber\n"
        "\t       ndasadmin config -s SlotNumber -t m -ps PeerSlotNumber\n"
        "\t       ndasadmin config -s SlotNumber -t s \n"
        "\t       type 'a': aggregation, 'm': mirroring, 's': separation\n" },
#endif    
#ifdef NDAS_MSHARE
	{"diskinfo", ndc_diskinfo,
		"\tUsage : ndasadmin diskinfo slotnumber\n"},
	{"discinfo", ndc_discinfo,
		"\tUsage : ndasadmin discinfo slotnumber discnumber\n"},
	{"seldisc", ndc_seldisc,
		"\tUsage : ndasadmin seldisc slotnumber discnumber\n"},
	{"deseldisc", ndc_deseldisc,
		"\tUsage : nadsadmin deseldisc slotnumber discnumber partnum\n"},
	{"rcopy", ndc_rcopy,
		"\tUsage : ndasadmin rcopy slotnumber discnumber DVD_devfile\n"},
	{"del", ndc_del,
		"\tUsage : ndasadmin del slotnumber discnumber\n"},
	{"validate", ndc_validate,
		"\tUsage : ndasadmin validate slotnumber discnumber DVD_devfile\n"},
	{"format", ndc_format,
		"\tUsage : ndasadmin format slotnumber\n"},
#endif
    {"request", ndc_request,
        "\tRequest other host in the network to give up write access of the device.\n"
        "\tUsage: ndasadmin request -s SlotNumber\n" },
    {"probe", ndc_probe,
        "\tShow the list of netdisks on the network.\n"
        "\tUsage: ndasadmin probe\n" }
/*    {"help", ndc_help, NULL}*/
};

#define ND_CMD_COUNT (sizeof(ndasadmin_cmd)/sizeof(ndasadmin_cmd[0]))

/*
 * Verify if the NDAS id format is valid
 */
int verify_ndasid(char *ndas_id)
{
    int i, j;

    if (strlen(ndas_id) != 23 && strlen(ndas_id) != 29) {
        return -1;
    } 
    if (ndas_id[5] != '-' || ndas_id[11] != '-' || ndas_id[17] != '-') 
    {
        return -1;
    }
    if ((strlen(ndas_id) == 29)  && (ndas_id[23] != '-')) {
        return -1;
    }
    for (i = 0; i < (strlen(ndas_id)/5); i++) {
        for (j = 0; j < 5; j++) {
            if (isalnum(ndas_id[i*6+j]) == 0) {
                return -1;
            } 
        }
    }
    return 0;
}

int verify_ndas_serial(char *serial)
{
    int i;

    if (strlen(serial) != NDAS_SERIAL_SHORT_LENGTH && strlen(serial) != NDAS_SERIAL_EXTEND_LENGTH) {
        return -1;
    } 

    for (i = 0;i<strlen(serial);i++) {
        if (!isdigit(serial[i])) {
                return -1;
        }
    }
    return 0;
}



/* 
 * cat /proc/ndas > /etc/ndas.conf
 */
static int save_registration_info() {
    int ifd,ofd,c, err = 0;
    char buf[1024];
    dbg_ndadmin(1,"err=%d", err);

    ifd = open("/proc/ndas/regdata",O_NDELAY|O_RDONLY);
    dbg_ndadmin(1,"ifd=%d", ifd);
    if ( ifd < 0 ) return ifd;

    ofd = open("/etc/ndas.conf", O_CREAT|O_NDELAY|O_TRUNC|O_WRONLY, 0600);
    dbg_ndadmin(1,"ofd=%d", ofd);
    if ( ofd < 0 ) {
        goto out;
    }
    do {
        c = read(ifd, buf, sizeof(buf));
        dbg_ndadmin(1,"c=%d", c);
        if ( c == 0 ) break;
        if ( c < 0 ) {
            err = c;
            break;
        }
        err = write(ofd,buf,c);
        dbg_ndadmin(1,"write:err=%d", err);
        if ( err == c ) continue;
        break;
    } while(1);
    close(ofd);
out:    
    close(ifd);
    return err;
}
/* 
 * cat /etc/ndas.conf > /proc/ndas
 */
static int restore_registration_info() 
{
    int ifd,ofd,c, err = 0;
    char buf[1024];
    ifd = open("/etc/ndas.conf",O_NDELAY|O_RDONLY);
    if ( ifd < 0 ) return ifd;
    ofd = open("/proc/ndas/regdata", O_NDELAY|O_WRONLY);
    if ( ofd < 0 ) 
    {
        err = ofd;
        goto out;
    }
    do {
        c = read(ifd, buf, sizeof(buf));
        if ( c == 0 ) {
            err = 0;
            break;
        }
        if ( c < 0 ) {
            err = c;
            break;
        }
        err = write(ofd,buf,c);
        if ( err == c ) 
            continue;
        break;
    } while(1);
    close(ofd);
out:    
    close(ifd);
    return err;
}

#ifdef NDAS_MSHARE
//
//	{"diskinfo", ndc_diskinfo,
//		"\tUsage : ndasadmin diskinfo slotnumber\n"},
//

/*
#define MAX_DISC_INFO 120
typedef struct _DISK_information {
    short partinfo[NDAS_NR_PARTITION];
    int     discinfo[MAX_DISC_INFO];
    int     count;
}DISK_information, *PDISK_information;

typedef struct _ndas_ioctl_juke_disk_info {
    IN	int				slot;
    IN	PDISK_information	diskInfo;
    OUT 	ndas_error_t 		error;	    	
}ndas_ioctl_juke_disk_info_t, * ndas_ioctl_juke_disk_info_ptr;
*/
#define EM_DISC_STATUS_ERASING			0x00000001
#define EM_DISC_STATUS_WRITING			0x00000002
#define EM_DISC_STATUS_VALID				0x00000004
#define EM_DISC_STATUS_VALID_END			0x00000008

static int do_ndc_diskinfo(ndas_ioctl_juke_disk_info_ptr arg){
	int err = -1;

	// for output
	int i = 0;
	PDISK_information pdiskinfo = NULL;

	fprintf(stderr,"do_ndc_diskinfo : POSE of diskinfo %p\n", arg->diskInfo);
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_GET_DISK_INFO, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_GET_DISK_INFO err_code = %d\n", arg->error);
		return err;
	}
	if(arg->error < 0){
		return arg->error;
	}

	if(arg->queryType == 1)
	{
		return arg->error;
	}

	// for output
	pdiskinfo = arg->diskInfo;
	if(pdiskinfo){
		int discnum = -1;
		fprintf(stderr, "[current partition information]\n");
		for(i = 0; i< 8; i++)
		{
			discnum = pdiskinfo->partinfo[i];
			fprintf(stderr,"partinfo[%d] --> %d\n", i, discnum);
			fprintf(stderr,"partnum %d : %s : discnum %d\n", i, (discnum == -1)?"empty":"enabled", (discnum == -1)?-1:discnum);
		}

		fprintf(stderr,"[current disc information]\n");
		for(i = 0; i< pdiskinfo->count; i++)
		{
			if(pdiskinfo->discinfo[i] & (EM_DISC_STATUS_VALID | EM_DISC_STATUS_VALID_END))
			{
				fprintf(stderr,"DISC[%d] : VALID STATUS\n",i);
			}else{
				fprintf(stderr,"DISC[%d] : INVALID STATUS\n",i);
			}
		}
	}
	return err;
}

static int ndc_diskinfo(int argc, char* argv[])
{
	int err = -1;
	int slotnumber = 0;
	PDISK_information pdiskinfo;
	ndas_ioctl_juke_disk_info_t arg;
	char ** spp = NULL;

	if(argc < 2){
		cmd_usage(argv[0]);
	}
	argc--;
	argv++;

	spp = argv;
	slotnumber = atoi(*spp++);
	pdiskinfo = (PDISK_information)malloc(sizeof(DISK_information));
	if(!pdiskinfo){
		fprintf(stderr, "ndc_diskinfo : can't alloc DISK_information\n");
		return -1;
	}
	arg.slot = slotnumber;
	arg.diskInfo = pdiskinfo;
	err = do_ndc_diskinfo(&arg);
	free(pdiskinfo);
	return err;
}


//
//	{"discinfo", ndc_discinfo,
//		"\tUsage : ndasadmin discinfo slotnumber discnumber\n"},
//

/*
IOCTL_NDCMD_MS_GET_DISC_INFO
typedef struct 	_DISC_INFO
{
	unsigned char		title_name[NDAS_MAX_NAME_LENGTH];
	unsigned char		additional_information[NDAS_MAX_NAME_LENGTH];
	unsigned char		key[NDAS_MAX_NAME_LENGTH];
	unsigned char		title_info[NDAS_MAX_NAME_LENGTH];
}DISC_INFO, *PDISC_INFO;

typedef struct _ndas_ioctl_juke_disc_info {
    IN	int			slot;
    IN	int			selected_disc;
    IN	PDISC_INFO	discInfo;
    OUT 	ndas_error_t 	error;	    	
}ndas_ioctl_juke_disc_info_t, *ndas_ioctl_juke_disc_info_ptr;
*/
static int do_ndc_discinfo(ndas_ioctl_juke_disc_info_ptr arg)
{
	int err = -1;

	// for output
	PDISC_INFO pdiscinfo = NULL;
	
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_GET_DISC_INFO, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_GET_DISC_INFO err_code = %d\n", arg->error);
		return err;
	}
	if(arg->error < 0){
		return arg->error;
	}

	// for output
	pdiscinfo = arg->discInfo;
	if(pdiscinfo){
		fprintf(stderr, "TITLE : %s\n",pdiscinfo->title_name);
		fprintf(stderr, "ADD_INFO: %s\n", pdiscinfo->additional_information);
	}
	return err;
}

static int ndc_discinfo(int argc, char* argv[])
{
	int err = -1;
	int slotnumber = 0;
	int discnumber = 0;
	
	PDISC_INFO pdiscinfo;
	ndas_ioctl_juke_disc_info_t arg;
	char ** spp = NULL;

	if(argc < 3){
		cmd_usage(argv[0]);
	}
	argc--;
	argv++;

	spp = argv;
	slotnumber = atoi(*spp++);
	discnumber = atoi(*spp++);
	
	pdiscinfo = (PDISC_INFO)malloc(sizeof(DISC_INFO));
	if(!pdiscinfo){
		fprintf(stderr, "ndc_discinfo : can't alloc DISC_INFO\n");
		return -1;
	}
	arg.slot = slotnumber;
	arg.selected_disc = discnumber;
	arg.discInfo = pdiscinfo;
	err = do_ndc_discinfo(&arg);
	free(pdiscinfo);
	return err;
}

//
//	{"seldisc", ndc_seldisc,
//		"\tUsage : ndasadmin seldisc slotnumber discnumber\n"},
//


/*
IOCTL_NDCMD_MS_SETUP_READ

typedef struct _ndas_ioctl_juke_read_start{
    IN	int			slot;	
    IN	unsigned int selected_disc;
    OUT 	int	partnum;			// allocated stream id --> linux version allow 8 stream
    OUT 	ndas_error_t 	error;				// error
}ndas_ioctl_juke_read_start_t, *ndas_ioctl_juke_read_start_ptr;
*/
static int do_ndc_seldisc(ndas_ioctl_juke_read_start_ptr arg)
{
	int err = -1;
	
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_SETUP_READ, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_SETUP_READ err_code = %d\n", arg->error);
		return err;
	}
	if(arg->error < 0){
		return arg->error;
	}

	// for output
	if(arg->partnum != 0 && arg->partnum < 8)
	{
		fprintf(stderr, "ALLOCATED PARTNUM [%d]\n",arg->partnum);
	}else{
		err = -1;
		fprintf(stderr, "Invalid partnum %d\n", arg->partnum);
	}
	return err;
}

static int ndc_seldisc(int argc, char* argv[])
{
	int err = -1;
	int slotnumber = 0;
	int discnumber = 0;
	
	ndas_ioctl_juke_read_start_t arg;
	char ** spp = NULL;

	if(argc < 3){
		cmd_usage(argv[0]);
	}
	argc--;
	argv++;

	spp = argv;
	slotnumber = atoi(*spp++);
	discnumber = atoi(*spp++);
	
	arg.slot = slotnumber;
	arg.selected_disc = discnumber;
	err = do_ndc_seldisc(&arg);
	return err;
}


/*

	{"deseldisc", ndc_deseldisc,
		"\tUsage : nadsadmin deseldisc slotnumber discnumber partnum\n"},

IOCTL_NDCMD_MS_END_READ

typedef struct _ndas_ioctl_juke_read_end{
    IN	int			slot;	
    IN	int			partnum;
    IN	unsigned int	selected_disc;		// selected disc index
    OUT 	ndas_error_t 	error;				// error
}ndas_ioctl_juke_read_end_t, *ndas_ioctl_juke_read_end_ptr;
*/	
static int do_ndc_deseldisc(ndas_ioctl_juke_read_end_ptr arg)
{
	int err = -1;
	
	fprintf(stderr, "ALLOCATED PARTNUM [%d]\n",arg->partnum);
	
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_END_READ, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_END_READ err_code = %d\n", arg->error);
		return err;
	}
	
	if(arg->error < 0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_END_READ err_code = %d\n", arg->error);
		return arg->error;
	}
	return err;
}



static int ndc_deseldisc(int argc, char* argv[])
{
	int err = -1;
	int slotnumber = 0;
	int discnumber = 0;
	int partnum = 0;
	
	ndas_ioctl_juke_read_end_t arg;
	char ** spp = NULL;

	if(argc < 4){
		cmd_usage(argv[0]);
	}
	argc--;
	argv++;

	spp = argv;
	slotnumber = atoi(*spp++);
	discnumber = atoi(*spp++);
	partnum = atoi(*spp++);
	
	arg.slot = slotnumber;
	arg.selected_disc = discnumber;
	arg.partnum = partnum;
	err = do_ndc_deseldisc(&arg);
	return err;
}

/*
	{"rcopy", ndc_rcopy,
		"\tUsage : ndasadmin rcopy slotnumber discnumber DVD_devfile\n"},

IOCTL_NDCMD_MS_SETUP_WRITE
IOCTL_NDCMD_MS_END_WRITE
		
typedef struct _ndas_ioctl_juke_write_start{
    IN	int			slot;
    IN	unsigned int	selected_disc;		// selected disc index
    IN	unsigned int	nr_sectors;			// size of disc
    IN	unsigned int	encrypted;			// need encrypt
    IN	unsigned char HINT[32];			// encrypt hint
    OUT 	int	partnum;			// allocated stream id --> linux version allow 8 stream
    OUT 	ndas_error_t 	error;				// error
}ndas_ioctl_juke_write_start_t, *ndas_ioctl_juke_write_start_ptr;


typedef struct 	_DISC_INFO
{
	unsigned char		title_name[NDAS_MAX_NAME_LENGTH];
	unsigned char		additional_information[NDAS_MAX_NAME_LENGTH];
	unsigned char		key[NDAS_MAX_NAME_LENGTH];
	unsigned char		title_info[NDAS_MAX_NAME_LENGTH];
}DISC_INFO, *PDISC_INFO;

typedef struct _ndas_ioctl_juke_write_end{
    IN	int			slot;	
    IN	int			partnum;
    IN	unsigned int	selected_disc;		// selected disc index
    IN	PDISC_INFO	discInfo;
    OUT 	ndas_error_t 	error;				// error
}ndas_ioctl_juke_write_end_t, *ndas_ioctl_juke_write_end_ptr;
		
*/

static int do_writing_disc_start(ndas_ioctl_juke_write_start_ptr arg,int slotnumber, int discnumber, PDISC_INFO discInfo, unsigned int total_count)
{
	int err = -1;
	
	arg->slot = slotnumber;
	arg->selected_disc = discnumber;
	arg->nr_sectors = total_count;
//	arg->encrypted = 1;
	arg->encrypted = 0;
	memcpy(arg->HINT, discInfo->title_name, 32);
	arg->HINT[31] = '\0';

	err = ioctl(ndas_fd,IOCTL_NDCMD_MS_SETUP_WRITE, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_SETUP_WRITE err_code = %d\n", arg->error);
		return err;
	}
	fprintf(stderr, "allocated partnumber %d\n", arg->partnum);
	if(arg->partnum == 0 || arg->partnum > 8) {
		fprintf(stderr, "invalid partnum %d\n", arg->partnum);
		return -1;
	}
	return 0;
}


static int do_writing_disc_end(ndas_ioctl_juke_write_end_ptr arg, int slotnumber, int discnumber, int partnum, PDISC_INFO discInfo)
{
	int err = -1;
	time_t tm;
	
	arg->slot = slotnumber;
	arg->partnum = partnum;
	arg->selected_disc = discnumber;
	arg->discInfo = discInfo;

	time(&tm);
	sprintf(discInfo->additional_information, "create time %d", tm);
	
	err = ioctl(ndas_fd,IOCTL_NDCMD_MS_END_WRITE, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_SETUP_WRITE err_code = %d\n", arg->error);
		return err;
	}	

	return 0;
	
}

static int do_ndc_rcopy(int slotnumber, int discnumber, char *disc_path)
{
	return -1;
}

static int ndc_rcopy(int argc, char* argv[])
{
	int err = -1;
	int slotnumber = 0;
	int discnumber = 0;
	char * disc_path;
	
	
	char ** spp = NULL;

	if(argc < 4){
		cmd_usage(argv[0]);
	}
	argc--;
	argv++;

	spp = argv;
	slotnumber = atoi(*spp++);
	discnumber = atoi(*spp++);
	disc_path = strdup(*spp++);
	
	err = do_ndc_rcopy(slotnumber, discnumber, disc_path);
	free(disc_path);
	return err;
}

/*
	{"del", ndc_del,
		"\tUsage : ndasadmin del slotnumber discnumber\n"},

IOCTL_NDCMD_MS_DEL_DISC
		
typedef struct _ndas_ioctl_juke_del{
    IN	int			slot;	
    IN	unsigned int	selected_disc;		// selected disc index
    OUT 	ndas_error_t 	error;
}ndas_ioctl_juke_del_t, * ndas_ioctl_juke_del_ptr;
		
*/	
static int do_ndc_del(ndas_ioctl_juke_del_ptr arg)
{
	int err = -1;
	
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_DEL_DISC, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_DEL_DISC err_code = %d\n", arg->error);
		return err;
	}
	
	if(arg->error < 0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_DEL_DISC err_code = %d\n", arg->error);
		return arg->error;
	}
	return err;
}

static int ndc_del(int argc, char* argv[])
{
	int err = -1;
	int slotnumber = 0;
	int discnumber = 0;
	
	ndas_ioctl_juke_del_t arg;
	char ** spp = NULL;

	if(argc < 3){
		cmd_usage(argv[0]);
	}
	argc--;
	argv++;

	spp = argv;
	slotnumber = atoi(*spp++);
	discnumber = atoi(*spp++);
	
	arg.slot = slotnumber;
	arg.selected_disc = discnumber;
	err = do_ndc_del(&arg);
	return err;
}

/*
	{"validate", ndc_validate,
		"\tUsage : ndasadmin validate slotnumber discnumber DVD_devfile\n"},

IOCTL_NDCMD_MS_VALIDATE_DISC



typedef struct _ndas_ioctl_juke_validate {
    IN	int			slot;	
    IN	unsigned int	selected_disc;		// selected disc index
    OUT 	ndas_error_t 	error;	
}ndas_ioctl_juke_validate_t, *ndas_ioctl_juke_validate_ptr;

*/
static int do_ndc_validate(int slotnumber, int discnumber, char *dvd_path)
{
	return ;
}
	
static int ndc_validate(int argc, char* argv[])
{
	int err = -1;
	int slotnumber = 0;
	int discnumber = 0;
	char *dvd_path = NULL;

	char ** spp = NULL;

	if(argc < 4){
		cmd_usage(argv[0]);
	}
	argc--;
	argv++;

	spp = argv;
	slotnumber = atoi(*spp++);
	discnumber = atoi(*spp++);
	dvd_path = strdup(*spp++);
	
	err = do_ndc_validate(slotnumber, discnumber, dvd_path);
	free(dvd_path);
	return err;
}

/*
	{"format", ndc_format,
		"\tUsage : ndasadmin format slotnumber\n"}

IOCTL_NDCMD_MS_FORMAT_DISK

typedef struct _ndas_ioctl_juke_format {
    IN	int			slot;
   OUT 	ndas_error_t 	error;	    
}ndas_ioctl_juke_format_t, * ndas_ioctl_juke_format_ptr;
	
*/

static int do_ndc_format(ndas_ioctl_juke_format_ptr arg)
{
	int err = -1;
	
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_FORMAT_DISK, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_FORMAT_DISK err_code = %d\n", arg->error);
		return err;
	}
	
	if(arg->error < 0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_FORMAT_DISK err_code = %d\n", arg->error);
		return arg->error;
	}
	return err;
}

static int ndc_format(int argc, char* argv[])
{
	int err = -1;
	int slotnumber = 0;
	
	ndas_ioctl_juke_format_t arg;
	char ** spp = NULL;

	if(argc < 3){
		cmd_usage(argv[0]);
	}
	argc--;
	argv++;

	spp = argv;
	slotnumber = atoi(*spp++);
	
	arg.slot = slotnumber;
	err = do_ndc_format(&arg);
	return err;
}

	
#endif

static int ndc_start(int argc, char* argv[]) {
    int err;
    err = ioctl(ndas_fd, IOCTL_NDCMD_START, NULL);
    if ( err != 0 ) 
    {
        fprintf(stderr,"start: error occurred\n");
        return -1;
    }
    err = restore_registration_info();

    if (err!=0) {
        // To do: Need to differentiate the unsupported release from error
//        printf("Failed to restore registration information.\n");
    }
    
    printf("NDAS driver is initialized.\n");        
    return 0;
}
static int ndc_stop(int argc, char* argv[]) {
    int err;

    save_registration_info();

    err = ioctl(ndas_fd, IOCTL_NDCMD_STOP, NULL);
    if ( err != 0 ) 
    {
        fprintf(stderr,"ndas: Failed to stop. Check whether NDAS device is in use.\n");
        return -1;
    }
    printf("NDAS driver is stopped\n");        
    return 0;
}

static int ndc_register(int argc, char *argv[]) {
    int c, i, err, option_index, ret = 0, ndas_id_arg_len = 0;
    char        *ndas_id_arg = NULL;
    int serial_reg = 0;
    ndas_ioctl_register_t arg = {0};
    const char    delimiter[] = "-";
    char* id[5] = { 0,0,0,0,0 };
    static struct option long_options[] = {
       {"name", 1, NULL, 'n'},
       {"background", 0, NULL, 'b'},        
       {0, 0, 0, 0}
    };
    if (argc<2) {
        cmd_usage(argv[0]);
        return 0;
    }
    dbg_ndadmin(4, "ndc_register: Entered...");

    ndas_id_arg = strdup(*(argv+1));
    ndas_id_arg_len = strlen(ndas_id_arg);

    dbg_ndadmin(4,  "length %d\t string %s", strlen(ndas_id_arg), ndas_id_arg);

    if (verify_ndasid(ndas_id_arg) == 0) {
        // Valid ID/key is given
    } else {
        // Check input is serial number
        if (verify_ndas_serial(ndas_id_arg) ==0) {
            serial_reg = 1;   
        } else {
            fprintf(stderr,"%s: invalid NDAS ID or serial\n", *argv);
            ret = -1;
            goto out;
        }
    }

    if (serial_reg) {
        memcpy(arg.ndas_idserial, ndas_id_arg, NDAS_SERIAL_LENGTH+1);
        arg.ndas_idserial[NDAS_SERIAL_LENGTH] = 0;
        arg.use_serial = 1;
    } else {
        i = 0;
        id[i++] = strtok(ndas_id_arg, delimiter);
        while ((id[i] = strtok(NULL, delimiter)) != NULL) {
            i++;
        }
        
        if (ndas_id_arg_len == 23) {
            id[4] = (char *)malloc(6);
            id[4][0] = '\0';
        }
        memcpy(arg.ndas_idserial, id[0], 5);
        memcpy(arg.ndas_idserial + 5, id[1], 5);
        memcpy(arg.ndas_idserial + 10, id[2], 5);
        memcpy(arg.ndas_idserial + 15, id[3], 5);
        memcpy(arg.ndas_key, id[4], 5);
        for(i=0;i<NDAS_ID_LENGTH;i++) {
            arg.ndas_idserial[i] = toupper(arg.ndas_idserial[i]);
        }
        for(i=0;i<NDAS_KEY_LENGTH;i++) {
            arg.ndas_key[i] = toupper(arg.ndas_key[i]);
        }
        arg.ndas_idserial[NDAS_ID_LENGTH] = 0;
        arg.ndas_key[NDAS_KEY_LENGTH] = 0;
        arg.use_serial = 0;
    }

    arg.discover = 1;
    
    arg.name[0] = 0;
    
    while ((c = getopt_long(argc-1, argv+1, "n:b", long_options, &option_index)) != EOF) 
    {
        if ( c == '?' ) {
            cmd_usage(argv[0]);
            ret = -1;    
            goto out;
        }
        if (c == 'n') {
            if (strnlen(optarg, NDAS_MAX_NAME_LENGTH) >= NDAS_MAX_NAME_LENGTH) {
                fprintf(stderr,"%s: \"%s\" is too long\n", *argv, arg.name);
                fprintf(stderr,"%s: The length of name should be less than %d\n",*argv,NDAS_MAX_NAME_LENGTH);
                ret = -1;
                goto out;
            }
            strncpy(arg.name,optarg,NDAS_MAX_NAME_LENGTH);
        } else if (c == 'b') {
            /* Run discover in background. */
            arg.discover = 0;
        } else {
            fprintf(stderr,"%s: unknown option \"-%c\"", *argv, c);
            cmd_usage(argv[0]);
            ret = -1;    
            goto out;
        }
    }
    if ( arg.name[0] == 0 ) 
    {
        cmd_usage(argv[0]);
        ret = -1;
        goto out;
    }
    if ( (ret = ioctl(ndas_fd, IOCTL_NDCMD_REGISTER, &arg)) != 0 ) 
    {
        switch(arg.error) {
        case NDAS_ERROR_ALREADY_REGISTERED_DEVICE:
            fprintf(stderr,"%s: already registered device.\n", *argv);
            //ret = FALSE;
            break;
        case NDAS_ERROR_INVALID_NDAS_ID:
            fprintf(stderr,"%s: invalid NDAS ID.\n", *argv);
            //ret = FALSE;
            break;
        case NDAS_ERROR_INVALID_NDAS_KEY:
            fprintf(stderr,"%s: invalid NDAS KEY.\n", *argv);
            //ret = FALSE;
            break;
        case NDAS_ERROR_INVALID_PARAMETER:
            fprintf(stderr,"%s: invalid serial.\n", *argv);
            //ret = FALSE;
            break;
        case NDAS_ERROR_ALREADY_REGISTERED_NAME:
            fprintf(stderr,"%s: already registered name.\n", *argv);
            //ret = FALSE;
            break; 
        default:
            fprintf(stderr,"%s: error(%d) occurred\n", *argv, arg.error);
            //ret = FALSE;
        }
        dbg_ndadmin(1,"fail to ioctl=%d",arg.error);
        goto out;
    }
    err = save_registration_info();
    printf("\n'%s' is registered successfully.\n", arg.name);
    printf("Please find the slot # by\n");
    printf("\ncat /proc/ndas/devices/%s/slots\n", arg.name);
    printf("\nThen execute the following command to enable the slot\n");
    printf("\nFor read-only mode");
    printf("\nndasadmin enable -s <slot#> -o r\n");
    printf("\nFor exclusive-write mode");
    printf("\nndasadmin enable -s <slot#> -o w\n");
    printf("\nFor GFS mode");
    printf("\nndasadmin enable -s <slot#> -o s\n");
out:
    if (ndas_id_arg != NULL)
        free(ndas_id_arg);

    if (ndas_id_arg_len == 23) free(id[4]);

    return ret;
}

static int ndc_unregister(int argc, char *argv[]) {
    ndas_ioctl_unregister_t arg;
    int option_index, c, err;
    static struct option long_options[] = {
       {"name", 1, 0, 0},
       {0, 0, 0, 0}
    };
    arg.name[0] = 0;
    while ((c = getopt_long(argc, argv, "n:", long_options, &option_index)) != EOF) 
    {
        if (c != 0 && c != 'n' ) {
            fprintf(stderr,"%s: unknown option \"-%c\"", *argv, c);
            cmd_usage(argv[0]);
            return -1;
        }            
        if (strnlen(optarg, NDAS_MAX_NAME_LENGTH) >= NDAS_MAX_NAME_LENGTH) 
        {
            fprintf(stderr,"%s: \"%s\" is too long.\n", *argv, arg.name);
            fprintf(stderr,"%s: name should be less than %d.\n",*argv,NDAS_MAX_NAME_LENGTH);
            return -1;
        }
        strncpy(arg.name, optarg,NDAS_MAX_NAME_LENGTH);
    }
    if ( arg.name[0] == 0 ) {
        cmd_usage(argv[0]);
        return -1;
    }
    if ( ioctl(ndas_fd, IOCTL_NDCMD_UNREGISTER, &arg) != 0 ) {
        switch(arg.error){
        case NDAS_ERROR_INVALID_NAME:
            fprintf(stderr,"%s: \"%s\" does not exist.\n", *argv, arg.name);
            return -1;
        case NDAS_ERROR_ALREADY_ENABLED_DEVICE:
            fprintf(stderr,"%s: The unit device(s) of \"%s\" is enabled. disable it(them) first.\n", *argv, arg.name);
            return -1;
        default:
            fprintf(stderr,"%s: An error(%d) occurred\n", *argv, arg.error);
            return -1;
        }
    }
    err = save_registration_info();
    printf("The NDAS device - \"%s\" is unregistered successfully\n", arg.name);
    return 0;
}
int get_slot_from_block(char *cmd,char* path)
{
    struct stat st;
    int ret = lstat(path, &st);
    if ( ret != 0 ) {
        fprintf(stderr,"%s: can\'t open %s\n", cmd, path); 
        return ret;
    }
    if ( !S_ISBLK(st.st_mode) ) {
        fprintf(stderr,"%s: not a block device %s\n", cmd, path); 
        return -1;
    }
    return NDA_MAJOR((int)st.st_rdev) - NDAS_BLK_MAJOR + NDAS_FIRST_SLOT_NR;
}

int procfs_read_number(char *path, int *number)
{
    FILE *fd;
    char buf[20], *p;
    
    fd = fopen(path, "r");
    if ( fd == NULL ) {
        return -1;       
    }
	if(fgets(buf, sizeof(buf),fd) == NULL ) {
        fclose(fd);
        return -2;
    }
	*number = strtol(buf, &p, 0);
    if ( p <= buf ) {
        fclose(fd);
        return -3;
    }
    fclose(fd);
    return 0;
}
int slot_get_unit(int slot)
{
    FILE *fd;
    char buf[255];
    int ret;
    
    snprintf(buf, sizeof(buf), "/proc/ndas/slots/%d/unit", slot);
    if ( procfs_read_number(buf, &ret) < 0 ) {
        fprintf(stderr, "Fail to get slot information from %s\n", buf);
        return -1;   
    }
    return ret;
}

int slot_get_devname(int slot, char* devname, int size)
{
    FILE *fd;
    char buf[255];
    char *ret;
    
    snprintf(buf, sizeof(buf), "/proc/ndas/slots/%d/devname", slot);
    fd = fopen(buf, "r");
    if ( fd == NULL) {
        goto err;        
    }
    ret = fgets(devname, size, fd);
    if(ret == NULL ) {
        fclose(fd);
        goto err;        
    }
    fclose(fd);
    return 0;
err:
    fprintf(stderr, "Fail to get ndas information from %s\n", buf);
    return -1;   
}

int is_2_6_kernel() 
{
    struct utsname ut;
    if ( uname(&ut) != 0 ) {
        return -1;
    }
    dbg_ndadmin(3, "release=%s", ut.release);
    return strncmp(ut.release, "2.6", 3) == 0  ? 1 : 0;
}

static int ndc_enable(int argc, char *argv[]) 
{
    ndas_ioctl_enable_t arg;
    char *p;
    int c;
    dbg_ndadmin(3, "ing");

    arg.slot = NDAS_FIRST_SLOT_NR - 1;
    arg.write_mode = -1;
    while ((c = getopt(argc, argv, "n:s:o:f:")) != EOF)
    {
        if ( c == '?' ) {
            cmd_usage(argv[0]);
            return -1;
        }
        if ( c == 'n' || c == 's') {
            arg.slot = strtol(optarg, &p, 0);
            if ( p <= optarg ) {
                fprintf(stderr,"%s: invalid slot format %s\n", *argv, optarg); 
                cmd_usage(argv[0]);
                return -1;
            }
            continue;
        }
        if ( c == 'f' ) {
            arg.slot = get_slot_from_block(*argv,optarg);
            if ( arg.slot < NDAS_FIRST_SLOT_NR ) {
                cmd_usage(argv[0]);
                return -1;
            }
            continue;
        }
        if ( c != 'o' ) {
            fprintf(stderr,"%s: unknown option \"-%c\"", *argv, c);
            cmd_usage(argv[0]);
            return -1;
        }
        if ( optarg[0] == 'w' && optarg[1] == '\0') {
            arg.write_mode = 1;
            dbg_ndadmin(3,"READ-WRITE");
        } else if ( optarg[0] == 'r' && optarg[1] == '\0') {
            arg.write_mode = 0;
            dbg_ndadmin(3,"READ-ONLY");
        } else if (optarg[0] == 's' && optarg[1] == '\0') {
            arg.write_mode = 2; 
            dbg_ndadmin(3,"SHARED-WRITE");
            fprintf(stderr,"WARNING: shared-write mode is only useful for clustering file system and is not recommended for other file systems\n");
        } else {
            fprintf(stderr,"%s: unknown access-mode \'%s\'", *argv, optarg);
            cmd_usage(argv[0]);
            return 0;
        }
    }
    if ( arg.slot == NDAS_FIRST_SLOT_NR - 1 || arg.write_mode == -1) 
    {
        cmd_usage(argv[0]);
        return -1;
    }
    dbg_ndadmin(3, "ioctl-ing arg.slot=%d", arg.slot);
    if ( ioctl(ndas_fd,IOCTL_NDCMD_ENABLE,&arg) != 0 ) {
        switch ( arg.error ) {
        case NDAS_ERROR_TOO_MANY_INVALID:
            fprintf(stderr,"%s: Too many defective disks."
                " At lease %d disk(s) should be valid to enable."
                " you can validate the disk by\n"
                " ndasadmin validate -s [slot]\n", *argv, 1);
            break;
        case NDAS_ERROR_UNSUPPORTED_DISK_MODE:
            fprintf(stderr,"%s: is a member of NDAS raid\n", *argv);
            break;
        case NDAS_ERROR_TIME_OUT:
            fprintf(stderr,"%s: timed out to connect NDAS device\n", *argv);
            break;
        case NDAS_ERROR_ALREADY_ENABLED_DEVICE:
#ifdef NDAS_DEVFS        
            fprintf(stderr,"%s: block device /dev/nd/disc%d is already enabled\n", arg.slot - NDAS_FIRST_SLOT_NR);
#else
            if ( is_2_6_kernel() == 1 ) {
                char devname[32];
                int ret;
                ret = slot_get_devname(arg.slot, devname, sizeof(devname));
                if (ret <0) {
                    fprintf(stderr, "%s: Unable to get the path of block device\n", *argv);
                    break;
                }
                fprintf(stderr, "%s: Block device /dev/%s is already enabled.\n", *argv, devname);
        	} else {
                    fprintf(stderr,"%s: block device /dev/nd%c is already enabled\n", *argv, 'a' + arg.slot - 1);
        	}
#endif            
            break;
        case NDAS_ERROR_INVALID_SLOT_NUMBER:
            fprintf(stderr,"%s: slot %d does not exist\n", *argv, arg.slot);
            break;
        case NDAS_ERROR_OUT_OF_MEMORY:
            fprintf(stderr,"%s: out of memory to enable the slot %d\n", *argv, arg.slot);
            break;
        case NDAS_ERROR_NOT_ONLINE:
        case NDAS_ERROR_TOO_MANY_OFFLINE:
            fprintf(stderr,"%s: device for slot %d is not on the network\n", *argv, arg.slot);
            break;
        case NDAS_ERROR_TRY_AGAIN:
            fprintf(stderr,"%s: slot %d is being changed. try again later\n", *argv, arg.slot);
            break;
        case NDAS_ERROR_NO_WRITE_ACCESS_RIGHT: 
            /* when the device unplugged and plug back in 10 sec, this error happens.
               display the correct message for such a case */
            fprintf(stderr,"%s: You did not provide a valid write key or slot %d is exclusively accessed by other host.\n"
                "Execute the following command to send the request for the write permission.\n"
                "ndasadmin request -s %d\n", *argv, arg.slot, arg.slot);
            break;
        case NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION:
            fprintf(stderr,"%s: this NDAS device does not support the requested access mode.\n", *argv);
            break;
        default:
            fprintf(stderr,"%s: error(%d) occurred\n", *argv, arg.error);
        }
        return -1;    
    }
    if ( arg.error == NDAS_OK ) {
        char serial[NDAS_SERIAL_LENGTH + 1];
        save_registration_info();

        if ( is_2_6_kernel() == 1 ) {
            char devname[32];
            int ret;
            ret = slot_get_devname(arg.slot, devname, sizeof(devname));
            if (ret <0) {
                printf("Slot %d was ready to use, but unable to get the path of block device\n", arg.slot);
                return -1;
            }
            printf("Block device /dev/%s is ready to use.\n", devname);
        } else {
#ifdef NDAS_DEVFS        
            printf("Block device /dev/nd/disc%d is ready to use.\n", arg.slot - NDAS_FIRST_SLOT_NR);
#else
            printf("Block device /dev/nd%c is ready to use.\n", 'a' + arg.slot - NDAS_FIRST_SLOT_NR);
#endif
        }
        return 0; 
    }
    printf("Slot %d may be a member of RAID disk.\n"
        "To enable the raid, excute the following command\n"
        "ndasadmin enable -s %d -o [\'r\'|\'w\']\n", arg.slot, arg.error);
    return 0;
        
}

static int ndc_disable(int argc, char *argv[]) {
    ndas_ioctl_arg_disable_t arg;
    int c;
    char *p;
    char devname[32];
    int ret;
    
    arg.slot = NDAS_FIRST_SLOT_NR - 1;
    while ((c = getopt(argc, argv, "n:s:f:")) != EOF)
    {
        switch(c) {
        case '?':
            cmd_usage(argv[0]);
            return -1;
        case 'n': /* deprecated */
        case 's':
            arg.slot = strtol(optarg, &p, 0);
            if ( p <= optarg ) {
                fprintf(stderr,"%s: invalid slot format %s\n", *argv, optarg); 
                cmd_usage(argv[0]);
                return -1;
            }
            if ( arg.slot <= NDAS_FIRST_SLOT_NR - 1 ) {
                fprintf(stderr,"%s: invalid slot number %d\n", *argv, arg.slot);
                cmd_usage(argv[0]);
                return -1;
            }
            break;
        case 'f':
            arg.slot = get_slot_from_block(*argv, optarg);
            if ( arg.slot < NDAS_FIRST_SLOT_NR ) {
                cmd_usage(argv[0]);
                return -1;
            }
            break;
        default:
            fprintf(stderr,"%s: unknown option \"-%c\"", *argv, c);
            cmd_usage(argv[0]);
            return -1;
        }
    }
    if ( arg.slot == NDAS_FIRST_SLOT_NR - 1 ) {
        cmd_usage(argv[0]);
        return -1;
    }
    if ( ioctl(ndas_fd,IOCTL_NDCMD_DISABLE,&arg) != 0 ) {
        switch ( arg.error ) {
        case NDAS_ERROR_INVALID_SLOT_NUMBER:
        case NDAS_ERROR_ALREADY_DISABLED_DEVICE:
            fprintf(stderr,"%s: slot %d is not enabled\n", *argv, arg.slot );
            break;
        case NDAS_ERROR_UNSUPPORTED_DISK_MODE:
            fprintf(stderr,"%s: is a member of NDAS raid\n", *argv);
            break;    
        default:
            fprintf(stderr,"%s: error(%d) occurred\n", *argv, arg.error);
        }
        return -1;    
    }    
    save_registration_info();

    if ( is_2_6_kernel() == 1 ) {
        ret = slot_get_devname(arg.slot, devname, sizeof(devname));
        if (ret <0) {
            printf("Slot %d is disabled\n", arg.slot);
        } else {
            printf("/dev/%s is disabled\n", devname);
        }
    } else {
#ifdef NDAS_DEVFS        
        printf("/dev/nd/disc%d is disabled.\n", arg.slot - NDAS_FIRST_SLOT_NR);
#else
        printf("/dev/nd%c is disabled\n", arg.slot + 'a' - NDAS_FIRST_SLOT_NR);
#endif        
    }
    return 0; 
}

static int ndc_edit(int argc, char *argv[]) {
    int c;
    ndas_ioctl_edit_t arg;
    
    while ((c = getopt(argc, argv, "o:n:")) != EOF) 
    {
        switch(c) {
        case '?':
            cmd_usage(argv[0]);
            return -1;
        case 'o':
            if ( strnlen(optarg, NDAS_MAX_NAME_LENGTH ) == NDAS_MAX_NAME_LENGTH )
            {
                fprintf(stderr,"%s: too long name %s. name should be less than %d\n", *argv, optarg, NDAS_MAX_NAME_LENGTH);
                cmd_usage(argv[0]);
                return -1;
            }
            strncpy(arg.name, optarg, NDAS_MAX_NAME_LENGTH);
            break;
        case 'n':
            if ( strnlen(optarg, NDAS_MAX_NAME_LENGTH) == NDAS_MAX_NAME_LENGTH)
            {
                fprintf(stderr,"%s: too long name %s. name should be less than %d\n", *argv, optarg, NDAS_MAX_NAME_LENGTH);
                cmd_usage(argv[0]);
                return -1;
            }
            strncpy(arg.newname, optarg, NDAS_MAX_NAME_LENGTH);
            break;
        default:
            fprintf(stderr,"%s: invalid option \"-%c\"\n", *argv, c);
            cmd_usage(argv[0]);
            return -1;
        }
    }
    if ( ioctl(ndas_fd, IOCTL_NDCMD_EDIT, &arg) != 0 ) {
        switch ( arg.error ) {
        case NDAS_ERROR_INVALID_NAME:
            fprintf(stderr,"%s: \'%s\' is not registered\n", *argv, arg.name);
            return -1;
        default:
            fprintf(stderr,"%s: error(%d) occurred\n", *argv, arg.error);
            return -1;
        }
    }
    printf("The device registered name changed from \'%s\' into \'%s\'\n", arg.name, arg.newname);
    return 0; 
}

static int ndc_request(int argc, char *argv[]) 
{
    int c;
    char *p;
    ndas_ioctl_request_t arg;
    arg.slot = NDAS_FIRST_SLOT_NR - 1;
    while ((c = getopt(argc, argv, "s:n:")) != EOF)
    {
        switch(c) {
        case '?':
            cmd_usage(argv[0]);
            return -1;
        case 'n':
        case 's':
            arg.slot = strtol(optarg, &p, 0);
            if ( *p ) {
                fprintf(stderr,"%s: invalid digits %s\n", *argv, optarg);
                cmd_usage(argv[0]);
                return -1;
            }
            if ( arg.slot <= NDAS_FIRST_SLOT_NR - 1 ) {
                fprintf(stderr,"%s: invalid slot number %d\n", *argv, arg.slot);
                return -1;
            }
            break;
        default:     
            cmd_usage(argv[0]);
            return -1;
        }
    }
    if ( arg.slot == NDAS_FIRST_SLOT_NR - 1 ) {
        cmd_usage(argv[0]);
        return -1;
    }
    if ( ioctl(ndas_fd,IOCTL_NDCMD_REQUEST, &arg) != 0 ) {
        switch( arg.error ) {
        default:        
            fprintf(stderr,"%s: error(%d) occurred\n", *argv, arg.error);
            return -1;
        }
    }
    printf("Request for surrending the exclusive write permission sent\n");
    return 0;    
}

static int ndc_probe(int argc, char *argv[]) { 
    ndas_ioctl_probe_t arg;
    int i, retry = 0;
    char *p;
    arg.sz_array = 0;
    arg.ndasids= NULL;

    do {
        /* Get required size */
#ifdef PROBE_BY_ID
		if ( ioctl(ndas_fd, IOCTL_NDCMD_PROBE_BY_ID, &arg) != 0 ) {
#else
		if ( ioctl(ndas_fd, IOCTL_NDCMD_PROBE, &arg) != 0 ) {
#endif
            if (arg.error==NDAS_ERROR_UNSUPPORTED_FEATURE) {
                fprintf(stderr,"Probing is disabled for this software release.\n");
                return -1;
            }
            fprintf(stderr, "%s: error(%d) occured\n", *argv, arg.error);
            return -1;
        }
        if ( arg.nr_ndasid<= 0 ) {
            printf("No NDAS device is available in the network\n");
            return 0;
        }
#ifdef PROBE_BY_ID
        /* Allocate storage */
        arg.ndasids = malloc((NDAS_ID_LENGTH + 1) * arg.nr_ndasid);
        arg.sz_array = arg.nr_ndasid;
                                                                                
        if ( ioctl(ndas_fd, IOCTL_NDCMD_PROBE_BY_ID, &arg) != 0 ) {
#else
        /* Allocate storage */
        arg.ndasids = malloc((NDAS_SERIAL_LENGTH + 1) * arg.nr_ndasid);
        arg.sz_array = arg.nr_ndasid;
                                                                                
        if ( ioctl(ndas_fd, IOCTL_NDCMD_PROBE, &arg) != 0 ) {
#endif
            if ( arg.error == NDAS_ERROR_INVALID_PARAMETER ) {
                if ( retry++ < 10 )
                    continue;
            }
            fprintf(stderr, "%s: error(%d) occured\n", *argv, arg.error);
            free(arg.ndasids);
            return -1;
        }
        if ( arg.nr_ndasid <= 0 ) {
            printf("No NDAS device is available in the network at this moment\n");
            free(arg.ndasids);
            return 0;
        }
        break;
    }while(0);
    /*  ignore even if arg.sz_array != arg.nr_ndasid */
    for (i = 0; i < arg.sz_array; i++) {
        int j;
#ifdef PROBE_BY_ID
        p = arg.ndasids + i * (NDAS_ID_LENGTH + 1);
#else
        p = arg.ndasids + i * (NDAS_SERIAL_LENGTH + 1);
#endif
        printf("%s\n",p);
    }
    free(arg.ndasids);
    return 0;
}

#ifdef NDAS_MULTIDISKS
static int ndc_config(int argc, char *argv[]) { return 0; }
#endif

int cmd_usage(char* cmd)
{
    int i;
    fprintf(stderr, "NDAS Administration Tool.\n");
    if (cmd!=NULL) {
        for(i=0;i<ND_CMD_COUNT;i++) {
            if (!strncmp(ndasadmin_cmd[i].str, cmd, sizeof(ndasadmin_cmd[i].str))) {
                fprintf(stderr, "%s:\n", ndasadmin_cmd[i].str);
                fprintf(stderr, "%s", ndasadmin_cmd[i].help);
                return 0;
            }
        }
    } 
    for(i=0;i<ND_CMD_COUNT;i++) {
        fprintf(stderr, "%s:\n", ndasadmin_cmd[i].str);
        fprintf(stderr, "%s", ndasadmin_cmd[i].help);
    }
    return 0;
}

static int ndc_help(int argc, char* argv[])
{
    if (argc>=2)
        cmd_usage(argv[1]);
    else
        cmd_usage(NULL);
    return 0;
}


/* 
 * Main function
 */

int main(int argc, char* argv[])
{
    char* cmd;
    int i;
    int ret;
    int retry;
    if ( argc < 2 ) {
        ndc_help(argc, argv);
        return 0;
    }
    if ( getuid() != 0 ) {
        fprintf(stderr, "Must be executed with root privilege\n");
        return -1;
    }
    retry=0;
    do {
  	ndas_fd = open("/dev/ndas",O_NDELAY);
	if (ndas_fd<0) {
		if (retry>5) {
		        fprintf(stderr,"Failed to open /dev/ndas\n");
        		fprintf(stderr,"Check NDAS device file exists, driver module is loaded and started by administration tool\n");
        		return -2;
		}
		/* 
		    When udev is used, device file is created after ndas_block is loaded.
		    Wait a while for device file to be created
		*/
		sleep(1);
		retry++;
	} else {
		break;
	}
    } while(1);
    dbg_ndadmin(4,"main:fd=%d",ndas_fd);
    cmd = argv[1];
    argv++;
    argc--;

    for(i=0;i<ND_CMD_COUNT;i++) {
        if(!strncmp(cmd,ndasadmin_cmd[i].str, strlen(ndasadmin_cmd[i].str) + 1)) {
            ret = ndasadmin_cmd[i].func(argc, argv);
            break;
        }
    }
    if (i==ND_CMD_COUNT) {
        cmd_usage(NULL);
    }
    close(ndas_fd);
    return ret;
}

