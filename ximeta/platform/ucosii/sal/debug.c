#include <stdio.h>
#include <porting.h>
#include <sybdebug.h>
#include "sal/debug.h"
#include "sal/sync.h"
#include "sal/types.h"

#ifdef DEBUG
void sal_debug_hexdump(char* buf, int len)
{
    int i;
    for(i=0;i<len;i++) {
        if (i!=0 && i%16==0)
            sal_debug_print("\r\n");
        sal_debug_print("%02x", 0x0ff & buf[i]);
        if (i%2)
            sal_debug_print(" ");
    }
    sal_debug_print("\r\n");
}
#endif

