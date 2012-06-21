#ifndef _NETDISK_NHIX_H_
#define _NETDISK_NHIX_H_

#include "ndasuser/ndaserr.h"
#include "netdisk/list.h" // list_head
#include "sal/net.h" // SAL_ETHER_ADDR_LEN
#include "ndasuser/def.h" // NDAS_MAX_HOSTNAME_LENGTH



#ifdef XPLAT_NDASHIX
extern ndas_error_t nhix_init(void);
extern void nhix_cleanup(void);
extern ndas_error_t nhix_reinit();
/* 
 * Send a request of the permission surrender for the unit device 
 */
extern ndas_error_t nhix_send_request(udev_t * udev);

#else
#define nhix_init() ({ NDAS_OK; })
#define nhix_reinit() ({ NDAS_OK; })
#define nhix_cleanup() do {} while(0)
#endif

#endif

