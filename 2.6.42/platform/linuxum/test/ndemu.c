/*
    ndemu.c : Netdisk emulater. This emulater supports only one concurrent connection only
    
*/

#include <stdio.h>
#include <unistd.h>
#include <ndasuser/ndasuser.h>
#include <ndasuser/persist.h>
#include <ndasuser/io.h>
#include <ndasuser/write.h>
#include <ndasuser/bind.h>


int main(int argc, char* argv[])
{
	int ret;
	char* dev;        
	int i;
	if (argc<2) {
		dev = "../emutest.bin";
		printf("Disk image file is not given. Using file %s\n", dev);
	} else {
		dev = argv[1];
	}
	sal_init();
	ndas_init(64,350, 16);
	ndas_register_network_interface("eth0");
	ndas_register_network_interface("eth1");
	
	ret = ndas_emu_start(dev, 1, 32);
	if (ret) {
	        // Failed to load
	    ndas_emu_stop();
	    ndas_cleanup();
	    return -1;
	}

	while(1) {
//		sal_mem_display(1);
		sleep(1);
	}
        ndas_emu_stop();
        ndas_cleanup();
        sal_cleanup();
        return 0;
}

