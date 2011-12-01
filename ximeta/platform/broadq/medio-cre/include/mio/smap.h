
#ifndef __MIO_SIMPLE_MAP_H__
#define __MIO_SIMPLE_MAP_H__


template<class T, class K>
class smap {
    typedef T type;
    typedef K key;
    typedef T* typePtr;
public:
    smap<T,K>(int mapSize);
    ~smap<T,K>();

    typePtr find(const K&) const;
    typePtr operator[](const key&) const;

    typePtr insert(typePtr);
    void remove(const key&);

protected:
    int indexof(const key&) const;
    typePtr *myEntries;
    int myMapSize;
};

#include <mio/smap.cc>

#endif // __MIO_SIMPLE_MAP_H__
