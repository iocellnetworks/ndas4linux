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

#include "xcsystem/debug.h"
#include "xcsystem/errinfo.h"
#include "xcsystem/system.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/md5.h"

/* Define module name */
#undef __XIXCORE_MODULE__
#define __XIXCORE_MODULE__ "XCMD5"


#define XIXCORE_MD5FUNC1(x, y, z)	( (z) ^  ( (x) &  ((y) ^ (z)) ) )
#define XIXCORE_MD5FUNC2(x, y, z)	XIXCORE_MD5FUNC1(z, x, y)
#define XIXCORE_MD5FUNC3(x, y, z)	((x) ^ (y) ^ (z))
#define XIXCORE_MD5FUNC4(x, y, z)	( (y) ^ ((x) | ~(z)) )


#define XIXORE_MD5STEP1(w, x, y, z, in, s) \
	((w) += XIXCORE_MD5FUNC1((x), (y), (z)) + in, (w) = ( (w) << (s) | (w) >> ( 32- (s) ) ) + (x) )

#define XIXORE_MD5STEP2(w, x, y, z, in, s) \
	((w) += XIXCORE_MD5FUNC2((x), (y), (z)) + in, (w) = ( (w) << (s) | (w) >> ( 32- (s) ) ) + (x) )

#define XIXORE_MD5STEP3(w, x, y, z, in, s) \
	((w) += XIXCORE_MD5FUNC3((x), (y), (z)) + in, (w) = ( (w) << (s) | (w) >> ( 32- (s) ) ) + (x) )

#define XIXORE_MD5STEP4(w, x, y, z, in, s) \
	((w) += XIXCORE_MD5FUNC4((x), (y), (z)) + in, (w) = ( (w) << (s) | (w) >> ( 32- (s) ) ) + (x) )





static
void 
xixcore_changebitorder_le2cpu_buffer(
	xc_uint32 buff[],
	xc_uint32 words
)
{

	while(words--) 
	{
		XIXCORE_LE2CPU32p(buff);
		buff++;
	}
}


static
void
xixcore_changebitorder_cpu2le_buffer(
	xc_uint32 buff[],
	xc_uint32 words
)
{
	while(words--) 
	{
		XIXCORE_CPU2LE32p(buff);
		buff++;
	}
}


static 
void 
xixcore_md5_function(
	xc_uint32 *hash, 
	xc_uint32 const *in)
{
	xc_uint32 a, b, c, d;

	a = hash[0];
	b = hash[1];
	c = hash[2];
	d = hash[3];

	XIXORE_MD5STEP1(a, b, c, d, in[0] + 0xd76aa478, 7);
	XIXORE_MD5STEP1(d, a, b, c, in[1] + 0xe8c7b756, 12);
	XIXORE_MD5STEP1(c, d, a, b, in[2] + 0x242070db, 17);
	XIXORE_MD5STEP1(b, c, d, a, in[3] + 0xc1bdceee, 22);
	XIXORE_MD5STEP1(a, b, c, d, in[4] + 0xf57c0faf, 7);
	XIXORE_MD5STEP1(d, a, b, c, in[5] + 0x4787c62a, 12);
	XIXORE_MD5STEP1(c, d, a, b, in[6] + 0xa8304613, 17);
	XIXORE_MD5STEP1(b, c, d, a, in[7] + 0xfd469501, 22);
	XIXORE_MD5STEP1(a, b, c, d, in[8] + 0x698098d8, 7);
	XIXORE_MD5STEP1(d, a, b, c, in[9] + 0x8b44f7af, 12);
	XIXORE_MD5STEP1(c, d, a, b, in[10] + 0xffff5bb1, 17);
	XIXORE_MD5STEP1(b, c, d, a, in[11] + 0x895cd7be, 22);
	XIXORE_MD5STEP1(a, b, c, d, in[12] + 0x6b901122, 7);
	XIXORE_MD5STEP1(d, a, b, c, in[13] + 0xfd987193, 12);
	XIXORE_MD5STEP1(c, d, a, b, in[14] + 0xa679438e, 17);
	XIXORE_MD5STEP1(b, c, d, a, in[15] + 0x49b40821, 22);

	XIXORE_MD5STEP2(a, b, c, d, in[1] + 0xf61e2562, 5);
	XIXORE_MD5STEP2(d, a, b, c, in[6] + 0xc040b340, 9);
	XIXORE_MD5STEP2(c, d, a, b, in[11] + 0x265e5a51, 14);
	XIXORE_MD5STEP2(b, c, d, a, in[0] + 0xe9b6c7aa, 20);
	XIXORE_MD5STEP2(a, b, c, d, in[5] + 0xd62f105d, 5);
	XIXORE_MD5STEP2(d, a, b, c, in[10] + 0x02441453, 9);
	XIXORE_MD5STEP2(c, d, a, b, in[15] + 0xd8a1e681, 14);
	XIXORE_MD5STEP2(b, c, d, a, in[4] + 0xe7d3fbc8, 20);
	XIXORE_MD5STEP2(a, b, c, d, in[9] + 0x21e1cde6, 5);
	XIXORE_MD5STEP2(d, a, b, c, in[14] + 0xc33707d6, 9);
	XIXORE_MD5STEP2(c, d, a, b, in[3] + 0xf4d50d87, 14);
	XIXORE_MD5STEP2(b, c, d, a, in[8] + 0x455a14ed, 20);
	XIXORE_MD5STEP2(a, b, c, d, in[13] + 0xa9e3e905, 5);
	XIXORE_MD5STEP2(d, a, b, c, in[2] + 0xfcefa3f8, 9);
	XIXORE_MD5STEP2(c, d, a, b, in[7] + 0x676f02d9, 14);
	XIXORE_MD5STEP2(b, c, d, a, in[12] + 0x8d2a4c8a, 20);

	XIXORE_MD5STEP3(a, b, c, d, in[5] + 0xfffa3942, 4);
	XIXORE_MD5STEP3(d, a, b, c, in[8] + 0x8771f681, 11);
	XIXORE_MD5STEP3(c, d, a, b, in[11] + 0x6d9d6122, 16);
	XIXORE_MD5STEP3(b, c, d, a, in[14] + 0xfde5380c, 23);
	XIXORE_MD5STEP3(a, b, c, d, in[1] + 0xa4beea44, 4);
	XIXORE_MD5STEP3(d, a, b, c, in[4] + 0x4bdecfa9, 11);
	XIXORE_MD5STEP3(c, d, a, b, in[7] + 0xf6bb4b60, 16);
	XIXORE_MD5STEP3(b, c, d, a, in[10] + 0xbebfbc70, 23);
	XIXORE_MD5STEP3(a, b, c, d, in[13] + 0x289b7ec6, 4);
	XIXORE_MD5STEP3(d, a, b, c, in[0] + 0xeaa127fa, 11);
	XIXORE_MD5STEP3(c, d, a, b, in[3] + 0xd4ef3085, 16);
	XIXORE_MD5STEP3(b, c, d, a, in[6] + 0x04881d05, 23);
	XIXORE_MD5STEP3(a, b, c, d, in[9] + 0xd9d4d039, 4);
	XIXORE_MD5STEP3(d, a, b, c, in[12] + 0xe6db99e5, 11);
	XIXORE_MD5STEP3(c, d, a, b, in[15] + 0x1fa27cf8, 16);
	XIXORE_MD5STEP3(b, c, d, a, in[2] + 0xc4ac5665, 23);

	XIXORE_MD5STEP4(a, b, c, d, in[0] + 0xf4292244, 6);
	XIXORE_MD5STEP4(d, a, b, c, in[7] + 0x432aff97, 10);
	XIXORE_MD5STEP4(c, d, a, b, in[14] + 0xab9423a7, 15);
	XIXORE_MD5STEP4(b, c, d, a, in[5] + 0xfc93a039, 21);
	XIXORE_MD5STEP4(a, b, c, d, in[12] + 0x655b59c3, 6);
	XIXORE_MD5STEP4(d, a, b, c, in[3] + 0x8f0ccc92, 10);
	XIXORE_MD5STEP4(c, d, a, b, in[10] + 0xffeff47d, 15);
	XIXORE_MD5STEP4(b, c, d, a, in[1] + 0x85845dd1, 21);
	XIXORE_MD5STEP4(a, b, c, d, in[8] + 0x6fa87e4f, 6);
	XIXORE_MD5STEP4(d, a, b, c, in[15] + 0xfe2ce6e0, 10);
	XIXORE_MD5STEP4(c, d, a, b, in[6] + 0xa3014314, 15);
	XIXORE_MD5STEP4(b, c, d, a, in[13] + 0x4e0811a1, 21);
	XIXORE_MD5STEP4(a, b, c, d, in[4] + 0xf7537e82, 6);
	XIXORE_MD5STEP4(d, a, b, c, in[11] + 0xbd3af235, 10);
	XIXORE_MD5STEP4(c, d, a, b, in[2] + 0x2ad7d2bb, 15);
	XIXORE_MD5STEP4(b, c, d, a, in[9] + 0xeb86d391, 21);

	hash[0] += a;
	hash[1] += b;
	hash[2] += c;
	hash[3] += d;
}

static
void 
xixcore_md5_transform(
	PXIXCORE_MD5DIGEST_CTX ctx
)
{
	xixcore_changebitorder_le2cpu_buffer(ctx->block, sizeof(ctx->block) / sizeof(xc_uint32));
	xixcore_md5_function(ctx->hash, ctx->block);
}


static 
void
xixcore_md5digest_ctx_init(
	PXIXCORE_MD5DIGEST_CTX ctx
)
{
	ctx->hash[0] = 0x67452301;
	ctx->hash[1] = 0xefcdab89;
	ctx->hash[2] = 0x98badcfe;
	ctx->hash[3] = 0x10325476;
	ctx->bytecount = 0;
}


static
void
xixcore_md5digest_update(
	 PXIXCORE_MD5DIGEST_CTX		ctx,
	 const xc_uint8				data[], 
	 xc_uint32					len
)
{
	const xc_uint32	avail = sizeof(ctx->block) - (xc_uint32)(ctx->bytecount & 0x3f);

	ctx->bytecount += len;

	if(avail > len){
		memcpy((xc_uint8 *)ctx->block + ( sizeof(ctx->block) - avail), data, len);
		return;
	}

	memcpy((xc_uint8 *)ctx->block + ( sizeof(ctx->block) - avail), data, avail);


	xixcore_md5_transform(ctx);
	data += avail;
	len -= avail;

	while( len >= sizeof(ctx->block))
	{
		memcpy(ctx->block, data, sizeof(ctx->block));
		xixcore_md5_transform(ctx);
		data += sizeof(ctx->block);
		len -= sizeof(ctx->block);
	}

	memcpy(ctx->block, data, len);

}


static 
void 
xixcore_md5digest_final(
	PXIXCORE_MD5DIGEST_CTX		ctx, 
	xc_uint8 out[]
)
{
	const xc_uint32 offset = (xc_uint32)(ctx->bytecount & 0x3f);
	xc_uint8 *p = (xc_uint8 *)ctx->block + offset;
	xc_int32 padding = 56 - (offset + 1);

	*p++ = 0x80;
	if (padding < 0) {
		memset(p, 0x00, padding + sizeof(xc_uint64));
		xixcore_md5_transform(ctx);
		p = (xc_uint8 *)ctx->block;
		padding = 56;
	}

	memset(p, 0, padding);
	ctx->block[14] = (xc_uint32)(ctx->bytecount << 3);
	ctx->block[15] = (xc_uint32)(ctx->bytecount >> 29);
	
	xixcore_changebitorder_le2cpu_buffer(ctx->block, (sizeof(ctx->block) - sizeof(xc_uint64)) / sizeof(xc_uint32));
	xixcore_md5_function(ctx->hash, ctx->block);
	xixcore_changebitorder_cpu2le_buffer(ctx->hash, sizeof(ctx->hash) / sizeof(xc_uint32));
	memcpy(out, ctx->hash, sizeof(ctx->hash));
	memset(ctx, 0, sizeof(XIXCORE_MD5DIGEST_CTX));
}



void
xixcore_call
xixcore_md5digest_metadata(
	xc_uint8	buffer[],
	xc_uint32	buffer_size,
	xc_uint8	out[]
	)
{
	XIXCORE_MD5DIGEST_CTX		ctx;
	
	memset(&ctx, 0, sizeof(XIXCORE_MD5DIGEST_CTX));
	

	xixcore_md5digest_ctx_init(&ctx);
	xixcore_md5digest_update(&ctx,buffer,buffer_size);
	xixcore_md5digest_final(&ctx,out);
	return;
}



