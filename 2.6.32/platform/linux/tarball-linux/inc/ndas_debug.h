#ifndef __NDAS_DEBUG_H__
#define __NDAS_DEBUG_H__

#include <linux/kernel.h>
#include <linux/compiler.h>

#ifndef BUG_ON
#define BUG_ON(condition) do { if (unlikely((condition)!=0)) BUG(); } while(0)
#endif

#define NDAS_BUG_ON(exp)	\
	if (exp) {				\
							\
		printk("NDAS_BUG: failure at %s:%d/%s()!\n", __FILE__, __LINE__, __FUNCTION__); \
		BUG_ON(exp);		\
	}

#define C_ASSERT(x, e)	typedef char __C_ASSERT__##x[(e)?1:-1]

#define DBG_LEVEL_LPX			1
#define DBG_LEVEL_LPX_KSOCKET	1

#define DBG_LEVEL_CDEV			1
#define DBG_LEVEL_BDEV			1
#define DBG_LEVEL_SDEV			1
#define DBG_LEVEL_UDEV			1
#define DBG_LEVEL_NDEV			1
#define DBG_LEVEL_NID			1
#define DBG_LEVEL_NPNP			1
#define DBG_LEVEL_NREG			2
#define DBG_LEVEL_NSS			1
#define DBG_LEVEL_NPROC			1
#define DBG_LEVEL_NDFC			1
#define DBG_LEVEL_NEMU			1

#define DBG_LEVEL_NDASFAT		1

#define DEBUG_LEVEL_NHIX		1
#define DEBUG_LEVEL_XLIB_XBUF	1


C_ASSERT( 400, sizeof(char)  == 1 );
C_ASSERT( 401, sizeof(short) == 2 );
C_ASSERT( 402, sizeof(int)   == 4 );

#ifdef __x86_64__
C_ASSERT( 403, sizeof(long)  == 8 );
#else
C_ASSERT( 403, sizeof(long)  == 4 );
#endif

C_ASSERT( 404, sizeof(long long) == 8 );


#ifdef DEBUG
#define NDAS_DEBUG_MAX_BUF (1024)
/**
 * SAL_DEBUG_HEXDUMP 
 * Get the hex string of give data and lenth
 */
#define NDAS_DEBUG_HEXDUMP(data,len) ({				\
    /* TODO: invent to use the flexible size */ 	\
    /*static const char buf[len*2+1];*/ 			\
    static char __buf__[NDAS_DEBUG_MAX_BUF]; 		\
    unsigned char *_d_ = (unsigned char *)(data); 	\
    if ( data ) {									\
        int _i_ = 0; 								\
        for (_i_ = 0; _i_ < (len) && _i_*2 < NDAS_DEBUG_MAX_BUF; _i_++) 			\
            snprintf(__buf__+_i_*2,3,"%02x:",(unsigned int)((*(_d_+_i_)) & 0xffU)); \
    }																				\
    else 																			\
        snprintf(__buf__,NDAS_DEBUG_MAX_BUF,"NULL"); 								\
    (const char*) __buf__; 															\
})
/**
 * NDAS_DEBUG_HEXDUMP_S 
 * Get the hex string of give data and lenth. save some stack memory
 */
#define NDAS_DEBUG_HEXDUMP_S(data,static_length) ({	\
    static char __buf__[static_length*2+1]; 		\
    unsigned char *_d_ = (data); 					\
    if ( data ) {									\
        int _i_ = 0; 								\
        for (_i_ = 0; _i_ < (static_length) && _i_*2 < sizeof(__buf__); _i_++) 		\
            snprintf(__buf__+_i_*2,3,"%02x:",(unsigned int)((*(_d_+_i_)) & 0xffU)); \
    }																				\
    else 																			\
        snprintf(__buf__,sizeof(__buf__),"NULL"); 									\
    (const char*) __buf__; 															\
})
#else
#define NDAS_DEBUG_HEXDUMP(data,len)  ({"";})
#define NDAS_DEBUG_HEXDUMP_S(data,len)  ({"";})
#endif

#endif // __NDAS_DEBUG_H__
