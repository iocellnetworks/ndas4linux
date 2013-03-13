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
#ifndef __XIXCORE_SYSTEM_DEBUG_H__
#define __XIXCORE_SYSTEM_DEBUG_H__

#include "xixcore/xctypes.h"

#ifdef __XIXCORE_DEBUG__
#define XIXCORE_DEBUG
#endif

#ifndef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ __FILE__
#endif

#if defined(XIXCORE_DEBUG)

#define XIXCORE_ASSERT(p) xixcore_Assert((void *)(xc_size_t)(p), #p)

#else

#define XIXCORE_ASSERT(p) do{;}while(0)

#endif //#if defined(XIXCORE_DEBUG)


/*
 *	define debug level
 */


#define DEBUG_LEVEL_ERROR				(0x00000008)
#define DEBUG_LEVEL_CRITICAL			(0x00000004)
#define DEBUG_LEVEL_INFO				(0x00000002)
#define DEBUG_LEVEL_TRACE				(0x00000001)

#define DEBUG_LEVEL_UNWIND			(0x00000010)
#define DEBUG_LEVEL_EXCEPTIONS		(0x00000020)
	
#define DEBUG_LEVEL_ALL				(0xffffffff)
#define DEBUG_LEVEL_TEST				(DEBUG_LEVEL_ERROR|DEBUG_LEVEL_TRACE)
//#define DEBUG_LEVEL_TEST				(DEBUG_LEVEL_ERROR|DEBUG_LEVEL_CRITICAL|DEBUG_LEVEL_UNWIND|DEBUG_LEVEL_EXCEPTIONS)
/*
 *	define debug target
 */
#define DEBUG_TARGET_INIT				(0x00000001)
#define DEBUG_TARGET_THREAD			(0x00000002)
#define DEBUG_TARGET_FSCTL			(0x00000004)
#define DEBUG_TARGET_DEVCTL			(0x00000008)

#define DEBUG_TARGET_CREATE			(0x00000010)
#define DEBUG_TARGET_CLEANUP			(0x00000020)
#define DEBUG_TARGET_CLOSE			(0x00000040)

#define DEBUG_TARGET_CHECK			(0x00000100)
#define DEBUG_TARGET_READ				(0x00000200)
#define DEBUG_TARGET_WRITE			(0x00000400)
#define DEBUG_TARGET_ADDRTRANS		(0x00000800)

#define DEBUG_TARGET_FILEINFO			(0x00001000)
#define DEBUG_TARGET_DIRINFO			(0x00002000)
#define DEBUG_TARGET_VOLINFO			(0x00004000)

#define DEBUG_TARGET_HOSTCOM			(0x00010000)
#define DEBUG_TARGET_PNP				(0x00020000)
#define DEBUG_TARGET_FLUSH			(0x00040000)

#define DEBUG_TARGET_IRPCONTEXT		(0x00100000)
#define DEBUG_TARGET_FCB				(0x00200000)
#define DEBUG_TARGET_CCB				(0x00400000)
#define DEBUG_TARGET_LCB				(0x00800000)
#define DEBUG_TARGET_VCB				(0x01000000)
#define DEBUG_TARGET_FILEOBJECT		(0x02000000)
#define DEBUG_TARGET_RESOURCE			(0x04000000)
#define DEBUG_TARGET_REFCOUNT		(0x08000000)
#define DEBUG_TARGET_GDATA			(0x10000000)
#define DEBUG_TARGET_CRITICAL			(0x20000000)
#define DEBUG_TARGET_LOCK				(0x40000000)
#define DEBUG_TARGET_VFSAPIT			(0x80000000)

#define DEBUG_TARGET_TEST				(DEBUG_TARGET_VFSAPIT)
#define DEBUG_TARGET_ALL				(0xffffffff)

#if defined(XIXCORE_DEBUG)


#define DebugTrace(LEVEL,TARGET,M) do {					\
	if (xixcore_DebugMaskAndPrefix( (LEVEL), (TARGET))) {		\
		if((LEVEL) == DEBUG_LEVEL_ERROR){				\
			xixcore_DebugPrintf("%s:%d:%s: ", __XIXCORE_MODULE__,__LINE__,__FUNCTION__);\
		}												\
		xixcore_DebugPrintf M;							\
	}													\
}while(0)

#else
#define DebugTrace(LEVEL,TARGET,M)		do{;}while(0)
#endif

#endif //#ifndef __XIXCORE_SYSTEM_DEBUG_H__
