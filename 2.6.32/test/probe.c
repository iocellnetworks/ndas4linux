/**********************************************************************
 * Copyright ( C ) 2012 IOCELL Networks
 * All rights reserved.
 **********************************************************************/
#include <sal/sal.h>
#include <sal/debug.h>
#include <sal/mem.h>
#include <sal/libc.h>

#include <ndasuser/ndasuser.h>
#include "ndasuser/io.h"

#define BLACK_LIST_NUM 6
char* blacklist[] = {
    "XT794EER8KP4LY8SXTMY", 
    "3ES7EH4SFSJR9JYSBB1U",
    "AU9PESHSPAFK8B8GJ62X",
    "GNVXGA4MPQSVY6CSLJ7K",
//    "TT6XGDSBD4Q7AEA8LJKC", // ver 1
    "B2B3Y04E0QYX1224NX9Y",
    "1VDSM64LSX67CKGRPJKX"
};
void test_probe_enumerator(const char *ndasid, int online,void *arg) 
{
    int i;
    char true[] = "TRUE";
    char false[] = "FALSE";
    char buf[1024];
    ndas_error_t err;
    ndas_device_info info;
    ndas_slot_info_t sinfo;
    ndas_io_request ioreq = { 
        .start_sec_low = 0, .start_sec_high = 0, .num_sec = 2, .buf = buf, .done = NULL
    };
    
    char* p = (online ? true : false);
    sal_error_print("[TEST] probed ndas_idserial=");
    for (i = 0; i < 20; i++) {
        if ( i/5*5 == i && i != 0) sal_debug_print("-");
        sal_error_print("%c",ndasid[i]);
    }
    sal_error_print(" online=%s:", p);
    if ( online == FALSE ) {
        sal_error_print("\n");
        return;
    }
    for (i = 0 ; i < BLACK_LIST_NUM; i ++ ) {
        if ( sal_strncmp(blacklist[i],ndasid,20) == 0 ) {
            sal_error_print("black listed\n");
            return;
        }
    }

    if ( (err = ndas_register_device(ndasid, ndasid, 0)) != NDAS_OK ) 
    {
        char buff[256];
        ndas_get_string_error(err, buff, 256);
        sal_error_print("fail to register reason=%s\n",buff);
        return;
    }
    err = ndas_detect_device(ndasid);
    if (err != NDAS_OK)
    {
        char buff[256];
        ndas_get_string_error(err, buff, 256);
        sal_error_print("fail to discover reason=%s\n",buff);
        return;
    }
    
    if ( (err = ndas_get_ndas_dev_info(ndasid,&info)) != NDAS_OK )
    {
        char buff[256];
        ndas_get_string_error(err, buff, 256);
        sal_error_print("fail to register reason=%s\n",buff);
        return;
    }
    if ( (err = ndas_enable_slot(info.slot[0])) != NDAS_OK) 
    {
        char buff[256];
        ndas_get_string_error(err, buff, 256);
        sal_error_print("fail to enable reason=%s\n",buff);
        ndas_unregister_device(ndasid);
        return;
    }
    
    ndas_query_slot(info.slot[0], &sinfo);
    err = ndas_read(info.slot[0],&ioreq);
    if ( !NDAS_SUCCESS(err) ) {
        char buff[256];
        ndas_get_string_error(err, buff, 256);
        sal_error_print("fail to read reason=%s\n",buff);
        ndas_disable_slot(info.slot[0]);
        ndas_unregister_device(ndasid);
        return;
    }        
    
    
    sal_error_print(" slot(%d) size=%u\n",info.slot[0], sinfo.capacity);
    ndas_disable_slot(info.slot[0]);
    ndas_unregister_device(ndasid);
}

