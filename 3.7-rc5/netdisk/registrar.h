#ifndef _NETDISK_REGISTRAR_H_
#define _NETDISK_REGISTRAR_H_

#include "netdisk/ndev.h"
#include "netdisk/sdev.h"

/*
 * Initialize the registrar that maintains the slot information
 */
ndas_error_t registrar_init(int max_slot);
void registrar_cleanup(void);
void registrar_stop(void);

/*
 * Look up the ndev of the given network_id.
 * ndas_idserial is optional(could be null)
 * Insert ndev into ndasid_2_device , mac_2_device hashtable
 * Called by 
 *   ndas_register_device
 *   v_update_alive_info
 */
ndas_error_t ndev_lookup(xuchar *network_id, ndev_t* *ret);

ndas_error_t ndev_register(ndev_t* dev);
ndas_error_t ndev_unregister(ndev_t* dev);
#ifdef XPLAT_PNP
/*
 * Register the NDAS device into the registrar when discovered by PNP packet
 * You should call ndev_register when the device is discovered by ndas_register_device function
 */
ndas_error_t ndev_discovered(ndev_t* dev);
#endif
/*
 * Remove the NDAS device with the given ndas id.
 * If the NDAS device is registered by ndev_register, 
 * it will return NDAS_ERROR_ALREADY_REGISTERED_DEVICE.
 * return:
 * NDAS_OK if successfully removed.
 * NDAS_ERROR_INTERNAL if the ndasid is not found
 * NDAS_ERROR_ALREADY_REGISTERED_DEVICE if the device is registered with name.
 * NDAS_ERROR_SHUTDOWN if the system is shutdown.
 */
ndas_error_t ndev_vanished(const char *serial);
ndev_t* ndev_lookup_byname(const char* name);
ndev_t* ndev_lookup_byserial(const char* serial);
ndev_t* ndev_lookup_bynetworkid(unsigned char* network_id);

ndas_error_t ndev_registered_size(void);
ndas_error_t ndev_registered_list(struct ndas_registered *data, int size);

#ifdef XPLAT_PNP
ndas_error_t ndev_probe_size(void);
ndas_error_t ndev_probe_ids(struct ndas_probed* data, int devicen);
ndas_error_t ndev_probe_serial(struct ndas_probed* data, int devicen);
#endif

#ifdef XPLAT_RESTORE
ndas_error_t ndev_set_registration_data(char *data, int size);
ndas_error_t ndev_get_registration_data(char **data,int *size);
#endif

ndas_error_t ndev_rename(ndev_t* dev, const char* newname);

ndas_error_t sdev_register(logunit_t * sdev);
ndas_error_t sdev_unregister(logunit_t * sdev);

#endif// _NETDISK_REGISTRAR_H_

