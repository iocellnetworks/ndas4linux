#ifndef _NDDEV_NETDISK_H_
#define _NDDEV_NETDISK_H_

#include "sal/types.h"
#include "ndasuser/info.h"
#include "ndasuser/ndaserr.h"
#include "ndasuser/io.h"

#define ND_SECTOR_SIZE            512

/**
 * nddev_init - Initialize the NDAS system
 */
extern ndas_error_t nddev_init(int io_buffer_size);
/**
 * nddev_cleanup - Clean up the resources allocated for NDAS system
 */
extern ndas_error_t nddev_cleanup(void);

/**
 * Parameters
 * func: function called with the slot number of the NDAS Device
 */
extern ndas_error_t nd_udev_enumerate(void (*func)(int));

#endif /* _NDDEV_NETDISK_H_ */
