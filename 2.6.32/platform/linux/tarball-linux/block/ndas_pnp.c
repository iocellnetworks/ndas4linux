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

#include <linux/delay.h>

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include <ndas_lpx.h>

#ifdef DEBUG

int	dbg_level_npnp = DBG_LEVEL_NPNP;

#define dbg_call(l,x...) do { 								\
    if (l <= dbg_level_npnp) {								\
        printk("NP|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif

#include "netdisk.h"

#include "ndas_session.h"


#include "xlib/xhash.h"
#include "ndas_ndev.h"
#include "ndas_id.h"
#include "ndas_pnp.h"
#include "ndas_regist.h"


#define RX_PACKET_BUFFER_SIZE	1600

#define ALIVE_CHECK_INTERVAL 	msecs_to_jiffies(10*MSEC_PER_SEC) //10 sec


typedef struct ndas_pnp_s {

    bool running;

    struct socket *sock;
    unsigned char buf[RX_PACKET_BUFFER_SIZE];

	pid_t				npnp_pid;
	struct completion 	completion;

	struct timer_list alive_checker;

    XLIB_HASH_TABLE *changed_inner_handlers;

    spinlock_t 	   	pnp_lock;

} ndas_pnp_t;

typedef struct subscribe_s {

    char ndas_id[NDAS_ID_LENGTH+1];
	void (*func)(const char *ndas_id, const char *name, NDAS_DEV_STATUS status);

} subscribe_t;

static ndas_pnp_t v_pnp = { 

    .running	= FALSE,
    .sock		= NULL
};

#if 0
static __u32 npnp_sub_hash(void* key)
{
    __u8* ck = (__u8*) key;
    __u32 val = ck[0] ^ (ck[1] << 8) ^ (ck[2] << 16) ^ (ck[3] << 24) 
        ^ ck[4] ^ (ck[5] << 16) ^ (ck[6] << 16) ^ (ck[7] << 24);
    if (ck[8]) {
        /* Long form serial */
        val ^= ck[8] ^ (ck[9] << 16) ^ (ck[10] << 16) ^ (ck[11] << 24)
        ^ ck[12] ^ (ck[13] << 16) ^ (ck[14] << 16) ^ (ck[15] << 24);       
    }
    return val;
}
#endif

static __u32 npnp_sub_hash(void* key) 
{
    char* six = (char*) key;

    return six[0] ^ six[1] ^ six[2] ^ six[3] ^ six[4] ^ six[5];
}

static bool npnp_sub_equals(void *a, void *b) 
{
    char *aid = (char*) a;
    char *bid = (char*) b;
	
	if (!memcmp( aid, bid, NDAS_ID_LENGTH )) {

	    dbg_call( 4, "aid = %s, bid = %s\n", aid, bid );
	}	

	return !memcmp( aid, bid, NDAS_ID_LENGTH );
}

static struct ndas_probed *v_probed_data = NULL; 
static int v_probed_data_size = 0;

static void v_netdisk_alive_info_test_dead_disk(const char *ndas_id)
{
	ndas_error_t	err;

    ndev_t 			*ndev;
	subscribe_t		*s;
    unsigned long 	flags;


    dbg_call( 6, "ing\n" );

	err = ndev_lookup_by_ndas_id( ndas_id, &ndev, FALSE ); 

	if (!NDAS_SUCCESS(err)) {

		NDAS_BUG_ON(true);
		return;
	}

	NDAS_BUG_ON(!ndev);

    if (ndev->info.status != NDAS_DEV_STATUS_OFFLINE &&
        time_after(jiffies, ndev->online_jiffies + PNP_ALIVE_TIMEOUT)) {

        dbg_call( 1, "Device %s has gone offline\n", ndas_id );

	    spin_lock_irqsave( &v_pnp.pnp_lock, flags );

	    s = xlib_hash_table_lookup( v_pnp.changed_inner_handlers, ndev->info.ndas_id );

	    spin_unlock_irqrestore( &v_pnp.pnp_lock, flags );

		if (s) {

		    ndev->info.status = NDAS_DEV_STATUS_OFFLINE;
		    s->func( ndev->info.ndas_id, ndev->info.name, NDAS_DEV_STATUS_OFFLINE ); // ndev_pnp_changed
		
		} else {

		    nosync_disappear(  ndev->info.ndas_id, NULL );
            ndev_cleanup(ndev);
        }
    }

    return;
}

static void v_prune_dead_disks_timer(unsigned long data)
{
	ndas_error_t err;
   	int i;
    int size;

    dbg_call( 3, "ing\n" );

    if (!v_pnp.running) {

        return;
    }

	size = ndev_probe_size();

    if (!NDAS_SUCCESS(size) || size == 0) {

        goto out;
    }

    if (v_probed_data_size < size) {

        if (v_probed_data) {
 
            ndas_kfree(v_probed_data);
		}

        v_probed_data_size = size;
        v_probed_data = ndas_kmalloc( sizeof(struct ndas_probed) * size );
    }

    if (!v_probed_data) {

        dbg_call( 1, "out of memory\n" );
        goto out;
    }

    err = ndev_probe_ndas_id( v_probed_data, size );

    if (!NDAS_SUCCESS(err)) {

		NDAS_BUG_ON(true);
        goto out;
    }

    for ( i = 0; i < size; i++) {

        v_netdisk_alive_info_test_dead_disk( v_probed_data[i].ndas_id );
    }

out:

	mod_timer( &v_pnp.alive_checker, jiffies + ALIVE_CHECK_INTERVAL );

    dbg_call( 3, "ed\n" );

    return;
}

bool npnp_subscribe ( 
	const char *ndas_id, 
	void (*ndas_device_handler) (const char* ndas_id, const char* name, NDAS_DEV_STATUS status)
	)
{
    subscribe_t *s = ndas_kmalloc(sizeof(subscribe_t));
    unsigned long 	flags;

    dbg_call( 1, "ing ndas_id=%s\n", ndas_id );

    if (!s) {

		NDAS_BUG_ON(true); 
        return FALSE;
	}

    strncpy( s->ndas_id, ndas_id, NDAS_ID_LENGTH+1 );
 
    s->func = ndas_device_handler;

    spin_lock_irqsave( &v_pnp.pnp_lock, flags );

    xlib_hash_table_insert( v_pnp.changed_inner_handlers, s->ndas_id, s );

    spin_unlock_irqrestore( &v_pnp.pnp_lock, flags );


    return TRUE;
}

void npnp_unsubscribe(const char *ndas_id, bool remove_from_list)
{
    subscribe_t 	*value;
    unsigned long 	flags;


    spin_lock_irqsave( &v_pnp.pnp_lock, flags );

	value = xlib_hash_table_lookup(v_pnp.changed_inner_handlers, ndas_id);

   	spin_unlock_irqrestore( &v_pnp.pnp_lock, flags );


    if (!value) {

        dbg_call( 1, "ing failed find ndas_id=%s\n", ndas_id );
		NDAS_BUG_ON(true);

        return;
    }

    if (remove_from_list) {

	    spin_lock_irqsave( &v_pnp.pnp_lock, flags );

        xlib_hash_table_remove( v_pnp.changed_inner_handlers, ndas_id );

   		spin_unlock_irqrestore( &v_pnp.pnp_lock, flags );
    }

    ndas_kfree(value);
}

static int npnp_thread(void *arg)
{
	const char 		*name = "npnpd";
    unsigned long 	flags;

	ndas_error_t	err;

	struct sockaddr_lpx from;
	int					fromlen = sizeof(struct sockaddr_lpx);

	char default_ndas_id[NDAS_ID_LENGTH + 1] = {0};

	dbg_call( 1, "start\n" );

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))
	daemonize(name);
#else
	daemonize();
	sprintf( current->comm, name );
    reparent_to_init(); // kernel over 2.5 don't need this because daemonize call this function.
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0))

    spin_lock_irqsave( &current->sighand->siglock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending();
    spin_unlock_irqrestore( &current->sighand->siglock, flags );

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,27))

    spin_lock_irqsave( &current->sigmask_lock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending(current);
    spin_unlock_irqrestore( &current->sigmask_lock, flags );

#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,23))

    spin_lock_irqsave( &current->sighand->siglock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending();
    spin_unlock_irqrestore( &current->sighand->siglock, flags );

#else

    spin_lock_irqsave( &current->sigmask_lock, flags );
    sigfillset( &current->blocked );
    recalc_sigpending(current);
    spin_unlock_irqrestore( &current->sigmask_lock, flags );

#endif

	do {

	    ndev_t* 		ndev;
		int 			type;
		int 			version;
	    unsigned long	flags;

    	subscribe_t *s = NULL;
	
		dbg_call( 4, "lpx_sock_recvfrom before\n" );

		err = lpx_sock_recvfrom( v_pnp.sock, v_pnp.buf, RX_PACKET_BUFFER_SIZE, 0, &from, fromlen );

		dbg_call( 4, "lpx_sock_recvfrom, err = %x\n", err );

		if (err < 0) {

			dbg_call( 1, "lpx_sock_recvfrom, err = %x\n", err );
			break;
		}

	    if (v_pnp.running == FALSE) {

    	    dbg_call( 1, "pnp thread exited\n" );
        	break; 
    	}

	    dbg_call( 4, "PNP message from %s - type %d, ver %d\n",
            	  	  NDAS_DEBUG_HEXDUMP(from.slpx_node,6), 
				  	  (int)((unsigned char *)v_pnp.buf)[0], (int)((unsigned char *)v_pnp.buf)[1] );

		network_id_2_ndas_default_id( from.slpx_node, default_ndas_id );

	    err = ndev_lookup_by_ndas_id( default_ndas_id, &ndev, true );

    	if (!NDAS_SUCCESS(err)) {

        	printk( "ndas: fail to identify the packet from the NDAS device: %s\n", 
					NDAS_GET_STRING_ERROR(err) );

			NDAS_BUG_ON(true);

			continue;
	    }

    	dbg_call( 6, "ndev-mac=%s ndev=%p\n",
        	    	   NDAS_DEBUG_HEXDUMP_S( from.slpx_node, LPX_NODE_LEN), ndev );

		type 	= (int)((unsigned char *)v_pnp.buf)[0];
		version = (int)((unsigned char *)v_pnp.buf)[1];

	    ndev->info.version   = version;
    	ndev->online_jiffies = jiffies;

	    dbg_call( 6, "ndev=%p ndasid=%s status=%d lasttick=%lu\n", 
					  ndev, ndev->info.ndas_id, NDAS_DEV_STATUS_HEALTHY, ndev->online_jiffies );

	    spin_lock_irqsave( &v_pnp.pnp_lock, flags );

	    s = xlib_hash_table_lookup( v_pnp.changed_inner_handlers, ndev->info.ndas_id );

    	spin_unlock_irqrestore( &v_pnp.pnp_lock, flags );

		if (s) {

			NDAS_BUG_ON( memcmp(s->ndas_id, ndev->info.ndas_id, NDAS_ID_LENGTH) );

		    dbg_call( 6, "s = %p, s->ndas_id = %s, ndev=%p ndasid=%s status=%d lasttick=%lu\n", 
					  	s, s->ndas_id, ndev, ndev->info.ndas_id, NDAS_DEV_STATUS_HEALTHY, ndev->online_jiffies );
	
			s->func( ndev->info.ndas_id, ndev->info.name, NDAS_DEV_STATUS_HEALTHY ); // ndev_pnp_changed
		}

	} while (1);

	complete( &v_pnp.completion );

	dbg_call( 1, "return\n" );

	return 0;
}

ndas_error_t npnp_init(void)
{
    ndas_error_t err;

    struct sockaddr_lpx slpx;

    dbg_call( 1, "ing\n" );

	init_completion( &v_pnp.completion );

    spin_lock_init( &v_pnp.pnp_lock );

    v_pnp.changed_inner_handlers = xlib_hash_table_new( npnp_sub_hash, npnp_sub_equals );

    if (!v_pnp.changed_inner_handlers) {

        return NDAS_ERROR_OUT_OF_MEMORY;
    }

    dbg_call( 1, "v_pnp.changed_inner_handlers = %p\n", v_pnp.changed_inner_handlers );

	err = lpx_sock_create( LPX_TYPE_DATAGRAM, 0, &v_pnp.sock );

    dbg_call( 2, "LPX: socket: %p\n", v_pnp.sock );

    if (!NDAS_SUCCESS(err)) {

		NDAS_BUG_ON(true);
        goto error_out; 
    }

    memset( &slpx, 0, sizeof(slpx) );

    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = htons(NDDEV_LPX_PORT_NUMBER_PNP);

    err = lpx_sock_bind( v_pnp.sock, &slpx, sizeof(slpx) );

	if (!NDAS_SUCCESS(err)) {

        goto error_out; 
    }

    v_pnp.npnp_pid = kernel_thread( npnp_thread, NULL, 0 );

	if (v_pnp.npnp_pid <= 0) {

		NDAS_BUG_ON( v_pnp.npnp_pid == 0 );
		NDAS_BUG_ON(true);

        dbg_call( 1, "fail to create thread\n" );
        err = v_pnp.npnp_pid;

        goto error_out; 
    }

	setup_timer( &v_pnp.alive_checker, &v_prune_dead_disks_timer, 0 ); 
	mod_timer( &v_pnp.alive_checker, jiffies + ALIVE_CHECK_INTERVAL );

    v_pnp.running = TRUE;

    dbg_call( 3, "ed\n" );

    return NDAS_OK; 

error_out:

	if (v_pnp.sock) {

		lpx_sock_close(v_pnp.sock);
    	v_pnp.sock = NULL;
	}

    xlib_hash_table_destroy( v_pnp.changed_inner_handlers );

    dbg_call( 3, "ed\n" );

    return err;
}

static bool _npnp_unsubscribe(void* key, void* value, void* user_data)
{
    npnp_unsubscribe((const char*)key, FALSE);
    return TRUE;
}

void npnp_cleanup(void) 
{
    unsigned long 	flags;

    dbg_call( 1, "ing\n" );

	del_timer( &v_pnp.alive_checker );

    v_pnp.running = FALSE;

    if (v_pnp.sock != NULL) {

        lpx_sock_close(v_pnp.sock);
        v_pnp.sock = NULL;
    }

	if (v_pnp.npnp_pid) {

		wait_for_completion( &v_pnp.completion );
		v_pnp.npnp_pid = 0;
	}

    if (v_probed_data)  {

        ndas_kfree( v_probed_data );
        v_probed_data = NULL;
        v_probed_data_size = 0;
    }

    spin_lock_irqsave( &v_pnp.pnp_lock, flags );

    xlib_hash_table_foreach_remove( v_pnp.changed_inner_handlers, _npnp_unsubscribe, NULL );

    xlib_hash_table_destroy( v_pnp.changed_inner_handlers );
    v_pnp.changed_inner_handlers = NULL;

	spin_unlock_irqrestore( &v_pnp.pnp_lock, flags );

	return;
}
