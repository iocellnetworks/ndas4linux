/*
 * unistr.c - NTFS Unicode string handling. Part of the Linux-NTFS project.
 *
 * Copyright (c) 2001-2005 Anton Altaparmakov
 *
 * This program/include file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program/include file is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (in the main directory of the Linux-NTFS
 * distribution in the file COPYING); if not, write to the Free Software
 * Foundation,Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
	simple copy and paste from NTFS porject change name
	ntfs_nlstoucs	--> xixfs_fs_to_uni_str
	ntfs_ucstonls	--> xixfs_uni_to_fs_str


*/

#include "linux_ver.h"
#if LINUX_VERSION_25_ABOVE
#include <linux/fs.h> // blkdev_ioctl , register file system
#include <linux/buffer_head.h> // file_fsync
#include <linux/genhd.h>  // rescan_parition
#include <linux/workqueue.h>  // 
#else
#include <linux/kdev_t.h> // kdev_t for linux/blkpg.h
#endif

#include <linux/slab.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <linux/nls.h>
#include <linux/types.h>

#include "xcsystem/debug.h"
#include "xcsystem/system.h"

#include "xixfs_global.h"
#include "xixfs_drv.h"
#include "xixfs_name_cov.h"


/* Define module name for Xixcore debug module */
#ifdef __XIXCORE_MODULE__
#undef __XIXCORE_MODULE__
#endif
#define __XIXCORE_MODULE__ "XIXFSNAMECONV"


void *
xixfs_large_malloc(unsigned long size, gfp_t gfp_mask)
{
	if (likely(size <= PAGE_SIZE)) {
		BUG_ON(!size);
		/* kmalloc() has per-CPU caches so is faster for now. */
		return kmalloc(PAGE_SIZE, gfp_mask & ~__GFP_HIGHMEM);
		/* return (void *)__get_free_page(gfp_mask); */
	}
	if (likely(size >> PAGE_SHIFT < num_physpages))
		return __vmalloc(size, gfp_mask, PAGE_KERNEL);
	return NULL;
}


void
xixfs_large_free(void *addr)
{
	if (likely(((unsigned long)addr < VMALLOC_START) ||
			((unsigned long)addr >= VMALLOC_END ))) {
		kfree(addr);
		/* free_page((unsigned long)addr); */
		return;
	}
	vfree(addr);
}


void 
xixfs_conversioned_name_free(__le16 * name)
{
	xixfs_large_free(name);
}



int 
xixfs_fs_to_uni_str(
	const PXIXFS_LINUX_VCB pVCB, 
	const char *ins,
	const int ins_len, 
	__le16 **outs
	)
{
	struct nls_table *nls = pVCB->nls_map;
	__le16 *ucs;
	wchar_t wc;
	int i, o, wc_len;

	/* We don't trust outside sources. */
	if (ins) {
		ucs = xixfs_large_malloc( (XIXCORE_MAX_NAME_LEN*sizeof(__le16)), GFP_KERNEL);
		if (ucs) {
			for (i = o = 0; i < ins_len; i += wc_len) {
				wc_len = nls->char2uni(ins + i, ins_len - i,
						&wc);
				if (wc_len >= 0) {
					if (wc) {
						ucs[o++] = cpu_to_le16(wc);
						continue;
					} /* else (!wc) */
					break;
				} /* else (wc_len < 0) */
				goto conversion_err;
			}
			ucs[o] = 0;
			*outs = ucs;
			return o;
		} /* else (!ucs) */
		DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Failed to allocate name from xixfs_name_cache!\n"));
		return -ENOMEM;
	} /* else (!ins) */
	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Received NULL pointer.\n"));
	return -EINVAL;
conversion_err:
	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
			("Name using character set %s contains characters that cannot be converted to Unicode.", nls->charset));
	xixfs_large_free(ucs);
	return -EILSEQ;
}



int 
xixfs_uni_to_fs_str(
	const PXIXFS_LINUX_VCB pVCB, 
	const __le16 *ins,
	const int ins_len, 
	unsigned char **outs, 
	int outs_len
	)
{
	struct nls_table *nls = pVCB->nls_map;
	unsigned char *ns;
	int i, o, ns_len, wc;

	/* We don't trust outside sources. */
	if (ins) {
		ns = *outs;
		ns_len = outs_len;
		if (ns && !ns_len) {
			wc = -ENAMETOOLONG;
			goto conversion_err;
		}
		if (!ns) {
			ns_len = ins_len * NLS_MAX_CHARSET_SIZE;
			ns = (unsigned char*)kmalloc(ns_len + 1, GFP_NOFS);
			if (!ns)
				goto mem_err_out;
		}
		for (i = o = 0; i < ins_len; i++) {
retry:			wc = nls->uni2char(le16_to_cpu(ins[i]), ns + o,
					ns_len - o);
			if (wc > 0) {
				o += wc;
				continue;
			} else if (!wc)
				break;
			else if (wc == -ENAMETOOLONG && ns != *outs) {
				unsigned char *tc;
				/* Grow in multiples of 64 bytes. */
				tc = (unsigned char*)kmalloc((ns_len + 64) &
						~63, GFP_NOFS);
				if (tc) {
					memcpy(tc, ns, ns_len);
					ns_len = ((ns_len + 64) & ~63) - 1;
					kfree(ns);
					ns = tc;
					goto retry;
				} /* No memory so goto conversion_error; */
			} /* wc < 0, real error. */
			goto conversion_err;
		}
		ns[o] = 0;
		*outs = ns;
		return o;
	} /* else (!ins) */
	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Received NULL pointer.\n"));
	return -EINVAL;
conversion_err:
	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
		("Unicode name contains characters that cannot be "
			"converted to character set %s.  You might want to "
			"try to use the mount option nls=utf8.", nls->charset));

	if (ns != *outs)
		kfree(ns);
	if (wc != -ENAMETOOLONG)
		wc = -EILSEQ;
	return wc;
mem_err_out:
	DebugTrace(DEBUG_LEVEL_ERROR, DEBUG_TARGET_ALL, 
				("Failed to allocate name!\n"));
	return -ENOMEM;
}
