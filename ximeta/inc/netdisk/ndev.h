#ifndef _NDDEV_NDDEV_H_
#define _NDDEV_NDDEV_H_

#include <sal/thread.h>
#include <sal/time.h>
#include <sal/sync.h>
#include <sal/mem.h>

#include <netdisk/netdisk.h>
#include <ndasuser/ndasuser.h>
#include <ndasuser/ndaserr.h>

#include "xlib/dpc.h"
#include "xlib/xbuf.h"


#include "netdisk/list.h"

#include "lpx/lpxutil.h"
#ifdef NDAS_MSHARE
#include <jukebox/diskmgr.h>
#endif

#include "lspx/lsp_util.h"
#include "netdisk/conn.h"

#define NDDEV_LPX_PORT_NUMBER                10000
#define NDDEV_LPX_PORT_NUMBER_PNP            10002

#define NDAS_SHARE_WRITE_LOCK_ID    0

/* 
    Temporary NDAS device management interface. 
*/

/* 
    Device abstraction for raw disk without aggregation/mirroring/partition.
*/


    /* currently user API uses raw device. Add aggregation/mirroring layer later */


#ifdef PS2DEV
#define ND_MAX_BLOCK_SIZE       (16*1024)
#else
#define ND_MAX_BLOCK_SIZE       (64*1024)
#endif


/* Wait for 2 minutes until marking the disk as offline */
#define PNP_ALIVE_TIMEOUT (30 * SAL_TICKS_PER_SEC)

//
// UDEV request block pool
//
extern void * Urb_pool;

struct _logunit;

/* 
 * Per NDAS device information.
 */
typedef struct _ndev {
    ndas_device_info info;
    xuchar network_id[LPX_NODE_LEN];
    
    /* 
     * unit devices (disk/dvd/etc) that attached to this NDAS device
     * to do: ndev is not directly associated with logunit_t . Need to related to udev.
     */
    struct _logunit* unit[NDAS_MAX_UNIT_DISK_PER_NDAS_DEVICE];
#ifdef XPLAT_PNP
    /* 
     * Last tick that we receive the PNP packet or successful IO.
     * Please note last_tick 0 doesn't mean it never receive the PNP
     */
    sal_tick            last_tick;
#endif
    /*
     * 1 if online notification is fired to the listeners
     * 0 if not fired or it is offline 
     */
    int online_notified;

    /* 
     * This flag will be applied when disk is registered or becomes online.
     */
    xuint16         enable_flags;
}ndev_t;

#ifdef DEBUG
#define NDEV_RESET_NAME(ndev) \
({ \
    /* sal_debug_println_impl(__FILE__,__LINE__,__FUNCTION__, "NDEV_RESET_NAME"); */ \
    (ndev)->info.name[0] = '\0';\
})
#else
#define NDEV_RESET_NAME(ndev) ((ndev)->info.name[0] = '\0')
#endif

#define NDEV_IS_REGISTERED(ndev) ((ndev)->info.name[0] != '\0')

/* 
 * Regiser the ndas device with given name, ndas id, and the ndas key.
 * You can specify if the unit device should be enabled.
 */
ndas_error_t register_internal(const char* name, const char *idserial, const char *ndas_key, xuint32 enable_bits, xbool use_serial);
ndas_error_t ndas_io(int slot, xint8 cmd, ndas_io_request* req, xbool bound_check);
ndas_error_t ndas_io_pc(int slot, ndas_io_request* req, xbool bound_check);


ndev_t* ndev_create(void);
void ndev_cleanup(ndev_t *ndev);
void ndev_unit_cleanup(ndev_t *ndev);

int int_wait(int *flag, sal_msec interval, int count);

ndas_error_t ndas_block_core_init(void);
ndas_error_t ndas_block_core_cleanup(void);

/* 
    To do: clean debugs
*/
#define ENABLE_FLAG(writable, writeshare, raid, pnp) \
    (((writable) ? 0x1 : 0) | ((writeshare) ? 0x2 : 0) | ((raid) ? 0x4 : 0) | ((pnp) ? 0x8 : 0))
#define ENABLE_FLAG_WRITABLE(flag) (((flag)&0x1)? TRUE:FALSE)
#define ENABLE_FLAG_WRITESHARE(flag) (((flag)&0x2)? TRUE:FALSE)
#define ENABLE_FLAG_RAID(flag) (((flag)&0x4)? TRUE:FALSE)
#define ENABLE_FLAG_PNP(flag) (((flag)&0x8)? TRUE:FALSE)
#define ENABLE_FLAG_DELAYED(flag) (((flag)&0x10)? TRUE:FALSE)

#endif /* _NDDEV_NDDEV_H_ */
