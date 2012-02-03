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
#include "linux_ver.h"
#if LINUX_VERSION_25_ABOVE
#include <linux/fs.h> // blkdev_ioctl , register file system
#include <linux/buffer_head.h> // file_fsync
#include <linux/genhd.h>  // rescan_parition
#include <linux/workqueue.h>  // 
#include <linux/mpage.h>
#include <linux/writeback.h>
#else
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/spinlock.h>
#include <linux/nls.h>

#include <asm/uaccess.h>
#include <asm/div64.h>

#include "xcsystem/debug.h"
#include "xcsystem/errinfo.h"
#include "xcsystem/system.h"
#include "xixcore/callback.h"
#include "xixcore/layouts.h"
#include "xixcore/ondisk.h"
#include "xixcore/volume.h"
#include "xixcore/hostreg.h"
#include "xixcore/bitmap.h"
#include "xixcore/lotinfo.h"
#include "xixcore/lotlock.h"
#include "xixcore/fileaddr.h"
#include "xixcore/lotaddr.h"

#include "xixfs_global.h"
#include "xixfs_xbuf.h"
#include "xixfs_drv.h"
#include "xixfs_name_cov.h"
#include "xixfs_ndasctl.h"
#include "xixfs_event_op.h"
#include "file.h"
#include "dir.h"
#include "inode.h"
#include "misc.h"

/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSINODE"

int
xixfs_fill_inode ( 
		struct super_block *sb,
		struct inode *inode,
		uint32		IsDir,
		uint64		LotNumber
);

//////////////////////////////////////////////////////////////////////////
//
//
//				inode hash table operation
//	
//
//////////////////////////////////////////////////////////////////////////

void 
xixfs_inode_hash_init(
	struct super_block *sb
)
{
	PXIXFS_LINUX_VCB	pVCB = NULL;
	int i;

	XIXCORE_ASSERT(sb);

	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);
	

	spin_lock_init(&pVCB->inode_hash_lock);
	
	for (i = 0; i < XIXFS_HASH_SIZE; i++){
#if LINUX_VERSION_25_ABOVE		
		INIT_HLIST_HEAD(&pVCB->inode_hashtable[i]);
#else
		INIT_LIST_HEAD(&pVCB->inode_hashtable[i]);
#endif
	}
}



inline 
unsigned long 
xixfs_inode_hash(
		struct super_block *sb, 
		uint64 LotNumber
)
{
	unsigned long tmp = (unsigned long)LotNumber | (unsigned long) sb;
	tmp = tmp + (tmp >> XIXFS_HASH_BITS) + (tmp >> XIXFS_HASH_BITS * 2);
	return tmp & XIXFS_HASH_MASK;
}

void 
xixfs_inode_attach(
		struct inode *inode
)
{
	struct super_block *sb = NULL;
	PXIXFS_LINUX_VCB pVCB = NULL;
	PXIXFS_LINUX_FCB pFCB = NULL;
	uint64	LotNumber = 0;

	
	XIXCORE_ASSERT(inode);
	XIXCORE_ASSERT(inode->i_sb);

	sb = inode->i_sb;
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);

	LotNumber = pFCB->XixcoreFcb.LotNumber;

	spin_lock(&pVCB->inode_hash_lock);
#if LINUX_VERSION_25_ABOVE		
	hlist_add_head(&(pFCB->FCB_hash),
			pVCB->inode_hashtable + xixfs_inode_hash(sb, LotNumber));
#else
	list_add(&(pFCB->FCB_hash),
			pVCB->inode_hashtable + xixfs_inode_hash(sb, LotNumber));
#endif
	spin_unlock(&pVCB->inode_hash_lock);
}



void 
xixfs_inode_detach(
	struct inode *inode
)
{

	struct super_block *sb = NULL;
	PXIXFS_LINUX_VCB pVCB = NULL;
	PXIXFS_LINUX_FCB pFCB = NULL;

	XIXCORE_ASSERT(inode);
	XIXCORE_ASSERT(inode->i_sb);

	sb = inode->i_sb;
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);

	spin_lock(&(pVCB->inode_hash_lock));
#if LINUX_VERSION_25_ABOVE	
	hlist_del_init(&(pFCB->FCB_hash));
#else
	list_del_init(&(pFCB->FCB_hash));	
#endif
	spin_unlock(&(pVCB->inode_hash_lock));
}



struct inode *
xixfs_find_inode_from_inode_hash(
	struct super_block *sb, 
	uint64 LotNumber
)
{

	PXIXFS_LINUX_VCB pVCB = NULL;
	PXIXFS_LINUX_FCB pFCB = NULL;
#if LINUX_VERSION_25_ABOVE	
	struct hlist_head *head = NULL;
	struct hlist_node *_p = NULL;
#else
	struct list_head *head = NULL;
	struct list_head *_p = NULL;
#endif

	struct inode *inode = NULL;
	

	
	XIXCORE_ASSERT(sb);

	pVCB = XIXFS_SB(sb);
	head = pVCB->inode_hashtable + xixfs_inode_hash(sb, LotNumber);


	spin_lock(&(pVCB->inode_hash_lock));
#if LINUX_VERSION_25_ABOVE		
	hlist_for_each_entry(pFCB, _p, head, FCB_hash) {
#else

	list_for_each(_p, head) {
		pFCB = list_entry(_p, XIXFS_LINUX_FCB, FCB_hash);
#endif
		XIXCORE_ASSERT(pFCB->linux_inode.i_sb == sb);
		if (pFCB->XixcoreFcb.LotNumber!= LotNumber)
			continue;
		inode = igrab(&(pFCB->linux_inode));
		if (inode)
			break;
	}
	spin_unlock(&(pVCB->inode_hash_lock));
	return inode;
}




struct inode *
xixfs_iget(
	struct super_block *sb, 
	uint64 LotNumber
)
{
	

	return xixfs_find_inode_from_inode_hash(sb, LotNumber);
}


struct inode *
xixfs_build_inode(
		struct super_block *sb,
		uint32			IsDir,
		uint64 			LotNumber
)
{
	struct inode *inode = NULL;
	int err;

	inode =xixfs_iget(sb, LotNumber);
	if (inode)
		goto out;
	inode = new_inode(sb);
	if (!inode) {
		inode = ERR_PTR(-ENOMEM);
		goto out;
	}
	inode->i_ino = iunique(sb, XIXFS_ROOT_INO);
	inode->i_version = 1;
	err = xixfs_fill_inode(sb, inode,  IsDir, LotNumber);
	if (err) {
		iput(inode);
		inode = ERR_PTR(err);
		goto out;
	}
	xixfs_inode_attach(inode);
	insert_inode_hash(inode);
out:
	return inode;
}

//////////////////////////////////////////////////////////////////////////
//
//
//				misc inode operation
//	
//
//////////////////////////////////////////////////////////////////////////


int
xixfs_read_fileInfo_from_fcb(
	PXIXFS_LINUX_VCB	pVCB,
	PXIXFS_LINUX_FCB	pFCB,
	PXIX_BUF	LotHeader,
	PXIX_BUF	Buffer,
	uint32		*FileType
)
{
	int							RC = 0;
	int 							Reason = 0;
	uint32						LotType = LOT_INFO_TYPE_INVALID;

	PXIDISK_COMMON_LOT_HEADER		pLotHeader = NULL;
	PXIDISK_FILE_INFO				pFileHeader = NULL;
	
	XIXFS_ASSERT_FCB(pFCB);
	XIXFS_ASSERT_VCB(pVCB);
	
	XIXCORE_ASSERT(LotHeader);
	XIXCORE_ASSERT(xixcore_GetBufferSize(&LotHeader->xixcore_buffer) >= XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
	XIXCORE_ASSERT(Buffer);
	XIXCORE_ASSERT(xixcore_GetBufferSize(&Buffer->xixcore_buffer) >= XIDISK_DUP_FILE_INFO_SIZE);



	xixcore_ZeroBufferOffset(&LotHeader->xixcore_buffer);
	memset(xixcore_GetDataBuffer(&LotHeader->xixcore_buffer),0, xixcore_GetBufferSize(&LotHeader->xixcore_buffer));	
	
	RC = xixcore_RawReadLotHeader(
				pVCB->XixcoreVcb.XixcoreBlockDevice,
				pVCB->XixcoreVcb.LotSize, 
				pVCB->XixcoreVcb.SectorSize,
				pVCB->XixcoreVcb.SectorSizeBit,
				pFCB->XixcoreFcb.LotNumber,
				&LotHeader->xixcore_buffer,
				&Reason
				);

	if(RC < 0 ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail (%x)  Reason(%x) xixfs_read_fileInfo_from_fcb: xixcore_RawReadLotHeader\n", RC, Reason));
		goto error_out;
	}



	xixcore_ZeroBufferOffset(&Buffer->xixcore_buffer);
	memset(xixcore_GetDataBuffer(&Buffer->xixcore_buffer),0, xixcore_GetBufferSize(&Buffer->xixcore_buffer));	


	RC = xixcore_RawReadFileHeader(
			pVCB->XixcoreVcb.XixcoreBlockDevice,
			pVCB->XixcoreVcb.LotSize, 
			pVCB->XixcoreVcb.SectorSize,
			pVCB->XixcoreVcb.SectorSizeBit,
			pFCB->XixcoreFcb.LotNumber,
			&Buffer->xixcore_buffer,
			&Reason
			);

	if(RC < 0 ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail (%x)  Reason(%x) xixfs_read_fileInfo_from_fcb: xixcore_RawReadFileHeader\n", RC, Reason));
		goto error_out;
	}



	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(&LotHeader->xixcore_buffer);
	pFileHeader = (PXIDISK_FILE_INFO)xixcore_GetDataBufferWithOffset(&Buffer->xixcore_buffer);


			

	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR) {
		LotType = LOT_INFO_TYPE_DIRECTORY;
		*FileType = FCB_TYPE_DIR;
	}else {
		LotType = LOT_INFO_TYPE_FILE;
		*FileType = FCB_TYPE_FILE;
	}
	

	RC = xixcore_CheckLotInfo(
				&pLotHeader->LotInfo,
				pVCB->XixcoreVcb.VolumeLotSignature,
				pFCB->XixcoreFcb.LotNumber,
				LotType,
				LOT_FLAG_BEGIN,
				&Reason
				);

	
	if(RC < 0 ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail (%x)  Reason(%x) xixfs_read_fileInfo_from_fcb: xixfs_CheckLotInfo\n", RC, Reason));
		goto error_out;
	}
	
error_out:
	

	return RC;
}

int
xixfs_write_fileInfo_from_fcb(
	PXIXFS_LINUX_VCB	pVCB,
	PXIXFS_LINUX_FCB	pFCB,
	PXIX_BUF			Buffer
)
{
	int							RC = 0;
	int							Reason = 0;
	
	XIXFS_ASSERT_FCB(pFCB);
	XIXFS_ASSERT_VCB(pVCB);
	

	XIXCORE_ASSERT(Buffer);
	XIXCORE_ASSERT(xixcore_GetBufferSize(&Buffer->xixcore_buffer) >= XIDISK_DUP_FILE_INFO_SIZE);

	
	RC = xixcore_RawWriteFileHeader(
				pVCB->XixcoreVcb.XixcoreBlockDevice,
				pVCB->XixcoreVcb.LotSize, 
				pVCB->XixcoreVcb.SectorSize,
				pVCB->XixcoreVcb.SectorSizeBit,
				pFCB->XixcoreFcb.LotNumber,
				&Buffer->xixcore_buffer,
				&Reason
				);


	if(RC < 0 ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail (%x)  Reason(%x) xixfs_write_fileInfo_from_fcb: xixfs_RawReadLotAndFileHeader\n", RC, Reason));
	}

	return RC;
}

int
xixfs_read_addrInfo_from_fcb(
		PXIXFS_LINUX_VCB	pVCB,
		PXIXFS_LINUX_FCB	pFCB,
		uint32		SecNum
)
{
	int RC = 0;
	int			Reason = 0;
	

	XIXFS_ASSERT_VCB(pVCB);
	XIXFS_ASSERT_FCB(pFCB);


	
	
	RC = xixcore_RawReadAddressOfFile(
				(PXIXCORE_BLOCK_DEVICE)pVCB->supreblockCtx->s_bdev, 
				pVCB->XixcoreVcb.LotSize, 
				pVCB->XixcoreVcb.SectorSize, 
				pVCB->XixcoreVcb.SectorSizeBit,
				pFCB->XixcoreFcb.LotNumber, 
				pFCB->XixcoreFcb.AddrLotNumber, 
				pFCB->XixcoreFcb.AddrLotSize,
				&pFCB->XixcoreFcb.AddrStartSecIndex, 
				SecNum, 
				pFCB->XixcoreFcb.AddrLot, 
				&Reason);

	if(RC < 0 ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(%x) Reason(%x) xixfs_read_addrInfo_from_fcb: xixfs_RawReadAddressOfFile\n", RC, Reason));
		
		goto error_out;
	}

error_out:	

	return RC;
	
}


int
xixfs_write_addrInfo_from_fcb(
		PXIXFS_LINUX_VCB	pVCB,
		PXIXFS_LINUX_FCB	pFCB,
		uint32		SecNum
)
{
	int RC = 0;
	int			Reason = 0;
	

	XIXFS_ASSERT_VCB(pVCB);
	XIXFS_ASSERT_FCB(pFCB);


	
	
	RC = xixcore_RawWriteAddressOfFile(
				(PXIXCORE_BLOCK_DEVICE)pVCB->supreblockCtx->s_bdev, 
				pVCB->XixcoreVcb.LotSize, 
				pVCB->XixcoreVcb.SectorSize, 
				pVCB->XixcoreVcb.SectorSizeBit,
				pFCB->XixcoreFcb.LotNumber, 
				pFCB->XixcoreFcb.AddrLotNumber, 
				pFCB->XixcoreFcb.AddrLotSize,
				&pFCB->XixcoreFcb.AddrStartSecIndex, 
				SecNum, 
				pFCB->XixcoreFcb.AddrLot, 
				&Reason);

	if(RC < 0 ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail(%x) Reason(%x) xixfs_read_addrInfo_from_fcb: xixfs_RawReadAddressOfFile\n", RC, Reason));
		
		goto error_out;
	}

error_out:	

	return RC;
	
}




int
xixfs_read_FCBinfo_from_LotNumber(
	PXIXFS_LINUX_VCB	pVCB,
	PXIX_BUF	LotHeader,
	PXIX_BUF	Buffer,
	uint32		IsDir,
	uint64		LotNumber
)
{
	PXIDISK_COMMON_LOT_HEADER	pLotHeader = NULL;
	PXIDISK_FILE_INFO			pFileHeader = NULL;
	int							RC = 0;
	int 							Reason = 0;
	uint32						LotType = LOT_INFO_TYPE_INVALID;
	
	XIXFS_ASSERT_VCB(pVCB);

	XIXCORE_ASSERT(LotHeader);
	XIXCORE_ASSERT(xixcore_GetBufferSize(&LotHeader->xixcore_buffer) >= XIDISK_DUP_COMMON_LOT_HEADER_SIZE);
	XIXCORE_ASSERT(Buffer);
	XIXCORE_ASSERT(xixcore_GetBufferSize(&Buffer->xixcore_buffer) >= XIDISK_DUP_FILE_INFO_SIZE);

	xixcore_ZeroBufferOffset(&LotHeader->xixcore_buffer);
	memset(xixcore_GetDataBuffer(&LotHeader->xixcore_buffer),0, xixcore_GetBufferSize(&LotHeader->xixcore_buffer));


	
	RC = xixcore_RawReadLotHeader(
				pVCB->XixcoreVcb.XixcoreBlockDevice,
				pVCB->XixcoreVcb.LotSize, 
				pVCB->XixcoreVcb.SectorSize,
				pVCB->XixcoreVcb.SectorSizeBit,
				LotNumber,
				&LotHeader->xixcore_buffer,
				&Reason
				);

	if(RC < 0 ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail (%x)  Reason(%x) xixfs_read_fileInfo_from_fcb: xixfs_RawReadLotAndFileHeader\n", RC, Reason));
		goto error_out;
	}

	

	xixcore_ZeroBufferOffset(&Buffer->xixcore_buffer);
	memset(xixcore_GetDataBuffer(&Buffer->xixcore_buffer),0, xixcore_GetBufferSize(&Buffer->xixcore_buffer));	


	RC = xixcore_RawReadFileHeader(
			pVCB->XixcoreVcb.XixcoreBlockDevice,
			pVCB->XixcoreVcb.LotSize, 
			pVCB->XixcoreVcb.SectorSize,
			pVCB->XixcoreVcb.SectorSizeBit,
			LotNumber,
			&Buffer->xixcore_buffer,
			&Reason
			);

	if(RC < 0 ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail (%x)  Reason(%x) xixfs_read_fileInfo_from_fcb: xixcore_RawReadFileHeader\n", RC, Reason));
		goto error_out;
	}


	pLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(&LotHeader->xixcore_buffer);
	pFileHeader = (PXIDISK_FILE_INFO)xixcore_GetDataBufferWithOffset(&Buffer->xixcore_buffer);

	if(IsDir == 1) {
		LotType = LOT_INFO_TYPE_DIRECTORY;
	}else {
		LotType = LOT_INFO_TYPE_FILE;
	}
	

	RC = xixcore_CheckLotInfo(
				&pLotHeader->LotInfo,
				pVCB->XixcoreVcb.VolumeLotSignature,
				LotNumber,
				LotType,
				LOT_FLAG_BEGIN,
				&Reason
				);

	
	if(RC < 0 ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail (%x)  Reason(%x) xixfs_read_fileInfo_from_fcb: xixfs_CheckLotInfo\n", RC, Reason));
		goto error_out;
	}
	
error_out:
	

	return RC;
}


int
xixfs_update_fileInfo ( 
		struct super_block *sb,
		struct inode *inode,
		uint32		IsDir,
		uint64		LotNumber
)
{
	PXIXFS_LINUX_FCB 				pFCB = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIDISK_FILE_INFO				pFileHeader = NULL;
	PXIDISK_DIR_INFO				pDirHeader = NULL;
	PXIX_BUF						LotHeader = NULL;
	PXIX_BUF						Buffer = NULL;
	int						RC = 0;
	uint64					TmpSize = 0;

	
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	XIXCORE_ASSERT(sb);
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	
	


	LotHeader = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if(LotHeader == NULL) {
		RC = -1;
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail xixfs_read_inode: can't allocate xbuf\n"));
		goto bad_inode;
	}


	Buffer = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_FILE_INFO_SIZE);
	if(LotHeader == NULL) {
		RC = -1;
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail xixfs_read_inode: can't allocate xbuf\n"));
		goto bad_inode;
	}

	RC =xixfs_read_FCBinfo_from_LotNumber(pVCB,LotHeader, Buffer,IsDir,LotNumber);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail (%x) xixfs_read_inode : xixfs_read_fileInfo_from_fcb\n", RC));
		goto bad_inode;
	}


	if( IsDir ) {
		pDirHeader = (PXIDISK_DIR_INFO)xixcore_GetDataBufferWithOffset(&Buffer->xixcore_buffer);
		pFCB->XixcoreFcb.Access_time = pDirHeader->Access_time ;
		pFCB->XixcoreFcb.Modified_time = pDirHeader->Modified_time ; 
		pFCB->XixcoreFcb.Create_time = pDirHeader->Create_time ;
		
		if(pDirHeader->NameSize > 0){

			if(pFCB->XixcoreFcb.FCBName) {
				if( pDirHeader->NameSize > SECTORALIGNSIZE_512(pFCB->XixcoreFcb.FCBNameLength) ) {
					kfree(pFCB->XixcoreFcb.FCBName);

					pFCB->XixcoreFcb.FCBName = kmalloc(SECTORALIGNSIZE_512(pDirHeader->NameSize), GFP_KERNEL);
					if(pFCB->XixcoreFcb.FCBName == NULL) {
						RC = -ENOMEM;
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("Fail (%x) xixfs_read_inode : can't allocate file name\n",
								RC));
						goto bad_inode;
					}
				}
			}else{
				pFCB->XixcoreFcb.FCBName = kmalloc(SECTORALIGNSIZE_512(pDirHeader->NameSize), GFP_KERNEL);
				if(pFCB->XixcoreFcb.FCBName == NULL) {
					RC = -ENOMEM;
					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail (%x) xixfs_read_inode : can't allocate file name\n",
							RC));
					goto bad_inode;
				}
			}

			pFCB->XixcoreFcb.FCBNameLength = pDirHeader->NameSize;
			memset(pFCB->XixcoreFcb.FCBName, 0, SECTORALIGNSIZE_512(pFCB->XixcoreFcb.FCBNameLength));
			memcpy(pFCB->XixcoreFcb.FCBName, pDirHeader->Name, pFCB->XixcoreFcb.FCBNameLength);
		}
			
			
	
			
	
		//pFCB->ValidDataLength=
		//pFCB->FileSize = 
		//pFCB->RealFileSize = pDirHeader->DirInfo.FileSize;
		i_size_write(inode, (loff_t)pDirHeader->FileSize);
		pFCB->XixcoreFcb.RealAllocationSize = pDirHeader->AllocationSize;
			
		
		pFCB->XixcoreFcb.AllocationSize = pDirHeader->AllocationSize; 
			
		pFCB->XixcoreFcb.FileAttribute = pDirHeader->FileAttribute;
			
		pFCB->XixcoreFcb.LinkCount = pDirHeader->LinkCount;
		pFCB->XixcoreFcb.Type = pDirHeader->Type;

		pFCB->XixcoreFcb.LotNumber = pDirHeader->LotIndex;
		pFCB->XixcoreFcb.ParentLotNumber = pDirHeader->ParentDirLotIndex;
		pFCB->XixcoreFcb.AddrLotNumber = pDirHeader->AddressMapIndex;

		if(pVCB->XixcoreVcb.RootDirectoryLotIndex == pFCB->XixcoreFcb.LotNumber) {
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.Type, (XIFS_FD_TYPE_ROOT_DIRECTORY|XIFS_FD_TYPE_DIRECTORY));
		}

		pFCB->XixcoreFcb.ChildCount= pDirHeader->childCount;		
		pFCB->XixcoreFcb.ChildCount = (uint32)xixcore_FindSetBitCount(1024, pDirHeader->ChildMap);
		pFCB->XixcoreFcb.AddrStartSecIndex = 0;

		pFCB->XixcoreFcb.FCBType = FCB_TYPE_DIR;

		
		
	}else {

		pFileHeader = (PXIDISK_FILE_INFO)xixcore_GetDataBufferWithOffset(&Buffer->xixcore_buffer);

		pFCB->XixcoreFcb.Access_time = pFileHeader->Access_time ;
		pFCB->XixcoreFcb.Modified_time = pFileHeader->Modified_time ;
		pFCB->XixcoreFcb.Create_time = pFileHeader->Create_time ;
	


		if(pFileHeader->NameSize > 0){

			if(pFCB->XixcoreFcb.FCBName) {
				if( pFileHeader->NameSize > SECTORALIGNSIZE_512(pFCB->XixcoreFcb.FCBNameLength) ) {
					kfree(pFCB->XixcoreFcb.FCBName);

					pFCB->XixcoreFcb.FCBName = kmalloc(SECTORALIGNSIZE_512(pFileHeader->NameSize), GFP_KERNEL);
					if(pFCB->XixcoreFcb.FCBName == NULL) {
						RC = -ENOMEM;
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("Fail (%x) xixfs_read_inode : can't allocate file name\n",
								RC));
						goto bad_inode;
					}
				}
			}else{
				pFCB->XixcoreFcb.FCBName = kmalloc(SECTORALIGNSIZE_512(pFileHeader->NameSize), GFP_KERNEL);
				if(pFCB->XixcoreFcb.FCBName == NULL) {
					RC = -ENOMEM;
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail (%x) xixfs_read_inode : can't allocate file name\n",
							RC));
					goto bad_inode;
				}
			}

			pFCB->XixcoreFcb.FCBNameLength = pFileHeader->NameSize;
			memset(pFCB->XixcoreFcb.FCBName, 0, SECTORALIGNSIZE_512(pFCB->XixcoreFcb.FCBNameLength));
			memcpy(pFCB->XixcoreFcb.FCBName, pFileHeader->Name, pFCB->XixcoreFcb.FCBNameLength);
		}



		//pFCB->ValidDataLength =
		//pFCB->FileSize =  
		//pFCB->RealFileSize = pFileHeader->FileInfo.FileSize;
		i_size_write(inode, (loff_t)pFileHeader->FileSize);
		pFCB->XixcoreFcb.RealAllocationSize = pFileHeader->AllocationSize;
		
		
		pFCB->XixcoreFcb.AllocationSize = pFileHeader->AllocationSize;
		
		pFCB->XixcoreFcb.FileAttribute = pFileHeader->FileAttribute;
		pFCB->XixcoreFcb.LinkCount = pFileHeader->LinkCount;
		pFCB->XixcoreFcb.Type = pFileHeader->Type;
		
		pFCB->XixcoreFcb.LotNumber = pFileHeader->LotIndex;
		pFCB->XixcoreFcb.ParentLotNumber = pFileHeader->ParentDirLotIndex;
		pFCB->XixcoreFcb.AddrLotNumber = pFileHeader->AddressMapIndex;

		
		pFCB->XixcoreFcb.AddrStartSecIndex = 0;

		pFCB->XixcoreFcb.FCBType = FCB_TYPE_FILE;
	}


	pFCB->XixcoreFcb.LotNumber = LotNumber;
	
	XIXCORE_ASSERT(pFCB->XixcoreFcb.AddrLot);
	
	RC = xixfs_read_addrInfo_from_fcb( pVCB, pFCB, 0);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail (%x) xixfs_read_inode : xixfs_read_addrInfo_from_fcb\n", RC));
		goto bad_inode;		
	}


	TmpSize = pFCB->XixcoreFcb.Access_time;
	(void)(do_div( TmpSize, XIXFS_TIC_P_SEC));
#if LINUX_VERSION_25_ABOVE		
	inode->i_atime.tv_sec = (long)TmpSize;
#else
	inode->i_atime = (long)TmpSize;
#endif
	TmpSize = 0;

	TmpSize = pFCB->XixcoreFcb.Modified_time;
	(void)(do_div( TmpSize, XIXFS_TIC_P_SEC));
#if LINUX_VERSION_25_ABOVE			
	inode->i_mtime.tv_sec = (long)TmpSize;
#else
	inode->i_mtime = (long)TmpSize;
#endif
	TmpSize = 0;

	TmpSize = pFCB->XixcoreFcb.Create_time;
	(void)(do_div( TmpSize, XIXFS_TIC_P_SEC));
#if LINUX_VERSION_25_ABOVE			
	inode->i_ctime.tv_sec = (long) TmpSize;
#else
	inode->i_ctime = (long) TmpSize;
#endif
	TmpSize = 0;


	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR){
		inode->i_nlink = pFCB->XixcoreFcb.LinkCount + pFCB->XixcoreFcb.ChildCount;
	}else{
		inode->i_nlink = pFCB->XixcoreFcb.LinkCount;
	}

	RC = 0;


	
bad_inode:
	if (LotHeader) {
		xixcore_FreeBuffer(&LotHeader->xixcore_buffer);
		LotHeader = NULL;
	}

	if(Buffer) {
		xixcore_FreeBuffer(&Buffer->xixcore_buffer);
		Buffer = NULL;
	}

	if(RC < 0 ) {

		make_bad_inode(inode);
	}
	
	return RC;
	
}




int
xixfs_fill_inode ( 
		struct super_block *sb,
		struct inode *inode,
		uint32		IsDir,
		uint64		LotNumber
)
{
	PXIXFS_LINUX_FCB 				pFCB = NULL;
	PXIXFS_LINUX_VCB				pVCB = NULL;
	PXIDISK_FILE_INFO				pFileHeader = NULL;
	PXIDISK_DIR_INFO				pDirHeader = NULL;
	PXIX_BUF						LotHeader = NULL;
	PXIX_BUF						Buffer = NULL;

	int						RC = 0;
	uint64					TmpSize = 0;

	
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	XIXCORE_ASSERT(sb);
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	
	LotHeader = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if(LotHeader == NULL) {
		RC = -1;
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail xixfs_read_inode: can't allocate xbuf\n"));
		goto bad_inode;
	}


	Buffer = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_FILE_INFO_SIZE);

	if(Buffer == NULL) {
		RC = -1;
		
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail xixfs_read_inode: can't allocate xbuf\n"));
		goto bad_inode;
	}
	


	RC =xixfs_read_FCBinfo_from_LotNumber(pVCB,LotHeader,Buffer,IsDir,LotNumber);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail (%x) xixfs_read_inode : xixfs_read_fileInfo_from_fcb\n", RC));
		goto bad_inode;
	}


	if( IsDir ) {
		pDirHeader = (PXIDISK_DIR_INFO)xixcore_GetDataBufferWithOffset(&Buffer->xixcore_buffer);
		pFCB->XixcoreFcb.Access_time = pDirHeader->Access_time ;
		pFCB->XixcoreFcb.Modified_time = pDirHeader->Modified_time ; 
		pFCB->XixcoreFcb.Create_time= pDirHeader->Create_time ;
		
		if(pDirHeader->NameSize > 0){

			if(pFCB->XixcoreFcb.FCBName) {
				if( pDirHeader->NameSize > SECTORALIGNSIZE_512(pFCB->XixcoreFcb.FCBNameLength) ) {
					kfree(pFCB->XixcoreFcb.FCBName);

					pFCB->XixcoreFcb.FCBName = kmalloc(SECTORALIGNSIZE_512(pDirHeader->NameSize), GFP_KERNEL);
					if(pFCB->XixcoreFcb.FCBName == NULL) {
						RC = -ENOMEM;
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("Fail (%x) xixfs_read_inode : can't allocate file name\n",
								RC));
						goto bad_inode;
					}
				}
			}else{
				pFCB->XixcoreFcb.FCBName = kmalloc(SECTORALIGNSIZE_512(pDirHeader->NameSize), GFP_KERNEL);
				if(pFCB->XixcoreFcb.FCBName == NULL) {
					RC = -ENOMEM;
					
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail (%x) xixfs_read_inode : can't allocate file name\n",
							RC));
					goto bad_inode;
				}
			}

			pFCB->XixcoreFcb.FCBNameLength = pDirHeader->NameSize;
			memset(pFCB->XixcoreFcb.FCBName, 0, SECTORALIGNSIZE_512(pFCB->XixcoreFcb.FCBNameLength));
			memcpy(pFCB->XixcoreFcb.FCBName, pDirHeader->Name, pFCB->XixcoreFcb.FCBNameLength);
		}
			
			
	
			
	
		//pFCB->ValidDataLength=
		//pFCB->FileSize = 
		//pFCB->RealFileSize = pDirHeader->DirInfo.FileSize;
		i_size_write(inode, (loff_t)pDirHeader->FileSize);
		pFCB->XixcoreFcb.RealAllocationSize= pDirHeader->AllocationSize;
			
		
		pFCB->XixcoreFcb.AllocationSize = pDirHeader->AllocationSize; 
			
		pFCB->XixcoreFcb.FileAttribute = pDirHeader->FileAttribute;
			
		pFCB->XixcoreFcb.LinkCount = pDirHeader->LinkCount;
		pFCB->XixcoreFcb.Type = pDirHeader->Type;

		pFCB->XixcoreFcb.LotNumber = pDirHeader->LotIndex;
		pFCB->XixcoreFcb.ParentLotNumber = pDirHeader->ParentDirLotIndex;
		pFCB->XixcoreFcb.AddrLotNumber = pDirHeader->AddressMapIndex;
			
		if(pVCB->XixcoreVcb.RootDirectoryLotIndex == pFCB->XixcoreFcb.LotNumber) {
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.Type, (XIFS_FD_TYPE_ROOT_DIRECTORY|XIFS_FD_TYPE_DIRECTORY));
		}

		pFCB->XixcoreFcb.ChildCount= pDirHeader->childCount;		
		pFCB->XixcoreFcb.ChildCount = (uint32)xixcore_FindSetBitCount(1024, pDirHeader->ChildMap);
		pFCB->XixcoreFcb.AddrStartSecIndex = 0;

		pFCB->XixcoreFcb.FCBType = FCB_TYPE_DIR;

		
		
	}else {

		pFileHeader = (PXIDISK_FILE_INFO)xixcore_GetDataBufferWithOffset(&Buffer->xixcore_buffer);

		pFCB->XixcoreFcb.Access_time = pFileHeader->Access_time ;
		pFCB->XixcoreFcb.Modified_time = pFileHeader->Modified_time ;
		pFCB->XixcoreFcb.Create_time = pFileHeader->Create_time ;
	


		if(pFileHeader->NameSize > 0){

			if(pFCB->XixcoreFcb.FCBName) {
				if( pFileHeader->NameSize > SECTORALIGNSIZE_512(pFCB->XixcoreFcb.FCBNameLength) ) {
					kfree(pFCB->XixcoreFcb.FCBName);

					pFCB->XixcoreFcb.FCBName = kmalloc(SECTORALIGNSIZE_512(pFileHeader->NameSize), GFP_KERNEL);
					if(pFCB->XixcoreFcb.FCBName == NULL) {
						RC = -ENOMEM;
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("Fail (%x) xixfs_read_inode : can't allocate file name\n",
								RC));
						goto bad_inode;
					}
				}
			}else{
				pFCB->XixcoreFcb.FCBName = kmalloc(SECTORALIGNSIZE_512(pFileHeader->NameSize), GFP_KERNEL);
				if(pFCB->XixcoreFcb.FCBName == NULL) {
					RC = -ENOMEM;
					DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
						("Fail (%x) xixfs_read_inode : can't allocate file name\n",
							RC));
					goto bad_inode;
				}
			}

			pFCB->XixcoreFcb.FCBNameLength = pFileHeader->NameSize;
			memset(pFCB->XixcoreFcb.FCBName, 0, SECTORALIGNSIZE_512(pFCB->XixcoreFcb.FCBNameLength));
			memcpy(pFCB->XixcoreFcb.FCBName, pFileHeader->Name, pFCB->XixcoreFcb.FCBNameLength);
		}



		//pFCB->ValidDataLength =
		//pFCB->FileSize =  
		//pFCB->RealFileSize = pFileHeader->FileInfo.FileSize;
		i_size_write(inode, (loff_t)pFileHeader->FileSize);
		pFCB->XixcoreFcb.RealAllocationSize = pFileHeader->AllocationSize;
		
		
		pFCB->XixcoreFcb.AllocationSize = pFileHeader->AllocationSize;
		
		pFCB->XixcoreFcb.FileAttribute = pFileHeader->FileAttribute;
		pFCB->XixcoreFcb.LinkCount = pFileHeader->LinkCount;
		pFCB->XixcoreFcb.Type = pFileHeader->Type;
		
		pFCB->XixcoreFcb.LotNumber = pFileHeader->LotIndex;
		pFCB->XixcoreFcb.ParentLotNumber = pFileHeader->ParentDirLotIndex;
		pFCB->XixcoreFcb.AddrLotNumber = pFileHeader->AddressMapIndex;

		
		pFCB->XixcoreFcb.AddrStartSecIndex = 0;

		pFCB->XixcoreFcb.FCBType = FCB_TYPE_FILE;
	}


	pFCB->XixcoreFcb.LotNumber = LotNumber;
	
	XIXCORE_ASSERT(pFCB->XixcoreFcb.AddrLot);
	
	RC = xixfs_read_addrInfo_from_fcb( pVCB, pFCB, 0);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail (%x) xixfs_read_inode : xixfs_read_addrInfo_from_fcb\n", RC));
		goto bad_inode;		
	}




	inode->i_uid = -1;
	inode->i_gid = -1;
	inode->i_version++;
	inode->i_generation = get_seconds();

	TmpSize = pFCB->XixcoreFcb.Access_time;
	(void)(do_div( TmpSize, XIXFS_TIC_P_SEC));
#if LINUX_VERSION_25_ABOVE				
	inode->i_atime.tv_sec = (long)TmpSize; 
#else
	inode->i_atime = (long)TmpSize; 
#endif
	TmpSize = 0;

	TmpSize = pFCB->XixcoreFcb.Modified_time;
	(void)(do_div( TmpSize, XIXFS_TIC_P_SEC));
#if LINUX_VERSION_25_ABOVE			
	inode->i_mtime.tv_sec = (long)TmpSize;
#else
	inode->i_mtime = (long)TmpSize;
#endif
	TmpSize = 0;

	TmpSize = pFCB->XixcoreFcb.Create_time;
	(void)(do_div( TmpSize, XIXFS_TIC_P_SEC));
#if LINUX_VERSION_25_ABOVE		
	inode->i_ctime.tv_sec = (long) TmpSize;
#else
	inode->i_ctime = (long) TmpSize;
#endif
	TmpSize = 0;

	inode->i_nlink = pFCB->XixcoreFcb.LinkCount;
	//inode->i_size = (loff_t)pFCB->FileSize;


	inode->i_blkbits = sb->s_blocksize_bits;
#if !LINUX_VERSION_2_6_19_REPLACE_INTERFACE
	inode->i_blksize = sb->s_blocksize; // To be obsolete
#endif
	inode->i_blocks = ((inode->i_size + ((1<<inode->i_blkbits) - 1))
			   & ~((loff_t)(1<<inode->i_blkbits)- 1)) >> inode->i_blkbits;


	if(IsDir == 1 ){
		inode->i_generation &= ~1;
		inode->i_mode = S_IRWXUGO | S_IFDIR;
		inode->i_op = &xixfs_dir_inode_operations;
		inode->i_fop = &xixfs_dir_operations;

	}else{
		inode->i_generation |= 1;
		inode->i_mode = S_IRUGO|S_IWUGO |S_IFREG;
		inode->i_op = &xixfs_file_inode_operations;
		inode->i_fop = &xixfs_file_operations;
		inode->i_mapping->a_ops = &xixfs_aops;
	}

	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR){
		inode->i_nlink = pFCB->XixcoreFcb.LinkCount + pFCB->XixcoreFcb.ChildCount;
	}else{
		inode->i_nlink = pFCB->XixcoreFcb.LinkCount;
	}

	RC = 0;
#if !LINUX_VERSION_2_6_19_REPLACE_INTERFACE
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("i_blksize=%d\n", (1<<inode->i_blkbits)));
#endif
bad_inode:
	if (LotHeader) {
		xixcore_FreeBuffer(&LotHeader->xixcore_buffer);;
		LotHeader = NULL;
	}
	
	if (Buffer) {
		xixcore_FreeBuffer(&Buffer->xixcore_buffer);;
		Buffer = NULL;
	}

	

	if(RC < 0 ) {

		make_bad_inode(inode);
	}
	
	return RC;
	
}

//////////////////////////////////////////////////////////////////////////
//
//
//				inode structure init/deinit
//	
//
//////////////////////////////////////////////////////////////////////////


#if  LINUX_VERSION_KMEM_CACHE_T_DEPRECATED
struct kmem_cache *xixfs_inode_cachep;;
#else
kmem_cache_t * xixfs_inode_cachep;
#endif



static void 
init_once(
	void * foo, 
#if  LINUX_VERSION_KMEM_CACHE_T_DEPRECATED
	struct kmem_cache * cachep,
#else
	kmem_cache_t * cachep,
#endif
	unsigned long flags
)
{
	PXIXFS_LINUX_FCB pFCB  = (PXIXFS_LINUX_FCB) foo;

#if LINUX_VERSION_2_6_22_CHANGE_SLAB_FLAGS_FUNCTION
		memset(pFCB, 0, sizeof(XIXFS_LINUX_FCB));
			
		inode_init_once(&pFCB ->linux_inode);
#else
	if ((flags & (SLAB_CTOR_VERIFY|SLAB_CTOR_CONSTRUCTOR)) ==
	    SLAB_CTOR_CONSTRUCTOR) {

		memset(pFCB, 0, sizeof(XIXFS_LINUX_FCB));
			
		inode_init_once(&pFCB ->linux_inode);
	}
#endif
}

int 
init_xixfs_inodecache(void)
{
	xixfs_inode_cachep = kmem_cache_create("xixfs_inode_cache",
					     sizeof(XIXFS_LINUX_FCB),		     
					     0,
#if LINUX_VERSION_25_ABOVE								     
					     SLAB_HWCACHE_ALIGN|SLAB_PANIC,
#else
					     SLAB_HWCACHE_ALIGN,	
#endif
					     init_once, NULL);
	if (xixfs_inode_cachep == NULL)
		return -ENOMEM;
	return 0;
}


void 
destroy_xixfs_inodecache(void)
{

#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
	kmem_cache_destroy(xixfs_inode_cachep);
#else
	if (kmem_cache_destroy(xixfs_inode_cachep))
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("xixfs_inode_cache: not all structures were freed\n"));
#endif
}


////////////////////////////////////////////////////////////////////////////////
//
//
//
//	Super block operations
//
//
//
////////////////////////////////////////////////////////////////////////////////

struct inode *
xixfs_alloc_inode(
	struct super_block *sb
) 
{

	PXIXFS_LINUX_VCB pVCB = NULL;
	PXIXFS_LINUX_FCB pFCB = NULL;
	xc_int32	RC;

	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXCORE_ASSERT(pVCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_alloc_inode \n"));

#if  LINUX_VERSION_KMEM_CACHE_T_DEPRECATED
	pFCB =	(PXIXFS_LINUX_FCB) kmem_cache_alloc(xixfs_inode_cachep, GFP_KERNEL);
#else
	pFCB =	(PXIXFS_LINUX_FCB) kmem_cache_alloc(xixfs_inode_cachep, SLAB_KERNEL);
#endif

	if( pFCB == NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("xixfs_alloc_inode: can't allocate FCB\n"));
		
		return NULL;
	}


	memset(pFCB, 0, sizeof(XIXFS_LINUX_FCB));


	spin_lock_init(&(pFCB->FCBLock));
	atomic_set(&(pFCB->FCBCleanup), 0);
	atomic_set(&(pFCB->FCBCloseCount), 0);
	atomic_set(&(pFCB->FCBReference), 0);
	atomic_set(&(pFCB->FCBUserReference), 0);
#if LINUX_VERSION_25_ABOVE	
	INIT_HLIST_NODE(&(pFCB->FCB_hash));
#else
	INIT_LIST_HEAD(&(pFCB->FCB_hash));
#endif
	/*
	INIT_HLIST_NODE(&(pFCB->linux_inode.i_hash));
	INIT_LIST_HEAD(&(pFCB->linux_inode.i_dentry));
	INIT_LIST_HEAD(&(pFCB->linux_inode.i_list));
	INIT_LIST_HEAD(&(pFCB->linux_inode.i_sb_list));
	INIT_LIST_HEAD(&(pFCB->linux_inode.i_dentry));
	*/
	
	inode_init_once(&(pFCB->linux_inode));


#if LINUX_VERSION_25_ABOVE	
	pFCB->N_TYPE.Type = XIXFS_NODE_TYPE_FCB;
	pFCB->N_TYPE.Size = sizeof(XIXFS_LINUX_FCB);
#else
	pFCB->n_type.Type = XIXFS_NODE_TYPE_FCB;
	pFCB->n_type.Size = sizeof(XIXFS_LINUX_FCB);
#endif	

	RC = xixcore_InitializeFCB(
				&pFCB->XixcoreFcb,
				&pVCB->XixcoreVcb,
				(PXIXCORE_SEMAPHORE)&pFCB->LinuxAddressLock
		);
	if(RC != XCCODE_SUCCESS) {
		DebugTrace(DEBUG_LEVEL_TRACE, DEBUG_TARGET_ALL, 
			("xixfs_alloc_inode: xixcore_InitializeFCB() failed.\n"));

		kmem_cache_free(xixfs_inode_cachep, pFCB);
		return NULL;
	}

	pFCB->linux_inode.i_version = 1;

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Exit xixfs_alloc_inode \n"));
	
	return &(pFCB->linux_inode);

	
}


void 
xixfs_destroy_inode(
		struct inode *inode
) 
{
	PXIXFS_LINUX_FCB pFCB = NULL;
	pFCB = XIXFS_I(inode);
	
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_destroy_inode (%lld) \n", pFCB->XixcoreFcb.LotNumber));

	xixcore_DestroyFCB(&pFCB->XixcoreFcb);

	kmem_cache_free(xixfs_inode_cachep, pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Exit xixfs_destroy_inode \n"));
}

#if LINUX_VERSION_25_ABOVE	
int 
#else
void
#endif
xixfs_write_inode(
	struct inode *inode, 
	int wait
)
{
	struct super_block  	*sb = NULL;
	PXIXFS_LINUX_VCB 			pVCB = NULL;
	PXIXFS_LINUX_FCB			pFCB = NULL;
	PXIX_BUF			LotHeader = NULL;
	PXIX_BUF			Buffer = NULL;
	int 					RC = 0;
	int					filetype = 0;
	uint32				bHasLock = 0;

	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);

	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_write_inode \n"));

	


	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_write_inode : is read only .\n"));	
		RC = -EPERM;
#if LINUX_VERSION_25_ABOVE	
		return RC;
#else
		return;
#endif
	}


	
	LotHeader = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if(LotHeader == NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixfs_write_inode: can't alloc LotHeader\n "));
		
#if LINUX_VERSION_25_ABOVE	
		return -1;
#else
		return;
#endif		
	}



	Buffer = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_FILE_INFO_SIZE);
	if(Buffer == NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixfs_write_inode: can't alloc Buffer\n "));
		xixcore_FreeBuffer(&LotHeader->xixcore_buffer);
#if LINUX_VERSION_25_ABOVE	
		return -1;
#else
		return;
#endif				
	}


	if( pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR ) {


		/* check Lock */

		if(pFCB->XixcoreFcb.HasLock == INODE_FILE_LOCK_HAS){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail xixfs_write_inode: Dir has lock some mistake in it!!!\n "));
			bHasLock = 1;
		}else {
			RC = xixcore_LotLock(&pVCB->XixcoreVcb,
							pFCB->XixcoreFcb.LotNumber,
							&pFCB->XixcoreFcb.HasLock,
							1,
							1
							);

			if(RC < 0 ) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail xixfs_write_inode: xixfs_LotLock!!!\n "));
				goto error_out;
			}

			bHasLock =1;
		}

	}else {
		if(pFCB->XixcoreFcb.HasLock != INODE_FILE_LOCK_HAS){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail xixfs_write_inode: don't have a lot lock %d(%p)\n ", inode->i_ino, inode));
			goto error_out;
		}	
	}

	
	//printk(KERN_DEBUG "CALL xixfs_write_inode2 (%lld)\n", pFCB->XixcoreFcb.LotNumber);

	if(wait ) {
		//printk(KERN_DEBUG "CALL xixfs_write_inode wait set (%lld)\n", pFCB->XixcoreFcb.LotNumber);
		//xixfs_sync_inode(inode);
	}




	//lock_kernel();
	
	RC = xixfs_read_fileInfo_from_fcb(pVCB, pFCB, LotHeader, Buffer, &filetype);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail (%x) xixfs_write_inode : xixfs_read_fileInfo_from_fcb\n", RC));
		
		goto error_out;
	}
	
		

	if ( pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR ) {
		
		PXIDISK_DIR_INFO		DirInfo = NULL;		
		DirInfo = (PXIDISK_DIR_INFO)xixcore_GetDataBufferWithOffset(&Buffer->xixcore_buffer);


		if(DirInfo->State == XIFS_FD_STATE_DELETED ) {
			XIXCORE_SET_FLAGS( pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED );

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail (%x) xixfs_write_inode : deleted inode\n", RC));

			RC = -1;
			goto error_out;
		}


		if( XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME ) ){
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME );

			memset(DirInfo->Name, 0, pFCB->XixcoreFcb.FCBNameLength);
			memcpy(DirInfo->Name, pFCB->XixcoreFcb.FCBName, pFCB->XixcoreFcb.FCBNameLength);
			
			DirInfo->NameSize = pFCB->XixcoreFcb.FCBNameLength;
			DirInfo->ParentDirLotIndex = pFCB->XixcoreFcb.ParentLotNumber;
			
			DebugTrace(DEBUG_LEVEL_INFO, DEBUG_TARGET_ALL,
				("xixfs_write_inode : update string\n"));
		}

		
		if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR)){
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR);
			DirInfo->FileAttribute = pFCB->XixcoreFcb.FileAttribute;
		}



		
		XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME);
		XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
#if LINUX_VERSION_25_ABOVE			
		DirInfo->Create_time = inode->i_ctime.tv_sec * XIXFS_TIC_P_SEC;
		DirInfo->Change_time = inode->i_mtime.tv_sec * XIXFS_TIC_P_SEC;
		DirInfo->Access_time = inode->i_atime.tv_sec * XIXFS_TIC_P_SEC;
		DirInfo->Modified_time = inode->i_mtime.tv_sec * XIXFS_TIC_P_SEC;
#else
		DirInfo->Create_time = inode->i_ctime * XIXFS_TIC_P_SEC;
		DirInfo->Change_time = inode->i_mtime * XIXFS_TIC_P_SEC;
		DirInfo->Access_time = inode->i_atime * XIXFS_TIC_P_SEC;
		DirInfo->Modified_time = inode->i_mtime * XIXFS_TIC_P_SEC;
#endif

		DirInfo->FileSize = i_size_read(inode);
		DirInfo->AllocationSize = pFCB->XixcoreFcb.RealAllocationSize;		
		DirInfo->LinkCount = pFCB->XixcoreFcb.LinkCount;
		
			

		
	}else {
		PXIDISK_FILE_INFO FileInfo = NULL;
	
		FileInfo = (PXIDISK_FILE_INFO)xixcore_GetDataBufferWithOffset(&Buffer->xixcore_buffer);
		 
		if(FileInfo->State == XIFS_FD_STATE_DELETED){
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags,XIXCORE_FCB_CHANGE_DELETED);

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail (%x) xixfs_write_inode : deleted inode\n", RC));

			RC = -1;
			goto error_out;
		}


		if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME)){
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_NAME);
			memset(FileInfo->Name, 0, pFCB->XixcoreFcb.FCBNameLength);
			memcpy(FileInfo->Name, pFCB->XixcoreFcb.FCBName, pFCB->XixcoreFcb.FCBNameLength);
			FileInfo->NameSize = pFCB->XixcoreFcb.FCBNameLength;
			FileInfo->ParentDirLotIndex = pFCB->XixcoreFcb.ParentLotNumber;
			
		}

		
		if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR)){
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_ATTR);
			FileInfo->FileAttribute = pFCB->XixcoreFcb.FileAttribute;
			
		}


		XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_TIME);
		XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
#if LINUX_VERSION_25_ABOVE		
		FileInfo->Create_time = inode->i_ctime.tv_sec * XIXFS_TIC_P_SEC;
		FileInfo->Change_time = inode->i_mtime.tv_sec * XIXFS_TIC_P_SEC;
		FileInfo->Access_time = inode->i_atime.tv_sec * XIXFS_TIC_P_SEC;
		FileInfo->Modified_time = inode->i_mtime.tv_sec * XIXFS_TIC_P_SEC;
#else
		FileInfo->Create_time = inode->i_ctime * XIXFS_TIC_P_SEC;
		FileInfo->Change_time = inode->i_mtime * XIXFS_TIC_P_SEC;
		FileInfo->Access_time = inode->i_atime * XIXFS_TIC_P_SEC;
		FileInfo->Modified_time = inode->i_mtime * XIXFS_TIC_P_SEC;
#endif
		
		//FileInfo->FileSize = pFCB->FileSize;
		FileInfo->FileSize = i_size_read(inode);
		FileInfo->AllocationSize = pFCB->XixcoreFcb.RealAllocationSize;
		FileInfo->LinkCount = pFCB->XixcoreFcb.LinkCount;
		
		
	}
	
	RC = xixfs_write_fileInfo_from_fcb(pVCB, pFCB, Buffer);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("Fail (%x) xixfs_write_inode : xixfs_write_fileInfo_from_fcb\n", RC));	

		goto error_out;
	}
	
	
error_out:

	if(LotHeader) {
		xixcore_FreeBuffer(&LotHeader->xixcore_buffer);
	}

	if(Buffer) {
		xixcore_FreeBuffer(&Buffer->xixcore_buffer);
	}
	
	if( bHasLock ) {
		xixcore_LotUnLock(&pVCB->XixcoreVcb,
			pFCB->XixcoreFcb.LotNumber,
			&pFCB->XixcoreFcb.HasLock
			);
	}

	//unlock_kernel();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Exit xixfs_write_inode \n"));

	//printk(KERN_DEBUG "RETURN xixfs_write_inode (%lld) i_block(%ld)\n", pFCB->XixcoreFcb.LotNumber, inode->i_blocks);

#if LINUX_VERSION_25_ABOVE	
	return RC;
#else
	return;
#endif									
}







void 
xixfs_delete_inode(
	struct inode *inode
)
{
	struct super_block  	*sb = NULL;
	PXIXFS_LINUX_VCB 		pVCB = NULL;
	PXIXFS_LINUX_FCB 		pFCB = NULL;
	PXIX_BUF		LotHeader = NULL;
	PXIX_BUF		Buffer = NULL;
	uint32			bHasLock = 0;
	int				RC = 0;
	uint32			bRealDelete = 0;
	int					filetype = 0;
	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);

	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);

	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_delete_inode \n"));

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERRORxixfs_delete_inode : is read only .\n"));	
		RC = -EPERM;
		return ;
	}


	
	LotHeader = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_COMMON_LOT_HEADER_SIZE);

	if(LotHeader == NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixfs_delete_inode: can't alloc LotHeader\n "));
		return ;
	}



	Buffer = (PXIX_BUF)xixcore_AllocateBuffer(XIDISK_DUP_FILE_INFO_SIZE);
	if(Buffer == NULL) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Fail xixfs_delete_inode: can't alloc Buffer\n "));
		goto error_out;
	}


	if( pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR ) {


		/* check Lock */

		if(pFCB->XixcoreFcb.HasLock == INODE_FILE_LOCK_HAS){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail xixfs_delete_inode: Dir has lock some mistake in it!!!\n "));
			bHasLock = 1;
		}else {
			RC = xixcore_LotLock(&pVCB->XixcoreVcb,
							pFCB->XixcoreFcb.LotNumber,
							&pFCB->XixcoreFcb.HasLock,
							1, 
							1
							);

			if(RC < 0 ) {
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail xixfs_delete_inode: xixfs_LotLock!!!\n "));
				goto error_out;
			}

			bHasLock =1;
		}

	}else {
		if(pFCB->XixcoreFcb.HasLock != INODE_FILE_LOCK_HAS){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Fail xixfs_delete_inode: don't have a lot lock\n "));
			goto error_out;
		}	
	}




	//lock_kernel();
	
	RC = xixfs_read_fileInfo_from_fcb(pVCB, pFCB, LotHeader, Buffer, &filetype);

	if( RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("Fail (%x) xixfs_write_inode : xixfs_read_fileInfo_from_fcb\n", RC));
		
		goto error_out;
	}

	

	if ( pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR ) {
		
		PXIDISK_COMMON_LOT_HEADER DirLotHeader = NULL;
		PXIDISK_DIR_INFO		DirInfo = NULL;

		
		DirLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(&LotHeader->xixcore_buffer);
		DirInfo = (PXIDISK_DIR_INFO)xixcore_GetDataBufferWithOffset(&Buffer->xixcore_buffer);


		if(DirInfo->State== XIFS_FD_STATE_DELETED){
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL xixfs_delete_inode : Deleted inode.\n"));	

			RC = 0;
			goto error_out;
		}


		if ( DirInfo->childCount != 0 ) {
			RC = -1;
			goto error_out;
		}

		if ( DirInfo->LinkCount  > 1 ) {
			DirInfo->LinkCount --;
		}else {

			bRealDelete = 1;
			
			DirInfo->AddressMapIndex = 0;
			DirInfo->State = XIFS_FD_STATE_DELETED;
			DirInfo->Type =  XIFS_FD_TYPE_INVALID;

			DirLotHeader->LotInfo.BeginningLotIndex = 0;
			DirLotHeader->LotInfo.LogicalStartOffset = 0;
			DirLotHeader->LotInfo.LotIndex = pFCB->XixcoreFcb.LotNumber;
			DirLotHeader->LotInfo.NextLotIndex = 0;
			DirLotHeader->LotInfo.PreviousLotIndex = 0;
			DirLotHeader->LotInfo.StartDataOffset = 0;
			DirLotHeader->LotInfo.Type = LOT_INFO_TYPE_INVALID;
			DirLotHeader->LotInfo.Flags = LOT_FLAG_INVALID;
			
		
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);
			pFCB->XixcoreFcb.HasLock = INODE_FILE_LOCK_INVALID;
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_OPEN_WRITE);	
			
			
		}
		
		
	}else {

		PXIDISK_COMMON_LOT_HEADER	FileLotHeader = NULL;
		PXIDISK_FILE_INFO FileInfo = NULL;

		FileLotHeader = (PXIDISK_COMMON_LOT_HEADER)xixcore_GetDataBufferWithOffset(&LotHeader->xixcore_buffer);
		FileInfo = (PXIDISK_FILE_INFO)xixcore_GetDataBufferWithOffset(&Buffer->xixcore_buffer);


		if(FileInfo->State== XIFS_FD_STATE_DELETED){
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);

			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL xixfs_delete_inode : Deleted inode.\n"));	

			RC = 0;
			goto error_out;
		}



		if ( FileInfo->LinkCount  > 1 ) {
			FileInfo->LinkCount --;
		}else {

			bRealDelete = 1;


			

			FileInfo->AddressMapIndex = 0;
			FileInfo->State = XIFS_FD_STATE_DELETED;
			FileInfo->Type =  XIFS_FD_TYPE_INVALID;

			FileLotHeader->LotInfo.BeginningLotIndex = 0;
			FileLotHeader->LotInfo.LogicalStartOffset = 0;
			FileLotHeader->LotInfo.LotIndex = pFCB->XixcoreFcb.LotNumber;
			FileLotHeader->LotInfo.NextLotIndex = 0;
			FileLotHeader->LotInfo.PreviousLotIndex = 0;
			FileLotHeader->LotInfo.StartDataOffset = 0;
			FileLotHeader->LotInfo.Type = LOT_INFO_TYPE_INVALID;
			FileLotHeader->LotInfo.Flags = LOT_FLAG_INVALID;
			
		
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_DELETE_ON_CLOSE);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_CHANGE_DELETED);
			pFCB->XixcoreFcb.HasLock = INODE_FILE_LOCK_INVALID;
			XIXCORE_CLEAR_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_OPEN_WRITE);	
			
			
		}
	
	}


	if(bRealDelete == 1) {
		uint32 Reason = 0;

		RC = xixcore_DeleteFileLotAddress(&pVCB->XixcoreVcb, &pFCB->XixcoreFcb);

		if( RC < 0 ) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL xixfs_delete_inode : xixfs_DeleteFileLotAddress.\n"));	

			goto error_out;
			
		}
		
		RC = xixcore_RawWriteLotHeader(
					pVCB->XixcoreVcb.XixcoreBlockDevice,
					pVCB->XixcoreVcb.LotSize, 
					pVCB->XixcoreVcb.SectorSize,
					pVCB->XixcoreVcb.SectorSizeBit,
					pFCB->XixcoreFcb.LotNumber,
					&LotHeader->xixcore_buffer,
					&Reason
					);

		if(RC < 0 ) {
			DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
					("Fail (%x)  Reason(%x) xixfs_read_fileInfo_from_fcb: xixcore_RawReadLotHeader\n", RC, Reason));
			goto error_out;
		}
	}



	RC = xixfs_write_fileInfo_from_fcb( pVCB, pFCB, Buffer) ;

	if(RC < 0 ) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("FAIL xixfs_delete_inode :xixfs_write_fileInfo_from_fcb.\n"));	

		goto error_out;
	}




error_out:

	//unlock_kernel();

	if(LotHeader) {
		xixcore_FreeBuffer(&LotHeader->xixcore_buffer);
	}

	if(Buffer) {
		xixcore_FreeBuffer(&Buffer->xixcore_buffer);
	}

	xixcore_LotUnLock(&pVCB->XixcoreVcb,
		pFCB->XixcoreFcb.LotNumber,
		&pFCB->XixcoreFcb.HasLock
		);
	

	if(bRealDelete) {

		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,  
				("REAL DELETE.\n"));	

		
		truncate_inode_pages(&inode->i_data, 0);
		clear_inode(inode);
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Exit xixfs_delete_inode \n"));

	return ;

}



void 
xixfs_clear_inode(
	struct inode *inode
)
{
	PXIXFS_LINUX_VCB 	pVCB = NULL;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	
	XIXCORE_ASSERT(inode);
	XIXCORE_ASSERT(inode->i_sb);

	pVCB = XIXFS_SB(inode->i_sb);
	XIXFS_ASSERT_VCB(pVCB);

	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_clear_inode \n"));
	
	if (is_bad_inode(inode))
		return;


	if(pVCB->XixcoreVcb.IsVolumeWriteProtected != 1){
		if(XIXCORE_TEST_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE)){
			xixfs_write_inode(inode, 1);

			if(pFCB->XixcoreFcb.WriteStartOffset != -1){
				xixfs_SendFileChangeRC(
					pVCB->XixcoreVcb.HostMac, 
					pFCB->XixcoreFcb.LotNumber, 
					pVCB->XixcoreVcb.VolumeId, 
					i_size_read(inode), 
					pFCB->XixcoreFcb.RealAllocationSize, 
					(uint64)pFCB->XixcoreFcb.WriteStartOffset
					);

				pFCB->XixcoreFcb.WriteStartOffset = -1;
			}

			
		}
	}


	//lock_kernel();
	xixfs_inode_detach(inode);
	//unlock_kernel();

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Exit xixfs_clear_inode \n"));

}




		



////////////////////////////////////////////////////////////////////////////////
//
//
//
//	address map operation
//
//
//
////////////////////////////////////////////////////////////////////////////////

int
xixfs_find_physicalBlockNo(
		PXIXFS_LINUX_VCB	pVCB,
		PXIXFS_LINUX_FCB	pFCB,
		sector_t 		iblock,
		sector_t		*Phyblock	
)
{
	int 					RC = 0;
	XIXCORE_IO_LOT_INFO	Address;
	uint64				offset = 0;
	uint64				physicalOffset = 0;
	uint64				Margin = 0;
	int					Reason = 0;

	XIXFS_ASSERT_VCB(pVCB);
	XIXFS_ASSERT_FCB(pFCB);
#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
	offset = (iblock << pFCB->linux_inode.i_blkbits);
#else
	offset = iblock*pFCB->linux_inode.i_blksize;
#endif
	memset(&Address, 0, sizeof(XIXCORE_IO_LOT_INFO));
	
	RC = xixcore_GetAddressInfoForOffset(
						&pVCB->XixcoreVcb,
						&pFCB->XixcoreFcb,
						offset,
						pFCB->XixcoreFcb.AddrLot,
						pFCB->XixcoreFcb.AddrLotSize,
						&pFCB->XixcoreFcb.AddrStartSecIndex,
						&Reason,
						&Address,
						1
						);

	if ( RC < 0 ) {
		DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
			("ERROR (0x%x) Reason(0x%x) xixfs_GetAddressInfoForOffset\n",
				RC, Reason ) );
		return RC;
	}

	XIXCORE_ASSERT(offset >= Address.LogicalStartOffset );
	Margin = offset - Address.LogicalStartOffset;
	physicalOffset = Address.PhysicalAddress + Margin;
#if LINUX_VERSION_2_6_19_REPLACE_INTERFACE
	do_div(physicalOffset, (1 << pFCB->linux_inode.i_blkbits));
#else
	do_div(physicalOffset, pFCB->linux_inode.i_blksize);
#endif
	*Phyblock  = physicalOffset;

	return 0;
}





int 
xixfs_get_block(
	struct inode *inode, 
	sector_t iblock, 
	struct buffer_head *bh_result, 
	int create
)
{


	int					RC = 0;
	struct super_block 	* sb = NULL;
	PXIXFS_LINUX_FCB 			pFCB = NULL;
	PXIXFS_LINUX_VCB			pVCB = NULL;

	//int32				Reason = 0;
	sector_t				lastoffset = 0;
	sector_t				phyblock = 0;
	
	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);
	

	XIXCORE_ASSERT(inode);
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR) {
		return -EINVAL;
	}

	lastoffset= (iblock << inode->i_blkbits) + (1<<inode->i_blkbits);

	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("inode %d(%p)\n", inode->i_ino, inode));

	if( lastoffset <= pFCB->XixcoreFcb.RealAllocationSize) {

	
		RC = xixfs_find_physicalBlockNo(
							pVCB,
							pFCB,
							iblock,
							&phyblock	
							);	

		if ( RC < 0 ) {
#if LINUX_VERSION_25_ABOVE
			DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ERROR (0x%x) xixfs_get_block(%lld) : xixfs_find_physicalBlockNo\n",
					RC , iblock));

			printk(KERN_DEBUG "ERROR (0x%x) xixfs_get_block(%lld) : xixfs_find_physicalBlockNo\n",
					RC , iblock);
#else
			DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ERROR (0x%x) xixfs_get_block(%ld) : xixfs_find_physicalBlockNo\n",
					RC , iblock));

			printk(KERN_DEBUG "ERROR (0x%x) xixfs_get_block(%ld) : xixfs_find_physicalBlockNo\n",
					RC , iblock);
#endif
			
			return RC;
		}

	}else { 

		if(!create) {
#if LINUX_VERSION_25_ABOVE			
			printk(KERN_DEBUG "ERROR NO CREATE(0x%x) xixfs_get_block(%lld)  lastblock(%lld) allocsize(%lld): xixfs_find_physicalBlockNo\n",
					RC , iblock, lastoffset, pFCB->XixcoreFcb.RealAllocationSize);
#else
			printk(KERN_DEBUG "ERROR NO CREATE(0x%x) xixfs_get_block(%ld)  lastblock(%ld) allocsize(%lld): xixfs_find_physicalBlockNo\n",
					RC , iblock, lastoffset, pFCB->XixcoreFcb.RealAllocationSize);
#endif
			return -EIO;
		}

redo:
		RC = xixfs_find_physicalBlockNo(
							pVCB,
							pFCB,
							iblock,
							&phyblock	
							);


		if( RC < 0 ) {
			if ( RC  == -EFBIG ) {

				uint64 LastOffset = 0;
				uint64 RequestStartOffset = 0;
				uint32 EndLotIndex = 0;
				uint32 CurentEndIndex = 0;
				//uint32 RequestStatIndex = 0;
				uint32 LotCount = 0;
				uint32 AllocatedLotCount = 0;
				
				RequestStartOffset = iblock*(1<<inode->i_blkbits);
				LastOffset = RequestStartOffset ;
#if LINUX_VERSION_25_ABOVE			
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("iblock=%lld i_blksize=%d\n", iblock, (1<<inode->i_blkbits)));
#else
				DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("iblock=%d i_blksize=%d\n", iblock, (1<<inode->i_blkbits)));
#endif
				CurentEndIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, (pFCB->XixcoreFcb.RealAllocationSize-1));
				EndLotIndex = xixcore_GetIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, LastOffset);

				if(EndLotIndex > CurentEndIndex){
					LotCount = EndLotIndex - CurentEndIndex;

						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,("LotCount=%d EndLotIndex=%d CurentEndIndex=%d\n", LotCount, EndLotIndex, CurentEndIndex));

					RC = xixcore_AddNewLot(&pVCB->XixcoreVcb,
											&pFCB->XixcoreFcb,
											CurentEndIndex,
											LotCount,
											&AllocatedLotCount, 
											pFCB->XixcoreFcb.AddrLot,
											pFCB->XixcoreFcb.AddrLotSize,
											&pFCB->XixcoreFcb.AddrStartSecIndex
											);
					
					if( RC < 0 ){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("Fail xixfs_AddNewLot FCB LotNumber(%lld) CurrentEndIndex(%d), Request Lot(%d) AllocatLength(%lld) .\n",
								pFCB->XixcoreFcb.LotNumber, CurentEndIndex, LotCount, pFCB->XixcoreFcb.RealAllocationSize));
						
						return RC;

					}

					goto redo;


				}


				return RC;
									
			}else {
#if LINUX_VERSION_25_ABOVE		
				DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("ERROR (0x%x) iblock(%lld) : xixfs_find_physicalBlockNo\n",
						RC , iblock));
#else
				DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("ERROR (0x%x) iblock(%ld) : xixfs_find_physicalBlockNo\n",
						RC , iblock));
#endif
				return RC;

			}
		}
	
	}

	RC = 0;

	if(phyblock) {
		map_bh(bh_result, inode->i_sb, phyblock);
	}
	
	return RC;
}


#if LINUX_VERSION_25_ABOVE	
static 
int
xixfs_get_blocks(
	struct inode *inode, 
	sector_t iblock, 
	unsigned long max_blocks,
	struct buffer_head *bh_result, 
	int create
)
{
	int RC;

	RC = xixfs_get_block(inode, iblock, bh_result, create);
	if (RC == 0)
		bh_result->b_size = (1 << inode->i_blkbits);
	return RC;
}
#endif

/*
static 
int
xixfs_get_blocks(
	struct inode *inode, 
	sector_t iblock, 
	unsigned long max_blocks,
	struct buffer_head *bh_result, 
	int create
)
{
	int					RC = 0;
	struct super_block 	* sb = NULL;
	PXIXFS_LINUX_FCB 			pFCB = NULL;
	PXIXFS_LINUX_VCB			pVCB = NULL;

	int32				Reason = 0;
	sector_t				lastblock = 0;
	sector_t				phyblock = 0;

	XIXCORE_IO_LOT_INFO	Address;
	uint64				offset = 0;
	uint64				physicalOffset = 0;
	int32				Margin = 0;
	int					Reason = 0;
	int					availableDataLen = 0;

	
	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);
	pVCB = XIXFS_SB(sb);
	XIXCORE_ASSERT(pVCB);
	

	XIXCORE_ASSERT(inode);
	pFCB = XIXFS_I(inode);
	XIXCORE_ASSERT(pFCB);	



	if(pFCB->XixcoreFcb.FCBType == FCB_TYPE_DIR) {
		return -EINVAL;
	}

	lastblock = inode->i_blocks;


	offset = iblock*(1<<inode->i_blkbits);
	

	if( iblock <=  lastblock) {

		
		memset(&Address, 0, sizeof(XIXCORE_IO_LOT_INFO));
		RC = xixfs_GetAddressInfoForOffset(
							pVCB,
							pFCB,
							offset,
							pFCB->XixcoreFcb.AddrLot,
							pFCB->XixcoreFcb.AddrLotSize,
							&pFCB->XixcoreFcb.AddrStartSecIndex,
							&Reason,
							&Address
							);

		if ( RC < 0 ) {
			DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
				("ERROR (0x%x) Reason(0x%x) xixfs_get_block : xixfs_GetAddressInfoForOffset\n",
					RC, Reason ) );
			return RC;
		}

		XIXCORE_ASSERT(offset >= Address.LogicalStartOffset );
		Margin = offset - Address.LogicalStartOffset;
		physicalOffset = Address.PhysicalAddress + Margin;
		phyblock =(uint64)( physicalOffset/(1<<inode->i_blkbits));
		availableDataLen = Address.LotTotalDataSize - Margin;

	}else {

		if(!create) {
			return -EIO;
		}

redo:

		memset(&Address, 0, sizeof(XIXCORE_IO_LOT_INFO));
		RC = xixfs_GetAddressInfoForOffset(
							pVCB,
							pFCB,
							offset,
							pFCB->XixcoreFcb.AddrLot,
							pFCB->XixcoreFcb.AddrLotSize,
							&pFCB->XixcoreFcb.AddrStartSecIndex,
							&Reason,
							&Address
							);

		if ( RC < 0 ) {

			if ( RC  == -EFBIG ) {

				uint64 LastOffset = 0;
				uint32 EndLotIndex = 0;
				uint32 CurentEndIndex = 0;
				uint32 RequestStatIndex = 0;
				uint32 LotCount = 0;
				uint32 AllocatedLotCount = 0;
				
				LastOffset = offset ;
				

				CurentEndIndex = xixfs_getIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, (pFCB->XixcoreFcb.RealAllocationSize-1));
				EndLotIndex = xixfs_getIndexOfLogicalAddress(pVCB->XixcoreVcb.LotSize, LastOffset);

				if(EndLotIndex > CurentEndIndex){
					LotCount = EndLotIndex - CurentEndIndex;

				
					RC = xixfs_AddNewLot(pVCB, 
											pFCB, 
											CurentEndIndex, 
											LotCount, 
											&AllocatedLotCount, 
											pFCB->XixcoreFcb.AddrLot,
											pFCB->XixcoreFcb.AddrLotSize,
											&pFCB->XixcoreFcb.AddrStartSecIndex
											);
					
					if( RC < 0 ){
						DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
							("Fail xixfs_AddNewLot FCB LotNumber(%lld) CurrentEndIndex(%d), Request Lot(%d) AllocatLength(%lld) .\n",
								pFCB->XixcoreFcb.LotNumber, CurentEndIndex, LotCount, pFCB->XixcoreFcb.RealAllocationSize));
						
						return RC;

					}

					goto redo;

				}

				return RC;
					
			}else {

				DebugTrace( DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
					("ERROR (0x%x) xixfs_get_block(%lld) : xixfs_find_physicalBlockNo\n",
						RC , iblock));
				return RC;

			}

		}

		XIXCORE_ASSERT(offset >= Address.LogicalStartOffset );
		Margin = offset - Address.LogicalStartOffset;
		physicalOffset = Address.PhysicalAddress + Margin;
		phyblock =(uint64)( physicalOffset/(1<<inode->i_blkbits));
		availableDataLen = Address.LotTotalDataSize - Margin;



	}
	


	if(phyblock) {
		map_bh(bh_result, inode->i_sb, phyblock);
		bh_result->b_size = availableDataLen;
	}


	return 0;
}
*/


static 
int xixfs_readpage(
	struct file *file, 
	struct page *page
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_readpage .\n"));	

	
#if LINUX_VERSION_25_ABOVE			
	return mpage_readpage(page, xixfs_get_block);
#else
	return block_read_full_page(page,xixfs_get_block);
#endif
}



#if LINUX_VERSION_25_ABOVE		
static 
int
xixfs_readpages(
	struct file *file, 
	struct address_space *mapping,
	struct list_head *pages, 
	unsigned nr_pages
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_readpages .\n"));	
	
	return mpage_readpages(mapping, pages, nr_pages, xixfs_get_block);
}
#endif



#if LINUX_VERSION_25_ABOVE
static 
int 
xixfs_writepage(
	struct page *page, 
	struct writeback_control *wbc
)
{
	pgoff_t 				index= 0;
	int					RC = 0;
	struct super_block 	*sb = NULL;
	struct inode 		 	*inode = NULL;
	PXIXFS_LINUX_VCB			pVCB = NULL;
	PXIXFS_LINUX_FCB			pFCB = NULL;

	inode = page->mapping->host;
	XIXCORE_ASSERT(inode);

	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);

	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);
	
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_writepage .\n"));	


	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_writepage : is read only .\n"));	
		return -EPERM;
	}

	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_writepage : is not file .\n"));	
		return -EPERM;
	}

#if 0
	if(pFCB->XixcoreFcb.HasLock != INODE_FILE_LOCK_HAS) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_writepage : Has no lock .\n"));	
		return -EPERM;
	}
#endif

	index = page->index;

	
	RC =  block_write_full_page(page, xixfs_get_block, wbc);


	if(RC == 0 ) {
		if(pFCB->XixcoreFcb.WriteStartOffset ==  -1) {
			pFCB->XixcoreFcb.WriteStartOffset = (index << PAGE_CACHE_SHIFT);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}

		if(pFCB->XixcoreFcb.WriteStartOffset > (index << PAGE_CACHE_SHIFT) ){
			pFCB->XixcoreFcb.WriteStartOffset = (index << PAGE_CACHE_SHIFT);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Exit xixfs_writepage .\n"));	
	return RC;
}
#else
static 
int 
xixfs_writepage(
	struct page *page
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_writepage .\n"));
	
	return block_write_full_page(page,xixfs_get_block);
}
#endif

static 
int
xixfs_prepare_write(
	struct file *file, 
	struct page *page,
	unsigned from, 
	unsigned to
)
{

	//int				RC = 0;
	struct super_block *sb = NULL;
	struct inode * inode = NULL;
	PXIXFS_LINUX_VCB	pVCB = NULL;
	PXIXFS_LINUX_FCB	pFCB = NULL;

#if LINUX_VERSION_25_ABOVE
	inode = file->f_mapping->host;
#else
	inode = file->f_dentry->d_inode->i_mapping->host;
#endif

	XIXCORE_ASSERT(inode);

	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);

	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);
	
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_prepare_write .\n"));	

	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_prepare_write : is read only .\n"));	
		return -EPERM;
	}

	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_prepare_write : is not file .\n"));	
		return -EPERM;
	}

#if 0
	if(pFCB->XixcoreFcb.HasLock != INODE_FILE_LOCK_HAS) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_prepare_write : Has no lock .\n"));	
		return -EPERM;
	}
#endif

	return block_prepare_write(page,from,to,xixfs_get_block);
}


static
int 
xixfs_commit_write(
	struct file *file, 
	struct page *page,
	unsigned from, 
	unsigned to
)
{
	int 			RC = 0;
	struct super_block *sb = NULL;
	struct inode * inode = NULL;
	PXIXFS_LINUX_VCB	pVCB = NULL;
	PXIXFS_LINUX_FCB	pFCB = NULL;
	loff_t new_i_size;

#if LINUX_VERSION_25_ABOVE
	inode = file->f_mapping->host;
#else
	inode = file->f_dentry->d_inode->i_mapping->host;
#endif
	
	XIXCORE_ASSERT(inode);

	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);

	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);
	
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_commit_write .\n"));	


	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_commit_write : is read only .\n"));	
		return -EPERM;
	}

	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_commit_write : is not file .\n"));	
		return -EPERM;
	}

#if 0
	if(pFCB->XixcoreFcb.HasLock != INODE_FILE_LOCK_HAS) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR xixfs_commit_write : Has no lock .\n"));	
		return -EPERM;
	}
#endif
	RC = generic_commit_write(file, page, from, to);




	if(RC == 0 ) {

		new_i_size = ((loff_t)page->index << PAGE_CACHE_SHIFT) + to;
		if (new_i_size > i_size_read(inode)) {
			loff_t size = (u64) i_size_read(inode);
			inode->i_blocks = ((size + ((1<<inode->i_blkbits) - 1))
				   & ~((loff_t)(1<<inode->i_blkbits)- 1)) >> inode->i_blkbits;
		}
		
		if(pFCB->XixcoreFcb.WriteStartOffset ==  -1) {
			pFCB->XixcoreFcb.WriteStartOffset = (from << PAGE_CACHE_SHIFT);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}

		if(pFCB->XixcoreFcb.WriteStartOffset > (from << PAGE_CACHE_SHIFT) ){
			pFCB->XixcoreFcb.WriteStartOffset = (from << PAGE_CACHE_SHIFT);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}
	} else {
		DebugTrace(DEBUG_LEVEL_ERROR, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
				("ERROR: RC=%d\n", RC));	
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Exit xixfs_commit_write .\n"));	


	return RC;
}

#if LINUX_VERSION_25_ABOVE
static 
sector_t xixfs_bmap(
#else
static 
int xixfs_bmap(
#endif
	struct address_space *mapping, 
	sector_t block
)
{
	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_bmap .\n"));	

	return generic_block_bmap(mapping,block,xixfs_get_block);
}


static 
ssize_t
xixfs_direct_IO(
	int rw, 
#if LINUX_VERSION_25_ABOVE
	struct kiocb *iocb, 
	const struct iovec *iov,
	loff_t offset, 
	unsigned long nr_segs	
#else
	struct inode * inode,
	struct kiobuf * iobuf,
 	unsigned long blocknr, 
 	int blocksize	
#endif
)
{

	
	int				RC = 0;
	struct super_block *sb = NULL;
	PXIXFS_LINUX_VCB	pVCB = NULL;
	PXIXFS_LINUX_FCB	pFCB = NULL;
#if LINUX_VERSION_25_ABOVE	
	struct file 		*file = iocb->ki_filp;
	struct inode 		*inode = file->f_mapping->host;
#endif

	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);

	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);
	
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter xixfs_direct_IO .\n"));	

		
	if(rw == WRITE) {
		

		if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR xixfs_prepare_write : is read only .\n"));	
			return -EPERM;
		}

		if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR xixfs_prepare_write : is not file .\n"));	
			return -EPERM;
		}


		if(pFCB->XixcoreFcb.HasLock != INODE_FILE_LOCK_HAS) {
			DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("ERROR xixfs_prepare_write : Has no lock .\n"));	
			return -EPERM;
		}

		
	}


#if LINUX_VERSION_25_ABOVE

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17))
	RC =  blockdev_direct_IO(rw, iocb, inode, inode->i_sb->s_bdev, iov,
				offset, nr_segs, xixfs_get_block, NULL);
#else
	RC =  blockdev_direct_IO(rw, iocb, inode, inode->i_sb->s_bdev, iov,
				offset, nr_segs, xixfs_get_blocks, NULL);
#endif

#else
 	RC = generic_direct_IO(rw, inode, iobuf, blocknr, blocksize, xixfs_get_block);
#endif

	if( (RC == 0) && (rw == WRITE)) {
		if(pFCB->XixcoreFcb.WriteStartOffset ==  -1) {
#if LINUX_VERSION_25_ABOVE	
			pFCB->XixcoreFcb.WriteStartOffset = (int64)offset;
#else
			pFCB->XixcoreFcb.WriteStartOffset = (int64)(blocknr * blocksize);
#endif
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}
#if LINUX_VERSION_25_ABOVE	
		if(pFCB->XixcoreFcb.WriteStartOffset > (int64)offset ){
			pFCB->XixcoreFcb.WriteStartOffset = (int64)offset;
#else
		if(pFCB->XixcoreFcb.WriteStartOffset > (int64)(blocknr * blocksize) ){
			pFCB->XixcoreFcb.WriteStartOffset = (int64)(blocknr * blocksize);
#endif			
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}

		
	}


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Exit xixfs_direct_IO .\n"));	

	return RC;
}




#if LINUX_VERSION_25_ABOVE
static 
int
xixfs_writepages(
	struct address_space *mapping, 
	struct writeback_control *wbc
)
{
	pgoff_t 			index= 0;
	int				RC = 0;
	struct super_block *sb = NULL;
	struct inode * inode = NULL;
	PXIXFS_LINUX_VCB	pVCB = NULL;
	PXIXFS_LINUX_FCB	pFCB = NULL;

	inode = mapping->host;
	XIXCORE_ASSERT(inode);

	sb = inode->i_sb;
	XIXCORE_ASSERT(sb);

	pVCB = XIXFS_SB(sb);
	XIXFS_ASSERT_VCB(pVCB);
	
	pFCB = XIXFS_I(inode);
	XIXFS_ASSERT_FCB(pFCB);

	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL,
		("%d(%p)\n", inode->i_ino, inode));	


	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Enter.\n"));	


	if(pVCB->XixcoreVcb.IsVolumeWriteProtected){
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR: is read only .\n"));	
		return -EPERM;
	}

	if(pFCB->XixcoreFcb.FCBType != FCB_TYPE_FILE) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR: is not file .\n"));	
		return -EPERM;
	}

#if 0
	if(pFCB->XixcoreFcb.HasLock != INODE_FILE_LOCK_HAS) {
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("ERROR: %d(%p) Has no lock .\n", inode->i_ino, inode));	
		return -EPERM;
	}
#endif

#if LINUX_VERSION_NEW_WRITEBACK_STRUCTURE
	if(wbc->range_cyclic) {
		index = mapping->writeback_index; /*start from prev offset */
	}else {
		index = wbc->range_start >> PAGE_CACHE_SHIFT;
	}
#else
	if (wbc->sync_mode == WB_SYNC_NONE) {
		index = mapping->writeback_index; /* Start from prev offset */
	} else {
		index = 0;			  /* whole-file sweep */
	}

	if (wbc->start) {
		index = wbc->start >> PAGE_CACHE_SHIFT;
	}
#endif



	RC =  mpage_writepages(mapping, wbc, xixfs_get_block);


	if (RC == 0) {
		if(pFCB->XixcoreFcb.WriteStartOffset ==  -1) {
			pFCB->XixcoreFcb.WriteStartOffset = (index << PAGE_CACHE_SHIFT);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}

		if(pFCB->XixcoreFcb.WriteStartOffset > (index << PAGE_CACHE_SHIFT) ){
			pFCB->XixcoreFcb.WriteStartOffset = (index << PAGE_CACHE_SHIFT);
			XIXCORE_SET_FLAGS(pFCB->XixcoreFcb.FCBFlags, XIXCORE_FCB_MODIFIED_FILE_SIZE);
		}		
	}

	DebugTrace(DEBUG_LEVEL_TRACE, (DEBUG_TARGET_FCB|DEBUG_TARGET_VFSAPIT), 
			("Exit xixfs_writepages .\n"));	

	return RC;
	
}
#endif



struct address_space_operations xixfs_aops = {
	.readpage		= xixfs_readpage,
#if LINUX_VERSION_25_ABOVE			
	.readpages		= xixfs_readpages,
#endif
	.writepage		= xixfs_writepage,			
	.sync_page		= block_sync_page,
	.prepare_write	= xixfs_prepare_write,
	.commit_write		= xixfs_commit_write,
	.bmap			= xixfs_bmap,
	.direct_IO		= xixfs_direct_IO,
#if LINUX_VERSION_25_ABOVE		
	.writepages		= xixfs_writepages,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16))
	.migratepage		= buffer_migrate_page,
#endif
};



/*
struct address_space_operations ext2_aops = {
	.readpage		= ext2_readpage,
	.readpages		= ext2_readpages,
	.writepage		= ext2_writepage,
	.sync_page		= block_sync_page,
	.prepare_write		= ext2_prepare_write,
	.commit_write		= generic_commit_write,
	.bmap			= ext2_bmap,
	.direct_IO		= ext2_direct_IO,
	.writepages		= ext2_writepages,
	.migratepage		= buffer_migrate_page,
};

static struct address_space_operations fat_aops = {
	.readpage	= fat_readpage,
	.readpages	= fat_readpages,
	.writepage	= fat_writepage,
	.writepages	= fat_writepages,
	.sync_page	= block_sync_page,
	.prepare_write	= fat_prepare_write,
	.commit_write	= fat_commit_write,
	.direct_IO	= fat_direct_IO,
	.bmap		= _fat_bmap
};
*/
