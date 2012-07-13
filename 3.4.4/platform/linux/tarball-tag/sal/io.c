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
      notice, this list of conditions and the following disclaimer#include "sal/io.h"
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
#include <linux/version.h> // LINUX_VERSION
#include <linux/module.h> // EXPORT_SYMBOL
#include <linux/spinlock.h> // spinklock_t
#include <linux/sched.h> // jiffies
#include <linux/semaphore.h> // struct semaphore
#include <asm/atomic.h> // atomic
#include <linux/mm.h> //
#include <linux/slab.h> // 2.4 kmalloc/kfree
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/ide.h>
#include <linux/mutex.h>

#include <linux/time.h>

#include "sal/sync.h"
#include "sal/thread.h"
#include "sal/time.h"
#include "sal/mem.h"
#include "sal/debug.h"
#include "sal/types.h"
#include "sal/io.h"

#include "linux_ver.h" // SAL_HZ

DEFINE_MUTEX(fs_mutex);

NDAS_SAL_API sal_file sal_file_open(const char * filename, int flags, int mode)
{
	int linux_flags = 0;
	sal_file nfile;
	if (flags & SAL_FILE_FLAG_RO) 
		linux_flags |= O_RDONLY;
	if (flags & SAL_FILE_FLAG_RW)
		linux_flags |= O_RDWR;
	if (flags & SAL_FILE_FLAG_DIRECT) {
		linux_flags |= O_DIRECT;
#if 0 /* Envision test code */
              int ret;
		linux_flags |= O_DIRECT;
              ret = set_blocksize(MKDEV(3,0), 512);  /* envision emulator test */
              if (ret !=0) {
                    printk("Failed to set blocksize ret=%d\n", ret);
              }
#endif
	}
	if(flags & SAL_FILE_FLAG_EXCL)
		linux_flags |= O_EXCL;
	nfile = (sal_file) filp_open(filename, linux_flags, mode);

	if (IS_ERR((void*)nfile)) {
		printk("SAL: Failed to open file %s\n", filename);
		return NULL;       
	}
	return nfile;
}
EXPORT_SYMBOL(sal_file_open);

NDAS_SAL_API xbool     sal_file_get_size(sal_file file, xuint64* size)
{
	struct file* filp = (struct file*) file;
	mm_segment_t oldfs;
	int	error = ENOTTY;
	xbool ret = FALSE;
#if !(LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
	int size32;
#endif

	mutex_lock(&fs_mutex);

        if (filp->f_dentry &&
		filp->f_dentry->d_inode &&
		filp->f_dentry->d_inode->i_size !=0) {
            /* Block special file's i_size is 0 */
            ret = TRUE;
            *size = filp->f_dentry->d_inode->i_size;
        } else if (filp->f_op) {
		oldfs = get_fs();
		set_fs(get_ds());
		error = filp->f_op->unlocked_ioctl(filp, BLKGETSIZE64, (unsigned long)size);
		smp_mb();
		set_fs(oldfs);
		if (error) {
			ret = FALSE;
			printk("SAL: unlocked_ioctl error=%d\n", error);
		}
		else {
		/* printk("SAL: Block size = %lld(unlocked_ioctl)\n", *size); */
			ret = TRUE;
		}
		
	} else {
		ret = FALSE;
	}
	mutex_unlock(&fs_mutex);
	return ret;
}

EXPORT_SYMBOL(sal_file_get_size);

NDAS_SAL_API void     sal_file_close(sal_file file)
{
	struct file* filp = (struct file*) file;
	if (filp->f_op && filp->f_op->fsync) {
		/* printk("SAL: Syncing..\n"); */
		filp->f_op->fsync(filp, 0, LLONG_MAX, 0);
	}
	filp_close(filp, NULL);
}
EXPORT_SYMBOL(sal_file_close);

NDAS_SAL_API xint32     sal_file_read(sal_file file, xuchar* buf, xint32 size, xuint64 offset)
{
	struct file* filp = (struct file*) file;
	mm_segment_t oldfs;
	int retval;
	loff_t foffset = offset;
/*	printk("SAL: read pos=%llx, size = %d, buf=%p\n", offset, size, buf); */
	oldfs = get_fs();
	set_fs(get_ds());
#if 1
	retval = filp->f_op->read(filp, buf, size, &foffset);
#else
	// read throughput test. do nothing when read...
	retval = size;
#endif
	smp_mb();
	set_fs(oldfs);	
	if (retval !=size) {
            	printk("SAL: read error buf=%p, ret=%d\n", buf, retval);
		return -1;
	} else {
/*        	printk("SAL: read returning\n"); */
		return size;
	}
}
EXPORT_SYMBOL(sal_file_read);

NDAS_SAL_API xint32     sal_file_write(sal_file file, xuchar* buf, xint32 size, xuint64 offset)
{
	struct file* filp = (struct file*) file;
	mm_segment_t oldfs;
	int retval;
	loff_t foffset = offset;
	
//	printk("SAL: pos=%llx, write size = %d\n", offset, size);
	oldfs = get_fs();
	set_fs(get_ds());
#if 1	
	retval = filp->f_op->write(filp, buf, size, &foffset);
#else
	// write throughput test. do nothing when read...
	retval = size;
#endif
	smp_mb();
	set_fs(oldfs);
	if (retval !=size) 
		return -1;
	else
		return size;
}
EXPORT_SYMBOL(sal_file_write);

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))

//
// Linux read actor routine.
// Relay the control to SAL read actor routine.
//
// This is only called further down in another function with this kernel version
//
#if LINUX_VERSION_25_ABOVE && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))

static
int
sal_read_actor(read_descriptor_t * read_desc, struct page * page, unsigned long offset, unsigned long size)
{
	sal_read_actor_context *sal_read_actor;
	sal_read_descriptor_t sal_read_desc;
	int ret;

//	printk("SAL: read actor\n");
#if LINUX_VERSION_25_ABOVE
	sal_read_actor = (sal_read_actor_context *)read_desc->arg.data;
#else
	sal_read_actor = (sal_read_actor_context *)read_desc->buf;
#endif
	if(sal_read_actor == NULL) {
		return 0;
	}

	//
	// Initilize the SAL read descriptor to be passed to the user's read actor routine.
	//

	sal_read_desc.written = read_desc->written;
	sal_read_desc.count = read_desc->count;
	sal_read_desc.arg.data = sal_read_actor->target;
	sal_read_desc.error = read_desc->error;

	ret = sal_read_actor->sal_read_actor(
		&sal_read_desc,
		(sal_page *)page,
		offset,
		size);

	read_desc->written = sal_read_desc.written;
	read_desc->count = sal_read_desc.count;
	sal_read_actor->target = sal_read_desc.arg.data;
	read_desc->error = sal_read_desc.error;

	return ret;
}
/* sal read_actor */
#endif


NDAS_SAL_API xint32     sal_file_send(sal_file file, xuint64 offset, xint32 size, sal_read_actor_context * read_actor_ctx)
{
	mm_segment_t oldfs;
	int retval;
	oldfs = get_fs();
	set_fs(get_ds());
#if 1
#if LINUX_VERSION_25_ABOVE && (LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,22))
        /* sendfile in file operation is deprecated in 2.6.23. Need to be implemented with splice in 2.6.23 kernel. */
	{
		struct file* filp = (struct file*) file;
		loff_t foffset = offset;
//		printk("SAL: pos=%llx, read size = %d\n", filp->f_pos, size);
		retval = filp->f_op->sendfile(filp, &foffset, size, sal_read_actor, read_actor_ctx);
	}
#else
        printk("Not implemented\n");
/*        do_generic_file_read(filp, &foffset, (read_descriptor_t *) read_actor_ctx, sal_read_actor);*/
        retval = 0;
#endif
#else
	/* read throughput test. do nothing when read... */
	retval = size;
#endif
	smp_mb();
	set_fs(oldfs);	
	if (retval !=size) 
		return -1;
	else
		return size;
}
EXPORT_SYMBOL(sal_file_send);

#else // (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,0))

NDAS_SAL_API xint32     sal_file_send(sal_file file, xuint64 offset, xint32 size, sal_read_actor_context * read_actor_ctx)
{
    printk("Not implemented\n");
    return -1;
}
EXPORT_SYMBOL(sal_file_send);
#endif

