/*
    run lpxrxtest on one host and run lpxtxtest on another host.
    Modify lpxtxtest's destination MAC address to the address of lpxrxtest host.
*/
#include "sal/debug.h"
#include "lpx/lpx.h"
#include "sal/sal.h"
#include "sal/libc.h"
#include "xlib/dpc.h"

int
lpxrxtest_main(int argc, char **argv)
{
    struct sockaddr_lpx slpx;
    int s;
    int result;
    int news;
    int count = 0 ; 

    sal_init();
    dpc_start(50);
    lpx_init(64);
    
    sal_debug_print("Starting..\n");
    result = ndas_register_network_interface("eth0");
    if (result) {
        sal_debug_print("lpx_register_dev failed\n");
        return 0;
    }

    s = lpx_socket(LPX_TYPE_STREAM, 0);
    if (s < 0)
    {
        sal_debug_print("LPX: socket: %d\n", s);
        return (-1);
    }
    memset(&slpx, 0, sizeof(slpx));
    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = htons(0x5000);

    result = lpx_bind(s, (struct sockaddr_lpx *) &slpx, sizeof(slpx));
    if (result < 0)
    {
        sal_debug_print("LPX: bind: %d\n",result);
        return (-1);
    }

    result = lpx_listen (s, 1);
    if (result < 0)
    {
        sal_debug_print("LPX: listen: %d\n", result);
        return (-1);
    }
restart:
    sal_debug_print("LPX: accepting \n" ) ;
    news = lpx_accept (s, (struct sockaddr_lpx *) NULL, 0u);
    if (news <0 )
    {
        sal_debug_print("LPX: accept: %d", news);
        lpx_close(s);
        return (-1);
    }
    else 
        sal_debug_print( "LPX: ******* connect ******** \n" ) ;

    while(1) {
        char msg[2];

        result = lpx_recv(news, msg, sizeof(msg)-1, 0);

        if (result <= 0)
        {
            sal_debug_print("LPX: recv: %d", result);
            break;
        }
        msg[result] = '\0';
        sal_debug_print("recv result = %d, \tGot \"%s\"\n", result, msg);
        result = lpx_send(news, msg, result, 0);
        if (result <= 0)
        {
            sal_debug_print("LPX: recv: %d", result);
            break;
            return -1;
        }
        sal_debug_print("send result = %d\n", result);
    }

    lpx_close(news);
    

    sal_msleep(10* 1000);
goto restart;
    return 0;
}
