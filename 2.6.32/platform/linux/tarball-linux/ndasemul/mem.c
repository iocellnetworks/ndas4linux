/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 
      
 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL v2),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#include <linux/version.h>
#include <linux/module.h> 

#include <linux/delay.h>

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#ifdef NDAS_EMU

#include "linux_ver.h"
#include "sal/ndas_sal_compat.h"


#include <linux/module.h> // kmalloc, kfree, EXPORT_SYMBOL
#include <linux/slab.h> // kmalloc, kfree
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/pagemap.h>
#include <linux/mm.h>
#include <linux/highmem.h> // kmap, kunmap

#if 0
#ifdef USERMODE_TEST
#include <linux/compatmac.h> // copy_from_user, copy_to_user
#endif
#include "linux_ver.h" // KMEM_CACHE, KMEM_ATOMIC
#include "sal/libc.h"
#include "sal/mem.h"
#include "sal/debug.h"

#endif

/* support for userlevel test application */
// #undef USERLEVEL_TEST

#ifdef DEBUG
    #ifdef DEBUG_LEVEL
        #ifndef DEBUGLEVEL_SAL_MEM
        #define DEBUGLEVEL_SAL_MEM DEBUG_LEVEL
        #endif
    #else
    #define DEBUGLEVEL_SAL_MEM 1
    #endif
#endif
#ifdef DEBUG
#define dbgl_salmem(l, x...) do { if ( l <= DEBUGLEVEL_SAL_MEM ) { sal_debug_print("MEM|%d|%s|",l, __FUNCTION__); sal_debug_println(x);}} while(0)
#else
#define dbgl_salmem(l, x...) do {} while(0)
#endif

#ifdef MEM_DEBUG
#define MAX_NUM_MEM_BLOCKS 2048
#define MAX_NUM_MEM_SIZES  64

typedef struct
{
    void * addr;
    unsigned int size;
} mem_block_entry;

typedef struct
{
    int      count;
    int     accum_count;
    unsigned int  size;
} mem_block_counter;

static mem_block_entry v_mem_blocks[MAX_NUM_MEM_BLOCKS];
static mem_block_counter v_mem_counters[MAX_NUM_MEM_SIZES];
static int v_mem_total_allocated = 0;
static spinlock_t v_mem_block_lock = SPIN_LOCK_UNLOCKED;
static int v_mem_max_allocated = 0;

static void set_mem_block_size_adjust(unsigned int size, int adj)
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

NDAS_SAL_API
void *sal_malloc_ex(unsigned int size, unsigned int flags)
{
#ifdef MEM_DEBUG
    int i;
    unsigned long lock_flags;
#endif
    void* ptr;
    int kmalloc_flags = 0;

    if (in_interrupt() ) {
        kmalloc_flags |= GFP_ATOMIC;
    } else {
#if LINUX_VERSION_25_ABOVE
        if (in_atomic() || irqs_disabled())
        {
            kmalloc_flags |= GFP_ATOMIC;
        }
        else
#endif        
        {
            kmalloc_flags |= GFP_KERNEL;
        }
    }
    
    if (flags & SAL_MEMFLAG_DMA) {
        kmalloc_flags |= GFP_DMA;
    }

    ptr = kmalloc(size, kmalloc_flags);
    if (ptr==NULL) {
        printk("ndas: failed to allocate memory size=%d flags=%x\n",size, kmalloc_flags);
        return ptr;
    }
    dbgl_salmem(9, "allocated size=%d",size);
#ifdef MEM_DEBUG        
    spin_lock_irqsave(&v_mem_block_lock, lock_flags);
    for (i = 0; i < MAX_NUM_MEM_BLOCKS; i++)
    {
        if (v_mem_blocks[i].addr == 0)
        {
            // found no mem address
            v_mem_blocks[i].addr = ptr;
            v_mem_blocks[i].size = size;
            set_mem_block_size_adjust(size, 1);
            break;
        }
    }
    v_mem_total_allocated += size;
    spin_unlock_irqrestore(&v_mem_block_lock, lock_flags);
    if (i >= MAX_NUM_MEM_BLOCKS) {
        dbgl_salmem(1,"Requested memory %d failed\n", size);
        sal_mem_display(1);
    }
    sal_assert(i < MAX_NUM_MEM_BLOCKS);
#endif
    return ptr;
}

/* to do: clean up. Merge with sal_malloc_ex */
#ifdef DEBUG_USE_MEM_TAG
void *sal_malloc_tag(unsigned int size, const char* name, int line) {
#ifdef MEM_DEBUG
    int i;
    unsigned long lock_flags;
    struct mem_tag* tag;
#endif
    void* ptr;
    int kmalloc_flags = 0;

    if (in_interrupt() ) {
        kmalloc_flags |= GFP_ATOMIC;
    } else {
#if LINUX_VERSION_25_ABOVE
        if (in_atomic() || irqs_disabled())
        {
            kmalloc_flags |= GFP_ATOMIC;
        }
        else
#endif        
        {
            kmalloc_flags |= GFP_KERNEL;
        }
    }
   
    tag = (struct mem_tag*) kmalloc(size+TAG_SIZE, kmalloc_flags);
    ptr = ((char*)tag) + TAG_SIZE;

    if (tag==NULL) {
        printk("ndas: failed to allocate memory size=%d",size);
        return ptr;
    }

#ifdef MEM_DEBUG        
    spin_lock_irqsave(&v_mem_block_lock, lock_flags);
    for (i = 0; i < MAX_NUM_MEM_BLOCKS; i++)
    {
        if (v_mem_blocks[i].addr == 0)
        {
            // found no_pending_dpc entry; use it 
            v_mem_blocks[i].addr = ptr;
            v_mem_blocks[i].size = size;
            set_mem_block_size_adjust(size, 1);
            break;
        }
    }
    v_mem_total_allocated += size;
    spin_unlock_irqrestore(&v_mem_block_lock, lock_flags);
    if (i >= MAX_NUM_MEM_BLOCKS) {
        dbgl_salmem(1,"Requested memory %d failed\n", size);
        sal_mem_display(1);
    }
    sal_assert(i < MAX_NUM_MEM_BLOCKS);
#endif
    tag->magic = MEM_TAG_MAGIC;
    tag->line = line;
    strncpy(tag->name, name, TAG_SIZE-5);
    tag->name[TAG_SIZE-5] = 0;

    if (v_mem_total_allocated>v_mem_max_allocated)
        v_mem_max_allocated = v_mem_total_allocated;
    return ptr;
}
#else
NDAS_SAL_API
void *sal_malloc(unsigned int size)
{
    return sal_malloc_ex(size, 0);
}
#endif


#ifdef DEBUG_USE_MEM_TAG
NDAS_SAL_API
void sal_free_tag(void *ptr)
#else
NDAS_SAL_API
void sal_free(void *ptr)
#endif
{
#ifdef DEBUG_USE_MEM_TAG
    struct mem_tag* tag = (struct mem_tag*)(((char*)ptr)-TAG_SIZE);
#endif

#ifdef MEM_DEBUG
    int i;
    unsigned long flags;
    spin_lock_irqsave(&v_mem_block_lock, flags);
    for (i = 0; i < MAX_NUM_MEM_BLOCKS; i++)
    {
        if (v_mem_blocks[i].addr == ptr)
        {
            // found block being released 
            set_mem_block_size_adjust(v_mem_blocks[i].size, -1);
            v_mem_total_allocated -= v_mem_blocks[i].size;
            v_mem_blocks[i].addr = 0;
            v_mem_blocks[i].size = 0;
            break;
        }
    }
    spin_unlock_irqrestore(&v_mem_block_lock, flags);
    if (i >= MAX_NUM_MEM_BLOCKS) {
        dbgl_salmem(1,"Failed find memory to free %p\n", ptr);
        sal_mem_display(1);
    }
    sal_assert(i < MAX_NUM_MEM_BLOCKS);
#endif

#ifdef DEBUG_USE_MEM_TAG
    tag->magic = MEM_TAG_MAGIC_FREED;
    kfree(tag);
#else
    kfree(ptr);
#endif
}



NDAS_SAL_API
void sal_mem_display(int verbose)
{
#ifdef MEM_DEBUG
    int i,j;
    sal_debug_print("\nSize     Freq.     Accum.");
    for (i = 0; i < MAX_NUM_MEM_SIZES; i++)
    {
        if (v_mem_counters[i].size == 0) 
            break;
        sal_debug_print("\n%d    %d     %d", 
            v_mem_counters[i].size, v_mem_counters[i].count, v_mem_counters[i].accum_count);
    }
    if (verbose>=1) {
#ifdef DEBUG_USE_MEM_TAG
        sal_debug_print("\nAddress\t\tSize\tFunc\tLine");
#else
        sal_debug_print("\nAddress            Size");
#endif
        for (i = 0; i < MAX_NUM_MEM_BLOCKS; i++) {
            if (v_mem_blocks[i].addr != 0) {
#ifdef DEBUG_USE_MEM_TAG
                sal_assert(((struct mem_tag*)(((char*)v_mem_blocks[i].addr)-TAG_SIZE))->magic == MEM_TAG_MAGIC);
                sal_debug_print("\n%p\t%d\t%s\t%d",
                        v_mem_blocks[i].addr, v_mem_blocks[i].size,
                        ((struct mem_tag*)(((char*)v_mem_blocks[i].addr)-TAG_SIZE))->name, 
                        ((struct mem_tag*)(((char*)v_mem_blocks[i].addr)-TAG_SIZE))->line
                        );
#else

                sal_debug_print("\n%p     %d",v_mem_blocks[i].addr, v_mem_blocks[i].size);
#endif
                if (verbose >=2) {
                    for(j=0;j<v_mem_blocks[i].size;j+=sizeof(unsigned int)) {
                        if (j%128==0) {
                            sal_debug_print("\n    ");
                        }
                        sal_debug_print("%08x ", *(((unsigned int*)v_mem_blocks[i].addr)+j));
                    }
                }
            }
        }
    }
    sal_debug_print("\nTotal memory allocated: %d\n", v_mem_total_allocated);
    sal_debug_print("Max memory allocated: %d\n", v_mem_max_allocated);
#else
    sal_debug_print("MEM_DEBUG flags is not turned on\n");
#endif
}


NDAS_SAL_API
int sal_inc_memref(void *buf_ptr, unsigned int buf_length)
{
    unsigned char *start;
    unsigned int pages_to_increase;
    unsigned int offset;

    if(buf_ptr == NULL)
        return NDAS_ERROR;
	if(buf_length == 0)
        return NDAS_ERROR;

    offset = (xsize_t)buf_ptr % PAGE_SIZE;
    pages_to_increase = (buf_length + offset - 1) / PAGE_SIZE + 1;
    start = buf_ptr;
    while(pages_to_increase) {
        struct page*t_page;

        t_page = virt_to_page(start);
        get_page(t_page);
        start += PAGE_SIZE;
        pages_to_increase--;
    }

    return NDAS_OK;
}

NDAS_SAL_API
int sal_dec_memref(void *buf_ptr, unsigned int buf_length)
{
    unsigned char *start;
    unsigned pages_to_decrease;
    int pagerefzero;
    unsigned int offset;

    if(buf_ptr == NULL)
        return NDAS_ERROR;
    if(buf_length == 0)
        return NDAS_ERROR;

    offset = (xsize_t)buf_ptr % PAGE_SIZE;
	pages_to_decrease = (buf_length + offset - 1)/PAGE_SIZE + 1;

    start = buf_ptr;
    while(pages_to_decrease) {
        struct page*t_page;

        t_page = virt_to_page(start);
		//
		// Do not call put_page() which might free the page.
		// We just need to decrement the reference count of the page.
		//
        pagerefzero = put_page_testzero(t_page);
#ifdef DEBUG
        if(pagerefzero == 0) {
            dbgl_salmem(1, "not zero ref page = %p", t_page);
        }
#endif
        start += PAGE_SIZE;
        pages_to_decrease--;
    }

    return NDAS_OK;
}


NDAS_SAL_API void* sal_create_mem_pool(char* name, int size)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,23))
    return (void*)kmem_cache_create(name, size, 0, 0, NULL);
#else
    return (void*)kmem_cache_create(name, size, 0, 0, NULL, NULL);
#endif
}

NDAS_SAL_API int sal_destroy_mem_pool(void* pool)
{
    kmem_cache_destroy((KMEM_CACHE*)pool);
    return 0;
}

#ifndef DEBUG_MEMORY_LEAK

#ifndef XPLAT_ALLOW_LINUX_HEADER

NDAS_SAL_API void* sal_alloc_from_pool(void* pool, int size)
{
    int kmalloc_flags = 0;
    if (in_interrupt() ) {
        kmalloc_flags |= GFP_ATOMIC;
    } else {
#if LINUX_VERSION_25_ABOVE
        if (in_atomic() || irqs_disabled())
        {
            kmalloc_flags |= GFP_ATOMIC;
        }
        else
#endif        
        {
            kmalloc_flags |= GFP_KERNEL;
        }
    }

    return kmem_cache_alloc((KMEM_CACHE*)pool, kmalloc_flags);
}

NDAS_SAL_API void sal_free_from_pool(void* pool, void* ptr)
{
    kmem_cache_free((KMEM_CACHE*)pool, ptr);
}

#endif

#endif

/*
 * Map the pages to the virtual address
 */
NDAS_SAL_API void *sal_map_page(sal_page *page)
{
    return kmap((struct page *)page);
}

/*
 * Unmap the pages from the virtual address
 */
NDAS_SAL_API void sal_unmap_page(sal_page *page)
{
    kunmap((struct page *)page);
}

NDAS_SAL_API
sal_page **
sal_alloc_page_array(
    void *buf_ptr,
    unsigned int buf_length,
    unsigned int flags
)
{
    unsigned int offset;
    unsigned int num_of_pages;

    offset = (xsize_t)buf_ptr % PAGE_SIZE;
    num_of_pages = (buf_length + offset - 1) / PAGE_SIZE + 1;

    return (sal_page **)sal_malloc_ex(num_of_pages * sizeof(sal_page *), flags);
}

NDAS_SAL_API
int
sal_get_pages_from_virtual(
    void *buf_ptr,
    unsigned int buf_length,
    sal_page **sal_page_array,
    unsigned int *offset)
{
    unsigned char *start;
    unsigned num_of_pages;
	struct page **page_array;
	unsigned idx_page;

    if(buf_length == 0)
        return NDAS_ERROR;
    page_array = (struct page **)sal_page_array;
    if(page_array == NULL)
        return NDAS_ERROR;

    *offset = (xsize_t)buf_ptr % PAGE_SIZE;
	num_of_pages = (buf_length + *offset - 1) / PAGE_SIZE + 1;

    start = buf_ptr;
	for(idx_page = 0; idx_page < num_of_pages; idx_page ++) {
        struct page*t_page;

        t_page = virt_to_page(start);
        page_array[idx_page] = t_page;

		dbgl_salmem(4, "sa=%p va=%p p=%p", start, page_address(t_page),t_page);

        start += PAGE_SIZE;
    }

	return NDAS_OK;
}

#endif
