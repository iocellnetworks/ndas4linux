#ifndef _SYSLIBS_XBUF_H_
#define _SYSLIBS_XBUF_H_
/* 
    Buffer management mechanism for Netdisk.
    Reduced version based on sk_buff and optimized for LPX protocol
*/    


#define XBUF_MAX_NETWORK_HEADER_SIZE 16

#define XBUF_MAGIC 0xabcd1236

#define XBUFQ_FLAG_AVAIL_EVENT  0x0001

struct xbuf_queue {
#ifdef DEBUG
    int magic;
#endif
    struct xbuf *next;  /* Must be first member. Can be casted from xbuf */
    struct xbuf *prev;  /* Must be second member. Can be casted from xbuf */
    sal_spinlock    lock;
    xuint16        qlen;
    xuint16     flags;
    sal_event avail;  /* changed to use single global semaphore for xbuf */
};

/*
 * XBuffer destruct handler
 */
typedef void (*xbuf_destruct_handler)(struct xbuf * xbuf, void * userdata);


/*
 *    Assume network layer header and data is continous
 * 
 * concept from struct sk_buff of linux    kernel
 * used on kernel mode network opertion, and lanscsi (not yet)
 * Objective : reduce the number of memory copy. 
 *             abstracted view of network packet buffer(data)
 * clone duplicate mac/lpx header only, share clonee's data field
 * 
 */
 
 //
 // Indicate system network buffer is detachable.
 // There are two case where system network buffer can be detachable when
 //  1. system network buffer will not be used again after sending to link layer.
 //  2. system network buffer can be re-built by using xbuffer's information
 //     even though system network buffer will be needed again.
 //
#define XBUF_FLAG_SYS_NETBUF_REBUILD        0x0001
#define XBUF_FLAG_FOLLOWING_PACKET_EXIST    0x0002

struct xbuf {
#ifdef DEBUG
    int magic;
#endif

    struct xbuf *next;  /* Must be first member. Can be casted to xbuf_queue */
    struct xbuf *prev;  /* Must be second member. Can be casted to xbuf_queue */
    struct xbuf_queue * queue;
    void*    owner;
    // 16 bytes

    char*   mach; /* MAC header. Can be pointer to system network buffer. Can be null for send packet. */
    char*    nh;    /* Network layer header. Can be pointer to system network buffer */
    xuint16     nhlen;  /* Network header length */
    xuint16    userlen; /* User's requested data buffer length */
    xuint16    len;    /* Data length including padding */
    xuint16 buflen; /* Buffer length allocated */
    // 32 bytes

    char*    head;    /* Head of buffer */
        /* head and tail is not changed as long as buffer memory is not re-allocated */
    char*    data;    /* Data head pointer */
                    /* End of data pointer: data+len */

    unsigned int xbuf_flags; /* flags */
    sal_atomic  ref;    /* Reference counter. Increased when alloc/clone, decreased when free */
    // 48 bytes

    xbuf_destruct_handler destruct_handler; /* Called when reference count is zero. */
    void *handler_userdata;     /* handler's userdata */

    struct xbuf *clonee; /* Cloned source. NULL if this is clone source */
    sal_net_buff    sys_buf; /* system dependent network buffer such as sk_buff, mbuf, pbuf */
    // 64 bytes

    int      sys_buf_len; /* System buffer allocation size. Required when copying system buffer */
    xuint16 pool_index; /* Index of free pool that this xbuf was sotred */
    xuint16 prot_seqnum;

    xsize_t    nr_data_segs;    /* Number of segs */
    struct sal_mem_block data_segs[SAL_NET_MAX_BUF_SEG_COUNT];

#ifndef USE_SYSTEM_MEM_POOL
#ifdef XPLAT_XBUF_DYNAMIC
    /* 0 if statically allocated, 1 if dynamically allocated */
    xuint8     dynamic:1;
#endif
#endif
};


static
inline
int
xbuf_get_netbuf_seg(
    struct xbuf *xb,
    sal_net_buf_seg *bufs
)
{
    bufs->data_len = xb->sys_buf_len;
    bufs->mac_header.ptr = xb->mach;
    bufs->mac_header.len = 0; // TODO: fill mac header length
    bufs->network_header.ptr = xb->nh;
    bufs->network_header.len = xb->nhlen;

//    sal_debug_print("xbuf_get_netbuf_seg: nr_data_segs=%d", (int)xb->nr_data_segs);
    bufs->nr_data_segs = xb->nr_data_segs;
    if (bufs->nr_data_segs) {
        sal_assert(xb->nr_data_segs <= SAL_NET_MAX_BUF_SEG_COUNT);
        sal_memcpy(
            bufs->data_segs,
            xb->data_segs,
            sizeof(struct sal_mem_block) * bufs->nr_data_segs);
    } else {
        return NDAS_ERROR;
    }

    return NDAS_OK;
}

static
inline
void xbuf_set_sys_netbuf_rebuild(struct xbuf *xb)
{
    sal_assert(xb->sys_buf); // Only when system network buffer is vaild.
    xb->xbuf_flags |= XBUF_FLAG_SYS_NETBUF_REBUILD;
}

static
inline
void xbuf_reset_sys_netbuf_rebuild(struct xbuf *xb)
{
    xb->xbuf_flags &= ~XBUF_FLAG_SYS_NETBUF_REBUILD;
}

static
inline
int xbuf_is_sys_netbuf_rebuild(struct xbuf *xb)
{
    return (xb->xbuf_flags & XBUF_FLAG_SYS_NETBUF_REBUILD) != 0;
}

static
inline
void xbuf_set_following_packet_exist(struct xbuf *xb)
{
    xb->xbuf_flags |= XBUF_FLAG_FOLLOWING_PACKET_EXIST;
}

static
inline
int xbuf_does_following_packet_exist(struct xbuf *xb)
{
    return (xb->xbuf_flags & XBUF_FLAG_FOLLOWING_PACKET_EXIST) != 0;
}

static
inline
void xbuf_reset_following_packet_exist(struct xbuf *xb)
{
    xb->xbuf_flags &= ~XBUF_FLAG_FOLLOWING_PACKET_EXIST;
}

static
inline
int xbuf_is_shared(struct xbuf *xb)
{
    if(sal_atomic_read(&xb->ref) > 1) {
        return 1;
    }

    if(xb->sys_buf) {
        return sal_net_is_platform_buf_shared(xb->sys_buf);
    }
    return 0;
}

#define xbuf_is_clone(xb)         ((xb->clonee)?1:0)
#define XBUF_QUEUE(xb)          ((struct xbuf_queue *)xb->queue)
#define XBUF_MAC_HEADER(xb)     ((sal_ether_header*)(xb->mach))
#define XBUF_NETWORK_HEADER(xb) (xbuf_is_clone(xb)?xb->clonee->nh:xb->nh)
#define XBUF_NETWORK_DATA(xb)   (xbuf_is_clone(xb)?xb->clonee->data:xb->data)
#define XBUF_NETWORK_DATA_LEN(xb)       (xb->len)
#define XBUF_NETWORK_TOTAL_LEN(xb)      (xb->nhlen + xb->len)
#define XBUF_NETWORK_DATA_ALIGNED(xb)   ((XBUF_NETWORK_DATA(xb)%4)==0)
#define XBUF_SG_BUFFER(xb)      (xbuf_is_clone(xb)?xb->clonee->data_segs:xb->data_segs)

#define xbuf_reserve_header(xb, _nhlen)  do {     sal_assert(!xbuf_is_clone(xb));\
        xb->nh = (void*)(xb->head); \
        xb->data = xb->nh + _nhlen; \
        xb->len = 0; \
        xb->nhlen = _nhlen;  \
     } while(0)

static
inline
struct xbuf *
xbuf_get(struct xbuf *xb)
{
    sal_atomic_inc(&xb->ref);
    return xb;
}

static
inline
void
xbuf_set_destruct_handler(
    struct xbuf *xb,
    xbuf_destruct_handler destruct_handler,
    void * userdata)
{
    sal_assert(xb->destruct_handler == NULL);
    sal_assert(xb->handler_userdata == NULL);

    xb->destruct_handler = destruct_handler;
    xb->handler_userdata = userdata;
}

static
inline
void
xbuf_reset_destruct_handler(
    struct xbuf *xb
)
{
    xb->destruct_handler = NULL;
    xb->handler_userdata = NULL;
}

extern void xbuf_free(struct xbuf* xb);

#ifdef DEBUG_MEMORY_LEAK
#define xbuf_alloc(size) xbuf_alloc_debug(size, __FILE__, __LINE__)
extern struct xbuf* xbuf_alloc_debug(int size, const char *file, int line);
#else
extern struct xbuf* xbuf_alloc(int size);
#endif

extern struct xbuf* xbuf_alloc_from_net_buf(sal_net_buff nbuf, sal_net_buf_seg *bufs);

/*
 * Initiailize the xbuf pool.
 */
extern ndas_error_t xbuf_pool_init(xint32 transfer_unit);
extern void xbuf_pool_shutdown(void);

/* 
 * Increase the size of xbuf pool.
 * inc: if TRUE, pool will be increased according to the transfer unit
 *      if FALSE, pool will be increased by the default value for initialization
 */
ndas_error_t xbuf_pool_inc(xbool inc, xbool write_mode);
/* 
 * Decrease the size of xbuf pool.
 * dec: if TRUE, pool will be decreased according to the transfer unit
 *      if FALSE, pool will be decreased by the default value for initialization
 */
ndas_error_t xbuf_pool_dec(xbool dec, xbool write_mode);

/*
 *  xbuf queue
 */
extern ndas_error_t xbuf_queue_init(struct xbuf_queue* q);
extern void xbuf_queue_reset(struct xbuf_queue* q);
extern void xbuf_queue_destroy(struct xbuf_queue* q);

extern void xbuf_queue_head(struct xbuf_queue *q, struct xbuf * xb);

extern void xbuf_queue_tail(struct xbuf_queue *list, struct xbuf *xb);

/*
  Insert at the tail without queue lock acquisition.
 */
static
inline
void
_xbuf_queue_tail(
    struct xbuf_queue *list,
    struct xbuf *xb)
{
    struct xbuf *prev, *next;

    sal_assert(list && xb);
    xb->queue = list;
    if ( list->qlen == 0 && (list->flags & XBUFQ_FLAG_AVAIL_EVENT)) {
        sal_event_set(list->avail);
    }
    list->qlen++;
    next = (struct xbuf *)list;
    prev = next->prev;
    xb->next = next;
    xb->prev = prev;
    next->prev = xb;
    prev->next = xb;
}

/*
  Dequeue without queue lock acquisition.
 */
static
inline
struct xbuf *
_xbuf_dequeue(
    struct xbuf_queue *list
){
    struct xbuf *next, *head, *result;

    sal_assert(list != NULL);

    head = (struct xbuf *)list;
    next = head->next;
    sal_assert(next != NULL);
    if (next != head) {
        // Get the first entry.
        result = next;

        // Remove from the list
        next = next->next;
        next->prev = head;
        head->next = next;

        // Set zero to queue field.
#ifdef DEBUG
        result->next = NULL;
        result->prev = NULL;
#endif
        result->queue = NULL;

        list->qlen--;
        if (list->qlen == 0 && (list->flags & XBUFQ_FLAG_AVAIL_EVENT))  {
            sal_event_reset(list->avail);
        }
    } else {
        result = NULL;
    }

    return result;
}


static
inline
void
_xbuf_unlink(struct xbuf * xb)
{
    struct xbuf * next, * prev;
    struct xbuf_queue *list;

    list = xb->queue;
    sal_assert(list);
    xb->queue = NULL;
    list->qlen--;

    next = xb->next;
    prev = xb->prev;
    next->prev = prev;
    prev->next = next;

#ifdef DEBUG
    xb->next = NULL;
    xb->prev = NULL;
#endif
}

static
inline
void
xbuf_qlock(struct xbuf_queue *q, unsigned long *flags) {
     sal_spinlock_take_softirq(q->lock);
}

static
inline
void
xbuf_qunlock(struct xbuf_queue *q, unsigned long *flags) {
     sal_spinlock_give_softirq(q->lock);
}

/* Be carefull with using xbuf_queue_empty for the race conditions */
#define xbuf_queue_empty(q) ((q)->next == (struct xbuf *) (q))
#define xbuf_peek(list) (xbuf_queue_empty(list)?NULL:(list)->next)

/* Remove one xbuf from header */
/* 
 * @sync - if non-zero, it will hold the list lock
 *         if zero, it assume that the list lock is already hold.
 */
extern struct xbuf *xbuf_dequeue(struct xbuf_queue *list);
extern struct xbuf *xbuf_dequeue_wait(struct xbuf_queue *list, sal_tick tout);

/* 
 * @sync - if non-zero, it will hold the list lock
 *         if zero, it assume that the list lock is already hold.
 */
extern void xbuf_unlink(struct xbuf* xb);
extern char* xbuf_pull(struct xbuf* xb, int len);
/* Remove len from end of buffer */
#define xbuf_trim(xb, _len)  do { sal_assert(xb->len>=_len); xb->len -=_len;  } while(0)
extern char* xbuf_put(struct xbuf* xb, unsigned int len);

extern int xbuf_copy_data_to(struct xbuf* xb, int offset, char* buf, int len);

/* 
 * Copy xbuf to user-supplied buffers 
 * @offset: offset from data region of xbuf
 * @user_blocks: data blocks provided by user
 * @nr_user_blocks: number of data blocks provided by user
 * @len: size of data to be copied into the data blocks provided by user
 */
extern int xbuf_copy_data_to_blocks(struct xbuf* xb, int offset, 
                    struct sal_mem_block* user_blocks, int nr_user_blocks, int len);
extern int xbuf_copy_data_from(struct xbuf* xb, char* buf, int len);
extern struct xbuf* xbuf_clone(struct xbuf* xb);
extern struct xbuf* xbuf_copy(struct xbuf* xb);

#ifdef DEBUG
#define DEBUG_XLIB_XBUF(xb) \
({\
    static char __buf__[2024];\
    if ( xb ) \
        sal_snprintf(__buf__,sizeof(__buf__),"struct xbuf(%p)={nh(%p)={%s},ref=%d,clonee=%p,next=%p,prev=%p,queue=%p,owner=%p,data(%p,%d bytes)={%s}}",\
            xb,\
            xb->nh,\
            SAL_DEBUG_HEXDUMP(xb->nh, xb->nhlen),\
            sal_atomic_read(&xb->ref),\
            xb->clonee,\
            xb->next,\
            xb->prev,\
            xb->queue,\
            xb->owner,\
            xb->data,\
            xb->len,\
            SAL_DEBUG_HEXDUMP(xb->data, xb->len)\
        );\
    else\
        sal_snprintf(__buf__,sizeof(__buf__),"NULL");\
    (const char*) __buf__;\
})
#else
#define DEBUG_XLIB_XBUF(xb) ({"";})
#endif

#endif /* _SYSLIBS_XBUF_H_ */
