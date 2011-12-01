
#ifndef __MIO_SIMPLE_FILE_IO_HELPER_H__
#define __MIO_SIMPLE_FILE_IO_HELPER_H__
#include <tamtypes.h>
#include <mio/fileiohelp.h>
#include <string>
using namespace std;

class MioSimpleFileIOHelper : public MioFileIOHelper {
public:
    MioSimpleFileIOHelper(const string& filename);
    virtual ~MioSimpleFileIOHelper();

    u32 lseek(u32 offset, int whence);

    virtual int isGood() const;
    virtual int read(u64 offset, void *buffer, u32 size);

private:
    int    myFd;
    u64    myOffset;
};

#endif //  __MIO_SIMPLE_FILE_IO_HELPER_H__
