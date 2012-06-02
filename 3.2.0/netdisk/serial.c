#ifndef NDAS_NO_LANSCSI
#include "xplatcfg.h"
#include    "netdisk/serial.h"
#include    "des.h"
#include    "crc.h"
#include     "sal/types.h"
#include     "sal/time.h"
#include     "sal/libc.h" // rand srand

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
} __attribute__((packed)) ndas_id_buff, *ndas_id_buff_ptr ;


LOCAL void            Bin2Serial(unsigned char *bin, unsigned char *write_key, ndas_id_info_ptr pInfo);
LOCAL void            Serial2Bin(ndas_id_info_ptr info, unsigned char *bin, unsigned char *ndas_key);
LOCAL unsigned char    Bin2Char(unsigned char c);
LOCAL unsigned char    Char2Bin(unsigned char c);

int EncryptNdasID(ndas_id_info_ptr pInfo)
{
    xuint32    des_key[32];
    unsigned char    write_key[3];
    ndas_id_buff        encrypted1, encrypted2;
    unsigned short    crc;
 
    if(!pInfo) return(FALSE);

//    srandom(sal_time_msec()); random(); random();

    encrypted1.u.s.ucAd0 = pInfo->ndas_network_id[0];
    encrypted1.u.s.ucAd1 = pInfo->ndas_network_id[1];
    encrypted1.u.s.ucAd2 = pInfo->ndas_network_id[2];
    encrypted1.u.s.ucAd3 = pInfo->ndas_network_id[3];
    encrypted1.u.s.ucAd4 = pInfo->ndas_network_id[4];
    encrypted1.u.s.ucAd5 = pInfo->ndas_network_id[5];
    encrypted1.u.s.ucVid = pInfo->vid;
    encrypted1.u.s.ucReserved0 = pInfo->reserved[0];
    encrypted1.u.s.ucReserved1 = pInfo->reserved[1];
    //encrypted1.u.s.ucRandom = pInfo->ucRandom = pInfo->ucRandom ? pInfo->ucRandom : (unsigned char) random();
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

    Bin2Serial(encrypted2.u.ucSerialBuff, write_key, pInfo);

    return(TRUE);
}

int DecryptNdasID(ndas_id_info_ptr id_info)
{
    unsigned int    des_key[32];
    unsigned char    ndas_key[3], cwrite_key[3];
    ndas_id_buff        encrypted1, encrypted2;
    unsigned short    crc;
    if(!id_info) return(FALSE);

    Serial2Bin(id_info, encrypted1.u.ucSerialBuff, ndas_key);

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
        return(FALSE);
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

    id_info->bIsReadWrite =    (cwrite_key[0] == ndas_key[0]) &&
                (cwrite_key[1] == ndas_key[1]) &&
                (cwrite_key[2] == ndas_key[2]);
    return(TRUE);
}

LOCAL void Bin2Serial(unsigned char *bin, unsigned char *write_key, ndas_id_info_ptr pInfo)
{
    unsigned xint32    temp;

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

LOCAL void Serial2Bin(ndas_id_info_ptr id_info, unsigned char *bin, unsigned char *ndas_key)
{
    xuint32    uiTemp;
    int            i;

    uiTemp = 0;
    for(i = 0; i < 5; i++) {
        uiTemp <<= 5;
        uiTemp |= Char2Bin(id_info->ndas_id[0][i]);
    }
    uiTemp >>= 1;
    bin[0] = (unsigned char) (uiTemp >> 16);
    bin[1] = (unsigned char) (uiTemp >> 8);
    bin[2] = (unsigned char) (uiTemp);

    uiTemp = 0;
    for(i = 0; i < 5; i++) {
        uiTemp <<= 5;
        uiTemp |= Char2Bin(id_info->ndas_id[1][i]);
    }
    uiTemp >>= 1;
    bin[3] = (unsigned char) (uiTemp >> 16);
    bin[4] = (unsigned char) (uiTemp >> 8);
    bin[5] = (unsigned char) (uiTemp);

    uiTemp = 0;
    for(i = 0; i < 5; i++) {
        uiTemp <<= 5;
        uiTemp |= Char2Bin(id_info->ndas_id[2][i]);
    }
    uiTemp >>= 1;
    bin[6] = (unsigned char) (uiTemp >> 16);
    bin[7] = (unsigned char) (uiTemp >> 8);
    bin[8] = (unsigned char) (uiTemp);

    uiTemp = 0;
    for(i = 0; i < 5; i++) {
        uiTemp <<= 5;
        uiTemp |= Char2Bin(id_info->ndas_id[3][i]);
    }
    uiTemp >>= 1;
    bin[9] = (unsigned char) (uiTemp >> 16);
    bin[10] = (unsigned char) (uiTemp >> 8);
    bin[11] = (unsigned char) (uiTemp);

    uiTemp = 0;
    for(i = 0; i < 5; i++) {
        uiTemp <<= 5;
        uiTemp |= Char2Bin(id_info->ndas_key[i]);
    }
    uiTemp >>= 1;
    ndas_key[0] = (unsigned char) (uiTemp >> 16);
    ndas_key[1] = (unsigned char) (uiTemp >> 8);
    ndas_key[2] = (unsigned char) (uiTemp);
}

LOCAL unsigned char Bin2Char(unsigned char c)
{
    char    ch;

    ch = (c >= 10) ? c - 10 + 'A' : c + '0';
    ch = (ch == 'I') ? 'X' : ch;
    ch = (ch == 'O') ? 'Y' : ch;
    return(ch);
}

LOCAL unsigned char Char2Bin(unsigned char c)
{
    c = ((c >= 'a') && (c <= 'z')) ? (c - 'a' + 'A') : c;
    c = (c == 'X') ? 'I' : c;
    c = (c == 'Y') ? 'O' : c;
    return( ((c >= 'A') && (c <= 'Z')) ? c - 'A' + 10 : c - '0' );
}

#endif /* #ifndef NDAS_NO_LANSCSI */

