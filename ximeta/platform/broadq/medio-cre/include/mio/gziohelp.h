
#ifndef __MIO_GZ_FILE_IO_HELPER_H__
#define __MIO_GZ_FILE_IO_HELPER_H__
#include <tamtypes.h>
#include <mio/fileiohelp.h>
#include <string>
#include <zlib.h>
using namespace std;

class MioGZFileIOHelper : public MioFileIOHelper {
public:
    MioGZFileIOHelper(const string& filename);
    virtual ~MioGZFileIOHelper();

    virtual int isGood() const;
    virtual int read(u64 offset, void *buffer, u32 size);

private:
    gzFile myFile;
    u64 myOffset;
};

#endif //  __MIO_GZ_FILE_IO_HELPER_H__
