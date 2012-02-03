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
#ifndef __XIXCORE_EVENT_PAKCETS_H__
#define __XIXCORE_EVENT_PAKCETS_H__

#define XIXFS_DATAGRAM_PROTOCOL	{ 'X', 'I', 'F', 'S' }
#define	XIXFS_TYPE_UNKOWN					0x00000000	
#define	XIXFS_TYPE_LOCK_REQUEST				0x00000001	//	request packet
#define	XIXFS_TYPE_LOCK_REPLY				0x00000002	//	reply packet
#define	XIXFS_TYPE_LOCK_BROADCAST			0x00000004	//	broadcasting packet
#define XIXFS_TYPE_FLUSH_BROADCAST			0x00000008	//	Flush range
#define	XIXFS_TYPE_DIR_CHANGE				0x00000010	// 	Dir Entry change
#define		XIXFS_SUBTYPE_DIR_ADD				0x00000001	//		Add
#define		XIXFS_SUBTYPE_DIR_DEL				0x00000002	//		Del
#define		XIXFS_SUBTYPE_DIR_MOD				0x00000003	//		Mod
#define	XIXFS_TYPE_FILE_LENGTH_CHANGE		0x00000020	//	File Length change
#define	XIXFS_TYPE_FILE_CHANGE				0x00000040	//	File Change
#define		XIXFS_SUBTYPE_FILE_DEL				0x00000001	//		File del
#define		XIXFS_SUBTYPE_FILE_MOD				0x00000002	//		File Mod
#define		XIXFS_SUBTYPE_FILE_RENAME			0x00000003	//		File Rename
#define		XIXFS_SUBTYPE_FILE_LINK				0x00000004	//		File Link
#define	XIXFS_TYPE_FILE_RANGE_LOCK			0x00000080
#define		XIXFS_SUBTYPE_FILE_RLOCK_ACQ		0x00000001
#define		XIXFS_SUBTYPE_FILE_RLOCK_REL		0x00000002
#define XIXFS_TYPE_MASK						0x000000FF


#define  XIXFS_PROTO_MAJOR_VERSION			0x0
#define  XIXFS_PROTO_MINOR_VERSION			0x1

#define	DEFAULT_XIXFS_SVRPORT				((unsigned short)0x1000)


#include "xcsystem/pshpack8.h"

typedef  struct _XIXFS_COMM_HEADER {	
		xc_uint8		Protocol[4];		//4
		xc_uint32		_alignment_1_;		//8
		xc_uint8		SrcMac[32];			//40
		xc_uint8		DstMac[32];			//72
		xc_uint32		XifsMajorVersion;	//76
		xc_uint32		XifsMinorVersion;	//80
		xc_uint32		Type;				//84
		xc_uint32		MessageSize; //Total Message Size (include Header size)	88
}  XIXFS_COMM_HEADER, *PXIXFS_COMM_HEADER;

XC_CASSERT_SIZEOF(XIXFS_COMM_HEADER, 88);


typedef  struct _XIXFS_LOCK_REQUEST{
		xc_uint8		VolumeId[16];		//16
		xc_uint32	 	Reserved32;		//20	
		xc_uint32		_alignment_1_;	//24
		xc_uint64	 	LotNumber;		//32
		xc_uint32		PacketNumber;	//36
		xc_uint32		_alignment_2_;	//40
		xc_uint8		LotOwnerID[16];	//56
		xc_uint8		LotOwnerMac[32];//88
		xc_uint8		Reserved[40];	//128
}  XIXFS_LOCK_REQUEST, *PXIXFS_LOCK_REQUEST;

XC_CASSERT_SIZEOF(XIXFS_LOCK_REQUEST, 128);

typedef  struct _XIXFS_LOCK_REPLY{
		xc_uint8		VolumeId[16];		//16
		xc_uint32	 	Reserved32;		//20
		xc_uint32		_alignment_1_;	//24
		xc_uint64	 	LotNumber;		//32
		xc_uint32		PacketNumber;	//36
		xc_uint32		LotState;		//40
		xc_uint8		Reserved[88];	//128
}  XIXFS_LOCK_REPLY, *PXIXFS_LOCK_REPLY;

XC_CASSERT_SIZEOF(XIXFS_LOCK_REPLY, 128);

typedef  struct _XIXFS_FILE_CHANGE_BROADCAST {
		xc_uint8		VolumeId[16];				//16
		xc_uint32	 	Reserved32;				//20
		xc_uint32		_alignment_1_;			//24
		xc_uint64	 	LotNumber;				//32
		xc_uint32		SubCommand;				//36
		xc_uint32		_alignment_2_;			//40
		xc_uint64		PrevParentLotNumber;	//48
		xc_uint64		NewParentLotNumber;		//56 		
		xc_uint8		Reserved[72];			//128
}  XIXFS_FILE_CHANGE_BROADCAST, *PXIXFS_FILE_CHANGE_BROADCAST;

XC_CASSERT_SIZEOF(XIXFS_FILE_CHANGE_BROADCAST, 128);


typedef  struct _XIXFS_LOCK_BROADCAST{
		xc_uint8		VolumeId[16];		//16
		xc_uint32	 	Reserved32;		//20
		xc_uint32		_alignment_1_;	//24
		xc_uint64	 	LotNumber;		//32
		xc_uint8		Reserved[96];	//128
} XIXFS_LOCK_BROADCAST, *PXIXFS_LOCK_BROADCAST;

XC_CASSERT_SIZEOF(XIXFS_LOCK_BROADCAST, 128);

typedef  struct _XIXFS_RANGE_FLUSH_BROADCAST{
		xc_uint8		VolumeId[16];		//16	
		xc_uint32	 	Reserved32;		//20
		xc_uint32		_alignment_1_;	//24
		xc_uint64	 	LotNumber;		//32
		xc_uint64		StartOffset;	//40
		xc_uint32		DataSize;		//44
		xc_uint32		_alignment_2_;	//48
		xc_uint8		Reserved[80];	//128
}  XIXFS_RANGE_FLUSH_BROADCAST, *PXIXFS_RANGE_FLUSH_BROADCAST;

XC_CASSERT_SIZEOF(XIXFS_RANGE_FLUSH_BROADCAST, 128);

typedef  struct _XIXFS_FILE_LENGTH_CHANGE_BROADCAST {
		xc_uint8		VolumeId[16];			//16	
		xc_uint32	 	Reserved32;			//20
		xc_uint32		_alignment_1_;		//24
		xc_uint64	 	LotNumber;			//32
		xc_uint64		FileLength;			//40
		xc_uint64		AllocationLength;	//48
		xc_uint64		WriteStartOffset;	//56
		xc_uint8		Reserved[72];		//128
}  XIXFS_FILE_LENGTH_CHANGE_BROADCAST, *PXIXFS_FILE_LENGTH_CHANGE_BROADCAST;

XC_CASSERT_SIZEOF(XIXFS_FILE_LENGTH_CHANGE_BROADCAST, 128);

typedef  struct _XIXFS_DIR_ENTRY_CHANGE_BROADCAST {
		xc_uint8		VolumeId[16];			//16
		xc_uint32	 	Reserved32;			//20
		xc_uint32		_alignment_1_;		//24
		xc_uint64	 	LotNumber;			//32
		xc_uint32		ChildSlotNumber;	//36
		xc_uint32		SubCommand;			//40
		xc_uint8		Reserved[88];		//128
}  XIXFS_DIR_ENTRY_CHANGE_BROADCAST, *PXIXFS_DIR_ENTRY_CHANGE_BROADCAST;


XC_CASSERT_SIZEOF(XIXFS_DIR_ENTRY_CHANGE_BROADCAST, 128);


typedef  struct _XIXFS_FILE_RANGE_LOCK_BROADCAST{
		xc_uint8		VolumeId[16];			//16
		xc_uint32	 	Reserved32;			//20
		xc_uint32		_alignment_1_;		//24
		xc_uint64	 	LotNumber;			//32
		xc_uint64		StartOffset;		//40
		xc_uint32		DataSize;			//44
		xc_uint32		SubCommand;			//48
		xc_uint8		Reserved[80];		//128
}  XIXFS_FILE_RANGE_LOCK_BROADCAST, *PXIXFS_FILE_RANGE_LOCK_BROADCAST;

XC_CASSERT_SIZEOF(XIXFS_FILE_RANGE_LOCK_BROADCAST, 128);

#include "xcsystem/poppack.h"

#endif // __XIXCORE_EVENT_PAKCETS_H__
