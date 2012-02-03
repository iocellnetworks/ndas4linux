#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sal/sal.h"
#include "sal/types.h"
#include "sal/debug.h"
#include "ndasuser/ndasuser.h"
#include "ndasuser/io.h"

#define DISK_BLOCK_SIZE 512

static struct {
    char* name;
    char* id;
    char* key;
    int unit;
} unit_under_test = {
//    "1.1_80G", "P04H61485XDG95M1GF5Q", "LVG0A", 0
//    "silver", "VVRSGEKDFY1E8BK5KJPX", "7VT00", 0
//    "gig", "RVGDCE2YMGRETGQTHEJ8","2E3PY", 0
//    "1.0", "GJVAMF5LY20DVS4J0FQU", "LP12E", 0
//    "1.1", "XHMSU5S33YPARBS7N1XX    ", "AQKES", 0
//    "Defect", "68HH8U1KP2ULGJ05ULGA", "26N70", 0
//   "1.1E", "5J2TUF9PSQJ237KPBEEU", "B2JQ8", 0
//   "Buffalo", "SS6789SL9SS8ELYXS6A4", "GCY7Y", 0
//        "1.0", "DBHVMQMVQ2F892G0S12A", "F5TGS", 0
	"FE0388_default", "HMT8XDUG8KSLHCK0BL42", "Y557E", 0
//	"FE0388_seagate", "N5R0AYCH0XV06D6FK5AS", "GKNPU", 0
//	"Twobay", "ERQNSMXD7YEXV5AKQ7PA", "MUT6E", 1
};


/* copied from test/ndbench.c */
typedef enum _NDBENCH_TYPE {
    NDBENCH_READ,
    NDBENCH_WRITE,
    NDBENCH_SHAREDWRITE,
    NDBENCH_INTERLEAVED_RW,
    NDBENCH_RANDOM_RW
} NDBENCH_TYPE;

struct ndbench_opt {
    NDBENCH_TYPE    type;
    xuint32 nr_test_sectors;        // number of blocks to test
    xuint32 nr_request_sectors; // Size of request per operation. In number of blocks
    xuint32 start_sec_low;
    xuint32 start_sec_high; // ignored right now
    xuint32 start_sec_low_2;
    xuint32 start_sec_high_2; // ignored right now
    char* name; // disk name
    char* ndas_id;
    char* ndas_key;
    xuint32 unit;
    xbool async; // True if you want async operation
    xbool encryption_on; // Test with encryption on
    xbool encryption_off;// Test with encryption off

    // Internal use
    xuint32 io_queued; // number of IO in progress asynchronously
};


char* NDBENCH_TYPE_DESC[] = {
    "Read",
    "Write",
    "Shared write",
    "Interleaved IO",
    "Random IO"
};
    
ndas_error_t ndbench_warmup(int slot)
{
    static char rbuf[DISK_BLOCK_SIZE];
    ndas_error_t status;
    ndas_io_request req = {0};

    req.start_sec = 0;
    req.num_sec = 1;
    req.buf = rbuf;
    req.done = NULL;
    status = ndas_read(slot, &req);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndbench: Warmup failed\r\n");
        return -1;
    }
    return NDAS_OK;
}

NDAS_CALL ndas_error_t permission_request_handler(int slot)
{
    // ignore. I'm testing now.
    return NDAS_OK;
}

void ndbench_ndas_io_done(int slot, ndas_error_t err,void* user_arg)
{
    ((struct ndbench_opt*)user_arg)->io_queued--;
    return;
}

static xuint32 ndbench_rand(void) { 
    static xbool inited = FALSE;
    static xuint32 idum = 0;
    if ( inited == FALSE ) {
        idum = sal_time_msec();
        inited = TRUE;
    }
    idum = 1664525L * idum + 1013904223L;
    return idum;
}

ndas_error_t ndbench_io_throughput(struct ndbench_opt* opt)
{
    char* ndd_buf = NULL;
    int slot, i;
    int start_msec, end_msec;
    unsigned long long thruput;
    ndas_error_t    status;
    ndas_io_request req = {0};
    ndas_io_request req2 = {0};
    ndas_device_info info;
    ndas_slot_info_t sinfo;
    xbool write_mode;
    int loop_count;
    sal_error_print("ndbench_io_throughput(%s,%s,%s) - %s\r\n",
        opt->name, opt->ndas_id, opt->ndas_key, NDBENCH_TYPE_DESC[opt->type]);
    sal_error_print("Testing size: %d MByte\n", opt->nr_test_sectors /1024/2);
        
    status = ndas_register_device(opt->name, opt->ndas_id, opt->ndas_key);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_register_device failed: %d\r\n", status);
        return 0;
    }
    
    status = ndas_detect_device(opt->name);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_detect_device failed\r\n");
    }
    
    status = ndas_get_ndas_dev_info(opt->name,&info);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_get_ndas_dev_info failed\r\n");
        return 0;
    }
    slot = info.slot[opt->unit];
    if (slot == NDAS_INVALID_SLOT) {
        sal_error_print("Target device don't have unit %d\r\n", opt->unit);
        goto unreg_out;
    }
    status = ndas_query_slot(slot, &sinfo);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_query_slot failed\r\n");
        goto unreg_out;
    } else {
        sal_error_print("Disk capacity:%dGB\n", (int)(sinfo.sectors/2/1024/1024));
    }
    if (opt->encryption_off) {
        sal_error_print("**** Turn off encryption ****\n");
        status = ndas_set_encryption_mode(slot, 0, 0);
        if (!NDAS_SUCCESS(status)) {
            sal_error_print("Failed to set encryption mode\n");
            goto unreg_out;
        }
    } else if (opt->encryption_on) {
        sal_error_print("**** Turn on encryption ****\n");
        status = ndas_set_encryption_mode(slot, 1, 1);
        if (!NDAS_SUCCESS(status)) {
            sal_error_print("Failed to set encryption mode\n");
            goto unreg_out;
        }
    }
    
    
    switch (opt->type) {
    case NDBENCH_READ:
        write_mode = FALSE;
        status = ndas_enable_slot(slot);
        break;
    case NDBENCH_WRITE:
        write_mode = TRUE;
    case NDBENCH_INTERLEAVED_RW:
    case NDBENCH_RANDOM_RW:
        status = ndas_enable_exclusive_writable(slot);
        break;
    case NDBENCH_SHAREDWRITE:
        write_mode = TRUE;
        status = ndas_enable_writeshare(slot);
        break;
    default:
        sal_error_print("Unknown testing type\n");
        goto unreg_out;
    }
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_enable_slot failed: status=%d\r\n", status);
        goto unreg_out;
    }

    status = ndbench_warmup(slot);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("Warmup failed\r\n");
        goto unreg_out;
    }

    ndd_buf = sal_malloc(opt->nr_request_sectors * DISK_BLOCK_SIZE);
    sal_memset(ndd_buf, 0xaa , opt->nr_request_sectors * DISK_BLOCK_SIZE);
    start_msec = sal_time_msec();

//    req.start_sec = 0;
    req.num_sec = opt->nr_request_sectors;
    req.buf = ndd_buf;    
    if (opt->async) {
        req.done = ndbench_ndas_io_done;
        req.done_arg = (void*)opt;
        opt->io_queued = 0;
    } else {
        req.done = NULL;
    }
    
    if (opt->type==NDBENCH_INTERLEAVED_RW) {
        sal_memcpy(&req2, &req, sizeof(req));
    }
    
    loop_count = opt->nr_test_sectors/opt->nr_request_sectors;
    for(i=0; i<loop_count;i++) {
        req.start_sec = opt->start_sec_low + i * opt->nr_request_sectors;
        if (opt->async) {
            if (opt->io_queued > 128*4) {
                while(opt->io_queued) {
                    sal_msleep(20);
                }
            }
            opt->io_queued++;
        }
#if 0
	/* Test network unregistration while reading */
        if (i == 10) {
            sal_error_print("Unregistering interface\n");
            ndas_unregister_network_interface("eth0");
            ndas_unregister_network_interface("eth1");            
            ndas_restart();
        }
#endif
        switch(opt->type) {
        case NDBENCH_READ:
            status = ndas_read(slot, &req);
            break;
        case NDBENCH_WRITE:
        case NDBENCH_SHAREDWRITE:
            status = ndas_write(slot, &req);
            break;
        case NDBENCH_INTERLEAVED_RW:
            write_mode = FALSE;
            status = ndas_read(slot, &req);    
            break;
        case NDBENCH_RANDOM_RW:
            req.start_sec = opt->start_sec_low + 
                ndbench_rand() % (opt->start_sec_low_2 - opt->start_sec_low);
            req.num_sec = ndbench_rand() % opt->nr_request_sectors + 1;
            if (ndbench_rand()%2) {
                write_mode = TRUE;
                status = ndas_write(slot, &req);
            } else {
                write_mode = FALSE;
                status = ndas_read(slot, &req);
            }
            break;
        }
        if (!NDAS_SUCCESS(status)) {
            sal_error_print("Error %s at sector %d, nr_sec=%d, error=%d\n", 
                write_mode?"writing":"reading", (int)req.start_sec, opt->nr_request_sectors, status);
            break;
        }
        if (opt->type == NDBENCH_INTERLEAVED_RW) {
            if (opt->async) 
                opt->io_queued++;
            req2.start_sec = opt->start_sec_low + i * opt->nr_request_sectors;
            write_mode = TRUE;
            status = ndas_write(slot, &req2);    
            if (!NDAS_SUCCESS(status)) {
                sal_error_print("Error %s at sector %d, nr_sec=%d, error=%d\n", 
                    write_mode?"writing":"reading", (int)req.start_sec, opt->nr_request_sectors, status);
                break;
            }
        }
        if (((unsigned long long)i)*opt->nr_request_sectors % (1024*2 * 64)==0) {/* print every 64 Mbyte */
            sal_error_print("%d%%(%lld/%lld)\n", i*100/loop_count, 
                ((unsigned long long)i) * opt->nr_request_sectors,
                ((unsigned long long)loop_count) * opt->nr_request_sectors);
        }
    }
    if (opt->async) {
        while(opt->io_queued) {
            sal_msleep(20);
        }
    }
    sal_error_print("\r\nTest completed\r\n");
    ndas_disable_slot(slot);
    end_msec = sal_time_msec();
    thruput = ((unsigned long long)opt->nr_test_sectors) / 1024 * 512 * 8 * 1000 / (end_msec - start_msec); /* in kbps */
    sal_error_print("Time elapsed: %d mili-sec, %d MBytes, Throughput: %d.%d Mbps\r\n", 
        end_msec - start_msec,  
        opt->nr_test_sectors / 1024 * 512 /1024, 
        (int)(thruput / 1024), 
        (int)(thruput - (thruput / 1024) * 1024) * 1000/1024 / 100);    
unreg_out:
    ndas_unregister_device(opt->name);
    if (ndd_buf)
        sal_free(ndd_buf);
    return 0;
}


void sample_probe_enumerator(const char *ndasid, int online,void *arg) 
{
        int i;
    char true[] = "TRUE";
    char false[] = "FALSE";
    
    char* p = (online ? true : false);
    sal_error_print("[TEST] probed ndas_idserial=");
    for (i = 0; i < 20; i++) {
        if ( i/5*5 == i && i != 0) sal_error_print("-");
        sal_error_print("%c",ndasid[i]);
    }
    sal_error_print(" online=%s", p);
    sal_error_print("\r\n");
}


int main(int argc, char* argv[])
{
    int ret = 0;
    struct ndbench_opt opt = {0};
#if 1 /* Read test */
    opt.type = NDBENCH_READ; //NDBENCH_READ;
//    opt.nr_test_sectors = 140 * 1024 * 1024 * (1024 / 512); // 140G
    opt.nr_test_sectors = 128 * 1024 * (1024 / 512); // 128M
    opt.nr_request_sectors = 32 * 1024/512; // 32k
    opt.start_sec_low = 2 * 1024 * 50; // 50G
    opt.start_sec_low_2 += opt.start_sec_low + 1024 * 1024 * (1024 / 512); // + 1G
//    opt.async = TRUE;
    opt.async = FALSE;
//    opt.encryption_off = TRUE;
//    opt.encryption_on = TRUE;
#elif 0
//    opt.type = NDBENCH_INTERLEAVED_RW;
    opt.type = NDBENCH_SHAREDWRITE;
    opt.nr_test_sectors = 64 * 1024 * (1024 / 512); // 64M
    opt.nr_request_sectors = 32 * 1024/512; // 32k
    opt.start_sec_low = 120 * 1024 * 1024 * (1024 / 512);// 120 G
    opt.start_sec_low_2 += opt.start_sec_low + 1024 * 1024 * (1024 / 512); // + 1G
    opt.async = TRUE;
//    opt.encryption_on = TRUE; 
#elif 0
    opt.type = NDBENCH_WRITE;
    opt.nr_test_sectors = 256 * 1024 * (1024 / 512); // 256M
    opt.nr_request_sectors = 32 * 1024/512; // 32k
    opt.start_sec_low = 120 * 1024 * 1024 * (1024 / 512);// 120 G
    opt.start_sec_low_2 += opt.start_sec_low + 1024 * 1024 * (1024 / 512); // + 1G
    opt.async = TRUE;
//    opt.encryption_on = TRUE; 
#endif
    if ( argc != 3 && argc != 4) {
        opt.name = unit_under_test.name;
        opt.ndas_id  = unit_under_test.id;
        opt.ndas_key = unit_under_test.key;
        opt.unit = unit_under_test.unit;
        fprintf(stderr, "Disk information is not given. Using default disk key: %s %s %s\n", opt.name, opt.ndas_id, opt.ndas_key);
    } else {
        opt.name = argv[1];
        opt.ndas_id  = argv[2];
        opt.ndas_key = argv[3];
        opt.unit = 0;
    }
    
    sal_init();
    ndas_init(0, 0, 0);
    ndas_register_network_interface("eth0");
    ndas_register_network_interface("eth1");
    ndas_register_network_interface("eth2");
    ndas_register_network_interface("eth3");
    
    ndas_start();
#if 0
    {
        int size;
        int i;
        struct ndas_probed* probed;
        sal_msleep(1000 * 2);
        size = ndas_probed_size();
        if (size>0) {
            probed  = sal_malloc(sizeof(struct ndas_probed) * size);
            ret = ndas_probed_list(probed, size);
            if (ret == NDAS_OK) {
                for(i=0;i<size;i++) {
                    sal_error_print("%s\n", probed[i].id);
                }
            } else {
                sal_error_print("ndas_probed_list failed\n");    
            }
        } else {
            sal_error_print("ndas_probed_size failed\n");
        }
//        ndas_probe_device(NDAS_PROBE_ALL, sample_probe_enumerator, NULL);
    }
#endif

    ndbench_io_throughput(&opt);
    ndas_stop();
    ndas_unregister_network_interface("eth3");
    ndas_unregister_network_interface("eth2");
    ndas_unregister_network_interface("eth1");
    ndas_unregister_network_interface("eth0");    
    ndas_cleanup();
    sal_cleanup();
    return ret;
}

