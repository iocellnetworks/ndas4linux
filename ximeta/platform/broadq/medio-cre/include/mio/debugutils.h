
#ifndef __MIO_DEBUG_UTILS_H__
#define __MIO_DEBUG_UTILS_H__
#include <mio/trace.h>

class MioDebugUtils {
public:
    static void regionScan(const void *fwa, unsigned int size);
    static void showTrace(TracePackage*);
    static void grdebug(int slot);
    static void grdebug2(int slot);
};

#endif // __MIO_DEBUG_UTILS_H__
