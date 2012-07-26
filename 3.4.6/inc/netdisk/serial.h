#ifndef _SERIAL_H_
#define    _SERIAL_H_

typedef struct _ndas_id_info {
    unsigned char    ndas_network_id[6];
    unsigned char    vid;
    unsigned char    reserved[2];
    unsigned char    random;
    unsigned char    pad[2]; /* for align */
    unsigned char    key1[8];
    unsigned char    key2[8];
    char            ndas_id[4][5];
    char            ndas_key[5];
    int            bIsReadWrite;
} ndas_id_info, *ndas_id_info_ptr;

extern int    EncryptNdasID(ndas_id_info_ptr pInfo);
extern int    DecryptNdasID(ndas_id_info_ptr pInfo);
#endif
