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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#include <linux/version.h>
#include <linux/module.h> 

#include <linux/delay.h>
#include <asm/uaccess.h> // VERIFY_WRITE

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include <ndas_dev.h>

#ifdef DEBUG

int	dbg_level_cdev = DBG_LEVEL_CDEV;

#define dbg_call(l,x...) do { 								\
    if (l <= dbg_level_cdev) {								\
        printk("CD|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif

#include "ndas_request.h"
#include "ndas_id.h"

#include "ndas_cdev.h"
#include "ndas_sdev.h"
#include "ndas_bdev.h"
#include "ndas_regist.h"
#include "ndas_pnp.h"
#include "ndas_ndev.h"
#include "ndas_procfs.h"
#include "nhix.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,20))
#else
#include "linux/compiler.h"
#endif

#ifdef NDAS_MSHARE 
#include <ndasuser/mediaop.h>
#endif

#ifdef NDAS_DEVFS
#ifndef CONFIG_DEVFS_FS
#error This kernel config does not support devfs.
#else
#include <linux/devfs_fs_kernel.h>
static devfs_handle_t devfs_control_handle = NULL;
#endif
#endif

#ifdef DEBUG
#undef static
#define static
#else
#undef static
#define static static
#endif

extern int ndas_io_unit;

#ifndef __user
#define __user
#endif


static ndas_error_t cdev_cmd_request_permission(int slot) 
{
    return ndev_request_permission(slot);
}

static inline ndas_error_t cdev_cmd_probe(char *user_array, int sz, int *count, int convert_to_id)
{
    ndas_error_t ret = NDAS_OK;
    struct ndas_probed *data;
    int retry = 0;

    dbg_call( 3,"ing sz=%d\n", sz );

    do {

        int i, c;

        c = ndev_probe_size();

        if (!NDAS_SUCCESS(c)) {

            return c;
        }

        if (user_array == NULL) {/* just return the count */

            (*count) = c;
            return NDAS_OK;
        }

        if (sz != c) {

            return NDAS_ERROR_INVALID_PARAMETER;
		}

        dbg_call( 3, "c=%d\n", c );
        
        data = ndas_kmalloc(sizeof(struct ndas_probed) * c);

        if (!data) {

            ret = NDAS_ERROR_OUT_OF_MEMORY;
            break;
        }

        // ndas_probed_list() returns the count.

        ret = ndev_probe_ndas_id(data, c);

        if (ret == NDAS_ERROR_INVALID_PARAMETER) {

            if (retry++ > 2) {

                break;
            }

            ndas_kfree(data);
            continue;
        }

        if (ret < 0) {

            break;
        }

        // Reset the return value.

        ret = NDAS_OK;

        if (convert_to_id) {

	        if (!access_ok(VERIFY_WRITE, user_array, (NDAS_ID_LENGTH+1) * c)) {

	            ret = NDAS_ERROR_INTERNAL;
	            break;
	        }

        } else { 

	        if (!access_ok(VERIFY_WRITE, user_array, (NDAS_SERIAL_LENGTH+1) * c)) {

	            ret = NDAS_ERROR_INTERNAL;
	            break;
	        }
	    }

        for ( i = 0; i < c; i++ ) {

            int result;

            dbg_call( 5, "ndas_id=%s\n", data[i].ndas_id );
            dbg_call( 5, "ndas_sid=%s\n", data[i].ndas_sid );

            if (convert_to_id) {

                if (ret == NDAS_OK) {

                    result = copy_to_user(user_array + (NDAS_ID_LENGTH+1)*i, data[i].ndas_id, NDAS_ID_LENGTH+1);

                    if (result != 0) {

                        ret = NDAS_ERROR_INTERNAL; 
                        (*count) = 0;
                        break;
                    }

                } else {

                    // ndas_query_ndas_id_by_serial is not supported

                    (*count) = 0;
                    break;
                }

            } else {

                result = copy_to_user(user_array + (NDAS_SERIAL_LENGTH+1)*i, data[i].ndas_sid, NDAS_SERIAL_LENGTH+1);

                if (result != 0) { 

                    ret = NDAS_ERROR_INTERNAL; 
                    (*count) = 0;
                    break;
                }
            }
        }

        if (NDAS_SUCCESS(ret)) {

            (*count)=c;
        }
        break;

    } while(1);

    ndas_kfree(data);

    dbg_call( 3,"ed\n" );

    return ret;
}

static 
ndas_error_t 
cdev_cmd_enable(int slot, int write_mode)
{
    ndas_error_t err;
    sdev_t 		 *sdev = sdev_lookup_byslot(slot);


    dbg_call( 1, "ing slot=%d %s\n", slot, write_mode ? "read-write": "read-only" );

#if defined(_MIPSEL) || defined(_MIPS) 

    // disable the encryption for mips devices: temporal

	err = sdev_set_encryption_mode( slot, 0, 0 );

    if (!NDAS_SUCCESS(err)) {

		NDAS_BUG_ON(true);
    	dbg_call( 1, "failed to set encryption mode\n" );
		return err;
	}

#endif

    if ( sdev == NULL ) {

        dbg_call( 1, "no slot device\n" );
        return NDAS_ERROR_INVALID_SLOT_NUMBER;
    }

    down( &sdev->sdev_mutex );

    switch (write_mode) {

    case 2:

        printk( "ndas: Enabling slot %d in write-share mode\n", slot );
        err = sdev_cmd_enable_writeshare(slot);

        break;

    case 1:

        printk( "ndas: Enabling slot %d in exclusive-write mode\n", slot );
        err = sdev_cmd_enable_exclusive_writable(slot);

        break;

    case 0:

        printk( "ndas: Enabling slot %d in read-only mode\n", slot );
        err = sdev_cmd_enable_slot(slot);

        break;

    default:

        dbg_call( 1, "Unknown write_mode: %d, enabling with read-only\n", write_mode );
        err = sdev_cmd_enable_slot(slot);

        break;
    }

    up( &sdev->sdev_mutex );

    dbg_call( 1, "ed err = %d\n", err );

    return err;
}

static ndas_error_t cdev_cmd_disable(int slot)
{
    ndas_error_t err;
    sdev_t 		 *sdev = sdev_lookup_byslot(slot);

    down( &sdev->sdev_mutex );
    err = sdev_disable_slot(slot);
    up( &sdev->sdev_mutex );

    return err;
}

static ndas_error_t cdev_cmd_register(char *name, char *id, char *ndas_key) 
{
    ndas_error_t err;


    dbg_call( 1, "name=%s\n", name );

    err = ndev_register_device( name, id, ndas_key, 0, FALSE, NULL, 0 );

    if (!NDAS_SUCCESS(err)) {
 
        return err;
	}

    nproc_add_ndev(name);

    return NDAS_OK;
}

static ndas_error_t cdev_cmd_register_by_serial(char *name, char *serial) 
{
    ndas_error_t err;


    dbg_call( 1, "name=%s\n", name );

    err = ndev_register_device( name, serial, NULL, 0, TRUE, NULL, 0 );

    if (!NDAS_SUCCESS(err)) {

        return err;
	}

    nproc_add_ndev(name);

    return NDAS_OK;
}

static ndas_error_t cdev_cmd_unregister(char *name) 
{
    ndas_error_t err;
    //int i;
    ndas_device_info info;


    dbg_call( 1, "name=%s\n", name );

    err = ndev_get_ndas_dev_info(name, &info); 

    if (!NDAS_SUCCESS(err)) {

        return err;
    }

    err = ndev_unregister_device(name);

    if (!NDAS_SUCCESS(err)) {

		NDAS_BUG_ON( err != NDAS_ERROR_ALREADY_ENABLED_DEVICE );
		
        return err;
    }

    nproc_remove_ndev(name);

    return err;
}

static bool started = false;

static ndas_error_t cdev_cmd_start(void) 
{
    ndas_error_t err;

    if (started == true) {

        return NDAS_ERROR_ALREADY_STARTED;
    }

    err = npnp_init();

    if (!NDAS_SUCCESS(err)) {

        return err;
	}

    err = nhix_init();

    if (!NDAS_SUCCESS(err)) {

        npnp_cleanup();
        return err;
    }

    //npnp_set_device_change_handler(cdev_cmd_dev_changed);

    started = 1;

    return err;
}

static ndas_error_t cdev_cmd_stop(void) 
{
    ndas_error_t ret;

    int i, size, slot;
    ndas_device_info info;
    struct ndas_registered *data = NULL;

	if (started == false) {

		return NDAS_ERROR_ALREADY_STOPPED;
	}

    started = 0;

    //npnp_set_device_change_handler(NULL);

    size = ndev_registered_size();

    if (!NDAS_SUCCESS(size)) {

		NDAS_BUG_ON(true);
        dbg_call( 1, "fail to get ndas_registered_size\n" );

        goto out;
    }

    dbg_call( 1,"ndas_registered_size=%d\n", size );

	if (size != 0) {

    	data = ndas_kmalloc(sizeof(struct ndas_registered)* size);

	    if (!data) {

			NDAS_BUG_ON(true);
    	    goto out;
    	}

    	for ( i = NDAS_FIRST_SLOT_NR; i < NDAS_MAX_SLOT; i++) {

        	sdev_t* sd = sdev_lookup_byslot(i); 

        	if (!sd) {

	            continue;
    	    }
        
			slot = sd->info.slot;
	        //slot_destroy(slot);
    	    sdev_disable_slot(slot);
    	}

    	ret = ndev_registered_list(data, size);
    
		if (!NDAS_SUCCESS(ret)) {
        	
			goto out_free;
    	}
    
    	if ( ret != size ) {
        
			goto out_free;
    	}

	    for ( i = 0; i < size; i ++ ) {

	        dbg_call( 1, "name[%d]=%s\n", i , data[i].name );

		    ret = ndev_get_ndas_dev_info(data[i].name, &info);

			if (!NDAS_SUCCESS(ret)) {

	            continue;
			}

	        nproc_remove_ndev(data[i].name);
    	    ndev_unregister_device(data[i].name);
    	}
	}

out_free:
    ndas_kfree(data);
out:

    nhix_cleanup();
    npnp_cleanup();
    registrar_stop();

    return NDAS_OK;
}

static int cdev_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    unsigned int        result = 0;

    dbg_call( 3, "ing inode=%p, filp=%p, cmd=0x%x, arg=%p\n", inode, filp, cmd, (void*) arg );

    if(!capable(CAP_SYS_ADMIN))
        return -EPERM;

    if(!inode) {
        printk("ndas: open called with inode = NULL\n");
        return -EIO;
    }
    if (iminor(inode) != NDAS_CHR_DEV_MINOR)  {
        return -EINVAL;
    }
                
    switch(cmd) {
    case IOCTL_NDCMD_START:
    {
        result = cdev_cmd_start();
    }
    break;
    case IOCTL_NDCMD_STOP:
    {
        result = cdev_cmd_stop();
    }
    break;
    case IOCTL_NDCMD_REGISTER: 
    {
        ndas_error_t err;
        ndas_ioctl_register_ptr rarg = (ndas_ioctl_register_ptr) arg;
        
        char name[NDAS_MAX_NAME_LENGTH];
        char idserial[NDAS_ID_LENGTH+1];
        char key[NDAS_KEY_LENGTH+1];
        char use_serial;
        char discover;

        dbg_call( 3, "IOCTL_NDCMD_REGISTER\n" );

        result = copy_from_user(name,rarg->name,NDAS_MAX_NAME_LENGTH);
        if ( result != 0) { return -EINVAL; }
        result = copy_from_user(idserial, rarg->ndas_idserial, NDAS_ID_LENGTH + 1);
        if ( result != 0) { return -EINVAL; }
        result = copy_from_user(key, rarg->ndas_key, NDAS_KEY_LENGTH + 1);
        if ( result != 0) { return -EINVAL; }
        result = copy_from_user(&use_serial, &rarg->use_serial, sizeof(rarg->use_serial));
        if ( result != 0) { return -EINVAL; }
        result = copy_from_user(&discover, &rarg->discover, sizeof(rarg->discover));
        if ( result != 0) { return -EINVAL; }
        
        if (use_serial) {
            dbg_call( 1,"Registering by serial:%s\n", idserial );
            err = cdev_cmd_register_by_serial(name, idserial);
        } else {
            dbg_call( 1,"Registering by id:%s, key: %s\n", idserial, key );
            err = cdev_cmd_register(name,idserial,key);
        }
        dbg_call( 1,"idserial=%s, err=%d\n", idserial, err );
        put_user(err, &rarg->error);
        if ( !NDAS_SUCCESS(err) ) {
            result = -EINVAL;
            break;
        } else {
            if (discover) {
                ndev_detect_device(name);
            }
        }
        result = 0;
    }
    break;
    case IOCTL_NDCMD_UNREGISTER: // unregister lanscsi controller
    {
        char name[NDAS_MAX_NAME_LENGTH];
        ndas_error_t err;
        ndas_ioctl_unregister_ptr rarg = (ndas_ioctl_unregister_ptr) arg;
        dbg_call( 3, "IOCTL_NDCMD_UNREGISTER" );

        result = copy_from_user(name,rarg->name,NDAS_MAX_NAME_LENGTH);
        if ( result != 0 ) { return -EINVAL; }
        err = cdev_cmd_unregister(name);
        put_user(err,&rarg->error);
        dbg_call( 3, "IOCTL_NDCMD_UNREGISTER name=%s err=%d\n", name, err );
        result = (err == NDAS_OK) ? 0: -EINVAL;
    }
    break;
    case IOCTL_NDCMD_ENABLE:
    {
        int slot = 0;
        int write_mode = 0;
        ndas_error_t err;
        ndas_ioctl_enable_ptr earg = (ndas_ioctl_enable_ptr) arg;
        get_user(slot,&earg->slot);
        get_user(write_mode,&earg->write_mode);

        err = cdev_cmd_enable(slot,write_mode);
        
        put_user(err,&earg->error);
        if ( err == NDAS_ERROR_INVALID_SLOT_NUMBER ) {
            dbg_call( 3, "ed NDAS_ERROR_INVALID_SLOT_NUMBER\n" );
            result = -ENODEV;
        } else if ( err == NDAS_ERROR_ALREADY_ENABLED_DEVICE ) {
            dbg_call( 3, "ed NDAS_ERROR_ALREADY_ENABLED_DEVICE\n" );
            result = -EINVAL; 
        } else if ( err == NDAS_ERROR_OUT_OF_MEMORY ) {
            dbg_call( 3, "ed NDAS_ERROR_OUT_OF_MEMORY\n" );
            result = -ENOMEM;
        } else if ( err == NDAS_ERROR_TRY_AGAIN ) {
            dbg_call( 3, "ed NDAS_ERROR_TRY_AGAIN\n" );
            result = -EAGAIN;
        } else if ( !NDAS_SUCCESS(err) ) {
            dbg_call( 3, "ed unknown error %d\n", err );
            result = -EINVAL;
        }     
    }
    break;
    case IOCTL_NDCMD_DISABLE:
    {
        int slot = 0;
        ndas_error_t err;
        ndas_ioctl_arg_disable_ptr darg = (ndas_ioctl_arg_disable_ptr) arg;

        get_user(slot,&darg->slot);
        err = cdev_cmd_disable(slot);
        put_user(err,&darg->error);

        if ( err == NDAS_ERROR_INVALID_SLOT_NUMBER ) {
            result = -ENODEV;
        } else if ( err != NDAS_OK ) {
            result = -EINVAL;
        } else 
            result = 0;
    }
    break;

    case IOCTL_NDCMD_PROBE:
    {    /* no consideration for race condition. */
        ndas_error_t err;
        ndas_ioctl_probe_ptr parg = (ndas_ioctl_probe_ptr) arg;
        char *user_array = NULL;
        int user_array_sz = 0;
        int probe_count = 0;
        get_user(user_array, &parg->ndasids);
        get_user(user_array_sz, &parg->sz_array);
        err = cdev_cmd_probe(user_array, user_array_sz, &probe_count, FALSE);
        put_user(err, &parg->error);
        put_user(probe_count, &parg->nr_ndasid);

        result = NDAS_SUCCESS(err) ? 0 : -EINVAL;
        
    }
    break;

    case IOCTL_NDCMD_PROBE_BY_ID:
    {    /* no consideration for race condition. */
        ndas_error_t err;
        ndas_ioctl_probe_ptr parg = (ndas_ioctl_probe_ptr) arg;
        char *user_array = NULL;
        int user_array_sz = 0;
        int probe_count = 0;
        get_user(user_array, &parg->ndasids);
        get_user(user_array_sz, &parg->sz_array);
        err = cdev_cmd_probe(user_array, user_array_sz, &probe_count, TRUE);

        put_user(err, &parg->error);
        put_user(probe_count, &parg->nr_ndasid);

        result = NDAS_SUCCESS(err) ? 0 : -EINVAL;

    }
    break;

    case IOCTL_NDCMD_QUERY:
    {
        ndas_error_t err;
        ndas_device_info info;
        char name[NDAS_MAX_NAME_LENGTH];
        ndas_ioctl_query_ptr parg = (ndas_ioctl_query_ptr) arg;

        result = copy_from_user(name, parg->name, NDAS_MAX_NAME_LENGTH);
        if ( result != 0 ) {
            result = -EINVAL;
            break;
        }
        err = ndev_get_ndas_dev_info(name, &info);
        if ( !NDAS_SUCCESS(err) )
        {
            result = -EINVAL;
            break;
        } 
        result = 0;
        if ( copy_to_user(parg->slots, info.slot, 
                NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE * sizeof(int)) != 0 ) {
            result = -EINVAL;
            break;
        }
        put_user(info.nr_unit, &parg->nr_unit);
    }
    break;
    case IOCTL_NDCMD_EDIT:
    {
        ndas_error_t err;
        char name[NDAS_MAX_NAME_LENGTH];
        char newname[NDAS_MAX_NAME_LENGTH];
        ndas_ioctl_edit_ptr parg = (ndas_ioctl_edit_ptr) arg;

        if ( copy_from_user(name, parg->name, NDAS_MAX_NAME_LENGTH) != 0 ) {
            result = -EINVAL;
            break;
        }
        if ( copy_from_user(newname, parg->newname, NDAS_MAX_NAME_LENGTH) ) {
            result = -EINVAL;
            break;
        }
        err = ndev_set_registration_name(name, newname);
        put_user(err, &parg->error);
        if ( !NDAS_SUCCESS(err) ) {
            result = -EINVAL;
            break;
        } 
        result = 0;
    }
    
    break;
    case IOCTL_NDCMD_REQUEST:
    {
        int slot = 0;
        ndas_error_t err;
        ndas_ioctl_request_ptr rarg = (ndas_ioctl_request_ptr) arg; 

        get_user(slot, &rarg->slot);
        err = cdev_cmd_request_permission(slot);
        put_user(err, &rarg->error);

        if ( err != NDAS_OK ) {
            result = -EINVAL;
        } else
            result = 0;
    }
    break;

    default:
#ifdef NDAS_MSHARE
		// juke_ioctl may handle unhandled ioctls
		result = juke_ioctl(inode, filp, cmd, arg);
#else
		result = -EINVAL;
#endif //#ifdef NDAS_MSHARE

    }

    dbg_call( 3, "ed cmd=%x result=%d\n", cmd, result );
    return result;
}

static ssize_t cdev_write(struct file *filp, const char __user *buf, size_t cnt, loff_t *offp)
{
		NDAS_BUG_ON(true);

        printk( "ndas: can't write to NDAS control device.\n" );
        return -EINVAL;
}

static ssize_t cdev_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
		NDAS_BUG_ON(true);

        printk( "ndas: can't read NDAS control device.\n" );
        return -EINVAL;
}

static int cdev_open(struct inode *inode, struct file *filp)
{
    if (iminor(inode) == NDAS_CHR_DEV_MINOR) {

        return 0;

    } else {

		NDAS_BUG_ON(true);
        return -EINVAL;
    }
}

static int cdev_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations ndasctrl_fops = {

	.write 	 = cdev_write,
	.read  	 = cdev_read,
	.open  	 = cdev_open,
	.release = cdev_release,
	.ioctl 	 = cdev_ioctl, 
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
    .owner =            THIS_MODULE
#endif
};


#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13))
static struct class *cdev_class;
#else
static struct class_simple *cdev_class;
#endif
#endif

int cdev_init(void)
{
	int ret = 0;

#ifdef NDAS_CRYPTO
	NDAS_BUG_ON( ndas_vid != NDAS_VENDOR_ID_WINDOWS_RO );
#endif

    ret = register_chrdev( NDAS_CHR_DEV_MAJOR, "ndas", &ndasctrl_fops );

    if (ret < 0) { 

		NDAS_BUG_ON(true);

        printk( KERN_ERR "ndas: can't register char device\n" );
        return ret;
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13))

    cdev_class = class_create( THIS_MODULE, "ndas" );

    if (IS_ERR(cdev_class)) {

		NDAS_BUG_ON(true);
		
		unregister_chrdev( NDAS_CHR_DEV_MAJOR, "ndas" );
	    return PTR_ERR(cdev_class);
    }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,15))
    class_device_create( cdev_class, NULL,
        				 MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR),
        				 NULL, "ndas" );
#else
    class_device_create( cdev_class,
        				 MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR),
       					 NULL, "ndas");
#endif

#else

    cdev_class = class_simple_create( THIS_MODULE, "ndas" );

    if (IS_ERR(cdev_class)) {

		NDAS_BUG_ON(true);
		
		unregister_chrdev( NDAS_CHR_DEV_MAJOR, "ndas" );
	    return PTR_ERR(cdev_class);
    }

    class_simple_device_add( cdev_class,
        					 MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR),
        					 NULL, "ndas" );

#endif

#endif

    return ret;
}

int cdev_cleanup(void)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13))
    class_device_destroy( cdev_class, MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR) );
    class_destroy(cdev_class);
#else
    class_simple_device_remove( MKDEV(NDAS_CHR_DEV_MAJOR, NDAS_CHR_DEV_MINOR) );
    class_simple_destroy(cdev_class);
#endif

#endif

	unregister_chrdev( NDAS_CHR_DEV_MAJOR, "ndas" );

    return 0;
}
