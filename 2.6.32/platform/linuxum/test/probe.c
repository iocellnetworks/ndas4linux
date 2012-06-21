#include <stdio.h>
#include "sal/sal.h"
#include "sal/types.h"
#include "sal/debug.h"
#include "ndasuser/ndasuser.h"

extern void test_probe_enumerator(const char* ndasid,int isOnLine,void *arg);

static const char *t_ndasid;
void test_1(const char* ndasid, int isonline, void *arg)
{
    ndas_error_t err;
    sal_error_print("%s %s\n", ndasid, isonline?"Online":"Offline");
    if ( strncmp((char*)arg,ndasid,20) != 0 ) return;
    if ( isonline ) { 
        err = ndas_register_device(ndasid,ndasid,0);
        if (!NDAS_SUCCESS(err)) {
             sal_error_print("fail to register %s\n",ndasid);
             return;
        }
        sal_error_print("registed %s\n",ndasid);
    } else
    {
        err = ndas_unregister_device(ndasid);
        if (!NDAS_SUCCESS(err)) {
            sal_error_print("fail to unregister %s\n",ndasid);
            return;
        }
        sal_error_print("unregisted %s\n",ndasid);
    }
}
int main(int argc, char* argv[])
{
    int ret = 0;
    if ( argc != 2) {
        printf("usage: %s [ndas-id no dash]\n",argv[0]);
        return 1;
    }
    sal_init();
    ndas_init(0,0);
    ndas_register_network_interface("eth0");    
    ndas_start();
    for(;;){
        int size; 
        ndas_error_t err;
        struct ndas_probed *data;
        sal_msleep(3*1000);
        sal_error_print("begin to probe\n");

        size = ndas_probed_size(NDAS_PROBE_ALL);

        if ( !NDAS_SUCCESS(size) )
            continue;
        
        data = sal_malloc(sizeof(struct ndas_probed)* size);
        if ( !data ) 
            continue;
        
        err = ndas_probed_list(data, size);
        if ( NDAS_SUCCESS(err) ) {
            int i;
            for (i = 0 ; i < size; i++ )
                test_1(data[i].serial, data[i].status, argv[1]);
        }
        sal_free(data);
    }
    ndas_stop();
    ndas_unregister_network_interface("eth0");    
    ndas_cleanup();
    sal_cleanup();
    return ret;
}
