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
#ifndef __XIXCORE_FILE_STUB_H__
#define __XIXCORE_FILE_STUB_H__

#include "xixcore/xctypes.h"
#include "xixcore/bitmap.h"

#ifndef FILE_SHARE_READ
/*
 * File open mode
 */
#define FILE_SHARE_READ                 0x00000001  // winnt
#define FILE_SHARE_WRITE                0x00000002  // winnt
#define FILE_SHARE_DELETE               0x00000004  // winnt
#define FILE_SHARE_VALID_FLAGS          0x00000007

/*
 * File attributes
 */

#define FILE_ATTRIBUTE_READONLY            	(0x00000001)  
#define FILE_ATTRIBUTE_HIDDEN               		(0x00000002)  
#define FILE_ATTRIBUTE_SYSTEM               		(0x00000004)  

#define FILE_ATTRIBUTE_DIRECTORY            	(0x00000010)  
#define FILE_ATTRIBUTE_ARCHIVE              		(0x00000020)  
#define FILE_ATTRIBUTE_DEVICE               		(0x00000040)  
#define FILE_ATTRIBUTE_NORMAL               		(0x00000080)  

#define FILE_ATTRIBUTE_TEMPORARY            	(0x00000100)  
#define FILE_ATTRIBUTE_SPARSE_FILE          	(0x00000200)  
#define FILE_ATTRIBUTE_REPARSE_POINT        	(0x00000400)  
#define FILE_ATTRIBUTE_COMPRESSED           	(0x00000800)  

#define FILE_ATTRIBUTE_OFFLINE              		(0x00001000)  
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  (0x00002000)  
#define FILE_ATTRIBUTE_ENCRYPTED            	(0x00004000)  
#endif //#ifndef FILE_SHARE_READ
/*
 * FCB types
 */
#define XIXCORE_SYM_LINK_MASK	(0x80000000)

#define FCB_TYPE_FILE		(0x000000001)
#define FCB_TYPE_DIR		(0x000000002)
#define FCB_TYPE_VOLUME	(0x000000003)

/*
 *	FCB Flags
 */
#define	XIXCORE_FCB_INIT							(0x00000001)
#define	XIXCORE_FCB_TEARDOWN						(0x00000002)
#define XIXCORE_FCB_IN_TABLE						(0x00000004)


//		Open States
#define	XIXCORE_FCB_OPEN_REF						(0x00000010)
#define	XIXCORE_FCB_OPEN_READ_ONLY					(0x00000020)
#define XIXCORE_FCB_OPEN_WRITE						(0x00000040)
#define	XIXCORE_FCB_OPEN_TEMPORARY					(0x00000080)



//		Operation States
#define	XIXCORE_FCB_OP_WRITE_THROUGH				(0x00000100)
#define	XIXCORE_FCB_OP_FAST_IO_READ_IN_PROGESS		(0x00000200)
#define	XIXCORE_FCB_OP_FAST_IO_WRITE_IN_PROGESS		(0x00000400)



//		Additional File States
#define	XIXCORE_FCB_CACHED_FILE						(0x00001000)
#define	XIXCORE_FCB_DELETE_ON_CLOSE					(0x00002000)
#define XIXCORE_FCB_OUTOF_LINK						(0x00004000)



//		File State Change Flag
//		File Header Change
#define	XIXCORE_FCB_MODIFIED_ALLOC_SIZE				(0x00010000)
#define	XIXCORE_FCB_MODIFIED_FILE_SIZE				(0x00020000)
#define	XIXCORE_FCB_MODIFIED_FILE_NAME				(0x00040000)
#define	XIXCORE_FCB_MODIFIED_FILE_TIME				(0x00080000)
#define	XIXCORE_FCB_MODIFIED_FILE_ATTR				(0x00100000)


#define	XIXCORE_FCB_MODIFIED_PATH					(0x00200000)
#define	XIXCORE_FCB_MODIFIED_LINKCOUNT				(0x00400000)
#define XIXCORE_FCB_MODIFIED_ADDR_BLOCK				(0x00800000)
#define XIXCORE_FCB_MODIFIED_FILE					(0x00FF2000)


#define	XIXCORE_FCB_MODIFIED_CHILDCOUNT				(0x00800000)
#define	XIXCORE_FCB_CHANGE_HEADER					(0x00FF0000)



//		File State Change
#define XIXCORE_FCB_CHANGE_DELETED					(0x01000000)
#define XIXCORE_FCB_CHANGE_RELOAD					(0x02000000)
#define	XIXCORE_FCB_CHANGE_RENAME					(0x04000000)
#define	XIXCORE_FCB_CHANGE_LINK						(0x08000000)
#define XIXCORE_FCB_LOCKED							(0x10000000)
#define XIXCORE_FCB_EOF_IO							(0x20000000)
#define	XIXCORE_FCB_NOT_FROM_POOL					(0x80000000)



//		FCB state
#define 	XIFS_STATE_FCB_NORM							(0x00000000)
#define 	XIFS_STATE_FCB_LOCKED						(0x00000001)				
#define 	XIFS_STATE_FCB_INVALID						(0x00000002)
#define 	XIFS_STATE_FCB_NEED_RELOAD					(0x00000004)

// File lock state
#define INODE_FILE_LOCK_INVALID		0x00000000
#define INODE_FILE_LOCK_HAS			0x00000001
#define INODE_FILE_LOCK_OTHER_HAS		0x00000002

typedef struct _XIXCORE_FCB {
	XIXCORE_NODE_TYPE NodeType;

	xc_uint32			FileAttribute;
	xc_uint32			DesiredAccess;
	xc_uint8			*FCBName;
	xc_uint32			FCBNameLength;			
	xc_uint32			FCBType;	

	PXIXCORE_SEMAPHORE	AddrLock;

	xc_uint64			Access_time;
	xc_uint64			Modified_time;
	xc_uint64			Create_time;
	xc_uint64			RealAllocationSize;
	xc_uint64			AllocationSize;
	
	xc_uint32			FCBFlags;
	xc_uint32			HasLock;
	
	xc_uint64			LotNumber;
	xc_uint64			ParentLotNumber;
	xc_uint64			AddrLotNumber;	
	
	xc_int64			WriteStartOffset;

	xc_uint32			Type;
	xc_uint32			LinkCount;
	xc_uint32 			AddrStartSecIndex;
	PXIXCORE_BUFFER		AddrLot;
	xc_uint32			AddrLotSize;
	// used by dir
	xc_uint32			ChildCount;


} XIXCORE_FCB, *PXIXCORE_FCB;


xc_int32
xixcore_call
xixcore_InitializeFCB(
	PXIXCORE_FCB		XixcoreFCB,
	PXIXCORE_VCB		XixcoreVCB,
	PXIXCORE_SEMAPHORE	XixcoreSemaphore
	);

void
xixcore_call
xixcore_DestroyFCB(
	PXIXCORE_FCB		XixcoreFCB
	);

int
xixcore_call
xixcore_CheckFileDirFromBitMapContext(
	PXIXCORE_VCB VCB,
	PXIXCORE_BITMAP_EMUL_CONTEXT BitmapEmulCtx
);

struct _XIXCORE_DIR_EMUL_CONTEXT;

int
xixcore_call
xixcore_CreateNewFile(
	PXIXCORE_VCB 					pVCB, 
	PXIXCORE_FCB 					pDir,
	xc_uint32						bIsFile,
	xc_uint8 *						Name, 
	xc_uint32 						NameLength, 
	xc_uint32 						FileAttribute,
	struct _XIXCORE_DIR_EMUL_CONTEXT *DirContext,
	xc_uint64						*NewFileId
);

#endif // #ifndef __XIXCORE_FILE_H__
