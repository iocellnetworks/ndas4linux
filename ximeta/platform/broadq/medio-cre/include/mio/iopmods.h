
#ifndef __MIO_Iop_MODULES_H__
#define __MIO_Iop_MODULES_H__
#include <_ansi.h>

typedef struct {
    const char *name;
    void *fwa;
} MioIopModuleInfo;

_BEGIN_STD_C
_EXFUN(void registerIopModule,(const char *name, void *fwa));
_EXFUN(void unregisterIopModule,(const char *name));
_EXFUN(void findIopModule, (MioIopModuleInfo *info));
_END_STD_C

#endif // __MIO_Iop_MODULES_H__
