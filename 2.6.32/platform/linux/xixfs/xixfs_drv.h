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
#ifndef __XIXFS_DRIVER_H__
#define __XIXFS_DRIVER_H__

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include <linux/fs.h>
#include <linux/time.h>
#if LINUX_VERSION_25_ABOVE
#include <linux/jiffies.h>
#endif

#include <xixfsevent/xixfs_event.h>
#include "xixcore/xctypes.h"
#include "xixcore/volume.h"
#include "xixcore/bitmap.h"
#include "xixcore/dir.h"
#include "xixfs_types.h"
#include "xixfs_xbuf.h"
#include "ndasdev.h"



/*
 * Structure node type
 */
#define	XIXFS_NODE_TYPE_VCB	(0x1901)
#define	XIXFS_NODE_TYPE_FCB	(0x1902)

typedef struct _NODE_TYPE {
	uint16	Type;
	uint16	Size;
}NODE_TYPE, *PNODE_TYPE ;

#if defined(XIXCORE_DEBUG)
#define XIXFS_NODE_TYPE(P)		( *((uint16 *)(P)) )
#define XIXFS_ASSERT_STRUCT(S, T)	XIXCORE_ASSERT( XIXFS_NODE_TYPE(S) == (T) )
#define XIXFS_ASSERT_VCB(F)			XIXFS_ASSERT_STRUCT((F), XIXFS_NODE_TYPE_VCB)
#define XIXFS_ASSERT_FCB(F)			XIXFS_ASSERT_STRUCT((F), XIXFS_NODE_TYPE_FCB)


#else
#define XIXFS_ASSERT_FCB(p) do{;}while(0)
#define XIXFS_ASSERT_VCB(p) do{;}while(0)
#endif //#if defined(XIXCORE_DEBUG)



#if LINUX_VERSION_25_ABOVE
#define  XIXFS_TIC_P_SEC		(msecs_to_jiffies(1000))
#else
#define  XIXFS_TIC_P_SEC		(HZ)
#endif
#define DEFAULT_XIXFS_RECHECKRESOURCE	(180 * XIXFS_TIC_P_SEC)
#define DEFAULT_XIXFS_UPDATEWAIT		(180 * XIXFS_TIC_P_SEC)
#define DEFAULT_XIXFS_UMOUNTWAIT		(30 * XIXFS_TIC_P_SEC)
#define DEFAULT_XIXFS_CHCKE_EVENT		(XIXFS_TIC_P_SEC)
#define DEFAULT_XIXFS_CHECK_REQ			(3*XIXFS_TIC_P_SEC)


typedef struct _XIXFS_LINUX_META_CTX {

	wait_queue_head_t		VCBMetaEvent;
#if !LINUX_VERSION_25_ABOVE
	struct timer_list		VCBMetaTimeOut;
#endif
	//wait_queue_head_t		VCBResourceEvent;
	spinlock_t			VCBResourceWaitSpinlock;
	struct list_head		VCBResourceWaitList;

#if LINUX_VERSION_25_ABOVE	
	struct completion		VCBMetaThreadStopCompletion;
#else
	spinlock_t			VCBMetaThreadStopWaitSpinlock;
	struct list_head		VCBMetaThreadStopWaitList;
#endif
	// Spin lock storage for Xixcore meta context
	spinlock_t			MetaLock;

	int					MetaThread;

	struct _XIXFS_LINUX_VCB	*VCBCtx;
	
} XIXFS_LINUX_META_CTX, *PXIXFS_LINUX_META_CTX;





typedef struct _XIXFS_WAIT_CTX {
	struct list_head	WaitList;
	struct completion  WaitCompletion;
}XIXFS_WAIT_CTX, *PXIXFS_WAIT_CTX;


#define XIXFS_VCB_STATUS_VOL_NON				(0x00000000)
#define XIXFS_VCB_STATUS_VOL_MOUNTED			(0x00000001)
#define XIXFS_VCB_STATUS_VOL_IN_MOUNTING	(0x00000002)
#define XIXFS_VCB_STATUS_VOL_DISMOUNTED		(0x00000004)
#define XIXFS_VCB_STATUS_VOL_IN_DISMOUNTING	(0x00000008)
#define XIXFS_VCB_STATUS_VOL_INVALID			(0x00000010)

#define XIXFS_HASH_BITS			(8)
#define XIXFS_HASH_SIZE			(1UL << XIXFS_HASH_BITS)
#define XIXFS_HASH_MASK		(XIXFS_HASH_SIZE-1)

struct xixfs_mount_option {

	char *iocharset;          /* Charset used for filename input/display */
	unsigned long			debug_level_mask;
	unsigned long			debug_target_mask;
	unsigned long			enable_debug;
};



typedef struct _XIXFS_LINUX_VCB {
#if LINUX_VERSION_25_ABOVE
	NODE_TYPE		N_TYPE;
#else
	NODE_TYPE		n_type;
#endif
	struct list_head	VCBLink;
	rwlock_t			VCBLock;
	spinlock_t		ChildCacheLock;
	XIXFS_LINUX_META_CTX	MetaCtx;
	long				VCBState;
	long				VCBFlags;
	struct xixfs_mount_option 	mountOpts;
	struct super_block 	*supreblockCtx; 

	/*
	 * Xixcore VCB
	 */

	XIXCORE_VCB		XixcoreVcb;

#if LINUX_VERSION_25_ABOVE		
	struct hd_struct	PartitionInfo;
#else
	ndas_xixfs_partinfo PartitionInfo;
#endif

	struct	inode 	*RootInode;
	struct nls_table	*nls_map;
	spinlock_t		inode_hash_lock;
#if LINUX_VERSION_25_ABOVE
	struct hlist_head 	inode_hashtable[XIXFS_HASH_SIZE	];
#else
	struct list_head 	inode_hashtable[XIXFS_HASH_SIZE	];
#endif
}XIXFS_LINUX_VCB, *PXIXFS_LINUX_VCB;



static inline PXIXFS_LINUX_VCB 
XIXFS_SB( struct super_block * sb)
{
#if LINUX_VERSION_25_ABOVE
	return (PXIXFS_LINUX_VCB) sb->s_fs_info;
#else
	return (PXIXFS_LINUX_VCB) sb->u.generic_sbp;
#endif
}

int 
xixfs_ScanForPreMounted(struct super_block *sb);


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,48)
#define get_seconds() CURRENT_TIME
#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0))
static inline loff_t i_size_read(struct inode * inode)
{
	return inode->i_size;
}

static inline void i_size_write(struct inode * inode, uint64 size)
{
	inode->i_size = (loff_t)size;
}


#define map_bh(bh, sb, block) ({				\
	bh->b_dev = kdev_t_to_nr(sb->s_dev);			\
	bh->b_blocknr = block;					\
	bh->b_state |= (1UL << BH_Mapped);			\
})

#endif

typedef struct _XIXFS_LINUX_FCB {
#if LINUX_VERSION_25_ABOVE
	NODE_TYPE		N_TYPE;
	struct hlist_node	FCB_hash;
#else
	NODE_TYPE		n_type;
	struct list_head	FCB_hash;
#endif
	spinlock_t		FCBLock;

	atomic_t		FCBCleanup;
   	atomic_t		FCBReference;
   	atomic_t		FCBUserReference;
	atomic_t		FCBCloseCount;

	/*
	 * Xixcore FCB
	 */
	XIXCORE_FCB		XixcoreFcb;
	// Semaphore storage for Xixcore FCB
	struct semaphore LinuxAddressLock;

	// linux vfs inode
	struct inode		linux_inode;
}XIXFS_LINUX_FCB, *PXIXFS_LINUX_FCB;


static inline
PXIXFS_LINUX_FCB
XIXFS_I(struct inode *inode) 
{
	return container_of(inode, XIXFS_LINUX_FCB, linux_inode);
}

/*
 *
 */

int
xixfs_GetMoreCheckOutLotMap(
	PXIXFS_LINUX_META_CTX		pCtx 
);


int
xixfs_UpdateMetaData(
	PXIXFS_LINUX_META_CTX		pCtx 
);


int
xixfs_ResourceThreadFunction(
		void 	*lpParameter
);

/*
	As of 2.6.18 linux writeback_control structure is changed
*/
#define LINUX_VERSION_NEW_WRITEBACK_STRUCTURE\
	((LINUX_VERSION_CODE >=KERNEL_VERSION(2,6,18)) )

/*
	As of 2.6.18 linux superblock operation interface is changed
*/
#define LINUX_VERSION_NEW_SUPER_OP\
	((LINUX_VERSION_CODE >=KERNEL_VERSION(2,6,18)) )

/*
	As of 2.6.18 linux file system interface is changed
*/
#define LINUX_VERSION_NEW_FILE_SYSTEM_TYPE\
	((LINUX_VERSION_CODE >=KERNEL_VERSION(2,6,18)) )

/*
	As of over 2.6.0 linux kernel
*/
#define LINUX_VERSION_OVER_2_6\
	((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) )

/*
	As of 2.6.0 linux work_queue routine support
*/
#define LINUX_VERSION_WORK_QUEUE\
	((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)) )

/*
	As of over 2.6.16 linux kernel
*/
#define LINUX_VERSION_OVER_2_6_16\
	((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)) )

/*
	As of over 2.6.19 linux kernel repleace generic_file_read/write to do_sync_read/write
*/
#define LINUX_VERSION_2_6_19_REPLACE_INTERFACE\
	((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)) )

/*
	As of over 2.6.20 INIT_WORK change the interface and structure : parameter is relayed with in work_struct 
*/
#define LINUX_VERSION_2_6_20_NEW_WORK_INTERFACE\
	((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)) )


/*
	As of over 2.6.22 SLAB changed to 
*/
#define LINUX_VERSION_2_6_22_CHANGE_SLAB_FLAGS_FUNCTION\
	((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)) )

#endif //#ifndef __XIXFS_DRIVE_H__
