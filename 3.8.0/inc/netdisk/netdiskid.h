#ifndef __NETDISK__NETDISKID__H__
#define __NETDISK__NETDISKID__H__

#include <sal/libc.h>
#include <ndasuser/def.h>
#include <ndasuser/ndaserr.h>

//
//    keys for XiMeta NetDisk ID V1
//
static const xuchar    NDIDV1Key1[8] = {0x45,0x32,0x56,0x2f,0xec,0x4a,0x38,0x53} ;
static const xuchar    NDIDV1Key2[8] = {0x1e,0x4e,0x0f,0xeb,0x33,0x27,0x50,0xc1} ;
static const xuchar    NDIDV1VID_DEFAULT = 0x01 ;
static const xuchar    NDIDV1VID_SEAGATE = 0x41 ;
static const xuchar    NDIDV1VID_NETCAM = 0x51 ;
static const xuchar    NDIDV1Rsv[2] = { 0xff, 0xff } ;
static const xuchar    NDIDV1Rnd = 0xcd ;


//
//    password for XiMeta NetDisk /Sample
//
//static xuchar    NDIDV1PWSample[8] = {0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x00} ;
//
//    password for XiMeta NetDisk
//

/* xuchar    NDIDV1PW_DEFAULT[8] = {0xbb,0xea,0x30,0x15,0x73,0x50,0x4a,0x1f} ; */
static const xuchar    NDIDV1PW_DEFAULT[8] = { 0x1f, 0x4a, 0x50, 0x73, 0x15, 0x30, 0xea, 0xbb };
static const xuchar    NDIDV1PW_SEAGATE[8] = { 0x99, 0xA2, 0x6E, 0xBC, 0x46, 0x27, 0x41, 0x52 };
static const xuchar	   NDIDV1PW_WINDOWS_RO[8] = { 0x3F, 0x7B, 0x33, 0x1F, 0x48, 0x53, 0x57, 0xBF };

//
//    password for DLink NetDisk
//
//static xuchar    NDIDV1PW_DLink[8] = {0x18,0x61,0xa6,0x88,0x30,0x98,0x00,0xce} ;

static inline
void get_serial_from_networkid(char* serial, unsigned char *network_id)
{
    int sn1, sn2, sn3, sn4;
    sn1 = (((unsigned int)(network_id[4]) & 0x00000080)>>7)
                              + (((unsigned int)(network_id[3]) & 0x000000FF)<<1);
    sn2 = (((unsigned int)(network_id[4]) & 0x0000007F)<<8)
                               + ((unsigned int)(network_id[5]) & 0x000000FF);
    sn3 = (((unsigned int)(network_id[1]) & 0x00000080)>>7)
                              + (((unsigned int)(network_id[0]) & 0x000000FF)<<1);
    sn4 = (((unsigned int)(network_id[1]) & 0x0000007F)<<8)
                               + ((unsigned int)(network_id[2]) & 0x000000FF);
    if (  network_id[0] == 0 && network_id[1] == 0xb && network_id[2] == 0xd0 )
    {
        sal_snprintf((char*)serial, NDAS_SERIAL_LENGTH + 1, "%03d%05d", sn1, sn2);
        sal_memset(
            serial + NDAS_SERIAL_SHORT_LENGTH,
            0,
            (NDAS_SERIAL_LENGTH + 1) - NDAS_SERIAL_SHORT_LENGTH);
    } else {
        sal_snprintf((char*)serial, NDAS_SERIAL_LENGTH + 1, "%03d%05d%03d%05d", sn3, sn4, sn1, sn2);
    }
}

/*
 * 
 */
static inline
ndas_error_t get_networkid_from_serial(unsigned char *network_id, const char* serial)
{
    int i = 0;
    int sn1 = 0, sn2 = 0; 
    // Short form serial
    if ( serial[8] == '\0' ) {
        for (i = 0; i < 8;i++) {
            if ( serial[i] < '0' || serial[i] > '9' )
                return NDAS_ERROR_INVALID_NDAS_ID;
            if ( i < 3 ) 
                sn1 = sn1*10 + (serial[i] - '0');
            else 
                sn2 = sn2*10 + (serial[i] - '0');
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
            if ( serial[i] < '0' || serial[i] > '9' )
                return NDAS_ERROR_INVALID_NDAS_ID;
            if ( i < 3 ) 
                sn1 = sn1*10 + (serial[i] - '0');
            else 
                sn2 = sn2*10 + (serial[i] - '0');
        }

        network_id[0] = (sn1 & 0x1fe) >> 1;
        network_id[1] = ((sn1 & 0x1) << 7) | ((sn2 & 0x7f00) >> 8);
        network_id[2] = (sn2 & 0xff);	

        sn1 = 0; sn2 = 0;
        for (i = 8; i < 16;i++) {
            if ( serial[i] < '0' || serial[i] > '9' )
                return NDAS_ERROR_INVALID_NDAS_ID;
            if ( i < 8 + 3 ) 
                sn1 = sn1*10 + (serial[i] - '0');
            else 
                sn2 = sn2*10 + (serial[i] - '0');
        }
        network_id[3] = (sn1 & 0x1fe) >> 1;
        network_id[4] = ((sn1 & 0x1) << 7) | ((sn2 & 0x7f00) >> 8);
        network_id[5] = (sn2 & 0xff);
    }
    return NDAS_OK;
}
#endif

