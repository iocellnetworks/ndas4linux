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
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#ifndef _SAL_SYS_DEBUG_H_
#define _SAL_SYS_DEBUG_H_

#ifdef DEBUG
NDAS_SAL_API extern int sal_debug_print(char* fmt, ...) 
                __attribute__ ((format (printf, 1, 2)));

NDAS_SAL_API extern int sal_debug_println_impl(const char* file, 
    int line, const char* function, const char* fmt, ...)
        __attribute__ ((format (printf, 4, 5)));


#define sal_debug_println(x...) \
    sal_debug_println_impl(__FILE__, __LINE__, __FUNCTION__, x)

    
#else
#define sal_debug_print(x...) do {} while(0)
#define sal_debug_println(l, x...) do {} while(0)
#endif

NDAS_SAL_API extern int sal_error_print(char* fmt, ...) 
                __attribute__ ((format (printf, 1, 2)));


/* LINUX kernel mode */

NDAS_SAL_API extern void sal_dump_stack(void); /* TODO figure out regparm in the run time */

#ifdef DEBUG
#define sal_assert(expr) do {\
    if(!(expr)) { \
            sal_error_print("Assertion failed \"%s\",%s,%s,line=%d\n",    \
                    #expr,__FILE__,__FUNCTION__,__LINE__);        \
		sal_dump_stack();\
    }\
} while(0)

#else
#define sal_assert(expr) do {} while(0)
#endif

#endif /* _SAL_SYS_DEBUG_H_ */

