#ifndef __MIO_CIRCULAR_BUFFER_H__
#define __MIO_CIRCULAR_BUFFER_H__
#include <mio/buffer.h>

class MioCircularBuffer : public MioBuffer {
public:
    MioCircularBuffer(int size, void *buffer = 0);
    ~MioCircularBuffer();
    virtual int readyData() const;
    virtual int availableRoom() const;
    virtual int insertData(MioBufferDesc& user);
    virtual int insertData(void *buffer, int count);
    virtual int removeData(MioBufferDesc& user);
    virtual int removeData(void *buffer, int count);
    virtual const char *asString() const;
    virtual int readyDataContig() const;
    virtual int availableRoomContig() const;
    virtual MioBufferDesc getContigDataArea();
    virtual MioBufferDesc refill();
    virtual void empty();
    virtual int isEmpty() const;

protected:
    int myBufferWasAllocated;
    char *myBuffer;
    int myIn;
    int myOut;
    int myLimit;
};

#endif // __MIO_CIRCULAR_BUFFER_H__
