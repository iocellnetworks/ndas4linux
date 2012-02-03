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
#include <linux/utsname.h>
#include <linux/kernel.h>
#include <linux/module.h> // EXPORT_SYMBOL
//#include <linux/rwsem.h>
#include <linux/string.h>
#include <asm/semaphore.h> // up_read, down_read
#include "linux_ver.h"
#include "sal/sal.h"
#include "sal/net.h"
#include "sal/debug.h"
#include "sal/types.h"
#include "sal/time.h"
#include "sal/mem.h"

NDAS_SAL_API
void sal_init(void)
{
    sal_net_init();
}
EXPORT_SYMBOL(sal_init);

extern NDAS_SAL_API void sal_mem_display(int verbose);
NDAS_SAL_API
void sal_cleanup(void)
{
    sal_net_cleanup();
//    sal_mem_display(2);
}
EXPORT_SYMBOL(sal_cleanup);

NDAS_SAL_API ndas_error_t sal_gethostname(char* name, int size)
{
    int i;
    ndas_error_t err;

    if (size < 0)
        return NDAS_ERROR_INVALID_PARAMETER;
    down_read(&uts_sem);
    i = 1 + strlen(GET_UTS_NODENAME());
    if (i > size)
        i = size;
    err = NDAS_OK;
    memcpy(name, GET_UTS_NODENAME(), i);
    up_read(&uts_sem);
    return err;
}
EXPORT_SYMBOL(sal_gethostname);

