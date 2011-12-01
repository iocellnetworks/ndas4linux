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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#ifndef _NDAS_PNP_H_
#define _NDAS_PNP_H_

#include "ndas_ndev.h"


#define NDDEV_LPX_PORT_NUMBER_PNP	10002

ndas_error_t npnp_init(void);
void 		 npnp_cleanup(void);

bool npnp_subscribe(const char *serial, 
					void (*ndas_device_handler) (const char* serial, const char* name, NDAS_DEV_STATUS status));

void npnp_unsubscribe(const char *serial, bool remove_from_list);


#endif // _NDAS_PNP_H_

