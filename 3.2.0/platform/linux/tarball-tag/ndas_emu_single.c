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
#include "linux_ver.h"
#include "ndasdev.h"
#include "sal/sal.h"
#include "sal/net.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
MODULE_LICENSE("Dual BSD/GPL");
#endif
MODULE_AUTHOR("IOCELL Networks");

char* dev = "/dev/discs/disc0/disc"; //"/dev/hda";
NDAS_MODULE_PARAM_STRING(dev, S_IRUGO);
NDAS_MODULE_PARAM_DESC(dev, "Device or file name that will be used for emulator device.");
    
char* ndas_dev = NULL;
NDAS_MODULE_PARAM_STRING(ndas_dev, 0);
NDAS_MODULE_PARAM_DESC(ndas_dev, "If set, use only given network interface for NDAS. Set as NULL to use all interfaces.");

#ifndef NDAS_IO_UNIT
#define NDAS_IO_UNIT 64 // KB
#endif
#ifndef NDAS_SOCK_IO_UNIT
#define NDAS_SOCK_IO_UNIT 32 // KB
#endif

int ndas_io_unit = NDAS_IO_UNIT;
int ndas_sock_io_unit = NDAS_SOCK_IO_UNIT;
int ndas_max_slot = NDAS_MAX_SLOT;
int ndas_max_jobs = 350;

static ndas_error_t unregister_network(void) {
    struct net_device *dev;
    
    read_lock(&dev_base_lock);
	
    FOR_EACH_NETDEV(dev)
    {
        if ( strcmp(dev->name, "lo") == 0 ) 
            continue;
        ndas_unregister_network_interface(dev->name);
    }    
    read_unlock(&dev_base_lock);
    return NDAS_OK;
}

static ndas_error_t register_network(void) {
    int c = 0;
    ndas_error_t err;
    struct net_device *dev;
    
    read_lock(&dev_base_lock);
	
    FOR_EACH_NETDEV(dev)
    {
        if ( strcmp(dev->name, "lo") == 0 ) 
            continue;
		if ( ndas_dev != NULL && ndas_dev[0]!=0 && strcmp(dev->name, ndas_dev) != 0 )
			continue;            
        printk("ndas: registering network interface %s\n", dev->name);
        err = ndas_register_network_interface(dev->name);
        if ( err == NDAS_ERROR_OUT_OF_MEMORY) {
            goto out;
        } if ( !NDAS_SUCCESS(err) ) {
            printk("ndas: fail to register network interface %s (%d): ignored\n", dev->name, err);
        }
        c++;
    }
    read_unlock(&dev_base_lock);
    if ( c == 0 ) { 
        return NDAS_ERROR_NO_DEVICE;
    }
    return NDAS_OK;
out:
    unregister_network();
    return err;
}

ndas_error_t ndas_change_handler_func(sal_net_change_event event, char* devname)
{
    if (event == SAL_NET_EVENT_DOWN) {
        printk("ndas: network down. Unregistering %s\n", devname);
        ndas_unregister_network_interface(devname);
        ndas_restart();
        return NDAS_OK;
    } else if (event == SAL_NET_EVENT_UP) {
        printk("ndas: network up. Registering %s\n", devname);    
        ndas_register_network_interface(devname);
        ndas_restart();
        return NDAS_OK;
    } else {
        printk("ndas: Unknown event %d\n", event);    
    }
    return NDAS_OK;
}

static int ndas_core_init(void) 
{
    ndas_error_t ret;
    ret = ndas_init(ndas_io_unit, ndas_sock_io_unit, ndas_max_jobs, ndas_max_slot);
    if (ret) {
    	printk("ndas: init failed.\n");
    	return -1;
    }
    sal_net_set_change_handler(ndas_change_handler_func);
    
    ret = register_network();
    if (ret) {
        printk("ndas: fail to register network interfaces.\n");
        ndas_cleanup();
        return -1;
    }
    return 0;
}

static void ndas_core_exit(void) 
{    
	unregister_network();
	ndas_cleanup();
}


// 
// NDAS emulator works only when NDAS library is built with NDAS_EMU defined.
//
int ndas_emu_init(void) 
{
	int ret;
	sal_init();
       ret = ndas_core_init();
       if (ret) {
            sal_cleanup();
            return -1;
       }

	printk("ndas: using %s as emulator device.\n", dev);
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
		ndas_core_exit();
		sal_cleanup();
		return -1;
	}
	return 0;
}

void ndas_emu_exit(void) 
{    
	ndas_emu_stop();
	ndas_core_exit();
	sal_cleanup();    
}

module_init(ndas_emu_init);
module_exit(ndas_emu_exit);
