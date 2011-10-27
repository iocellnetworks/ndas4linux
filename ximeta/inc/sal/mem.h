/*
 -------------------------------------------------------------------------
 Copyright (c) 2005, XIMETA, Inc., IRVINE, CA, USA.
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
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#ifndef _SAL_MEM_H_
#define _SAL_MEM_H_

#include "sal/types.h"
#include "sal/sal.h"

//#define MEM_DEBUG 1
//#define DEBUG_USE_MEM_TAG 1
//#define DEBUG_MEMORY_LEAK 1

/**
 * memory block.
 * 
 * @param ptr the pointer to the data
 * @param private the pointer to the private reference
 * @param len the lenth of the data pointed by ptr
 */
struct sal_mem_block {
    xpointer ptr;
#ifndef NDAS_MEMBLK_NO_PRIVATE
    xpointer private;
#endif
    xsize_t len;
};

#define SAL_MEMFLAG_DMA (1<<0)  // allocate from DMA memory region.

#ifdef DEBUG_USE_MEM_TAG
#define TAG_SIZE 48
struct mem_tag {
    unsigned short  magic;
    unsigned short line;
    char name[TAG_SIZE-4];
} __x_attribute_packed__;

#define MEM_TAG_MAGIC 0x87af
#define MEM_TAG_MAGIC_FREED 0x87bf

NDAS_SAL_API extern void *sal_malloc_tag(unsigned int size, const char* name, int line);
NDAS_SAL_API extern void sal_free_tag(void *ptr);
#define sal_malloc(size)    sal_malloc_tag(size, __FUNCTION__, __LINE__)
#define sal_free(ptr)        sal_free_tag(ptr)
#else
/* Allocate the memroy */
NDAS_SAL_API extern void    *sal_malloc(unsigned int size);

NDAS_SAL_API extern void    *sal_malloc_ex(unsigned int size, unsigned int flags);

/**
 * Free the memory
 */
NDAS_SAL_API extern void sal_free(void *ptr);


#endif

NDAS_SAL_API extern int sal_inc_memref(void *buf_ptr, unsigned int buf_length);
NDAS_SAL_API extern int sal_dec_memref(void *buf_ptr, unsigned int buf_length);

NDAS_SAL_API extern void sal_mem_display(int verbose);

#ifdef DEBUG_MEMORY_LEAK
#ifndef DEBUG_USE_MEM_TAG
#error DEBUG_USE_MEM_TAG should be defined
#endif
#define sal_alloc_from_pool(pool, size) sal_malloc(size)
#define sal_free_from_pool(pool, ptr) sal_free(ptr)
#else
NDAS_SAL_API extern void* sal_alloc_from_pool(void* pool, int size); 
NDAS_SAL_API extern void sal_free_from_pool(void* pool, void* ptr); 
#endif
/* used only when USE_SYSTEM_MEM_POOL is defined */
NDAS_SAL_API extern void* sal_create_mem_pool(char* name, int size); 
NDAS_SAL_API extern int sal_destroy_mem_pool(void* pool); 


/*
 * SAL memory page abstration structure
 */
typedef struct _sal_page {
	void * private;
} sal_page;

/*
 * Map the pages to the virtual address
 */
NDAS_SAL_API extern void *sal_map_page(sal_page *page);
/*
 * Unmap the pages from the virtual address
 */
NDAS_SAL_API extern void sal_unmap_page(sal_page *page);

NDAS_SAL_API
sal_page **
sal_alloc_page_array(
    void *buf_ptr,
    unsigned int buf_length,
    unsigned int flags
);

NDAS_SAL_API extern int sal_get_pages_from_virtual(
    void *buf_ptr,
    unsigned int buf_length,
    sal_page **sal_page_array,
    unsigned int *offset);


#endif /* _SAL_MEM_H_ */
