#include <stdio.h>
#include "sal/sal.h"
#include "sal/types.h"
#include "sal/debug.h"
#include "ndasuser/ndasuser.h"

extern void* multidisk_test(const char *src_id, const char * des_id, const char* des_key);

int main(int argc, char* argv[])
{
    sal_init();
    ndas_init(0,0);
    ndas_register_network_interface("eth0");    

    ndas_start();

    multidisk_test("VVRSGEKDFY1E8BK5KJPX","RVGDCE2YMGRETGQTHEJ8", "2E3PY");
    ndas_stop();
    ndas_unregister_network_interface("eth0");    
    ndas_cleanup();
    sal_cleanup();
    return 0;
}

