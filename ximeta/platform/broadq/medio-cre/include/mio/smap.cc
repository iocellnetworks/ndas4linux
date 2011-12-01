
#include <stdlib.h>
#include "smap.h"

template<class T, class K>
smap<T,K>::smap(int mapSize)
    :    myMapSize(mapSize)
{
    myEntries = new typePtr[mapSize];
    for (int i = 0; i < myMapSize; i++) myEntries[i] = 0;
}

template<class T, class K>
smap<T,K>::~smap()
{
    // we just delete the list, not its contents...
    delete myEntries;
}

template<class T, class K>
T*
smap<T,K>::find(const key& k) const
{
    for (int d = 0; d < myMapSize; d++) {
        if (myEntries[d]) { // slot is used
            if (*myEntries[d] == k) return myEntries[d];
        }
    }
    return 0;
}

template<class T, class K>
T*
smap<T,K>::operator[](const key& k) const
{ return find(k); }

template<class T, class K>
int
smap<T,K>::indexof(const key& k) const
{
    for (int d = 0; d < myMapSize; d++) {
        if (myEntries[d]) { // slot is used
            if (*myEntries[d] == k) return d;
        }
    }
    return -1;
}

template<class T, class K>
T*
smap<T,K>::insert(typePtr data)
{
    int s = -1;
    for (int d = 0; d < myMapSize; d++) {
        if (myEntries[d]) {
            if (*data == *myEntries[d]) { // if found same device
                return myEntries[d];
            }
        } else {
            if (-1 == s) s = d; // save index of first unused slot
        }
    }

    if (-1 == s) { // if no empty slots
        // a nice implementation would allocate a new entry and add...
        return 0; // no room
    }
    myEntries[s] = data;
    return myEntries[s];
}

template<class T, class K>
void
smap<T,K>::remove(const key& k)
{
    int d = indexof(k);
    if (-1 == d) {
        // not found
        return;
    }
    myEntries[d] = 0;
    // a nicer implementation might check if the table can be shortened...
}
