#include <stdio.h>
#include <string.h>

#include "porting.h"
#include "config.h"
#include "em86xxapi.h"    // make sure always follow config.h
#include "uart.h"
#include "io.h"
#include <ucos_ii.h>
#include "syabas.h"

#define    ESC            "\x1b"
//#define CLRSCR        ESC"c"ESC"[2J"ESC"[1;1H"
#define CLRSCR

volatile int svc_stack; 
volatile int usr_stack;
volatile int irq_stack;
volatile int fiq_stack;

void _init_stacks(void)
{
    register volatile UINT32 *sp asm("sp");
    register int temp32;

    /* setup mode stacks */
    /* should be in SVC mode */
    temp32 = (int)sp;
    svc_stack = temp32;      
    temp32 -= SVC_STACK_SIZE;

    /* switch to USR/SYS mode */
    asm("mov r0, %0"::"I"(SYS32MODE | NOINT));
    asm("msr cpsr_c, r0");
    /* init USR stack */
    sp = (UINT32*)temp32;
    usr_stack = temp32;
    temp32 -= USR_STACK_SIZE;

    /* switch to IRQ mode */
    asm("mov r0, %0"::"I"(IRQ32MODE | NOINT));
    asm("msr cpsr_c, r0");
    /* init IRQ stack */
    sp = (UINT32*)temp32;
    irq_stack = temp32;
    temp32 -= IRQ_STACK_SIZE;

    /* switch to FIQ mode */
    asm("mov r0, %0"::"I"(FIQ32MODE | NOINT));
    asm("msr cpsr_c, r0");
    /* init FIQ stack */
    sp = (UINT32*)temp32;
    fiq_stack = temp32;
    temp32 -= FIQ_STACK_SIZE;

    asm("mov r0, %0"::"I"(SVC32MODE | NOINT));
    asm("msr cpsr_c, r0");
}

volatile UINT32 glb_sec;
volatile UINT32 glb_msec;
volatile UINT32 ntp_sec;
volatile UINT32 ntp_msec;
//BOOL f_sync_clock;

void boot_config(void)
{         
    UINT32 data;
    // PCI configuration
#ifdef CONFIGVALID_PCISUBID
    __raw_writel(DEFAULT_PCI_SUBSYSTEM_ID, REG_BASE_HOST + PCI_devcfg_reg2);
#endif

#ifdef CONFIGVALID_PCIREVID
    data = __raw_readl(REG_BASE_HOST + PCI_devcfg_reg1);
    data &= 0xffffff00;
    data |= DEFAULT_PCI_REVISION_ID;
    __raw_writel(data, REG_BASE_HOST + PCI_devcfg_reg1);
#endif

#ifdef CONFIGVALID_PCIMEMSIZE
    data = __raw_readl(REG_BASE_HOST + PCI_devcfg_reg3);
    data &= 0xfffffff8;
    data |= DEFAULT_PCI_MEMORY_SIZE;
    __raw_writel(data, REG_BASE_HOST + PCI_devcfg_reg3);
#endif

    // set PCI configuration valid bit
    __raw_writel(0x00010000, REG_BASE_HOST + PCI_host_host_reg2);
}
       
#if 0
void Sigma_memcpy(void *to, const void *from, unsigned long n)
{
#if 0    
    // brute-force copying

    int i;
    unsigned char *_to = (unsigned char *) to;
    unsigned char *_from = (unsigned char *) from;
    
    for (i = 0; i < n; ++i)
        _to[i] = _from[i];
#else    
    // copy based on alignment of source and target
    // copy is fast when the source and destination have the same alignment 
    //   and the both are 4-byte aligned
    // alignment difference : 
    //   0 : copy_4
    //   1, 3 : copy_1
    //   2 : copy_2

    unsigned char *_to = (unsigned char *) to;
    unsigned char *_from = (unsigned char *) from;
    int align_to, align_from;
    unsigned int i, x, y, z;

    if (n < 8)
        goto copy_1;
    
    align_to = ((unsigned int) to) & 0x03;
    align_from = ((unsigned int) from) & 0x03;

    if (align_to == align_from) {
        x = (align_to) ? 4 - align_to : 0;
        n -= x;
        y = n >> 2;
        z = n & 0x03;
        goto copy_4;
    } else {
        if ((align_to & 0x01) == (align_from & 0x01)) {
            x = (align_to & 0x01);
            n -= x;
            y = n >> 1;
            z = n & 0x01;
            goto copy_2;
        } else
            goto copy_1;
    }
        
copy_4:
    while (x--)
        *_to++ = *_from++;
    if (y) {
        unsigned int *__to = (unsigned int *) _to;
        unsigned int *__from = (unsigned int *) _from;
        for (i = 0; i < y; ++i)
            __to[i] = __from[i];
        _to += (y << 2);
        _from += (y << 2);
    }
    while (z--)
        *_to++ = *_from++;
    goto copy_return;

copy_2:
    while (x--)
        *_to++ = *_from++;
    if (y) {
        unsigned short *__to = (unsigned short *) _to;
        unsigned short *__from = (unsigned short *) _from;
        for (i = 0; i < y; ++i)
            __to[i] = __from[i];
        _to += (y << 1);
        _from += (y << 1);
    }
    while (z--)
        *_to++ = *_from++;
    goto copy_return;

copy_1:
    for (i = 0; i < n; ++i)
        _to[i] = _from[i];

copy_return:
#endif
}
#endif

char debug_date[40];

int main(void)
{
/*    UINT32 test = KERNEL_LOAD_ADDRESS; */
    
    syb_uart_init();
    sprintf(debug_date, "%s %s", __TIME__, __DATE__);
    uart_printf(CLRSCR"\r\nHello world! ("__DATE__" "__TIME__")\r\n");
    uart_printf("stack: svc = 0x%08x, usr = 0x%08x, irq = 0x%08x, fiq = 0x%08x\r\n", 
        svc_stack, usr_stack, irq_stack, fiq_stack);
    uart_printf("CPU speed = %ld MHz\r\n", (INT32)em86xx_getclockmhz());
    uart_printf("sys clock = %ld \r\n\r\n", (INT32)em86xx_getclock());

#ifdef TW_LITEONIT
        //added by Paul for HW mute workaround
        __raw_writel(0x00100010, SYS_gpio_data);
    uart_printf("Paul set up GPIO4 to high mute \r\n\r\n");
    //~added by Paul for HW mute workaround
#endif /* TW_LITEONIT */

/*
    debug_outs("sizeof(INT32) = %ld\r\n"
               "sizeof(INT32) = %ld\r\n"
               "sizeof(INT16) = %ld\r\n"
               "sizeof(void *) = %ld\r\n"
               "sizeof(CHAR) = %ld\r\n",
               sizeof(INT32), sizeof(INT32), sizeof(INT16),
               sizeof(void *), sizeof(CHAR));
    debug_outs("Endian test: ");
    test = 0x11223344;
    dump((CHAR*)&test, 4);
    debug_outs("\r\n");
*/
    boot_config();
         
    OSInit();
    debug_outs("OSInit done.\r\n");
    ARM_init();
    debug_outs("ARM_init done.\r\n");

#if 0
    {
        int i, j;
                    
        uart_printf("Copying 4GB from non-cached to non-cached\r\n");
//        tick_start = timer_getticks();
        for (i = 0; i < 10; ++i) {
            uart_putc('.');
            for (j = 0; j < 50; ++j)
                memcpy((void *) 0x10800000, (void *) 0x10000000, 0x00800000);
        }

        uart_printf("Copying 4GB from cached to cached\r\n");

        for (i = 0; i < 10; ++i) {
            uart_putc('.');
            for (j = 0; j < 50; ++j)
                memcpy((void *) 0x90800000, (void *) 0x90000000, 0x00800000);
        }


        uart_printf("Copying 4GB from non-cached to cached\r\n");
        for (i = 0; i < 10; ++i) {
            uart_putc('.');
            for (j = 0; j < 50; ++j)
                memcpy((void *) 0x90800000, (void *) 0x10000000, 0x00800000);
        }

    }    
#endif

    init_first_task();
    debug_outs("init_first_task done.\r\n");

    {
        extern void init_netdisk_test_task(void);
        init_netdisk_test_task();
    }

    OSStart();

    return 0;
}

