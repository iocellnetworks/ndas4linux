#ifndef __DOUBLY_LINKED_LIST_H__
#define __DOUBLY_LINKED_LIST_H__
#include <_ansi.h>

typedef struct _MioDoubleLink {
    struct _MioDoubleLink *prev;
    struct _MioDoubleLink *next;
} MioDoubleLink;

typedef struct _MioDoublyLinkedList {
    MioDoubleLink *head;
    MioDoubleLink *tail;
} MioDoublyLinkedList;

_BEGIN_STD_C
extern void MioDoublyLinkedListInit(MioDoublyLinkedList *self);
extern void MioDoublyLinkedListTerm(MioDoublyLinkedList *self);
extern void MioDoublyLinkedListInsertAtFront(MioDoublyLinkedList *self, MioDoubleLink *object);
extern void MioDoublyLinkedListInsertAtEnd(MioDoublyLinkedList *self, MioDoubleLink *object);
extern void MioDoublyLinkedListRemove(MioDoublyLinkedList *self, MioDoubleLink *object);
_END_STD_C

#endif // __DOUBLY_LINKED_LIST_H__
