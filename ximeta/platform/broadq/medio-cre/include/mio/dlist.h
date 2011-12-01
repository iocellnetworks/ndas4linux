
#ifndef __DOUBLY_LINKED_LIST_H__
#define __DOUBLY_LINKED_LIST_H__
//#include <mio/list.h>

class MioDoubleLink {
public:
    MioDoubleLink *myPrev;
    MioDoubleLink *myNext;
};


class MioDoublyLinkedList : public MioList {
public:
    MioDoubleLink *head;
    MioDoubleLink *tail;
};

extern void MioDoublyLinkedListInit(MioDoublyLinkedList *this);
extern void MioDoublyLinkedListTerm(MioDoublyLinkedList *this);
extern void MioDoublyLinkedListInsertAtFront(MioDoublyLinkedList *this, MioDoubleLink *object);
extern void MioDoublyLinkedListInsertAtEnd(MioDoublyLinkedList *this, MioDoubleLink *object);
extern void MioDoublyLinkedListRemove(MioDoublyLinkedList *this, MioDoubleLink *object);

#endif // __DOUBLY_LINKED_LIST_H__
