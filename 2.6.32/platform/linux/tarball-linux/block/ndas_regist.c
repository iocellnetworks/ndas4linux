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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#include <linux/version.h>
#include <linux/module.h> 

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include "netdisk.h"
#include "ndas_session.h"
#include "ndas_ndev.h"
#include "ndas_regist.h"
#include "ndas_pnp.h"
#include "des.h"
#include "ndas_sdev.h"
#include "ndas_id.h"

#include "xlib/xhash.h"

#ifdef DEBUG

int	dbg_level_nreg = DBG_LEVEL_NREG;

#define dbg_call(l,x...) do { 								\
    if (l <= dbg_level_nreg) {								\
        printk("REG|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif


struct reg_arg { 
    int count;
    struct ndas_registered *data;
    int size;
};
struct probe_arg {
    int count;
    struct ndas_probed *data;
    int size;
};

struct m2d {

    struct list_head node;
    unsigned char *mac;
    ndev_t* ndev;
};

struct _reg_info {

	spinlock_t 	   	reg_lock;

    XLIB_HASH_TABLE	*slot_2_sdev;				// mapping ( slot number -> sdev_t *)

    XLIB_HASH_TABLE	*ndas_name_2_ndev;			// mapping (name -> ndev_t *)
    XLIB_HASH_TABLE	*ndas_default_id_2_ndev;	// mapping (ndas_default_id -> ndev_t *)

    struct list_head  mac_2_ndev;	

    __u32           max_slot;
};

static struct _reg_info v_slot;

/*
 * called in bpc
 */

/* note: the name is allocated when registered and maintained in the structure.
        will be de-allocated when unregistered. */


static bool m2d_remove(unsigned char *network_id, int sync)
{
    struct list_head *pos;
    unsigned long 	 flags = 0;

    if (sync) {

	    spin_lock_irqsave( &v_slot.reg_lock, flags );
	}

    list_for_each(pos, &v_slot.mac_2_ndev) {

        struct m2d *n = list_entry(pos,struct m2d, node);

        if (memcmp(n->mac , network_id, LPX_NODE_LEN) == 0) {
 
           	list_del(&n->node);
			ndas_kfree(n);

		    if (sync) {

	    		spin_unlock_irqrestore( &v_slot.reg_lock, flags );
			}

			return TRUE;
        }
    }

    if (sync) {

   		spin_unlock_irqrestore( &v_slot.reg_lock, flags );
	}

    return FALSE;
}

static __u32 v_name_hash(void* key)
{
	char *name = key; 
    __u32 hash = 0;
    int i;

    for (i = 0; i<NDAS_MAX_NAME_LENGTH && name[i];i++) {

        hash ^= name[i];
	}

    return hash;
}

static bool v_name_equals (void *a, void *b)
{
    if (!a ) return FALSE;
    if (!b ) return FALSE;

    return strncmp((char *)a, (char *)b, NDAS_MAX_NAME_LENGTH) == 0;
}

static __u32 v_6bytes_hash (void *key) 
{
    char *six = (char*) key;

    return six[0] ^ six[1] ^ six[2] ^ six[3] ^ six[4] ^ six[5];
}

static bool v_slot_ndas_id_equals(void *a, void *b) 
{
    char *aid = (char*) a;
    char *bid = (char*) b;

	bool ret;

	ret = !memcmp( aid, bid, NDAS_ID_LENGTH );

	if (ret) {

		dbg_call( 6, "aid = %s, bid = %s\n", aid, bid );
	}

	return ret;
}

C_ASSERT( 0, sizeof(((ndas_slot_info_t *)NULL)->slot) == sizeof(__u32) );

static __u32 v_slot_hash (void *key) 
{
	__u32 *slot = (__u32 *)key;

    return *slot;
}

static bool v_slot_equals(void *a, void *b) 
{
    __u32 *aslot = (__u32 *) a;
    __u32 *bslot = (__u32 *) b;

	bool ret;

	ret = (*aslot == *bslot);

	if (ret) {

		dbg_call( 6, "aslot = %u, bslot = %u\n", *aslot, *bslot );
	}

	return ret;
}

#if 0 /* not used */
static bool v_mac_equals(void *a, void *b) 
{
    char *aid = (char*) a;
    char *bid = (char*) b;
    int i = 0;
    
    for (i = 0; i < LPX_NODE_LEN; i++ ) {
        if ( aid[i] != bid[i] ) return FALSE;
    }
    return TRUE;
}
#endif

static void v_enumerator(void* name, void* _dev, struct reg_arg* arg) 
{
    dbg_call( 3, "name=%s\n", (char*) name );
    dbg_call( 3, "info->ndas_id=%s\n", ((ndev_t*)_dev)->info.ndas_id );
    if ( arg->count >= arg->size ) {
        return;
    }
    strncpy(arg->data[arg->count++].name,name,NDAS_MAX_NAME_LENGTH);
}

ndas_error_t registrar_init(int max_slot) 
{
    spin_lock_init( &v_slot.reg_lock );

    v_slot.max_slot = max_slot;

    v_slot.slot_2_sdev 		= xlib_hash_table_new( v_slot_hash, v_slot_equals );

    v_slot.ndas_name_2_ndev	  		= xlib_hash_table_new( v_name_hash, v_name_equals );
    v_slot.ndas_default_id_2_ndev 	= xlib_hash_table_new( v_6bytes_hash, v_slot_ndas_id_equals );

    INIT_LIST_HEAD( &v_slot.mac_2_ndev );

    return NDAS_OK;
}

static bool return_true(void* key, void* value, void* user_data) 
{
    return TRUE;
}

/* 
 * Free those ndev_t* objects created by pnp-discovery facility.
 * TODO: invent the better way for free these object.
 *  also there could be a race condition for creating object
 *  make sure the pnp-discovery is disabled when this function is called.
 * called by : ndas_stop (user thread of 'ndasadmin stop')
 */
void registrar_stop() 
{
    unsigned long 	 flags;

    dbg_call( 1, "ing\n" );

	spin_lock_irqsave( &v_slot.reg_lock, flags );

    while(!list_empty(&v_slot.mac_2_ndev))
    {
        struct m2d *n = list_entry(v_slot.mac_2_ndev.next, struct m2d, node);
        ndev_t* ndev = n->ndev;

        list_del(&n->node);
        ndas_kfree(n);
        spin_unlock_irqrestore( &v_slot.reg_lock, flags );

        // Unregister the ndev
        ndev_unregister(ndev);

		nosync_disappear(ndev->info.ndas_id, NULL);

        // Clean up the ndev

        ndev_cleanup(ndev);

	    spin_lock_irqsave( &v_slot.reg_lock, flags );
    }

    xlib_hash_table_foreach_remove( v_slot.ndas_default_id_2_ndev, return_true, NULL );
    xlib_hash_table_foreach_remove( v_slot.ndas_name_2_ndev, return_true, NULL );

    xlib_hash_table_foreach_remove( v_slot.slot_2_sdev, return_true, NULL);

    spin_unlock_irqrestore( &v_slot.reg_lock, flags );

    dbg_call( 1, "ed\n" );
}
/* 
 * called by ndas_cleanup (user thread of 'rmmod ndas_core')
 */
void registrar_cleanup(void) 
{
    unsigned long 	 flags;

    dbg_call( 1, "ing\n" );

    spin_lock_irqsave( &v_slot.reg_lock, flags );

    NDAS_BUG_ON( xlib_hash_table_size(v_slot.ndas_default_id_2_ndev) );

    xlib_hash_table_destroy( v_slot.ndas_default_id_2_ndev ); 
    v_slot.ndas_default_id_2_ndev = NULL;

    NDAS_BUG_ON( xlib_hash_table_size(v_slot.ndas_name_2_ndev) );

    xlib_hash_table_destroy( v_slot.ndas_name_2_ndev ); 
    v_slot.ndas_name_2_ndev = NULL;

    NDAS_BUG_ON( !list_empty(&v_slot.mac_2_ndev) );

    NDAS_BUG_ON( xlib_hash_table_size(v_slot.slot_2_sdev) );

    xlib_hash_table_destroy( v_slot.slot_2_sdev ); 
    v_slot.slot_2_sdev = NULL;

    spin_unlock_irqrestore( &v_slot.reg_lock, flags );

    dbg_call( 1, "ed\n" );

	return;
}

ndev_t* ndev_lookup_byname(const char* name) {

    ndev_t* ret;

    if (name == NULL || name[0]==0) {

        return NULL;
    }

    NDAS_BUG_ON( v_slot.ndas_name_2_ndev == NULL );

    ret = (ndev_t*) xlib_hash_table_lookup( v_slot.ndas_name_2_ndev, name );

    if (ret == NULL) {

        dbg_call( 4, "Failed to find %s, name_2_device size=%d\n", name, xlib_hash_table_size(v_slot.ndas_name_2_ndev) );
    }

    return ret;
}

inline static 
ndev_t* ndev_lookup_bynetworkid_nosync(__u8* network_id) {
    struct list_head *pos;
    list_for_each(pos, &v_slot.mac_2_ndev) 
    {
        struct m2d *n = list_entry(pos,struct m2d, node);
        if (memcmp(n->mac , network_id, LPX_NODE_LEN) == 0) {
            ndev_t* ret = n->ndev;
            return ret;
        }
    }
    return NULL;
}
ndev_t* ndev_lookup_bynetworkid(__u8* network_id) {
    ndev_t			*np;
    unsigned long	flags;

    spin_lock_irqsave( &v_slot.reg_lock, flags );
    np = ndev_lookup_bynetworkid_nosync(network_id);
    spin_unlock_irqrestore( &v_slot.reg_lock, flags );
    return np;
}

ndas_error_t ndev_register(ndev_t* ndev) 
{
    ndas_error_t 	err;
    unsigned long 	flags;

    dbg_call( 3, "ing ndev=%p\n", ndev );
    dbg_call( 5, "ing name=%s\n", ndev->info.name );
    dbg_call( 5, "ing id=%s, MAC=%02x:%02x:%02x:%02x:%02x:%02x\n", 
    	ndev->info.ndas_id, 
    	ndev->network_id[0], ndev->network_id[1], ndev->network_id[2],
    	ndev->network_id[3], ndev->network_id[4], ndev->network_id[5]
    );
    if ( ndev->info.name[0] == '\0' ) {
        return NDAS_ERROR_INVALID_PARAMETER;
    }
    spin_lock_irqsave( &v_slot.reg_lock, flags );
    if ( !v_slot.ndas_name_2_ndev ) {
        err = NDAS_ERROR_SHUTDOWN;
        goto out;
    }
    dbg_call( 3, "ndev->info.name=%s ndev=%p\n", ndev->info.name, ndev );
    xlib_hash_table_insert(v_slot.ndas_name_2_ndev, ndev->info.name, ndev);

out:
    spin_unlock_irqrestore( &v_slot.reg_lock, flags );
    return NDAS_OK;
}
/*
 * Remove the entry in ndas_2_device registarar and reset the name.
 * called by 
 *   1. ndas_unregister_device (user thread 'ndasadmin unregister' )
 *   2. ndas_register_device (user thread 'ndasadamin register' ) on fail to register
 *   3. registrar_stop (user thread 'ndasadmin stop') for each registered NDAS device
 *   4. registrar_cleanup (user thread 'rmmod ndas_core') for each registered NDAS device
 */
ndas_error_t ndev_unregister(ndev_t* dev) {

    unsigned long 	flags;

    dbg_call( 5, "ing dev=%p name=%s\n", dev, dev->info.name );

    spin_lock_irqsave( &v_slot.reg_lock, flags );

    if ( !v_slot.ndas_name_2_ndev ) {
        spin_unlock_irqrestore( &v_slot.reg_lock, flags );
        return NDAS_ERROR_SHUTDOWN;
    }

    xlib_hash_table_remove(v_slot.ndas_name_2_ndev, dev->info.name);
    /* should be removed. if not set 0, will not be removed by nosync_disappear */
    dev->info.name[0] = 0;
    dev->info.status = NDAS_DEV_STATUS_UNKNOWN;
    
//    dev->last_tick = 0;
//    e = nosync_disappear(dev->info.ndas_idserial);
    spin_unlock_irqrestore( &v_slot.reg_lock, flags );
    return NDAS_OK;
}

ndas_error_t ndev_lookup_by_ndas_id (const char *ndas_id, ndev_t **ndev, bool create) 
{
	ndas_error_t	err;
    unsigned long 	flags;
    __u8 			network_id[LPX_NODE_LEN];
    struct m2d  	*m2d_node;

	char ndas_default_id[NDAS_ID_LENGTH + 1] = {0};

    dbg_call( 3, "ndas_id=%s\n", ndas_id );

    NDAS_BUG_ON( ndas_id == NULL );

    spin_lock_irqsave( &v_slot.reg_lock, flags );

    if (!v_slot.ndas_default_id_2_ndev) {

	    spin_unlock_irqrestore( &v_slot.reg_lock, flags );
        return NDAS_ERROR_SHUTDOWN;
    }

	err = ndas_id_2_network_id( ndas_id, NULL, network_id, NULL );

    if (!NDAS_SUCCESS(err)) {

	    spin_unlock_irqrestore( &v_slot.reg_lock, flags );
        return err;
    }

	err = ndas_id_2_ndas_default_id( ndas_id, ndas_default_id );

    if (!NDAS_SUCCESS(err)) {

	    spin_unlock_irqrestore( &v_slot.reg_lock, flags );
        return err;
    }

	*ndev = NULL;
    *ndev = (ndev_t*)xlib_hash_table_lookup( v_slot.ndas_default_id_2_ndev, ndas_default_id );

    spin_unlock_irqrestore( &v_slot.reg_lock, flags );

    if (*ndev) {

		NDAS_BUG_ON( memcmp(ndas_default_id, (*ndev)->info.ndas_default_id, NDAS_ID_LENGTH) );

		if (memcmp(ndas_id, ndas_default_id, NDAS_ID_LENGTH) &&
			memcmp(ndas_id, (*ndev)->info.ndas_id, NDAS_ID_LENGTH)) {

			// called by ndev

		    dbg_call( 1, "set *ndev = %p, ndas_default_id = %s, ndas_id=%s, (*ndev)->info.ndas_id=%s\n", 
						  *ndev, ndas_default_id, ndas_id, (*ndev)->info.ndas_id );

			memcpy( (*ndev)->info.ndas_id, ndas_id, NDAS_ID_LENGTH );
		}

        dbg_call( 9, "exists ndas_id=%s\n", (*ndev)->info.ndas_id );

        return NDAS_OK;
    }

	if (create == false) {

		return NDAS_ERROR_NO_DEVICE;
	}

    m2d_node = ndas_kmalloc( sizeof(struct m2d) );

	if (!m2d_node) {

        return NDAS_ERROR_OUT_OF_MEMORY;
    }

	*ndev = ndev_create();
	
	if (*ndev == NULL) {

		ndas_kfree(m2d_node);
        return NDAS_ERROR_OUT_OF_MEMORY;
	}

    ndas_id_2_ndas_sid( ndas_id, (*ndev)->info.ndas_sid );

	memcpy( (*ndev)->info.ndas_default_id, ndas_default_id, NDAS_ID_LENGTH );
	memcpy( (*ndev)->info.ndas_id, ndas_id, NDAS_ID_LENGTH );

    memcpy( (*ndev)->network_id, network_id, LPX_NODE_LEN );

    (*ndev)->info.version = NDAS_VERSION_UNKNOWN;
    (*ndev)->info.status = NDAS_DEV_STATUS_UNKNOWN;

    dbg_call( 2, "ndev=%p ndas_sid=%s ndas_id=%s ndas_default_id = %s mac=%s\n",
				 (*ndev), (*ndev)->info.ndas_sid, (*ndev)->info.ndas_id, ndas_default_id,
            	   NDAS_DEBUG_HEXDUMP_S((*ndev)->network_id, LPX_NODE_LEN) );

    spin_lock_irqsave( &v_slot.reg_lock, flags );

    xlib_hash_table_insert(v_slot.ndas_default_id_2_ndev, (*ndev)->info.ndas_default_id, (*ndev) );

	m2d_node->mac = (*ndev)->network_id;
    m2d_node->ndev = (*ndev);

    list_add( &m2d_node->node, &v_slot.mac_2_ndev );

    spin_unlock_irqrestore( &v_slot.reg_lock, flags );

    return NDAS_OK;
}

/*
 * The NDAS device is vanished in the local network
 * 
 * called by
 *   1. ndev_vanished (bpc)
 *   2. registrar_cleanup (user thread of 'rmmod ndas_core' but via bpc_queue
 *   3. registrar_stop (user thread of 'ndasadmin stop' but via bpc_queue)
 * To do: change name and parameter of this function
 */

ndas_error_t nosync_disappear(const char *ndas_id, void *noarg1)
{
    ndas_error_t 	ret;
    unsigned long 	flags;
    ndev_t		 	*ndev;

	char ndas_default_id[NDAS_ID_LENGTH + 1] = {0};

	ret = ndas_id_2_ndas_default_id( ndas_id, ndas_default_id );

    spin_lock_irqsave( &v_slot.reg_lock, flags );

    ndev = (ndev_t*)xlib_hash_table_lookup( v_slot.ndas_default_id_2_ndev, ndas_default_id );

    dbg_call( 1, "ndas_id=%s\n", ndas_id );
    dbg_call( 5, "dev=%p", ndev );

    if (!ndev) {

        ret = NDAS_ERROR_INTERNAL;
        dbg_call( 2, "Not found\n" );
        goto out;
    }

    if (!v_slot.ndas_default_id_2_ndev) {

	    ret = NDAS_ERROR_SHUTDOWN;
        goto out;
    }

    ndev->info.status = NDAS_DEV_STATUS_OFFLINE;

    NDAS_BUG_ON( ndev->info.name[0] );

    xlib_hash_table_remove( v_slot.ndas_default_id_2_ndev, ndev->info.ndas_default_id );

    m2d_remove( ndev->network_id, FALSE );

	ret = NDAS_OK;

out:

    spin_unlock_irqrestore( &v_slot.reg_lock, flags );

    return ret;
}

ndas_error_t sdev_register(sdev_t *sdev)
{
    unsigned long 	flags;

	spin_lock_irqsave( &v_slot.reg_lock, flags );

	if (sdev->info.slot < NDAS_FIRST_SLOT_NR) {

		NDAS_BUG_ON(true);
	}

    dbg_call( 1, "slot=%d\n", sdev->info.slot );

    // TODO: check the validity of slot number.

    if ( !v_slot.slot_2_sdev ) {
        spin_unlock_irqrestore( &v_slot.reg_lock, flags );
        return NDAS_ERROR_SHUTDOWN;
    }

    xlib_hash_table_insert( v_slot.slot_2_sdev, &sdev->info.slot, sdev );
    spin_unlock_irqrestore( &v_slot.reg_lock, flags );

    return NDAS_OK;
}

ndas_error_t sdev_unregister(sdev_t *sdev) 
{
    dbg_call( 1, "ing slot=%d\n", sdev->info.slot );
    if ( !v_slot.slot_2_sdev ) {
        return NDAS_ERROR_SHUTDOWN;
    }
    xlib_hash_table_remove(v_slot.slot_2_sdev, &sdev->info.slot);
    return NDAS_OK;
}

sdev_t * sdev_lookup_byslot(int slot) 
{
#ifdef DEBUG
    sdev_t *ret = xlib_hash_table_lookup(v_slot.slot_2_sdev, &slot);
    dbg_call( 5, "ing slot=%d\n", slot );
    if ( ret ) NDAS_BUG_ON( ret->magic != SDEV_MAGIC );
    return ret;
#else
    dbg_call( 5, "ing slot=%d\n", slot );
    return (sdev_t *) xlib_hash_table_lookup(v_slot.slot_2_sdev, &slot);
#endif
}

ndas_error_t ndev_probe_size() 
{
    return xlib_hash_table_size(v_slot.ndas_default_id_2_ndev);
}

static void v_probie_ndas_id(void *key, void *value, void *user_data) 
{
	char 	*ndas_default_id = key;
    ndev_t 	*ndev = (ndev_t*)value;

    struct probe_arg *arg = (struct probe_arg *)user_data;

    dbg_call( 3, "ing ndev=%p count=%d\n", ndev, arg->count );
    dbg_call( 3, "ing ndas_id=%s\n", ndas_default_id );

	NDAS_BUG_ON( memcmp(ndas_default_id, ndev->info.ndas_default_id, NDAS_ID_LENGTH) );

    if (arg->count >= arg->size) {

        return;
    }

    arg->data[arg->count].status = ndev->info.status;

	memcpy( arg->data[arg->count].ndas_sid, ndev->info.ndas_sid, NDAS_SERIAL_LENGTH );
    arg->data[arg->count].ndas_sid[NDAS_SERIAL_LENGTH] = 0;

	memcpy( arg->data[arg->count].ndas_id, ndev->info.ndas_id, NDAS_ID_LENGTH );
    arg->data[arg->count].ndas_id[NDAS_ID_LENGTH] = 0;

    arg->count++;
}

ndas_error_t ndev_probe_ndas_id(struct ndas_probed *data, int size) 
{
    unsigned long 	flags;

    struct probe_arg arg = { 

        .data = data, 
        .count = 0, 
    };

    dbg_call( 2, "ing size=%d\n", size );

    if (!data) {
 
		return NDAS_ERROR_INVALID_PARAMETER;
    }

	    spin_lock_irqsave( &v_slot.reg_lock, flags );


    arg.size = xlib_hash_table_size(v_slot.ndas_default_id_2_ndev);
    xlib_hash_table_foreach(v_slot.ndas_default_id_2_ndev, v_probie_ndas_id, &arg);

    spin_unlock_irqrestore( &v_slot.reg_lock, flags );

    dbg_call( 2, "ed count=%d\n", arg.count );

    return arg.count;
}

ndas_error_t
ndev_registered_size()
{
    return xlib_hash_table_size(v_slot.ndas_name_2_ndev);
}    
ndas_error_t ndev_registered_list(struct ndas_registered *data, int size ) 
{
    unsigned long 	flags;
    struct reg_arg arg = { .data = data, .count = 0 };

	spin_lock_irqsave( &v_slot.reg_lock, flags );

    if ( !v_slot.ndas_name_2_ndev ) {
        spin_unlock_irqrestore( &v_slot.reg_lock, flags );
        return NDAS_ERROR_SHUTDOWN;
    }
    arg.size = xlib_hash_table_size(v_slot.ndas_name_2_ndev);
    
    xlib_hash_table_foreach(v_slot.ndas_name_2_ndev, (XlibHashIteFunc) v_enumerator, &arg);
    spin_unlock_irqrestore( &v_slot.reg_lock, flags );
    return arg.count;
}

ndas_error_t ndev_rename(ndev_t* ndev, const char* newname) 
{
    unsigned long 	flags;

    if ( strlen(newname) > NDAS_MAX_NAME_LENGTH ) {
        return NDAS_ERROR_INVALID_NAME;
    }
        spin_lock_irqsave( &v_slot.reg_lock, flags );

    if ( !v_slot.ndas_name_2_ndev ) {
        spin_unlock_irqrestore( &v_slot.reg_lock, flags );
        return NDAS_ERROR_SHUTDOWN;
    }
    xlib_hash_table_remove(v_slot.ndas_name_2_ndev, ndev->info.name);
    strncpy(ndev->info.name, newname, NDAS_MAX_NAME_LENGTH);
    xlib_hash_table_insert(v_slot.ndas_name_2_ndev, ndev->info.name, ndev);
    spin_unlock_irqrestore( &v_slot.reg_lock, flags );
    return NDAS_OK;
}

#define NDAS_RESTORE_ENTRY_SIZE (32*5)
#if NDAS_RESTORE_ENTRY_SIZE <(NDAS_MAX_NAME_LENGTH + NDAS_ID_LENGTH + 1 + NDAS_KEY_LENGTH + 1 )
#error NDAS_RESTORE_ENTRY_SIZE is not small
#endif
#define DES_UNIT 8u

#define NREG_SLOT_NUM	2

struct _reg {

    char	name[NDAS_MAX_NAME_LENGTH];
    char 	ndas_id[NDAS_ID_LENGTH + 1];
    char 	ndas_key[NDAS_KEY_LENGTH + 1];
    __u16 	enabled_bitmap;   /* Initial enabling status */
	__u8	slot[NREG_SLOT_NUM];
};

static char key[] = {0x27, 0xE2, 0x55, 0x3A, 0xA6, 0xE2, 0xD9, 0x7A};

struct enc_arg {
    char* data;
    int count;
};

static void v_encryptor(char* name, ndev_t* ndev, struct enc_arg* arg)
{
    __u32 des_key[32];

    char  *ob; 
    char  ik[32];
    char  *ib;

    unsigned int i;

    __u16 enabled_bitmap = 0;
    
    dbg_call( 1,"ing ndev=%p\n", ndev );
    dbg_call( 3,"ing data=%p\n",arg->data );
    dbg_call( 8,"ing name=%s(%p)\n", ndev->info.name, ndev->info.name );
    dbg_call( 5,"ing ndas_idserial=%s(%p)\n", ndev->info.ndas_id, ndev->info.ndas_id );
    des_ky(key, des_key);

    ib = (char*) ndev->info.name;
    dbg_call( 5,"ing name=%s(%p)\n", ndev->info.name, ndev->info.name );
    ob = arg->data + (arg->count++ * NDAS_RESTORE_ENTRY_SIZE); 
    for (i = 0; i < NDAS_MAX_NAME_LENGTH/DES_UNIT; i++) 
    {
        des_ec(ib, ob, des_key);
        ib += DES_UNIT;
        ob += DES_UNIT;
    }

    ib = ik;

	dbg_call( 2, "ing ndas_idserial=%s(%p)\n", ndev->info.ndas_id, ndev->info.ndas_id );
    dbg_call( 2, "ing name=%s(%p)\n",ndev->info.name, ndev->info.name );
    dbg_call( 2, "ing ndas_key=%s\n", ndev->info.ndas_key );

	memcpy(ib, ndev->info.ndas_id, sizeof(ndev->info.ndas_id));
    ib+= sizeof(ndev->info.ndas_id);
 
    if ( ndev->info.ndas_key == NULL || ndev->info.ndas_key[0] == '\0' ) {
        memset(ib, 0, sizeof(ndev->info.ndas_key));
    } else 
        memcpy(ib , ndev->info.ndas_key, sizeof(ndev->info.ndas_key));
    ib+= sizeof(ndev->info.ndas_key);
    
    /* no consideration for endian */
    dbg_call( 5,"ing nr_unit=%d\n", ndev->info.nr_unit );
	
#if 0
    for (i = 0; i < ndev->info.nr_unit; i++) 
    {
        sdev_t *sdev = NULL;

		if (ndev->info.slot[i] >= NDAS_FIRST_SLOT_NR) {

			sdev = sdev_lookup_byslot(ndev->info.slot[i]);
		}

        dbg_call( 5,"ing sdev=%p\n", sdev );

        if (sdev && sdev->info.enabled) 
        {
            int bit = 0;
            if ( sdev->info.writeshared ) 
                bit = REGDATA_BIT_SHARE_ENABLED;
            else if ( sdev->info.writable ) 
                bit = REGDATA_BIT_WRITE_ENABLED;
            else 
                bit = REGDATA_BIT_READ_ENABLED;

            enabled_bitmap |= bit << (i*2);   
            dbg_call( 5,"ing bit=0x%x\n", bit );
        }
    }
#else
	enabled_bitmap = ndev->enabled_bitmap;
#endif

    dbg_call( 1,"ing enabled_bitmap=0x%x\n", enabled_bitmap );

    memcpy(ib, &enabled_bitmap, sizeof(enabled_bitmap));
    ib+=sizeof(enabled_bitmap);

	NDAS_BUG_ON( sizeof(ndev->info.slot[0]) != 1 );

	for (i=0; i<NREG_SLOT_NUM; i++) {

		__u8	slot;

		slot = ndev->info.slot[i];
		
		memcpy( ib, &slot, sizeof(__u8) );
		ib += sizeof(__u8); 
	}

    dbg_call( 1, "ndev->info.slot[0] = %d\n", ndev->info.slot[0] );
    dbg_call( 1, "ndev->info.slot[1] = %d\n", ndev->info.slot[1] );

    ib = ik;
    for (i = 0; i < 32/DES_UNIT; i++) {
        des_ec(ib, ob , des_key);
        ib += DES_UNIT;
        ob += DES_UNIT;
    }
    return;
}

static ndas_error_t v_decryptor(int c, char* data, struct _reg* reg)
{
    __u32 des_key[32];
    unsigned int i;
    char ik[32];
    char* ob;
    char* ib;

    des_ky(key,des_key);

    ib = reg->name;
    ob = data + (c * NDAS_RESTORE_ENTRY_SIZE); 
    for (i = 0; i < NDAS_MAX_NAME_LENGTH/DES_UNIT; i++) {
        des_dc(ob, ib, des_key);
        ib += DES_UNIT;
        ob += DES_UNIT;
    }
    ib = ik;
    for (i = 0; i < sizeof(ik)/DES_UNIT; i++) {
        des_dc(ob, ib, des_key);
        ib += DES_UNIT; 
        ob += DES_UNIT;
    }

    NDAS_BUG_ON( NDAS_ID_LENGTH + 1 + NDAS_KEY_LENGTH + 1 + 
                 sizeof(reg->enabled_bitmap) + sizeof(reg->slot) > sizeof(ik) );
    ib = ik;
    memcpy(reg->ndas_id, ib, NDAS_ID_LENGTH + 1); 
    ib += NDAS_ID_LENGTH + 1;
    memcpy(reg->ndas_key, ib, NDAS_KEY_LENGTH + 1); 
    ib += NDAS_KEY_LENGTH + 1;
    /* no consideration for the endianness */
    memcpy(&reg->enabled_bitmap, ib, sizeof(reg->enabled_bitmap));
    ib+=sizeof(reg->enabled_bitmap);
    memcpy(&reg->slot, ib, sizeof(__u8)*NREG_SLOT_NUM);

    dbg_call( 2, "name=%s\n", reg->name );
    dbg_call( 2, "ndas_idserial=%s\n", reg->ndas_id );
    dbg_call( 2, "ndas_key=%s\n", reg->ndas_key );
    dbg_call( 2, "enabled_bitmap=0x%x\n", reg->enabled_bitmap );

    return  NDAS_OK;
}

ndas_error_t ndev_get_registration_data(char **data,int *size) 
{
    unsigned long 	flags;

    struct enc_arg arg;
        spin_lock_irqsave( &v_slot.reg_lock, flags );

    (*size) = xlib_hash_table_size(v_slot.ndas_name_2_ndev) * NDAS_RESTORE_ENTRY_SIZE  + 5;
    (*data) = ndas_kmalloc(*size);
    
    dbg_call( 1, "data=%p size=%d\n",*data,(*size) );
    if ( (*data) == NULL ) {
        spin_unlock_irqrestore( &v_slot.reg_lock, flags );
        return NDAS_ERROR_OUT_OF_MEMORY;
    }
    memcpy((*data), "NDAS", 4);
    (*data)[4] = 0x1; // version

    arg.count = 0;
    arg.data = (*data) + 5;
    
    xlib_hash_table_foreach(v_slot.ndas_name_2_ndev, 
        (XlibHashIteFunc) v_encryptor, &arg);
    dbg_call( 3, "ed count=%d\n", arg.count );
    spin_unlock_irqrestore( &v_slot.reg_lock, flags );

    return NDAS_OK;
}

ndas_error_t ndev_set_registration_data(char *data, int size)
{
    int i;
    int num_disk;
    struct _reg* to_register;
    ndas_error_t e;
    dbg_call( 3, "ing data=%p size=%d\n", data, size );
    
    if ( memcmp(data, "NDAS", 4) != 0 ) 
        return NDAS_ERROR_INVALID_NDAS_ID;
    
    if ( data[4] != 0x1 ) 
        return NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION;

    num_disk = (size - 5) / NDAS_RESTORE_ENTRY_SIZE;
    if ( num_disk * NDAS_RESTORE_ENTRY_SIZE != size - 5 ) 
        return NDAS_ERROR_INVALID_NDAS_ID;

    dbg_call( 5, "num_disk=%d\n", num_disk );
    to_register = ndas_kmalloc(num_disk * sizeof(struct _reg ));
    for ( i = 0 ; i < num_disk ; i++) {
        dbg_call( 5, "data=%p to_register=%p\n", data + 5,to_register + i );
        v_decryptor(i, data + 5,  to_register + i);
    }
    
    for ( i = 0; i < num_disk; i++) 
    {
        ndev_t *ndev;
		__u8	slot_num;

        dbg_call( 2, "name=%s\n", to_register[i].name );
        dbg_call( 2, "ndas_idserial=%s\n", to_register[i].ndas_id );
        dbg_call( 2, "ndas_key=%s\n", to_register[i].ndas_key );
        dbg_call( 2, "ndas_key=0x%x\n", to_register[i].enabled_bitmap );

		slot_num = 0;

        dbg_call( 1, "to_register[i].slot[0] = %d\n", to_register[i].slot[0] );
        dbg_call( 1, "to_register[i].slot[1] = %d\n", to_register[i].slot[1] );

		if (to_register[i].slot[0] >= NDAS_FIRST_SLOT_NR) {

			slot_num ++;

			if (to_register[i].slot[1] >= NDAS_FIRST_SLOT_NR) {

				slot_num ++;
			}
		}

        e = ndev_register_device( to_register[i].name, to_register[i].ndas_id, to_register[i].ndas_key, 
								  to_register[i].enabled_bitmap, FALSE, to_register[i].slot, slot_num );

        if ( !NDAS_SUCCESS(e)) {
            to_register[i].ndas_id[15] = '*';
            to_register[i].ndas_id[16] = '*';
            to_register[i].ndas_id[17] = '*';
            to_register[i].ndas_id[18] = '*';
            to_register[i].ndas_id[19] = '*';
            dbg_call( 1, "ndas: fail to register the ndas device %s\n", to_register[i].ndas_id );
            
            dbg_call( 1, "e=%d\n", e );
        }
        ndev = ndev_lookup_byname(to_register[i].name);
        if ( !ndev ) {
            dbg_call( 1, "Fail to get the just-registered ndev object %s\n", to_register[i].name );
            continue;
        }
    }
    ndas_kfree(to_register);
    return NDAS_OK;
}

