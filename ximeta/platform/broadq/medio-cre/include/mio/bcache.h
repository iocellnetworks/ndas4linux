
#ifndef __BUFFER_CACHE_H__
#define __BUFFER_CACHE_H__
#include <mio/cdlist.h>

typedef struct _MioBufferCacheEntry {
    u64 externalByteAddress; // byte address
    void *ramAddress;
    u32 blockSize; // in bytes
} MioBufferCacheEntry;

typedef struct _MioBufferCacheListEntry {
    MioDoubleLink links;
    MioBufferCacheEntry entry;
} MioBufferCacheListEntry;

typedef struct _MioBufferCache {
    MioDoublyLinkedList list;
    MioBufferCacheListEntry *pool;
} MioBufferCache;

extern void MioBufferCacheEntryInit(MioBufferCacheEntry *this);
extern void MioBufferCacheEntryTerm(MioBufferCacheEntry *this);
extern void MioBufferCacheEntryPrint(MioBufferCacheEntry *this, const char *prefix);
extern const char *MioBufferCacheEntryAsString(MioBufferCacheEntry *this, char *buffer, int bufferSize);

extern void MioBufferCacheInit(MioBufferCache *this, int cacheSize);
extern void MioBufferCacheAddEntry(MioBufferCache *this, MioBufferCacheEntry *add);
extern int MioBufferCacheContains(MioBufferCache *this, u64 logicalUserAddress, MioBufferCacheEntry *found);
extern void MioBufferCacheTerm(MioBufferCache *this);
extern void MioBufferCachePrint(MioBufferCache *this, const char *prefix);

#endif // __BUFFER_CACHE_H__
