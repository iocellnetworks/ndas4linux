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
#include <linux/module.h> // EXPORT_NO_SYMBOLS, MODULE_LICENSE
#include <linux/version.h> // LINUX_VERSION_CODE, KERNEL_VERSION
#include <linux/init.h> // module_init, module_exit
#include <ndasuser/ndasuser.h>
#include <sal/sal.h>
#include "procfs.h"
#ifdef XPLAT_XIXFS_EVENT
#include "ndasdev.h"
#endif //#ifdef XPLAT_XIXFS_EVENT
#include "linux_ver.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
MODULE_LICENSE("Dual BSD/GPL");
#endif
MODULE_AUTHOR("Ximeta, Inc.");
extern int ndas_ctrldev_init(void);
extern int ndas_ctrldev_cleanup(void);
extern int blk_init(void);
extern int blk_cleanup(void);

int ndas_block_init(void) 
{    
    int ret;

    init_ndasproc();
    ret = ndas_ctrldev_init();
    if (ret) {
        printk("Failed to initialize NDAS control device\n");
    }
    ret = blk_init();
    if (ret) {
        printk("Failed to initialize NDAS block device\n");
    }

    return ret;
}

void ndas_block_exit(void) 
{
    blk_cleanup();
    ndas_ctrldev_cleanup();
    cleanup_ndasproc();
}
module_init(ndas_block_init);
module_exit(ndas_block_exit);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,0))
EXPORT_NO_SYMBOLS;
#endif


#ifdef XPLAT_XIXFS_EVENT
EXPORT_SYMBOL(ndas_block_xixfs_lock);

#if !LINUX_VERSION_25_ABOVE
EXPORT_SYMBOL(ndas_block_xixfs_direct_io);
#endif //#if !LINUX_VERSION_25_ABOVE
#endif //#ifdef XPLAT_XIXFS_EVENT

