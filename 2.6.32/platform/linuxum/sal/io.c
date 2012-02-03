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
      notice, this list of conditions and the following disclaimer#include "sal/io.h"
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "sal/sync.h"
#include "sal/thread.h"
#include "sal/time.h"
#include "sal/mem.h"
#include "sal/debug.h"
#include "sal/types.h"
#include "sal/io.h"

static sal_semaphore lum_io_lock = SAL_INVALID_SEMAPHORE;
static int lum_open_count = 0;

#if 1 // system call ver.
NDAS_SAL_API sal_file sal_file_open(const char * filename, int flags, int mode)
{
	int linux_flags = 0;
	int fd;
	if (flags & SAL_FILE_FLAG_RO) 
		linux_flags |= O_RDONLY;
	if (flags & SAL_FILE_FLAG_RW)
		linux_flags |= O_RDWR;
	fd = open(filename, linux_flags, mode);
	if (fd==-1)
		return NULL;
	lum_open_count++;
	if (lum_io_lock == SAL_INVALID_SEMAPHORE)
		lum_io_lock = sal_semaphore_create("io", 1, 1);
	return (sal_file)(long)fd;
}

NDAS_SAL_API xbool     sal_file_get_size(sal_file file, xuint64* size)
{
	int fd = (int)((long)file);
	*size = lseek(fd, 0, SEEK_END);
	if (*size== -1) {
		return FALSE;
	}
	return TRUE;
}

NDAS_SAL_API void     sal_file_close(sal_file file)
{
	int fd = (int) ((long)file);
	close(fd);
	lum_open_count--;
	if (lum_open_count==0) {
		sal_semaphore_destroy(lum_io_lock);
		lum_io_lock = SAL_INVALID_SEMAPHORE;
	}
}

NDAS_SAL_API xint32     sal_file_read(sal_file file, xuchar* buf, xint32 size, xuint64 offset)
{
	int fd = (int) ((long)file);
	int ret;
#if 1
        do {
            ret = sal_semaphore_take(lum_io_lock, SAL_SYNC_FOREVER);
        }  while (ret == SAL_SYNC_INTERRUPTED);
   
	lseek(fd, offset, SEEK_SET);
	ret = read(fd, (void*)buf, size);
	sal_semaphore_give(lum_io_lock);
	return ret;
#else	
	/* Don't read actual disk. Used to test network throughput */
	return size;
#endif
}

NDAS_SAL_API xint32     sal_file_write(sal_file file, xuchar* buf, xint32 size,  xuint64 offset)
{
	int fd = (int) ((long)file);
	int ret;
#if 1
	static int i=0;
	i++;
	if (i%1000 ==0) {// sometimes fail operation..
		return -1;
		}

        do {
            sal_semaphore_take(lum_io_lock, SAL_SYNC_FOREVER);
        }  while (ret == SAL_SYNC_INTERRUPTED);

	lseek(fd, offset, SEEK_SET);
	ret = write(fd, (void*)buf, size);
	sal_semaphore_give(lum_io_lock);
	return ret;
#else
	return size; // temp test...
#endif
}

#else // stdio version. works only with small file.
NDAS_SAL_API sal_file sal_file_open(const char * filename, int flags, int mode)
{
	char* fmode;	
	fmode = "r";
	if (flags & SAL_FILE_FLAG_RO) 
		fmode = "r";
	if (flags & SAL_FILE_FLAG_RW)
		fmode = "a+";
	return (sal_file) fopen(filename, fmode);
}

NDAS_SAL_API xbool     sal_file_get_size(sal_file file, xuint64* size)
{
	FILE* f = (FILE*) file;
	int ret;
	ret = fseek(f, 0, SEEK_END);
	if (ret!=0) {
		printf("failed to seek\n");
		return FALSE;
	}
	*size = ftell(f);
	printf("size=%d\n", (int)*size);
	return TRUE;
}

NDAS_SAL_API void     sal_file_close(sal_file file)
{
	FILE* f = (FILE*) file;
	fclose(f);
}

NDAS_SAL_API xint32     sal_file_read(sal_file file, xuchar* buf, xint32 size)
{
	FILE* f = (FILE*) file;
	int ret;
	ret =  fread((void*)buf, 1, size, f);
	printf("read %d\n", ret);
	return ret;
}

NDAS_SAL_API xint32     sal_file_write(sal_file file, xuchar* buf, xint32 size)
{
	FILE* f = (FILE*) file;
	int ret;
	ret = fwrite((void*) buf, 1, size, f);
	printf("write ret=%d\n", ret);
	return ret;
}

#if 0   /* lseek is not used anymore */
NDAS_SAL_API xuint64     sal_file_lseek(sal_file file, xuint64 offset, int whence)
{
	FILE* f = (FILE*) file;
	int ret;
	ret = fseek(f, (long)offset, SEEK_SET);
	if (ret !=0) {
		printf("seek failed: offset=%d\n", (int)offset);
	}
	return offset;
}
#endif
#endif

