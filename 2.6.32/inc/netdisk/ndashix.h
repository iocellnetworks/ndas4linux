//
// NDAS HIX (Host Information Exchange) Protocol Definition
//
// Copyright (C) 2003-2005 XIMETA, Inc.
// All rights reserved.
//
// Revision History:
// 2004-10-25: Gene Park <jhpark@ximeta.com
// - Ported to NDAS Cross Platform
//
// 2004-10-21: Chesong Lee <cslee@ximeta.com>
// - Initial Documentattion
//
// 2004-XX-XX: Chesong Lee <cslee@ximeta.com>
// - Initial Implementation
//
//
// Introduction:
//
// HIX is a peer-to-peer protocol for NDAS hosts to communicate
// with hosts using a LPX Datagram.
// Using HIX, you can,
//
// - Discover the NDAS hosts that is using a NDAS device in the network
// - Request the NDAS host to surrender a specific access to the NDAS device
// - Query the NDAS host information such as OS version, host name,
//   network address of the host, and NDFS compatible version.
// - Get a notification of the DIB change.
//
// Implementations of the server:
//
// HIX server should listen to LPX datagram port HIX_LISTEN_PORT
// Maximum size of the packet is 512-bytes.
// Any invalid header or the data packet shall be discarded from the server.
//

#ifndef _NDAS_HIX_H_
#define _NDAS_HIX_H_

#include "sal/types.h"

//
// LPX Datagram
//
// HIX_LISTEN_PORT 7918
//
#define HIX_LISTEN_PORT    0x00EE

//
// The following SIGNATURE value is little endian.
// You should use 0x5869684E for bit-endian machines
//
#define HIX_SIGNATURE 0x4E686958
#define HIX_SIGNATURE_CHAR_ARRAY {'N','h','i','X'}

#define HIX_CURRENT_REVISION 0x01

//
// Maximum message length of the HIX packet is 512-bytes.
// Server implementation may allocate a fixed 512 byte buffer
// to receive the data.
//
#define HIX_MAX_MESSAGE_LEN    512
/* */
#define HIX_TYPE_DISCOVER            0x01u
/* */
#define HIX_TYPE_QUERY_HOST_INFO    0x02u
/* */
#define HIX_TYPE_SURRENDER_ACCESS    0x03u
/* */
#define HIX_TYPE_DEVICE_CHANGE        0x04u
/* */
#define HIX_TYPE_UNITDEVICE_CHANGE    0x05u

/* 
 * Update: 3000 sec 
 * 
*/

//
// 32 byte header for NHIX packets.
// HIX_HEADER is also declared as a field of each specialized packet types.
// So you don't have to send the header and the data twice.
// 
typedef struct _NDAS_HIX_HEADER {
    xuint32 signature;
    xuchar revision;                // HIX_CURRENT_REVISION
#if defined(__LITTLE_ENDIAN_BITFIELD)
    unsigned int reply_flag : 1;        // 0 for request, 1 for reply
    unsigned int type : 7;                // HIX_TYPE_XXX
#else
    unsigned int type : 7;                // HIX_TYPE_XXX
    unsigned int reply_flag : 1;        // 0 for request, 1 for reply
#endif    
    xuint16 length;                // Including header
    xuchar hostid[16];        // Generally 16 byte GUID is used
    xuchar reserved[8];            // Should be zero's

} __attribute__ ((__packed__)) hix_header, *hix_header_ptr;

#define HIX_UDA_NO_ACCESS                            0x00
#define HIX_UDA_READ_ACCESS                        0x80
#define HIX_UDA_WRITE_ACCESS                        0x40
#define HIX_UDA_READ_WRITE_ACCESS                    0xD0
#define HIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS    0xE0
#define HIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS    0xF0

#define HIX_UDA_NONE        HIX_UDA_NO_ACCESS
#define HIX_UDA_RO            HIX_UDA_READ_ACCESS
#define HIX_UDA_WO            HIX_UDA_WRITE_ACCESS
#define HIX_UDA_RW            HIX_UDA_READ_WRITE_ACCESS
#define HIX_UDA_SHRW_PRIM    HIX_UDA_SHARED_READ_WRITE_PRIMARY_ACCESS
#define HIX_UDA_SHRW_SEC    HIX_UDA_SHARED_READ_WRITE_SECONDARY_ACCESS

//
// UDA bit set is an bit set structure for representing UDA
// You may cast this to hix_uda
//
typedef struct _HIX_UDA_BITSET {
#if defined(__LITTLE_ENDIAN_BITFIELD)
    xuchar read_access : 1;
    xuchar write_access : 1;
    xuchar shared_rW : 1;
    xuchar shared_rw_primary : 1;
    xuchar reserved : 4;
#else
    xuchar reserved : 4;
    xuchar shared_rw_primary : 1;
    xuchar shared_rW : 1;
    xuchar write_access : 1;
    xuchar read_access : 1;
#endif
} __attribute__ ((__packed__)) hix_uda_bitset, *hix_uda_bitset_ptr;

typedef xuchar hix_uda;

typedef struct _NDAS_HIX_UNITDEVICE_ENTRY_DATA {
    xuchar network_id[SAL_ETHER_ADDR_LEN];
    xuchar unit;
    hix_uda access_type;
} __attribute__ ((__packed__)) hix_udev_entry, *hix_udev_entry_ptr;

typedef struct _NDAS_HIX_HOST_INFO_VER_INFO {
    xuint16 major;
    xuint16 minor;
    xuint16 build;
    xuint16 private;
} __attribute__ ((__packed__)) hix_hi_version, *hix_hi_version_ptr;

// Ad Hoc Information
typedef struct _NDAS_HIX_HOST_INFO_AD_HOC {
    xbool unicode;
    xuchar FieldLength; // up to 255 chars
    xuchar DataLength; // up to 255 chars
    //xuchar Field[];
    //xuchar Data[];
} __attribute__ ((__packed__)) hix_hi_adhoc, *hix_hi_adhoc_ptr;

//
// NHDP Host Info Class Type and Fields
//

#define HIX_HIC_OS                0x00000001
// Required
// Field: xuchar, one of HIX_HIC_OS_xxxx

typedef xuchar HIX_HIC_OS_FAMILY_TYPE;

#define HIX_HIC_OS_UNKNOWN        0x00
#define HIX_HIC_OS_WIN9X        0x01
#define HIX_HIC_OS_WINNT        0x02
#define HIX_HIC_OS_LINUX        0x03
#define HIX_HIC_OS_WINCE        0x04
#define HIX_HIC_OS_PS2            0x05
#define HIX_HIC_OS_MAC            0x06
#define HIX_HIC_OS_EMBEDDED_OS    0x07
#define HIX_HIC_OS_OTHER        0xFF

// Required
// Field: hix_hi_version - hix_hi_version
#define HIX_HIC_OS_VER_INFO            0x00000002

// Optional : 
// Field: CHAR[], null terminated
#define HIX_HIC_HOSTNAME                0x00000004

// Optional
// Field: CHAR[], null terminated
#define HIX_HIC_FQDN                    0x00000008

// Optional
// Field: CHAR[], null terminated
#define HIX_HIC_NETBIOSNAME            0x00000010

// Optional
// Field: WCHAR[], null terminated (WCHAR NULL (0x00 0x00))
// UNICODE on network byte order 
#define HIX_HIC_UNICODE_HOSTNAME        0x00000020

// Optional
// Field: WCHAR[], null terminated (WCHAR NULL (0x00 0x00))
// UNICODE on network byte order 
#define HIX_HIC_UNICODE_FQDN            0x00000040

// Optional
// Field: WCHAR[], null terminated (WCHAR NULL (0x00 0x00))
// UNICODE on network byte order 
#define HIX_HIC_UNICODE_NETBIOSNAME    0x00000080

// Optional
// Field: xuchar Count, xuchar AddressLen = 6, xuchar[][6]
#define HIX_HIC_ADDR_LPX                0x00000100

// Optional
// Field: xuchar Count, xuchar AddressLen = 4, xuchar[][4]
#define HIX_HIC_ADDR_IPV4                0x00000200

// Optional
// Field: xuchar Count, xuchar AddressLen = 16, xuchar[][16]
#define HIX_HIC_ADDR_IPV6                0x00000400

// Required
// Field: hix_hi_version
#define HIX_HIC_NDAS_SW_VER_INFO        0x00000800

// Required if using HIX_UDA_SHRW_xxx
// Field: hix_hi_version
#define HIX_HIC_NDFS_VER_INFO            0x00001000

// Required
// Field: xuint32, flags (network byte order)
#define HIX_HIC_HOST_FEATURE            0x00002000

// #define HIX_HFF_SHARED_WRITE_HOST        0x00000001
#define HIX_HFF_DEFAULT                0x00000001 // should be always set
#define HIX_HFF_SHARED_WRITE_CLIENT    0x00000002
#define HIX_HFF_SHARED_WRITE_SERVER    0x00000004
#define HIX_HFF_AUTO_REGISTER            0x00000100 // reserved
#define HIX_HFF_UPNP_HOST                0x00000200 // reserved

typedef xuint32 HIX_HIC_HOST_FEATURE_TYPE;

// Required
// Field: xuchar, one or more of HIX_HIC_TRANSPORT_{LPX,IP}
#define HIX_HIC_TRANSPORT                0x00004000

#define HIX_TF_LPX        0x01
#define HIX_TF_IP        0x02

typedef xuchar HIX_HIC_TRANSPORT_TYPE;

// Optional
// Field: AD_HOC
#define HIX_HIC_AD_HOC                    0x80000000

typedef struct _NDAS_HIX_HOST_INFO_ENTRY {
    xuchar length; // Entire length (e.g. 1 + 4 + sizeof(data))
    xuint32 class; // 
    xuchar data[0]; // at least one byte data is required
    // Class Specific Data
} __attribute__ ((__packed__)) hix_hi_entry, *hix_hi_entry_ptr;

//
// NDAS HIX Discover 
// Intiated by user.
//
// Discover Request packet is composed of HEADER and REQUEST DATA
// REQUEST data contains the array of Unit Device Entry 
// with a specific access (UDA) bit
//
// UDA bits are:
//
// 1 1 1 1 0 0 0 0
// | | | |
// | | | Shared RW Primary
// | | Shared RW
// | Write
// Read
//
// 1. When the NDAS unit device is used (aka Mounted) at the host,
//    with the any of the access (UDA) specified, the host should
//    reply with the current access (UDA) in the reply packet.
//
//    For example, when host A is using Unit Device U1 as RW mode (Non-NDFS),
//    and the host B sends the request for U1 with HIX_UDA_RO,
//    the host A should reply with U1 with HIX_UDA_RW
//
//    Possible configurations for the host A:
//    RO                        HIX_UDA_RO        (10000000)
//    RW (Non-NDFS)             HIX_UDA_RW        (11000000)
//    RW (Shared RW Secondary)  HIX_UDA_SHRW_SEC  (11100000)
//    RW (Shared RW Primary)    HIX_UDA_SHRW_PRIM (11110000)
//    
// 2. When the EntryCount is 0 (zero), any NDAS hosts that receive 
//    this request should reply with the valid HEADER and the 
//    REPLY data with EntryCount set to 0
//    This special type of DISCOVER is used for discovering
//    NDAS hosts in the network
//
// TIME OUT. guideline
// 
typedef struct _NDAS_HIX_DISCOVER_REQUEST { // broadcasting
    hix_header header;
    xuchar entry_count;
    hix_udev_entry entries[0];
} __attribute__ ((__packed__)) hix_dc_request, *hix_dc_request_ptr;

typedef struct _NDAS_HIX_DISCOVER_REPLY { // sento
    hix_header header;
    xuchar entry_count;
    hix_udev_entry entries[0];
} __attribute__ ((__packed__)) hix_dc_reply, *hix_dc_reply_ptr;

//
// NDAS HIX Query Host Info
//
// Query Host Information Request is composed of a HEADER and dummy DATA.
// For the given request packet for the host information,
// the HIX server should reply with the HEADER and the REPLY data.
// 
// REPLY data contains the hix_host_info, 
// which contains the array of hix_host_info.
// The order of HOST_INFO_ENTRY is not defined and it may be arbitrary.
// Any available information of the host shall be contained 
// in the entry.
// 
// The header and the reply data should not be more than 512 bytes
// and the server may not include the non-critical data from the 
// packet in case of overflow.
//


typedef struct _NDAS_HIX_QUERY_HOST_INFO_REQUEST { // sendto
    hix_header header;
    struct _NDAS_HIX_QUERY_HOST_INFO_REQUEST_DATA {
        xuchar reserved[32];    
    } __attribute__ ((__packed__)) data;
} __attribute__ ((__packed__)) hix_hi_request, *hix_hi_request_ptr;

typedef struct _NDAS_HIX_QUERY_HOST_INFO_REPLY { // sendto
    hix_header header;
    struct _NDAS_HIX_QUERY_HOST_INFO_REPLY_DATA {
        xuchar reserved[32];
        /* sizeof(length) + sizeof(contains)  + sizeof(count) + 
            sum of entries[i].length < 256 
            if bigger than 256, it should be 0xff
         */
        xuchar length;   
        xuint32 contains; // Class contains Flags
        xuchar count; // number of entries
        /* Required fields first then optional fields */
        hix_hi_entry entries[0];
    } __attribute__ ((__packed__)) data;
} __attribute__ ((__packed__)) hix_hi_reply, *hix_hi_reply_ptr;

//
// NDAS HIX Surrender Access
// N    H   S         A
//
// Requesting surrender access to the unit device with UDA
// is interpreted with the following criteria.
// At this time, surrender access is vaild only for the single
// Unit Device Entry.
// For a given UDA of the unit device,
//
// - Only READ_ACCESS and WRITE_ACCESS bit can be set.
//
//   If other bits are set, the request may be invalidated
//   or just the entry can be ignored.
//
//   * Only WRITE_ACCESS and (READ_ACCESS | WRITE_ACCESS) 
//     requests are valid for Windows hosts.
//     Single invalid entry invalidates the entire request.
//
// - READ_ACCESS | WRITE_ACCESS
//
//   To request to surrender both READ and WRITE access, turn the both bit on.
//   The requestee (host) will respond with only the result of 
//   the validation of the packet
//
//   The request host should query the status of the surrender
//   by query NDAS Hosts count from the NDAS device or query DISCOVER
//   packet.
//
// - WRITE_ACCESS
//
//   To request to surrender WRITE access only.
//   the host can 
//   - change the access mode to READ-ONLY or
//   - disconnect the access. 
//   Final reply contains the changed access mode.
//
// - READ_ACCESS (DO NOT USE AT THIS TIME)
//
//   To request to surrender READ access, the requester can turn on
//   only READ_ACCESS bit. READ_ACCESS bit implies to surrender
//   the READ_ACCESS and the host may not surrender WRITE_ACCESS
//   when there are WRITE-ONLY-ACCESSIBLE NDAS devices.
//   This request may be treated as invalid request.
//


typedef struct _NDAS_HIX_SURRENDER_ACCESS_REQUEST { //sendto
    hix_header header;
    xuchar entry_count; // Should be always 1
    hix_udev_entry udev;
} __attribute__ ((__packed__)) hix_sa_request, *hix_sa_request_ptr;

#define NHSA_STATUS_QUEUED 0x01
#define NHSA_STATUS_ERROR  0x02
#define NHSA_STATUS_NO_ACCESS  0x03
#define NHSA_STATUS_INVALID_REQUEST 0xFF

typedef struct _NDAS_HIX_SURRENDER_ACCESS_REPLY {  //sendto
    hix_header header;
    /* NHSA_STATUS_* value */
    xuchar status;
} __attribute__ ((__packed__)) hix_sa_reply, *hix_sa_reply_ptr;

//
// NDAS HIX Change Notify for NDAS Device / NDAS Unit Device
// N    H   C      N
// 
// This is a notification packet for changes of DIB(Disk Information Block)
// to the NDAS hosts in the network. This notification is sent 
// through the broadcast.
//
// Receiving the packet of this type of the given unit device,
// the NDAS host should invalidate the status of DIB of that unit device,
// and should re-read the DIB information from the unit device.
//
// This notification is generally sent from the NDAS Bind application.
//
typedef struct _NDAS_HIX_DEVICE_CHANGE_NOTIFY { // broadcast
    hix_header header;
    xuchar network_id[6];
} __attribute__ ((__packed__)) hix_cn_device, *hix_cn_device_ptr;

typedef struct _NDAS_HIX_UNITDEVICE_CHANGE_NOTIFY { //broadcast
    hix_header header;
    xuchar network_id[6];
    xuchar unit;
} __attribute__ ((__packed__)) hix_cn_unit_device, *hix_cn_unit_device_ptr;

#endif // _NDAS_HIX_H_

