#include "xplatcfg.h"
#include "sal/libc.h"
#include "sal/mem.h"
#include "sal/debug.h"
#if defined(LINUX) && !defined(LINUXUM)
#define __USE_GNU
#endif
#include <pthread.h>

#define MEM_DEBUG 1

#ifdef MEM_DEBUG
#define MAX_NUM_MEM_BLOCKS 4096 //1024
#define MAX_NUM_MEM_SIZES  64

typedef struct
{
    void * addr;
    unsigned int size;
} MEM_BLOCK_ENTRY;

typedef struct
{
    int      count;
    int     accum_count;
    unsigned int  size;
} MEM_BLOCK_COUNTER;

static MEM_BLOCK_ENTRY v_mem_blocks[MAX_NUM_MEM_BLOCKS];
static MEM_BLOCK_COUNTER v_mem_counters[MAX_NUM_MEM_SIZES];
static int v_mem_total_allocated = 0;
static int v_mem_max_allocated = 0;
static int v_mem_alloc_count = 0; 

static void _SalMemAdjustSizeCounter(unsigned int size, int adj)
{
    int i;
    for(i=0;i<MAX_NUM_MEM_SIZES;i++) {
        if (v_mem_counters[i].size==0) { /* new entry */
            v_mem_counters[i].size = size; 
        } 
        if (v_mem_counters[i].size == size) {
            v_mem_counters[i].count += adj; 
            if (adj>0)
                v_mem_counters[i].accum_count +=adj;
            break;
        }
    }
    sal_assert(i<MAX_NUM_MEM_SIZES);
}
#endif

#ifdef DEBUG_USE_MEM_TAG
pthread_mutex_t v_malloc_tag_mutex;//=PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP;

void *sal_malloc_tag(unsigned int size, const char* name, int line)
{
#ifdef MEM_DEBUG
    int i;
#endif
    struct mem_tag* tag ;
    void* ptr;
    sal_assert(size > 0);
    ptr = malloc(size+TAG_SIZE);
//    sal_debug_print("sal_malloc %d, %s:%d\n", size, name, line);
    tag = (struct mem_tag*) ptr;
    if (ptr==NULL) {
        sal_debug_print("sal_malloc failed\n");
        return ptr;
    }
    pthread_mutex_lock(&v_malloc_tag_mutex);
    tag->magic = MEM_TAG_MAGIC;
    tag->line = line;
    strncpy(tag->name, name, TAG_SIZE-5);
    tag->name[TAG_SIZE-5] = 0;
    
#ifdef MEM_DEBUG        
    v_mem_alloc_count++;
    for (i = 0; i < MAX_NUM_MEM_BLOCKS; i++)
    {
        if (v_mem_blocks[i].addr == 0)
        {
            v_mem_blocks[i].addr = ptr;
            v_mem_blocks[i].size = size;
            _SalMemAdjustSizeCounter(size, 1);
            break;
        }
    }
    sal_assert(i < MAX_NUM_MEM_BLOCKS);
    v_mem_total_allocated += size;    
    if (v_mem_total_allocated>v_mem_max_allocated)
        v_mem_max_allocated = v_mem_total_allocated;
#endif    
    pthread_mutex_unlock(&v_malloc_tag_mutex);
    return (((char*)ptr)+TAG_SIZE);
}

void  sal_free_tag(void* ptr)
{
#ifdef MEM_DEBUG    
    int i;
    struct mem_tag* tag = (struct mem_tag*)(((char*)ptr)-TAG_SIZE);
    
    sal_assert(ptr!=NULL);
    sal_assert( tag->magic != MEM_TAG_MAGIC_FREED);
    sal_assert( tag->magic == MEM_TAG_MAGIC);
    pthread_mutex_lock(&v_malloc_tag_mutex);
    for (i = 0; i < MAX_NUM_MEM_BLOCKS; i++)
    {
        if (v_mem_blocks[i].addr == (void*)tag)
        {
            // found block being released 
            _SalMemAdjustSizeCounter(v_mem_blocks[i].size, -1);
            v_mem_total_allocated -= v_mem_blocks[i].size;
            v_mem_blocks[i].addr = 0;
            v_mem_blocks[i].size = 0;
            break;
        }
    }
    if (i>=MAX_NUM_MEM_BLOCKS) {
        /* Trying to free invalid pointer */ 
        sal_assert(0);
    }
#endif
    tag->magic = MEM_TAG_MAGIC_FREED;
    pthread_mutex_unlock(&v_malloc_tag_mutex);
    free(tag);
}
#else 
void *sal_malloc(unsigned int size)
{
#ifdef MEM_DEBUG
    int i;
#endif
    sal_assert(size > 0);
    void* ptr = malloc(size);
    if (ptr==NULL) {
        sal_debug_print("sal_malloc failed\n");
        return ptr;
    }
#ifdef MEM_DEBUG        
    v_mem_alloc_count++;
    for (i = 0; i < MAX_NUM_MEM_BLOCKS; i++)
    {
        if (v_mem_blocks[i].addr == 0)
        {
            // found no_pending_dpc entry; use it 
            v_mem_blocks[i].addr = ptr;
            v_mem_blocks[i].size = size;
            _SalMemAdjustSizeCounter(size, 1);
            break;
        }
    }
    sal_assert(i < MAX_NUM_MEM_BLOCKS);
    v_mem_total_allocated += size;    
    if (v_mem_total_allocated>v_mem_max_allocated)
        v_mem_max_allocated = v_mem_total_allocated;
#endif    
    return ptr;
}

void sal_free(void *ptr)
{
#ifdef MEM_DEBUG    
    int i;
    for (i = 0; i < MAX_NUM_MEM_BLOCKS; i++)
    {
        if (v_mem_blocks[i].addr == ptr)
        {
            // found block being released 
            _SalMemAdjustSizeCounter(v_mem_blocks[i].size, -1);
            v_mem_total_allocated -= v_mem_blocks[i].size;
            v_mem_blocks[i].addr = 0;
            v_mem_blocks[i].size = 0;
            break;
        }
    }
    if (i>=MAX_NUM_MEM_BLOCKS) {
        /* Trying to free invalid pointer */ 
        sal_assert(0);
    }
#endif    
    free(ptr);
}
#endif /* !DEBUG_USE_MEM_TAG */

void sal_mem_display(int verbose)
{
#ifdef MEM_DEBUG    
    int i;
    sal_debug_print("\nSize\tFreq.\tAccum.");
    for (i = 0; i < MAX_NUM_MEM_SIZES; i++)
    {
        if (v_mem_counters[i].size == 0) 
            break;
        sal_debug_print("\n%d\t%d\t%d", 
            v_mem_counters[i].size, v_mem_counters[i].count, v_mem_counters[i].accum_count);
    }
    if (verbose>=1) {
        sal_debug_print("\nAddress\t\tSize\tFunc\tLine");
        for (i = 0; i < MAX_NUM_MEM_BLOCKS; i++) {
            if (v_mem_blocks[i].addr != 0) {
#ifdef DEBUG_USE_MEM_TAG                
                sal_debug_print("\n%p\t%d\t%s\t%d",
                    v_mem_blocks[i].addr, v_mem_blocks[i].size,
                    ((struct mem_tag*)(v_mem_blocks[i].addr))->name, 
                    ((struct mem_tag*)(v_mem_blocks[i].addr))->line
                    );
#else
                sal_debug_print("\n%p\t%d",
                    v_mem_blocks[i].addr, v_mem_blocks[i].size
                    );
#endif
            }
        }
    }
    sal_debug_print("\nTotal memory allocated: %d\n", v_mem_total_allocated);
    sal_debug_print("Max memory allocated: %d\n", v_mem_max_allocated);    
    sal_debug_print("Memory allocation count: %d\n", v_mem_alloc_count);
    if (v_mem_total_allocated) {
        // This should be 0 when exiting.
        sal_assert(FALSE);
    }
#else
//    sal_debug_print("MEM_DEBUG flags is not turned on\n");
#endif        
}


void* sal_create_mem_pool(char* name, int size)
{
    return (void*)1;
}

int sal_destroy_mem_pool(void* pool)
{
    return 0;
}
#ifndef DEBUG_MEMORY_LEAK
void* sal_alloc_from_pool(void* pool, int size)
{
    return sal_malloc(size);
}

void sal_free_from_pool(void* pool, void* ptr)
{
    sal_free(ptr);
}
#endif
