#include "sal/sal.h"
#include "ndasuser/ndasuser.h"

extern void* lanscsiclimain(char* mac);

int main(int argc, char* argv[])
{
    int ret;
    sal_init();
    ndas_init(0,0);
    ndas_register_network_interface("eth0");    
    if (argc==2) {
        ret = (int)lanscsiclimain(argv[1]);
    } else {
//        ret = lanscsiclimain("00:0B:D0:00:F2:8A"); /* silver 7VT00 */
        ret = (int)lanscsiclimain("00:0D:56:9D:12:53"); /* pm02 */
    }
    ndas_cleanup();
    sal_cleanup();
    return ret;
}

