/*
 -------------------------------------------------------------------------
 Copyright (c) 2005, XIMETA, Inc., IRVINE, CA, USA.
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
/*
    Abstracts file/disk IO mechanism of operating system

 */

#ifndef _SAL_IO_H
#define _SAL_IO_H

struct _sal_file;
typedef struct _sal_file* sal_file;

#define SAL_FILE_FLAG_RO     (1<<0)
#define SAL_FILE_FLAG_RW     (1<<1)
#define SAL_FILE_FLAG_EXCL   (1<<2)
#define SAL_FILE_FLAG_DIRECT (1<<3)

NDAS_SAL_API sal_file     sal_file_open(const char * filename, int flags, int mode);
NDAS_SAL_API xbool     sal_file_get_size(sal_file file, xuint64* size);
NDAS_SAL_API void     sal_file_close(sal_file file);

NDAS_SAL_API xint32     sal_file_read(sal_file file, xuchar* buf, xint32 size, xuint64 offset);
NDAS_SAL_API xint32     sal_file_write(sal_file file, xuchar* buf, xint32 size, xuint64 offset);

#if 0
#define SAL_SEEK_SET 0
#define SAL_SEEK_CUR 1
#define SAL_SEEK_END 2

NDAS_SAL_API xuint64     sal_file_lseek(sal_file file, xuint64 offset, int whence);
#endif


/*
 * "descriptor" for what we're up to with a read for sendfile().
 * This allows us to use the same read code yet
 * have multiple different users of the data that
 * we read from a file.
 *
 * The simplest case just copies the data to user
 * mode.
 */

typedef struct {
        xsize_t written;
        xsize_t count;
        union {
                char * buf;
                void * data;
        } arg;
        int error;
} sal_read_descriptor_t;

typedef NDAS_SAL_API int (*sal_read_actor_t)(sal_read_descriptor_t *,sal_page *, unsigned long, unsigned long);

typedef struct _sal_read_actor_context {
	sal_read_actor_t	sal_read_actor;
	void *target;
} sal_read_actor_context;

NDAS_SAL_API xint32     sal_file_send(sal_file file, xuint64 offset, xint32 size, sal_read_actor_context * read_actor_ctx);


#endif    /* !_SAL_IO_H */

