/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/
#include "sal/libc.h" // strcpy
#include "sal/time.h"
#include "sal/debug.h"
#include "sal/mem.h"
#include "ndasuser/ndasuser.h"
#ifdef XPLAT_UIO
#error This code does not support uio
#else
#include "ndasuser/io.h"
#endif

#define DISK_BLOCK_SIZE 512
#define NDD_TEST_BUF_SIZE 512 * 32 /* 16k */


//#define NDD_TEST_SECTOR_SIZE (1024 * 1024 * 1024 * 1 /512) /* 1 Gbyte */
//#define NDD_TEST_SECTOR_SIZE (1024 * 1024 * 128 /512) /*128 Mbyte */
#define NDD_TEST_SECTOR_SIZE (1024 * 1024 * 32 /512) /* 32 Mbyte */

NDAS_CALL ndas_error_t permission_request_handler(int slot)
{
    // Ignore. I'm testing now.
    return NDAS_OK;
}

/* return -1 for error */
int v_initialize_connection(const char *ndas_id, const char * key)
{
    ndas_device_info info;
    ndas_slot_info_t sinfo;
    ndas_error_t    status;
    int slot;
    status = ndas_register_device(ndas_id, ndas_id, key);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_register_device failed\r\n");
        return -1;
    }

    status = ndas_query_device(ndas_id ,&info);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_query_device failed\r\n");
        return -1;
    }
    slot = info.slot[0];
    status = ndas_query_slot(slot, &sinfo);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_query_slot failed\r\n");
        return -1;
    }
    status = ndas_enable_slot(slot);
    if (!NDAS_SUCCESS(status)) {
        sal_error_print("ndas_enable_slot failed\r\n");
        return -1;
    }
    return slot;
}

void* multidisk_test(const char *src_ndas_id, const char *target_ndas_id, const char * target_ndas_key)
{
    char* ndd_buf;
    char* ndd_buf2;
    int slot_src, slot_des, i;
    ndas_error_t    status;
    ndas_io_request req = {0}, req2 = {0};

    sal_error_print("multidisk_test: Copying from %s to %s\r\n",src_ndas_id,target_ndas_id);

    slot_src = v_initialize_connection(src_ndas_id, "");
    slot_des = v_initialize_connection(target_ndas_id, target_ndas_key);
    if (slot_src == -1 || slot_des == -1) {
        sal_error_print("Fail to initialize\n");
        return 0;
    }
    
    ndd_buf = sal_malloc(NDD_TEST_BUF_SIZE);
    ndd_buf2 = sal_malloc(NDD_TEST_BUF_SIZE);
    
    req.start_sec = 0;
    req.num_sec = NDD_TEST_BUF_SIZE / 512;
    req.buf = ndd_buf;    
    req.done = NULL;

    sal_error_print("Clearing target disk\n");        
    req.start_sec = 0;
    req.num_sec = NDD_TEST_BUF_SIZE / 512;
    req.buf = ndd_buf;    
    req.done = NULL;    
    sal_memset(ndd_buf, 0, NDD_TEST_BUF_SIZE);
    for(i=0; i< NDD_TEST_SECTOR_SIZE; i+=NDD_TEST_BUF_SIZE / 512) {
            req.start_sec = i;
clear_retry:
        status = ndas_write(slot_des, &req);

        if (!NDAS_SUCCESS(status)) {
            int retry=0;
            sal_error_print("ndas_write failed %d\r\n", status);
            while(1) {
                status = ndas_enable_slot(slot_des);
                if (!NDAS_SUCCESS(status)) {
                    sal_error_print("ndas_enable_slot failed\r\n");
                    retry++;
                    sal_msleep(200);
                    if (retry>5) {
                        sal_error_print("ndas_enable_slot failed 5 times.Exiting\r\n");
                        goto unreg_out;
                    }
                } else {
                    goto clear_retry;
                }
            }
        }
    }

    sal_error_print("Start copying\n");
    req.start_sec = 0;
    req.num_sec = NDD_TEST_BUF_SIZE / 512;
    req.buf = ndd_buf;    
    req.done = NULL;    
    for(i=0; i< NDD_TEST_SECTOR_SIZE; i+=NDD_TEST_BUF_SIZE / 512) {
            req.start_sec = i;
read_retry:
        status = ndas_read(slot_src, &req);

        if (!NDAS_SUCCESS(status)) {
            int retry=0;
            sal_error_print("ndas_read failed %d\r\n", status);
            while(1) {
                status = ndas_enable_slot(slot_src);
                if (!NDAS_SUCCESS(status)) {
                    sal_error_print("ndas_enable_slot failed\r\n");
                    retry++;
                    sal_msleep(200);
                    if (retry>5) {
                        sal_error_print("ndas_enable_slot failed 5 times.Exiting\r\n");
                        goto unreg_out;
                    }
                } else {
                    goto read_retry;
                }
            }
        }
write_retry:
        status = ndas_write(slot_des, &req);

        if (!NDAS_SUCCESS(status)) {
            int retry=0;
            sal_error_print("ndas_write failed %d\r\n", status);
            while(1) {
                status = ndas_enable_slot(slot_des);
                if (!NDAS_SUCCESS(status)) {
                    sal_error_print("ndas_enable_slot failed\r\n");
                    retry++;
                    sal_msleep(200);
                    if (retry>5) {
                        sal_error_print("ndas_enable_slot failed 5 times.Exiting\r\n");
                        goto unreg_out;
                    }
                } else {
                    goto write_retry;
                }
            }
        }

        if (i%(1024 * 2 * 2)==0) { /* print every 2M byte */
            sal_error_print("%d%% ", i*100/NDD_TEST_SECTOR_SIZE);
        }
    }


    sal_error_print("Start checking\n");
    req.start_sec = 0;
    req.num_sec = NDD_TEST_BUF_SIZE / 512;
    req.buf = ndd_buf;    
    req.done = NULL;
    req2.start_sec = 0;
    req2.num_sec = NDD_TEST_BUF_SIZE / 512;
    req2.buf = ndd_buf2;    
    req2.done = NULL;

    for(i=0; i< NDD_TEST_SECTOR_SIZE; i+=NDD_TEST_BUF_SIZE / 512) {
            req.start_sec = i;
            req2.start_sec = i;            
src_retry:
        status = ndas_read(slot_src, &req);

        if (!NDAS_SUCCESS(status)) {
            int retry=0;
            sal_error_print("ndas_read failed %d\r\n", status);
            while(1) {
                status = ndas_enable_slot(slot_src);
                if (!NDAS_SUCCESS(status)) {
                    sal_error_print("ndas_enable_slot failed\r\n");
                    retry++;
                    sal_msleep(200);
                    if (retry>5) {
                        sal_error_print("ndas_enable_slot failed 5 times.Exiting\r\n");
                        goto unreg_out;
                    }
                } else {
                    goto src_retry;
                }
            }
        }
des_retry:
        status = ndas_read(slot_des, &req2);

        if (!NDAS_SUCCESS(status)) {
            int retry=0;
            sal_error_print("ndas_read failed %d\r\n", status);
            while(1) {
                status = ndas_enable_slot(slot_des);
                if (!NDAS_SUCCESS(status)) {
                    sal_error_print("ndas_enable_slot failed\r\n");
                    retry++;
                    sal_msleep(200);
                    if (retry>5) {
                        sal_error_print("ndas_enable_slot failed 5 times.Exiting\r\n");
                        goto unreg_out;
                    }
                } else {
                    goto des_retry;
                }
            }
        }

        if (i%(1024 * 2 * 2)==0) { /* print every 2M byte */
            sal_error_print("%d%% ", i*100/NDD_TEST_SECTOR_SIZE);
        }
        if (sal_memcmp(ndd_buf, ndd_buf2, NDD_TEST_BUF_SIZE) !=0 ) {
            sal_error_print("Found mismatch data: at sector %d\n", i);
        }
    }


    sal_error_print("\r\nTest completed\r\n");
    ndas_disable_slot(slot_src);
    ndas_disable_slot(slot_des);    

unreg_out:
    ndas_unregister_device(src_ndas_id);
    ndas_unregister_device(target_ndas_id);    
    return 0; 
}
