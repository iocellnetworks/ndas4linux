/**********************************************************************
 * Copyright ( C ) 2012 IOCELL Networks
 * All rights reserved.
 **********************************************************************/
/* Public APIs of LPX Protocol */

#ifndef _LPX_LPX_H_
#define _LPX_LPX_H_

#include "sal/sal.h" /* NDAS_SAL_API */
#include "sal/net.h"
#include "ndasuser/ndaserr.h"

#define ETH_P_LPX    0x88ad
#define LPX_NODE_LEN    6
#define AF_LPX        29    
#define PF_LPX        AF_LPX
#define LPX_API        NDAS_CALL

#define LPX_TYPE_RAW            0x0
#define LPX_TYPE_DATAGRAM        0x2
#define LPX_TYPE_STREAM        0x3    

#endif /* _NET_LPX_H */
