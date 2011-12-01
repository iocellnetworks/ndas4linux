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
#ifndef __XIXCORE_SYSTEM_TYPES_H__
#define __XIXCORE_SYSTEM_TYPES_H__

/*
 * System supplied structures
 * Xixcore must use only pointer of these system types.
 * If Xixcore uses the system types, casues compiler error that the compiler
 * cannot resolve the structure.
 * Should be implemented in platform-dependent part of Xixcore.
 */

/*
 * Xixcore block device
 */

typedef struct _XIXCORE_BLOCK_DEVICE XIXCORE_BLOCK_DEVICE, *PXIXCORE_BLOCK_DEVICE;

/*
 * Xixcore spin lock
 */

typedef struct _XIXCORE_SPINLOCK XIXCORE_SPINLOCK, * PXIXCORE_SPINLOCK;

/*
 * Xixcore semaphore
 */

typedef struct _XIXCORE_SEMAPHORE XIXCORE_SEMAPHORE, * PXIXCORE_SEMAPHORE;


#endif // __XIXCORE_SYSTEM_TYPES_H__
