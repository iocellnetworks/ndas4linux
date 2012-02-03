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
#ifndef __XIXFS_LINUX_GLOBAL_H__
#define __XIXFS_LINUX_GLOBAL_H__

#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/completion.h>
#include "xixfsevent/xixfs_event.h"
#include "xixfs_types.h"



#define XIXFS_EVENT_SRV_SHUTDOWN		0x00000001
#define XIXFS_EVENT_SRV_RECVPKT			0x00000002
#define XIXFS_EVENT_SRV_ERRNETWORK	0x00000004
#define XIXFS_EVENT_SRV_MASK			0x00000007


#define XIXFS_EVENT_CLI_SHUTDOWN		0x00000001
#define XIXFS_EVENT_CLI_SENDPKT			0x00000002
#define XIXFS_EVENT_CLI_ERRNETWORK		0x00000004
#if LINUX_VERSION_25_ABOVE
#define XIXFS_EVENT_CLI_MASK			0x00000007
#else
#define XIXFS_EVENT_CLI_TIMEOUT			0x00000008
#define XIXFS_EVENT_CLI_MASK			0x0000000F
#endif


typedef struct _XIXFS_SERVICE_CTX {
	spinlock_t			Eventlock;
	atomic_t				PacketNumber;
	struct completion		SrvCompletion;
	wait_queue_head_t		SrvEvent;
	long					SrvEventFlags;
	spinlock_t			RecvPktListLock;
	struct list_head		RecvPktList;
	XIXFS_EVENT_CTX		SrvCtx;
	int					SrvThreadId;


	struct completion		CliCompletion;
	wait_queue_head_t		CliEvent;
#if !LINUX_VERSION_25_ABOVE
	struct timer_list		CliEventTimeOut;
#endif
	long					CliEventFlags;
	spinlock_t			SendPktListLock;
	struct list_head		SendPktList;
	XIXFS_EVENT_CTX		CliCtx;
	int					CliThreadId;

	void					*GDCtx;

	
}XIXFS_SERVICE_CTX, *PXIXFS_SERVICE_CTX;




typedef struct _XIXFS_LINUX_DATA{
	// Access control for superblock list
	spinlock_t		sb_list_lock;
	struct list_head	sb_list;
	
	int32 			EventThreadId;

	// Spin lock storage for Xixcore globals.
	spinlock_t			xixcore_tmp_lot_lock_list_lock;

	// global information
	struct semaphore	sb_datalock;

	uint32			IsInitialized;

	// event handling context
	XIXFS_SERVICE_CTX EventCtx;

}XIXFS_LINUX_DATA, *PXIXFS_LINUX_DATA;


extern XIXFS_LINUX_DATA	xixfs_linux_global;


extern struct workqueue_struct *xixfs_workqueue;
#endif // #ifndef __XIXFS_GLOBAL_H__
