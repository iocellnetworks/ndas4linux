#include "sal/libc.h"
#include "sal/net.h"
#include "sal/sal.h"
#include "sal/debug.h"
#include "sal/sync.h"
#include "sal/types.h"
#include "sal/thread.h"
#include "xlib/dpc.h"
#include "xlib/xbuf.h"

int test_SalNetEther(int opt);
int test_dpc(void);
int test_sema(void);
int test_event(void);

int sal_test_main(int argv, char** argc)
{
    int i;
    struct xbuf* xb1, *xb2, *xb3;
    sal_init();
    xbuf_pool_init(100);
    for(i=0;i<100;i++) {
        xb1 = xbuf_alloc(100);    
        xb2 = xbuf_clone(xb1);
        xb3 = xbuf_clone(xb1);
        xbuf_free(xb3);
        xbuf_free(xb2);
        xbuf_free(xb1);
    }
#if 0    
    dpc_start();
    test_event();
#endif    
    
#if 0
    test_sema();
#endif
#if 0
    test_dpc();
#endif    
#if 0
    {
        int opt;
        if (argv==2) {
            opt = atoi(argc[1]);
        } else {
            opt = 0;
        }
        test_SalNetEther(opt);
    }
#endif

#if 0
    test_SalGetEthernetAddress();
#endif
    printf("Calling sal_cleanup\n");
    sal_cleanup();
    return 0;
}

void test_hexdump(char* buf, int len)
{
    int i;
    for(i=0;i<len;i++) {
        printf("%02x ", 0x0ff & buf[i]);
        if (i!=0 && i%16==0)
            printf("\n");
    }
    printf("\n");
}

static void test_print_mac(char* mac)
{
    int i;
    for(i=0;i<6;i++) {
        printf("%02X", (0x0ff) & mac[i]);
        if (i!=5) 
            printf(":");
    }
}

static void test_event_proc(void * p1, void* p2)
{
    sal_event evt = (sal_event) p1;
    sal_debug_print("Setting event\n");
    sal_event_set(evt);
}
static void* test_event_proc2(void * p1)
{
    sal_event evt = (sal_event) p1;
    sal_debug_print("Waiting event 2\n");
    sal_event_wait(evt, SAL_SYNC_FOREVER);
    sal_debug_print("Waken up 2\n");
    return NULL;
}

int test_event(void)
{
    sal_event evt;
    
    evt = sal_event_create("test");
    sal_thread_create(NULL, NULL, 0,0, test_event_proc2, (void*)evt);
    dpc_queue(3* 1000, DPC_PRIO_NORMAL, test_event_proc, (void*)evt,0);
    sal_debug_print("Waiting event 1\n");
    sal_event_wait(evt, SAL_SYNC_FOREVER);
    sal_debug_print("Waken up 1\n");
    return 0;    
}
static void test_sema_proc(void* p1, void* p2)
{
    sal_semaphore sem;
    int ret;
    sem = (sal_semaphore) p1;
    sal_debug_print("give\n");
    ret = sal_semaphore_give(sem);
    sal_debug_print("give ret=%d\n", ret);
    ret = sal_semaphore_give(sem);
    sal_debug_print("give ret=%d\n", ret);
}


int test_sema(void)
{
    sal_semaphore sem;
    int ret;
    sem = sal_semaphore_create("test", 1, 0);
    sal_assert(sem!=SAL_INVALID_SEMAPHORE);
    ret = sal_semaphore_give(sem);
    sal_debug_print("give 0 ret=%d\n", ret);
    ret = sal_semaphore_give(sem);
    sal_debug_print("give 0 ret=%d\n", ret);
    dpc_queue(5* 1000, DPC_PRIO_NORMAL, test_sema_proc, (void*)sem,0);
    printf("Take 1\n");
    sal_semaphore_take(sem, SAL_SYNC_FOREVER);
    printf("Take 2\n");
    sal_semaphore_take(sem, SAL_SYNC_FOREVER);
    printf("Take 3\n");
    sal_semaphore_take(sem, SAL_SYNC_FOREVER);
    printf("Take 4\n");
    sal_semaphore_take(sem, SAL_SYNC_FOREVER);
    printf("Take 5\n");
    return 0;
}

static volatile int v_dpc_test_count;

static void test_dpc_proc(void* p1, void* p2)
{
    sal_debug_print("          called at %u msec: %d\n", sal_time_msec(), (int)p1);    
    v_dpc_test_count++;
}

int test_dpc(void)
{
    int i;
    v_dpc_test_count = 0;
#ifdef srand
    srand(100);
#endif
    for(i=0;i<30;i++) {
        sal_debug_print("Queue at %u msec: %d\n", sal_time_msec(), i);
#ifdef RAND_MAX
        dpc_queue((int)(10000.0 * rand()/RAND_MAX), DPC_PRIO_NORMAL, test_dpc_proc, (void*)i,0);
        sal_msleep((int)(1000.0 * rand()/RAND_MAX));
#endif
    }
    sal_msleep(10* 1000);
    sal_assert(v_dpc_test_count==i);
    return 0;
}

#if 0
int test_SalNetEther(int opt)
{
    #define TEST_DATA_SIZE 1000
    #define TEST_BUF_SIZE (TEST_DATA_SIZE+SAL_ETHER_HEADER_SIZE)

    #define TEST_PROTO_ID 0x88ae 
    int c;
    sal_net_desc snd;
    int is_prim;
    char txbuf[TEST_BUF_SIZE]; /* buffer for a whole ethernet packet */
    char rxbuf[TEST_BUF_SIZE];
    int i;
    int ret;
    sal_tick tout = 500;
    char peermac[6] = {0};
    char mymac[6];
    const char hellomsg[] = "LPX_HELLO_MSG";
    sal_ether_header* prxh=(sal_ether_header*) rxbuf;
    sal_ether_header* ptxh=(sal_ether_header*) txbuf;
    char* rxdata = rxbuf + SAL_ETHER_HEADER_SIZE; /* payload part of ethernet packet */
    char* txdata = txbuf + SAL_ETHER_HEADER_SIZE;
    
    if (opt==0) {
        printf("===test_SalNetEther===\n");
        printf("Type 1 for primary test host, type 2 for secondary test host.");
        c = getchar();
        if (c!='1' && c!='2') {
            printf("Invalid input\n");
            return 0;
        }
        is_prim=(c=='1')?1:0;
    } else {
        is_prim=(opt==1)?1:0;
    }
    snd = sal_net_open("eth0");
    if (SAL_INVALID_NET_DESC == snd) {
        printf("sal_net_open failed\n");
        return 0;
    }
    sal_ether_address_get(snd, mymac);
    sal_ether_bind(snd, htons(TEST_PROTO_ID));

    /* Broadcast my MAC address and get peer's MAC address */
    sal_memcpy(txdata, hellomsg, sal_strlen(hellomsg));
    sal_memset(ptxh->des, 0xff, 6);
    printf("Now handshaking..\n");
    if (is_prim) {
        sal_ether_send(snd, ptxh, txdata, sal_strlen(hellomsg));
        ret = sal_ether_recv(snd, prxh, rxdata, TEST_DATA_SIZE, 1000 * 30);
        if (ret>0 && prxh->proto == htons(TEST_PROTO_ID)) {
            if (sal_memcmp(rxdata, hellomsg, sal_strlen(hellomsg))==0) {
                sal_memcpy(peermac, prxh->src, 6);
            } else {
                printf("Invalid handshake msg: %s\n", rxdata);
                goto errclose;
            }    
        } else {
            printf("Handshake timeout\n");
            goto errclose;
        }
    } else {
        ret = sal_ether_recv(snd, prxh, rxdata, TEST_DATA_SIZE, 1000 * 30);
        if (ret>0 && prxh->proto == htons(TEST_PROTO_ID)) {
            if (sal_memcmp(rxdata, hellomsg, sal_strlen(hellomsg))==0) {
                sal_memcpy(peermac, prxh->src, 6);
            } else {
                printf("Invalid handshake msg: %s\n", rxdata);
                goto errclose;
            }    
        } else {
            printf("Handshake timeout\n");
            goto errclose;
        }
        sal_ether_send(snd, ptxh, txdata, sizeof(hellomsg));    
    }

    printf("OK\n");

    test_print_mac(mymac);
    printf(" (I) <-> (peer) ");
    test_print_mac(peermac);
    printf("\n");
    
    for(i=0;i<TEST_BUF_SIZE;i++) {
        txdata[i] = i%100;
    }
    sal_memcpy(ptxh->des, peermac, 6);
    for(i=0;i<10;i++) {
        if (is_prim) {
            /* Primary: Check sent packet is replied by another host */
            ret = sal_ether_send(snd, ptxh, txdata, TEST_DATA_SIZE);
            if (ret!=TEST_DATA_SIZE) {
                printf("sal_ether_send failed\n");
                goto errclose;
            }
            ret = sal_ether_recv(snd, prxh, rxdata, TEST_DATA_SIZE, tout);
            if (ret!=TEST_DATA_SIZE) {
                printf("sal_ether_recv failed %d\n", ret);
                goto errclose;
            }
            if (sal_memcmp(prxh->src, peermac,6) == 0 && 
                sal_memcmp(txdata, rxdata, TEST_DATA_SIZE)==0) {
                printf("Trial %d OK\n", i);
            } else {
                printf("Trial %d Error\n", i);
                goto errclose;
            }
        } else {
            ret = sal_ether_recv(snd, prxh, rxdata, TEST_DATA_SIZE, tout);
            if (ret!=TEST_DATA_SIZE) {
                printf("sal_ether_recv failed %d\n", ret);
                goto errclose;
            }
            if (sal_memcmp(prxh->src, peermac, SAL_ETHER_ADDR_LEN)==0) {
                printf("Get packet %d\n", i);    
            } else {
                printf("Packet from invalid peer\n");
            }
            /* Send rxed data back */
            ret = sal_ether_send(snd, ptxh, rxdata, TEST_DATA_SIZE);
            if (ret!=TEST_DATA_SIZE) {
                printf("sal_ether_send failed\n");
                goto errclose;
            }
        }
    }
errclose:
    sal_net_close(snd);
    return 0;
}
#endif
