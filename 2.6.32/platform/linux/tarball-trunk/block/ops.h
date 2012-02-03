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
*/
#ifndef _LINUX_BLOCK_OPS_H_
#define _LINUX_BLOCK_OPS_H_

int ndop_open(struct inode *inode, struct file *filp);

int ndop_release(struct inode *inode, struct file *filp);

int ndop_ioctl(struct inode *inode, struct file *filp,
                  unsigned int cmd, unsigned long arg);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)) // 2.5.x or above
int ndop_media_changed(struct gendisk *);

int ndop_revalidate_disk(struct gendisk *);
#else
int ndop_check_netdisk_change(kdev_t rdev);

int ndop_revalidate(kdev_t rdev);
#endif

extern struct block_device_operations ndas_fops;
extern struct ndas_slot* g_ndas_dev[];
#define NDAS_GET_SLOT_DEV(slot) (g_ndas_dev[(slot) - NDAS_FIRST_SLOT_NR])
#define NDAS_SET_SLOT_DEV(slot, dev)   do { g_ndas_dev[slot - NDAS_FIRST_SLOT_NR] = dev; } while(0)

#endif // _LINUX_BLOCK_OPS_H_
