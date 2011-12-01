/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
#include <types.h>
#include <stdio.h>
#include <irx.h>
#include <thbase.h>
#include <ps2ip.h>
#include <loadcore.h>
#include <sifcmd.h>

#include "sal/sal.h"
#include "sal/debug.h"
#include "sal/time.h"
#include "sal/mem.h"
#include "sal/thread.h"
#include "sal/libc.h"

#include "../smapx/smapx.h"
#include "ndasuser/ndasuser.h"
#include "ndrpccmd.h"

#define MODNAME "netdisk"
#define PS2DEV_HEARBEAT    1
IRX_ID(MODNAME, 1, 1);

struct irx_export_table _exp_netdisk;

extern void* lanscsiclimain(void* param);
extern void* ndbench(const char *name,const char *ndas_id, const char *ndas_key);
extern void* lpxtxtest(void* param);

extern int SalGetLog(char* buf, int len);

static SifRpcDataQueue_t ps2netdisk_queue  __attribute__((aligned(64)));
static SifRpcServerData_t ps2netdisk_server  __attribute__((aligned(64)));
static int _rpc_buffer[512] __attribute__((aligned(64)));

void do_ps2netdisk_read_log(void* rpcBuffer, int size)
{
    int *ptr = rpcBuffer;
    char* data = (char*)&(ptr[1]); 
    int buflen;
    int len;

    buflen = ptr[0];
    len = SalGetLog(data, buflen-1);
    data[len] =0;
    ptr[0] = len;
}

extern int fattest_init(char* name, char* id, char* key);
extern int fattest_show_root(void);
extern int fattest_play(void);

void do_fat_test(void* rpcBuffer, int size)
{
    static int count=0;
    if (count==0) {
        fattest_init("silver", "VVRSGEKDFY1E8BK5KJPX", "7VT00");
    } else if (count==1) {
        fattest_show_root();
    } else if (count==2) {
        fattest_play();
    } else {
        sal_debug_print("No more test..\n");
    }
    count++;
}

#ifdef PS2DEV_HEARBEAT
void* _irx_heartbeat(void* param)
{
    while(1) {
        sal_debug_print("$");
        sal_msleep(1000);
    }
    return 0;
}
#endif //PS2DEV_HEARTBEAT

/**
 * netdiskRpcHandler - Handle the RPC request from EmotionalEngine or peer IRX module
 * @command the command of RPC request
 * @rpcBuffer the data buffer of RPC request
 * @size the size of the data buffer specified by rpcBuffer
 */
void* netdiskRpcHandler(unsigned int command, void * rpcBuffer, int size)
{
    int size;
    ndas_error_t err;
    struct ndas_probed *data;
    extern void test_probe_enumerator(char*, int);
    
    switch(command) {
/*    case PS2NETDISK_CMD_COUNT:
        do_ps2netdisk_count(rpcBuffer, size);
        break; */
    case PS2NETDISK_CMD_READ_LOG:
        do_ps2netdisk_read_log(rpcBuffer, size);
        break;
    case PS2NETDISK_CMD_FAT_TEST:
        do_fat_test(rpcBuffer, size);
        break;
    case PS2NETDISK_CMD_MISC_TEST:        
/*        ndShowAliveList(); */
        break;
    case PS2NETDISK_CMD_PROBE_TEST:
        size = ndas_probed_size();
        if (!NDAS_SUCCESS(size))
            break;
        data = sal_malloc(sizeof(struct ndas_probed) * size);
        if ( !data ) 
            break;
        err = ndas_probed_list(data,size);
        if (NDAS_SUCCESS(err)) {
            int i;
            for (i = 0; i < size; i++) {
                test_probe_enumerator(data[i].id, data[i].online);    
            }
        }
        sal_free(data);
        break;
    default:
        sal_debug_print("PS2Netdisk: Unknown rpc command\n");
    }        
    return rpcBuffer;
}
/**
 * netdiskRpcFunction - Register RPC handler and loop the RPC request queue.
 * @param - 
 */
void* netdiskRpcFunction(void* param)
{
   int threadId;
   
   sal_debug_print("Netdisk: RPC Thread Started\n");

   SifInitRpc( 0 );
   threadId = GetThreadId();

   SifSetRpcQueue( &ps2netdisk_queue , threadId );
   SifRegisterRpc( &ps2netdisk_server, PS2NETDISK_IRX, 
           (void *)netdiskRpcHandler,(u8 *)&_rpc_buffer,0,0, &ps2netdisk_queue );
   SifRpcLoop( &ps2netdisk_queue );    
   return NULL;
}

extern void* fat_test_init(void* param);
/**
 * Entry point
 */
void
_start( int argc, char **argv)
{
    int res;
    sal_init();
    ndas_init(32,50);    
    res = RegisterLibraryEntries(&_exp_netdisk);
    if (res!=0) {
        sal_debug_print("RegisterLibraryEntries failed\n");
        return;
    }
    res = ndas_register_network_interface("sm1");
    ndas_start();
    if (res) {
        sal_debug_print("ndas_register_dev failed\n");
        return;
    }

    sal_msleep(2 * 1000);

#ifdef PS2DEV_HEARBEAT    
    sal_thread_create(NULL, "heartbeat", -1,0, _irx_heartbeat, NULL);
#endif

    sal_thread_create(NULL, "netdisk rpc", 1,0, netdiskRpcFunction, 0);

//    sal_thread_create(NULL, "lanscsicli", -3,0, lanscsiclimain, "00:0B:D0:00:F2:8A");

//    sal_thread_create(NULL, "ndbench", -3,0, ndbench, 0);    
#ifdef PS2DEV_FATTEST
    sal_thread_create(NULL, "fatinit", -3,0, fat_test_init, 0);    
#endif     
//    sal_thread_create(NULL, "lpxtxtest", -3,0, lpxtxtest, 0);

    sal_debug_print("Netdisk: IRX test module exiting\n");
    return;
}
/**
 * Exit point
 */
void shutdown(void) {};

/*----------------------------------*/
int netdisk_irx_test_export(void)
{
    return 0;
}
