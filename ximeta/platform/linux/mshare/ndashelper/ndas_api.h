/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2005, XIMETA, Inc., IRVINE, CA, USA.
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
#ifndef _LINUX_ND_API_H
#define _LINUX_ND_API_H
#include "../../tarball-tag/inc/ndasdev.h"

#define MAX_ADDR_STRING_LEN 255
#define PASSWORD_STR_SIZE 9

#ifdef DEBUG
#if defined(DEBUG_LEVEL) && !defined(DEBUG_LEVEL_NDADMIN) 
#define DEBUG_LEVEL_NDADMIN DEBUG_LEVEL
#else
#define DEBUG_LEVEL_NDADMIN 10
#endif
#define    debug_ndapi(l, x...) do {\
    if(l <= DEBUG_LEVEL_NDADMIN) {\
        fprintf(stderr,"NDA|%d|%s|", l,__FUNCTION__); \
        fprintf(stderr,x); \
        fprintf(stderr,"|%s:%d\n",__FILE__,__LINE__);\
    }\
} while(0)
#else
#define    debug_ndapi(l, x...) do {} while(0)
#endif


#define EM_DISC_STATUS_ERASING			0x00000001
#define EM_DISC_STATUS_WRITING			0x00000002
#define EM_DISC_STATUS_VALID			0x00000004
#define EM_DISC_STATUS_VALID_END		0x00000008

int get_slot_number(char *Name,  int *slot);
#ifdef NDAS_MSHARE
int do_ndc_diskinfo(ndas_ioctl_juke_disk_info_ptr arg);
int do_ndc_discinfo(ndas_ioctl_juke_disc_info_ptr arg);
int do_ndc_seldisc(ndas_ioctl_juke_read_start_ptr arg);
int do_ndc_deseldisc(ndas_ioctl_juke_read_end_ptr arg);
#endif
#ifdef NDAS_MSHARE_WRITE
int do_ndc_rcopy(int slotnumber, int discnumber, char *disc_path);
int do_ndc_del(ndas_ioctl_juke_del_ptr arg);
int do_ndc_validate(int slotnumber, int discnumber, char *dvd_path);
int do_ndc_format(ndas_ioctl_juke_format_ptr arg);
#endif //#ifdef NDAS_MSHARE_WRITE

int do_ndc_start() ;
int do_ndc_stop() ;
int do_ndc_register(ndas_ioctl_register_ptr arg) ;
int do_ndc_unregister(ndas_ioctl_unregister_ptr arg);
int do_ndc_enable(ndas_ioctl_enable_ptr arg);
int do_ndc_disable(ndas_ioctl_arg_disable_ptr arg);
int do_ndc_edit( ndas_ioctl_edit_ptr arg);
#ifdef NDAS_WRITE

int do_ndc_request(ndas_ioctl_request_ptr arg);
#endif //#ifdef NDAS_WRITE
int do_ndc_probe(ndas_ioctl_probe_ptr arg) ;


#endif //_LINUX_ND_API_H
