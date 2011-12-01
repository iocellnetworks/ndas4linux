#ifndef __MIO_DELIMITTED_BUFFER_H__
#define __MIO_DELIMITTED_BUFFER_H__
#include <mio/buffer.h>
#include <mio/slist.h>

class MioDelimittedRecord : public MioBufferDesc, public MioSingleLink {
public:
    MioDelimittedRecord(int maxRecordSize);
    ~MioDelimittedRecord();
    const char *asString() const;
    void *operator new(unsigned int, void*);

protected:
    MioDelimittedRecord *next() const;
};

class MioDelimittedBuffer : public MioBuffer {
public:
    MioDelimittedBuffer(char delim, int maxRecords, int maxRecordSize);
    ~MioDelimittedBuffer();
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
    MioSingleLinkedList myAvail;
    MioSingleLinkedList myFree;
    MioDelimittedRecord *myCurrent;
    char *myPool;
    char myDelim;
};

#endif // __MIO_DELIMITTED_BUFFER_H__
