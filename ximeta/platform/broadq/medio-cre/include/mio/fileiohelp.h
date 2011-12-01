
#ifndef __MIO_FILE_IO_HELPER_H__
#define __MIO_FILE_IO_HELPER_H__
#include <mio/iohelper.h>
#include <string>
using namespace std;

class MioFileIOHelper : public MioIOHelper {
public:
    MioFileIOHelper(const string& filename);
    virtual ~MioFileIOHelper();

    string getName() const { return myFilename; };

protected:
    string myFilename;
};

#endif // __MIO_FILE_IO_HELPER_H__
