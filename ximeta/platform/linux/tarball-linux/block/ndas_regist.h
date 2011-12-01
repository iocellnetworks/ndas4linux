#ifndef _NETDISK_REGISTRAR_H_
#define _NETDISK_REGISTRAR_H_

#include "ndas_ndev.h"
#include "ndas_sdev.h"

#include <ndas/def.h>

struct ndas_registered {

    char name[NDAS_MAX_NAME_LENGTH];
};

struct ndas_probed {

    char ndas_sid[NDAS_SERIAL_LENGTH+1];
	char ndas_id[NDAS_ID_LENGTH + 1];

    NDAS_DEV_STATUS status;
};

#include "netdisk.h"

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

ndas_error_t ndev_register(ndev_t* dev);
ndas_error_t ndev_unregister(ndev_t* dev);
/*
 * Register the NDAS device into the registrar when discovered by PNP packet
 * You should call ndev_register when the device is discovered by ndas_register_device function
 */
ndas_error_t ndev_discovered(ndev_t* dev);
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

ndas_error_t ndev_lookup_by_ndas_id (const char *ndas_id, ndev_t **ndev, bool create);

ndas_error_t nosync_disappear(const char *serial, void *noarg1);
ndev_t* ndev_lookup_byname(const char* name);
ndev_t* ndev_lookup_byserial(const char* serial);

ndev_t* ndev_lookup_bynetworkid(unsigned char* network_id);

ndas_error_t ndev_registered_size(void);
ndas_error_t ndev_registered_list(struct ndas_registered *data, int size);

ndas_error_t ndev_probe_size(void);
ndas_error_t ndev_probe_ids(struct ndas_probed* data, int devicen);
//ndas_error_t ndev_probe_serial(struct ndas_probed* data, int devicen);
ndas_error_t ndev_probe_ndas_id(struct ndas_probed *data, int size);

ndas_error_t ndev_set_registration_data(char *data, int size);
ndas_error_t ndev_get_registration_data(char **data,int *size);

ndas_error_t ndev_rename(ndev_t* dev, const char* newname);

ndas_error_t sdev_register(sdev_t * sdev);
ndas_error_t sdev_unregister(sdev_t * sdev);

#endif// _NETDISK_REGISTRAR_H_

