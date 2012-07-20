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
#ifndef __XIXCORE_MD5_H__
#define __XIXCORE_MD5_H__

#include "xixcore/xctypes.h"
#include "xcsystem/systypes.h"


#define XIXCORE_MD5DIGEST_HASH_WORD		4
#define XIXCORE_MD5DIGEST_BLOCK_WORD	16
#define XIXCORE_MD5DIGEST_SIZE			16
#define XIXCORE_MD5DIGEST_AND_SEQSIZE	24
typedef struct _XIXCORE_MD5DIGEST_CTX {
	xc_uint32 hash[XIXCORE_MD5DIGEST_HASH_WORD];
	xc_uint32 block[XIXCORE_MD5DIGEST_BLOCK_WORD];
	xc_uint64 bytecount;
}XIXCORE_MD5DIGEST_CTX, *PXIXCORE_MD5DIGEST_CTX;


void
xixcore_call
xixcore_md5digest_metadata(
	xc_uint8 buffer[],
	xc_uint32	buffer_size,
	xc_uint8	out[]
);



#endif // #ifndef __XIXCORE_MD5_H__

