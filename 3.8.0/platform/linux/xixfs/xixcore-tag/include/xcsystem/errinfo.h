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
#ifndef __XIXCORE_ERROR_INFO_H__
#define __XIXCORE_ERROR_INFO_H__

#include "xcsystem/system.h"

/*
 * This file defines error code for Xixcore.
 * Xixcore code is compatible to the local system code
 */

XIXCORE_INLINE
XCCODE
XccodeIndexToSystemCode(int XccodeIndex)
{
	if(XccodeIndex >= XCCODE_MAX)
		return -1;

	return XixcoreErrorCodeLookup[XccodeIndex];
}


//
// Success code
// Assume success code is zero or positive value.
//

#define XCCODE_SUCCESS		XccodeIndexToSystemCode(0)
#define XCCODE_UNSUCCESS	XccodeIndexToSystemCode(35)

// Came from Linux kernel
#define	XCCODE_EPERM		XccodeIndexToSystemCode(1)		/* Operation not permitted */
#define	XCCODE_ENOENT		XccodeIndexToSystemCode(2)		/* No such file or directory */
#define	XCCODE_ESRCH		XccodeIndexToSystemCode(3)		/* No such process */
#define	XCCODE_EINTR		XccodeIndexToSystemCode(4)		/* Interrupted system call */
#define	XCCODE_EIO			XccodeIndexToSystemCode(5)		/* I/O error */
#define	XCCODE_ENXIO		XccodeIndexToSystemCode(6)		/* No such device or address */
#define	XCCODE_E2BIG		XccodeIndexToSystemCode(7)		/* Argument list too long */
#define	XCCODE_ENOEXEC		XccodeIndexToSystemCode(8)		/* Exec format error */
#define	XCCODE_EBADF		XccodeIndexToSystemCode(9)		/* Bad file number */
#define	XCCODE_ECHILD		XccodeIndexToSystemCode(10)		/* No child processes */
#define	XCCODE_EAGAIN		XccodeIndexToSystemCode(11)		/* Try again */
#define	XCCODE_ENOMEM		XccodeIndexToSystemCode(12)		/* Out of memory */
#define	XCCODE_EACCES		XccodeIndexToSystemCode(13)		/* Permission denied */
#define	XCCODE_EFAULT		XccodeIndexToSystemCode(14)		/* Bad address */
#define	XCCODE_ENOTBLK		XccodeIndexToSystemCode(15)		/* Block device required */
#define	XCCODE_EBUSY		XccodeIndexToSystemCode(16)		/* Device or resource busy */
#define	XCCODE_EEXIST		XccodeIndexToSystemCode(17)		/* File exists */
#define	XCCODE_EXDEV		XccodeIndexToSystemCode(18)		/* Cross-device link */
#define	XCCODE_ENODEV		XccodeIndexToSystemCode(19)		/* No such device */
#define	XCCODE_ENOTDIR		XccodeIndexToSystemCode(20)		/* Not a directory */
#define	XCCODE_EISDIR		XccodeIndexToSystemCode(21)		/* Is a directory */
#define	XCCODE_EINVAL		XccodeIndexToSystemCode(22)		/* Invalid argument */
#define	XCCODE_ENFILE		XccodeIndexToSystemCode(23)		/* File table overflow */
#define	XCCODE_EMFILE		XccodeIndexToSystemCode(24)		/* Too many open files */
#define	XCCODE_ENOTTY		XccodeIndexToSystemCode(25)		/* Not a typewriter */
#define	XCCODE_ETXTBSY		XccodeIndexToSystemCode(26)		/* Text file busy */
#define	XCCODE_EFBIG		XccodeIndexToSystemCode(27)		/* File too large */
#define	XCCODE_ENOSPC		XccodeIndexToSystemCode(28)		/* No space left on device */
#define	XCCODE_ESPIPE		XccodeIndexToSystemCode(29)		/* Illegal seek */
#define	XCCODE_EROFS		XccodeIndexToSystemCode(30)		/* Read-only file system */
#define	XCCODE_EMLINK		XccodeIndexToSystemCode(31)		/* Too many links */
#define	XCCODE_EPIPE		XccodeIndexToSystemCode(32)		/* Broken pipe */
#define	XCCODE_EDOM			XccodeIndexToSystemCode(33)		/* Math argument out of domain of func */
#define	XCCODE_ERANGE		XccodeIndexToSystemCode(34)		/* Math result not representable */
#define XCCODE_CRCERROR		XccodeIndexToSystemCode(36)		/* OnDisk structure is not valid */


/*
 * Xixcore reason code
 */

// Severity
#define	XCREASON_PREFIX_SUCCESS			(0x00000000) // 0
#define	XCREASON_PREFIX_INFORMATION		(0x40000000) // 1
#define	XCREASON_PREFIX_WARNING			(0x80000000) // 2
#define	XCREASON_PREFIX_ERROR			(0xC0000000) // 3
#define	XCREASON_PREFIX_MASK			(0xC0000000)

// Reserved
#define XCREASON_RESERVED_MASK			(0x30000000)

// Facility
#define	XCREASON_FAC_GENERIC			(0x00000000)
#define XCREASON_FAC_BUFFER				(0x00100000)
#define	XCREASON_FAC_LOT				(0x00200000)
#define	XCREASON_FAC_ADDR				(0x00300000)
#define	XCREASON_FAC_RAW_IO				(0x00400000)
#define	XCREASON_FAC_MASK				(0x0FF00000)

// Code
#define	XCREASON_MASK					(0x000FFFFF)

#define XCMAKE_SUCCESS(FACILITY,BARE_CODE)	((XCREASON_PREFIX_SUCCESS)|(FACILITY) | (BARE_CODE))
#define XCMAKE_WARNING(FACILITY,BARE_CODE)	((XCREASON_PREFIX_WARNING)|(FACILITY) | (BARE_CODE))
#define XCMAKE_INFORMATION(FACILITY,BARE_CODE) ((XCREASON_PREFIX_INFORMATION)|(FACILITY) | (BARE_CODE))
#define XCMAKE_ERROR(FACILITY,BARE_CODE)	((XCREASON_PREFIX_ERROR)|(FACILITY) | (BARE_CODE))

#define XCREASON_GET_CODE(XCCODE) ((int)((XCCODE)&XCCODE_MASK))

//
// Buffer facility code
//

#define XCREASON_BUF_SIZE_SMALL		XCMAKE_WARNING(XCREASON_FAC_BUFFER, 1)
#define XCREASON_BUF_NOT_ALLOCATED	XCMAKE_WARNING(XCREASON_FAC_BUFFER, 2)

//
// Lot facility code
//

#define	XCREASON_LOT_TYPE_WARN		XCMAKE_WARNING(XCREASON_FAC_LOT, 1)
#define	XCREASON_LOT_FLAG_WARN		XCMAKE_WARNING(XCREASON_FAC_LOT, 2)
#define	XCREASON_LOT_INDEX_WARN		XCMAKE_WARNING(XCREASON_FAC_LOT, 4)
#define XCREASON_LOT_SIGNATURE_WARN	XCMAKE_WARNING(XCREASON_FAC_LOT, 5)

//
// Address facility code
//

#define	XCREASON_ADDR_INVALID			XCMAKE_WARNING(XCREASON_FAC_ADDR, 1)
#define	XCREASON_ADDR_INSUFFICENT_LOT	XCMAKE_WARNING(XCREASON_FAC_ADDR, 2)

//
// Raw IO facility code
//

#define XCREASON_RAWIO_ERROR_OP		XCMAKE_ERROR(XCREASON_FAC_RAW_IO, 1)
#define XCREASON_RAWIO_ERROR_SUBMIT	XCMAKE_ERROR(XCREASON_FAC_RAW_IO, 2)

#endif //#ifndef __XIXCORE_ERROR_INFO_H__
