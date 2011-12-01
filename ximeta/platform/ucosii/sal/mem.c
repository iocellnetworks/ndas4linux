#include "sal/libc.h"
#include "sal/mem.h"
#include "sal/debug.h"
#include <ucos_ii.h>

//#define MEM_DEBUG 1

#ifdef MEM_DEBUG
#define MAX_NUM_MEM_BLOCKS 2048
#define MAX_NUM_MEM_SIZES  512

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
static long v_mem_total_allocated = 0;

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

void *sal_malloc(unsigned int size)
{
#ifdef MEM_DEBUG
    int i;
#endif
    void* ptr = OS_malloc(size);    
    if (ptr==NULL) {
        sal_debug_print("sal_malloc failed(size=%d)\r\n", size);
        return ptr;
    }
#ifdef MEM_DEBUG        
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
    sal_assert(i < MAX_NUM_MEM_BLOCKS);
#endif    
    OS_free(ptr);
}

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
        sal_debug_print("\nAddress\t\tSize");
        for (i = 0; i < MAX_NUM_MEM_BLOCKS; i++) {
            if (v_mem_blocks[i].addr != 0) {
                sal_debug_print("\n%p\t%d",v_mem_blocks[i].addr, v_mem_blocks[i].size);
            }
        }
    }
    sal_debug_print("\nTotal memory allocated: %d\n", v_mem_total_allocated);
#else
    sal_debug_print("MEM_DEBUG flags is not turned on\n");
#endif        
}

