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
#ifndef __XIXCORE_HOST_REG_H__
#define __XIXCORE_HOST_REG_H__

#include "xixcore/xctypes.h"

typedef struct _XIXCORE_RECORD_EMUL_CONTEXT{
	PXIXCORE_VCB		VCB;
	xc_uint32			RecordIndex;
	xc_int32			RecordSearchIndex;
	xc_uint8			HostSignature[16];	
	PXIXCORE_BUFFER 	RecordInfo;
	PXIXCORE_BUFFER  	RecordEntry;
}XIXCORE_RECORD_EMUL_CONTEXT, *PXIXCORE_RECORD_EMUL_CONTEXT;

int
xixcore_call
xixcore_RegisterHost(
	PXIXCORE_VCB	pVCB
	);

int
xixcore_call
xixcore_DeregisterHost(
	PXIXCORE_VCB VCB
	);


int
xixcore_call
xixcore_IsSingleHost(
	PXIXCORE_VCB VCB
	);


#endif // #ifndef __XIXCORE_HOST_REG_H__

