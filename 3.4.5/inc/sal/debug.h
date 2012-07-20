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
#ifndef _SAL_DEBUG_H_
#define _SAL_DEBUG_H_

#include "sal/sal.h"

#if defined(LINUXUM) || defined(PS2DEV)
#ifdef DEBUG
NDAS_SAL_API extern 
int sal_debug_print(char* fmt, ...)
    __attribute__ ((format (printf, 1, 2)));
NDAS_SAL_API extern 
int sal_debug_println_impl(const char* file, int line, const char* function, 
                            const char* fmt, ...)
    __attribute__ ((format (printf, 4, 5)));

#define sal_debug_println(x...) \
    sal_debug_println_impl(__FILE__,__LINE__,__FUNCTION__,x)

    
#else
#define sal_debug_print(x...) do {} while(0)
#define sal_debug_println(l, x...) do {} while(0)
#endif

NDAS_SAL_API extern 
int sal_error_print(char* fmt, ...) __attribute__ ((format (printf, 1, 2)));

#ifdef DEBUG
#ifdef LINUXUM
/* LINUXUM */
#ifdef DEBUG_USE_C_ASSERT
#include <assert.h>
#define sal_assert(expr) assert(expr)
#else
#ifdef _X86
#define sal_dbg_break() asm("int3")
#else
#define sal_dbg_break() 
#endif
#define sal_assert(expr) do { \
    if (!(expr)) { \
        sal_error_print("Assertion failure: %s,%d,%s\n", __FILE__, __LINE__, __FUNCTION__); \
        sal_dbg_break(); \
    } \
} while(0)
#endif
#endif 
#ifdef PS2DEV
#include <stdio.h>
#define sal_assert(expr) do { if(!(expr)) \
    sal_error_print("Assertion failure in %s:%d\n", __FILE__, __LINE__); } while(0)
#endif    
#else /* DEBUG */
#define sal_assert(expr) do { if ( expr ) {} } while(0)
#endif /* DEBUG */

#elif defined(LINUX)
#include "sal/linux/debug.h"
#endif /* defined(LINUXUM) || defined(PS2DEV) || defined(LINUX) */

#ifdef SYABAS
#include <ucos_ii.h>
#include <porting.h>
#include <sybdebug.h>
#ifdef DEBUG
#define sal_assert(expr) do { \
    if (!(expr)) { \
        debug_outs("Assertion failure: %s,%d,%s,", __FILE__, __LINE__, __FUNCTION__); \
        OSTimeDly(100);\
    } \
    SYB_ASSERT(expr); \
} while(0)
#define sal_error_print(x...) debug_outs(x)
#define sal_debug_print(x...) do { \
    /*debug_outs("\x1b[0;34m");*/ \
    debug_outs(x); \
    /*debug_outs("\x1b[0m");*/ \
    } while (0) 
#define sal_debug_println(x...)  do { debug_outs(x); debug_outs("\r\n"); } while(0)
#define sal_debug_println_impl(file, line, function, x...) ({\
    int ret; \
    debug_outs("|%s|%d|%s|", file, line, function); \
    ret = debug_outs(x);\
    debug_outs("\r\n");\
    ret;\
})
#else
#define sal_assert(expr) do {} while(0)
#define sal_error_print(x...) do {} while(0)
#define sal_debug_print(x...) do {} while(0)
#define sal_debug_println(x...) do { } while (0)
#define sal_debug_println_impl(file, line, function, x...) do {} while(0)
#endif /* DEBUG */
#endif /* SYABAS */


#ifdef DEBUG
NDAS_SAL_API extern void sal_debug_hexdump(char* buf, int len);
#else
#define sal_debug_hexdump(x, y) do {} while(0)
#endif

#ifdef DEBUG
#include "sal/libc.h" // sal_snprintf
#define SAL_DEBUG_MAX_BUF (1024)
/**
 * SAL_DEBUG_HEXDUMP 
 * Get the hex string of give data and lenth
 */
#define SAL_DEBUG_HEXDUMP(data,len) ({\
    /* TODO: invent to use the flexible size */ \
    /*static const char buf[len*2+1];*/ \
    static char __buf__[SAL_DEBUG_MAX_BUF]; \
    unsigned char *_d_ = (unsigned char *)(data); \
    if ( data ) {\
        int _i_ = 0; \
        for (_i_ = 0; _i_ < (len) && _i_*2 < SAL_DEBUG_MAX_BUF; _i_++) \
            sal_snprintf(__buf__+_i_*2,3,"%02x:",(unsigned int)((*(_d_+_i_)) & 0xffU)); \
    }\
    else \
        sal_snprintf(__buf__,SAL_DEBUG_MAX_BUF,"NULL"); \
    (const char*) __buf__; \
})
/**
 * SAL_DEBUG_HEXDUMP_S 
 * Get the hex string of give data and lenth. save some stack memory
 */
#define SAL_DEBUG_HEXDUMP_S(data,static_length) ({\
    static char __buf__[static_length*2+1]; \
    unsigned char *_d_ = (data); \
    if ( data ) {\
        int _i_ = 0; \
        for (_i_ = 0; _i_ < (static_length) && _i_*2 < sizeof(__buf__); _i_++) \
            sal_snprintf(__buf__+_i_*2,3,"%02x:",(unsigned int)((*(_d_+_i_)) & 0xffU)); \
    }\
    else \
        sal_snprintf(__buf__,sizeof(__buf__),"NULL"); \
    (const char*) __buf__; \
})
#else
#define SAL_DEBUG_HEXDUMP(data,len)  ({"";})
#define SAL_DEBUG_HEXDUMP_S(data,len)  ({"";})
#endif

#endif
