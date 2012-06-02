/***************************************************************************
 *   Copyright (C) 2007 by root   *
 *   root@fedora7   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#ifndef __NDAS_FAT_PROC_H__
#define __NDAS_FAT_PROC_H__

#define __NDAS_FAT__	1

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

#include <linux/fs.h>
#include <linux/msdos_fs.h>
#include <linux/buffer_head.h>

#include <linux/slab.h>
#include <linux/time.h>
#include <linux/dirent.h>
#include <linux/smp_lock.h>
#include <linux/compat.h>
#include <asm/uaccess.h>

#include <linux/capability.h>
#include <linux/writeback.h>
#include <linux/backing-dev.h>
#include <linux/blkdev.h>

#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/pagemap.h>
#include <linux/mpage.h>
#include <linux/mount.h>
#include <linux/vfs.h>
#include <linux/parser.h>
#include <linux/uio.h>
#include <linux/writeback.h>
#include <asm/unaligned.h>

#include <linux/jiffies.h>
#include <linux/ctype.h>
#include <linux/namei.h>
#include <linux/delay.h>

#include <linux/kgdb.h>

# if defined(__BIG_ENDIAN)
#	define ntohll(x) (x)
#	define htonll(x) (x)
# elif defined(__LITTLE_ENDIAN)
#	define ntohll(x)  be64_to_cpu(x)
#	define htonll(x)  cpu_to_be64(x)
# else
#	error "Could not determine byte order"
# endif

#include "md5.h"

#include "ndasfsprotocol.h"

#include "../inc/lpx/lpxutil.h"

#include "ndftprotocolheader.h"


#include "windowscontext.h"
#include "winlpx.h"
#include "ndasfsinterface.h"
#include "secondary.h"

#include "ndasfat.h"
#include "ndft.h"

#define NDAS_ASSERT(expr) 								\
        if(!(expr)) {									\
        printk( "Assertion failed! %s,%s,%s,line=%d\n",	\
        #expr,__FILE__,__FUNCTION__,__LINE__);			\
		breakpoint();									\
        }

#define NDASFAT_PRINTK(LEVEL, EXPR)			\
		if (LEVEL <= NdasfatDebugLevel) { 	\
			printk( "ndasfat: %s : ", __FUNCTION__ );	\
			printk EXPR;	\
		}

#define NDAS_ASSERT_INSUFFICIENT_RESOURCES	true

#define FlagOn(_F, _SF)		((_F) & (_SF))
#define SetFlag(_F, _SF)	((_F) |= (_SF))
#define ClearFlag(_F, _SF)	((_F) &= ~(_SF))

static inline 
LARGE_INTEGER 
NdasCurrentTime (
	VOID
	)
{
	LARGE_INTEGER	time;
	
	time.LowPart = jiffies;

	return time;
}

int init_vfat_fs(void);
void exit_vfat_fs(void);

int init_fat_fs(void);
void exit_fat_fs(void);

// ndft.c

int 
NdftInit (
	void
	); 

void 
NdftExit (
	void
	);
 
int 
QueryPrimary (
	PNETDISK_INFORMATION	NetdiskInformation,
	struct sockaddr_lpx		*PrimaryAddress
	);

int 
AddNetdisk (
	PNETDISK_INFORMATION	NetdiskInformation,
	bool					Primary,
	struct sockaddr_lpx		*PrimaryAddress
	);

void 
RemoveNetdisk (
	PNETDISK_INFORMATION	NetdiskInformation
	);

NTSTATUS
SendMessage (
	IN PFILE_OBJECT			ConnectionFileObject,
	IN int					*SendNdasFcStatistics,
	IN PLARGE_INTEGER		TimeOut,
	IN UINT8				*Buffer, 
	IN UINT32				BufferLength
	);

NTSTATUS
RecvMessage (
	IN PFILE_OBJECT			ConnectionFileObject,
	IN int					*RecvNdasFcStatistics,
	IN PLARGE_INTEGER		TimeOut,
	IN UINT8				*Buffer, 
	IN UINT32				BufferLength
	);

VOID
SuperblockExtReference (
	PSUPERBLOCK_EXTENSION	SuperblockExt
	);

VOID
SuperblockExtDereference (
	PSUPERBLOCK_EXTENSION	SuperblockExt
	);

PSECONDARY
SecondaryCreate (
	IN	PSUPERBLOCK_EXTENSION	SuperblockExt
	);

VOID
SecondaryClose (
	IN  PSECONDARY	Secondary
	);

VOID
SecondaryReference (
	IN  PSECONDARY	Secondary
	);

VOID
SecondaryDereference (
	IN  PSECONDARY	Secondary
	);

PSECONDARY_REQUEST
AllocSecondaryRequest (
	IN	PSECONDARY	Secondary,
	IN 	UINT32		MessageSize,
	IN	BOOLEAN		Synchronous
	); 

VOID
ReferenceSecondaryRequest (
	IN	PSECONDARY_REQUEST	SecondaryRequest
	); 

VOID
DereferenceSecondaryRequest (
	IN  PSECONDARY_REQUEST	SecondaryRequest
	);

FORCEINLINE
NTSTATUS
QueueingSecondaryRequest (
	IN	PSECONDARY			Secondary,
	IN	PSECONDARY_REQUEST	SecondaryRequest
	);

// secondarythread.c

int
SecondaryThreadProc (
	void *arg
	);

#endif
