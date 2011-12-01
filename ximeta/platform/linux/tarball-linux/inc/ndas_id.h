/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#ifndef _NDAS_ID_H_
#define _NDAS_ID_H_

#define	NDAS_VENDOR_ID_DEFAULT		((__u8)0x01)
#define	NDAS_VENDOR_ID_LINUX_ONLY  	((__u8)0x10)
#define	NDAS_VENDOR_ID_WINDOWS_RO  	((__u8)0x20)
#define	NDAS_VENDOR_ID_SEAGATE		((__u8)0x41)

#define NDAS_MAX_NAME_LENGTH		128	// Maximum length of NDAS device name including null termination
#define NDAS_NETWORK_ID_LEN			6
#define NDAS_ID_LENGTH              20  // Length of NDAS device ID excluding null termination
#define NDAS_KEY_LENGTH             5   // Length of NDAS device key excluding null termination
#define NDAS_SERIAL_LENGTH          15  // Length of NDAS serial number excluding null termination
#define NDAS_SERIAL_EXTEND_LENGTH   NDAS_SERIAL_LENGTH  // Length of NDAS serial number 
																// in extended format excluding null termination
#define NDAS_SERIAL_SHORT_LENGTH    8	// Length of NDAS serial number in short format excluding null termination

typedef struct ndas_id {

    unsigned char id1[5];
    unsigned char id2[5];
    unsigned char id3[5];
    unsigned char id4[5];

} ndas_id_t, *ndas_id_ptr;

typedef struct ndas_key {

    unsigned char key[5];

} ndas_key_t, *ndas_key_ptr;

typedef struct ndas_idkey {

    ndas_id_t id;
    ndas_key_t key;

} ndas_idkey_t, *ndas_idkey_ptr;

extern unsigned char ndas_vid;

typedef struct _ndas_id_info {

    unsigned char    ndas_network_id[NDAS_NETWORK_ID_LEN];
    unsigned char    vid;
    unsigned char    reserved[2];
    unsigned char    random;
    unsigned char    pad[2]; /* for align */
    unsigned char    key1[8];
    unsigned char    key2[8];
    char             ndas_id[4][5];
    char             ndas_key[5];
    int              bIsReadWrite;

} ndas_id_info, *ndas_id_info_ptr;

#ifdef __KERNEL__

void 		 network_id_2_ndas_sid(const unsigned char *network_id, char *ndas_sid);
ndas_error_t ndas_sid_2_network_id(const char *ndas_sid, unsigned char *network_id);

ndas_error_t ndas_sid_2_ndas_id(const char *ndas_sid, char *ndas_id, char *ndas_key);
ndas_error_t ndas_id_2_ndas_sid(const char *ndas_id, char* ndas_sid);

void network_id_2_ndas_default_id(const unsigned char *network_id, char *ndas_id);

ndas_error_t ndas_id_2_network_id(const char *ndas_id, const char *ndas_key, unsigned char *network_id, bool *writable);

ndas_error_t ndas_id_2_ndas_default_id(const char *ndas_id, char *ndas_default_id);

bool is_ndas_writable(const char *ndas_id, const char* ndas_key);
bool is_vaild_ndas_id(const char *ndas_id, const char *ndas_key);

#endif

#endif // _NDAS_ID_H_
