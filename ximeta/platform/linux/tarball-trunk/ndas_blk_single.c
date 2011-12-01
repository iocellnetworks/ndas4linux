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
#include <linux/netdevice.h> // dev_base dev_base_lock
#include <ndasuser/ndasuser.h>
#include <ndasuser/persist.h>
#include <ndasuser/io.h>
#include <ndasuser/write.h>
#include <ndasuser/bind.h>

#include "ndasdev.h"
#include "linux_ver.h"
#include "sal/sal.h"
#include "sal/net.h"

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

#ifndef NDAS_IO_UNIT
#define NDAS_IO_UNIT 64
#endif
#ifndef NDAS_SOCK_IO_UNIT
#define NDAS_SOCK_IO_UNIT 32
#endif

int ndas_io_unit = NDAS_IO_UNIT;
int ndas_sock_io_unit = NDAS_SOCK_IO_UNIT;
int ndas_max_slot = NDAS_MAX_SLOT;
int ndas_max_jobs = 350;

NDAS_MODULE_PARAM_INT(ndas_io_unit, 0);
NDAS_MODULE_PARAM_DESC(ndas_io_unit, "The maximum size of input/output transfer unit to a NDAS device (default 64)");

NDAS_MODULE_PARAM_INT(ndas_sock_io_unit, 0);
NDAS_MODULE_PARAM_DESC(ndas_sock_io_unit, "The maximum size of socket transfer buffer to a NDAS device (default 32)");

NDAS_MODULE_PARAM_INT(ndas_max_slot, 0);
NDAS_MODULE_PARAM_DESC(ndas_max_slot, "The maximum number of hard disk can be used by NDAS (default 32)");

char* ndas_dev = NULL;
NDAS_MODULE_PARAM_STRING(ndas_dev, 0);
NDAS_MODULE_PARAM_DESC(ndas_dev, "If set, use only given network interface for NDAS. Set as NULL to use all interfaces.");

NDAS_MODULE_PARAM_INT(ndas_max_jobs, 0);
NDAS_MODULE_PARAM_DESC(ndas_max_jobs, "The maximum number of jobs NDAS can queue inside core module (default 350)");

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


int ndas_block_init(void) 
{    
    int ret;

    sal_init();
    ret = ndas_core_init();
    if (ret) {
        sal_cleanup();
        return -1;
    }

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
    ndas_core_exit();
    sal_cleanup();    
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

