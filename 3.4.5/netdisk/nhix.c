#include "xplatcfg.h"
#include <sal/libc.h>
#include <sal/sync.h>
#include <sal/time.h>
#include <xlib/gtypes.h> // g_ntohl g_htonl g_ntohs g_htons
#include "lpx/lpx.h"
#include "lpx/lpxutil.h"
#include "netdisk/ndashix.h"
#include "netdisk/list.h"
#include "netdisk/ndev.h"
#include "netdisk/sdev.h"
#include "registrar.h"
#include "nhix.h"
#include "uuid.h"
#ifdef XPLAT_NDASHIX

#ifndef XPLAT_SIO
#error XPLAT_SIO should be set to use NDAS HIX
#endif

#define HIX_DEFTIMEOUT_SA SAL_TICKS_PER_SEC
#define HIX_DEFTIMEOUT_DC SAL_TICKS_PER_SEC
#define HIX_DEFTIMEOUT_HI (SAL_TICKS_PER_SEC / 5 * 4)
#define HIX_HOSTINFO_LIFE (300 * SAL_TICKS_PER_SEC)

#ifdef DEBUG
#include "sal/debug.h"
#define    debug_hix(l, x...)    \
do {\
    if(l <= DEBUG_LEVEL_HIX ) {     \
        sal_debug_print("HIX|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x); \
    } \
} while(0) 
inline static 
const char* DEBUG_HIX_HEADER(hix_header_ptr h) {
    char* type_s[] ={ 
        "HIX_TYPE_DISCOVER", 
        "HIX_TYPE_QUERY_HOST_INFO",
        "HIX_TYPE_SURRENDER_ACCESS",
        "HIX_TYPE_DEVICE_CHANGE",
        "HIX_TYPE_UNITDEVICE_CHANGE"
    };
    static char buf[1024];
    
    if ( h ) {
        unsigned int type = h->type; /* to get the address in HEXDUMP */
        sal_snprintf(buf,sizeof(buf), "(%p)={signature=%c%c%c%c,revision=%d,"
            "%s,type=%s,length=%d,hostid=%s}",
            h,((char*)h)[0],((char*)h)[1],((char*)h)[2],((char*)h)[3],
            h->revision,
            (h->reply_flag)?"reply":"request",
            (type>0 && type <= HIX_TYPE_UNITDEVICE_CHANGE) ?
                type_s[type-1]: SAL_DEBUG_HEXDUMP_S((xuchar*)&type,2),
            g_ntohs(h->length),
            SAL_DEBUG_HEXDUMP_S(h->hostid,16)
        );
    }
    else 
        sal_snprintf(buf,sizeof(buf),"NULL");
    return (const char*) buf;
}

inline static 
const char* DEBUG_HIX_DISCOVER_REQUEST(hix_dc_request_ptr dc) {
    static char buf[1024];
    if ( dc ) {
        char* p = buf;
        int i = 0;
        p += sal_snprintf(p,sizeof(buf),"(%p)=h%scount=%d,{",
            dc,DEBUG_HIX_HEADER(&dc->header), dc->entry_count);
        for (; i < dc->entry_count;i++) {
            p+= sal_snprintf(p,sizeof(buf)-(p-buf),"entry={%s:u%d,access_type=0x%x}",
                SAL_DEBUG_HEXDUMP_S(dc->entries[i].network_id, 6),
                dc->entries[i].unit,dc->entries[i].access_type);
        sal_snprintf(p,sizeof(buf)-(p-buf), "}");
        }
    } else {
        sal_snprintf(buf,sizeof(buf), "NULL");
    }
    return (const char *) buf;
}

inline static 
const char* DEBUG_HIX_DISCOVER_REPLY(hix_dc_reply_ptr dc) {
    static char buf[1024];
    if ( dc ) 
    {
        char* p = buf;
        int i = 0;
        p += sal_snprintf(p,sizeof(buf),"(%p)=h%scount=%d,{",
            dc,DEBUG_HIX_HEADER(&dc->header), dc->entry_count);
        for (; i < dc->entry_count;i++) {
            p+= sal_snprintf(p,sizeof(buf)-(p-buf),"entry={%s:u%d,access_type=0x%x}",
                SAL_DEBUG_HEXDUMP_S(dc->entries[i].network_id, 6),
                dc->entries[i].unit,dc->entries[i].access_type);
        sal_snprintf(p,sizeof(buf)-(p-buf), "}");
        }
    } else {
        sal_snprintf(buf,sizeof(buf), "NULL");
    }
    return (const char *) buf;
}

inline static
const char* DEBUG_HIX_HOSTINFO_CLASS(xuint32 classes) 
{
    static char buf[1024];  
    char *descs[] = {
        "OS",
        "OS_VER_INFO",
        "HOSTNAME",
        "FQDN",
        "NETBIOSNAME",
        "UNI_HOSTNAME",
        "UNI_FQDN",
        "UNI_NETBIOSNAME",
        "ADDR_LPX",
        "ADDR_IPV4",
        "ADDR_IPV6",
        "NDAS_SW_VER_INFO",
        "NDFS_VER_INFO",
        "HOST_FEATURE", 
        "TRANSPORT"
    };
    char *p = buf;
    int i; 
    xuint32 flag = 0x1;
    p += sal_snprintf(p, sizeof(buf) - (p-buf), "classes(0x%x)={", classes);
    for ( i = 0 ; i < sizeof(descs)/sizeof(descs[0]); i++, flag<<=1 ) {
        if ( flag & classes ) {
            p += sal_snprintf(p, sizeof(buf) - (p-buf), "%s|", descs[i]);
        }
    }
    p += sal_snprintf(p, sizeof(buf) - (p-buf), "}");
    return (const char *) buf;
}

inline static 
const char* DEBUG_HIX_HOSTINFO_REPLY(hix_hi_reply_ptr hi) 
{
    static char buf[1024];
    if ( hi ) 
    {
        char* p = buf;
        hix_hi_entry_ptr entry = &hi->data.entries[0];
        int i = 0;
        p += sal_snprintf(p,sizeof(buf),"(%p)=h%s%s,count=%d,{",
            hi,
            DEBUG_HIX_HEADER(&hi->header), 
            DEBUG_HIX_HOSTINFO_CLASS(g_ntohl(hi->data.contains)),
            hi->data.count);
        for (; i < hi->data.count;i++) 
        {
            p += sal_snprintf(p,sizeof(buf)-(p-buf),
                "entry[%d]={%s:%s}",
                i, 
                DEBUG_HIX_HOSTINFO_CLASS(g_ntohl(entry->class)),
                SAL_DEBUG_HEXDUMP(entry->data, entry->length - (1+4))
            );
            sal_snprintf(p,sizeof(buf)-(p-buf), "}");
            entry = (hix_hi_entry_ptr) (((char*)entry) + entry->length);
        }
    } else {
        sal_snprintf(buf,sizeof(buf), "NULL");
    }
    return (const char *) buf;
}
#else
#define debug_hix(l,x...) do {} while(0)
#define DEBUG_HIX_HEADER(x) ({ ""; });
#define DEBUG_HIX_DISCOVER_REQUEST(x) ({ ""; });
#define DEBUG_HIX_DISCOVER_REPLY(x) ({ ""; });
#define DEBUG_HIX_HOSTINFO_REPLY(x) ({ ""; });
#endif

/* some machines has problem in alignment.
 * ie: Roku mipsel CPU.
 */        
#ifdef __LITTLE_ENDIAN_BYTEORDER
static inline void set_16(xuchar *left,xuint16 right) 
{
    left[0] = (xuchar) ((right & (xuint16) 0xff00U) >> 8);
    left[1] = (xuchar) (right & (xuint16) 0x00ffU);
}

static inline void set_32(xuchar *left,xuint32 right) 
{
    left[0] = (xuchar) ((right & (xuint32)0xff000000U) >> 24);
    left[1] = (xuchar) ((right & (xuint32)0xff0000U) >> 16);
    left[2] = (xuchar) ((right & (xuint32)0xff00U) >> 8);
    left[3] = (xuchar) (right & (xuint32)0x00ffU);
}

#define get_16(ptr) ({ \
    ((xuint16)((xuchar*)(ptr))[0]) |  \
        (((xuint16)((xuchar*)(ptr))[1]) << 8); \
}) 

#define get_32(ptr) ({ \
    ((xuint32)((xuchar*)(ptr))[0]) |  \
        (((xuint32)((xuchar*)(ptr))[1]) << 8) |  \
        (((xuint32)((xuchar*)(ptr))[2]) << 16 ) |  \
        (((xuint32)((xuchar*)(ptr))[3]) << 24 ); \
}) 

#else
static inline void set_32(char *left,xuint32 right) 
{ 
    left[0] = (xuchar) ((xuint32)(right) & (xuint32)0x00ffU);
    left[1] = (xuchar) (((xuint32)(right) & (xuint32)0xff00U) >> 8);
    left[2] = (xuchar) (((xuint32)(right) & (xuint32)0xff0000U) >> 16);
    left[3] = (xuchar) (((xuint32)(right) & (xuint32)0xff000000U) >> 24);
}
static inline void set_16(xuchar *left,xuint16 right) 
{ 
    left[0] = (xuchar) ((xuint16)(right) & (xuint16)0x00ffU);
    left[1] = (xuchar) (((xuint16)(right) & (xuint16)0xff00U) >> 8);
}

#define get_16(ptr) ({ \
    ((xuint16)((xuchar*)(ptr))[1]) |  \
        (((xuint16)((xuchar*)(ptr))[0]) << 8); \
}) 

#define get_32(ptr) ({ \
    ((xuint32)((xuchar*)(ptr))[3]) |  \
        (((xuint32)((xuchar*)(ptr))[2]) << 8) |  \
        (((xuint32)((xuchar*)(ptr))[1]) << 16 ) |  \
        (((xuint32)((xuchar*)(ptr))[0]) << 24 ); \
}) 


#endif

/* list item for hix_host.address_list */
struct hix_nic {
    struct list_head node;
    xuchar address[SAL_ETHER_ADDR_LEN];
};

/* list itmem for hix_host.udev_list */
struct hix_udev {
    struct list_head node;
    udev_t * udev;
    hix_uda      access_mode;
};

struct hix_host {
    uuid_t uid;
    /* the time when the information is last updated in any field */
    sal_msec updated;
    
/*<- begin of host info */    
    xuchar os_type;
    xuint32 feature;
    char hostname[NDAS_MAX_HOSTNAME_LENGTH+1];
    xuchar ipv4[4];
    xuchar ipv6[6];
    hix_hi_version  sw_ver;
    hix_hi_version  os_ver;
/*end of host info ->*/    

    /* list of struct hix_nic */
    struct list_head address_list;
    /* list of struct hix_udev */
    struct list_head udev_list;
    /* node of NDAS host list */
    struct list_head node;
}; 

struct hix_host_list {
    /* lock to gurantee the integraty of list */
    sal_spinlock lock;
    /* list of struct hix_host */    
    struct list_head list;
};
static struct hix_host_list v_host_list;

struct _hix {
    xint16 running;
    xint16 sz;
    /* server socket */
    int sockfd;
    char* buf;
    struct sockaddr_lpx *dest;
    struct sockaddr_lpx myaddr;
    uuid_t hostid;
};
static struct _hix v_hix_svr = { 
    .sockfd = -1,
    .buf = NULL,
    .sz = 0,
    .dest = NULL,
    .running = 0
};

/* 
  Find out the hix_host of the given uuid from v_host_list
  Note: this function is only called by dpc thread,
    so no mutex is necessary.
 */
static
struct hix_host * hix_host_lookup(uuid_t uuid) {
    struct list_head* i;
    //sal_spinlock_take(v_host_list.lock, SAL_SYNC_FOREVER);
    list_for_each(i, &v_host_list.list) 
    {
        struct hix_host* host = 
            list_entry(i, struct hix_host, node);
        if ( host && sal_memcmp(host->uid, uuid, sizeof(uuid_t)) ) 
            continue;
        
        //sal_spinlock_give(v_host_list.lock);
        return host;
    }
    //sal_spinlock_give(v_host_list.lock);
    return NULL;
}
static
void hix_host_destroy(struct hix_host *host)
{
    struct list_head *i,*n;
    list_for_each_safe(i,n,&host->address_list) 
    {
        struct hix_nic *nic = list_entry(i, struct hix_nic, node); 
        list_del(i);
        sal_free(nic);
    }
    list_for_each_safe(i,n,&host->udev_list) 
    {
        struct hix_udev *hudev = list_entry(i, struct hix_udev, node); 
        list_del(i);
        sal_free(hudev);
    }
    sal_free(host);   
}
static
struct hix_host* hix_host_create(uuid_t uuid)
{
    struct hix_host* host = sal_malloc(sizeof(struct hix_host));
    if ( !host ) return NULL; // out of memory
    sal_memcpy(host->uid, uuid, sizeof(uuid_t));
    INIT_LIST_HEAD(&host->address_list);
    INIT_LIST_HEAD(&host->udev_list);
    host->updated = sal_time_msec();
    return host;
}

static inline
void hix_host_add(struct hix_host* new) {
    //sal_spinlock_take(v_host_list.lock, SAL_SYNC_FOREVER);
    list_add(&new->node, &v_host_list.list);
    //sal_spinlock_give(v_host_list.lock);
}

static inline
void hix_host_del(struct hix_host* deletee) {
    //sal_spinlock_take(v_host_list.lock, SAL_SYNC_FOREVER);
    list_del(&deletee->node);
    //sal_spinlock_give(v_host_list.lock);
}

         
#define foreach_node_of_hix_host(i, nic, host) \
    /* type check */ \
    for ( i = (host)->address_list.next; \
        i != &(host)->address_list && \
        i && \
        ({ nic = list_entry(i, struct hix_nic, node); 1; }); \
        i = i->next)

#define foreach_host(i, host) \
    /* type check */ \
    { struct list_head *__i = i; } \
    { struct hix_host*__h = host; } \
    for ( i = v_host_list.list.next; \
        i != &v_host_list->list && \
        i && \
        ({ host = list_entry(i, struct hix_host, node); 1; }); \
        i = i->next)
static
struct hix_udev* hix_udev_lookup(struct hix_host* host, udev_t * udev){
    struct list_head* i;
    list_for_each(i, &host->udev_list) {
        struct hix_udev* hix_udev = 
            list_entry(i, struct hix_udev, node);
        if ( hix_udev && hix_udev->udev == udev )
            return hix_udev;
    }
    return NULL;
}

static
struct hix_udev* hix_udev_create(udev_t * udev, hix_uda access_mode) {
    struct hix_udev* hud = sal_malloc(sizeof(struct hix_udev));
    if ( !udev ) return NULL;
    hud->udev = udev;
    hud->access_mode = access_mode;
    return hud;
}

static 
void hix_init_hdr(hix_header_ptr hdr, int reply, int type,int length) 
{
    hdr->signature = g_htonl(HIX_SIGNATURE);
    hdr->revision = HIX_CURRENT_REVISION;
    hdr->reply_flag = reply ? 1 : 0;
    hdr->type = type;
    hdr->length = g_htons(length);
    sal_memcpy(hdr->hostid, v_hix_svr.hostid, sizeof(((hix_header_ptr)0)->hostid)); 
}
static hix_hi_entry_ptr put_hi_8(hix_hi_entry_ptr entry,xuint32 class, xuint8 value) 
{
    entry->class = g_htonl(class);//HIX_HIC_OS;
    entry->data[0] = value; //HIX_HIC_OS_OTHER;
    entry->length = 1 + 4 + sizeof(xuint8);
    return (hix_hi_entry_ptr)
        (((xuchar*) entry) + entry->length);
}
static 
hix_hi_entry_ptr put_hi_32(hix_hi_entry_ptr entry,xuint32 class, xuint32 value) 
{
    entry->class = g_htonl(class);
    set_32(entry->data,value); 
    entry->length = 1 + 4 + sizeof(xuint32);
    return (hix_hi_entry_ptr)
        (((xuchar*) entry) + entry->length);
}

static 
hix_hi_entry_ptr put_hi_version(hix_hi_entry_ptr entry, xuint32 class, 
    xuint16 major, xuint16 minor, xuint16 build, xuint16 private) 
{
    hix_hi_version_ptr data = (hix_hi_version_ptr) entry->data;
    entry->class = g_htonl(class);
    set_16((xuchar*)&data->major,g_htons(1));
    set_16((xuchar*)&data->minor,g_htons(1));
    set_16((xuchar*)&data->build,g_htons(1));
    set_16((xuchar*)&data->private,g_htons(1));
    entry->length = sizeof(entry->length) + sizeof(entry->class) + sizeof(hix_hi_version);
    return (hix_hi_entry_ptr)
        (((xuchar*) entry) + entry->length);
}
static 
hix_hi_entry_ptr put_hi_bytes(hix_hi_entry_ptr entry, xuint32 class,
    char *data, int size)
{
    debug_hix(8, "added data={%s}", SAL_DEBUG_HEXDUMP(data, size));
    set_32((xuchar*)&entry->class, class);
    sal_memcpy(entry->data, data, size);
    entry->length = sizeof(entry->length) + sizeof(entry->class) + size;
    debug_hix(8, "entry-length=%d", entry->length);
    debug_hix(8, "entry-skip from %p to %p", entry, (((xuchar*) entry) + entry->length));
    return (hix_hi_entry_ptr)
        (((xuchar*) entry) + entry->length);
}

static 
void put_hi_lpx_node(xuchar* node, void* arg)
{
//    hix_hi_entry_ptr* entry = (hix_hi_entry_ptr*) arg;
    char **ptr = (char**) arg;
    debug_hix(8, "added address=%s", DEBUG_LPX_NODE(node));
//    (*entry) = put_hi_bytes((*entry), HIX_HIC_ADDR_LPX, node, SAL_ETHER_ADDR_LEN);
    sal_memcpy(*ptr, node, SAL_ETHER_ADDR_LEN);
}

static 
hix_udev_entry_ptr put_udev(hix_udev_entry_ptr entry, xuchar *network_id, int unit,hix_uda mode)
{
    sal_memcpy(entry->network_id, network_id, SAL_ETHER_ADDR_LEN);
    entry->unit = unit;
    entry->access_type = mode;
    return entry + 1;
}

/* hix client */
static 
ndas_error_t hix_cli_init() 
{
    struct sockaddr_lpx myaddr;

    int fd;
    ndas_error_t err;
    
    fd = lpx_socket(LPX_TYPE_DATAGRAM, 0);

    if (!NDAS_SUCCESS(fd))
        return fd;

    LPX_ADDR_INIT(&myaddr, v_hix_svr.myaddr.slpx_node, 0);

    err = lpx_bind(fd, &myaddr,sizeof(myaddr));

    if (!NDAS_SUCCESS(err) ) {
        lpx_close(fd);
        return err;
    }
    return fd;
}


/*
 * Check if the reply header is valid
 */
static 
xbool hix_check_header(hix_header_ptr header, unsigned int type, int size) {
    return get_32(&header->signature) == g_htonl(HIX_SIGNATURE) &&
        header->revision == HIX_CURRENT_REVISION &&
        header->reply_flag &&
        get_16(&header->length) == g_htons(size) &&
        header->type == type;
}
static void 
hix_cli_on_surrender_request(lpx_siocb* cb, 
    struct sockaddr_lpx *from, void* user_arg) 
{
    hix_sa_reply_ptr reply = cb->buf;

    if ( !NDAS_SUCCESS(cb->result) ) {
        sal_free(reply);
        return;    
    }
        
    if ( !hix_check_header(&reply->header, HIX_TYPE_SURRENDER_ACCESS, cb->nbytes) ) 
    {
        return; // protocol error
    }
    //reply->status;
    sal_free(reply);
}
/*
  TODO: LPX should have the routing table 
  to figure out which interface should be used for the packet to be routed*/
static 
ndas_error_t hix_cli_surrender_request(udev_t * udev, struct hix_host *host)
{
    hix_sa_request_ptr req;
    struct hix_nic *nic;
    struct list_head *i;
    ndas_error_t err = NDAS_OK;
    
    req = sal_malloc(sizeof(hix_sa_request));
    if ( !req ) return NDAS_ERROR_OUT_OF_MEMORY;

    req->entry_count = 1;
    
    req->udev.unit = udev->info->unit;
    req->udev.access_type = HIX_UDA_RW;
    sal_memcpy(
        req->udev.network_id,
        udev->ndev->network_id, 
        SAL_ETHER_ADDR_LEN);

    hix_init_hdr(&req->header,0, HIX_TYPE_SURRENDER_ACCESS, 
                    sizeof(hix_sa_request));

    foreach_node_of_hix_host(i, nic, host) 
    {
        struct sockaddr_lpx dest;
        int fd;
        hix_sa_reply_ptr reply = sal_malloc(sizeof(hix_sa_reply));
        
        if ( !reply ) {
            err = NDAS_ERROR_OUT_OF_MEMORY;
            goto out;
        }
        LPX_ADDR_INIT(&dest, nic->address, HIX_LISTEN_PORT);

        fd = hix_cli_init();
        if ( !NDAS_SUCCESS(fd) ) {
            err = fd;
            goto loop_out;
        }

        err = lpx_sendto(fd, req, sizeof(hix_sa_request), 0, &dest, sizeof(dest));
        if ( !NDAS_SUCCESS(err) ) 
            goto loop_out;
        

        err = lpx_sio_recvfrom(fd, reply, sizeof(hix_sa_reply), 0,
                hix_cli_on_surrender_request, NULL, HIX_DEFTIMEOUT_SA);
        if ( !NDAS_SUCCESS(err) ) 
            goto loop_out;

        err = NDAS_OK;        
        continue;
    loop_out:        
        sal_free(reply);
    }
out:
    sal_free(req);
    return err;
}
static 
void hix_cli_on_discover(lpx_siocb* cb, 
    struct sockaddr_lpx *from, void* user_arg) 
{
    int i = 0, flag = 0;
    struct hix_host * host;
    udev_t * udev = (udev_t *) user_arg;
    hix_dc_reply_ptr reply = cb->buf;
    struct hix_udev* hix_udev;
    xbool new_host = FALSE;

    debug_hix(3, "ing cb=%p",cb);
    debug_hix(3, "from=%s udev=%p",    from ? DEBUG_LPX_NODE(from->slpx_node) : "TIMEDOUT", 
        udev);

    debug_hix(5, "cb->result=%d", cb->result);
    
    if ( !NDAS_SUCCESS(cb->result) ) {
        sal_free(reply);
        sal_semaphore_give((sal_semaphore)udev->hix_sema);
        return;    
    }
    
    debug_hix(8, "reply:%s", DEBUG_HIX_DISCOVER_REPLY(reply));
    debug_hix(8, "reply:%s", SAL_DEBUG_HEXDUMP(reply, cb->nbytes));
    /* verify header */
    if ( !hix_check_header(&reply->header, HIX_TYPE_DISCOVER, cb->nbytes) )
    {
        debug_hix(1, "Wrong reply for DISCOVERY for udev %s",
            DEBUG_LPX_NODE(udev->ndev->network_id));
        goto out;
    }
    debug_hix(5, "reply->entry_count=%d", reply->entry_count);

    for (i = 0 ; i < reply->entry_count; i++) {
        if ( sal_memcmp(reply->entries[i].network_id, udev->ndev->network_id, SAL_ETHER_ADDR_LEN) != 0 ||
                udev->info->unit != reply->entries[i].unit ) 
        {
            debug_hix(1, "not for me %s", DEBUG_LPX_NODE(reply->entries[i].network_id));
            continue;
        }
        flag = 1;
    }
    if ( flag == 0 ) {
        debug_hix(1, "No entry for %s", DEBUG_LPX_NODE(udev->ndev->network_id));
        goto out;
    }
    host = hix_host_lookup(reply->header.hostid);
    debug_hix(6, "looked up host=%p", host);
    if ( ! host ) {
        struct hix_nic * nic;
        debug_hix(6, "create new host for %s", DEBUG_LPX_NODE(from->slpx_node));
        host = hix_host_create(reply->header.hostid);
        if ( host == NULL ) { // out of memory
            goto out;
        }
        new_host = TRUE;
        hix_host_add(host);
        nic = sal_malloc(sizeof(struct hix_nic));
        sal_memcpy(nic->address, from->slpx_node, LPX_NODE_LEN);
        list_add(&nic->node, &host->address_list);
    }    
    debug_hix(6, "create udev entry(%s) for host %s",
        DEBUG_LPX_NODE(udev->ndev->network_id),
        DEBUG_LPX_NODE(from->slpx_node));
    
    hix_udev = hix_udev_lookup(host, udev);
    if ( hix_udev ) { /* update the access type */
        debug_hix(6, "udev entry exits, just update the info");
        hix_udev->access_mode = reply->entries[0].access_type;
        sal_semaphore_give((sal_semaphore)udev->hix_sema);
        goto out;;
    }
    /* allocate udev item for this NDAS host */
    hix_udev = hix_udev_create(udev, reply->entries[0].access_type);
    if ( hix_udev == NULL ) {
        goto out;
    }
    list_add(&hix_udev->node, &host->udev_list);
    sal_semaphore_give((sal_semaphore)udev->hix_sema);
out:
    return;
}

/* Description
 * Find out who access the unit disk.
 *
 * Parameters
 * _logdev : unit disk that we want to know who accesses.
 */
static 
ndas_error_t hix_cli_discover(udev_t * udev, hix_uda access_type) 
{
    hix_dc_request_ptr req;
    void* reply;
    hix_udev_entry_ptr entry;
    struct sockaddr_lpx bcast;
    int fd;
    int len;
    ndas_error_t err;

    debug_hix(3, "ing sdev=%p access_type=0x%x", udev, access_type);
    /* Initialize DISCOVERY REQUEST packet */
    req = sal_malloc(HIX_MAX_MESSAGE_LEN);

    if ( !req ) return NDAS_ERROR_OUT_OF_MEMORY;

    /* will de-allocated by hix_cli_on_discover or hix_cli_timedout */
    reply = sal_malloc(HIX_MAX_MESSAGE_LEN); 

    if ( !reply ) {
        sal_free(req);
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    
    sal_memset(req, 0, HIX_MAX_MESSAGE_LEN);

    req->entry_count = 1;
    
    entry = req->entries;
    entry->access_type = access_type;
    sal_memcpy(entry->network_id, udev->ndev->network_id, SAL_ETHER_ADDR_LEN);
    entry->unit = udev->info->unit;

    debug_hix(9, "device mac address=%s", DEBUG_LPX_NODE(udev->ndev->network_id));
    
    len =  sizeof(hix_header) + sizeof(req->entry_count) + 
        sizeof(hix_udev_entry) * req->entry_count;
    
    hix_init_hdr(&req->header, 0, HIX_TYPE_DISCOVER, len);

    LPX_ADDR_INIT(&bcast, LPX_BROADCAST_NODE, HIX_LISTEN_PORT);

    fd = hix_cli_init();
    if ( !NDAS_SUCCESS(fd)) {
        debug_hix(1, "fail to open s-socket");
        sal_free(req);
        sal_free(reply);
        return fd;
    }

    err = lpx_sio_recvfrom(fd, reply, sizeof(reply), SIO_FLAG_MULTIPLE_NOTIFICATION, 
        hix_cli_on_discover, udev, HIX_DEFTIMEOUT_DC + SAL_TICKS_PER_SEC);

    if ( !NDAS_SUCCESS(err) ) {
        debug_hix(1, "fail to open s-socket err=%d", err);
        lpx_close(fd);
        sal_free(req);
        sal_free(reply);
        return err;
    }

    err = lpx_sendto(fd, req, len, 0, &bcast, sizeof(bcast));

    sal_free(req);
    
    if ( !NDAS_SUCCESS(err) ) {
        debug_hix(5, "fail to send err=%d",err);
        lpx_close(fd);
        sal_free(reply);
        return err;
    }
    
    debug_hix(5, "ed err=%d",err);
    return err;
}


#if 0 /* not used */
/** Description
 * hix client handler for query_host_info reply packet 
 */
static void
hix_cli_on_host_info(lpx_siocb* cb, struct sockaddr_lpx *from, void* user_arg) 
{
    hix_hi_entry_ptr entry;
    xuint32 contains;
    struct hix_host *host = (struct hix_host *) user_arg;
    hix_hi_reply_ptr reply = cb->buf;
    
    if ( !NDAS_SUCCESS(cb->result) ) {
        sal_free(reply);
        return;    
    }
    
    if ( hix_check_header(&reply->header, HIX_TYPE_QUERY_HOST_INFO, 
                            cb->nbytes) == FALSE ) 
    {
        return;
    }
    /* count to check the boundary for str-functions. */
    reply->data.length -= sizeof(reply->data.length) + 
                            sizeof(reply->data.contains) +
                            sizeof(reply->data.count);
    
    contains = g_ntohl(get_32(&reply->data.contains));
    if ( !(contains & HIX_HIC_HOST_FEATURE &
        HIX_HIC_NDAS_SW_VER_INFO & 
        HIX_HIC_OS_VER_INFO &
        HIX_HIC_OS )) 
    {
        return; /* REQUIRED fields are not included*/
    }
        
    entry = reply->data.entries;

    while( reply->data.count-- > 0 ) 
    {
        int data_len = entry->length - sizeof(entry->class) - sizeof(entry->length);
        xuint32 class = g_ntohl(get_32(&entry->class));
        
        switch(class) {
        case HIX_HIC_OS:
            host->os_type = entry->data[0];
            host->updated = sal_time_msec();
            break;
        case HIX_HIC_HOST_FEATURE:
        {
            xuint32 feature = g_ntohl(get_32(entry->data));
            if ( !(HIX_HFF_DEFAULT & feature) ) {
                continue; /* HIX_HFF_DEFAULT is a 'must' */
            }
            host->feature = feature;
            host->updated = sal_time_msec();
        }
        break;
        case HIX_HIC_ADDR_LPX:
        {
            struct list_head* i;
            struct hix_nic * nic;  
            
            /* add */
            list_for_each(i, &host->address_list) {
                nic = list_entry(i, struct hix_nic, node);
                if ( sal_memcmp(nic->address, entry->data, SAL_ETHER_ADDR_LEN) == 0 )
                    continue;
            }
            /* not found */
            nic = sal_malloc(sizeof(struct hix_nic));
            sal_memcpy(nic->address, entry->data, SAL_ETHER_ADDR_LEN);
            list_add(&nic->node, &host->address_list);
            host->updated = sal_time_msec();
        }
        break;
        case HIX_HIC_ADDR_IPV6:
            if ( 6 == data_len ) {
                sal_memcpy(host->ipv6, entry->data, 6);
                host->updated = sal_time_msec();
            }
            break;    
        case HIX_HIC_ADDR_IPV4:
            if ( 4 == data_len ) {
                sal_memcpy(host->ipv4, entry->data, 4);
                host->updated = sal_time_msec();
            }
            break;
        case HIX_HIC_HOSTNAME:
	  sal_strncpy(host->hostname,(char*)entry->data, data_len);
            host->updated = sal_time_msec();
            break;
        case HIX_HIC_NDAS_SW_VER_INFO:
        {
            hix_hi_version_ptr ver = (hix_hi_version_ptr) entry->data;
            host->sw_ver.major = g_ntohs(get_16(&ver->major)); /* TODO : alignment */
            host->sw_ver.minor = g_ntohs(get_16(&ver->minor));
            host->sw_ver.build = g_ntohs(get_16(&ver->build));
            host->sw_ver.private = g_ntohs(get_16(&ver->private));
            host->updated = sal_time_msec();
        }
        break;
        case HIX_HIC_OS_VER_INFO:
        {
            hix_hi_version_ptr ver = (hix_hi_version_ptr) entry->data;
            host->os_ver.major = g_ntohs(get_16(&ver->major)); /* TODO : alignment */
            host->os_ver.minor = g_ntohs(get_16(&ver->minor));
            host->os_ver.build = g_ntohs(get_16(&ver->build));
            host->os_ver.private = g_ntohs(get_16(&ver->private));
            host->updated = sal_time_msec();
        }
        break;
        }
        entry = (hix_hi_entry_ptr) (((char*) entry) + entry->length);
    }
    return;
}
#endif
#if 0   /* Not used */
static 
ndas_error_t hix_cli_host_info(struct hix_host *host) 
{
    hix_hi_request_ptr req = NULL;
    ndas_error_t err = NDAS_OK;
    struct list_head* i;
    struct hix_nic* nic;
    
    req = sal_malloc(sizeof(hix_hi_request));
    if ( !req ) return NDAS_ERROR_OUT_OF_MEMORY;

    /* Initialize request packet */
    sal_memset(req, 0, sizeof(hix_hi_request));
    hix_init_hdr(&req->header, 0, HIX_TYPE_QUERY_HOST_INFO, sizeof(hix_hi_request));

    if ( list_empty(&host->address_list) ) {
        err = NDAS_ERROR_ADDRESS_NOT_AVAIABLE;
        goto out;
    }
        
    foreach_node_of_hix_host(i, nic, host) 
    {
        struct sockaddr_lpx dest;
        int fd;
        char* reply = NULL;
        
        reply = sal_malloc(HIX_MAX_MESSAGE_LEN);
        if ( !reply ) {
            err = NDAS_ERROR_OUT_OF_MEMORY;
            goto out;    
        }

        fd = hix_cli_init();
        
        if ( !NDAS_SUCCESS(fd) ) {
            goto loop_err_out;
        }

        LPX_ADDR_INIT(&dest, nic->address, HIX_LISTEN_PORT);

        err = lpx_sio_recvfrom(fd, reply, HIX_MAX_MESSAGE_LEN, 0,  
                hix_cli_on_host_info, host, HIX_DEFTIMEOUT_HI);
        
        if ( !NDAS_SUCCESS(err) ) 
            goto loop_err_out;
        
        err = lpx_sendto(fd, req, sizeof(hix_hi_request), 0, &dest, sizeof(dest));
        
        if ( !NDAS_SUCCESS(err) ) 
            goto loop_err_out;
        
        continue;
        
loop_err_out:
        sal_free(reply); 
        /* try to send the packet to the next address */
    }

out:
    sal_free(req);
    return err;
}
#endif

static void hix_svr(lpx_siocb* cb, struct sockaddr_lpx *from, void* user_arg);

ndas_error_t hix_reinit() 
{
    ndas_error_t result;
   /* make sure lpx is inited */
    int fd = v_hix_svr.sockfd;

    if ( fd > 0 ) {
        v_hix_svr.sockfd = -1;
        lpx_close(fd);
    }
    if ( v_hix_svr.buf ) {
        sal_free(v_hix_svr.buf);
        v_hix_svr.buf = NULL;
    }
    
    v_hix_svr.sockfd = lpx_socket(LPX_TYPE_DATAGRAM, 0);
    debug_hix(3, "hix fd=%d",v_hix_svr.sockfd );
    if ( !NDAS_SUCCESS(v_hix_svr.sockfd) )
        return v_hix_svr.sockfd;

    LPX_ADDR_INIT(&v_hix_svr.myaddr, 0, HIX_LISTEN_PORT); /* any address */

    result = lpx_bind(v_hix_svr.sockfd, &v_hix_svr.myaddr, sizeof(v_hix_svr.myaddr));
    debug_hix(5, "LPX: bind: %d", result);
    if (!NDAS_SUCCESS(result))
    {
        goto error_out;
    }
    v_hix_svr.buf = sal_malloc(HIX_MAX_MESSAGE_LEN);
    if ( !v_hix_svr.buf ) {
        result =  NDAS_ERROR_OUT_OF_MEMORY;
        goto error_out;
    }
    result = lpx_sio_recvfrom(
                v_hix_svr.sockfd, 
                v_hix_svr.buf, 
                HIX_MAX_MESSAGE_LEN,
                SIO_FLAG_MULTIPLE_NOTIFICATION, 
                hix_svr,
                NULL, 
                0);
    if ( !NDAS_SUCCESS(result) ) {
        // TODO: reinit again
        goto error_out;
    }
    debug_hix(3, "ed");

    return NDAS_OK;
error_out:
    nhix_cleanup();
    return result;    
}
/** Description 
 * Send the data (hix packet) to the destination from random local port.
 */
static
ndas_error_t hix_svr_reply(void* data, int len, struct sockaddr_lpx *dest) 
{
    int fd;
    ndas_error_t err = NDAS_OK;
    debug_hix(5, "ing data=%p len=%d", data, len);

    fd = v_hix_svr.sockfd; //hix_cli_init(dest);

    if ( !NDAS_SUCCESS(fd) ) 
        return fd;
    
    debug_hix(8, "sending fd=%d to %s:0x%x", fd,
            SAL_DEBUG_HEXDUMP_S(v_hix_svr.dest->slpx_node,SAL_ETHER_ADDR_LEN),
            g_ntohs(v_hix_svr.dest->slpx_port));
    
    err = lpx_sendto(fd, data,len, 0, dest, sizeof(*dest));

//    lpx_close(fd);
    debug_hix(5, "ed err=%d",err);
    return err;
}

static 
void hix_svr_on_discover(void) {
    int length;
    ndas_error_t result;
    hix_dc_request_ptr req = (hix_dc_request_ptr) v_hix_svr.buf;
    hix_dc_reply_ptr reply;    
    hix_udev_entry_ptr entry;
    int i;

    debug_hix(5, "ing");
    debug_hix(9, "request=%s", DEBUG_HIX_DISCOVER_REQUEST(req));
    
    if ( v_hix_svr.sz < sizeof(hix_dc_request))
        return; /* ignore */

    reply = (hix_dc_reply_ptr) sal_malloc(HIX_MAX_MESSAGE_LEN);

    if ( !reply ) 
        return; /* ignore */

    sal_memset(reply, 0, HIX_MAX_MESSAGE_LEN);
    reply->entry_count = 0;
    entry = &reply->entries[0];
    
    /* data part */

    for (i = 0; i < req->entry_count; i++ ) 
    {
        ndev_t* dev = ndev_lookup_bynetworkid(req->entries[i].network_id);

        debug_hix(6, "req-entry[%d].access_type=0x%x for %s", i , 
            req->entries[i].access_type,
            SAL_DEBUG_HEXDUMP_S(req->entries[i].network_id, 6));
            
        if ( dev && req->entries[i].unit < dev->info.nr_unit 
            && dev->unit[req->entries[i].unit] ) 
        {
            int unit = req->entries[i].unit;
	    hix_uda uda;
            logunit_t *sdev = dev->unit[unit];
            debug_hix(5, "slot %d ",sdev->info.slot );
            if ( !sdev->info.enabled ) {
                debug_hix(5, "slot %d is disabled", sdev->info.slot);
                continue;
            }
	    uda = sdev->info.writable ? HIX_UDA_RW : HIX_UDA_RO;
            if ( (uda & req->entries[i].access_type) !=  req->entries[i].access_type)
            {
		debug_hix(5, "ignored requested_uda=%x device_uda=%x",
			  req->entries[i].access_type, uda);
		continue;
	    }

	    debug_hix(5, "added udev=%s with 0x%x",
		      SAL_DEBUG_HEXDUMP_S(dev->network_id, SAL_ETHER_ADDR_LEN), uda);

	    entry = put_udev(
		entry, 
		req->entries[i].network_id, 
		unit, 
		uda);

	    reply->entry_count ++;
        }
    }
    if ( reply->entry_count == 0 ) {
        debug_hix(8, "Nothing to report");    
        goto out;
    }
    length = sizeof(reply->header) + sizeof(reply->entry_count) 
        + (((xuchar*) entry) - ((xuchar*)&reply->entries[0]));

    debug_hix(8, "reply:%s", SAL_DEBUG_HEXDUMP((char*)reply, length));
    
    /* header part */
    hix_init_hdr(&reply->header, 1, HIX_TYPE_DISCOVER, length);
    /* send the data */
    debug_hix(8, "reply:%s length=%d", SAL_DEBUG_HEXDUMP((char*)reply, length), length);
    debug_hix(8, "reply=%s", DEBUG_HIX_DISCOVER_REPLY(reply));

    result = hix_svr_reply(reply, length, v_hix_svr.dest);
    if ( !NDAS_SUCCESS(result) )
    {
        debug_hix(1, "err to sent err=%d", result);
        hix_reinit(); // and retry
    }
out:    
    sal_free(reply);
}


static 
void hix_svr_on_host_info(void) 
{
    int length, data_length;
    ndas_error_t result;
    hix_hi_reply_ptr reply;
    hix_hi_entry_ptr entry;
    extern int lpxitf_iterate_address(void (*func)(xuchar*, void*),void* arg);
    char hostname[32];
    xuchar *ptr;
    debug_hix(5, "ing");
    
    if ( v_hix_svr.sz < sizeof(hix_hi_request) )
        return; /* ignore */

    /* check if I am using the device */
    reply = (hix_hi_reply_ptr) sal_malloc(HIX_MAX_MESSAGE_LEN);
    
    if ( !reply ) 
        return; // fail to create the packet
    sal_memset(reply,0, HIX_MAX_MESSAGE_LEN);
    /* data */    
    reply->data.contains = g_htonl(HIX_HIC_OS | HIX_HIC_OS_VER_INFO | 
            HIX_HIC_NDAS_SW_VER_INFO | HIX_HIC_HOST_FEATURE |
            HIX_HIC_HOSTNAME | HIX_HIC_ADDR_LPX | HIX_HIC_TRANSPORT);
    reply->data.count = 7;

    entry = &reply->data.entries[0];

    /* OS family */
    entry = put_hi_8(entry, HIX_HIC_OS, HIX_HIC_OS_OTHER);
    /* OS Ver */
    entry = put_hi_version(entry, HIX_HIC_OS_VER_INFO,1,1,1,1);
    /* SW Ver */
    entry = put_hi_version(entry, HIX_HIC_NDAS_SW_VER_INFO,1,1,1,1);
    /* HOST feature Ver */
    entry = put_hi_32(entry, HIX_HIC_HOST_FEATURE, HIX_HFF_DEFAULT);
    /* TRANPORT MEDIUM */
    entry = put_hi_8(entry, HIX_HIC_TRANSPORT, HIX_TF_LPX);
    
    /* hostname */
    result = sal_gethostname(hostname,32);
    debug_hix(6, "hostname=%s",hostname);
    if ( NDAS_SUCCESS(result)) 
        entry = put_hi_bytes(entry, HIX_HIC_HOSTNAME, hostname, sal_strlen(hostname) + 1);
    /* LPX addresses */
    ptr = entry->data + 2;
    entry->data[0] = lpxitf_iterate_address(put_hi_lpx_node, &ptr);    
    entry->data[1] = SAL_ETHER_ADDR_LEN;
    entry->class = g_htonl(HIX_HIC_ADDR_LPX);
    entry->length = sizeof(entry->length) + sizeof(entry->class)
                  + sizeof( entry->data[0] ) /* count of lpx addresses*/
                  + sizeof( entry->data[1] ) /* size of LPX address*/
                  + entry->data[0] * SAL_ETHER_ADDR_LEN;

    entry = (hix_hi_entry_ptr)
        (((xuchar*) entry) + entry->length);
    
    /* total size of entries */
    data_length = sizeof(reply->data.length) 
            + sizeof(reply->data.contains) 
            + sizeof(reply->data.count) 
            + (((xuchar*) entry) 
            - ((xuchar*)&reply->data.entries[0]));
    /* to do: the bug in the design of hix field */
    reply->data.length = data_length > 0xff ? 0xff : data_length;
    /* */
    length = sizeof(reply->header) + sizeof(reply->data.reserved) + data_length;
    
    /* header */
    hix_init_hdr(&reply->header, 1, HIX_TYPE_QUERY_HOST_INFO, length);

    debug_hix(8, "HIX:HOSTINFO=%s", DEBUG_HIX_HOSTINFO_REPLY(reply));
    result = hix_svr_reply(reply,length, v_hix_svr.dest);
    if ( !NDAS_SUCCESS(result) )
    {
        hix_reinit(); //and retry
    }
    sal_free(reply);
}
static void hix_svr_on_surrender_access(void) {
    ndas_error_t result;
    hix_sa_request_ptr req = (hix_sa_request_ptr)v_hix_svr.buf;
    hix_sa_reply_ptr reply;
    ndev_t* ndev;
    logunit_t * sdev;
    debug_hix(8, "ing");
    if ( req->entry_count != 1 ) 
        return; // ignore

    reply = (hix_sa_reply_ptr) sal_malloc(sizeof(hix_sa_reply));
    if ( !reply ) 
        return; // ignore
        
    ndev = ndev_lookup_bynetworkid(req->udev.network_id);
    if ( ndev &&  req->udev.unit < ndev->info.nr_unit 
        && ndev->unit[req->udev.unit] ) 
    {
        int unit = req->udev.unit;
        sdev = ndev->unit[unit];
        if ( sdev->info.writable && sdev->info.enabled
            && req->udev.access_type & HIX_UDA_WRITE_ACCESS )
        {
            reply->status = NHSA_STATUS_QUEUED;
/*            sal_error_print("ndas:Request for surrending the writing access for slot %d is accepted."
                , sdev->info.slot);*/
            // TODO: consider if schedule this call in the next dpc.
            (*sdev->surrender_request_handler) (sdev->info.slot);
        } else {
            reply->status = NHSA_STATUS_NO_ACCESS;
        }
    } else
        reply->status = NHSA_STATUS_ERROR;

    /* header */
    hix_init_hdr(&reply->header, 1, HIX_TYPE_QUERY_HOST_INFO,sizeof(hix_sa_reply));
    
    result = hix_svr_reply(reply,sizeof(hix_sa_reply), v_hix_svr.dest);
    if ( !NDAS_SUCCESS(result) )
    {
        hix_reinit();// and retry?
    }
    sal_free(reply);
}

static void hix_svr_on_packet(void) {
    hix_header_ptr header = (hix_header_ptr) v_hix_svr.buf;
    debug_hix(8, "v_hix_svr header=%s", DEBUG_HIX_HEADER(header));
    if ( v_hix_svr.sz < sizeof(hix_header)) return;
    /* verify */
    if ( g_ntohl(header->signature) != HIX_SIGNATURE ) {
        debug_hix(2, "Invalid header");
        return; // ignore
    }
    if ( header->revision != HIX_CURRENT_REVISION ) {
        debug_hix(2, "Invalid REVISIOn");
        return; // ignore
    }
    debug_hix(9, "reply_flag=%d", header->reply_flag );
    debug_hix(9, "type=%d", header->type );
    if ( header->reply_flag ) {
		debug_hix(1, "reply packet type=%d", header->type );
        // route to the client
        return;
    }
    /* _hix server reply */
    switch ( header->type ) {
    case HIX_TYPE_DISCOVER:    
        hix_svr_on_discover();
        break;
     case HIX_TYPE_QUERY_HOST_INFO:    
        hix_svr_on_host_info();
        break;
    case HIX_TYPE_SURRENDER_ACCESS:
        hix_svr_on_surrender_access();
        break;
    case HIX_TYPE_DEVICE_CHANGE:        
        break;
    case HIX_TYPE_UNITDEVICE_CHANGE:
        break;
    default:
        debug_hix(1, "ignore: invalid type=0x%x", header->type);
        break;// ignore
    }
    return; 
}
/* only called by dcp thread */
static void hix_svr(lpx_siocb* cb, struct sockaddr_lpx *from, void* user_arg)
{
    debug_hix(8, "ing cb=%p from={%s:0x%x} user_arg=%p",
        cb,from ? SAL_DEBUG_HEXDUMP_S(from->slpx_node,SAL_ETHER_ADDR_LEN) :"NULL",
        from ? g_ntohs(from->slpx_port) : -1, user_arg);

    if ( !NDAS_SUCCESS(cb->result)) {
        debug_hix(1, "result=%d", cb->result);
        if ( v_hix_svr.running ) 
    		hix_reinit();
        return;
    }
    if ( cb->buf != v_hix_svr.buf ) {
        debug_hix(1, "INTERNAL ERROR cb->buf != v_hix_svr.buf");
        return;
    }
    
    v_hix_svr.sz = cb->nbytes;     
    v_hix_svr.dest = from;

    debug_hix(8, "message from %s:0x%x" , 
        SAL_DEBUG_HEXDUMP_S(from->slpx_node,SAL_ETHER_ADDR_LEN),
        g_ntohs(from->slpx_port));
    
    hix_svr_on_packet();
    if ( v_hix_svr.sockfd < 0 ) {
        hix_reinit();
        return;
    }
}

struct hix_host * lookup_host_accessing(udev_t * udev) {
    struct hix_host * host;
    struct list_head *i, *j;
    struct hix_udev* ud;
    
    list_for_each(i, &v_host_list.list) {
        host = list_entry(i, struct hix_host, node);
        list_for_each(j, &host->udev_list) {
            ud = list_entry(j, struct hix_udev, node);
            if ( ud->access_mode & HIX_UDA_RW ) {
                return host;
            }
        }
    }
    return NULL;
}

ndas_error_t nhix_send_request(udev_t *udev)
{
    struct hix_host* host;
    ndas_error_t err;

    debug_hix(3, "ing udev=%p", udev);
    
    host = lookup_host_accessing(udev);
    debug_hix(8, "host=%p", host);
    
    if ( !host ) {
        
        err = hix_cli_discover(udev, HIX_UDA_RW);
        if ( !NDAS_SUCCESS(err) )
            return err;
        
        do {
            err = sal_semaphore_take((sal_semaphore)udev->hix_sema, 
                                    HIX_DEFTIMEOUT_DC + SAL_TICKS_PER_SEC);
        } while (err == SAL_SYNC_INTERRUPTED);
        
        if ( err != SAL_SYNC_OK ) {// timed out and nobody accesses the disk
            debug_hix(1, "timed out and nobody accesses the disk");
            return NDAS_OK;
        }
        host = lookup_host_accessing(udev);
        if ( !host ) 
            return NDAS_OK;
    }
    err = hix_cli_surrender_request(udev, host);

    if ( !NDAS_SUCCESS(err) )
        return err;

    do {
        err = sal_semaphore_take((sal_semaphore)udev->hix_sema, 
                            HIX_DEFTIMEOUT_SA + SAL_TICKS_PER_SEC);
    } while (err == SAL_SYNC_INTERRUPTED);

    /* For now, it always successes if we send the packet successfully*/
    return NDAS_OK; 
}

ndas_error_t nhix_reinit() {
    int fd = v_hix_svr.sockfd;
    if ( fd > 0 ) {
        v_hix_svr.sockfd = -1;
        lpx_close(fd);
    }
    return NDAS_OK;
}

ndas_error_t nhix_init() 
{
    int ret;
    debug_hix(3, "ing");
    uuid_generate(v_hix_svr.hostid);
    debug_hix(8, "hostid=%s", SAL_DEBUG_HEXDUMP((char*)v_hix_svr.hostid, sizeof(v_hix_svr.hostid)));

    INIT_LIST_HEAD(&v_host_list.list);
    ret = sal_spinlock_create("host_list", &v_host_list.lock);
    if (!ret) 
        return NDAS_ERROR_OUT_OF_MEMORY;
    v_hix_svr.running = 1;
    return hix_reinit();
}

static
void free_host_list() 
{
    struct list_head *i,*n;
    sal_spinlock_take(v_host_list.lock);
    list_for_each_safe(i, n, &v_host_list.list) 
    {
        struct hix_host *host = list_entry(i, struct hix_host, node);
        list_del(i);
        hix_host_destroy(host);
    }
    sal_spinlock_give(v_host_list.lock);
    sal_spinlock_destroy(v_host_list.lock);
}
void nhix_cleanup() 
{
    debug_hix(2, "ing");
    v_hix_svr.running = 0;
    if ( v_hix_svr.sockfd > -1 ) {
        lpx_close(v_hix_svr.sockfd);
        v_hix_svr.sockfd = -1;
    }
    if ( v_hix_svr.buf ) {
        sal_free(v_hix_svr.buf);
        v_hix_svr.buf = NULL;
    }
    free_host_list();
    debug_hix(2, "ed");
}

#endif

