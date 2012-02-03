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
#ifndef __XIXFS_LINUX_TYPE_H__
#define __XIXFS_LINUX_TYPE_H__

#include <linux/types.h>

#include "xixcore/xctypes.h"

typedef xc_int8 int8;
typedef xc_int16 int16;
typedef xc_int32 int32;
typedef xc_int64 int64;
typedef xc_uint8 uint8;
typedef xc_uint16 uint16;
typedef xc_uint32 uint32;
typedef xc_uint64 uint64;


#ifndef IN
#define IN
#endif

#ifndef IN
#define OUT
#endif 

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
typedef  u16		__le16;
typedef  long 	sector_t;
typedef  u32		gfp_t;
#endif

#ifndef container_of
#define container_of(ptr, type, member) ({			\
        const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
        (type *)( (char *)__mptr - offsetof(type,member) );})

#endif

#ifndef CLONE_KERNEL 
#define CLONE_KERNEL	(CLONE_FS | CLONE_FILES | CLONE_SIGHAND)
#endif
#endif // #ifdef __XIXFS_TYPE_H__
