
#ifndef __SINGLE_LINKED_LIST_H__
#define __SINGLE_LINKED_LIST_H__
#include <mio/list.h>

class MioSingleLink {
public:
    MioSingleLink *myNext;
};

class MioSingleLinkedList {
public:
    MioSingleLinkedList();
    virtual ~MioSingleLinkedList();

    virtual void insert(MioSingleLink *item);
    virtual void insertAtEnd(MioSingleLink *item);
    virtual void insertAtFront(MioSingleLink *item);
    virtual void insertAfter(MioSingleLink *item, MioSingleLink *oldItem);
    virtual void insertBefore(MioSingleLink *item, MioSingleLink *oldItem);
    virtual void remove(MioSingleLink *item);
    virtual int isEmpty() const;
    virtual int entries() const;
    virtual MioSingleLink* head() const;
    virtual const char *asString(int deep = 0) const;

protected:
    MioSingleLink *myHead;
};

#endif // __SINGLE_LINKED_LIST_H__
