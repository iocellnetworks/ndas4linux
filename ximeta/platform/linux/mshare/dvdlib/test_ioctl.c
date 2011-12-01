#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/cdrom.h>
#include <sys/ioctl.h>

void usage()
{
	printf("test_ioctl dev_name block_size(k--must be 2x) count\n");
}

int main(int argc, char **argv)
{
	int fd;
	struct cdrom_generic_command cgc;
	struct request_sense sense;
	char * dev_name = NULL;
	int block_size = 32;
	int count = 0;
	char * buff = NULL;
	unsigned int start_sector; 	//2048 base
	unsigned short sec_count;	//2048 base
	int i = 0;

	if(argc < 4){
		usage();
		return -1;
	}

	argc++;
	argv--;
	

	dev_name = strdup(argv[0]);
	if(!dev_name) {
		printf("Fail get dev_name\n");
		usage();
		goto error_out;
	}
	
	block_size = atoi(argv[1]);
	count = atoi(argv[2]);

	if(block_size == 0 || count == 0){
		printf("size(%d) : count(%d) is invalid\n", block_size, count);
		usage();
		goto error_out;
	}

	buff =(char *) malloc(block_size * 1024);
	if(!buff)
	{
		printf("can't alloc buffer\n");
		goto error_out;
	}

	fd = open(dev_name, O_RDONLY);
	if(fd < 0){
		printf("Fail open %s\n",dev_name);
		goto error_out;
	}
	
	memset(&sense, 0, sizeof(struct request_sense));
	memset(&cgc, 0, sizeof(struct cdrom_generic_command));
	memset(buff,0, (block_size * 1024) );

	cgc.buffer = buff;
	cgc.buflen = (block_size * 1024);
	cgc.sense = &sense;
	cgc.data_direction = CGC_DATA_READ;
	sec_count = (block_size)/2;
	for(i = 0; i < count; i++)
	{	
		memset(cgc.cmd,0,CDROM_PACKET_SIZE);
		cgc.cmd[0] = 0x28; 
		cgc.cmd[2] = (unsigned char)((unsigned char)(start_sector >> 24) & 0xff);
		cgc.cmd[3] = (unsigned char)((unsigned char)(start_sector >> 16) & 0xff);
		cgc.cmd[4] = (unsigned char)((unsigned char)(start_sector >> 8) & 0xff);
		cgc.cmd[5] = (unsigned char)((unsigned char)(start_sector) & 0xff);
		cgc.cmd[7] = (unsigned char)((unsigned char)(sec_count >> 8) & 0xff);
		cgc.cmd[8] = (unsigned char)((unsigned char)(sec_count) & 0xff); 
		if(ioctl(fd, CDROM_SEND_PACKET, &cgc) < 0){
			printf("fail ioctl \n");
			close(fd);
			goto error_out;
		}
		start_sector += sec_count;									
	}

error_out:
	if(buff) free(buff);
	if(dev_name) free(dev_name);
	return 0;													
}
