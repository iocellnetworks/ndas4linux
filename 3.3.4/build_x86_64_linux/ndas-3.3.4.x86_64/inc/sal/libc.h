/*
 -------------------------------------------------------------------------
 Copyright (C) 2011, IOCELL Networks Corp. Plainsboro, NJ, USA.
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
#ifndef _SAL_LIBC_H_
#define _SAL_LIBC_H_
#ifdef LINUX
#ifdef LINUXUM

#define _GNU_SOURCE /* strnlen requires this definition */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#else // LINUX kernel mode
#include "sal/linux/libc.h"
#endif // LINUX kernel mode
#endif // LINUX

#ifdef PS2DEV
#ifdef _EE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif
#ifdef _PS2
#include <tamtypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#elif defined(_IOP)
#include <tamtypes.h>
#include <stdio.h>
#include <stdarg.h>
#include <sysclib.h>
#endif 
#endif

#ifdef VXWORKS
extern int snprintf(char* buffer, size_t count, const char* format, ...);
#endif
#ifdef SYABAS
#include <stdio.h>
#include <string.h>
#endif

#if defined(LINUX) && !defined(LINUXUM)

#ifdef XPLAT_ALLOW_LINUX_HEADER

#define sal_memset memset
#define sal_memcpy memcpy
#define sal_memcmp memcmp 

#endif

#else
#define sal_snprintf(x...) snprintf(x) 
#define sal_strcat strcat
#define sal_strcmp strcmp
#define sal_strncmp strncmp
#define sal_strcpy strcpy
#define sal_strncpy strncpy
#define sal_strlen strlen
#define sal_strnlen strnlen
#define sal_memset memset
#define sal_memcpy memcpy
#define sal_memcmp memcmp 

#endif

#endif // _SAL_LIBC_H_

