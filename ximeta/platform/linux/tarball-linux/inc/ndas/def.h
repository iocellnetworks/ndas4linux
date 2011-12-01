/*
 -------------------------------------------------------------------------
 Copyright (c) 2005, XIMETA, Inc., IRVINE, CA, USA.
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

#ifndef _NDAS_DEF_H_
#define _NDAS_DEF_H_

/* SAL API attribute modifier */
#if (__GNUC__ > 3 || __GNUC__ == 3 ) && defined(_X86)
#define NDASUSER_API __attribute__((regparm(3)))
#else
#define NDASUSER_API
#endif

// NDAS LU ( usually a disk ) length
#define NDAS_DISK_MODEL_NAME_LENGTH        	40
#define NDAS_DISK_FMREV_LENGTH            	8
#define NDAS_MAX_DEVICE_SERIAL_LENGTH       20

// NDAS device length

#define NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE  2 	// until next revision of library
#define NDAS_MAX_BIND_COUNT                	8   // Upper count limit for aggregation and mirroring

#define NDAS_MAX_HOSTNAME_LENGTH        	128 //  excluding null termination

#define NDAS_VERSION_UNKNOWN		-1
#define NDAS_VERSION_1_0             0
#define NDAS_VERSION_1_1             1
#define NDAS_VERSION_2_0             2

#define NDAS_UNIT_TYPE_NOT_EXIST    0
#define NDAS_UNIT_TYPE_UNKNOWN      1
#define NDAS_UNIT_TYPE_HARD_DISK    2
#define NDAS_UNIT_TYPE_ATAPI        3

/* To do: change NDAS_DISK to NDAS_SLOT_TYPE_xxx */
#define NDAS_DISK_MODE_UNKNOWN          0
#define NDAS_DISK_MODE_SINGLE           1
#define NDAS_DISK_MODE_RAID0            2
#define NDAS_DISK_MODE_RAID1            3
#define NDAS_DISK_MODE_RAID4            4
#define NDAS_DISK_MODE_AGGREGATION  	20
#define NDAS_DISK_MODE_MIRROR           21
#define NDAS_DISK_MODE_ATAPI            100
#define NDAS_DISK_MODE_MEDIAJUKE		104

#define NDAS_PROBE_REGISTERED        	(1<<1)
#define NDAS_PROBE_UNREGISTERED        	(1<<2)
#define NDAS_PROBE_ALL                	(NDAS_PROBE_REGISTERED | NDAS_PROBE_UNREGISTERED)

#define NDAS_FIRST_SLOT_NR 	1
#define NDAS_INVALID_SLOT 	((NDAS_FIRST_SLOT_NR) -1)

#ifdef __KERNEL__

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0))
#define NDAS_MAX_SLOT        (1<<4)
#else
#define NDAS_MAX_SLOT        (1<<5)
#endif

#endif

#endif //_NDAS_DEF_H_
