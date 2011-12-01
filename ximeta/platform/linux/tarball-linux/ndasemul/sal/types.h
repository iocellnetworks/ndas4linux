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
    Machine/OS dependent type definitions 
*/

#ifndef __TYPES_H
#define __TYPES_H


#if defined(LINUX) || defined(VXWORKS) || defined(PS2DEV) || defined(UCOSII)

#define __x_attribute_packed__ __attribute__((packed)) 

#if defined(_X86) || defined(_IOP) || defined(_ARM) || defined(_MIPSEL) || defined(_MIPS) || defined(_CRIS)
#define xint8        char
#define xint16    short
#define xint32    int
#define xint64    long long

#define xuint8    unsigned char
#define xuint16    unsigned short
#define xuint32     unsigned int
#define xuint64     unsigned long long

#define xuchar      unsigned char
#define xulong        unsigned long
#define xpointer    void*
#define xsize_t        xulong
#elif defined(_X86_64)
#define xint8        char
#define xint16    short
#define xint32    int
#define xint64    long

#define xuint8    unsigned char
#define xuint16    unsigned short
#define xuint32     unsigned int
#define xuint64     unsigned long 

#define xuchar      unsigned char
#define xulong        unsigned long
#define xpointer    void*
#define xsize_t        xulong
#endif

#if defined(_EE)
#define xint8        char
#define xint16    short
#define xint32    int
#define xint64    long int

#define xuint8    unsigned char
#define xuint16     unsigned short
#define xuint32     unsigned int
#define xuint64     unsigned long int

#define xuchar      unsigned char
#define xulong        unsigned long
#define xpointer    void*
#define xsize_t        xulong
#endif

#ifndef xbool
#define xbool        xuint8
#endif

#ifndef FALSE
#define FALSE    0
#endif
#ifndef TRUE
#define TRUE    1
#endif

#define IN
#define OUT
#define FAR

#else  /* VXWORKS || LINUX */
#define __x_attribute_packed__     /* Use other packing method */
#endif 

#ifndef NULL
#define NULL 0
#endif

/* Compile time assertions. Check size of struct is correct */
#define sal_c_assert(t, e) typedef char __SAL_C_ASSERT__##t[(e)?1:-1]
#define sal_c_assert_sizeof(type, size) sal_c_assert(type, sizeof(type) == size)

#ifndef xint8
#error "SAL types are not defined. Specify platform and OS"
#endif

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(TYPE,MEMBER) __compiler_offsetof(TYPE,MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((xsize_t) &((TYPE *)0)->MEMBER)
#endif

#define container_of(ptr, type, member) ({          \
        const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
        (type *)( (char *)__mptr - offsetof(type,member) );})

#endif 
