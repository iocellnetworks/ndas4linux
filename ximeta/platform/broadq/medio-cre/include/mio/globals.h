#ifndef GLOBALS_H
#define GLOBALS_H
#include <_ansi.h>
#include <tamtypes.h>
#include <mio/trace.h>

typedef enum {
    QCAST_KERNEL = 0,
    MEDIO_KERNEL_1,
} MioKernelVersion;


typedef struct {            // BEWARE: this structure parallels that in bridge.s
    int vmode;                // video mode selection
    int sloader_lang;        // selected language index
    void *font_data_base;
    int display_xoffset;    // x offset for screen centering
    int display_yoffset;    // y offset for screen centering
    const char *oemName;    // name of OEM (used in finding UI components)
    u64 now;                // microseconds since boot
    TracePackage* masterTrace;
    int mioIOPTraceFD;        // FD used for IOP trace files
    MioKernelVersion kernelVersion;        // version of kernel
    void *ukernelStart;        // address of ukernel start
    int sloader_version;
    int padAccept;            // character used for "accept" function
    int padCancel;            // character used for "cancel" function
    int screenwx;            // screen width
    int screenwy;            // screen height
    void (*sleepThread)(int delay);    // function to delay calling thread by "delay" ms
    int    loadingThreadID;    // thread ID of loading thread
} globals_data_t;

#ifdef LIBSTUB_SLOADER_SIDE
extern globals_data_t _globals_data;
#endif
extern globals_data_t *globals_data;

_BEGIN_STD_C
extern int mioPrintf(const char *format, ...);
_END_STD_C


#endif
