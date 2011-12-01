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
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // strncpy
#include <pthread.h>
#include <linux/fs.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define NR_ACCESS (100)
#define MAX_BUF (1024 * 4 * 10)

char file_name[1024];
int nr_thread;
long dev_size;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void thread_main(int n) {
    int i,err = 0;
    char buf[MAX_BUF];
    off_t offset;
    size_t size;
    int fd;
    fd = open(file_name,O_RDWR);
    if ( fd < 0 ) {
        printf("%s doesn't exist or not enabled\n", file_name);
        --nr_thread;
        return;
    }
    
    for (i = 0; i < NR_ACCESS;i++)
    {
        size = 1024 * 4 * 10;
        if ( RAND_MAX > (dev_size - size) )
            offset = random() / (RAND_MAX/ (dev_size - size) );
        else
            offset = random() * (dev_size - size) / RAND_MAX;
        
        if ( lseek(fd, offset,SEEK_SET) < 0 ) {
            printf("seek fail at %ld\n",offset);
            if ( err++ < 10 )
                continue;
            break;
        }
        if ( read(fd, buf, size) < 0 ) {
            printf("read fail at %ld\n",offset);
            if ( err++ < 1 )
                continue;
            break;
        }
        if ( write(fd, buf, size) < 0 ){
            printf("write fail at %ld (totol size=%ld, (%ld,%ld)\n",offset , dev_size, offset, offset+size);
            if ( err++ < 10 )
                continue;
            break;
        }
        printf("%d:%d:(%ld,%d)\n",n,i,offset,size);
    }    
    close(fd);
    --nr_thread;
    printf("%d:(%ld,%d),done\n",n,offset,size);
}
int main(int argc, char* argv[]) 
{
    int i,err,fd;

    if ( argc != 3 ) {
        printf("usage: %s <block device> <number of threads>\n",argv[0]);
        printf("  eg: %s /dev/nda 10\n",argv[0]);
        printf("     it will create 10 threads that access the /dev/nda randomly\n");
        return 0;
    }
    nr_thread = atoi(argv[2]);
    if ( nr_thread < 1 ) 
    {
        printf("invalid thread number %s\n",argv[2]);
        return 0;
    }
    if ( nr_thread > 100) 
    {
        printf("too many threads creation requested. should be less than 100\n");
        return 0;
    }
    fd = open(argv[1],O_RDWR);
    if ( fd < 0 ) {
        printf("%s doesn't exist or not enabled\n", argv[1]);
        return 0;
    }
    err = ioctl(fd, BLKGETSIZE, &dev_size);
    dev_size*=512;
    printf("%s is %ldbytes\n",argv[1],dev_size);
    if ( err != 0 ) {
        printf("fail to get the size of block %s\n", argv[1]);
        close(fd);
        return 0;
    }
    close(fd);
    strncpy(file_name,argv[1],1024);
    srandom(1);
    for (i = 0; i < nr_thread;i++) 
    {
        pthread_t tid;
        pthread_attr_t attr;
        err = pthread_attr_init(&attr);
        if ( err ) {
            printf("fail to init %dth thread attr\n", i);
            close(fd);
            return 0;    
        }
        err = pthread_create(&tid,&attr, (void*)(void*)thread_main,(void*)i);
        if ( err ) {
            printf("fail to create %dth thread\n", i);
            close(fd);
            return 0;
        }
    }

    
    while (1) {
        sleep(1);        
        if (nr_thread == 0 ) {
            close(fd);
            printf("device closed\n");
            pthread_mutex_unlock(&mutex);
            break;
        }
    }
    
    return 0;
}
