/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2005, XIMETA, Inc., IRVINE, CA, USA.
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
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/

#ifdef NDAS_MSHARE
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
#include "ndas_api.h"
#include "../../tarball-tag/inc/ndas_id.h"
#include "../../tarball-tag/inc/ndasdev.h"
#include <time.h>
#ifdef NDAS_MSHARE_WRITE
#include "dvdcopy.h"
#endif // #ifdef NDAS_MSHARE_WRITE


#ifndef NULL
#define NULL ((void*)0)
#endif


static int get_ndas_fd()
{
    int ndas_fd = -1;
    ndas_fd = open("/dev/ndas",O_NDELAY);
    if ( ndas_fd < 0 ) {
        fprintf(stderr,"Failed to open /dev/ndas\n");
        return -1;
    }
    debug_ndapi(1,"ndas_dev :fd=%d",ndas_fd); 
    return ndas_fd;
}


/*
 * Verify if the NDAS id format is valid
 */
static int verify_ndasid(char *ndas_id)
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
            ndas_id[i*6+j] = toupper(ndas_id[i*6+j]);
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
    debug_ndapi(1,"err=%d", err);
    ifd = open("/proc/ndas/regdata",O_NDELAY|O_RDONLY);
    debug_ndapi(1,"ifd=%d", ifd);
    if ( ifd < 0 ) return ifd;

    ofd = open("/etc/ndas.conf", O_CREAT|O_NDELAY|O_TRUNC|O_WRONLY, 0600);
    debug_ndapi(1,"ofd=%d", ofd);
    if ( ofd < 0 ) {
        goto out;
    }
    do {
        c = read(ifd, buf, sizeof(buf));
        debug_ndapi(1,"c=%d", c);
        if ( c == 0 ) break;
        if ( c < 0 ) {
            err = c;
            break;
        }
        err = write(ofd,buf,c);
        debug_ndapi(1,"write:err=%d", err);
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
static int restore_registration_info() {
    int ifd,ofd,c, err = 0;
    char buf[1024];
    ifd = open("/etc/ndas.conf",O_NDELAY|O_RDONLY);
    if ( ifd < 0 ) return ifd;
    ofd = open("/proc/ndas/regdata", O_NDELAY|O_WRONLY);
    if ( ofd < 0 ) {
        goto out;
    }
    do {
        c = read(ifd, buf, sizeof(buf));
        if ( c == 0 ) break;
        if ( c < 0 ) {
            err = c;
            break;
        }
        err = write(ofd,buf,c);
        if ( err == c ) continue;
        break;
    } while(1);
    close(ofd);
out:    
    close(ifd);
    return err;
}



static char * get_string(FILE  * file_fd,char * token_buff)
{
	
	int c = 0;
	char * q = NULL;
	q = token_buff;
	
//	skip undefined char /space
	c = fgetc(file_fd);
	if(c == EOF ) return NULL;
	
	while((!isalnum(c)) &&  ((char) c != '/')){
		c = fgetc(file_fd);
		if(c == EOF) return NULL;
	}

	while( (isalnum(c))  || ((char) c == '/')){
		*q++ = (char)c;
		c = fgetc(file_fd);
		if(c == EOF) return NULL;
	}

	*q = '\0'; 
	return q;
	 
}


static int parse_devs(FILE * file_fd, char * Name, int * slot)
{
	char token[(NDAS_MAX_NAME_LENGTH)];
	char * p = NULL;
	int i = 0;
	int name_len;
	
	fprintf(stderr,"enter parse_devs\n");
	memset(token, 0, (NDAS_MAX_NAME_LENGTH));
	
	while( get_string(file_fd,token) != NULL){
		name_len = strlen(Name);
		fprintf(stderr,"token = %s\n", token);
		if(!strncmp(Name, token, name_len))
		{
			fprintf(stderr,"Name = %s\n", token);
			for(i = 0; i < 5; i++)
			{
				if(get_string(file_fd,token) == NULL){
					fprintf(stderr, "parse_devs : Parse Fail\n");
					return -1;
				}
				if(i == 0){
					fprintf(stderr, "ID= %s\n", token);
				}else if(i == 1){
					fprintf(stderr, "Key= %s\n", token);
				}else if(i == 2){
					fprintf(stderr, "Serial= %s\n", token);
				}else if(i == 3){
					fprintf(stderr, "version= %s\n", token);
				}else if(i == 4){
					fprintf(stderr, "Online= %s\n", token);
				}
			}

			if(get_string(file_fd,token) == NULL)
			{
				fprintf(stderr,"parse_devs : Parse Fail last slot\n");
				return -1;
			}

			fprintf(stderr,"Parsed slot number=%s\n", token);
			*slot = atoi(token);
			return 0;
		}
	}
	return -1;
}


int get_unit_number(int slot)
{
    FILE *fd;
    char buf[255];
    int c;
    
    snprintf(buf, 255, "/proc/ndas/slots/%d/unit", slot);
    fd = fopen(buf, "r");
    if ( fd < 0 ) {
        goto err;        
    }
    c = fgetc(fd);
	if(c == EOF ) {
        fclose(fd);
        goto err;        
    }
	
    if ( c < '0' || c > '9' ) {
        fclose(fd);
        goto err;
    }
    fclose(fd);
    return c - '0';
err:
    fprintf(stderr, "Fail to get slot information from %s\n", buf);
    return -1;
}

// Get slot information from proc/ndas/devs
int get_slot_number(char *Name,  int *slot)
{
	FILE * devs_fd = NULL;
	int c =0;
	int err = 0;
	char * buf = NULL;
	int i;
	int retry = 5;
	int result = 0;
	const char *filename_dev = "/proc/ndas/devs";

	while(1)
	{
		// read device list file
		fprintf(stderr,"reading = %s\n", filename_dev);
		devs_fd =fopen(filename_dev, "r");
		if(devs_fd <0 ) {
			fprintf(stderr,"fail to open devs information = %s\n", filename_dev);
			err = -2;
			goto error_out;
		}

		result = parse_devs(devs_fd,Name, slot);
		fclose(devs_fd);

		// success
		if(result == 0)
			break;

		// fail
		if(--retry == 0)
		{
			fprintf(stderr,"get_slot_number : Fail parsing\n");
			err = -1;
			goto error_out;
		}

		// retry
		fprintf(stderr,"retrying...\n");
   		sleep(4);
    }

 error_out:
       return err;
	
}

//
//	{"diskinfo", ndc_diskinfo,
//		"\tUsage : ndasadmin diskinfo slotnumber\n"},
//

/*
#define MAX_DISC_INFO 120
typedef struct _DISK_information {
    short partinfo[NR_PARTITION];
    int     discinfo[MAX_DISC_INFO];
    int     count;
}DISK_information, *PDISK_information;

typedef struct _ndas_ioctl_juke_disk_info {
    IN	int				slot;
    IN	PDISK_information	diskInfo;
    OUT 	ndas_error_t 		error;	    	
}ndas_ioctl_juke_disk_info_t, * ndas_ioctl_juke_disk_info_ptr;
*/

int do_ndc_diskinfo(ndas_ioctl_juke_disk_info_ptr arg){
	int err = 0;
	// for output
	int i = 0;
	PDISK_information pdiskinfo = NULL;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		return -1;
	}
	
	fprintf(stderr,"do_ndc_diskinfo :POSE of diskinfo %p\n", arg->diskInfo);
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_GET_DISK_INFO, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_GET_DISK_INFO err_code = %d\n", arg->error);
		goto error_out;
	}
	if(arg->error < 0){
		err = arg->error;
		goto error_out;
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
error_out:
	close(ndas_fd);
	return err;
}

/*
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
*/

//
//	{"discinfo", ndc_discinfo,
//		"\tUsage : ndasadmin discinfo slotnumber discnumber\n"},
//

/*
IOCTL_NDCMD_MS_GET_DISC_INFO
typedef struct 	_DISC_INFO
{
	unsigned char		title_name[NDAS_MAX_NAME_LENGTH];
	unsigned char		additional_infomation[NDAS_MAX_NAME_LENGTH];
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
int do_ndc_discinfo(ndas_ioctl_juke_disc_info_ptr arg)
{
	int err = 0;

	// for output
	PDISC_INFO pdiscinfo = NULL;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		return -1;
	}
	
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_GET_DISC_INFO, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_GET_DISC_INFO err_code = %d\n", arg->error);
		goto error_out;
	}
	if(arg->error < 0){
		err = arg->error;
		goto error_out;
	}

	// for output
	pdiscinfo = arg->discInfo;
	if(pdiscinfo){
		fprintf(stderr, "TITLE : %s\n",pdiscinfo->title_name);
		fprintf(stderr, "ADD_INFO: %s\n", pdiscinfo->additional_infomation);
	}
error_out:
	close(ndas_fd);
	return err;
}

/*
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
*/
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
int do_ndc_seldisc(ndas_ioctl_juke_read_start_ptr arg)
{
	int err = 0;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		return -1;
	}
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_SETUP_READ, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_SETUP_READ err_code = %d\n", arg->error);
		goto error_out;
	}
	if(arg->error < 0){
		err = arg->error;
	}

	// for output
	if(arg->partnum != 0 && arg->partnum < 8)
	{
		fprintf(stderr, "ALLOCATED PARTNUM [%d]\n",arg->partnum);
	}else{
		err = -1;
		fprintf(stderr, "Invalid partnum %d\n", arg->partnum);
	}
error_out:
	close(ndas_fd);
	return err;
}

/*
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
*/

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
int do_ndc_deseldisc(ndas_ioctl_juke_read_end_ptr arg)
{
	int err = 0;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		return -1;
	}
	fprintf(stderr, "ALLOCATED PARTNUM [%d]\n",arg->partnum);
	
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_END_READ, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_END_READ err_code = %d\n", arg->error);
		goto error_out;
	}
	
	if(arg->error < 0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_END_READ err_code = %d\n", arg->error);
		err =  arg->error;
		goto error_out;
	}
error_out:
	close(ndas_fd);
	return err;
}


/*
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
*/
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
	unsigned char		additional_infomation[NDAS_MAX_NAME_LENGTH];
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
#ifdef NDAS_MSHARE_WRITE

static int do_writing_disc_start(ndas_ioctl_juke_write_start_ptr arg,int slotnumber, int discnumber, PDISC_INFO discInfo, unsigned int total_count)
{
	int err = 0;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		return -1;
	}
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
		goto error_out;
	}
	fprintf(stderr, "allocated partnumber %d\n", arg->partnum);
	if(arg->partnum == 0 || arg->partnum > 8) {
		fprintf(stderr, "invalid partnum %d\n", arg->partnum);
		err =  -1;
		goto error_out;
	}
error_out:
	close(ndas_fd);	
	return 0;
}

static int do_writing_disc_end(ndas_ioctl_juke_write_end_ptr arg, int slotnumber, int discnumber, int partnum, PDISC_INFO discInfo)
{
	int err = 0;
	time_t tm;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		return -1;
	}
	arg->slot = slotnumber;
	arg->partnum = partnum;
	arg->selected_disc = discnumber;
	arg->discInfo = discInfo;

	time(&tm);
	sprintf(discInfo->additional_infomation, "create time %d", tm);
	
	err = ioctl(ndas_fd,IOCTL_NDCMD_MS_END_WRITE, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_SETUP_WRITE err_code = %d\n", arg->error);
		goto error_out;
	}	
error_out:
	close(ndas_fd);
	return 0;
	
}

int do_ndc_rcopy(int slotnumber, int discnumber, char *disc_path)
{
	unsigned int total_count = 0;
	ndas_ioctl_juke_write_start_t arg;
	ndas_ioctl_juke_write_end_t  arg2;
	PDISC_INFO	discInfo = NULL;
	char * databuffer = NULL;
	char dest_path[30];
	char destChar = 'a';

	memset(&arg, 0, sizeof(ndas_ioctl_juke_write_start_t));
	memset(&arg2, 0, sizeof(ndas_ioctl_juke_write_end_t));
	
	databuffer = (char *)malloc(sizeof(DISC_INFO));
	if(!databuffer){
		fprintf(stderr,"fail alloc databuffer\n");
		return -1;
	}
	memset(databuffer,0,sizeof(DISC_INFO));

	discInfo = (PDISC_INFO) databuffer;
	
	//get DVD info
	if(GetDVDTitleName(disc_path, discInfo->title_name) != 0)
	{
		fprintf(stderr,"do_nd_real_copy error GetDvdTitleName %s\n", disc_path);
		free(discInfo);
		return -1;
	}
	
	fprintf(stderr,"TITLE NAME of %s : %s\n",disc_path, discInfo->title_name);

	if( GetDVDSize(disc_path, &total_count) != 0)
	{
		fprintf(stderr,"do_nd_real_copy error GetDvdSize %s\n", disc_path);
		free(discInfo);
		return -1;
	}
	
	fprintf(stderr,"SIZE of DVD TITLE : %s (%d)\n", discInfo->title_name, total_count);

	//start Burn
	if(do_writing_disc_start(&arg, slotnumber, discnumber, discInfo, total_count) != 0)
	{
		fprintf(stderr,"fail do_writing_disc_start\n");
		free(discInfo);
		return -1;
	}

	//make dest path
	memset(dest_path,0,30);
	destChar = 'a' + (slotnumber-1);
	sprintf(dest_path,"/dev/nd%c%d",destChar,arg.partnum);

	// Writing Data to Disc
	if(SetDVDCopytoMedia(disc_path, dest_path) != 0)
	{
		fprintf(stderr,"do_nd_real_copy error : GetDvdCopytoMedia \n");
		free(discInfo);
		return -1;
	}	
	
	//end Burn
	sprintf(discInfo->title_info, "%d\n", total_count);	
	if(do_writing_disc_end(&arg2, slotnumber, discnumber, arg.partnum, discInfo) != 0)
	{
		fprintf(stderr,"fail do_writing_disc_end\n");
		free(discInfo);
		return -1;
	}
	free(discInfo);
	return 0;
}

/*
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
*/
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
int do_ndc_del(ndas_ioctl_juke_del_ptr arg)
{
	int err = 0;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		return -1;
	}
	
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_DEL_DISC, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_DEL_DISC err_code = %d\n", arg->error);
		goto error_out;
	}
	
	if(arg->error < 0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_DEL_DISC err_code = %d\n", arg->error);
		err =  arg->error;
		goto error_out;
	}
error_out:
	close(ndas_fd);
	return err;
}

/*
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
*/
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
int do_ndc_validate(int slotnumber, int discnumber, char *dvd_path)
{
	int err = 0;
	ndas_ioctl_juke_validate_t arg;
	PDISC_INFO pdiscinfo;
	ndas_ioctl_juke_disc_info_t arg2;
	char title_name[128];

	memset(&arg,0,sizeof(ndas_ioctl_juke_validate_t));
	memset(&arg2,0,sizeof(ndas_ioctl_juke_disc_info_t));

	pdiscinfo = (PDISC_INFO) malloc(sizeof(DISC_INFO));
	if(!pdiscinfo){
		fprintf(stderr,"fail alloc discinfo\n");
		return -1;
	}
	arg2.slot = slotnumber;
	arg2.selected_disc = discnumber;
	arg2.discInfo = pdiscinfo;

	if(do_ndc_discinfo(&arg2) != 0){
		fprintf(stderr, "fail get disc information\n");
		free(pdiscinfo);
		return -1;
	}

	memset(title_name,0,128);
	
	//get DVD info
	if(GetDVDTitleName(dvd_path, title_name) != 0)
	{
		fprintf(stderr,"fail GetDvdTitleName %s\n", dvd_path);
		free(pdiscinfo);
		return -1;
	}
	fprintf(stderr,"TITLE NAME of %s : %s\n", dvd_path,title_name);

	if(!strncmp(arg2.discInfo->title_name, title_name, 10)){
		arg.slot = slotnumber;
		arg.selected_disc =discnumber;
		err = ioctl(ndas_fd, IOCTL_NDCMD_MS_VALIDATE_DISC, &arg);
		if(err !=0){
			fprintf(stderr, "fail IOCTL_NDCMD_MS_FORMAT_DISK err_code = %d\n", arg.error);
			free(pdiscinfo);
			return err;
		}
	}

	free(pdiscinfo);
	return err;
	
}

/*	
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
*/
/*
	{"format", ndc_format,
		"\tUsage : ndasadmin format slotnumber\n"}

IOCTL_NDCMD_MS_FORMAT_DISK

typedef struct _ndas_ioctl_juke_format {
    IN	int			slot;
   OUT 	ndas_error_t 	error;	    
}ndas_ioctl_juke_format_t, * ndas_ioctl_juke_format_ptr;
	
*/

int do_ndc_format(ndas_ioctl_juke_format_ptr arg)
{
	int err = 0;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		return -1;
	}	
	err = ioctl(ndas_fd, IOCTL_NDCMD_MS_FORMAT_DISK, arg);
	if(err !=0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_FORMAT_DISK err_code = %d\n", arg->error);
		goto error_out;
	}
	
	if(arg->error < 0){
		fprintf(stderr, "fail IOCTL_NDCMD_MS_FORMAT_DISK err_code = %d\n", arg->error);
		err = arg->error;
		goto error_out;
	}
error_out:
	close(ndas_fd);
	return err;
}

/*
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
*/
	
#endif //#ifdef NDAS_MSHARE_WRITE



int do_ndc_start() {
    int err = 0;
    int ndas_fd = get_ndas_fd();

    if(ndas_fd < 0) {
    	 return -1;
    }	
    err = ioctl(ndas_fd, IOCTL_NDCMD_START, NULL);
    if ( err != 0 ) 
    {
        fprintf(stderr,"start: error occurred\n");
        goto error_out;
    }
    err = restore_registration_info();
    debug_ndapi(1,"err=%d", err);
    printf("NDAS driver initialized.\n");        
 error_out:
    close(ndas_fd);
    return err;
}

int do_ndc_stop() {
    int err = 0;
    int ndas_fd = get_ndas_fd();

    if(ndas_fd < 0) {
    	 return -1;
    }	    
    err = ioctl(ndas_fd, IOCTL_NDCMD_STOP, NULL);
    if ( err != 0 ) 
    {
        fprintf(stderr,"stop: error occurred\n");
        goto error_out;
    }
    printf("NDAS driver is stopped\n");
 error_out:
    close(ndas_fd);
    return 0;
}


/*
static int ndc_register(int argc, char *argv[]) {
    int c, i, err, option_index, ret = 0, ndas_id_arg_len = 0;
    unsigned char        *ndas_id_arg = NULL;
    ndas_ioctl_register_t arg = {0};
    const char    delimiter[] = "-";
    char* id[5] = { 0,0,0,0,0 };
    static struct option long_options[] = {
       {"name", 1, 0, 0},
       {0, 0, 0, 0}
    };
    if (argc<2) {
        cmd_usage(argv[0]);
        return 0;
    }
    debug_ndapi(4, "ndc_register: Entered...");

    ndas_id_arg = strdup(*(argv+1));

    debug_ndapi(4,  "length %d\t string %s", strlen(ndas_id_arg), ndas_id_arg);

#ifndef NDAS_SERIAL
    if (verify_ndasid(ndas_id_arg) != 0) {
        fprintf(stderr,"%s: invalid NDAS ID\n", *argv);
        ret = -1;
        goto out;
    }
#endif
    ndas_id_arg_len = strlen(ndas_id_arg);
#ifdef NDAS_SERIAL
    if ( ndas_id_arg_len != NDAS_SERIAL_LENGTH )  {
        fprintf(stderr,"%s: invalid NDAS serial ID\n", *argv);
        ret = -1;
        goto out;
    }    
    memcpy(arg.ndas_id, ndas_id_arg, NDAS_SERIAL_LENGTH+1);
    arg.ndas_key[0] = 0;
#else
    i = 0;
    id[i++] = strtok(ndas_id_arg, delimiter);
    while ((id[i] = strtok(NULL, delimiter)) != NULL) {
        i++;
    }
    
    if (ndas_id_arg_len == 23) {
        id[4] = (char *)malloc(6);
        id[4][0] = '\0';
    }
    memcpy(arg.ndas_id, id[0], 5);
    memcpy(arg.ndas_id + 5, id[1], 5);
    memcpy(arg.ndas_id + 10, id[2], 5);
    memcpy(arg.ndas_id + 15, id[3], 5);
    memcpy(arg.ndas_key, id[4], 5);
#endif
    
    arg.name[0] = 0;
    debug_ndapi(4, "arg.ndas_id=%s", arg.ndas_id);
    debug_ndapi(4, "arg.ndas_key=%s", arg.ndas_key);
    arg.ndas_id[NDAS_ID_LENGTH] = 0;
    arg.ndas_key[NDAS_KEY_LENGTH] = 0;
    debug_ndapi(4, "arg.ndas_id=%s", arg.ndas_id);
    debug_ndapi(4, "arg.ndas_key=%s", arg.ndas_key);
    
    while ((c = getopt_long(argc, argv, "n:", long_options, &option_index)) != EOF) 
    {
        if ( c == '?' ) {
            cmd_usage(argv[0]);
            ret = -1;    
            goto out;
        }
        if ( c != 0 && c != 'n' ) {
            fprintf(stderr,"%s: unknown option \"-%c\"", *argv, c);
            cmd_usage(argv[0]);
            ret = -1;    
            goto out;
        }
        if (strnlen(optarg, NDAS_MAX_NAME_LENGTH) >= NDAS_MAX_NAME_LENGTH) {
            fprintf(stderr,"%s: \"%s\" is too long\n", *argv, arg.name);
            fprintf(stderr,"%s: name should be less than %d\n",*argv,NDAS_MAX_NAME_LENGTH);
            ret = -1;
            goto out;
        }
        strncpy(arg.name,optarg,NDAS_MAX_NAME_LENGTH);
    }
    if ( arg.name[0] == 0 ) 
    {
        cmd_usage(argv[0]);
        ret = -1;
        goto out;
    }
    if ( ioctl(ndas_fd, IOCTL_NDCMD_REGISTER, &arg) != 0 ) 
    {
        switch(arg.error) {
        case NDAS_ERROR_ALREADY_REGISTERED_DEVICE:
            fprintf(stderr,"%s: already registered device\n", *argv);
            ret = FALSE;
            break;
        case NDAS_ERROR_INVALID_NDAS_ID:
            fprintf(stderr,"%s: invalid NDAS ID\n", *argv);
            ret = FALSE;
            break;
        case NDAS_ERROR_ALREADY_REGISTERED_NAME:
            fprintf(stderr,"%s: already registered name\n", *argv);
            ret = FALSE;
            break; 
        default:
            fprintf(stderr,"%s: error(%d) occurred", *argv, arg.error);
            ret = FALSE;
        }
        debug_ndapi(1,"fail to ioctl=%d",arg.error);
        goto out;
    }
    err = save_registration_info();
    printf("\n'%s' is registered successfully.\n", arg.name);
out:
    if (ndas_id_arg != NULL)
        free(ndas_id_arg);

    if (ndas_id_arg_len == 23) free(id[4]);

    return ret;
}
*/
int do_ndc_register(ndas_ioctl_register_ptr arg) 
{
	int ndas_id_arg_len = 0;
	int err = 0;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		 return -1;
	}
	
	debug_ndapi(4, "arg.ndas_id=%s", arg->ndas_id);
 	debug_ndapi(4,"arg.name = %s", arg->name);
	 if ( ioctl(ndas_fd, IOCTL_NDCMD_REGISTER, arg) != 0 ) 
	{	
		err = arg->error;
		switch(arg->error) {
		case NDAS_ERROR_ALREADY_REGISTERED_DEVICE:
			fprintf(stderr,"%s: already registered device\n","register");
			goto error_out;
			break;
		case NDAS_ERROR_INVALID_NDAS_ID:
			fprintf(stderr,"%s: invalid NDAS ID\n", "register");
			goto error_out;
			break;
		case NDAS_ERROR_ALREADY_REGISTERED_NAME:
			fprintf(stderr,"%s: already registered name\n", "register");
			goto error_out;
			break; 
		default:
			fprintf(stderr,"%s: error(%d) occurred", "register", arg->error);
			goto error_out;
		}
		debug_ndapi(1,"fail to ioctl=%d",arg->error);
		goto error_out;
	}
	err = save_registration_info();
	printf("\n'%s(%s)' is registered successfully.\n", arg->ndas_idserial, arg->name);
error_out:
	close(ndas_fd);	
	return err;	
}


/*
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
            fprintf(stderr,"%s: \"%s\" is too long\n", *argv, arg.name);
            fprintf(stderr,"%s: name should be less than %d\n",*argv,NDAS_MAX_NAME_LENGTH);
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
            fprintf(stderr,"%s: \"%s\" does not exist\n", *argv, arg.name);
            return -1;
        default:
            fprintf(stderr,"%s: error(%d) occurred\n", *argv, arg.error);
            return -1;
        }
    }
    err = save_registration_info();
    printf("The NDAS device - \"%s\" is unregistered successfully\n", arg.name);
    return 0;
}
*/
int do_ndc_unregister(ndas_ioctl_unregister_ptr arg)
{
	int ndas_id_arg_len = 0;
	int err = 0;
	int ndas_fd = get_ndas_fd();
	int retry = 5;

	if(ndas_fd < 0) {
		 return -1;
	}
	
	debug_ndapi(4, "arg.name=%s", arg->name);
retry:
	if ( ioctl(ndas_fd, IOCTL_NDCMD_UNREGISTER, arg) != 0 ) {
		err = arg->error;
		switch(arg->error){
		case NDAS_ERROR_INVALID_NAME:
			fprintf(stderr,"%s: \"%s\" does not exist\n", "unregister", arg->name);
			goto error_out;
		case NDAS_ERROR_ALREADY_ENABLED_DEVICE:
			if (--retry == 0)
			{
				fprintf(stderr,"%s: error(%d) occurred\n", "unregister", arg->error);
				goto error_out;
			}
			fprintf(stderr,"retrying unregistering...\n");
			sleep(4);
			goto retry;
			break;
		default:
			fprintf(stderr,"%s: error(%d) occurred\n", "unregister", arg->error);
			goto error_out;
		}
		
	}
	err = save_registration_info();
	printf("The NDAS device - \"%s\" is unregistered successfully\n", arg->name);
error_out:
	close(ndas_fd);
	return err;	
}

/*
static int ndc_enable(int argc, char *argv[]) 
{
    ndas_ioctl_enable_t arg;
    char *p;
    int c;
    debug_ndapi(3, "ing");

    arg.slot = NDAS_FIRST_SLOT_NR - 1;
    arg.write_mode = -1;
    while ((c = getopt(argc, argv, "n:s:o:")) != EOF)
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
#ifdef NDAS_WRITE        
        if ( c != 'o' ) {
            fprintf(stderr,"%s: unknown option \"-%c\"", *argv, c);
            cmd_usage(argv[0]);
            return -1;
        }
        if ( optarg[0] == 'w' && optarg[1] == '\0') {
            arg.write_mode = 1;
            debug_ndapi(3,"READ-WRITE");
        } else if ( optarg[0] == 'r' && optarg[1] == '\0') {
            arg.write_mode = 0;
            debug_ndapi(3,"READ-ONLY");
        } else if (optarg[0] == 's' && optarg[1] == '\0') {
            arg.write_mode = 2; 
            debug_ndapi(3,"SHARED-WRITE");            
        } else {
            fprintf(stderr,"%s: unknown access-mode \'%s\'", *argv, optarg);
            cmd_usage(argv[0]);
            return FALSE;
        }
#else
        fprintf(stderr,"%s: unknown option \"-%c\"", *argv, c);
        cmd_usage(argv[0]);
        return -1;
#endif
    }
#ifdef NDAS_WRITE
    if ( arg.slot == NDAS_FIRST_SLOT_NR - 1 || arg.write_mode == -1) 
#else
    arg.write_mode = 0;
    if ( arg.slot == NDAS_FIRST_SLOT_NR - 1) 
#endif
    {
        cmd_usage(argv[0]);
        return -1;
    }
    debug_ndapi(3, "ioctl-ing arg.slot=%d", arg.slot);
    if ( ioctl(ndas_fd,IOCTL_NDCMD_ENABLE,&arg) != 0 ) {
        switch ( arg.error ) {
        case NDAS_ERROR_TOO_MANY_INVALID:
            fprintf(stderr,"%s: too many defective disks."
                " At lease %d disks should be valid to enable."
                " you can validate the disk by\n"
                " ndasadmin validate -s [slot]", *argv, 1);
            break;
        case NDAS_ERROR_UNSUPPORTED_DISK_MODE:
            fprintf(stderr,"%s: is a member of NDAS bind\n", *argv);
            break;
        case NDAS_ERROR_TIME_OUT:
            fprintf(stderr,"%s: timed out to connect NDAS device\n", *argv);
            break;
        case NDAS_ERROR_ALREADY_ENABLED_DEVICE:
            fprintf(stderr,"%s: block device /dev/nd%c is already enabled\n", *argv, 'a' + arg.slot - 1);
            break;
        case NDAS_ERROR_INVALID_SLOT_NUMBER:
            fprintf(stderr,"%s: slot %d does not exists\n", *argv, arg.slot);
            break;
        case NDAS_ERROR_OUT_OF_MEMORY:
            fprintf(stderr,"%s: out of memory to enable the slot %d\n", *argv, arg.slot);
            break;
        case NDAS_ERROR_NOT_ONLINE:
            fprintf(stderr,"%s: device for slot %d is not on the network\n", *argv, arg.slot);
            break;
#ifdef NDAS_WRITE
        case NDAS_ERROR_NO_WRITE_ACCESS_RIGHT: 
            //when the device unplugged and plug back in 10 sec, this error happens.
            //   display the correct message for such a case 
            fprintf(stderr,"%s: You did not provide a valid write key or slot %d is exclusively accessed by other host.\n"
                "Execute the following command to send the request for the write permission.\n"
                "ndasadmin request -s %d\n", *argv, arg.slot, arg.slot);
            break;
#endif            
        default:
            fprintf(stderr,"%s: error(%d) occurred\n", *argv, arg.error);
        }
        return -1;    
    }
    if ( arg.error == NDAS_OK ) {
        printf("Block device /dev/nd%c is ready to use.\n",'a' + arg.slot - 1);
        return 0; 
    }
    printf("Slot %d may be a member of RAID disk.\n"
        "To enable the raid, excute the following command\n"
        "ndasadmin enable -s %d -o [\'r\'|\'w\']\n", arg.slot, arg.error);
    return 0;
        
}
*/
int do_ndc_enable(ndas_ioctl_enable_ptr arg)
{
	int ndas_id_arg_len = 0;
	int err = 0;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		 return -1;
	}
	
#ifdef NDAS_WRITE
	if ( arg->slot == NDAS_FIRST_SLOT_NR - 1 || arg->write_mode == -1) 
#else
	arg->write_mode = 0;
	if ( arg->slot == NDAS_FIRST_SLOT_NR - 1) 
#endif
	{
		fprintf(stderr,"invalid slot number %d mode %d\n", arg->slot, arg->write_mode);
		err = -1;
		goto error_out;
	}

	debug_ndapi(3, "ioctl-ing arg.slot=%d", arg->slot);
	fprintf(stderr, "ioctl(IOCTL_NDCMD_ENABLE) : arg->slot(%d), arg->write_mode(%d)\n", arg->slot, arg->write_mode);
	if ( ioctl(ndas_fd,IOCTL_NDCMD_ENABLE,arg) != 0 ) {
		err = arg->error;
		switch ( arg->error ) {
		case NDAS_ERROR_TOO_MANY_INVALID:
			fprintf(stderr,"%s :enable: too many defective disks."
			    " At lease %d disks should be valid to enable."
			    " you can validate the disk by\n"
			    " ndasadmin validate -s [slot]","enable", 1);
			break;
		case NDAS_ERROR_UNSUPPORTED_DISK_MODE:
			fprintf(stderr,"%s: is a member of NDAS bind\n", "enable");
			break;
		case NDAS_ERROR_TIME_OUT:
			fprintf(stderr,"%s: timed out to connect NDAS device\n", "enable");
			break;
		case NDAS_ERROR_ALREADY_ENABLED_DEVICE:
			fprintf(stderr,"%s: block device /dev/nd%c is already enabled\n", "enable", 'a' + arg->slot - 1);
			break;
		case NDAS_ERROR_INVALID_SLOT_NUMBER:
			fprintf(stderr,"%s: slot %d does not exists\n", "enable", arg->slot);
			break;
		case NDAS_ERROR_OUT_OF_MEMORY:
			fprintf(stderr,"%s: out of memory to enable the slot %d\n", "enable", arg->slot);
			break;
		case NDAS_ERROR_NOT_ONLINE:
			fprintf(stderr,"%s: device for slot %d is not on the network\n", "enable", arg->slot);
			break;
#ifdef NDAS_WRITE
		case NDAS_ERROR_NO_WRITE_ACCESS_RIGHT: 
			//when the device unplugged and plug back in 10 sec, this error happens.
			//   display the correct message for such a case 
			fprintf(stderr,"%s: You did not provide a valid write key or slot %d is exclusively accessed by other host.\n"
			    "Execute the following command to send the request for the write permission.\n"
			    "ndasadmin request -s %d\n", "enable", arg->slot, arg->slot);
			break;
#endif            
		default:
			fprintf(stderr,"%s: error(%d) occurred\n", "enable", arg->error);
		}
		goto error_out;
	}
	if ( arg->error == NDAS_OK ) {
		printf("Block device /dev/nd%c is ready to use.\n",'a' + arg->slot - 1); 
	}
	close(ndas_fd);
	return 0;
error_out:
	close(ndas_fd);
	printf("Slot %d may be a member of RAID disk.\n"
	    "To enable the raid, excute the following command\n"
	    "ndasadmin enable -s %d -o [\'r\'|\'w\']\n", arg->slot, arg->error);
	return err;
        	
}

/*
static int ndc_disable(int argc, char *argv[]) {
    ndas_ioctl_arg_disable_t arg;
    int c;
    char *p;
    arg.slot = NDAS_FIRST_SLOT_NR - 1;
    while ((c = getopt(argc, argv, "n:s:")) != EOF)
    {
        switch(c) {
        case '?':
            cmd_usage(argv[0]);
            return -1;
        case 'n': // deprecated 
        case 's':
            arg.slot = strtol(optarg, &p, 0);
            if ( p <= optarg ) {
                fprintf(stderr,"%s: invalid slot format %s\n", *argv, optarg); 
                cmd_usage(argv[0]);
                return -1;
            }
            if ( arg.slot <= NDAS_FIRST_SLOT_NR - 1 ) {
                fprintf(stderr,"%s: invalid slot number %d\n", *argv, arg.slot);
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
        default:
            fprintf(stderr,"%s: error(%d) occurred\n", *argv, arg.error);
        }
        return -1;    
    }    
    printf("/dev/nd%c is disabled\n", 'a' + arg.slot - 1);
    return 0; 
}
*/
int do_ndc_disable(ndas_ioctl_arg_disable_ptr arg)
{
	int ndas_id_arg_len = 0;
	int err = 0;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		 return -1;
	}
	if ( arg->slot == NDAS_FIRST_SLOT_NR - 1 ) {
		return -1;
	}
	if ( ioctl(ndas_fd,IOCTL_NDCMD_DISABLE,arg) != 0 ) {
		err = arg->error;
		switch ( arg->error ) {
		case NDAS_ERROR_INVALID_SLOT_NUMBER:
		case NDAS_ERROR_ALREADY_DISABLED_DEVICE:
			fprintf(stderr,"%s: slot %d is not enabled\n", "disable", arg->slot );
			goto error_out;
			break;
		default:
			fprintf(stderr,"%s: error(%d) occurred\n", "disable", arg->error);
			goto error_out;
		}
		return -1;    
	}    
	printf("/dev/nd%c is disabled\n", 'a' + arg->slot - 1);
error_out:
	close(ndas_fd);
	return err; 
	
}

/*
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
            if ( strnlen(optarg, NDAS_MAX_NAME_LENGTH + 1) == NDAS_MAX_NAME_LENGTH + 1 )
            {
                fprintf(stderr,"%s: too long name %s. name should be less than %d\n", *argv, optarg, NDAS_MAX_NAME_LENGTH + 1);
                cmd_usage(argv[0]);
                return -1;
            }
            strncpy(arg.name, optarg, NDAS_MAX_NAME_LENGTH + 1);
            break;
        case 'n':
            if ( strnlen(optarg, NDAS_MAX_NAME_LENGTH + 1) == NDAS_MAX_NAME_LENGTH + 1)
            {
                fprintf(stderr,"%s: too long name %s. name should be less than %d\n", *argv, optarg, NDAS_MAX_NAME_LENGTH + 1);
                cmd_usage(argv[0]);
                return -1;
            }
            strncpy(arg.newname, optarg, NDAS_MAX_NAME_LENGTH + 1);
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
*/

int do_ndc_edit( ndas_ioctl_edit_ptr arg) {
	int ndas_id_arg_len = 0;
	int err = 0;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		 return -1;
	}

	if( strnlen(arg->name, NDAS_MAX_NAME_LENGTH) == NDAS_MAX_NAME_LENGTH)
	{
                fprintf(stderr,"%s: too long name %s. name should be less than %d\n", "edit" ,arg->name, NDAS_MAX_NAME_LENGTH);
                err = -1;
                goto error_out;
	}
	
	if(strnlen(arg->newname, NDAS_MAX_NAME_LENGTH) == NDAS_MAX_NAME_LENGTH)
	{
                fprintf(stderr,"%s: too long name %s. name should be less than %d\n", "edit" ,arg->newname, NDAS_MAX_NAME_LENGTH);
                err = -1;
                goto error_out;
	}
	
	if ( ioctl(ndas_fd, IOCTL_NDCMD_EDIT, arg) != 0 ) {
		err = arg->error;
		switch ( arg->error ) {
		case NDAS_ERROR_INVALID_NAME:
			fprintf(stderr,"%s: \'%s\' is not registered\n", "edit", arg->name);
			goto error_out;
		default:
			fprintf(stderr,"%s: error(%d) occurred\n", "edit", arg->error);
			goto error_out;
		}
	}
	printf("The device registered name changed from \'%s\' into \'%s\'\n", arg->name, arg->newname);
error_out:
	close(ndas_fd);
	return err; 
}

#ifdef NDAS_WRITE
/*
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
*/
int do_ndc_request(ndas_ioctl_request_ptr arg)
{
	int ndas_id_arg_len = 0;
	int err = 0;
	int ndas_fd = get_ndas_fd();

	if(ndas_fd < 0) {
		 return -1;
	}

	if ( arg->slot == NDAS_FIRST_SLOT_NR - 1 ) {
		fprintf(stderr,"%s : invalid slot number %d\n", "request", arg->slot);
		err = -1;
		goto error_out;
	}
	if ( ioctl(ndas_fd,IOCTL_NDCMD_REQUEST, arg) != 0 ) {
		err = arg->error;
		switch( arg->error ) {
		default:        
			fprintf(stderr,"%s: error(%d) occurred\n", "request", arg->error);
			goto error_out;
		}
	}
	printf("Request for surrending the exclusive write permission sent\n");
error_out:
	close(ndas_fd);
	return err;    	
}
#endif

/*
static int ndc_probe(int argc, char *argv[]) { 
    ndas_ioctl_probe_t arg;
    int i, retry = 0;
    char *p;
    arg.sz_array = 0;
    arg.ndasids= NULL;

    do {
        if ( ioctl(ndas_fd, IOCTL_NDCMD_PROBE, &arg) != 0 ) {
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
        arg.ndasids = malloc((NDAS_ID_LENGTH + 1) * arg.nr_ndasid);
        arg.sz_array = arg.nr_ndasid;

        if ( ioctl(ndas_fd, IOCTL_NDCMD_PROBE, &arg) != 0 ) {
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
    //  ignore even if arg.sz_array != arg.nr_ndasid 
    printf("NDAS device\n");
    printf("--------------------\n");
    for (i = 0; i < arg.sz_array; i++) {
        int j;
        p = arg.ndasids + i * (NDAS_ID_LENGTH + 1);
#ifndef NDAS_SHOWID        
        for ( j = NDAS_ID_LENGTH - 5; j < NDAS_ID_LENGTH; j++)
            p[j] = '*';
#endif        
        printf("%s\n",p);
    }
    printf("--------------------\n");
    free(arg.ndasids);
    return 0;
}
*/
int do_ndc_probe(ndas_ioctl_probe_ptr arg) 
{
	int ndas_id_arg_len = 0;
	int err = 0;
	int ndas_fd = get_ndas_fd();


	if(ndas_fd < 0) {
		 return -1;
	}

	if ( ioctl(ndas_fd, IOCTL_NDCMD_PROBE_BY_ID, arg) != 0 ) {
		if (arg->error==NDAS_ERROR_UNSUPPORTED_FEATURE) {
			fprintf(stderr,"Probing is disabled for this software release.\n");
			err = arg->error;
			goto error_out;
		}
		fprintf(stderr, "%s: error(%d) occured\n", "probe", arg->error);
		err = arg->error;
		goto error_out;
	}
error_out:
	close(ndas_fd);
	return err;
}

#endif




