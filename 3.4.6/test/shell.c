/* 
    Automated test of netdisk software 
    In my hope, we will be able to check 
        whether porting of netdisk software is correct by running this code.
*/
#include "sal/libc.h"
#include "sal/net.h"
#include "sal/sal.h"
#include "sal/debug.h"
#include "xlib/dpc.h"
#include "sal/sync.h"
#include "sal/types.h"
#include "sal/thread.h"
#include "xlib/xhash.h"
#include "xlib/dpc.h"

extern void perform();
static void _check_hash(void);

int self_test_main(int argv, char** argc)
{
    printf("Initiazing..\n");
    sal_init();
    ndas_init(32,100, 2);
    ndas_register_network_interface("eth0");
    _check_hash();
    
    perform(); 
    
    ndas_unregister_network_interface("eth0");
    ndas_cleanup();
    sal_cleanup();
    printf("Test completed.\n");
    return 0;
}


/* Check the hash function works correct on this platform */
static void _check_hash(void)
{
    /* to do.. */
    return;
}

