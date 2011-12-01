
#ifndef __MIO_LIST_H__
#define __MIO_LIST_H__
#include <mio/collect.h>

class MioList : public MioCollection {
public:
    virtual void insertAfter(MioCollectable *newItem, MioCollectable *oldItem) = 0;
    virtual void insertAtEnd(MioCollectable *item) = 0;
    virtual void insertAtFront(MioCollectable *item) = 0;
    virtual void insertBefore(MioCollectable *newItem, MioCollectable *oldItem) = 0;
};


#endif // __MIO_LIST_H__
