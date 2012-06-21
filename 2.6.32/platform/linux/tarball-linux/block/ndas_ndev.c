/*
 -------------------------------------------------------------------------
 Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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
#include <linux/if_ether.h>		/* ETH_ALEN */

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#if   defined(__LITTLE_ENDIAN)

#define __LITTLE_ENDIAN_BYTEORDER
#define __LITTLE_ENDIAN__

#elif defined(__BIG_ENDIAN)

#define __BIG_ENDIAN_BYTEORDER
#define __BIG_ENDIAN__

#else
#error "byte endian is not specified"
#endif 

#include <lspx/lsp_util.h>

#include <ndas_lpx.h>

#include "ndas_ndev.h"

#include "ndas_id.h"
#include "ndas_session.h"
#include "ndas_sdev.h"
#include "ndas_pnp.h"
#include "ndas_regist.h"
#include "ndas_udev.h"

#ifdef DEBUG

int	dbg_level_ndev = DBG_LEVEL_NDEV;

#define dbg_call(l,x...) do { 								\
    if (l <= dbg_level_ndev) {								\
        printk("%s|%d|%d|",__FUNCTION__, __LINE__,l); 		\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif
	

static void ndev_unit_cleanup(ndev_t *ndev); 

ndas_error_t ndev_detect_device(const char* name) 
{
    ndev_t *ndev;

	dbg_call( 1, "ing\n" );

    ndev = ndev_lookup_byname(name);

    if (ndev) {

		ndev_pnp_changed( ndev->info.ndas_id, ndev->info.name, NDAS_DEV_STATUS_UNKNOWN );
        return NDAS_OK;

    } else {

        return NDAS_ERROR_INVALID_NAME;
    }
}

ndas_error_t ndev_get_ndas_dev_info(const char* name, ndas_device_info* info) 
{
    ndev_t* ndev = ndev_lookup_byname(name);
    if ( ndev == NULL ) 
    {
        dbg_call(1, "NDAS_ERROR_INVALID_NAME name=%s\n", name);
        return NDAS_ERROR_INVALID_NAME;
    }
    memcpy(info,&ndev->info, sizeof(ndas_device_info));
    /* Hide last five char of ID */
    strcpy(&info->ndas_id[15], "*****");
    dbg_call(3, "nr_unit=%d\n",ndev->info.nr_unit);
    return NDAS_OK;
}

ndas_error_t ndev_set_registration_name(const char* name, const char *newname) 
{
    ndev_t* dev = ndev_lookup_byname(name);
    if ( dev == NULL ) return NDAS_ERROR_INVALID_NAME;
    return ndev_rename(dev,newname);
}

#ifdef NDAS_HIX

#include "nhix.h"

ndas_error_t ndev_request_permission(int slot) 
{
    sdev_t * sdev = sdev_lookup_byslot(slot);
    if ( sdev == NULL ) return NDAS_ERROR_INVALID_SLOT_NUMBER;
    if ( sdev->info.mode == NDAS_DISK_MODE_SINGLE )
        return nhix_send_request(sdev->child[0]);
    return NDAS_ERROR_NOT_IMPLEMENTED;
    // TODO: check if the proper status.
}
#else

ndas_error_t
ndev_request_permission(int slot) 
{
    return NDAS_OK;
}
#endif

static ndas_error_t ndev_discover_device(ndev_t	*ndev) 
{
    ndas_error_t err;

    uconn_t conn = {0};

	const lsp_ata_handshake_data_t *hsdata;
    lsp_text_target_list_t 	 targetList;

    int i;

    dbg_call( 1, "ing %s..\n", ndev->info.name );

    err = conn_init( &conn, ndev->network_id, 0, NDAS_CONN_MODE_READ_ONLY );

    if (!NDAS_SUCCESS(err)) {

		NDAS_BUG_ON(true);
        return err;
    }

    err = conn_connect( &conn, NDAS_RECONNECT_TIMEOUT, (ndev->info.status == NDAS_DEV_STATUS_HEALTHY) ? 5 : 1 );

    if (!NDAS_SUCCESS(err)) {

		printk( "connection failed err = %d\n", err );

        // Connect failed

        ndev->info.version = NDAS_VERSION_UNKNOWN;
        ndev->info.nr_unit = 0;

        //  Determine proper status

        if (time_before(jiffies, ndev->online_jiffies + PNP_ALIVE_TIMEOUT)) {

            // PNP packet is received recently.

            if (NDAS_ERROR_CONNECT_FAILED == err) {

			    ndev->info.status = NDAS_DEV_STATUS_CONNECT_FAILURE;
 
			} else {
 
			    ndev->info.status = NDAS_DEV_STATUS_LOGIN_FAILURE;
            }

        } else {

            ndev->info.status = NDAS_DEV_STATUS_OFFLINE;
        }

        conn_clean(&conn);
        return NDAS_ERROR;
    } 

    ndev->info.status  = NDAS_DEV_STATUS_HEALTHY;
    ndev->info.version = conn.hwdata->hardware_version;

    err = conn_text_target_list( &conn, &targetList, sizeof(lsp_text_target_list_t) );

    if (!NDAS_SUCCESS(err)) {

        dbg_call( 1, "Text target list failed\n" );
		NDAS_BUG_ON(true);

        ndev->info.status = NDAS_DEV_STATUS_TARGET_LIST_FAILURE;

        conn_disconnect(&conn);
        conn_clean(&conn);
        return NDAS_ERROR;        
    }

    ndev->info.nr_unit = targetList.number_of_elements;

    dbg_call( 1, "ndas: discovered %d unit device from %s\n", ndev->info.nr_unit, ndev->info.name );

    if ( ndev->info.nr_unit <1 || ndev->info.nr_unit >2 ) {

		NDAS_BUG_ON(true);
        ndev->info.nr_unit = 0;
    }

    for (i = 0; i < ndev->info.nr_unit; i++) {

        err  = conn_handshake( &conn );

        if (!NDAS_SUCCESS(err)) {

            ndev->info.status = NDAS_DEV_STATUS_IO_FAILURE;

            err = NDAS_ERROR;
            goto out;
        }

        hsdata = lsp_get_ata_handshake_data( conn.lsp_handle );

        if (ndev->udev[i]) {

            dbg_call( 1, "Device %d is already created\n", i );

		} else {
 
			ndev->udev[i] = udev_create( ndev, i );
		}

		udev_set_disk_information( ndev->udev[i], hsdata );

        ndev->udev[i]->uinfo.type = NDAS_UNIT_TYPE_HARD_DISK;
		ndev->info.mode[i] = NDAS_DISK_MODE_SINGLE;

#if 0
        if (hsdata->device_type == 0) {

            PNDAS_DIB_V2 dib;
            xuint32 size =sizeof(NDAS_DIB_V2);

            dib = sal_malloc(size);

            if(dib == NULL) {
                sinfo->mode = NDAS_DISK_MODE_UNKNOWN;
                ndev->info.status = NDAS_DEV_STATUS_IO_FAILURE;
                dbg_call(1, "Error allocate DIB memory");
				up( &sdev->sdev_mutex );
                goto out;
            }
            /* ATA */
            udev->uinfo->type = NDAS_UNIT_TYPE_HARD_DISK;
            err = conn_read_dib(&conn[i], sinfo->sectors, dib, &size);

            if ( !NDAS_SUCCESS(err) ) {

                sal_free(dib);
                sinfo->mode = NDAS_DISK_MODE_UNKNOWN;
                ndev->info.status = NDAS_DEV_STATUS_IO_FAILURE;
                dbg_call(1, "Error reading DIB %d", err);
				up( &sdev->sdev_mutex );
                goto out;
            }

            dbg_call(1, "mediaType=%u", dib->u.s.iMediaType);

            if ( dib->u.s.iMediaType == NMT_RAID0 ) {

                sinfo->mode = NDAS_DISK_MODE_RAID0;
                sinfo->mode_role = dib->u.s.iSequence;
            } else if ( dib->u.s.iMediaType == NMT_RAID1 ) {
                sinfo->mode = NDAS_DISK_MODE_RAID1;
                sinfo->mode_role = dib->u.s.iSequence;
            } else if ( dib->u.s.iMediaType == NMT_RAID4 ) {
                sinfo->mode = NDAS_DISK_MODE_RAID4;
                sinfo->mode_role = dib->u.s.iSequence;
            } else if ( dib->u.s.iMediaType == NMT_AGGREGATE ) {
                sinfo->mode = NDAS_DISK_MODE_AGGREGATION;
                sinfo->mode_role = dib->u.s.iSequence;
            } else if ( dib->u.s.iMediaType == NMT_MIRROR ) {
                sinfo->mode = NDAS_DISK_MODE_MIRROR; // TODO convert into raid1
                sinfo->mode_role = dib->u.s.iSequence;
            } else if ( dib->u.s.iMediaType == NMT_MEDIAJUKE) {
                sal_debug_println("NDAS_DISK_MODE_MEDIAJUKE");
                sinfo->mode = NDAS_DISK_MODE_MEDIAJUKE; 
            } else {
                sinfo->mode = NDAS_DISK_MODE_SINGLE;
            }
            sal_free(dib);
            err = NDAS_OK;

        } else {

            dbg_call(0, "ATAPI detected");
            /* ATAPI */
            udev->uinfo->type = NDAS_UNIT_TYPE_ATAPI;
            sinfo->mode = NDAS_DISK_MODE_ATAPI;
            err = NDAS_OK;
        }

#endif
    }

#if 0
/* To do: Detect changes in RAID configuration */
#ifdef XPLAT_RAID
            err = raid_detect(ndev->unit[i], pnp);
            dbg_call(5, "err=%d i=%d", err, i);
            if ( !NDAS_SUCCESS(err) )
                goto out;
            sdev_unlock(ndev->unit[i]);
    for ( i = 0; i < ndev->info.nr_unit; i++)
    {
        sdev_t *sdev;
        if ( newborn ) {
            dbg_call(1, "ndas: slot %d is assigned for %d%s unit disk of \"%s\"\n"
                ,ndev->unit[i]->info.slot, i+1, TH(i+1), name);
        }
        dbg_call(1, "slot=%d", ndev->info.slot[i]);
        sdev = sdev_lookup_byslot(ndev->info.slot[i]);
        if ( !sdev ) continue;
        dbg_call(1, "mode=%d", sdev->info.mode);

        if ( !slot_is_the_raid(&sdev->info) )
            continue;

        raid_sdev = sdev_lookup_byslot(SDEV_UNIT_INFO(sdev)->raid_slot);
        if ( !raid_sdev) continue;
        if ( !newborn ) {
            sal_assert(!sdev_is_locked(raid_sdev));
            if ( raid_sdev->ops->changed ) {
                ndev_reenter_online_notify(raid_sdev, ndev->info.slot[i]);
                return;
            }
        }
        if ( raid_sdev->ops->changed )
            raid_sdev->ops->changed(raid_sdev, ndev->info.slot[i], online); // rddc_device_changed
    }
#endif
#endif

out:

	conn_logout(&conn);
    conn_disconnect(&conn);
    conn_clean(&conn);

    dbg_call( 1, "nr_unit=%d\n",ndev->info.nr_unit );

    return err;
}

void ndev_pnp_changed(const char *ndas_id, const char *name, NDAS_DEV_STATUS status) 
{
    ndas_error_t	err;
    ndev_t			*ndev;
	int				i;

    sdev_create_param_t 	 sparam;

    dbg_call( 2, "serial=%s online=%d\n", ndas_id, status );

    if (name == NULL) {

		NDAS_BUG_ON(true);
        return;
    }

	err = ndev_lookup_by_ndas_id( ndas_id, &ndev, FALSE );

	if (!NDAS_SUCCESS(err)) {

		NDAS_BUG_ON(true);
		return;
	}

	NDAS_BUG_ON(!ndev);

    if (!ndev) {

		NDAS_BUG_ON(true);
        return;
    }

	if (status != NDAS_DEV_STATUS_UNKNOWN && ndev->info.status == status) {

		return;
	}

    dbg_call( 1, "ndas_id=%s online=%d ndev->info.status = %d\n", ndas_id, status, ndev->info.status );

	if (ndev_lookup_byname(ndev->info.name) == NULL) {

		NDAS_BUG_ON(true);
		return;
	}

	down( &ndev->ndev_mutex );

	ndev->info.status = status;

    if (status == NDAS_DEV_STATUS_OFFLINE) {

        dbg_call( 1,  "ndas: device became offline\n" ); 
 		up( &ndev->ndev_mutex );

		return;
    }

    dbg_call( 1, "ndas: registered device \"%s\" is status %d\n", name, status );

    err = ndev_discover_device(ndev);

    if (!NDAS_SUCCESS(err)) {

		//NDAS_BUG_ON(true);

        printk( "Failed to discover device %s", name );
 		up( &ndev->ndev_mutex );
		
		return;
    } 

    for (i = 0; i < ndev->info.nr_unit; i++) {
	
		sdev_t *sdev = NULL;

		if (ndev->info.slot[i] >= NDAS_FIRST_SLOT_NR) {

			sdev = sdev_lookup_byslot(ndev->info.slot[i]);
		}

        if (sdev) {

            dbg_call( 1, "Device %d is already created\n", i );

		} else {

			if (ndev->info.slot[i] == NDAS_INVALID_SLOT) {

				ndev->info.slot[i] = sdev_get_slot(NDAS_INVALID_SLOT);
				NDAS_BUG_ON( ndev->info.slot[i] == NDAS_INVALID_SLOT );
			}

			NDAS_BUG_ON( !sdev_is_slot_allocated(ndev->info.slot[i]) );

			memset( &sparam, 0, sizeof(sdev_create_param_t) );

			NDAS_BUG_ON( ndev->info.mode[i] != NDAS_DISK_MODE_SINGLE);

			sparam.slot      = ndev->info.slot[i];
   		    sparam.mode 	 = ndev->info.mode[i];

		    sparam.has_key   = ndev->info.ndas_key[0] ? TRUE: FALSE;

    		strncpy( sparam.ndas_id, ndev->info.ndas_id, NDAS_ID_LENGTH+1 );

		   	sparam.logical_block_size 	= ndev->udev[i]->uinfo.hard_sector_size;
			sparam.logical_blocks 		= ndev->udev[i]->uinfo.hard_sectors;

			sparam.nr_child = 1;

			sparam.child[0].slot    = ndev->info.slot[i];
       		sparam.child[0].ndev    = ndev;
			sparam.child[0].udev    = ndev->udev[i];
			sparam.child[0].unit    = i;

		   	sparam.child[0].headers_per_disk		= ndev->udev[i]->uinfo.headers_per_disk;
		   	sparam.child[0].sectors_per_cylinder	= ndev->udev[i]->uinfo.sectors_per_cylinder;
		   	sparam.child[0].cylinders_per_header	= ndev->udev[i]->uinfo.cylinders_per_header;

			sdev = sdev_create(&sparam);
		}

	    dbg_call( 1, "ed1 ndev->enabled_bitmap =%d\n", ndev->enabled_bitmap );

	    if (ndev->enabled_bitmap) {

          	__u16 enable_flags;
			int   slot = ndev->info.slot[i];

			enable_flags = (ndev->enabled_bitmap >> i*2) & REGDATA_BIT_MASK;

			down( &sdev_lookup_byslot(slot)->sdev_mutex );

            switch (enable_flags) {

			case REGDATA_BIT_SHARE_ENABLED:

            	sdev_cmd_enable_writeshare(slot);
                break;

			case REGDATA_BIT_WRITE_ENABLED:

				sdev_cmd_enable_exclusive_writable(slot);
                break;

			case REGDATA_BIT_READ_ENABLED:

				sdev_cmd_enable_slot(slot);
                break;
			}

			up( &sdev_lookup_byslot(slot)->sdev_mutex );
		}
	}

	up( &ndev->ndev_mutex );

	return;	
}

ndas_error_t ndev_register_device(const char* name, const char *ndas_id_or_serial, const char *ndas_key,
								  __u16 enabled_bitmap, bool use_serial, __u8 *slot, unsigned char slot_num)
{
    ndas_error_t err;

	char ndas_default_id[NDAS_ID_LENGTH + 1] = {0};

    ndev_t	*ndev;
	int		i;

 
    if (!name || !name[0] || strnlen(name, NDAS_MAX_NAME_LENGTH) > NDAS_MAX_NAME_LENGTH) {
 
		NDAS_BUG_ON(true);
        return NDAS_ERROR_INVALID_NAME;
	}

    if (!ndas_id_or_serial) {

		NDAS_BUG_ON(true);
        return NDAS_ERROR_INVALID_NDAS_ID;
	}

    if (ndev_lookup_byname(name)) {

        return NDAS_ERROR_ALREADY_REGISTERED_NAME;
    }

    if (use_serial) {

		err = ndas_sid_2_ndas_id( ndas_id_or_serial, ndas_default_id, NULL );

    } else {

		if (is_vaild_ndas_id(ndas_id_or_serial, ndas_key) == true) {

			err = NDAS_OK;

		} else {

			err = NDAS_ERROR_INVALID_NDAS_ID;
		}
	}

    if (!NDAS_SUCCESS(err)) {

        return err;
    }

    if (use_serial) {

    	err = ndev_lookup_by_ndas_id( ndas_default_id, &ndev, true );

    } else {

	    err = ndev_lookup_by_ndas_id( ndas_id_or_serial, &ndev, true );
    }

    if (!NDAS_SUCCESS(err)) {

		NDAS_BUG_ON(true);
        return err;
    }

    if (ndev->info.name[0]) {

        return NDAS_ERROR_ALREADY_REGISTERED_DEVICE;
    }

    strncpy( ndev->info.name, name, NDAS_MAX_NAME_LENGTH );

	if (is_ndas_writable(ndev->info.ndas_id, ndas_key)) {

        strncpy( ndev->info.ndas_key, ndas_key, NDAS_KEY_LENGTH );
 
	} else {

	    memset( ndev->info.ndas_key, 0 ,NDAS_KEY_LENGTH );
    }

    dbg_call( 1, "ndev=%p slot_num=%d, enabled_bitmap = %d ndas_id=%s\n", ndev, slot_num, enabled_bitmap, ndev->info.ndas_id );

    ndev->enabled_bitmap = enabled_bitmap;
	ndev->info.nr_unit = slot_num;

	for (i=0; i<ndev->info.nr_unit; i++) {

	    dbg_call( 1, "slot[i] = %d\n", slot[i] );
	
		ndev->info.slot[i] = slot[i];

		if (ndev->info.slot[i] != NDAS_INVALID_SLOT) {

			sdev_get_slot( ndev->info.slot[i] );
		}
	}

    err = ndev_register(ndev);

    if (!NDAS_SUCCESS(err)) {

		NDAS_BUG_ON(true);
		ndev_unit_cleanup(ndev);
        return err;
    }

    npnp_subscribe( ndev->info.ndas_id, ndev_pnp_changed );

    if (enabled_bitmap) {

		ndev_pnp_changed( ndev->info.ndas_id, ndev->info.name, NDAS_DEV_STATUS_UNKNOWN );
    }

    return NDAS_OK;
}

static void ndev_unit_cleanup(ndev_t *ndev) 
{
    int unit;

    dbg_call( 3, "ndev=%p\n",ndev );

    for (unit = 0; unit < ndev->info.nr_unit; unit++) {

        sdev_t *sdev = NULL;

		if (ndev->info.slot[unit] >= NDAS_FIRST_SLOT_NR) {

			sdev = sdev_lookup_byslot(ndev->info.slot[unit]);
		}

        if (sdev == NULL) {

            dbg_call( 1, "sdev=NULL\n" );
            continue;
        }

        dbg_call( 1, "ndev->info.slot[unit] = %d, sdev->info.slot = %d\n", ndev->info.slot[unit], sdev->info.slot );

		ndev->info.slot[unit] = NDAS_INVALID_SLOT;
        sdev_cleanup(sdev, NULL);
    }

    for (unit = 0; unit < ndev->info.nr_unit; unit++) {

        udev_t *udev = ndev->udev[unit];

        if (udev == NULL) {

            dbg_call( 1, "udev=NULL\n" );
            continue;
        }

        dbg_call( 1, "ndev->info.slot[unit] = %d, udev->uinfo.slot = %d\n", ndev->info.slot[unit], udev->uinfo.slot );

        udev_cleanup(udev);
        ndev->udev[unit] = NULL;
    }
}

ndas_error_t ndev_unregister_device(const char *name)
{
    int i = 0;
    ndev_t* ndev = ndev_lookup_byname(name);

	dbg_call( 1, "st\n" );

    if (!ndev) {

		NDAS_BUG_ON(true);
        return NDAS_ERROR_INVALID_NAME;
    }

    for (i = 0; i < ndev->info.nr_unit; i++) {

		if (ndev->info.slot[i] >= NDAS_FIRST_SLOT_NR) {

	        sdev_t *sdev;
	
			sdev = sdev_lookup_byslot(ndev->info.slot[i]);

	        if ( sdev && sdev->info.enabled ) {

    	        return NDAS_ERROR_ALREADY_ENABLED_DEVICE;
        	}
		}
    }

	for (i=0; i<NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE; i++) {

		dbg_call( 1, "ndev->info.slot[i] = %d\n", ndev->info.slot[i] );
	
		if (ndev->info.slot[i] != NDAS_INVALID_SLOT) {

			sdev_put_slot( ndev->info.slot[i] );
		}
	}

    npnp_unsubscribe(ndev->info.ndas_id, TRUE);
    ndev_unregister(ndev);
    ndev_unit_cleanup(ndev);

    return NDAS_OK;
}

ndev_t *ndev_create(void) {

	ndev_t  *ndev;
	int		i;

	dbg_call( 1, "start\n" );

	ndev = ndas_kmalloc( sizeof(ndev_t) );

	if (ndev == NULL) {

        return NULL;
    }

    memset( ndev, 0, sizeof(ndev_t) );

	for (i=0; i<NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE; i++) {

		ndev->info.slot[i] = NDAS_INVALID_SLOT;
	}

    sema_init( &ndev->ndev_mutex, 1 );

	return ndev;
}

void ndev_cleanup(ndev_t *ndev)
{
	dbg_call( 1, "start\n" );

	ndev_unit_cleanup(ndev);
    ndas_kfree(ndev);
}

