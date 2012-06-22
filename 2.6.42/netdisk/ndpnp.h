#ifndef _NETDISK_NDPNP_H_
#define _NETDISK_NDPNP_H_

#include "ndasuser/ndaserr.h"

#ifdef XPLAT_PNP

/**
 * Description
 *  Create the thread to collect the information for the NDAS device in the network.
 *
 * Parameters
 * packet_buffer_size: the size of the buffer to receive the packets.
 *
 * Returns
 * NDAS_OK: success
 */
extern ndas_error_t ndpnp_init(void);

extern ndas_error_t ndpnp_reinit(void);
/*
 * Request to kill the nd/pnp thread
 */
extern void ndpnp_cleanup(void);

/*
 * Notify to the external lister and internal listeners that the status is changed
 * TODO: refactor this function into ndev.c
 */
extern void ndev_notify_status_changed(ndev_t* ndev,NDAS_DEV_STATUS online);

extern xbool pnp_subscribe(const char *serial, ndas_device_handler func);
extern void pnp_unsubscribe(const char *serial, xbool remove_from_list);

#else

#define ndpnp_init() ({NDAS_OK;})
#define ndpnp_reinit() ({NDAS_OK;})
#define ndpnp_cleanup() do {} while(0)
#define pnp_subscribe(x,y) ({NDAS_ERROR_NOT_IMPLEMENTED;})
#define pnp_unsubscribe(x,y) ({NDAS_ERROR_NOT_IMPLEMENTED;})

#endif /* XPLAT_PNP */
#endif

