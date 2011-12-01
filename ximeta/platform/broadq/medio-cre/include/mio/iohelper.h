
#ifndef __MIO_IO_HELPER_H__
#define __MIO_IO_HELPER_H__
#include <tamtypes.h>

class MioIOHelper {
public:
    MioIOHelper() { };
    virtual ~MioIOHelper() { };
    virtual int isGood() const = 0;
    virtual int read(u64 offset, void *buffer, u32 size) = 0;
};

#endif //  __MIO_IO_HELPER_H__
