#include <sal/sal.h>
#include <sal/debug.h>
#include <sal/mem.h>
#include <sal/libc.h>

#include <ndasuser/ndasuser.h>

int main() {
    sal_init();
    ndas_init(0,0);
    ndas_register_network_interface("eth0");
    ndas_start();
    sal_msleep(5000);

    ndas_stop();
    ndas_unregister_network_interface("eth0");
    
    ndas_cleanup();
    
    sal_cleanup();
    
    return 1;
}
