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

#include <linux/module.h> // EXPORT_NO_SYMBOLS, MODULE_LICENSE
#include <linux/version.h> // LINUX_VERSION_CODE, KERNEL_VERSION
#include <linux/init.h> // module_init, module_exit
#include <linux/netdevice.h> // dev_base dev_base_lock
#include <ndasuser/ndasuser.h>
#include <ndasuser/persist.h>
#include <ndasuser/io.h>
#include <ndasuser/write.h>
#include <ndasuser/bind.h>
#ifdef NDAS_MSHARE
#include <ndasuser/mediaop.h>
#endif//#ifdef NDAS_MSHARE
#include "linux_ver.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
MODULE_LICENSE("Dual BSD/GPL");
#endif
MODULE_AUTHOR("IOCELL Networks");

char* dev = "/dev/discs/disc0/disc";
//char* dev = "/dev/sda07";
NDAS_MODULE_PARAM_STRING(dev, S_IRUGO);
NDAS_MODULE_PARAM_DESC(dev, "Device or file name that will be used for emulator device.");

// 
// NDAS emulator works only when NDAS library is built with NDAS_EMU defined.
//
int ndas_emu_init(void) 
{
	int ret;
//	ret = ndas_emu_start(dev, 1, 32);
	ret = ndas_emu_start(dev, 1, 64);

	if (ret) {
		if (NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION == ret) {
			printk("ndas: NDAS binary is not built for NDAS emulator\n");
		} else {
			printk("ndas: failed to initailize emulator\n");
		}
		// Failed to load
		ndas_emu_stop();
		return -1;
	}
    return 0;
}

void ndas_emu_exit(void) 
{    
	ndas_emu_stop();
}

module_init(ndas_emu_init);
module_exit(ndas_emu_exit);

