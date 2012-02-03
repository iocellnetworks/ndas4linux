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
 may be distributed under the terms of the GNU General Public License (GPL v2),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#ifndef __XIXCORE_TYPE_H__
#define __XIXCORE_TYPE_H__

#include "xcsystem/types.h"


#define XCTAG_LOTMAP		0x6d6c4358 // 'XClm'
#define XCTAG_CHILDBITMAP	0x62634358 // 'XCcb'
#define XCTAG_ADDRINFO		0x69614358 // 'XCai'
#define XCTAG_BITMAPEMUL	0x65624358 // 'XCbe'
#define XCTAG_HOSTDIRTYMAP	0x64684358 // 'XChd'
#define XCTAG_HOSTFREEMAP	0x66684358 // 'XChf'
#define XCTAG_VOLFREEMAP	0x66764358 // 'XCvf'
#define XCTAG_TEMPFREEMAP	0x66744358 // 'XCtf'
#define XCTAG_AUXLOCKINFO	0x696c4358 // 'XCli'
#define XCTAG_VOLNAME		0x6e764358 // 'XCvn'
#define XCTAG_FCBNAME		0x6e664358 // 'XCfn'
#define XCTAG_UPCASENAME	0x6e754358 // 'XCun'
// Added by ILGU HONG
#define XCTAG_CHILDCACHE	0x65634358 //'XCce'
// Added by ILGU HONG END



/*
 * Lot map information
 */
struct _XIXCORE_BUFFER;

typedef struct _XIXCORE_LOT_MAP{
	xc_uint32			MapType;				//allocated/free/checkout/system
	xc_uint64			NumLots;
	xc_uint32			BitMapBytes;
	xc_uint64			StartIndex;
	struct _XIXCORE_BUFFER	* Data;	
}XIXCORE_LOT_MAP, *PXIXCORE_LOT_MAP;

#endif // #ifdef __XIXCORE_TYPE_H__
