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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#include <linux/version.h>
#include <linux/module.h> 

#define __LPX__		1

#include <linux/mm.h>
#include <linux/sysctl.h>

#ifndef CONFIG_SYSCTL
#error This file should not be compiled without CONFIG_SYSCTL defined
#endif

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#ifdef DEBUG

extern int	dbg_level_lpx;

#define dbg_call(l,x...) do { 								\
    if ( l <= dbg_level_lpx ) {								\
        printk("LP|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
        printk(x); 											\
    } 														\
} while(0)

#define DEBUG_CALL(l,x) do { 								\
    if ( l <= dbg_level_lpx ) {								\
        printk("LP|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
		printk x;											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#define DEBUG_CALL(l,x) do {} while(0)
#endif

#if __LPX__
/* /proc/sys/net/lpx */
enum {

	NET_LPX_PPROP_BROADCASTING=1,
	NET_LPX_FORWARDING=2,
	NET_LPX=152
};
#endif

/* From af_lpx.c */
extern int sysctl_lpx_pprop_broadcasting;

static struct ctl_table lpx_table[] = {
	{
		.ctl_name	= NET_LPX_PPROP_BROADCASTING,
		.procname	= "ipx_pprop_broadcasting",
		.data		= &sysctl_lpx_pprop_broadcasting,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dointvec,
	},
	{ 0 },
};

static struct ctl_table lpx_dir_table[] = {
	{
		.ctl_name	= NET_IPX,
		.procname	= "ipx",
		.mode		= 0555,
		.child		= lpx_table,
	},
	{ 0 },
};

static struct ctl_table lpx_root_table[] = {
	{
		.ctl_name	= CTL_NET,
		.procname	= "net",
		.mode		= 0555,
		.child		= lpx_dir_table,
	},
	{ 0 },
};

static struct ctl_table_header *lpx_table_header;

void lpx_register_sysctl(void)
{
	dbg_call( 1, "start\n" );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,21))
	lpx_table_header = register_sysctl_table(lpx_root_table);
#else
	lpx_table_header = register_sysctl_table(lpx_root_table, 1);
#endif

	dbg_call( 1, "end\n" );
}

void lpx_unregister_sysctl(void)
{
	unregister_sysctl_table(lpx_table_header);
}
