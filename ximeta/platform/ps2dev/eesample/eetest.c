/**********************************************************************
 * Copyright ( C ) 2002-2005 XIMETA, Inc.
 * All rights reserved.
 **********************************************************************/

#include <tamtypes.h>
#include <kernel.h>
#include <sifrpc.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <stdio.h>
#include <string.h>
#include "libpad.h"
#include "ps2ip.h"
#include "../irx/ndrpccmd.h"

#define ROM_PADMAN

// Change these to fit your network configuration
#define IPADDR "192.168.0.80"
#define NETMASK "255.255.255.0"
#define GWADDR "192.168.0.1"

// Change this path to reflect your setup
#define HOSTPATH "host:"

#define RPC_BUFFER_LEN 512 * 4

/* defined in graph.c */
extern void init_scr();
extern void scr_printf(const char *format, ...);
extern void scr_clear(void);

static int _init_check = 0;
static SifRpcClientData_t _ps2netdisk;
static int _rpc_buffer[RPC_BUFFER_LEN/sizeof(int)] __attribute__((aligned(64)));
static int _intr_data[32] __attribute__((aligned(64)));

int ps2netdisk_rpc_init(void)
{
    int i;

    memset(&_ps2netdisk, 0, sizeof(_ps2netdisk));

    while(1)
    {
        if(SifBindRpc(&_ps2netdisk, PS2NETDISK_IRX, 0) < 0)
            return -1;

        if(_ps2netdisk.server != 0) 
            break;

        i = 0x10000;
        while(i--);
    }
 
    _init_check = 1;
    
    return 0;    
}

/**
 * ps2netdisk_read_ioplog - polling for log generated on IOProcessor.
 */
int ps2netdisk_read_ioplog(char* buf, int len)
{
    char* dataptr;
    _rpc_buffer[0] = len;
    SifWriteBackDCache(&_rpc_buffer, sizeof(_rpc_buffer));        
    SifCallRpc(&_ps2netdisk, PS2NETDISK_CMD_READ_LOG, 0, (void*)_rpc_buffer, 4, (void*)_rpc_buffer, 4 + ((len+3)/4) * 4, 0, 0);
    dataptr = (char*)UNCACHED_SEG(&(_rpc_buffer[1]));
    if (_rpc_buffer[0]) {        
        memcpy(buf, &(_rpc_buffer[1]), _rpc_buffer[0]);
        buf[_rpc_buffer[0]] = 0;
    }
    return _rpc_buffer[0];
}
/**
 * ps2netdisk_fat_test - send the RCP request for fat file system 
 */
void ps2netdisk_fat_test(void)
{
    SifCallRpc(&_ps2netdisk, PS2NETDISK_CMD_FAT_TEST, 0, (void*)_rpc_buffer, 0, (void*)_rpc_buffer, 0, 0, 0);
}
/**
 * ps2netdisk_misc_test - send the RPC request for test
 */
int ps2netdisk_misc_test(void)
{
    SifCallRpc(&_ps2netdisk, PS2NETDISK_CMD_MISC_TEST, 0, (void*)_rpc_buffer, 0, (void*)_rpc_buffer, 0, 0, 0);
    return 0;
}
void ps2netdisk_probe_device(void) 
{
    SifCallRpc(&_ps2netdisk, PS2NETDISK_CMD_PROBE_TEST, 0, (void*)_rpc_buffer, 0, (void*)_rpc_buffer, 0, 0, 0);
}    

static int v_hold_debug_output=0;
extern void _putchar(int x, int y, u32 color, u8 ch);

void ps2netdisk_hold_debug_output(void)
{
    v_hold_debug_output = !v_hold_debug_output;
    if (v_hold_debug_output) {
        _putchar(76*7, 0, 0xFF0000, 'H');
        _putchar(77*7, 0, 0xFF0000, 'O');
        _putchar(78*7, 0, 0xFF0000, 'L');
        _putchar(79*7, 0, 0xFF0000, 'D');
    } else {
        _putchar(76*7, 0, 0xFF0000, ' ');
        _putchar(77*7, 0, 0xFF0000, ' ');
        _putchar(78*7, 0, 0xFF0000, ' ');
        _putchar(79*7, 0, 0xFF0000, ' ');
    }
}

struct button_cmd_map_s {
    u32 button;
    void (*cmd)(void);    
} button_cmd_map[] = {
    {PAD_CROSS, ps2netdisk_hold_debug_output},
    {PAD_CIRCLE, ps2netdisk_probe_device},
    {PAD_TRIANGLE, scr_clear},
    {PAD_SQUARE, ps2netdisk_fat_test}
};

void ps2netdisk_handle_button(void)
{
    int i;
    struct button_cmd_map_s* cmd;
    u32 paddata;
     static u32 old_pad = 0;
    u32 new_pad;
    struct padButtonStatus buttons;
    int ret;
    
    ret = padRead(0, 0, &buttons); 
    paddata = 0xffff ^ ((buttons.btns[0] << 8) | 
                              buttons.btns[1]);
                
    new_pad = paddata & ~old_pad;
    old_pad = paddata;

    for(i=0;i<sizeof(button_cmd_map)/sizeof(button_cmd_map[0]);i++) {
        cmd = &button_cmd_map[i];
        if (new_pad & cmd->button) {
            cmd->cmd();
        }
    }
}

static int busy_delay_count;

void busy_delay(int t)
{
    int i;
    int j;
    for(j=0;j<t*1000;j++) {
        for(i=0;i<1000;i++) {
            busy_delay_count++;
        }
    }
}


static char padBuf[256] __attribute__((aligned(64)));
static char actAlign[6];
static int actuators;
/**
 * waitPadReady
 */
int waitPadReady(int port, int slot)
{
    int state;
    int lastState;
    char stateString[16];

    state = padGetState(port, slot);
    lastState = -1;
    while((state != PAD_STATE_STABLE) && (state != PAD_STATE_FINDCTP1)) {
        if (state != lastState) {
            padStateInt2String(state, stateString);
  //          scr_printf("Please wait, pad(%d,%d) is in state %s\n", 
    //                   port, slot, stateString);
        }
        lastState = state;
        state=padGetState(port, slot);
    }
    // Were the pad ever 'out of sync'?
    if (lastState != -1) {
        scr_printf("Pad OK!\n");
    }
    return 0;
}


int
initializePad(int port, int slot)
{

    int ret;
    int modes;
    int i;

    waitPadReady(port, slot);

    // How many different modes can this device operate in?
    // i.e. get # entrys in the modetable
    modes = padInfoMode(port, slot, PAD_MODETABLE, -1);
//    scr_printf("The device has %d modes\n", modes);

    if (modes > 0) {
//        scr_printf("( ");
        for (i = 0; i < modes; i++) {
 //           scr_printf("%d ", padInfoMode(port, slot, PAD_MODETABLE, i));
        }
   //     scr_printf(")");
    }

 //   scr_printf("It is currently using mode %d\n", 
   //            padInfoMode(port, slot, PAD_MODECURID, 0));

    // If modes == 0, this is not a Dual shock controller 
    // (it has no actuator engines)
    if (modes == 0) {
     //   scr_printf("This is a digital controller?\n");
        return 1;
    }

    // Verify that the controller has a DUAL SHOCK mode
    i = 0;
    do {
        if (padInfoMode(port, slot, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
            break;
        i++;
    } while (i < modes);
    if (i >= modes) {
       // scr_printf("This is no Dual Shock controller\n");
        return 1;
    }

    // If ExId != 0x0 => This controller has actuator engines
    // This check should always pass if the Dual Shock test above passed
    ret = padInfoMode(port, slot, PAD_MODECUREXID, 0);
    if (ret == 0) {
    //    scr_printf("This is no Dual Shock controller??\n");
        return 1;
    }

//    scr_printf("Enabling dual shock functions\n");

    // When using MMODE_LOCK, user cant change mode with Select button
    padSetMainMode(port, slot, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK);

    waitPadReady(port, slot);
  //  scr_printf("infoPressMode: %d\n", padInfoPressMode(port, slot));

    waitPadReady(port, slot);        
 //   scr_printf("enterPressMode: %d\n", padEnterPressMode(port, slot));

    waitPadReady(port, slot);
    actuators = padInfoAct(port, slot, -1, 0);
  //  scr_printf("# of actuators: %d\n",actuators);

    if (actuators != 0) {
        actAlign[0] = 0;   // Enable small engine
        actAlign[1] = 1;   // Enable big engine
        actAlign[2] = 0xff;
        actAlign[3] = 0xff;
        actAlign[4] = 0xff;
        actAlign[5] = 0xff;

        waitPadReady(port, slot);
 //       scr_printf("padSetActAlign: %d\n", 
  //                 padSetActAlign(port, slot, actAlign));
    }
    else {
  //      scr_printf("Did not find any actuators.\n");
    }

    waitPadReady(port, slot);

    return 1;
}

/**
 * init_modules - load IRX modules
 */
static void init_modules(void)
{
    char args[LF_ARG_MAX];
    int argsLen;
    int ret;    
    
    SifInitRpc(0);

    SifLoadModule(HOSTPATH "iomanX.irx", 0, NULL);
    SifLoadModule(HOSTPATH "ps2dev9.irx", 0, NULL);
    SifLoadModule(HOSTPATH "ps2ip.irx", 0, NULL);

    memset(args, 0, LF_ARG_MAX);
    argsLen = 0;
    // Make smap argument list. Each argument is a null-separated string.
    // Argument length is the total length of the list, including null chars
    strcpy(args, IPADDR);
    argsLen += strlen(IPADDR) + 1;
    strcpy(&args[argsLen], NETMASK);
    argsLen += strlen(NETMASK) + 1;
    strcpy(&args[argsLen], GWADDR);
    argsLen += strlen(GWADDR) + 1;

    SifLoadModule(HOSTPATH "ps2smapx.irx", argsLen, args); // modified by sjcho
    SifLoadModule(HOSTPATH "ps2ips.irx", 0, NULL);

    if(ps2ip_init() < 0) {
        scr_printf("ERROR: ps2ip_init falied!\n");
        SleepThread();
    }

    scr_printf("Loading netdisk iop module.\n");

    ret = SifLoadModule(HOSTPATH "ps2ndas.irx", 0, NULL); 
    if (ret<0) {
        printf("Failed to load ps2ndas.irx\n");
        SleepThread();
    }    
    
#ifdef ROM_PADMAN
    ret = SifLoadModule("rom0:SIO2MAN", 0, NULL);
#else
    ret = SifLoadModule("rom0:XSIO2MAN", 0, NULL);
#endif
    if (ret < 0) {
        scr_printf("sifLoadModule sio failed: %d\n", ret);
        SleepThread();
    }    

#ifdef ROM_PADMAN
    ret = SifLoadModule("rom0:PADMAN", 0, NULL);
#else
    ret = SifLoadModule("rom0:XPADMAN", 0, NULL);
#endif 
    if (ret < 0) {
        scr_printf("sifLoadModule pad failed: %d\n", ret);
        SleepThread();
    }    
    padInit(0);
    if((ret = padPortOpen(0, 0, padBuf)) == 0) {
        scr_printf("padOpenPort failed: %d\n", ret);
        SleepThread();
    }
    if(!initializePad(0, 0)) {
        printf("pad initalization failed!\n");
        SleepThread();
    } 
    
}
/**
 * main - the entry point of the program that runs on EmotionalEngine
 */
int main(void)
{

    int ret;
    char logbuf[2048];
    init_scr();
    scr_printf("EE main starting\n");

    init_modules();
    scr_printf("Initializing ps2netdisk\n");

    ps2netdisk_rpc_init();

    while(1) {
        ps2netdisk_handle_button();

        if (!v_hold_debug_output) {
            ret = ps2netdisk_read_ioplog(logbuf, 2048-1);
            if (ret) {
                scr_printf("%s", logbuf);
            } 
        }
        busy_delay(5);
    }

    scr_printf("Exiting eetest main\n");
    SleepThread();
    return 0;
}

