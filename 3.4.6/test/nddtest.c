/*
    Test netdisk device operation
*/
#include "sal/libc.h"
#include "sal/net.h"
#include "sal/sal.h"
#include "sal/debug.h"
#include "xlib/dpc.h"
#include "sal/sync.h"
#include "sal/types.h"
#include "sal/thread.h"
#include "lpx/lpx.h"
#include "ndasuser/ndasuser.h"
#include "ndasuser/info.h"

int nddtest(void);

int ndd_test_main(int argv, char** argc)
{    
    int result;
    
    sal_init();
    ndas_init(0,0,2);
//    test_serial();

    result =  ndas_register_network_interface("eth0");
    if (result) {
        printf("ndas_register_network_interface failed\n");
        return 0;
    }    
    ndas_start();
    nddtest();
    ndas_stop();
    ndas_unregister_network_interface("eth0");
    ndas_cleanup();    
    sal_cleanup();
    return 0;
}

#define NDD_TEST_BUF_SIZE 512 * 20
static char ndd_buf[NDD_TEST_BUF_SIZE];
NDAS_CALL ndas_error_t permission_request_handler(int slot)
{
    // ignore. I'm testing now.
}
int nddtest(void)
{
    int slot;
    ndas_error_t    status;
    int i;
    ndas_device_info info;
//    status = ndas_register_device("orin", "0QXPG74DCYH80FYRYGHU", "PRKTY");
    status = ndas_register_device("silver", "VVRSGEKDFY1E8BK5KJPX", "7VT00");
    if (!NDAS_SUCCESS(status)) {
        printf("ndas_register_device failed\n");
        return 0;
    }
    ndas_detect_device("silver");
    status = ndas_get_ndas_dev_info("silver",&info);
    if (!NDAS_SUCCESS(status)) {
        printf("ndas_register_device failed\n");
        return 0;
    }
    slot = info.slot[0];
    status = ndas_enable_exclusive_writable(slot);
    if (!NDAS_SUCCESS(status)) {
        printf("ndas_enable_slot failed\n");
        return 0;
    }
    
    for(i=0;i<NDD_TEST_BUF_SIZE;i++) {
        ndd_buf[i] = i%200;    
    }

    ndas_io_request req = {0};
    req.buf = ndd_buf;
    req.num_sec = NDD_TEST_BUF_SIZE / 512;
    req.start_sec = 0;
    req.done = NULL;
    
    status = ndas_write(slot, &req);
    if (!NDAS_SUCCESS(status)) {
        printf("ndas_write failed to write\n");
        return 0;
    }

    req.buf = ndd_buf;
    req.num_sec = NDD_TEST_BUF_SIZE / 512;
    req.start_sec = 0;
    status = ndas_read(slot, &req);
    if (!NDAS_SUCCESS(status)) {
        printf("ndas_read failed to read\n");
        return 0;
    }

    sal_debug_print("----------------------\n");
    sal_debug_hexdump(ndd_buf, NDD_TEST_BUF_SIZE);
    sal_debug_print("----------------------\n");

    ndas_disable_slot(slot);
    return 0;
}
