#ifndef __MIO_TEXT_STREAM_H__
#define __MIO_TEXT_STREAM_H__
#include "bufferdesc.h"
#include "buffer.h"


class MioTextStream {
public:
    MioTextStream(MioBuffer& buffer, char delim = '\n');
    ~MioTextStream();
    void insertData(MioBufferDesc& user);
    void insertData(void *buffer, int count);
    int getLine(MioBufferDesc& user);
    int getLinesAvailable() const;

private:
    MioBuffer& myBuffer;
    int myLinesAvailable;
    char myDelim;
};

#endif // __MIO_TEXT_STREAM_H__
