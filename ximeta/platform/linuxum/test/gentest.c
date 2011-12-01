#include "sal/sal.h"
#include "sal/types.h"
#include "ndasuser/ndasuser.h"
#include "sal/debug.h"

#if 0
int main(int argc, char* argv[])
{
    ndas_error_t ret = NDAS_OK;
    struct ndas_probed *data;
    int retry = 0;

    sal_init();
    ndas_init(0,0,0);
    ndas_register_network_interface("eth0");
    ndas_start();
    
	while(1) {
	    do {
	        int i, c;
	        c = ndas_probed_size();
	        if ( !NDAS_SUCCESS(c) ) {
			printf("probe failed\n");
	            return c;
	        }
	        
        
	        data = sal_malloc(sizeof(struct ndas_probed) * c);
	        if ( !data ) {
	            ret = NDAS_ERROR_OUT_OF_MEMORY;
	            break;
	        }
	        ret = ndas_probed_list(data, c);
	        if ( ret == NDAS_ERROR_INVALID_PARAMETER ) {
	            if ( retry++ > 2 ) {
	                break;
	            }
	            sal_free(data);
	            continue;
	        }
	        if ( !NDAS_SUCCESS(ret) ) {
	            break;
	        }
	        
	        for ( i = 0; i < c; i++ ) {
	        	printf("%s\n", data[i].id);
	        }
	        break;
	    } while(1);
	    sal_free(data);
	    printf("-------------------\n");
		sal_msleep(1000*5);
	}    
    return ret;
}

#endif

#if 1
// Test code for 2.0 small packet bug
int main(int argc, char* argv[])
{
    ndas_error_t    status;
    unsigned char* srcbuf;
    unsigned char* readbuf;
    ndas_io_request req;
    int i;
    int seed = 0;

    if (argc==2) {
        seed = atoi(argv[1]);
    }
    sal_error_print("Seed = %d\n", seed);
    
    sal_init();
    ndas_init(0,0,0);
    ndas_register_network_interface("eth0");
    ndas_register_network_interface("eth1");    
    ndas_start();

    status = ndas_register_device("Disk1", "68HH8U1KP2ULGJ05ULGA", "26N70");
		
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_register_device failed: %d\n",  status);
        return 0;
    }

    status = ndas_enable_writeshare(1);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_enable_slot failed\r\n");
        return 0;
    }


    sal_error_print("Starting with default config 1\n");
    
    srcbuf = sal_malloc(512);
    readbuf = sal_malloc(512);
    
#if 1
    for(i=0;i<512;i++) {
        srcbuf[i] = (unsigned char) (((seed+i) * 32217)%256);
    }
#else
    for(i=0;i<512;i++) {
        srcbuf[i] = 256-(unsigned char)i;
    }
#endif

    req.num_sec = 1;
    req.buf = (char*)srcbuf;    
    req.done = NULL;
    req.start_sec = 0;
    status = ndas_write(1, &req);

    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_write failed: %d\r\n", status);
        return 0;
    }

    req.num_sec = 1;
    req.buf = (char*)readbuf;
    req.done = NULL;
    req.start_sec = 0;
    
    status = ndas_read(1, &req);
    for(i=0;i<512;i++) {
        if (readbuf[i] != srcbuf[i]) {
            sal_error_print("Mismatch %d: %02x -> %02x\r\n", i, srcbuf[i], readbuf[i]);
        }
    }

    status = ndas_disable_slot(1); 
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_disable_slot failed %d\r\n", status);
        return 0;
    }

    status = ndas_unregister_device("Disk1");
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_unregister failed %d\r\n", status);
        return 0;
    }

    ndas_stop();
    ndas_unregister_network_interface("eth0");
    ndas_unregister_network_interface("eth1");
    ndas_cleanup();
    sal_cleanup();
    return 0;
}
#endif

