#ifndef _BIND_BITMAP_H_
#define _BIND_BITMAP_H_

#include <netdisk/list.h>

#define BITMAP_CHUNK_SIZE (4*1024)

typedef struct bitmap_s {
    int chunk_size;
    int nr_chunks;
    char *chunks[0];
} bitmap_t;


void bitmap_destroy(bitmap_t* bitmap) ;

extern bitmap_t* bitmap_create(int size);

inline static
void bitmap_set_all(bitmap_t* bitmap) {
    int i,j;
    for ( i = 0; i < bitmap->nr_chunks;i++) 
        for ( j = 0; j < bitmap->chunk_size; j++)
            bitmap->chunks[i][j] = 0xff;
}

inline static
void bitmap_set(bitmap_t* bitmap, int idx) {
    int chunk_idx = (idx/8) / bitmap->chunk_size;
    int chunk_remain = (idx/8) % bitmap->chunk_size;

    bitmap->chunks[chunk_idx][chunk_remain] |= (1 << (idx%8));
}
inline static
char *bitmap_get(bitmap_t* bitmap, int idx) 
{
    int chunk_idx = (idx/8) / bitmap->chunk_size;
    int chunk_remain = (idx/8) % bitmap->chunk_size;
    return (char *) &bitmap->chunks[chunk_idx][chunk_remain];
}
    
inline static
void bitmap_reset(bitmap_t* bitmap, int idx) 
{
    int chunk_idx = (idx/8) / bitmap->chunk_size;
    int chunk_remain = (idx/8) % bitmap->chunk_size;

    bitmap->chunks[chunk_idx][chunk_remain] &= ~((1 << (idx%8)));
}

inline static
int bitmap_isset(bitmap_t* bitmap, int idx)
{
    int chunk_idx = (idx/8) / bitmap->chunk_size;
    int chunk_remain = (idx/8) % bitmap->chunk_size;
    return (bitmap->chunks[chunk_idx][chunk_remain] & (1 << (idx%8))) ? 1 : 0;
}
/* 
 * bitmap merge and find out if any bit of bitmap is set
 * @return : TRUE if any bit of bitmap is set
 */
inline static
xbool bitmap_merge(bitmap_t *dst, bitmap_t *src)
{
    xbool ret = FALSE;
    int i,j;
    if ( dst->nr_chunks < src->nr_chunks || dst->chunk_size != src->chunk_size )
        return FALSE;
    
    for (i = 0; i < dst->nr_chunks; i++ )
        for (j = 0; j < dst->chunk_size; j++)
            ret |= (dst->chunks[i][j] |= src->chunks[i][j]) != 0x0;
    return ret;
}

static inline unsigned long bitmap_ffb(unsigned long word)
{
    int b = 0, s;

    if ( sizeof(int) == 4 ) {
        s = 16; if (word << 16 != 0) s = 0; b += s; word >>= s;
        s =  8; if (word << 24 != 0) s = 0; b += s; word >>= s;
        s =  4; if (word << 28 != 0) s = 0; b += s; word >>= s;
        s =  2; if (word << 30 != 0) s = 0; b += s; word >>= s;
        s =  1; if (word << 31 != 0) s = 0; b += s;
    }
/*    
    if ( sizeof(int) == 8 ) {
        s = 32; if (word << 32 != 0) s = 0; b += s; word >>= s;
        s = 16; if (word << 48 != 0) s = 0; b += s; word >>= s;
        s =  8; if (word << 56 != 0) s = 0; b += s; word >>= s;
        s =  4; if (word << 60 != 0) s = 0; b += s; word >>= s;
        s =  2; if (word << 62 != 0) s = 0; b += s; word >>= s;
        s =  1; if (word << 63 != 0) s = 0; b += s;
    }
*/    
    return b;
}
inline static
xuint64 bitmap_first_set_index(bitmap_t* bitmap) 
{
    int i,j;
    for (i = 0; i < bitmap->nr_chunks; i++ )
        for (j = 0; j < bitmap->chunk_size; j++)
            if ( bitmap->chunks[i][j] ) {
                return ((xuint64)i)* bitmap->chunk_size * 8
                    + j * 8
                    + bitmap_ffb((unsigned long)bitmap->chunks[i][j]);
            }
            
    return 0xffffffffffffffffULL;
}

#endif //_BIND_BITMAP_H_
