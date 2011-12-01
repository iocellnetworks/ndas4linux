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
#ifndef __XIXCORE_SYSTEM_PRIMITIVE_TYPE_H__
#define __XIXCORE_SYSTEM_PRIMITIVE_TYPE_H__

#if defined(_MSC_VER)

#define XIXCORE_INLINE static __inline

#elif defined(__GNUC__)

#define XIXCORE_INLINE static inline

#else

#error "Unrecongnized compiler."

#endif


#if defined(_MSC_VER)
#define xixcore_call __fastcall
#else
#define xixcore_call
#endif

#if defined(_MSC_VER)

typedef __int8  xc_int8;
typedef __int16 xc_int16;
typedef __int32 xc_int32;
typedef __int64 xc_int64;

typedef unsigned __int8  xc_uint8;
typedef unsigned __int16 xc_uint16;
typedef unsigned __int32 xc_uint32;
typedef unsigned __int64 xc_uint64;
typedef  __int16		xc_le16;


#if !defined(_W64)
#if !defined(__midl) && (defined(_X86_) || defined(_M_IX86)) && _MSC_VER >= 1300
#define _W64 __w64
#else
#define _W64
#endif
#endif

#ifdef _WIN64
typedef unsigned __int64	xc_size_t;
#else
typedef _W64 unsigned int	xc_size_t;
#endif

XIXCORE_INLINE
xc_uint32
xixcore_PointerToUint32(void *pointer) {
	return (xc_uint32)(xc_size_t)pointer;
}

#pragma intrinsic(memset)
#pragma intrinsic(memcpy)
#pragma intrinsic(memcmp)

/*
 * Atomic type
 */

typedef struct { long value; } XIXCORE_ATOMIC;
#define XIXCORE_ATOMIC_INIT(i)	{ (i) }

#elif defined(__GNUC__)

typedef char  xc_int8;
typedef short int xc_int16;
typedef int xc_int32;
typedef long long xc_int64;

typedef unsigned char  xc_uint8;
typedef unsigned short int xc_uint16;
typedef unsigned int xc_uint32;
typedef unsigned long long xc_uint64;

typedef  unsigned short int	xc_le16;
typedef unsigned long		xc_size_t;

XIXCORE_INLINE
xc_uint32
xixcore_PointerToUint32(void *pointer) {
	return (xc_uint32)pointer;
}

#ifndef memset
void * memset (void *, int, unsigned int);
#endif
#ifndef memcpy
void * memcpy (void *, const void *, unsigned int);
#endif
#ifndef memcmp
int memcmp (const void *, const void *, unsigned int);
#endif

/*
 * Atomic type
 */

typedef struct { volatile int value; } XIXCORE_ATOMIC;
#define XIXCORE_ATOMIC_INIT(i)	{ (i) }

#else

#error "Unrecongnized compiler."

#endif

// For Mac OS X
#if !defined( __XIXCORE_BYTEORDER_BIG__ ) && !defined( __XIXCORE_BYTEORDER_LITTLE__ )

	#if defined ( __BIG_ENDIAN__ ) 
		#define __XIXCORE_BYTEORDER_BIG__
	#elif defined ( __LITTLE_ENDIAN__ )
		#define __XIXCORE_BYTEORDER_LITTLE__
	#else
		#error "No byte order specified."
	#endif

#endif

#if defined( __XIXCORE_BYTEORDER_BIG__ )

// TODO. by jgahn.
// 
//	Since the byte swap code is function, Code in dir.c line 53, 54 can't complied with Big endian machine. 
//
//  xc_le16 XixfsUnicodeSelfArray[] = { XIXCORE_CPU2LE16('.') };
//	xc_le16 XixfsUnicodeParentArray[] = { XIXCORE_CPU2LE16('.'), XIXCORE_CPU2LE16('.') };

#define xixcore_ByteswapUint16(Data)	\
(xc_uint16)((((Data)&0x00FF) << 8) | (((Data)&0xFF00) >> 8))


#define xixcore_ByteswapUint32(Data)	\
(xc_uint32) ( (((Data)&0x000000FF) << 24) | (((Data)&0x0000FF00) << 8) \
			 | (((Data)&0x00FF0000)  >> 8) | (((Data)&0xFF000000) >> 24))

#define xixcore_ByteswapUint64(Data)    \
(xc_uint64) ( (((Data)&0x00000000000000FFULL) << 56) | (((Data)&0x000000000000FF00ULL) << 40) \
			 | (((Data)&0x0000000000FF0000ULL) << 24) | (((Data)&0x00000000FF000000ULL) << 8)  \
			 | (((Data)&0x000000FF00000000ULL) >> 8)  | (((Data)&0x0000FF0000000000ULL) >> 24) \
			 | (((Data)&0x00FF000000000000ULL) >> 40) | (((Data)&0xFF00000000000000ULL) >> 56))

#else

XIXCORE_INLINE
unsigned short xixcore_ByteswapUint16(unsigned short i)
{
#if defined(_X86_) && !defined(_NO_INLINE_ASM)
	__asm 
	{
		mov ax, i
		xchg al, ah
	}
#else
	xc_uint16 j;
	j =  (i << 8) ;
	j += (i >> 8) ;
	return j;
#endif
}

XIXCORE_INLINE
unsigned long xixcore_ByteswapUint32(unsigned long i)
{
#if defined(_X86_) && !defined(_NO_INLINE_ASM_)
	__asm 
	{
		mov eax, i
		bswap eax
	}
#else
	xc_uint32 j;
	j =  (i << 24);
	j += (i <<  8) & 0x00FF0000;
	j += (i >>  8) & 0x0000FF00;
	j += (i >> 24);
	return j;
#endif
}

XIXCORE_INLINE
xc_uint64 xixcore_ByteswapUint64(xc_uint64 i)
{
	xc_uint64 j;
	j =  (i << 56);
	j += (i << 40)&0x00FF000000000000LL;
	j += (i << 24)&0x0000FF0000000000LL;
	j += (i <<  8)&0x000000FF00000000LL;
	j += (i >>  8)&0x00000000FF000000LL;
	j += (i >> 24)&0x0000000000FF0000LL;
	j += (i >> 40)&0x000000000000FF00LL;
	j += (i >> 56);
	return j;
	
}

#endif


#if defined( __XIXCORE_BYTEORDER_BIG__ )

#define XIXCORE_CPU2BE16p(x)	(*(x) = (xc_uint16)*(x))
#define XIXCORE_CPU2BE32p(x)	(*(x) = (xc_uint32)*(x))
#define XIXCORE_CPU2BE64p(x)	(*(x) = (xc_uint32)*(x))
#define XIXCORE_BE2CPU16p(x)	(*(x) = (xc_uint16)*(x))
#define XIXCORE_BE2CPU32p(x)	(*(x) = (xc_uint32)*(x))
#define XIXCORE_BE2CPU64p(x)	(*(x) = (xc_uint64)*(x))
#define XIXCORE_CPU2LE16p(x)	(*(x) = xixcore_ByteswapUint16(*(x)))
#define XIXCORE_CPU2LE32p(x)	(*(x) = xixcore_ByteswapUint32(*(x)))
#define XIXCORE_CPU2LE64p(x)	(*(x) = xixcore_ByteswapUint64(*(x)))
#define XIXCORE_LE2CPU16p(x)	(*(x) = xixcore_ByteswapUint16(*(x)))
#define XIXCORE_LE2CPU32p(x)	(*(x) = xixcore_ByteswapUint32(*(x)))
#define XIXCORE_LE2CPU64p(x)	(*(x) = xixcore_ByteswapUint64(*(x)))


#define XIXCORE_CPU2BE16(x)		((xc_uint16)(x))
#define XIXCORE_CPU2BE32(x)		((xc_uint32)(x))
#define XIXCORE_CPU2BE64(x) 	((xc_uint64)(x))
#define XIXCORE_BE2CPU16(x)		((xc_uint16)(x))
#define XIXCORE_BE2CPU32(x)		((xc_uint32)(x))
#define XIXCORE_BE2CPU64(x) 	((xc_uint64)(x))
#define XIXCORE_CPU2LE16(x)		xixcore_ByteswapUint16(x)
#define XIXCORE_CPU2LE32(x)		xixcore_ByteswapUint32(x)
#define XIXCORE_CPU2LE64(x) 	xixcore_ByteswapUint64(x)
#define XIXCORE_LE2CPU16(x)		xixcore_ByteswapUint16(x)
#define XIXCORE_LE2CPU32(x)		xixcore_ByteswapUint32(x)
#define XIXCORE_LE2CPU64(x) 	xixcore_ByteswapUint64(x)

#elif defined( __XIXCORE_BYTEORDER_LITTLE__ )

#define XIXCORE_CPU2BE16p(x)	(*(x) = xixcore_ByteswapUint16(*(x)))
#define XIXCORE_CPU2BE32p(x)	(*(x) = xixcore_ByteswapUint32(*(x)))
#define XIXCORE_CPU2BE64p(x)	(*(x) = xixcore_ByteswapUint64(*(x)))
#define XIXCORE_BE2CPU16p(x)	(*(x) = xixcore_ByteswapUint16(*(x)))
#define XIXCORE_BE2CPU32p(x)	(*(x) = xixcore_ByteswapUint32(*(x)))
#define XIXCORE_BE2CPU64p(x)	(*(x) = xixcore_ByteswapUint64(*(x)))
#define XIXCORE_CPU2LE16p(x)	(*(x) = (xc_uint16)*(x))
#define XIXCORE_CPU2LE32p(x)	(*(x) = (xc_uint32)*(x))
#define XIXCORE_CPU2LE64p(x)	(*(x) = (xc_uint32)*(x))
#define XIXCORE_LE2CPU16p(x)	(*(x) = (xc_uint16)*(x))
#define XIXCORE_LE2CPU32p(x)	(*(x) = (xc_uint32)*(x))
#define XIXCORE_LE2CPU64p(x)	(*(x) = (xc_uint64)*(x))

#define XIXCORE_CPU2BE16(x)		xixcore_ByteswapUint16(x)
#define XIXCORE_CPU2BE32(x)		xixcore_ByteswapUint32(x)
#define XIXCORE_CPU2BE64(x) 	xixcore_ByteswapUint64(x)
#define XIXCORE_BE2CPU16(x)		xixcore_ByteswapUint16(x)
#define XIXCORE_BE2CPU32(x)		xixcore_ByteswapUint32(x)
#define XIXCORE_BE2CPU64(x) 	xixcore_ByteswapUint64(x)
#define XIXCORE_CPU2LE16(x)		((xc_uint16)(x))
#define XIXCORE_CPU2LE32(x)		((xc_uint32)(x))
#define XIXCORE_CPU2LE64(x) 	((xc_uint64)(x))
#define XIXCORE_LE2CPU16(x)		((xc_uint16)(x))
#define XIXCORE_LE2CPU32(x)		((xc_uint32)(x))
#define XIXCORE_LE2CPU64(x) 	((xc_uint64)(x))

#else

#error "No byte order specified."

#endif

#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif


#ifndef XC_CASSERT
#define XC_CASSERT(t, e) typedef char __C_ASSERT__##t[(e)?1:-1]
#endif
#define XC_CASSERT_SIZEOF(type, size) XC_CASSERT(type, sizeof(type) == size)

/*
 * Xixcore interrupt request level
 */

typedef int XIXCORE_IRQL, *PXIXCORE_IRQL;

/*
 * Sector address
 */
#ifdef __XIXCORE_BLOCK_LONG_ADDRESS__

typedef xc_uint64	xc_sector_t;

#else

typedef xc_uint32	xc_sector_t;

#endif

/*
 * Xixcore error code
 */

typedef xc_int32 XCCODE, *PXCCODE;

/*
 * NULL pointer value
 */

#undef NULL
#if defined(__cplusplus)
#define NULL 0
#else
#define NULL ((void *)0)
#endif

/*
 * XIX file system string
 */
#define XIXFS_STRING					"XIXFS"

/*
 * Flag macros
 */

#define XIXCORE_SET_FLAGS(flags, value)		(	(flags) |= (value)	)
#define XIXCORE_CLEAR_FLAGS(flags, value)		(	(flags) &= ~(value)	)
#define XIXCORE_TEST_FLAGS(flags, value)		(	( (flags) & (value) )? 1: 0 )
#define XIXCORE_MASK_FLAGS(flags, value)		(	(flags) & (value) )

/*
 *  doubly linked list
 */

typedef struct _XIXCORE_LISTENTRY {
	struct _XIXCORE_LISTENTRY *next, *prev;
} XIXCORE_LISTENTRY, *PXIXCORE_LISTENTRY;

XIXCORE_INLINE void xixcore_InitializeListHead(XIXCORE_LISTENTRY *listhead)
{
	listhead->next = listhead;
	listhead->prev = listhead;
}

XIXCORE_INLINE void __xixcore_AddListEntry(XIXCORE_LISTENTRY *new,
			      XIXCORE_LISTENTRY *prev,
			      XIXCORE_LISTENTRY *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

XIXCORE_INLINE void xixcore_AddListEntryToHead(XIXCORE_LISTENTRY *new, XIXCORE_LISTENTRY *head)
{
	__xixcore_AddListEntry(new, head, head->next);
}

XIXCORE_INLINE void xixcore_AddListEntryToTail(XIXCORE_LISTENTRY *new, XIXCORE_LISTENTRY *head)
{
	__xixcore_AddListEntry(new, head->prev, head);
}

XIXCORE_INLINE void __xixcore_DelListEntry(XIXCORE_LISTENTRY * prev, XIXCORE_LISTENTRY * next)
{
	next->prev = prev;
	prev->next = next;
}

#define XIXCORE_LIST_INVALID_NEXT	((XIXCORE_LISTENTRY *)0x11)
#define XIXCORE_LIST_INVALID_PREV	((XIXCORE_LISTENTRY *)0x22)
XIXCORE_INLINE void xixcore_RemoveListEntry(XIXCORE_LISTENTRY *entry)
{
	__xixcore_DelListEntry(entry->prev, entry->next);
	entry->next = XIXCORE_LIST_INVALID_NEXT;
	entry->prev = XIXCORE_LIST_INVALID_PREV;
}

#endif // #ifdef __XIXCORE_SYSTEM_PRIMITIVE_TYPE_H__
