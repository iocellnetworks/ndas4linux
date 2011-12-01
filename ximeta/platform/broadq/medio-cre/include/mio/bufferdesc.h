#ifndef __MIO_BUFFER_DESCRIPTOR_H__
#define __MIO_BUFFER_DESCRIPTOR_H__

class MioBufferDesc {
public:
    MioBufferDesc(void *address = 0, int size = 0, int used = 0);
    ~MioBufferDesc();
    void *address() const;
    int size() const;
    int used() const;
    void *setAddress(void *address);
    int setSize(int size);
    int setUsed(int used);
    const char *asString() const;
    operator char*() const;

private:
    void *myAddress;
    int mySize;
    int myUsed;
};

#endif // __MIO_BUFFER_DESCRIPTOR_H__
