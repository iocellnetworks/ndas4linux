
#ifndef __MIO_IOP_ARGS_H__
#define __MIO_IOP_ARGS_H__

class MioIopArgPacker {
public:
    MioIopArgPacker(unsigned int maxSize);
    ~MioIopArgPacker();

    unsigned int size() const;
    const char *addr() const;

    int insert(const char *arg);

private:
    unsigned int myNext;
    unsigned int myMaxSize;
    char myBuffer[200];
};

#endif // __MIO_IOP_ARGS_H__
