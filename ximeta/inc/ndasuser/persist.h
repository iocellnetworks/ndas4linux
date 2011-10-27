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
#ifndef _NDASUSER_PERSIST_H_
#define _NDASUSER_PERSIST_H_

#include <ndasuser/ndaserr.h>
#include <ndasuser/def.h>

/* 
 * Fetch the registration data of registered NDAS devices.
 * Please note that the data allocated by the function in 'data' pointer
 * should be freed by calling sal_free() function
 */
NDASUSER_API ndas_error_t 
ndas_get_registration_data(char** data, int* size);

/*
 * Restore the registration data of NDAS devices.
 * Please note the memory passed via 'data' will be freed inside the function
 */
NDASUSER_API ndas_error_t 
ndas_set_registration_data(char *data, int size);

#endif //_NDASUSER_PERSIST_H_

