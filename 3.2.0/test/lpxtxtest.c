/*
    See comment in lpxrxtest.c
*/
#include "lpx/lpx.h"
#include "sal/sal.h"
#include "sal/libc.h"
#include "sal/mem.h"
#include "sal/debug.h"
#include "xlib/dpc.h"

void* lpxtxtest(void* param)
{
    struct sockaddr_lpx slpx;
    int s;
    int result;
    char msg[100] = "012345678901234567890012345678901234567890";
    char rmsg[100];
    int count = 0 ;
                
    s = lpx_socket(LPX_TYPE_STREAM, 0);
    if (s <0)
    {
        sal_debug_print("LPXTXTEST: lpx_socket error");
        return (void*)0;
    }
    memset(&slpx, 0, sizeof(slpx));

    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = 0;
    result = lpx_bind(s, &slpx, sizeof(slpx));
    if (result < 0)
    {
        sal_debug_print("AF_LPX: bind: %d\n",result);
        lpx_close(s);
        return 0;
    }
    
    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = htons(0x5000);

    slpx.slpx_node[0] = 0x00;
    slpx.slpx_node[1] = 0x0C;
    slpx.slpx_node[2] = 0x29;
    slpx.slpx_node[3] = 0xB8;
    slpx.slpx_node[4] = 0x46;
    slpx.slpx_node[5] = 0x07;

    sal_debug_print("LPXTXTEST: connecting");
    result = lpx_connect (s, &slpx, sizeof(slpx));
    if (result != 0)
    {
        sal_debug_print("LPXTXTEST: connect error");
        lpx_close (s);
        return (void*)-1;
    }
    else 
        sal_debug_print( "LPXTXTEST: ******* connect ******* \n" ) ;
    
    while(1) {
        sal_debug_print("LPXTXTEST:Trial %dth sending\n", count+1);
        result = lpx_send(s, msg, strlen(msg) + 1, 0);
        if (result < 0)
        {
            sal_debug_print("LPXTXTEST: send: %d\n",result);
            return (void*)-1;
        }
        result = lpx_recv(s, rmsg, result, 0);
        if (result < 0)
        {
            sal_debug_print("LPXTXTEST: recv: %d\n",result);
            return (void*)-1;
        }
        sal_debug_print("LPXTXTEST: Received message = %s, len=%d\n", rmsg, result);
        count ++ ;
        if( count == 30 ) 
            break ;
        sal_msleep(1* 1000);
    }

    lpx_close(s);
    sal_debug_print("LPXTXTEST: Tx test completed\n");
    return 0;
}

