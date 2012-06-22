#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include "sal/debug.h"
#include "sal/sync.h"
#include "sal/types.h"


int SalErrorVprint(const char* fmt, va_list varg)
{
    return vfprintf(stderr, fmt, varg);
}

int sal_error_print(char* fmt, ...)
{
    va_list varg;
    int ret;
    va_start(varg, fmt);
    ret = SalErrorVprint(fmt, varg);
    va_end(varg);
    return ret;
}

#ifdef DEBUG
int SalDebugVprint(const char* fmt, va_list varg)
{
    vfprintf(stderr, fmt, varg);
    return 0;
}

int sal_debug_print(char* fmt, ...)
{
    int ret;
    va_list varg;
    va_start(varg, fmt);
    ret = SalDebugVprint(fmt, varg);
    va_end(varg);
    return ret;
}

int sal_debug_println_impl(const char* file, 
    int line, const char* function, const char* fmt, ...)
{
    int ret;
    va_list varg;
    va_start(varg, fmt);
    ret = SalDebugVprint(fmt, varg);
    va_end(varg);
//    fprintf(stderr, "|%s,%d,%s\n",file,line,function);
    fprintf(stderr, "|%s,%d\n", file, line);
    return ret;
}

void sal_debug_hexdump(char* buf, int len)
{
    int i;
    for(i=0;i<len;i++) {
        if (i!=0 && i%16==0)
            sal_debug_print("\n");
        sal_debug_print("%02x", 0x0ff & buf[i]);
        if (i%2)
            sal_debug_print(" ");
    }
    sal_debug_print("\n");
}
#endif

