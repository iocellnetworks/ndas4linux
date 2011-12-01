#ifndef __NDAS_COMPAT_H__
#define __NDAS_COMPAT_H__

#ifdef NDAS_EMU

#if defined(__KERNEL__)

#include <linux/version.h>
#include <linux/module.h>

#include <linux/slab.h>
#include <linux/if_ether.h>		/* ETH_ALEN */
#include <linux/interrupt.h>	/* for in_interrupt() */
#include <linux/utsname.h>

#if   defined(__LITTLE_ENDIAN)

#define __LITTLE_ENDIAN_BYTEORDER
#define __LITTLE_ENDIAN__

#elif defined(__BIG_ENDIAN)

#define __BIG_ENDIAN_BYTEORDER
#define __BIG_ENDIAN__

#else

#error "byte endian is not specified"

#endif 


// from sal

/* SAL API attribute modifier */
#if (__GNUC__ > 3 || __GNUC__ == 3 ) && defined(_X86)
#define NDAS_CALL __attribute__((regparm(3)))
#else
#define NDAS_CALL
#endif

#define NDAS_SAL_API NDAS_CALL

#define sal_memset memset
#define sal_memcpy memcpy
#define sal_memcmp memcmp 

#define sal_snprintf(x...) snprintf(x) 
#define sal_strcat strcat
#define sal_strcmp strcmp
#define sal_strncmp strncmp
#define sal_strcpy strcpy
#define sal_strncpy strncpy
#define sal_strlen strlen
#define sal_strnlen strnlen

#define SAL_MSEC_PER_SEC 1000

#define sal_msec	unsigned long
#define sal_tick	unsigned long

#define sal_time_msec()		jiffies_to_msecs(jiffies)
#define sal_msleep(msecs)	msleep(msecs)

#define sal_tick_sub(t1, t2) ((long)((t1)-(t2)))
#define sal_tick_add(t1, t2) ((t1)+(t2))

#define SAL_TICKS_PER_SEC	msecs_to_jiffies(1000)


#include <linux/interrupt.h>	/* for in_interrupt() */

#if 0

inline static void *sal_malloc(unsigned int size)
{
    if (in_interrupt()) {

		return kmalloc(size, GFP_ATOMIC);    
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

	if (in_atomic() || irqs_disabled()) {

		return kmalloc(size, GFP_ATOMIC);    
	}
        else
#endif

	return kmalloc(size, GFP_KERNEL);
}

inline static void sal_free(void *ptr)
{
    kfree(ptr);
}

#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19))
#define GET_UTS_NODENAME() (utsname()->nodename)
#else
#define GET_UTS_NODENAME() (system_utsname.nodename)
#endif	


inline static ndas_error_t sal_gethostname(char* name, int size)
{
    int i;
    ndas_error_t err;

    if (size < 0)
        return NDAS_ERROR_INVALID_PARAMETER;
    down_read(&uts_sem);
    i = 1 + strlen(GET_UTS_NODENAME());
    if (i > size)
        i = size;
    err = NDAS_OK;
    memcpy(name, GET_UTS_NODENAME(), i);
    up_read(&uts_sem);
    return err;
}

#ifdef DEBUG

#define sal_assert(expr) do { NDAS_BUG_ON(!(expr)); } while(0)
#define sal_debug_print(x...)	do { printk(x); } while(0)
#define sal_debug_println(x...) do { printk(x); printk("\n"); } while (0)
#define sal_error_print(x...) do { printk(x); printk("\n"); } while(0)

#else

#define sal_assert(expr) do {} while(0)
#define sal_error_print(x...) do {} while(0)
#define sal_debug_println_impl(file, line, function, x...) do {} while(0)
#define sal_debug_println(x...) do { } while (0)
#define sal_debug_print(x...) do {} while(0)

#endif

inline static void sal_debug_hexdump(char* buf, int len)
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

#ifdef DEBUG
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

#include "types.h"
#include "sync.h"
#include "thread.h"

#include "mem.h"
#include "io.h"
#include "net.h"

#endif

#endif // __NDAS_COMPAT_H__
