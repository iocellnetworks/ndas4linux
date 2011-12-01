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
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#ifndef _LINUX_NDADMIN_H
#define _LINUX_NDADMIN_H

#define NDA_MAKEDEV(major, minor) \
    (((minor) & 0xff) | (((major) & 0xfff) << 8) \
          | (((unsigned long long int) ((minor) & ~0xff)) << 12) \
          | (((unsigned long long int) ((major) & ~0xfff)) << 32))

#define NDA_MAJOR(dev) \
    (((dev) >> 8) & 0xfff)

#define NDA_MINOR(dev) \
    ((dev) & 0xff) | ((unsigned int) ((dev) >> 12) & 0x0fff00)
    
#define MAX_ADDR_STRING_LEN 255
#define PASSWORD_STR_SIZE 9

#ifdef DEBUG
#if defined(DEBUG_LEVEL) && !defined(DEBUG_LEVEL_NDADMIN) 
#define DEBUG_LEVEL_NDADMIN DEBUG_LEVEL
#else
#define DEBUG_LEVEL_NDADMIN 1
#endif
#define    dbg_ndadmin(l, x...) do {\
    if(l <= DEBUG_LEVEL_NDADMIN) {\
        fprintf(stderr,"NDA|%d|%s|", l,__FUNCTION__); \
        fprintf(stderr,x); \
        fprintf(stderr,"|%s:%d\n",__FILE__,__LINE__);\
    }\
} while(0)
#else
#define    dbg_ndadmin(l, x...) do {} while(0)
#endif


#endif // _LINUX_NDADMIN_H
