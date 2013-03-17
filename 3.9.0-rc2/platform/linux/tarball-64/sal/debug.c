/*
 -------------------------------------------------------------------------
 Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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
#include <linux/kernel.h>
#include <linux/sched.h> // current
#include <linux/module.h> // mod count, EXPORT_SYMBOL
#include "sal/debug.h"
#include "sal/sync.h"
#include "sal/types.h"
#include "linux_ver.h" // SAL_HZ

static int vsprintk(char* fmt, va_list varg)
{
    static char printk_buf[1024];
    vsnprintf(printk_buf, sizeof(printk_buf), fmt, varg);
    return printk("%s", printk_buf);
}
#ifdef DEBUG
static int vsprintlnk(const char* fmt, va_list varg)
{
    static char printk_buf[512];
#ifdef NO_DEBUG_PROCESS_NAME
    char name[] = "";
#else
    char *name = current->comm;
#endif
    unsigned long now = jiffies;
    long esp = 0;
#if LINUX_VERSION_NO_SNPRINTF    
    vsprintf(printk_buf, fmt, varg);
#else
    vsnprintf(printk_buf, sizeof(printk_buf), fmt, varg);
#endif
#ifndef NO_DEBUG_ESP
#ifdef _X86
    __asm__ __volatile__("andl %%esp,%0" :
                "=r" (esp) : "0" (4096 - 1));
#endif                
#endif

    return printk("%s|%s,%ld:%ld:%ld.%lu,%ld:%ld\n",
        printk_buf,
        name,
        now / (60* 60* HZ), 
        now / (60*HZ) %60, 
        now / (HZ) %60, 
        now %HZ, 
        esp,
        MOD_COUNT
    );
}
#endif

NDAS_SAL_API int sal_error_print(char* fmt, ...)
{
    va_list varg;
    int ret;
    va_start(varg, fmt);
    ret = vsprintk(fmt, varg);
    va_end(varg);
    return ret;
}
EXPORT_SYMBOL(sal_error_print);

//#ifdef DEBUG
//static sal_semaphore v_debug_sema = SAL_INVALID_SEMAPHORE;

#ifdef DEBUG
NDAS_SAL_API int sal_debug_print(char* fmt, ...)
{
    int ret;
    va_list varg;
    va_start(varg, fmt);
    ret = vsprintk(fmt, varg);
    va_end(varg);
    return ret;
}
EXPORT_SYMBOL(sal_debug_print);

NDAS_SAL_API int sal_debug_println_impl(const char* file, 
    int line, const char* function, const char* fmt, ...)
{
    int ret;
    va_list varg;
    printk("%s:%d|", file, line);
    va_start(varg, fmt);
    ret = vsprintlnk(fmt, varg);
    va_end(varg);
    return ret;
}
EXPORT_SYMBOL(sal_debug_println_impl);
 
NDAS_SAL_API void sal_debug_hexdump(char* buf, int len)
{
    int i;
    for(i=0;i<len;i++) {
        if (i!=0 && i%16==0)
            sal_debug_print("\n");
        sal_debug_print("%02x", 0x0ff & buf[i]);
        if (i%2)
            sal_debug_print(" ");
    }
    sal_debug_print("\n");
}
EXPORT_SYMBOL(sal_debug_hexdump);
#endif

NDAS_SAL_API void sal_dump_stack(void) 
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20))
    dump_stack();
#endif
}
EXPORT_SYMBOL(sal_dump_stack);
