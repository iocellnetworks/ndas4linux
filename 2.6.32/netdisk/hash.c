#if defined(NDAS_EMU) || defined(NDAS_MSHARE)

/*
    !! Need to check this functions works in big-endian system, too
*/

#include "sal/types.h"
#include "hash.h"
#include "sal/debug.h"
#define ROTL(X,n)  ( ( ( X ) << n ) | ( ( X ) >> ( 32 - n ) ) )
#define ROTR(X,n)  ( ( ( X ) >> n ) | ( ( X ) << ( 32 - n ) ) )

//#define DISABLE_ALIGN_OPTIMIZATION 1 
#undef DISABLE_ALIGN_OPTIMIZATION 

/*
    All input and output data is Big-endian.
*/

void
Hash32To128(
            unsigned char    *pSource,
            unsigned char    *pResult,
            unsigned char    *pKey
            )
{
    unsigned char    ucKey_con0[4] = KEY_CON0;
    unsigned char    ucKey_con1[4] = KEY_CON1;

    pResult[ 0] = pKey[5] ^ pSource[2];
    pResult[ 1] = ucKey_con0[0] | pSource[1];
    pResult[ 2] = ucKey_con0[1] & pSource[2];
    pResult[ 3] = ~(pKey[1] ^ pSource[1]);
    pResult[ 4] = ucKey_con1[1] ^ pSource[0];
    pResult[ 5] = pKey[7] & pSource[3];
    pResult[ 6] = ~(ucKey_con1[3] ^ pSource[2]);
    pResult[ 7] = pKey[2] | pSource[0];
    pResult[ 8] = pKey[0] & pSource[1];
    pResult[ 9] = ucKey_con0[3] ^ pSource[2];
    pResult[10] = pKey[3] ^ pSource[0];
    pResult[11] = ~(ucKey_con1[2] ^ pSource[3]);
    pResult[12] = pKey[4] ^ pSource[1];
    pResult[13] = ucKey_con0[2] & pSource[0];
    pResult[14] = pKey[6] ^ pSource[0];
    pResult[15] = ucKey_con1[0] | pSource[3];

    return;
}

void
Encrypt32(
          unsigned char        *pData,
          unsigned int         uiDataLength,
          unsigned char        *pKey,
          unsigned char        *pPassword
          )
{
    unsigned int    i;//, j;
    register char    temp0;
    register char    temp1;
    register char    temp2;
    register char    temp3;
    unsigned char    *pDataPtr;

    for(i = 0; i < uiDataLength / 4; i++) {
        pDataPtr = &pData[i * 4];

        temp0 = (pPassword[6] ^ pPassword[0] ^ pDataPtr[3] ^ pKey[0]);
        temp1 = ~(pPassword[7] ^ pPassword[4] ^ pKey[3] ^ pDataPtr[0]);
        temp2 = pPassword[5] ^ pPassword[1] ^ pDataPtr[1] ^ pKey[1];
        temp3 = ~(pPassword[3] ^ pPassword[2] ^ pDataPtr[2] ^ pKey[2]);
        
        pDataPtr[0] = temp0;
        pDataPtr[1] = temp1;
        pDataPtr[2] = temp2;
        pDataPtr[3] = temp3;
    }
}

void 
Encrypt32SPAligned(
            unsigned char        *pData,
            unsigned int        uiDataLength,
            unsigned char        *pEncryptIR
            )
{
#ifdef __LITTLE_ENDIAN_BYTEORDER    
    register xuint32 dwEncryptIR = (pEncryptIR[0]) + (pEncryptIR[1]<<8) + (pEncryptIR[2]<<16) + (pEncryptIR[3]<<24);
#else
    register xuint32 dwEncryptIR = (pEncryptIR[0]<<24) + (pEncryptIR[1]<<16) + (pEncryptIR[2]<<8) + (pEncryptIR[3]);
#endif
    register xuint32* pDataPtr;
    register xuint32  dwBuf;
    register xuint32* toPtr = (xuint32*)(pData + uiDataLength);
    for(pDataPtr = (xuint32*) pData; pDataPtr < toPtr; pDataPtr++) {
#ifdef __LITTLE_ENDIAN_BYTEORDER            
        dwBuf = ROTL(*pDataPtr, 8);
        *pDataPtr = dwEncryptIR ^ dwBuf;
        *pDataPtr = (*pDataPtr)^(0xff00ff00);
#else
        dwBuf = ROTR(*pDataPtr, 8);
        *pDataPtr = dwEncryptIR ^ dwBuf;
        *pDataPtr = (*pDataPtr)^(0x00ff00ff);
#endif
    }
}


void
Encrypt32SP(
            unsigned char        *pData,
            unsigned int        uiDataLength,
            unsigned char        *pEncryptIR
            )
{
    unsigned int    i;//, j;
    register char    temp0;
    register char    temp1;
    register char    temp2;
    register char    temp3;
    unsigned char    *pDataPtr;
#if !defined(DISABLE_ALIGN_OPTIMIZATION)
    if ((long)pData%4==0) { // We can use faster version for align input
        Encrypt32SPAligned(pData, uiDataLength, pEncryptIR);
        return;
    }
#endif

    for(i = 0; i < uiDataLength / 4; i++) {
        pDataPtr = &pData[i * 4];

        temp0 = pEncryptIR[0] ^ pDataPtr[3];
        temp1 = ~(pEncryptIR[1] ^ pDataPtr[0]);
        temp2 = pEncryptIR[2] ^ pDataPtr[1];
        temp3 = ~(pEncryptIR[3] ^ pDataPtr[2]);
        
        pDataPtr[0] = temp0;
        pDataPtr[1] = temp1;
        pDataPtr[2] = temp2;
        pDataPtr[3] = temp3;
    }
}

void
Encrypt32SPAndCopyAligned(
                   unsigned char        *pDestinationData,
                   unsigned char        *pSourceData,
                   unsigned int        uiDataLength,
                   unsigned char        *pEncryptIR
                   )
{
#ifdef __LITTLE_ENDIAN_BYTEORDER    
    register xuint32 dwEncryptIR = (pEncryptIR[0]) + (pEncryptIR[1]<<8) + (pEncryptIR[2]<<16) + (pEncryptIR[3]<<24);
#else
    register xuint32 dwEncryptIR = (pEncryptIR[0]<<24) + (pEncryptIR[1]<<16) + (pEncryptIR[2]<<8) + (pEncryptIR[3]);
#endif
    register xuint32* pDataPtr;
    register xuint32* pDestDataPtr;
    register xuint32* toPtr;
    register xuint32  dwBuf;
    toPtr = (xuint32*)(pSourceData + uiDataLength);
    pDataPtr = (xuint32*)pSourceData;
    pDestDataPtr = (xuint32*)pDestinationData;
    for( ; pDataPtr < toPtr; pDataPtr++, pDestDataPtr++) {
#ifdef __LITTLE_ENDIAN_BYTEORDER    
        dwBuf = ROTL(*pDataPtr, 8);
        dwBuf = dwEncryptIR ^ dwBuf;
        *pDestDataPtr = dwBuf^(0xff00ff00);
#else
        dwBuf = ROTR(*pDataPtr, 8);
        dwBuf = dwEncryptIR ^ dwBuf;
        *pDestDataPtr = dwBuf^(0x00ff00ff);
#endif
    }
}

void
Encrypt32SPAndCopy(
                   unsigned char        *pDestinationData,
                   unsigned char        *pSourceData,
                   unsigned int            uiDataLength,
                   unsigned char        *pEncryptIR
                   )
{
    unsigned int    i;
    //register char    temp0;
    //register char    temp1;
    //register char    temp2;
    //register char    temp3;
    unsigned char    *pSrcDataPtr;
    unsigned char    *pDestDataPtr;
#if !defined(DISABLE_ALIGN_OPTIMIZATION)    
    if ((long)pDestinationData%4==0 && (long)pSourceData%4==0) { // We can use faster version for align input
        Encrypt32SPAndCopyAligned(pDestinationData, pSourceData, uiDataLength, pEncryptIR);
        return;
    }
#endif
    for(i = 0; i < uiDataLength / 4; i++) {
        pSrcDataPtr = &pSourceData[i * 4];
        pDestDataPtr = &pDestinationData[i * 4];

        pDestDataPtr[0] = pEncryptIR[0] ^ pSrcDataPtr[3];
        pDestDataPtr[1] = ~(pEncryptIR[1] ^ pSrcDataPtr[0]);
        pDestDataPtr[2] = pEncryptIR[2] ^ pSrcDataPtr[1];
        pDestDataPtr[3] = ~(pEncryptIR[3] ^ pSrcDataPtr[2]);
    }
}

void
Decrypt32(
          unsigned char        *pData,
          unsigned    int        uiDataLength,
          unsigned char        *pKey,
          unsigned char        *pPassword
          )
{ 
    unsigned int    i;//, j;
    register char    temp0;
    register char    temp1;
    register char    temp2;
    register char    temp3;
    unsigned char    *pDataPtr;
    for(i = 0; i < uiDataLength / 4; i++) {
        pDataPtr = &pData[i * 4];

        temp0 = pPassword[7] ^ pPassword[4] ^ pKey[3] ^ ~(pDataPtr[1]);
        temp1 = pPassword[5] ^ pPassword[1] ^ pDataPtr[2] ^ pKey[1];
        temp2 = pPassword[3] ^ pPassword[2] ^ pDataPtr[3] ^ ~(pKey[2]);
        temp3 = pPassword[6] ^ pPassword[0] ^ pDataPtr[0] ^ pKey[0];

        pDataPtr[0] = temp0;
        pDataPtr[1] = temp1;
        pDataPtr[2] = temp2;
        pDataPtr[3] = temp3;
    }

}

void
Decrypt32SPAligned(
          unsigned char        *pData,
          unsigned int        uiDataLength,
          unsigned char        *pDecryptIR
          )
{
    register xuint32* pDataPtr;
    register xuint32  dwBuf;
#ifdef __LITTLE_ENDIAN_BYTEORDER        
    register xuint32 dwDecryptIR = (pDecryptIR[3]) + (pDecryptIR[0]<<8) + (pDecryptIR[1]<<16) + (pDecryptIR[2]<<24);
#else
    register xuint32 dwDecryptIR = (pDecryptIR[3]<<24) + (pDecryptIR[0]<<16) + (pDecryptIR[1]<<8) + (pDecryptIR[2]);
#endif
    register xuint32* toPtr;

    toPtr = (xuint32*)(pData + uiDataLength);
    for(pDataPtr = (xuint32*)pData; pDataPtr < toPtr; pDataPtr++) {
        dwBuf = dwDecryptIR ^ (*pDataPtr);
#ifdef __LITTLE_ENDIAN_BYTEORDER
        *pDataPtr = ROTR(dwBuf, 8);
#else
        *pDataPtr = ROTL(dwBuf, 8);
#endif
    }
}

void
Decrypt32SP(
          unsigned char        *pData,
          unsigned int        uiDataLength,
          unsigned char        *pDecryptIR
          )
{
    unsigned int    i;
    register char    temp0;
    register char    temp1;
    register char    temp2;
    register char    temp3;
    unsigned char    *pDataPtr;
#if !defined(DISABLE_ALIGN_OPTIMIZATION)    
    if ((long)pData%4==0) { // We can use faster version for aligned input
        Decrypt32SPAligned(pData, uiDataLength, pDecryptIR);
        return;
    }
#endif

    for(i = 0; i < uiDataLength / 4; i++) {
        pDataPtr = &pData[i * 4];

        temp3 = pDecryptIR[3] ^ pDataPtr[0];
        temp0 = pDecryptIR[0] ^ pDataPtr[1];
        temp1 = pDecryptIR[1] ^ pDataPtr[2];
        temp2 = pDecryptIR[2] ^ pDataPtr[3];

        pDataPtr[0] = temp0;
        pDataPtr[1] = temp1;
        pDataPtr[2] = temp2;
        pDataPtr[3] = temp3;
    }
}

void
Decrypt32SPAndCopyAligned(
            unsigned char        *desData,
            unsigned char        *srcData,
            unsigned int        uiDataLength,
            unsigned char        *pDecryptIR
            )
{
    register xuint32* pDataPtr;
    register xuint32  dwBuf;
#ifdef __LITTLE_ENDIAN_BYTEORDER    
    register xuint32 dwDecryptIR = (pDecryptIR[3]) + (pDecryptIR[0]<<8) + (pDecryptIR[1]<<16) + (pDecryptIR[2]<<24);
#else
    register xuint32 dwDecryptIR = (pDecryptIR[3]<<24) + (pDecryptIR[0]<<16) + (pDecryptIR[1]<<8) + (pDecryptIR[2]);
#endif
    register xuint32* toPtr;
    register xuint32* desPtr;
    toPtr = (xuint32*)(srcData + uiDataLength);

    for(pDataPtr = (xuint32*)srcData, desPtr = (xuint32*)desData; pDataPtr < toPtr; pDataPtr++, desPtr++) {
        dwBuf = dwDecryptIR ^ (*pDataPtr);
#ifdef __LITTLE_ENDIAN_BYTEORDER    
        *desPtr = ROTR(dwBuf, 8);
#else
        *desPtr = ROTL(dwBuf, 8);
#endif
    }
}

void
Decrypt32SPAndCopySrcUnaligned(
            unsigned char        *desData,
            unsigned char        *srcData,
            unsigned int        uiDataLength,
            unsigned char        *pDecryptIR
            )
{
    register xuchar * pDataPtr;
    register xuint32  dwBuf;
#ifdef __LITTLE_ENDIAN_BYTEORDER
    register xuint32 dwDecryptIR = (pDecryptIR[3]) + (pDecryptIR[0]<<8) + (pDecryptIR[1]<<16) + (pDecryptIR[2]<<24);
#else
    register xuint32 dwDecryptIR = (pDecryptIR[3]<<24) + (pDecryptIR[0]<<16) + (pDecryptIR[1]<<8) + (pDecryptIR[2]);
#endif
    register xuchar* toPtr;
    register xuint32* desPtr;
    
    toPtr = srcData + uiDataLength;
    for(pDataPtr = srcData, desPtr = (xuint32*)desData; pDataPtr < toPtr; pDataPtr+=4, desPtr++) {
#ifdef __LITTLE_ENDIAN_BYTEORDER
        dwBuf = dwDecryptIR ^ (pDataPtr[0] + (pDataPtr[1]<<8) + (pDataPtr[2]<<16) + (pDataPtr[3]<<24));
        *desPtr = ROTR(dwBuf, 8);
#elif __BIG_ENDIAN_BYTEORDER
        dwBuf = dwDecryptIR ^ ((pDataPtr[0]<<24) + (pDataPtr[1]<<16) + (pDataPtr[2]<<8) + pDataPtr[3]);
        *desPtr = ROTL(dwBuf, 8);
#endif
    }
}

void
Decrypt32SPAndCopy(
            unsigned char        *desData,
            unsigned char        *srcData,
            unsigned int        uiDataLength,
            unsigned char        *pDecryptIR
            )
{
    unsigned int    i;
    register char    temp0;
    register char    temp1;
    register char    temp2;
    register char    temp3;
    unsigned char    *pDataPtr;
    unsigned char    *pDesDataPtr;
    sal_assert(uiDataLength%4==0);

#if !defined(DISABLE_ALIGN_OPTIMIZATION)
    if ((long)srcData%4==2 && (long)desData%4==0) { // Unaligned source data
        Decrypt32SPAndCopySrcUnaligned(desData, srcData, uiDataLength, pDecryptIR);
        return;
    }
    if ((long)srcData%4==0 && (long)desData%4==0) { // We can use faster version for aligned input
        Decrypt32SPAndCopyAligned(desData, srcData, uiDataLength, pDecryptIR);
        return;
    }
#endif

    for(i = 0; i < uiDataLength / 4; i++) {
        pDataPtr = &srcData[i * 4];
        pDesDataPtr = &desData[i * 4];
        
        temp3 = pDecryptIR[3] ^ pDataPtr[0];
        temp0 = pDecryptIR[0] ^ pDataPtr[1];
        temp1 = pDecryptIR[1] ^ pDataPtr[2];
        temp2 = pDecryptIR[2] ^ pDataPtr[3];

        pDesDataPtr[0] = temp0;
        pDesDataPtr[1] = temp1;
        pDesDataPtr[2] = temp2;
        pDesDataPtr[3] = temp3;
    }
}
#endif

