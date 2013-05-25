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
#ifndef _sal_sal_H_
#define _sal_sal_H_

#include "ndasuser/ndaserr.h"

#if defined(LINUX) && !defined(LINUXUM)
#include "sal/linux/sal.h"
#elif defined(UCOSII)
#include "sal/ucosii/sal.h"
#else
#include "sal/generic/sal.h"
#endif

#define NDAS_SAL_API NDAS_CALL
/**
 * sal_init - initialize sal layer
 * must be called before any use of sal APIs.
 */
NDAS_SAL_API extern void sal_init(void);
/**
 * clean up the resources allocated to sal layer by sal_init
 */
NDAS_SAL_API extern void sal_cleanup(void);

/**
 * sal_gethostname
 * Return the hostname. 
 * this is for display purpose only for Write permission request.
 * So if the hostname is longer than the given size, 
 * it will return the truncated name.
 *
 * Returns
 * NDAS_OK: 
 * NDAS_ERROR_INVALID_PARAMETER: size is negative.
 * 
 */
NDAS_SAL_API extern ndas_error_t sal_gethostname(char* name, int size);

#endif
