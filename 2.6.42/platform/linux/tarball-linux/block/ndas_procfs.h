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
#ifndef _NDAS_PROCFS_H_
#define _NDAS_PROCFS_H_

#include "ndas_sdev.h"
#include "ndas_ndev.h"

struct ndev_node {

    char name[NDAS_MAX_NAME_LENGTH];

    ndas_device_info info;

    struct proc_dir_entry *proc_dir;
    struct proc_dir_entry *proc_nr_unit;
    struct proc_dir_entry *proc_slots;
    struct proc_dir_entry *proc_name;
    struct proc_dir_entry *proc_id;

    struct list_head proc_node;   
};

ndas_error_t init_ndasproc(void);
void cleanup_ndasproc(void);

ndas_error_t nproc_add_ndev(const char *name);
void nproc_remove_ndev(const char *name);

ndas_error_t nproc_add_slot(sdev_t *slot);
void nproc_remove_slot(sdev_t *slot);

#endif // _NDAS_PROCFS_H_

