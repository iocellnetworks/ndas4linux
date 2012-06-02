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
/*
    Support compatability between Linux Versions
*/

#ifndef _SAL_LINUX_VER_H_
#define _SAL_LINUX_VER_H_

#include <linux/version.h>
#include <linux/module.h> // module_put/get for 2.6, MOD_INC/DEC_USE_COUNT for 2.4
#include <linux/slab.h>
#include "sal/libc.h"

#define LINUX_VERSION_25_ABOVE (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
#define LINUX_VERSION_23_ABOVE (LINUX_VERSION_CODE >= KERNEL_VERSION(2,3,0))
#define LINUX_VERSION_NO_SNPRINTF (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,7))
#define LINUX_VERSION_NO_LIST_FOR_EACH_SAFE (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,9))
#define LINUX_VERSION_NO_INVALIDATE_DEVICE (LINUX_VERSION_CODE < KERNEL_VERSION(2,4,5))
#define LINUX_VERSION_HAS_CLASS_CREATE  (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,13))
#define LINUX_VERSION_HAS_DEVICE_CREATE  (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
#define LINUX_VERSION_NEW_SEMAPHORE  (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26))
/*
    As of 2.6.22, sk_buff uses sk_buff_data_t type and provides functions to access each network layer header.
*/
#define LINUX_VERSION_USE_SK_BUFF_DATA_T    (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22))

/*
 * It was reported that the NDAS driver doesn't work well with CFQ i/o elevation scheduler of 2.6.13 or above kernels.
 * To make sure conservatively, I change the schedule from 2.6.12 kernels(not 2.6.13) until I have the exact information of
 * which version of the kernels it has the problem with.
 * URL: http://ndas4linux.iocellnetworks.com/cgi-bin/trac.cgi/ticket/20
 */
#define LINUX_VERSION_AVOID_CFQ_SCHEDULER (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12))
/*
 * SuSE 9.0 2.4.21-99 kernel has non constant value for HZ
 */
#if (LINUX_VERSION_CODE == KERNEL_VERSION(2,4,21)) 
#define LINUX_VERSION_NON_CONSTANT_HZ ((CONFIG_RELEASE) == 99)
#else
#define LINUX_VERSION_NON_CONSTANT_HZ (0)
#endif
/*
 * end_that_request_last function signature changed from 2.6.16.
 * Note that the early fedora core 5 kernels is derived from 2.6.16, but the version macro indicate 2.6.15.
 *  so you should specify NDAS_KERNEL_2_6_16 in such a case.
 */ 
#define LINUX_VERSION_END_THAT_REQUEST_LAST_WITH_UPTODATES \
	((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,16)) || defined(NDAS_KERNEL_2_6_16))

/*
 * flags in struct request in linux/blkdev.h of 2.6.19 changed as cmd_flags
 */
#define LINUX_VERSION_REQUEST_CMD_FLAGS \
	((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)) || defined(NDAS_KERNEL_2_6_19))
#if LINUX_VERSION_REQUEST_CMD_FLAGS
#define REQUEST_FLAGS(req) ((req)->cmd_flags)
#else
#define REQUEST_FLAGS(req) ((req)->flags)
#endif

/* utsname is not available in 2.6.27 */
#define LINUX_NO_UTSNAME (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27))
#if !LINUX_NO_UTSNAME
#define LINUX_VERSION_NEW_UTSNAME \
	((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)) || defined(NDAS_KERNEL_2_6_19))
#if LINUX_VERSION_NEW_UTSNAME
#define GET_UTS_NODENAME() (utsname()->nodename)
#else
#define GET_UTS_NODENAME() (system_utsname.nodename)
#endif
#endif
/*
 * From 2.6.12, sk_alloc signature changed from
 *
 *  struct sock	*sk_alloc(int family, gfp_t priority, int zero_it, kmem_cache_t *slab);
 *
 * into
 *
 *  struct sock	*sk_alloc(int family, gfp_t priority,struct proto *prot, int zero_it);
 * 
 * Kernel for Fedora Core, use the new signature from 2.6.11 kernels.
 */
#define LINUX_VERSION_SK_ALLOC_NEED_PROTO \
    ( \
     (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,12)) || \
     ((LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,11)) && defined(NDAS_REDHAT))\
    )
/**
 * reparent_to_init
 */
#define LINUX_VERSION_HAS_REPARENT_TO_INIT (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))    

#define LINUX_VERSION_HAS_UNLIKELY (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
/* 
 * If the kernel version is 2.6.17 below but sources are from 2.6.18,
 * build the driver with 
 * export NDAS_EXTRA_CFLAGS="-DLINUX_VERSION_DEFS_REMOVED_COMPLETELY"
 */
#ifndef LINUX_VERSION_DEVFS_REMOVED_COMPLETELY
#define LINUX_VERSION_DEVFS_REMOVED_COMPLETELY (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18))
#endif

/* 
 * kmem_cache_t is deprecated as of 2.6.20.
 * If the kernel version is 2.6.19 or below but sources are from 2.6.20 tree (redhat does this often),
 * build the driver with
 * export NDAS_EXTRA_CFLAGS="-DLINUX_VERSION_KMEM_CACHE_T_DEPRECATED"
 */
#ifndef LINUX_VERSION_KMEM_CACHE_T_DEPRECATED
#define LINUX_VERSION_KMEM_CACHE_T_DEPRECATED (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
#endif

#if LINUX_VERSION_KMEM_CACHE_T_DEPRECATED
typedef struct kmem_cache KMEM_CACHE;
#else
typedef kmem_cache_t KMEM_CACHE;
#endif

/* 
 * SLAB_ATOMIC is removed as of 2.6.20.
 * If the kernel version is 2.6.19 or below but sources are from 2.6.20 tree (redhat does this often),
 * build the driver with
 * export NDAS_EXTRA_CFLAGS="-DLINUX_VERSION_SLAB_ATOMIC_REMOVED"
 */
#ifndef LINUX_VERSION_SLAB_ATOMIC_REMOVED
#define LINUX_VERSION_SLAB_ATOMIC_REMOVED (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
#endif

#if LINUX_VERSION_SLAB_ATOMIC_REMOVED
#define KMEM_ATOMIC GFP_ATOMIC
#else
#define KMEM_ATOMIC SLAB_ATOMIC
#endif

#if !LINUX_VERSION_HAS_UNLIKELY
#if __GNUC__ < 3 && __GNUC_MINOR__ < 96
#define __builtin_expect(x, expected_condition) (x)
#endif
#define unlikely(x)     __builtin_expect((x),0)
#endif

#if LINUX_VERSION_NO_INVALIDATE_DEVICE
/* from compatmac.h of linux 2.4.30 */
#define invalidate_device(dev, do_sync) ({ \
        struct super_block *sb = get_super(dev);\
        int res = 0;\
        if (do_sync) fsync_dev(dev); \
        if (sb) res = invalidate_inodes(sb); \
        invalidate_buffers(dev); \
        return res; \
})
#endif

#if LINUX_VERSION_NO_LIST_FOR_EACH_SAFE
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)
#endif        

        
#if defined(NDAS_REDHAT) && (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,5))
#define SOCK_CREATE(family, type, protocol, res) \
    sock_create(family, type, protocol, res, 1)
#elif (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,5)) 
#define SOCK_CREATE(family, type, protocol, res) \
    sock_create_kern(family, type, protocol, res)
#else 
#define SOCK_CREATE(family, type, protocol, res) \
    sock_create(family, type, protocol, res)
#endif

#if LINUX_VERSION_25_ABOVE 
#define GENDISK_MAX_DISKNAME sizeof(((struct gendisk*)0)->disk_name)
#define GENDISK_MAX_DDEVFSNAME sizeof(((struct gendisk*)0)->devfs_name)

#include <linux/moduleparam.h>
#define NDAS_MODULE_PARAM_INT(var, perm) module_param(var, int, perm)
#define NDAS_MODULE_PARAM_STRING(var, perm) module_param(var, charp, perm)
#define NDAS_MODULE_PARAM_DESC(var, desc)

/* for members of struct sock*/
#define SK_SOCKET sk_socket
#define SK_ALLOCATION sk_allocation
#define SK_DESTRUCT sk_destruct
#define SK_BACKLOG_RCV sk_backlog_rcv
#define SK_USER_DATA sk_user_data

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
#define DEV_GET_BY_NAME(DEV_NAME) dev_get_by_name(&init_net, DEV_NAME)
#else
#define DEV_GET_BY_NAME(DEV_NAME) dev_get_by_name(DEV_NAME)
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24))
#define FOR_EACH_NETDEV(DEV) for_each_netdev(&init_net, (DEV))
#elif defined(for_each_netdev)
#define FOR_EACH_NETDEV(DEV) for_each_netdev(DEV)
#else
#define FOR_EACH_NETDEV(DEV) for ((DEV)=dev_base; (DEV); (DEV)=(DEV)->next) 
#endif

/* For 2.6, this is done by kernel internal in most case */
#ifdef MOD_INC_USE_COUNT
#undef MOD_INC_USE_COUNT
#endif
#ifdef MOD_DEC_USE_COUNT
#undef MOD_DEC_USE_COUNT
#endif
#define MOD_INC_USE_COUNT do {} while(0)
#define MOD_DEC_USE_COUNT do {} while(0)

#define REQ_DIR(req) rq_data_dir(req)
#define QUEUE_IS_RUNNING(q) (!blk_queue_plugged(q))
#define GET_NEXT_QUEUE(q) elv_next_request(q)

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39) )
#define NBLK_NEXT_REQUEST(q)  (\
    blk_peek_request(q))
    //blk_queue_plugged is no more in 2.6.39. I hope it will
    //simply send back mo request when this is called, thus
    //skipping the function. see block/block26.c line 480.
    //david 
    
#elif ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31) )
#define NBLK_NEXT_REQUEST(q) (\
    blk_queue_plugged(q) ? NULL : \
    blk_peek_request(q))
#else
#define NBLK_NEXT_REQUEST(q) (\
    blk_queue_plugged(q) ? NULL : \
    elv_next_request(q))
#endif

#if ( LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36) )
	#define BLK_CHECK_VALID_STATUS(req) ( (req)->cmd_type == REQ_TYPE_FS )
#else
	#define BLK_CHECK_VALID_STATUS(req) blk_fs_request(req)
#endif

#define BLK_ATTEMPT_WRITE_RDONLY_DEVICE(req) \
    (NDAS_GET_SLOT_DEV(SLOT_R(req)) && \
    !NDAS_GET_SLOT_DEV(SLOT_R(req))->info.writable && \
    REQ_DIR(req) == WRITE )
    
#define MOD_COUNT module_refcount(THIS_MODULE)
#if LINUX_VERSION_END_THAT_REQUEST_LAST_WITH_UPTODATES
#define END_THAT_REQUEST_LAST(req, uptodates) end_that_request_last(req,uptodates)
#else
#define END_THAT_REQUEST_LAST(req, uptodates) end_that_request_last(req)
#endif
#else // 2.4.x or below

#define NDAS_MODULE_PARAM_INT(var, perm) MODULE_PARM(var, "i")
#define NDAS_MODULE_PARAM_STRING(var, perm) MODULE_PARM(var, "s")
#define NDAS_MODULE_PARAM_DESC(var, desc) MODULE_PARM_DESC(var, desc)

/* for members of struct sock*/
#define SK_SOCKET socket
#define SK_ALLOCATION allocation
#define SK_DESTRUCT destruct
#define SK_BACKLOG_RCV backlog_rcv
#define SK_USER_DATA user_data
/* functions */
#define DEV_GET_BY_NAME(DEV_NAME) dev_get_by_name(DEV_NAME)
#if defined(for_each_netdev)
#define FOR_EACH_NETDEV(DEV) for_each_netdev(DEV)
#else
#define FOR_EACH_NETDEV(DEV) for ((DEV)=dev_base; (DEV); (DEV)=(DEV)->next) 
#endif
#define SK_ALLOC(family, priority, zero_it, proto) \
    sk_alloc(family, priority, zero_it)
    
/* 
   For 2.6, NDAS device enable/disable should call this explicitly
   to prevent the block driver rmmod with an enabled disk
*/
#define module_put(x) do {} while(0)
/* always true */
#define try_module_get(x) ({ 1;})

#define REQ_DIR(req) ((req)->cmd)
#define QUEUE_IS_RUNNING(q) (!list_empty(&(q)->queue_header))
#define GET_NEXT_QUEUE(q) blkdev_entry_next_request(&(q)->queue_head)

#define NBLK_NEXT_REQUEST(q) (\
    list_empty(&(q)->queue_head) ? NULL : \
    blkdev_entry_next_request(&(q)->queue_head))
    
#define BLK_ATTEMPT_WRITE_RDONLY_DEVICE(req) \
    (NDAS_GET_SLOT_DEV(SLOT_R(req) && \
    !NDAS_GET_SLOT_DEV(SLOT_R(req)->info.writable && \
    REQ_DIR(req) == WRITE )

#define BLK_CHECK_VALID_STATUS(req) ((req)->rq_status != RQ_INACTIVE)

#define MOD_COUNT atomic_read(&(THIS_MODULE)->uc.usecount)

#if LINUX_VERSION_NO_SNPRINTF
#define snprintf(x,n,y...) sprintf(x,y)
#define vsnprintf(x,n,y...) vsprintf(x,y)
#endif

#endif // end of linux kernel version

#endif // _SAL_LINUX_VER_H_

