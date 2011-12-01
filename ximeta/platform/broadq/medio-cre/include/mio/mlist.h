
#ifndef __MIO_LIST_H__
#define __MIO_LIST_H__

#if defined(X__IOP__)

template<class T>
class mlist {
    typedef T type;
    typedef T* typePtr;
public:
    mlist();
    ~mlist();

    void push_back(typePtr);
    void remove(typePtr);
};

#else
#include <list>
#define mlist list
#endif

#endif // __MIO_LIST_H__
