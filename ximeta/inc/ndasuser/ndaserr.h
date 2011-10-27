/*
 -------------------------------------------------------------------------
 Copyright (c) 2005, XIMETA, Inc., IRVINE, CA, USA.
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
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
*/
#ifndef _NDAS_ERR_H_
#define _NDAS_ERR_H_

typedef int ndas_error_t;

/**
 * MACROs for values of ndas_error_t
 */
#define NDAS_MORE_PROCESSING_RQ (1)
#define NDAS_OK                            (0)
#define NDAS_ERROR                         (-1)
#define NDAS_ERROR_DRIVER_NOT_LOADED            (-2)
#define NDAS_ERROR_DRIVER_ALREADY_LOADED        (-3)
#define NDAS_ERROR_LIBARARY_NOT_INITIALIZED    (-4)
#define NDAS_ERROR_INVALID_NDAS_ID            (-5)
#define NDAS_ERROR_INVALID_NDAS_KEY            (-6)
#define NDAS_ERROR_NOT_IMPLEMENTED                (-8)
#define NDAS_ERROR_INVALID_PARAMETER            (-9)
#define NDAS_ERROR_INVALID_SLOT_NUMBER            (-10)
#define NDAS_ERROR_INVALID_NAME                    (-11)
#define NDAS_ERROR_NO_DEVICE                    (-12)
#define NDAS_ERROR_ALREADY_REGISTERED_DEVICE    (-13)
#define NDAS_ERROR_ALREADY_ENABLED_DEVICE       (-14)
#define NDAS_ERROR_ALREADY_REGISTERED_NAME         (-15)
#define NDAS_ERROR_ALREADY_DISABLED_DEVICE       (-16)
#define NDAS_ERROR_ALREADY_STARTED                (-17)
#define NDAS_ERROR_ALREADY_STOPPED                (-18)
#define NDAS_ERROR_NOT_ONLINE                        (-19)
#define NDAS_ERROR_TRY_AGAIN                    (-20)

#define NDAS_ERROR_NOT_CONNECTED                (-21)
#define NDAS_ERROR_INVALID_HANDLE                (-22)
#define NDAS_ERROR_NO_WRITE_ACCESS_RIGHT         (-23)
#define NDAS_ERROR_WRITE_BUSY                        (-24)
#define NDAS_ERROR_CONNECT_FAILED               (-25)

#define NDAS_ERROR_UNSUPPORTED_HARDWARE_VERSION        (-26)
#define NDAS_ERROR_UNSUPPORTED_SOFTWARE_VERSION        (-27)
#define NDAS_ERROR_UNSUPPORTED_DISK_MODE                (-28)
#define NDAS_ERROR_UNSUPPORTED_FEATURE                    (-29)
#define NDAS_ERROR_BUFFER_OVERFLOW                (-30) /* buffer size is not enough */
#define NDAS_ERROR_NO_NETWORK_INTERFACE                    (-31)
#define NDAS_ERROR_INVALID_OPERATION            (-32)
#define NDAS_ERROR_NETWORK_DOWN                    (-33)
#define NDAS_ERROR_INVALID_METADATA                (-34)
#define NDAS_ERROR_LOCK_LOST                (-35)
#define NDAS_ERROR_ACQUIRE_LOCK_FAILED                        (-36)


#define NDAS_ERROR_MEDIA_CHANGED                (-40)
#define NDAS_ERROR_TIME_OUT                    (-41)
#define NDAS_ERROR_READONLY                    (-42)
#define NDAS_ERROR_OUT_OF_MEMORY            (-43)
#define NDAS_ERROR_EXIST                        (-44)
#define NDAS_ERROR_SHUTDOWN                        (-45)
#define NDAS_ERROR_PROTO_REGISTRATION_FAIL        (-46)
#define NDAS_ERROR_ADDRESS_NOT_AVAIABLE        (-47)
#define NDAS_ERROR_NOT_BOUND                    (-49) /* trying to recv from unbound socket */
#define NDAS_ERROR_INVALID_RANGE_REQUEST      (-50) /* io request exceeds the size of disk*/
#define NDAS_ERROR_SHUTDOWN_IN_PROGRESS     (-51)
#define NDAS_ERROR_CONNECTING_IN_PROGRESS     (-52)

#define NDAS_ERROR_NETWORK_FAIL         (-60)
#define NDAS_ERROR_HARDWARE_DEFECT    (-61)
#define NDAS_ERROR_BAD_SECTOR            (-62)
#define NDAS_ERROR_UNSUPPORTED_HDD_VENDOR	(-63) /* Some NDAS devices are configured to support specific HDD vendor only */

/* Internal HW Error codes */
#define NDAS_ERROR_HDD_DMA2_NOT_SUPPORTED (-70)
#define NDAS_ERROR_IDE_REMOTE_INITIATOR_NOT_EXIST (-71)
#define NDAS_ERROR_IDE_REMOTE_INITIATOR_BAD_COMMAND (-72)
#define NDAS_ERROR_IDE_REMOTE_COMMAND_FAILED (-73)
#define NDAS_ERROR_IDE_REMOTE_AUTH_FAILED (-74)
#define NDAS_ERROR_IDE_TARGET_NOT_EXIST (-75)
#define NDAS_ERROR_IDE_TARGET_BROKEN_DATA (-78)
#define NDAS_ERROR_IDE_VENDOR_SPECIFIC (-79)

#define NDAS_ERROR_TOO_MANY_INVALID (-80)
#define NDAS_ERROR_TOO_MANY_OFFLINE (-81)
#define NDAS_ERROR_LSP_ERROR (-82)

/* On redundency raid system, 
   user should specify a disk as invalid if the whole disk are crashed. */
#define NDAS_ERROR_RAID_CHOOSE_INVALID_DISK (-91)
/* The error is caused by the internal framework bug */
#define NDAS_ERROR_TIMEDOUT_ONLINE          (-98)
#define NDAS_ERROR_INTERNAL                    (-99) 

/* Jukebox error codes */
#define NDAS_ERROR_JUKE_DISC_NOT_SET						(-101)
#define NDAS_ERROR_JUKE_DISC_ADDR_NOT_FOUND			(-102)
#define NDAS_ERROR_JUKE_DISK_NOT_SET						(-103)
#define NDAS_ERROR_JUKE_DISK_NOT_FORMATED				(-104)
#define NDAS_ERROR_JUKE_DISC_WRITE_S_FAIL				(-105)
#define NDAS_ERROR_JUKE_DISC_WRITE_E_FAIL				(-106)
#define NDAS_ERROR_JUKE_DISC_READ_S_FAIL					(-107)
#define NDAS_ERROR_JUKE_DISC_DEL_FAIL					(-108)
#define NDAS_ERROR_JUKE_DISC_VALIDATE_FAIL				(-109)
#define NDAS_ERROR_JUKE_DISK_FORMAT_FAIL				(-110)
#define NDAS_ERROR_JUKE_DISC_INFO_FAIL					(-111)

#define NDAS_ERROR_MAX_USER_ERR_NUM						(-120)

/* Test returned ndas_status_t is success. */
#define NDAS_SUCCESS(x) (NDAS_OK <= (ndas_error_t)(x))

#endif /* _NDAS_ERR_H_ */

