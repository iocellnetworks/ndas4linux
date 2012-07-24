/*
 -------------------------------------------------------------------------
 Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
 All rights reserved.

DEFINITIONS

Software.  "Software" means NDAS core binary as a form of "libndas.a."

Documentation.  "Documentation" means the manual and any other instructional
or descriptive material, printed or on-line, provided with the Software.

License.  "License" means the license to use the Software. The License is
granted pursuant to the provisions of this Agreement.

Customer.  "Customer" means the person or business entity to whom this copy of
the Software is licensed.


LICENSE

Grant of License.  Subject to Customer's compliance with this Agreement,
IOCELL Networks grants to Customer, and Customer purchases, a nonexclusive License to
use the Software for non-commercial use only.  Rights not expressly granted by
this Agreement are reserved by IOCELL Networks.  Customer who wants to use the Software
for commercial use must contact IOCELL Networks to obtain the commercial License.

Customer purchases a License only. IOCELL Networks retains title to and ownership of
the Software and Documentation and any copies thereof, including the originals
provided with this Agreement.

Copies.  Customer may not copy the Software except as necessary to use the
Software.  Such necessary use includes copying the Software to the internal
hard disk, copying the Software to a network file server in order to make the
Software available for use, and copying the Software to archival backup media.
All trademark and copyright notices must be included on any copies made.
Customer may not copy the Documentation.

Transfer and Use.  Customer may not transfer any copy of the Software or
Documentation to any other person or entity unless the transferee first
accepts this Agreement and Customer transfers all copies of the Software and
Documentation, including the originals.

Customer may not rent, loan, lease, sublicense, or otherwise make the Software
or Documentation available for use by any other person except as provided
above.  Customer may not modify, decompile, disassemble, or reverse engineer
the Software or create any derivative works based on the Software or the
Documentation.

Customer may not reverse engineer, decompile, or disassemble the SOFTWARE, nor
attempt in any other manner to obtain the source code or to understand the
protocol.

The Software is protected by national laws and international agreements on
copyright as well by other agreements on intellectual property.

DISCLAIMER

This software is provided 'as is' with no explcit or implied warranties
in respect of any properties, including, but not limited to, correctness 
and fitness for purpose.

GENERAL PROVISIONS

Indemnification.  Customer agrees that Customer shall defend and hold IOCELL Networks
harmless against any liability, claim, or suit and shall pay any related
expense, including but not limited to reasonable attorneys' fees, arising out
of any use of the Software or Content.  IOCELL Networks reserves the right to control
all litigation involving IOCELL Networks, Customer, and third parties.

Entire Agreement.  This Agreement represents the entire agreement between
IOCELL Networks and Customer. No distributor, employee, or other person is authorized
by IOCELL Networks to modify this Agreement or to make any warranty or representation
which is different than, or in addition to, the warranties and representations
of this Agreement.

Governing Law.  This Agreement shall be governed by the laws of the State of
California and of the United States of America.

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
#endif /* NDAS_MSHARE */


#ifdef XPLAT_XIXFS_EVENT
#include <xixfsevent/xixfs_event.h>
#endif //#ifdef XPLAT_XIXFS_EVENT

#include "ndasdev.h"
#include "linux_ver.h"
#include "sal/sal.h"
#include "sal/net.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
MODULE_LICENSE("Proprietary, Send bug reports to linux@iocellnetworks.com");
#endif
MODULE_AUTHOR("IOCELL Networks");

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

int ndas_core_init(void) 
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

void ndas_core_exit(void) 
{    
	unregister_network();
    ndas_cleanup();
}

module_init(ndas_core_init);
module_exit(ndas_core_exit);

EXPORT_SYMBOL(ndas_io_unit);
EXPORT_SYMBOL(ndas_cleanup);
EXPORT_SYMBOL(ndas_disable_slot);
EXPORT_SYMBOL(ndas_enable_slot);
EXPORT_SYMBOL(ndas_enable_exclusive_writable);
EXPORT_SYMBOL(ndas_enable_writeshare);
EXPORT_SYMBOL(ndas_registered_size);
EXPORT_SYMBOL(ndas_registered_list);
EXPORT_SYMBOL(ndas_get_registration_data);
EXPORT_SYMBOL(ndas_get_string_error);
EXPORT_SYMBOL(ndas_init);
EXPORT_SYMBOL(ndas_probed_size);
EXPORT_SYMBOL(ndas_probed_list);
EXPORT_SYMBOL(ndas_get_ndas_dev_info);
EXPORT_SYMBOL(ndas_query_slot);
EXPORT_SYMBOL(ndas_query_unit);
EXPORT_SYMBOL(ndas_query_raid);
EXPORT_SYMBOL(ndas_queue_task);
EXPORT_SYMBOL(ndas_detect_device);
EXPORT_SYMBOL(ndas_read);
EXPORT_SYMBOL(ndas_flush);
EXPORT_SYMBOL(ndas_packetcmd);
EXPORT_SYMBOL(ndas_register_device);
EXPORT_SYMBOL(ndas_register_device_by_serial);
EXPORT_SYMBOL(ndas_query_ndas_id_by_serial);
EXPORT_SYMBOL(ndas_get_version);
EXPORT_SYMBOL(ndas_register_network_interface);
EXPORT_SYMBOL(ndas_request_permission);
EXPORT_SYMBOL(ndas_set_registration_data);
EXPORT_SYMBOL(ndas_set_registration_name);
EXPORT_SYMBOL(ndas_set_encryption_mode);
EXPORT_SYMBOL(ndas_set_device_change_handler);
EXPORT_SYMBOL(ndas_set_slot_handlers);
EXPORT_SYMBOL(ndas_start);
EXPORT_SYMBOL(ndas_restart);
EXPORT_SYMBOL(ndas_stop);
EXPORT_SYMBOL(ndas_unregister_device);
EXPORT_SYMBOL(ndas_unregister_network_interface);
EXPORT_SYMBOL(ndas_write);
EXPORT_SYMBOL(ndas_emu_start);
EXPORT_SYMBOL(ndas_emu_stop);
#ifdef NDAS_MSHARE
EXPORT_SYMBOL(ndas_deallocate_part_id);
EXPORT_SYMBOL(ndas_allocate_part_id);
EXPORT_SYMBOL(ndas_TranslateAddr);
EXPORT_SYMBOL(ndas_IsDiscSet);
EXPORT_SYMBOL(ndas_GetDiscStatus);
EXPORT_SYMBOL(ndas_freeDisc);
EXPORT_SYMBOL(ndas_getkey);
EXPORT_SYMBOL(ndas_encrypt);
EXPORT_SYMBOL(ndas_decrypt);
EXPORT_SYMBOL(ndas_ATAPIRequest);
EXPORT_SYMBOL(ndas_BurnStartCurrentDisc); 
EXPORT_SYMBOL(ndas_BurnEndCurrentDisc);
EXPORT_SYMBOL(ndas_CheckDiscValidity);
EXPORT_SYMBOL(ndas_DeleteCurrentDisc);
EXPORT_SYMBOL(ndas_GetCurrentDiscInfo);
EXPORT_SYMBOL(ndas_DISK_FORMAT);
EXPORT_SYMBOL(ndas_ValidateDisc);
EXPORT_SYMBOL(ndas_CheckFormat);
EXPORT_SYMBOL(ndas_GetCurrentDiskInfo);
#endif //#ifdef NDAS_MSHARE
#ifdef XPLAT_XIXFS_EVENT
EXPORT_SYMBOL(xixfs_Event_Send);
EXPORT_SYMBOL(xixfs_Event_clenup);
EXPORT_SYMBOL(xixfs_Event_init);
EXPORT_SYMBOL(xixfs_IsLocalAddress);
EXPORT_SYMBOL(ndas_lock_operation);
#endif //#ifdef XPLAT_XIXFS_EVENT
