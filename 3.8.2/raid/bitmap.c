#include "xplatcfg.h"
#ifdef XPLAT_RAID
#include <sal/types.h>
#include <sal/libc.h>
#include <sal/debug.h>
#include <sal/mem.h>
#include <xlib/xmem.h>
#include "raid/bitmap.h"

#define BITMAP_MAX_SIZE (1024*1024)

#define DEBUG_LEVEL_BITMAP 1

#ifdef DEBUG
#define    dbg_bm(l, x...)    do {\
    if(l <= DEBUG_LEVEL_BITMAP) {     \
        sal_debug_print("BM|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)
#else
#define dbg_bm(l, x...) do {} while(0)
#endif

bitmap_t* bitmap_create(int size)
{
    int i, sz;
    bitmap_t* ret;
    
    if ( BITMAP_MAX_SIZE < size ) 
    {
        sal_assert( BITMAP_MAX_SIZE >= size);
        return NULL;
    }
    
    sz = sizeof(bitmap_t) + (size + BITMAP_CHUNK_SIZE - 1) / BITMAP_CHUNK_SIZE * sizeof(char *);
    ret = sal_malloc(sz);
    if ( !ret ) return NULL;
    dbg_bm(5, "ret=%p, %p size=%d", ret, ret+sz,sz);
    sal_memset(ret, 0, sz);
    ret->nr_chunks = (size + BITMAP_CHUNK_SIZE - 1) / BITMAP_CHUNK_SIZE;
    dbg_bm(5, "nr_chunks=%d", ret->nr_chunks);
    ret->chunk_size = BITMAP_CHUNK_SIZE;

    for ( i = 0; i < ret->nr_chunks; i++)
    {
        ret->chunks[i] = sal_malloc(ret->chunk_size);
        dbg_bm(5, "chunk[%d]=%p", i , ret->chunks[i]);
        if ( !ret->chunks[i] )
            goto out;
    }
    return ret;
out:
    bitmap_destroy(ret);
    return NULL;
    
}

void bitmap_destroy(bitmap_t* bitmap) 
{
    int i;
    dbg_bm(3, "ing bitmap=%p", bitmap);
    for ( i = 0; i < bitmap->nr_chunks ; i++)
    {
        dbg_bm(5, "chunk[%d]=%p", i, bitmap->chunks[i]);
         if ( bitmap->chunks[i] ) {
             sal_free(bitmap->chunks[i]);
            bitmap->chunks[i] = NULL;
         }
    }
    dbg_bm(5, "bitmap=%p", bitmap);
    sal_free(bitmap);
    dbg_bm(3, "ed");
}


#endif

