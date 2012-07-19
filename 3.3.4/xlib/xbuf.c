#include "xplatcfg.h"
#include "sal/mem.h"
#include "sal/debug.h"
#include "sal/sync.h"
#include "sal/libc.h"
#include "xlib/xbuf.h"
#include "xlib/xhash.h"
#include "xlib/dpc.h"

#if defined(RELEASE) && defined(STAT_XBUF)
#warning "Undefining STAT_XBUF in release mode"
#undef STAT_XBUF
#endif

#ifdef DEBUG
#define    debug_xbuf(l,x...) \
do { \
    if(l <= DEBUG_LEVEL_XLIB_XBUF) { \
        sal_debug_print("XB|%d|%s|",l, __FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)
#else
#define    debug_xbuf(l, x...) do {} while(0)
#endif

#define ETHERNET_HEAD_RESERVE 16 /* Reserve 16 bytes for ethernet header for alignment */

#ifndef min_t
#define min_t(type,x,y)  \
    ({ type __x = (x); type __y = (y); __x < __y ? __x: __y; })
#endif
#ifndef max_t
#define max_t(type,x,y) \
    ({ type __x = (x); type __y = (y); __x > __y ? __x: __y; })
#endif


#ifdef MEM_DEBUG

static
void
print_xbuf_struc(void)
{
    sal_error_print("xbuf structure\n");
#ifdef DEBUG
    sal_error_print("magic %d\n", offsetof(struct xbuf, magic));
#endif
    sal_error_print("next %d prev %d queue %d owner %d\n",
        offsetof(struct xbuf, next),
        offsetof(struct xbuf, prev),
        offsetof(struct xbuf, queue),
        offsetof(struct xbuf, owner));
    sal_error_print("mach %d nh %d nhlen %d userlen %d len %d buflen %d\n",
        offsetof(struct xbuf, mach),
        offsetof(struct xbuf, nh),
        offsetof(struct xbuf, nhlen),
        offsetof(struct xbuf, userlen),
        offsetof(struct xbuf, len),
        offsetof(struct xbuf, buflen));
    sal_error_print("head %d data %d xbuf_flags %d ref %d\n",
        offsetof(struct xbuf, head),
        offsetof(struct xbuf, data),
        offsetof(struct xbuf, xbuf_flags),
        offsetof(struct xbuf, ref));
    sal_error_print("destruct_handler %d handler_userdata %d clonee %d sys_buf %d\n",
        offsetof(struct xbuf, destruct_handler),
        offsetof(struct xbuf, handler_userdata),
        offsetof(struct xbuf, clonee),
        offsetof(struct xbuf, sys_buf));
    sal_error_print("sys_buf_len %d pool_index %d prot_seqnum %d nr_data_segs %d data_segs %d\n",
        offsetof(struct xbuf, sys_buf_len),
        offsetof(struct xbuf, pool_index),
        offsetof(struct xbuf, prot_seqnum),
        offsetof(struct xbuf, nr_data_segs),
        offsetof(struct xbuf, data_segs));
    sal_error_print("sizeof(data_segs)=%d\n",
        sizeof(struct sal_mem_block [SAL_NET_MAX_BUF_SEG_COUNT]));
}

#endif

ndas_error_t xbuf_queue_init(struct xbuf_queue* q)
{
    debug_xbuf(3, "q=%p",q);
    if ( !q ) return NDAS_ERROR_INVALID_PARAMETER;
    q->prev = q->next = (struct xbuf*)q;
    q->qlen = 0;
    q->flags = 0;
    sal_spinlock_create("xbuf_lock", &q->lock);// maybe we can use q->avail->lock
    q->avail = NULL; // xlib_event_create("xq-wait");  // Lazy allocation for reduce unnecessary resource allocation.
    return NDAS_OK;
}

void xbuf_queue_reset(struct xbuf_queue* q)
{
    struct xbuf* xb;
    debug_xbuf(9, "ing q=%p",q);

    if (q->qlen) { /* In case for release version */
        debug_xbuf(1, "xq is not empty len=%d", q->qlen);
        while((xb = xbuf_dequeue(q))) {
            sal_assert(xb->magic == XBUF_MAGIC);
            xbuf_unlink(xb);
            xbuf_free(xb);
        }
    }
}

/* destroy the queue */

void xbuf_queue_destroy(struct xbuf_queue* q)
{
    debug_xbuf(3, "q=%p",q);
    xbuf_queue_reset(q);
    sal_spinlock_destroy(q->lock);
    if (q->flags & XBUFQ_FLAG_AVAIL_EVENT)
        sal_event_destroy(q->avail);
}



void xbuf_queue_head(struct xbuf_queue *list, struct xbuf * xb)
{
    struct xbuf *prev, *next;
    unsigned long flags;

    sal_assert(list!=NULL);
    sal_assert(xb->magic == XBUF_MAGIC);

    debug_xbuf(9, "xbuf_queue_head:list=%p",list);
    xbuf_qlock(list, &flags);
    xb->queue = list;
    if ( list->qlen == 0 && (list->flags & XBUFQ_FLAG_AVAIL_EVENT)) {
        sal_event_set(list->avail);
    }
    list->qlen ++;

    prev=(struct xbuf *)list;
    next = prev->next;
    xb->next = next;
    xb->prev = prev;
    next->prev = xb;
    prev->next = xb;

    xbuf_qunlock(list, &flags);
}

void xbuf_queue_tail(struct xbuf_queue *list, struct xbuf *xb)
{
    unsigned long flags;

    sal_assert(xb->magic == XBUF_MAGIC);

    debug_xbuf(3, "xbuf_queue_tail:list=%p",list);
    debug_xbuf(9, "xbuf_queue_tail:xb=%s",DEBUG_XLIB_XBUF(xb));
    xbuf_qlock(list, &flags);
    _xbuf_queue_tail(list, xb);
    xbuf_qunlock(list, &flags);
}


/* if lock is true, use lock while dequeuing */
struct xbuf *xbuf_dequeue(struct xbuf_queue *list)
{
    struct xbuf *result;
    unsigned long flags;

    debug_xbuf(3, "list=%p",list);
    sal_assert(list != NULL);

    xbuf_qlock(list, &flags);
    result = _xbuf_dequeue(list);
    xbuf_qunlock(list, &flags);

    debug_xbuf(9, "ed result=%s",DEBUG_XLIB_XBUF(result));
    debug_xbuf(3, "ed result=%p", result);
    return result;
}

/* This function can return after waiting less than 'tout' time */
struct xbuf *xbuf_dequeue_wait(struct xbuf_queue *list, sal_tick tout)
{
    struct xbuf *xb;
    debug_xbuf(3, "list=%p",list);

    if (!(list->flags & XBUFQ_FLAG_AVAIL_EVENT)) { 
        list->avail = sal_event_create("xq-a");
        list->flags |= XBUFQ_FLAG_AVAIL_EVENT;
    }
    if ( (xb = xbuf_dequeue(list)) == NULL) {
        if ( sal_event_wait(list->avail, tout) == SAL_SYNC_OK ) {
            xb = xbuf_dequeue(list);
        }
    }
    debug_xbuf(3, "ed xb=%p", xb);
    return xb;
}

void xbuf_unlink(struct xbuf* xb)
{
    struct xbuf_queue *list = xb->queue;
    unsigned long flags;

    sal_assert(xb->magic == XBUF_MAGIC);

    if(list) {
        xbuf_qlock(list, &flags);
        _xbuf_unlink(xb);
        xbuf_qunlock(list, &flags);
    }
}

/* Copy xbuf to user-supplied buffer 
    offset is offset from data region of xbuf */
int xbuf_copy_data_to(struct xbuf* xb, int offset, char* buf, int len)
{
    sal_assert(xb);
    sal_assert(buf);
    sal_assert(!xbuf_is_clone(xb));
    sal_assert(XBUF_NETWORK_DATA(xb));
    sal_memcpy(buf, XBUF_NETWORK_DATA(xb)+offset, len);
    //sal_copy_to_user(buf, XBUF_NETWORK_DATA(xb)+offset, len);
    return 0;
}

/* Copy xbuf to user-supplied buffers 
    offset is offset from data region of xbuf */
int xbuf_copy_data_to_blocks(struct xbuf* xb, int offset, 
        struct sal_mem_block* blocks, int nr_blocks, int len)
{
    char* kdata = XBUF_NETWORK_DATA(xb)+offset;
    sal_assert(xb->magic == XBUF_MAGIC);
    sal_assert(XBUF_NETWORK_DATA(xb));

    sal_assert(!xbuf_is_clone(xb));


    while(len>0)
    {
        if(blocks->len)
        {
            int copy = min_t(unsigned int, blocks->len, len);
            sal_memcpy(blocks->ptr, kdata, copy);
            kdata+=copy;
            len-=copy;
            blocks->len-=copy;
            blocks->ptr+=copy;
        }
        else
            blocks++;
    }
    
    
    //sal_memcpy(buf, XBUF_NETWORK_DATA(xb)+offset, len);
    //sal_copy_to_user(buf, XBUF_NETWORK_DATA(xb)+offset, len);
    return 0;
}


/*  Copy data from user-supplied buffer to xbuf
    offset is offset from data region of xbuf */
int xbuf_copy_data_from(struct xbuf* xb, char* buf, int len)
{
    sal_assert(xb->magic == XBUF_MAGIC);
    sal_assert(!xbuf_is_clone(xb));
    sal_assert(xb!=NULL);
    sal_assert(XBUF_NETWORK_DATA(xb));
    
    sal_memcpy(XBUF_NETWORK_DATA(xb), buf, len);

    if (len > xb->len) {
        xb->len = len;
    }
    if(len > xb->userlen) {
        xb->userlen = len;
    }
    return 0;
}


/* Remove data from head of data region 
    Return pointer to the resultant data buffer head.
    */
char* xbuf_pull(struct xbuf* xb, int len)
{
    sal_assert(!xbuf_is_clone(xb));
    if (len > xb->len)
        return NULL;
    xb->len -= len;
    xb->data += len;
    return (xb->data);
} 

/*
   Copy xbuf. Don't be confused with xbuf_clone().
 */
struct xbuf* xbuf_copy(struct xbuf* xb)
{
    struct xbuf* newxb;
    
    sal_assert(xb->magic == XBUF_MAGIC);
    newxb = xbuf_alloc(xb->buflen);
    if (!newxb)
        return NULL;
    
    newxb->next = newxb->prev = NULL;
    newxb->queue = NULL;
    newxb->owner = xb->owner;
    newxb->xbuf_flags = xb->xbuf_flags;
    newxb->prot_seqnum = xb->prot_seqnum;
    newxb->userlen = xb->userlen;

    /* Copy scatter and gather buffer pointers */
    newxb->nr_data_segs = xb->nr_data_segs;
    if(xb->nr_data_segs) {
        sal_assert(xb->nr_data_segs <= SAL_NET_MAX_BUF_SEG_COUNT);
        sal_memcpy(
            newxb->data_segs,
            xb->data_segs,
            sizeof(struct sal_mem_block) * xb->nr_data_segs);
    }

    if (xb->sys_buf) {    
        sal_net_buf_seg bufs;
        newxb->sys_buf = sal_net_copy_platform_buf(xb->sys_buf, xb->nhlen, xb->sys_buf_len, &bufs);
        newxb->sys_buf_len = xb->sys_buf_len;
        newxb->nh = bufs.network_header.ptr;
        newxb->nhlen = xb->nhlen;

        if (bufs.nr_data_segs) {
            sal_assert(bufs.nr_data_segs == 1);
            newxb->data = (char*)bufs.data_segs[0].ptr;            
        } else {
            newxb->data = NULL;
        }
        newxb->len = xb->len;
        newxb->clonee = NULL;
    //
    // If system network buffer rebuild flag is set,
    // Copy some length fields.
    //
    } else if(xbuf_is_sys_netbuf_rebuild(xb)) {
        newxb->sys_buf_len = xb->sys_buf_len;
        newxb->nhlen = xb->nhlen;
        newxb->data = NULL;
        newxb->len = xb->len;
        newxb->clonee = NULL;
    } else {
        newxb->nh    = newxb->head + (xb->nh - xb->head);
        newxb->nhlen = xb->nhlen;
        newxb->len = xb->len;
        newxb->data = newxb->head + (xb->data - xb->head);

        if (xb->nhlen) {
            sal_memcpy(newxb->nh, xb->nh, xb->nhlen);
        }
        if (xb->len)
            sal_memcpy(newxb->data, xb->data, xb->len);
    }
    return newxb;
}

/* 
    "Clone" copies only ethernet header & network header.
    Data is redirected to clonee's.
    If system buffer ( platform buffer ) is used, xbuf itself is duplicated,
    and system buffer is cloned and shared.
*/
struct xbuf* xbuf_clone(struct xbuf* xb)
{
    struct xbuf* newxb;
    sal_assert(xb->magic == XBUF_MAGIC);
    debug_xbuf(3, "from=%p",xb);
    debug_xbuf(5, "from=%s",DEBUG_XLIB_XBUF(xb));
    newxb = xbuf_alloc(SAL_ETHER_HEADER_SIZE + xb->nhlen); /* Mac and LPX reserve is required */
    if (!newxb)
        return NULL;
    newxb->next = newxb->prev = NULL;
    newxb->queue = NULL;
    // no need to copy mac address because it will be rewritten at transmit time
    newxb->len = xb->len;

    /* just forward data ptr to clonees */
    newxb->data = XBUF_NETWORK_DATA(xb);

    newxb->owner = xb->owner;
    newxb->xbuf_flags = xb->xbuf_flags;
    newxb->prot_seqnum = xb->prot_seqnum;
    newxb->userlen = xb->userlen;

    /* We will not copy scatter and gather buffer pointers 
       use clonee's. */
    newxb->nr_data_segs = xb->nr_data_segs;


    //
    // Make a reference to the clonee ( original xbuffer )
    //
    if (xb->clonee) {
        newxb->clonee = xb->clonee;
    } else {
        newxb->clonee = xb;
    }

    //
    // Copy the data buffer information.
    //
    if (xb->sys_buf) {
        newxb->nh = xb->nh;
        newxb->nhlen = xb->nhlen;
        newxb->sys_buf = sal_net_clone_platform_buf(xb->sys_buf);
        if (newxb->sys_buf == NULL){
            xbuf_free(newxb);
            return NULL;
        }

        newxb->sys_buf_len = xb->sys_buf_len;
    //
    // If system network buffer rebuild flag is set,
    // Copy some length fields.
    //
    } else if(xbuf_is_sys_netbuf_rebuild(xb)) {
        newxb->nh = NULL;
        newxb->nhlen = xb->nhlen;
        newxb->sys_buf = NULL;
        newxb->sys_buf_len = xb->sys_buf_len;
    } else {
        sal_assert(xb->nh >= xb->head);
        newxb->nh = newxb->head + (xb->nh - xb->head);
        newxb->nhlen = xb->nhlen;
        if (xb->nhlen) {
/*            sal_error_print("h: %x, n:%x, l:%d\n", 
                newxb->head,
                newxb->nh, xb->nhlen);*/
            sal_memcpy(newxb->nh, xb->nh, xb->nhlen);
        }
    }

    sal_atomic_inc(&newxb->clonee->ref);

    return newxb;
}

/* 
    Add(reserve) data area to buffer
    Return the first byte of reserved region.
*/
char* xbuf_put(struct xbuf* xb, unsigned int len)
{
    char* ptr;
    sal_assert(xb->magic == XBUF_MAGIC);
    sal_assert(XBUF_NETWORK_DATA(xb));
    ptr = XBUF_NETWORK_DATA(xb) + xb->len;
    xb->len +=len;
    return ptr;
}

static
inline
struct xbuf*
xbuf_init_from_netbuf_seg(
    struct xbuf *xb,
    sal_net_buf_seg *bufs
)
{
    xb->sys_buf_len = bufs->data_len;
    xb->mach = bufs->mac_header.ptr;
    xb->nh = bufs->network_header.ptr;
    xb->nhlen = bufs->network_header.len;

    xb->nr_data_segs = bufs->nr_data_segs;
    if (bufs->nr_data_segs) {
        xb->data =  (char*)bufs->data_segs[0].ptr;
        xb->len = bufs->data_len;
        xb->userlen = bufs->data_len;
        sal_assert(bufs->nr_data_segs <= SAL_NET_MAX_BUF_SEG_COUNT);
        sal_memcpy(
            xb->data_segs,
            bufs->data_segs,
            sizeof(struct sal_mem_block) * bufs->nr_data_segs);
    } else {
        /* Data pointer is not supplied. */
        xb->data =  NULL;
        xb->len = bufs->data_len;
        xb->userlen = bufs->data_len;
    }
    return xb;
}


struct xbuf* xbuf_alloc_from_net_buf(sal_net_buff nbuf, sal_net_buf_seg *bufs) 
{
    struct xbuf* xb;

    xb = xbuf_alloc(0); /* net buff has actual data buffer. So no data need to be reserved */
    if ( !xb ) 
        return NULL;// out of memory 

    xb->sys_buf = nbuf;
    return xbuf_init_from_netbuf_seg(xb, bufs);
}

#ifdef USE_SYSTEM_MEM_POOL
#define XBUF_POOL_COUNT 3
#define XBUF_ALIGNED_SIZE ((sizeof(struct xbuf) + 3) / 4 * 4)
struct _xbuf_pool {
    int bytes;
    char* name;
    void* sys_pool;
};

static struct _xbuf_pool v_xbuf_pool[XBUF_POOL_COUNT] = {
    {64 + XBUF_ALIGNED_SIZE,     "ndas_xb_64", NULL},
    {600 + XBUF_ALIGNED_SIZE,   "ndas_xb_600", NULL},
    {1600 + XBUF_ALIGNED_SIZE, "ndas_xb_1600", NULL}
};

void xbuf_pool_shutdown(void)
{
    int i;
    for(i=0;i<XBUF_POOL_COUNT;i++) {
        if (v_xbuf_pool[i].sys_pool) {
            sal_destroy_mem_pool(v_xbuf_pool[i].sys_pool);
            v_xbuf_pool[i].sys_pool = NULL;
        }
    }
}

ndas_error_t xbuf_pool_init(xint32 transfer_unit)
{
    int i;
#ifdef MEM_DEBUG
    print_xbuf_struc();
#endif
    for(i=0;i<XBUF_POOL_COUNT;i++) {
        v_xbuf_pool[i].sys_pool = sal_create_mem_pool(
            v_xbuf_pool[i].name, v_xbuf_pool[i].bytes);
        if (v_xbuf_pool[i].sys_pool==NULL) {
            xbuf_pool_shutdown();
            return NDAS_ERROR_OUT_OF_MEMORY;
        }
    }    
    return NDAS_OK;
}

#ifdef DEBUG_MEMORY_LEAK
struct xbuf* xbuf_alloc_debug(int size, const char *file, int line)
#else
struct xbuf* xbuf_alloc(int size)
#endif
{
    struct xbuf *xb;
    int pool_index;
    char* buf;

    debug_xbuf(3,"size=%d", size);

    if (size<=64) { // Most frequent case. ignore pool 0 right now..
        pool_index = 0;
    } else if (size<=600) {
        pool_index = 1;
    } else if (size<=1600) {
        pool_index = 2;
    } else {
        debug_xbuf(1, "Too large buffer requested %d\n", size);
        return NULL;
    }
#ifdef DEBUG_MEMORY_LEAK
    buf = sal_malloc_tag(v_xbuf_pool[pool_index].bytes, file, line);
#else
    buf = sal_alloc_from_pool(v_xbuf_pool[pool_index].sys_pool, v_xbuf_pool[pool_index].bytes);
#endif
    if (!buf) {
        debug_xbuf(1,"Failed to allocate xbuf\n");
        return NULL;
    }
    xb = (struct xbuf*) buf;

    xb->next = xb->prev= NULL;
    xb->queue = NULL;
    xb->owner = NULL;
    xb->userlen = 0;
    xb->len = 0;
    xb->buflen = size;
    xb->head = buf + XBUF_ALIGNED_SIZE;
    xb->data = xb->head;
    sal_atomic_set(&xb->ref, 1);
    xb->destruct_handler = NULL;
    xb->handler_userdata = NULL;
    xb->clonee = NULL;
    xb->sys_buf = NULL;
    xb->pool_index = pool_index;
    xb->xbuf_flags = 0;
    xb->prot_seqnum = 0;
    xb->nr_data_segs = 0;

#ifdef DEBUG
    xb->magic = XBUF_MAGIC;
#endif
    return xb;
}


void xbuf_free(struct xbuf* xb)
{
    int last_ref;

#ifdef DEBUG
    sal_assert(xb->magic == XBUF_MAGIC);
#endif

    last_ref = sal_atomic_dec_and_test(&xb->ref);
    if (!last_ref) {
        /* Somebody is using the xbuffer */
        return;
    }
    xbuf_unlink(xb);

    /*
     * Call user's destruct handler.
     */
    if(xb->destruct_handler) {
        xb->destruct_handler(xb, xb->handler_userdata);
    }

    if (xb->clonee) {
        xbuf_free(xb->clonee);
    }

    if (xb->sys_buf) {
        sal_net_free_platform_buf(xb->sys_buf);
        xb->sys_buf = NULL;
    }

    sal_free_from_pool(v_xbuf_pool[xb->pool_index].sys_pool, xb);
}

ndas_error_t xbuf_pool_inc(xbool inc, xbool write_mode){
    return NDAS_OK;
}
ndas_error_t xbuf_pool_dec(xbool inc, xbool write_mode){
    return NDAS_OK;
}


#else /* USE_SYSTEM_MEM_POOL */

#define XBUF_POOL_COUNT 6

#ifdef STAT_XBUF
#define XBUF_POOL_INIT_NULL 0,0,0
#else
#define XBUF_POOL_INIT_NULL 
#endif
struct _xbuf_pool {
    int bytes;
    int read_incremental;
    int write_incremental;
    int default_count; /* default count */
    int xbuf_count; /* xbuf count. belongs to this pool */    
    struct xbuf_queue *queue;
    int dynamic_count;     /* number of dynamically allocated xbuf for this pool */
#ifdef STAT_XBUF    
    volatile int current_usage;      /* */
    volatile int max_usage;           /* max number of allocations */
    volatile int usage_count;         /* number of count that allocated from this queue */
#endif
};
static struct _xbuf_pool v_xbuf_pool[XBUF_POOL_COUNT] = {
    {0,       0, 0, 0, 0, NULL,  0, XBUF_POOL_INIT_NULL}, /* to do: use this for cloned packet, xbuf that uses platform buffer (not now) */
    {64,     0, 0, 50, 0, NULL, 0, XBUF_POOL_INIT_NULL},
    {128,   1, 0, 2, 0, NULL, 0, XBUF_POOL_INIT_NULL},
    {600,   1, 0, 1, 0, NULL, 0, XBUF_POOL_INIT_NULL},
    {1100, 1, 0, 0, 0, NULL, 0, XBUF_POOL_INIT_NULL},
    {1600, 0, 0, 2, 0, NULL, 0, XBUF_POOL_INIT_NULL}, /* used only for write */
};

static int v_xbuf_total_count = 0; /* for debug purpose */

#ifdef STAT_XBUF
sal_spinlock v_xbuf_stat_lock;
#endif

LOCAL struct xbuf* _xbuf_create(int size)
{
    char* buf;
    struct xbuf* xb;
    int alloc_size;
    int xb_size = (sizeof(struct xbuf) + 3) / 4 * 4;
    alloc_size = size + xb_size;
    
    buf = sal_malloc(alloc_size);

    if (!buf) {
        return NULL;
    }
    xb = (struct xbuf*) buf;    
    sal_memset(xb, 0, sizeof(struct xbuf));
#ifdef DEBUG
    xb->magic = XBUF_MAGIC;
#endif
    xb->head = buf+xb_size;
    xb->len = 0;
    xb->buflen = size;
    xb->data = xb->head;
    v_xbuf_total_count++;
    return xb;
}

LOCAL void _xbuf_destroy(struct xbuf* xb)
{
#ifdef XPLAT_XBUF_DYNAMIC    
    if (xb->dynamic)
        v_xbuf_pool[xb->pool_index].dynamic_count--;
#endif
    sal_free(xb);
    v_xbuf_total_count--;
}

#ifdef XPLAT_XBUF_DYNAMIC
struct xbuf* dyn_xbuf_alloc(int pool_index)
{
    struct xbuf* xb;
    xb = _xbuf_create(v_xbuf_pool[pool_index].bytes);
    if ( !xb ) {
        return NULL;
    }
    v_xbuf_pool[pool_index].dynamic_count++;
    xb->queue = NULL;
    sal_atomic_set(&xb->ref, 1);
    xb->clonee = 0;
    xb->owner = NULL;
    xb->sys_buf = NULL;
    xb->dynamic = 1;
    return xb;
}
#endif

void xbuf_dump_stat(void)
{
    int i;
    sal_error_print("Xbuf statistics\n");
#ifdef STAT_XBUF
    sal_error_print("Bytes\tStatic\tAvail\tUsed\tMax\tTotal Access\n");
    for(i=0;i<XBUF_POOL_COUNT;i++) {
        sal_error_print("%d\t%d\t%d\t%d\t%d\t%d\n",
            v_xbuf_pool[i].bytes, 
            v_xbuf_pool[i].xbuf_count, 
            v_xbuf_pool[i].queue ? v_xbuf_pool[i].queue->qlen : 0, 
            v_xbuf_pool[i].current_usage, 
            v_xbuf_pool[i].max_usage, 
            v_xbuf_pool[i].usage_count
        );
    }
#else
    sal_error_print("Bytes\tInitial\tFree\n");
    for(i=0;i<XBUF_POOL_COUNT;i++) {
        sal_error_print("%d\t%d\t%d\t\n",
            v_xbuf_pool[i].bytes, 
            v_xbuf_pool[i].xbuf_count, 
            v_xbuf_pool[i].queue ? v_xbuf_pool[i].queue->qlen : 0
        );
    }
#endif
}

#ifdef STAT_XBUF
#define XBUF_DEBUG_INTERVAL (20*SAL_TICKS_PER_SEC)
static
inline
void
QUEUE_STAT_DUMP() {
    int ret;
    dpc_id dpcid;
    dpcid = dpc_create(DPC_PRIO_NORMAL,_xbuf_dump, NULL, NULL, NULL);
    if(dpcid) {
        ret = dpc_queue(dpcid, XBUF_DEBUG_INTERVAL);
        if(ret < 0) {
            dpc_destroy(dpcid);
        }
    }
}
LOCAL int _xbuf_dump(void* p, void* p2)
{
    xbuf_dump_stat();
    QUEUE_STAT_DUMP();
    return 0;
}
#else
#define QUEUE_STAT_DUMP() do {} while(0)
#endif

int xfer_unit = 64;

ndas_error_t xbuf_pool_init(xint32 transfer_unit)
{
    ndas_error_t err;

    xfer_unit = transfer_unit;
    v_xbuf_pool[1].read_incremental = xfer_unit * 2 + 1;
    v_xbuf_pool[1].write_incremental = xfer_unit * 5+ 1;
#ifdef USE_XBUF_FROM_NET_BUF_AT_TX    
    v_xbuf_pool[5].write_incremental = 2;
#else
    v_xbuf_pool[5].write_incremental = (xfer_unit + 1) * 1024 / ((xint32) 1600); 
#endif
        /* this buffer is mainly used when writing */
    debug_xbuf(3, "xbuf_pool_init transfer_unit=%d", xfer_unit);

    err = xbuf_pool_inc(FALSE, TRUE);
    if ( !NDAS_SUCCESS(err) ) 
        return err;
#ifdef STAT_XBUF
    sal_spinlock_create("xs_lock", &v_xbuf_stat_lock);
    QUEUE_STAT_DUMP();
#endif
    
    return NDAS_OK;
}

void xbuf_pool_shutdown(void)
{
    struct xbuf* xb;
    int cnt=0;
    int i;
#if defined(DEBUG) && defined(STAT_XBUF)
    xbuf_dump_stat();
#endif
    for(i=0;i<XBUF_POOL_COUNT;i++) {
        debug_xbuf(1, "xbuf_queue_destroy:%d(queue=%p)\n", i, v_xbuf_pool[i].queue);
        if (v_xbuf_pool[i].queue==NULL)
            continue;
        while((xb=xbuf_dequeue(v_xbuf_pool[i].queue))!=NULL) {
            cnt++;
            _xbuf_destroy(xb);
        }    
        xbuf_queue_destroy(v_xbuf_pool[i].queue);
        sal_free(v_xbuf_pool[i].queue);
        v_xbuf_pool[i].queue = NULL;
    }
    if (v_xbuf_total_count !=0) {
        debug_xbuf(1,"%d of xbufs are not freed", v_xbuf_total_count);
    }
}
int xbuf_pool_size(void) 
{
    return 0;
}
ndas_error_t xbuf_pool_inc(xbool inc, xbool write_mode)
{
    int i ,j;
    struct xbuf *xb;
    
    for( i = 0;i < XBUF_POOL_COUNT; i++) 
    {
        int size;
        if (inc) {
            size = write_mode?v_xbuf_pool[i].write_incremental:v_xbuf_pool[i].read_incremental;
        } else { 
            size = v_xbuf_pool[i].default_count;    
        }
        if ( v_xbuf_pool[i].queue == NULL ) {
            v_xbuf_pool[i].queue = sal_malloc(sizeof(struct xbuf_queue));
            if ( v_xbuf_pool[i].queue == NULL ) {
                j=0;
                goto out;
            }
            xbuf_queue_init(v_xbuf_pool[i].queue);
        }
        for( j = 0; j < size; j++) 
        {
            xb = _xbuf_create(v_xbuf_pool[i].bytes);
            if ( !xb ) {
                goto out;
            }
            xb->pool_index = i;
            xbuf_queue_head(v_xbuf_pool[i].queue, xb);
            v_xbuf_pool[i].xbuf_count++;
        }
    }
    return NDAS_OK;
out:

    while( i ) {
        while( j-- ) {
            xb = xbuf_dequeue(v_xbuf_pool[i].queue);
            if ( xb ) {
                _xbuf_destroy(xb);
                v_xbuf_pool[i].xbuf_count--;
            }
        }    
/*        if ( v_xbuf_pool[i].queue->qlen <= 0 ) { // race
            xbuf_queue_destroy(v_xbuf_pool[i].queue);
            sal_free(v_xbuf_pool[i].queue);
            v_xbuf_pool[i].queue = NULL;
        }*/
        i--;
        j = v_xbuf_pool[i].default_count * inc;
    }
    return NDAS_ERROR_OUT_OF_MEMORY;    
}
ndas_error_t xbuf_pool_dec(xbool dec, xbool write_mode)
{
    int i,j, size;
    struct xbuf* xb;
    struct xbuf_queue *xq;
#if defined(DEBUG) && defined(STAT_XBUF)
    xbuf_dump_stat();
#endif
    for(i = 0 ; i < XBUF_POOL_COUNT; i++) 
    {
        if (dec) {
            size = (write_mode)?v_xbuf_pool[i].write_incremental:v_xbuf_pool[i].read_incremental;
        } else {
            size = v_xbuf_pool[i].default_count;
        }
        xq = v_xbuf_pool[i].queue;
        if ( !xq ) continue;
        for ( j = 0 ; j < size ; j++ ) {
            xb = xbuf_dequeue(xq);
            if ( xb ) {
                _xbuf_destroy(xb);
                v_xbuf_pool[i].xbuf_count--;
            }
        }    
/*        if ( xq->qlen <= 0 ) { // race
            xbuf_queue_destroy(xq);
            sal_free(xq);
            v_xbuf_pool[i].queue = NULL;
        }*/
    }

    return NDAS_OK;
}


struct xbuf* xbuf_alloc(int size)
{
    struct xbuf *xb;
    int pool_index;

    debug_xbuf(3,"size=%d", size);
    // to do: optimize this
    if (size<=64) { // Most frequent case. ignore pool 0 right now..
        pool_index = 1;
    } else if (1100<=size && size<=1600) {
        pool_index = 5;
    } else if (size<=128) {
        pool_index = 2;
    } else if (size<=600) {
        pool_index = 3;
    } else if (size<=1100) {
        pool_index =4;
    } else {
        debug_xbuf(1, "Too large buffer requested %d\n", size);
        return NULL;
    }

#ifdef STAT_XBUF
    sal_spinlock_take_softirq(v_xbuf_stat_lock);
    v_xbuf_pool[pool_index].current_usage++;
    if (v_xbuf_pool[pool_index].current_usage > v_xbuf_pool[pool_index].max_usage) {
        v_xbuf_pool[pool_index].max_usage = v_xbuf_pool[pool_index].current_usage;
    }
    v_xbuf_pool[pool_index].usage_count++;
    sal_spinlock_give_softirq(v_xbuf_stat_lock);
#endif    
    if ((xb = xbuf_dequeue(v_xbuf_pool[pool_index].queue)) == NULL) {
#ifdef XPLAT_XBUF_DYNAMIC
//        sal_debug_print("Dyn xbuf alloc: %d\n", size);
        xb= dyn_xbuf_alloc(pool_index);
        xb->pool_index = pool_index;
        return xb;
#else
        sal_debug_print("Out of buffer for size %d", size);
        return NULL;
#endif
    }
    xb->pool_index = pool_index;
    xb->queue = NULL;
    sal_atomic_set(&xb->ref, 1);
    xb->clonee = NULL;
    xb->owner = NULL;
    xb->sys_buf = NULL;
    return xb;
}

void xbuf_free(struct xbuf* xb)
{
    int last_ref;

    debug_xbuf(3,"xb=%p", xb);
    debug_xbuf(9,"xb=%s", DEBUG_XLIB_XBUF(xb));

#ifdef DEBUG
    if (xb->queue == v_xbuf_pool[xb->pool_index].queue) {
        sal_assert(0); /* Already free xbuf */
        sal_error_print("Already in free queue\n");
    }
    // check already freed xbuf 
/*    {
        struct xbuf_queue* freeq = v_xbuf_pool[xb->pool_index].queue;
        struct xbuf * ite;
        unsigned long flags;
        xbuf_qlock(freeq, &flags);
        for(ite=freeq->next;ite!=(struct xbuf * )freeq;ite=ite->next) {
            if (ite==xb) {
                sal_error_print("Already freed xbuf\n");
                sal_assert(0);
            }
        }
        xbuf_qunlock(freeq, &flags);
    } */

#endif
    
    last_ref = sal_atomic_dec_and_test(&xb->ref);

    if (!last_ref) {
        /* Cloned xbuf is using my buffer */
        return;
    }
    xbuf_unlink(xb);

    /*
     * Call user's destruct handler.
     */
    if(xb->destruct_handler) {
        xb->destruct_handler(xb, xb->handler_userdata);
    }

    if (xb->clonee) {
//        sal_debug_print("Freeing xb clonee\n");
        xbuf_free(xb->clonee);
    }

    xb->userlen = 0;
    xb->len = 0;
    xb->data = xb->head;
    xb->owner = 0;
    xb->xbuf_flags = 0;
    xb->nr_data_segs = 0;
    xb->destruct_handler = NULL;
    xb->handler_userdata = NULL;

#if 0
    // After the reference count has gone to zero,
    // must not increase again.
    if (xb->ref) { 
        debug_xbuf(1, "someone is using xb's data. Don't free xb=%s",DEBUG_XLIB_XBUF(xb));
        return; /* someone is using xb's data. Don't free data */
    }
#endif
    if (xb->sys_buf) {
        sal_net_free_platform_buf(xb->sys_buf);
        xb->sys_buf = NULL;
    }
#if 0
#ifdef XPLAT_XBUF_DYNAMIC  // to do: Destroy pool if buffer is not used for a while 
    if ( xb->dynamic ) {
#ifdef DEBUG    
        debug_xbuf(2, "Removing dynamic buffer", xb->buflen);
#endif
        _xbuf_destroy(xb);
        return;
    }
#endif
#endif
#ifdef STAT_XBUF
    sal_spinlock_take_softirq(v_xbuf_stat_lock);
    v_xbuf_pool[xb->pool_index].current_usage--;
    sal_spinlock_give_softirq(v_xbuf_stat_lock);
#endif
    if (v_xbuf_pool[xb->pool_index].queue==NULL) {
        sal_debug_print("xb->pool_index=%d\n", xb->pool_index);
    }
    xbuf_queue_head(v_xbuf_pool[xb->pool_index].queue, xb);
    debug_xbuf(3,"ed xb=%p", xb);
}

#endif /* USE_SYSTEM_MEM_POOL */
