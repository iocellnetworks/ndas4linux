#ifndef __MIO_BUFFER_H__
#define __MIO_BUFFER_H__
#include <mio/bufferdesc.h>

class MioBuffer;
typedef void (*MioBufferEmptierHandler)(MioBuffer& buffer, void *cookie);
typedef void (*MioBufferRefillHandler)(MioBuffer& buffer, void *cookie);

class MioBuffer {
public:
    MioBuffer();
    virtual ~MioBuffer();
    virtual int readyData() const = 0;
    virtual int availableRoom() const = 0;
    virtual int insertData(MioBufferDesc& user) = 0;
    virtual int insertData(void *buffer, int count) = 0;
    virtual int removeData(MioBufferDesc& user) = 0;
    virtual int removeData(void *buffer, int count) = 0;
    virtual const char *asString() const = 0;
    virtual int readyDataContig() const = 0;
    virtual int availableRoomContig() const = 0;
    virtual MioBufferDesc getContigDataArea() = 0;
    virtual void setRefillHandler(MioBufferRefillHandler, void *cookie);
    virtual void setEmptierHandler(MioBufferEmptierHandler, void *cookie);
    virtual MioBufferDesc refill() = 0;
    virtual void empty() = 0;
    virtual int isEmpty() const = 0;

protected:
    void *myRefillCookie;
    void *myEmptierCookie;
    MioBufferRefillHandler myRefillHandler;
    MioBufferEmptierHandler myEmptierHandler;
};

#endif // __MIO_BUFFER_H__
