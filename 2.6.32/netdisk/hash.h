#ifndef _NDDEV_HASH_H_
#define _NDDEV_HASH_H_

#define KEY_CON0                    {0x26, 0x8F, 0x27, 0x36}
#define KEY_CON1                    {0x81, 0x3A, 0x76, 0xBC}

#define HASH_KEY_USER        {0x1F, 0x4A, 0x50, 0x73, 0x15, 0x30, 0xEA, 0xBB}
#define HASH_KEY_SUPER1        {0x0F, 0x0E, 0x0D, 0x03, 0x04, 0x05, 0x06, 0x07}
#define HASH_KEY_SUPER2     {0x3E, 0x2B, 0x32, 0x1A, 0x47, 0x50, 0x13, 0x1E}

#define HASH_KEY_SUPER_LOGIN        HASH_KEY_SUPER2
#define HASH_KEY_SUPER_ENCDEC        HASH_KEY_USER

void
Hash32To128(
            unsigned char    *pSource,
            unsigned char    *pResult,
            unsigned char    *pKey
            );

void
Encrypt32(
          unsigned char        *pData,
          unsigned int    uiDataLength,
          unsigned char        *pKey,
          unsigned char        *pPassword
          );

void
Decrypt32(
          unsigned char        *pData,
          unsigned int        uiDataLength,
          unsigned char        *pKey,
          unsigned char        *pPassword
          );

void
Encrypt32SP(
          unsigned char        *pData,
          unsigned int    uiDataLength,
          unsigned char    *pEncyptIR
          );

void
Encrypt32SPAndCopy(
          unsigned char        *pDestinationData,
          unsigned char        *pSourceData,
          unsigned int    uiDataLength,
          unsigned char    *pEncyptIR
          );

void
Decrypt32SP(
          unsigned char    *pData,
          unsigned int    uiDataLength,
          unsigned char    *pDecryptIR
          );
          
void
Decrypt32SPAndCopy(
            unsigned char        *desData,
            unsigned char        *srcData,
            unsigned int        uiDataLength,
            unsigned char        *pDecryptIR
            );
            
void
Decrypt32SPAndCopy(
            unsigned char        *desData,
            unsigned char        *srcData,
            unsigned int        uiDataLength,
            unsigned char        *pDecryptIR
            );
            
#endif /* _NDDEV_HASH_H_ */

