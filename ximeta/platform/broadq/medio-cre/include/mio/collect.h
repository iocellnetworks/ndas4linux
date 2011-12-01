
#ifndef __MIO_COLLECTION_H__
#define __MIO_COLLECTION_H__

class MioCollectable;

class MioCollection {
public:
    virtual void insert(MioCollectable *item) = 0;
    virtual void remove(MioCollectable *item) = 0;
    virtual int entries() const = 0;
    virtual int isEmpty() const = 0;
};


#endif // __MIO_COLLECTION_H__
