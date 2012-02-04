/*
 -------------------------------------------------------------------------
 Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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
 may be distributed under the terms of the GNU General Public License (GPL v2),
 in which case the provisions of the GPL apply INSTEAD OF those given above.

 DISCLAIMER

 This software is provided 'as is' with no explcit or implied warranties
 in respect of any properties, including, but not limited to, correctness 
 and fitness for purpose.
 -------------------------------------------------------------------------
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#include <linux/version.h>
#include <linux/module.h> 

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>


#ifdef DEBUG

int	dbg_level_nid = DBG_LEVEL_NID;

#define dbg_call(l,x...) do { 								\
    if (l <= dbg_level_nid) {								\
        printk("ID|%d|%s|%d|",l,__FUNCTION__, __LINE__); 	\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif

#include "des.h"
#include "crc.h"

#include <ndas_id.h>

typedef struct _ndas_id_buff {

    union {

        unsigned char        ucSerialBuff[12];

        struct {

            unsigned char    ucAd0;
            unsigned char    ucVid;
            unsigned char    ucAd1;
            unsigned char    ucAd2;
            unsigned char    ucReserved0;
            unsigned char    ucAd3;
            unsigned char    ucAd4;
            unsigned char    ucReserved1;
            unsigned char    ucAd5;
            unsigned char    ucRandom;
            unsigned char    ucCRC[2];

        } s;

    } u;

} __attribute__((packed)) ndas_id_buff, *ndas_id_buff_ptr;

#ifndef NDAS_VID

unsigned char	ndas_vid = 0;

#else

#if   NDAS_VID==0x10
unsigned char	ndas_vid = NDAS_VENDOR_ID_LINUX_ONLY;
#elif NDAS_VID==0x20
unsigned char	ndas_vid = NDAS_VENDOR_ID_WINDOWS_RO;
#else
unsigned char	ndas_vid = 0;
#endif

#endif

//    keys for XiMeta NetDisk ID V1

static const __u8    NDIDV1Key1[8] = {0x45,0x32,0x56,0x2f,0xec,0x4a,0x38,0x53};
static const __u8    NDIDV1Key2[8] = {0x1e,0x4e,0x0f,0xeb,0x33,0x27,0x50,0xc1};

static const __u8    NDIDV1Rsv[2] = { 0xff, 0xff };
static const __u8    NDIDV1Rnd = 0xcd;

static const __u8    NDIDV1PW_DEFAULT[8] = { 0x1f, 0x4a, 0x50, 0x73, 0x15, 0x30, 0xea, 0xbb };
static const __u8    NDIDV1PW_WINDOWS_RO[8]   = { 0xBF, 0x57, 0x53, 0x48, 0x1F, 0x33, 0x7B, 0x3F };
static const __u8    NDIDV1PW_SEAGATE[8] = { 0x99, 0xA2, 0x6E, 0xBC, 0x46, 0x27, 0x41, 0x52 };


static unsigned char Bin2Char (unsigned char c)
{
    char    ch;

    ch = (c >= 10) ? c - 10 + 'A' : c + '0';
    ch = (ch == 'I') ? 'X' : ch;
    ch = (ch == 'O') ? 'Y' : ch;

	return ch;
}

static unsigned char Char2Bin (unsigned char c)
{
    c = ((c >= 'a') && (c <= 'z')) ? (c - 'a' + 'A') : c;
    c = (c == 'X') ? 'I' : c;
    c = (c == 'Y') ? 'O' : c;

	return ((c >= 'A') && (c <= 'Z')) ? c - 'A' + 10 : c - '0';
}

static void Bin2Chars (unsigned char *bin, unsigned char *write_key, ndas_id_info_ptr pInfo)
{
    __u32    temp;

    // first 3 bytes => first five serial key

    temp  = bin[0];    temp <<= 8;
    temp |= bin[1];    temp <<= 8;
    temp |= bin[2];    temp <<= 1;

    pInfo->ndas_id[0][0] = Bin2Char((unsigned char) (temp >> 20) & (0x1f));
    pInfo->ndas_id[0][1] = Bin2Char((unsigned char) (temp >> 15) & (0x1f));
    pInfo->ndas_id[0][2] = Bin2Char((unsigned char) (temp >> 10) & (0x1f));
    pInfo->ndas_id[0][3] = Bin2Char((unsigned char) (temp >>  5) & (0x1f));
    pInfo->ndas_id[0][4] = Bin2Char((unsigned char) (temp)       & (0x1f));

    // second 3 bytes => second five serial key

    temp  = bin[3];    temp <<= 8;
    temp |= bin[4];    temp <<= 8;
    temp |= bin[5];    temp <<= 1;

    pInfo->ndas_id[1][0] = Bin2Char((unsigned char) (temp >> 20) & (0x1f));
    pInfo->ndas_id[1][1] = Bin2Char((unsigned char) (temp >> 15) & (0x1f));
    pInfo->ndas_id[1][2] = Bin2Char((unsigned char) (temp >> 10) & (0x1f));
    pInfo->ndas_id[1][3] = Bin2Char((unsigned char) (temp >>  5) & (0x1f));
    pInfo->ndas_id[1][4] = Bin2Char((unsigned char) (temp)       & (0x1f));

    // third 3 bytes => third five serial key

    temp  = bin[6];    temp <<= 8;
    temp |= bin[7];    temp <<= 8;
    temp |= bin[8];    temp <<= 1;

    pInfo->ndas_id[2][0] = Bin2Char((unsigned char) (temp >> 20) & (0x1f));
    pInfo->ndas_id[2][1] = Bin2Char((unsigned char) (temp >> 15) & (0x1f));
    pInfo->ndas_id[2][2] = Bin2Char((unsigned char) (temp >> 10) & (0x1f));
    pInfo->ndas_id[2][3] = Bin2Char((unsigned char) (temp >>  5) & (0x1f));
    pInfo->ndas_id[2][4] = Bin2Char((unsigned char) (temp)       & (0x1f));

    // fourth 3 bytes => fourth five serial key

    temp  = bin[ 9];    temp <<= 8;
    temp |= bin[10];    temp <<= 8;
    temp |= bin[11];    temp <<= 1;

    pInfo->ndas_id[3][0] = Bin2Char((unsigned char) (temp >> 20) & (0x1f));
    pInfo->ndas_id[3][1] = Bin2Char((unsigned char) (temp >> 15) & (0x1f));
    pInfo->ndas_id[3][2] = Bin2Char((unsigned char) (temp >> 10) & (0x1f));
    pInfo->ndas_id[3][3] = Bin2Char((unsigned char) (temp >>  5) & (0x1f));
    pInfo->ndas_id[3][4] = Bin2Char((unsigned char) (temp)       & (0x1f));

    // three password bytes => 5 password characters

    temp  = write_key[0];    temp <<= 8;
    temp |= write_key[1];    temp <<= 8;
    temp |= write_key[2];    temp <<= 1;

    pInfo->ndas_key[0] = Bin2Char((unsigned char) (temp >> 20) & (0x1f));
    pInfo->ndas_key[1] = Bin2Char((unsigned char) (temp >> 15) & (0x1f));
    pInfo->ndas_key[2] = Bin2Char((unsigned char) (temp >> 10) & (0x1f));
    pInfo->ndas_key[3] = Bin2Char((unsigned char) (temp >>  5) & (0x1f));
    pInfo->ndas_key[4] = Bin2Char((unsigned char) (temp)       & (0x1f));
}

static void Chars2Bin(ndas_id_info_ptr id_info, unsigned char *bin, unsigned char *ndas_key)
{
    __u32	uiTemp;
    int     i;

    uiTemp = 0;

    for (i = 0; i < 5; i++) {

        uiTemp <<= 5;
        uiTemp |= Char2Bin(id_info->ndas_id[0][i]);
    }

    uiTemp >>= 1;

    bin[0] = (unsigned char) (uiTemp >> 16);
    bin[1] = (unsigned char) (uiTemp >> 8);
    bin[2] = (unsigned char) (uiTemp);

    uiTemp = 0;

    for (i = 0; i < 5; i++) {

        uiTemp <<= 5;
        uiTemp |= Char2Bin(id_info->ndas_id[1][i]);
    }

    uiTemp >>= 1;

    bin[3] = (unsigned char) (uiTemp >> 16);
    bin[4] = (unsigned char) (uiTemp >> 8);
    bin[5] = (unsigned char) (uiTemp);

    uiTemp = 0;

    for (i = 0; i < 5; i++) {

        uiTemp <<= 5;
        uiTemp |= Char2Bin(id_info->ndas_id[2][i]);
    }

    uiTemp >>= 1;

    bin[6] = (unsigned char) (uiTemp >> 16);
    bin[7] = (unsigned char) (uiTemp >> 8);
    bin[8] = (unsigned char) (uiTemp);

    uiTemp = 0;

    for (i = 0; i < 5; i++) {

        uiTemp <<= 5;
        uiTemp |= Char2Bin(id_info->ndas_id[3][i]);
    }

    uiTemp >>= 1;

    bin[9] = (unsigned char) (uiTemp >> 16);
    bin[10] = (unsigned char) (uiTemp >> 8);
    bin[11] = (unsigned char) (uiTemp);

    uiTemp = 0;

    for (i = 0; i < 5; i++) {

        uiTemp <<= 5;
        uiTemp |= Char2Bin(id_info->ndas_key[i]);
    }

    uiTemp >>= 1;
    ndas_key[0] = (unsigned char) (uiTemp >> 16);
    ndas_key[1] = (unsigned char) (uiTemp >> 8);
    ndas_key[2] = (unsigned char) (uiTemp);

	return;
}


int EncryptNdasID (ndas_id_info_ptr pInfo)
{
    __u32    		des_key[32];
    unsigned char   write_key[3];
    ndas_id_buff    encrypted1, encrypted2;
    unsigned short  crc;
 
	NDAS_BUG_ON( !pInfo );

    encrypted1.u.s.ucAd0 = pInfo->ndas_network_id[0];
    encrypted1.u.s.ucAd1 = pInfo->ndas_network_id[1];
    encrypted1.u.s.ucAd2 = pInfo->ndas_network_id[2];
    encrypted1.u.s.ucAd3 = pInfo->ndas_network_id[3];
    encrypted1.u.s.ucAd4 = pInfo->ndas_network_id[4];
    encrypted1.u.s.ucAd5 = pInfo->ndas_network_id[5];
    encrypted1.u.s.ucVid = pInfo->vid;

    encrypted1.u.s.ucReserved0 = pInfo->reserved[0];
    encrypted1.u.s.ucReserved1 = pInfo->reserved[1];

    encrypted1.u.s.ucRandom = pInfo->random;

    crc = (unsigned short) CRC_calculate(encrypted1.u.ucSerialBuff, 10);

    encrypted1.u.s.ucCRC[0] = (char) (crc & 0xff);
    encrypted1.u.s.ucCRC[1] = (char) ((crc & 0xff00) >> 8);

    // 1st step

    des_ky(pInfo->key1, des_key);
    des_ec((void *) encrypted1.u.ucSerialBuff, (void *) encrypted2.u.ucSerialBuff, (void *) des_key);

    encrypted2.u.ucSerialBuff[ 8] = encrypted2.u.ucSerialBuff[0];
    encrypted2.u.ucSerialBuff[ 9] = encrypted2.u.ucSerialBuff[2];
    encrypted2.u.ucSerialBuff[10] = encrypted2.u.ucSerialBuff[4];
    encrypted2.u.ucSerialBuff[11] = encrypted2.u.ucSerialBuff[6];

    encrypted2.u.ucSerialBuff[0] = encrypted1.u.ucSerialBuff[ 8];
    encrypted2.u.ucSerialBuff[2] = encrypted1.u.ucSerialBuff[ 9];
    encrypted2.u.ucSerialBuff[4] = encrypted1.u.ucSerialBuff[10];
    encrypted2.u.ucSerialBuff[6] = encrypted1.u.ucSerialBuff[11];

    // 2nd step

    des_ky(pInfo->key2, des_key);
    des_dc((void *) encrypted2.u.ucSerialBuff, (void *) encrypted1.u.ucSerialBuff, (void *) des_key);

    encrypted1.u.ucSerialBuff[ 8] = encrypted1.u.ucSerialBuff[1];
    encrypted1.u.ucSerialBuff[ 9] = encrypted1.u.ucSerialBuff[3];
    encrypted1.u.ucSerialBuff[10] = encrypted1.u.ucSerialBuff[5];
    encrypted1.u.ucSerialBuff[11] = encrypted1.u.ucSerialBuff[7];

    encrypted1.u.ucSerialBuff[1] = encrypted2.u.ucSerialBuff[8];
    encrypted1.u.ucSerialBuff[3] = encrypted2.u.ucSerialBuff[9];
    encrypted1.u.ucSerialBuff[5] = encrypted2.u.ucSerialBuff[10];
    encrypted1.u.ucSerialBuff[7] = encrypted2.u.ucSerialBuff[11];

    write_key[0] = encrypted1.u.ucSerialBuff[2];
    write_key[1] = encrypted1.u.ucSerialBuff[4];
    write_key[2] = encrypted1.u.ucSerialBuff[6];

    // 3rd step

    des_ky(pInfo->key1, des_key);
    des_ec((void *) encrypted1.u.ucSerialBuff, (void *) encrypted2.u.ucSerialBuff, (void *) des_key);

    encrypted2.u.ucSerialBuff[8]  = encrypted1.u.ucSerialBuff[8];
    encrypted2.u.ucSerialBuff[9]  = encrypted1.u.ucSerialBuff[9];
    encrypted2.u.ucSerialBuff[10] = encrypted1.u.ucSerialBuff[10];
    encrypted2.u.ucSerialBuff[11] = encrypted1.u.ucSerialBuff[11];

    // Encryption Finished !!
    // Now, Convert encrypted password to Serial Key Form

    Bin2Chars( encrypted2.u.ucSerialBuff, write_key, pInfo );

    return true;
}

int DecryptNdasID (ndas_id_info_ptr id_info)
{
    unsigned int	des_key[32];
    unsigned char	ndas_key[3], cwrite_key[3];
    ndas_id_buff    encrypted1, encrypted2;
    unsigned short  crc;

	NDAS_BUG_ON( !id_info );

    Chars2Bin( id_info, encrypted1.u.ucSerialBuff, ndas_key );

    // reverse 3rd step

    des_ky(id_info->key1, des_key);
    des_dc((void *) encrypted1.u.ucSerialBuff, (void *) encrypted2.u.ucSerialBuff, (void *) des_key);

    encrypted2.u.ucSerialBuff[ 8] = encrypted2.u.ucSerialBuff[1];
    encrypted2.u.ucSerialBuff[ 9] = encrypted2.u.ucSerialBuff[3];
    encrypted2.u.ucSerialBuff[10] = encrypted2.u.ucSerialBuff[5];
    encrypted2.u.ucSerialBuff[11] = encrypted2.u.ucSerialBuff[7];

    encrypted2.u.ucSerialBuff[1] = encrypted1.u.ucSerialBuff[ 8];
    encrypted2.u.ucSerialBuff[3] = encrypted1.u.ucSerialBuff[ 9];
    encrypted2.u.ucSerialBuff[5] = encrypted1.u.ucSerialBuff[10];
    encrypted2.u.ucSerialBuff[7] = encrypted1.u.ucSerialBuff[11];

    // save password decrypted for later comparison

    cwrite_key[0] = encrypted2.u.ucSerialBuff[2];
    cwrite_key[1] = encrypted2.u.ucSerialBuff[4];
    cwrite_key[2] = encrypted2.u.ucSerialBuff[6];


    // reverse 2nd step

    des_ky(id_info->key2, des_key); 
    des_ec((void *) encrypted2.u.ucSerialBuff, (void *) encrypted1.u.ucSerialBuff, (void *) des_key);

    encrypted1.u.ucSerialBuff[ 8] = encrypted1.u.ucSerialBuff[0];
    encrypted1.u.ucSerialBuff[ 9] = encrypted1.u.ucSerialBuff[2];
    encrypted1.u.ucSerialBuff[10] = encrypted1.u.ucSerialBuff[4];
    encrypted1.u.ucSerialBuff[11] = encrypted1.u.ucSerialBuff[6];

    encrypted1.u.ucSerialBuff[0] = encrypted2.u.ucSerialBuff[ 8];
    encrypted1.u.ucSerialBuff[2] = encrypted2.u.ucSerialBuff[ 9];
    encrypted1.u.ucSerialBuff[4] = encrypted2.u.ucSerialBuff[10];
    encrypted1.u.ucSerialBuff[6] = encrypted2.u.ucSerialBuff[11];

    // reverse 1st step

    des_ky(id_info->key1, des_key);
    des_dc((void *) encrypted1.u.ucSerialBuff, (void *) encrypted2.u.ucSerialBuff, (void *) des_key);

    encrypted2.u.ucSerialBuff[ 8] = encrypted1.u.ucSerialBuff[ 8];
    encrypted2.u.ucSerialBuff[ 9] = encrypted1.u.ucSerialBuff[ 9];
    encrypted2.u.ucSerialBuff[10] = encrypted1.u.ucSerialBuff[10];
    encrypted2.u.ucSerialBuff[11] = encrypted1.u.ucSerialBuff[11];

    // decryption Finished !!
    // Now, compare checksum
    // if checksum is mismatch, then invalid serial

    crc = (unsigned short) CRC_calculate(encrypted2.u.ucSerialBuff, 10);

    if( (encrypted2.u.s.ucCRC[0] != (crc & 0xff)) || (encrypted2.u.s.ucCRC[1] != ((crc & 0xff00) >> 8)) ) {

        return false;
    }

    // ok, it's valid serial
    // Now, move retrieved eth addr, vid, reserved words to id_info and return

    id_info->ndas_network_id[0] = encrypted2.u.s.ucAd0;
    id_info->ndas_network_id[1] = encrypted2.u.s.ucAd1;
    id_info->ndas_network_id[2] = encrypted2.u.s.ucAd2;
    id_info->ndas_network_id[3] = encrypted2.u.s.ucAd3;
    id_info->ndas_network_id[4] = encrypted2.u.s.ucAd4;
    id_info->ndas_network_id[5] = encrypted2.u.s.ucAd5;

    id_info->vid = encrypted2.u.s.ucVid;

    id_info->reserved[0] = encrypted2.u.s.ucReserved0;
    id_info->reserved[1] = encrypted2.u.s.ucReserved1;

    id_info->random = encrypted2.u.s.ucRandom;

    id_info->bIsReadWrite = (cwrite_key[0] == ndas_key[0]) &&
                			(cwrite_key[1] == ndas_key[1]) &&
                			(cwrite_key[2] == ndas_key[2]);

	return true;
}

void network_id_2_ndas_sid (const unsigned char *network_id, char *ndas_sid)
{
    int sn1, sn2, sn3, sn4;

    sn1 = (((unsigned int)(network_id[4]) & 0x00000080)>>7) + (((unsigned int)(network_id[3]) & 0x000000FF)<<1);

    sn2 = (((unsigned int)(network_id[4]) & 0x0000007F)<<8) + ((unsigned int)(network_id[5]) & 0x000000FF);

    sn3 = (((unsigned int)(network_id[1]) & 0x00000080)>>7) + (((unsigned int)(network_id[0]) & 0x000000FF)<<1);

    sn4 = (((unsigned int)(network_id[1]) & 0x0000007F)<<8) + ((unsigned int)(network_id[2]) & 0x000000FF);

	if (network_id[0] == 0 && network_id[1] == 0xb && network_id[2] == 0xd0) {

        snprintf((char*)ndas_sid, NDAS_SERIAL_LENGTH + 1, "%03d%05d", sn1, sn2);

        memset( ndas_sid + NDAS_SERIAL_SHORT_LENGTH,
            	0,
            	(NDAS_SERIAL_LENGTH + 1) - NDAS_SERIAL_SHORT_LENGTH);

    } else {

        snprintf( (char*)ndas_sid, NDAS_SERIAL_LENGTH + 1, "%03d%05d%03d%05d", sn3, sn4, sn1, sn2 );
    }

	return;
}

ndas_error_t ndas_sid_2_network_id (const char *ndas_sid, unsigned char *network_id)
{
    int i = 0;
    int sn1 = 0, sn2 = 0; 

    // Short form serial

    if (ndas_sid[8] == '\0') {

        for (i = 0; i < 8;i++) {

            if (ndas_sid[i] < '0' || ndas_sid[i] > '9') {

				NDAS_BUG_ON( true );
                return NDAS_ERROR_INVALID_NDAS_ID;
			}

            if (i < 3) {
 
                sn1 = sn1*10 + (ndas_sid[i] - '0');

			} else {
 
                sn2 = sn2*10 + (ndas_sid[i] - '0');
			}
        }

        network_id[3] = (sn1 & 0x1fe) >> 1;
        network_id[4] = ((sn1 & 0x1) << 7) | ((sn2 & 0x7f00) >> 8);
        network_id[5] = (sn2 & 0xff);	

        network_id[0] = 0;
        network_id[1] = 0xb;
        network_id[2] = 0xd0;

        return NDAS_OK;

    } else {

        // long form serial

        sn1 = 0; sn2 = 0;

        for (i = 0; i < 8;i++) {

            if (ndas_sid[i] < '0' || ndas_sid[i] > '9') {

				NDAS_BUG_ON(true);
                return NDAS_ERROR_INVALID_NDAS_ID;
			}

            if ( i < 3 ) {

                sn1 = sn1*10 + (ndas_sid[i] - '0');

			} else {

                sn2 = sn2*10 + (ndas_sid[i] - '0');
			}
        }

        network_id[0] = (sn1 & 0x1fe) >> 1;
        network_id[1] = ((sn1 & 0x1) << 7) | ((sn2 & 0x7f00) >> 8);
        network_id[2] = (sn2 & 0xff);	

        sn1 = 0; sn2 = 0;

        for (i = 8; i < 16;i++) {

            if (ndas_sid[i] < '0' || ndas_sid[i] > '9') {

				NDAS_BUG_ON(true);

                return NDAS_ERROR_INVALID_NDAS_ID;
			}

            if (i < 8 + 3) {
 
                sn1 = sn1*10 + (ndas_sid[i] - '0');

            } else {
 
                sn2 = sn2*10 + (ndas_sid[i] - '0');
			}
        }

        network_id[3] = (sn1 & 0x1fe) >> 1;
        network_id[4] = ((sn1 & 0x1) << 7) | ((sn2 & 0x7f00) >> 8);
        network_id[5] = (sn2 & 0xff);
    }

    return NDAS_OK;
}
 
ndas_error_t ndas_sid_2_ndas_id (const char *ndas_sid, char *ndas_id, char *ndas_key)
{
    ndas_id_info	nid;
    ndas_error_t	ret;

    NDAS_BUG_ON( !ndas_sid || !ndas_id );

	memset( &nid, 0, sizeof(ndas_id_info) );

    ret = ndas_sid_2_network_id( ndas_sid, nid.ndas_network_id );

    if (ret != NDAS_OK) {

        return ret;
    }

    memcpy( nid.key1, NDIDV1Key1, 8 );
    memcpy( nid.key2, NDIDV1Key2, 8 );
    memcpy( nid.reserved, NDIDV1Rsv, 2 );

	if (ndas_vid == 0) {

		nid.vid = NDAS_VENDOR_ID_DEFAULT; 
	
	} else {

		nid.vid = ndas_vid;
	}

    nid.random = NDIDV1Rnd;

    if (EncryptNdasID(&nid)) {

		memcpy( ndas_id, 	nid.ndas_id[0], NDAS_KEY_LENGTH );
		memcpy( ndas_id+5,  nid.ndas_id[1], NDAS_KEY_LENGTH );
		memcpy( ndas_id+10, nid.ndas_id[2], NDAS_KEY_LENGTH );
		memcpy( ndas_id+15, nid.ndas_id[3], NDAS_KEY_LENGTH );

		ndas_id[NDAS_ID_LENGTH] = 0;

		if (ndas_key) {

	    	memcpy( ndas_key, nid.ndas_key, NDAS_KEY_LENGTH );
    		ndas_key[NDAS_KEY_LENGTH] = 0;
		}
#if 0

		// Hide last 5 digit and key

		memset(id+15, '*', NDAS_ID_LENGTH);
		id[NDAS_ID_LENGTH] = 0;
		memset(key, '*', NDAS_KEY_LENGTH);
		key[NDAS_KEY_LENGTH] = 0;

#endif
        return NDAS_OK;

	} else {

	    return NDAS_ERROR_INVALID_PARAMETER;
    }
}

ndas_error_t ndas_id_2_ndas_sid (const char *ndas_id, char *ndas_sid) 
{
	ndas_error_t	err;
    __u8 			network_id[NDAS_NETWORK_ID_LEN];

    dbg_call( 3, "ndas_id=%s\n", ndas_id );

    NDAS_BUG_ON( ndas_id == NULL );

	err = ndas_id_2_network_id( ndas_id, NULL, network_id, NULL );

    if (!NDAS_SUCCESS(err)) {

        return err;
    }

    network_id_2_ndas_sid( network_id, ndas_sid );

    return NDAS_OK;
}

void network_id_2_ndas_default_id(const __u8 *network_id, char* ndas_default_id) 
{
    ndas_id_info nid;


    dbg_call( 6, "ing network_id=%p ndas_default_id=%p\n", network_id, ndas_default_id );

    memcpy( nid.ndas_network_id, network_id, 6 );
    memcpy( nid.key1, NDIDV1Key1, 8 );
    memcpy( nid.key2, NDIDV1Key2, 8 );
    memcpy( nid.reserved, NDIDV1Rsv, 2 );

#ifdef NDAS_VID
    nid.vid = NDAS_VID;
#else
    nid.vid = NDAS_VENDOR_ID_DEFAULT;
#endif

    nid.random = NDIDV1Rnd;

    EncryptNdasID(&nid);

    memcpy( ndas_default_id, nid.ndas_id, NDAS_ID_LENGTH );
    ndas_default_id[NDAS_ID_LENGTH] = 0;

    dbg_call( 6, "TRUE\n" );
}

ndas_error_t ndas_id_2_network_id(const char *ndas_id, const char *ndas_key, __u8 *network_id, bool *writable) 
{
    ndas_id_info        info;

    dbg_call( 3, "ing: %s %s\n", ndas_id, ndas_key );

    // inputs for decryption
    // NDAS ID / NDAS Key(Write key)

    memcpy( info.ndas_id[0], &ndas_id[0], 5 );
    memcpy( info.ndas_id[1], &ndas_id[5], 5 );
    memcpy( info.ndas_id[2], &ndas_id[10], 5 );
    memcpy( info.ndas_id[3], &ndas_id[15], 5 );

    if (ndas_key) {

        memcpy( info.ndas_key, ndas_key, 5 );

    } else {

        memset( info.ndas_key, 0, 5 );
    }

    //    two keys for decryption

    memcpy( info.key1, NDIDV1Key1, 8 );
    memcpy( info.key2, NDIDV1Key2, 8 );

    if (DecryptNdasID(&info) == true) {

		if (info.vid == NDAS_VENDOR_ID_DEFAULT 	&&
		    info.reserved[0] == NDIDV1Rsv[0] 	&&
		    info.reserved[1] == NDIDV1Rsv[1] 	&&
		    info.random == NDIDV1Rnd) {

            dbg_call( 9, "valid id&key\n" );

            // Key is correct.

            if (network_id) {

                memcpy(network_id, info.ndas_network_id, 6);
			}

            if (writable) {
 
                *writable = info.bIsReadWrite;
			}

            return NDAS_OK;

		} else if (info.vid == NDAS_VENDOR_ID_LINUX_ONLY 	&&
		   		   info.reserved[0] == NDIDV1Rsv[0] 		&&
		   		   info.reserved[1] == NDIDV1Rsv[1] 		&&
		   		   info.random == NDIDV1Rnd) {

            dbg_call( 9, "valid id&key(linux only)\n" );

            // Key is correct.

            if (network_id) {

                memcpy( network_id, info.ndas_network_id, 6 );
			}

            if (writable) {
 
                *writable = info.bIsReadWrite;
			}

            return NDAS_OK;

        } else if (info.vid == NDAS_VENDOR_ID_WINDOWS_RO 	&&
				   info.reserved[0] == NDIDV1Rsv[0] 		&&
            	   info.reserved[1] == NDIDV1Rsv[1] 		&&
            	   info.random == NDIDV1Rnd) {

            dbg_call( 9, "valid id&key(windows read only)\n" );

            // Key is correct.

            if (network_id) {

                memcpy( network_id, info.ndas_network_id, 6 );
			}

            if (writable) {
 
                *writable = info.bIsReadWrite;
			}

            return NDAS_OK;

        } else if (info.vid == NDAS_VENDOR_ID_SEAGATE	&&
				   info.reserved[0] == NDIDV1Rsv[0] 	&&
				   info.reserved[1] == NDIDV1Rsv[1] 	&&
				   info.random == NDIDV1Rnd) {

            dbg_call( 9, "valid id&key(seagate)\n" );

            // Key is correct.

            if (network_id) {

                memcpy( network_id, info.ndas_network_id, 6 );
			}

            if (writable) {
 
                *writable = info.bIsReadWrite;
			}

            return NDAS_OK;
        }
    }

    return NDAS_ERROR_INVALID_NDAS_ID;
}

ndas_error_t ndas_id_2_ndas_default_id(const char *ndas_id, char *ndas_default_id)
{
    ndas_error_t	ret;
    ndas_id_info	nid;

    NDAS_BUG_ON( !ndas_id || !ndas_default_id );

	memset( &nid, 0, sizeof(ndas_id_info) );

    ret = ndas_id_2_network_id( ndas_id, NULL, nid.ndas_network_id, NULL );

    if (ret != NDAS_OK) {

        return ret;
    }

    memcpy( nid.key1, NDIDV1Key1, 8 );
    memcpy( nid.key2, NDIDV1Key2, 8 );
    memcpy( nid.reserved, NDIDV1Rsv, 2 );
 
    nid.vid = NDAS_VENDOR_ID_DEFAULT;

    nid.random = NDIDV1Rnd;

    if (EncryptNdasID(&nid)) {

		memcpy( ndas_default_id,   	nid.ndas_id[0], NDAS_KEY_LENGTH );
		memcpy( ndas_default_id+5,  nid.ndas_id[1], NDAS_KEY_LENGTH );
		memcpy( ndas_default_id+10, nid.ndas_id[2], NDAS_KEY_LENGTH );
		memcpy( ndas_default_id+15, nid.ndas_id[3], NDAS_KEY_LENGTH );

		ndas_default_id[NDAS_ID_LENGTH] = 0;

        return NDAS_OK;

	} else {

	    return NDAS_ERROR_INVALID_PARAMETER;
    }
}

bool is_ndas_writable(const char *ndas_id, const char *ndas_key)
{
    ndas_id_info	info;

    dbg_call( 3, "ing: %s %s\n", ndas_id, ndas_key );

    memcpy( info.ndas_id[0], &ndas_id[0], 5 );
    memcpy( info.ndas_id[1], &ndas_id[5], 5 );
    memcpy( info.ndas_id[2], &ndas_id[10], 5 );
    memcpy( info.ndas_id[3], &ndas_id[15], 5 );

    if (ndas_key) {

        memcpy( info.ndas_key, ndas_key, 5 );

    } else {

        memset( info.ndas_key, 0, 5 );
    }

    memcpy( info.key1, NDIDV1Key1, 8 );
    memcpy( info.key2, NDIDV1Key2, 8 );

    if (DecryptNdasID(&info) == true) {

	    dbg_call( 1, "info.bIsReadWrite = %d\n", info.bIsReadWrite );
	
		return info.bIsReadWrite;
    }
	
	NDAS_BUG_ON(true);
	
	return false;
}

bool is_vaild_ndas_id(const char *ndas_id, const char *ndas_key) 
{
    ndas_id_info	info;

    dbg_call( 3, "ing: %s %s\n", ndas_id, ndas_key );

    // inputs for decryption
    // NDAS ID / NDAS Key(Write key)

    memcpy( info.ndas_id[0], &ndas_id[0], 5 );
    memcpy( info.ndas_id[1], &ndas_id[5], 5 );
    memcpy( info.ndas_id[2], &ndas_id[10], 5 );
    memcpy( info.ndas_id[3], &ndas_id[15], 5 );

    if (ndas_key) {

        memcpy( info.ndas_key, ndas_key, 5 );

    } else {

        memset( info.ndas_key, 0, 5 );
    }

    //    two keys for decryption

    memcpy( info.key1, NDIDV1Key1, 8 );
    memcpy( info.key2, NDIDV1Key2, 8 );

    if (DecryptNdasID(&info) == false) {

		return false;
	}

	if (ndas_vid == 0) {

		return true;
	}

	if (ndas_vid == info.vid) {

		return true;
	
	} else {

		return false;
	}

	return false;
}
