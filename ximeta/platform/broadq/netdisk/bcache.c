
/**        bcache.c - Simple Buffer Cache Mechanism.
 *
 *        @author        t.j. davidson
 *
 *        This module contains the implmentation for a simple caching mechanism for disk
 *        blocks. It uses a least frequently used policy and when full, will drop that entry.
 *        The algorithm is not exposed the caller, thus facilitating changes without resulting
 *        in external interface modifications.
 */

#include <tamtypes.h>
#include <mio/bcache.h>
#include <mio/modules.h>
#include <mio/utils.h>

#define DelayThread delay

//        Statics, globals, externs etc.

static MioModule thisModule = {MIO_MODULE_BCACHE_C, __FILE__, 0};


/**        MioBufferCacheInit - Initialize buffer cache object.
 *
 *        This function (constructor) simply initializes a buffer
 *        cache object.
 *
 *        @param    this        Address of buffer cache object.
 *        @param    cacheSize    Number of entries to allocate in the buffer cache.
 */

void
MioBufferCacheInit(MioBufferCache *this, int cacheSize)
{
    int i;
    MioBufferCacheListEntry *pool;

    if (MioModuleIsTracing(&thisModule)) {
        printf("%s: Initializing buffer cache at %#x with %u entries.",
            "bcache.c", this, cacheSize);
    }

    bzero(this, sizeof(*this));

    // create a pool of cache entries all at once for ease of allocation/deallocation.

    this->pool = malloc(sizeof(MioBufferCacheListEntry) * cacheSize);
    if (0 == this->pool) {
        printf("buffer cache: no more memory, size: %u\n", cacheSize);
        return;
    }

    bzero(this->pool, sizeof(MioBufferCacheListEntry) * cacheSize);

    // now, add them to our list

    for (i = 0; i < cacheSize; i++) {
        MioDoublyLinkedListInsertAtFront(&this->list, &this->pool[i].links);
    }
}


/**        MioBufferCacheTerm - Destroy a buffer cache object.
 *
 *        This function (destructor) destroys a buffer cache object and
 *        frees the resources contained.
 *
 *        @param    this        Address of buffer cache object.
 */

void
MioBufferCacheTerm(MioBufferCache *this)
{
    if (MioModuleIsTracing(&thisModule)) {
        printf("%s: Destroying buffer cache at %x", __FILE__, this);
    }

    if (this->pool) free(this->pool);
    bzero(this, sizeof(*this));
}


/**        MioBufferCacheAddEntry - Add an entry to the buffer cache.
 *
 *        This function adds a buffer cache entry to the buffer cache.
 *        Since the list is maintained in order or access, the last one
 *        will be used to house the new entry and will be inserted at the
 *        front.
 *
 *        @param    this    Address of buffer cache object.
 *        @param    add        Buffer cache entry to add.
 */

void
MioBufferCacheAddEntry(MioBufferCache *this, MioBufferCacheEntry *add)
{
    MioBufferCacheListEntry *object = (MioBufferCacheListEntry*)this->list.tail;

    if (MioModuleIsTracing(&thisModule)) {
        char buffer[100];
        printf("%s: Add buffer cache[0x%x] entry: %s",
            __FILE__, this, MioBufferCacheEntryAsString(&object->entry, buffer, sizeof(buffer)));
    }

    // remove the oldest one which is at the end

    MioDoublyLinkedListRemove(&this->list, &object->links);
    MioBufferCacheEntryTerm(&object->entry); // don't need it anymore

    // insert the new one at the front

    object->entry = *add; // update its contents

    MioDoublyLinkedListInsertAtFront(&this->list, &object->links);
}



/**        MioBufferCacheContains - Check if an object is in the buffer cache.
 *
 *        This function scans the buffer cache to see if the object corresponding to
 *        the first byte of the user address is present in the cache.
 *
 *        @param    this        Address of buffer cache object.
 *        @param    userAddress    First address sought by the caller.
 *        @param    found        Updated to found cache entry (see return).
 *
 *        @return    1            If cache entry was found.
 */

int
MioBufferCacheContains(MioBufferCache *this, u64 userAddress, MioBufferCacheEntry *found)
{
    MioDoubleLink *e = this->list.head;

    while (e) { // walk the buffer cache
        MioBufferCacheListEntry *object = (MioBufferCacheListEntry*)e;
        if (0 != object->entry.ramAddress) { // if valid entry...
            if (userAddress >= object->entry.externalByteAddress
                && userAddress < (object->entry.externalByteAddress + object->entry.blockSize)) {
                *found = object->entry; // give a copy back to the caller
                if (MioModuleIsTracing(&thisModule)) {
                    char buffer[100];
                    //printf("%s: buffer cache[0x%x] hit: %s",
                        //__FILE__, this, MioBufferCacheEntryAsString(&object->entry, buffer, sizeof(buffer)));
                }

                // since it was just accessed, move to the front

                if (this->list.head != e) { // not needed if we are the first one already
                    MioDoublyLinkedListRemove(&this->list, e);
                    MioDoublyLinkedListInsertAtFront(&this->list, e);
                }
                return 1;
            }
        }
        e = e->next;
    }

    return 0; // sorry, no hit
}


/**        MioBufferCachePrint - Print the buffer cache map.
 *
 *        This function prints the contents of the buffer cache.
 *
 *        @param    this        Address of buffer cache object.
 *        @param    prefix        Character string prefix for each line printed.
 */

void
MioBufferCachePrint(MioBufferCache *this, const char *prefix)
{
    MioDoubleLink *e = this->list.head;

    while (e) { // walk the buffer cache
        char pr[100];
        MioBufferCacheListEntry *object = (MioBufferCacheListEntry*)e;
        sprintf(pr, "%s[0x%x]= ", prefix, e);
        MioBufferCacheEntryPrint(&object->entry, pr);
        e = e->next;
    }
}


/**        MioBufferCacheEntryInit - Initialize a buffer cache entry.
 *
 *        This function initializes an entry in the buffer cache.
 *
 *        @param    this        Address of buffer cache entry object.
 */

void
MioBufferCacheEntryInit(MioBufferCacheEntry *this)
{
    bzero(this, sizeof(*this));
}


/**        MioBufferCacheEntryTerm - Destroy a buffer cache entry.
 *
 *        This function destroys a buffer cache entry. It does not
 *        release its memory.
 *
 *        @param    this        Address of buffer cache entry object.
 */

void
MioBufferCacheEntryTerm(MioBufferCacheEntry *this)
{
    if (this->ramAddress) free(this->ramAddress);
    bzero(this, sizeof(*this)); // leave no traces to confuse anyone
}



/**        MioBufferCacheEntryPrint - Print a buffer cache entry.
 *
 *        This function prints the contents of a single buffer cache entry.
 *
 *        @param    this        Address of buffer cache entry object.
 *        @param    prefix        Character string prefix for each line printed.
 */

void
MioBufferCacheEntryPrint(MioBufferCacheEntry *this, const char *prefix)
{
    char buffer[100];

    printf("%s%s", prefix, MioBufferCacheEntryAsString(this, buffer, sizeof(buffer)));
}



/**        MioBufferCacheEntryAsString - Format a buffer cache entry as a string.
 *
 *        This function formats the contents of a single buffer cache entry.
 *
 *        @param    this        Address of buffer cache entry object.
 *        @param    prefix        Character string prefix for each line printed.
 */

const char*
MioBufferCacheEntryAsString(MioBufferCacheEntry *this, char *buffer, int bufferSize)
{
    snprintf(buffer, bufferSize, "offset: %lld, size: %d, ram: 0x%x",
        this->externalByteAddress, this->blockSize, this->ramAddress);
    return buffer;
}
