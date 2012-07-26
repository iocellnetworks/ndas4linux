#include "lpx/lpx.h"
#include "lpx/lpxutil.h"
#include "sal/sal.h"
#include "sal/libc.h"
#include "sal/debug.h"
#include "sal/mem.h"
#include "../netdisk/hash.h"
#include "../netdisk/lanscsi.h"
#include "../netdisk/binaryparameters.h"
#include "../netdisk/hdreg.h"

#define perror(x) sal_debug_print("Error-" x "\n")

#define    NR_MAX_TARGET            2
#define    MAX_DATA_BUFFER_SIZE    64 * 1024
#define BLOCK_SIZE                512
#define    MAX_TRANSFER_SIZE        (64 * 1024)
#define MAX_TRANSFER_BLOCKS         MAX_TRANSFER_SIZE / BLOCK_SIZE

#define SEC            (LONGLONG)(1000)
#define TIME_OUT    (SEC * 30)                    /* 5 min. */


#define MAKE_UINT64_FROM_CHARS(x)     ((((x)[0] & 0x0ffULL)<<56) | (((x)[1] & 0x0ffULL)<<48) | (((x)[2] & 0x0ffULL)<<40) | (((x)[3] & 0x0ffULL)<<32) \
                        | (((x)[4] & 0x0ffULL)<<24) | (((x)[5] & 0x0ffULL)<<16) | (((x)[6] & 0x0ffULL)<<8) | (((x)[7] & 0x0ffULL))) 
/* Make 8 byte char array(in network byte order) from xuint64 type number */
static inline void MAKE_CHARS_FROM_UINT64(char* c, xuint64 ui) 
{
    c[0] = (ui&0xFF00000000000000)>>56;
    c[1] = (ui&0x00FF000000000000)>>48;
    c[2] = (ui&0x0000FF0000000000)>>40;
    c[3] = (ui&0x000000FF00000000)>>32;
    c[4] = (ui&0x00000000FF000000)>>24;
    c[5] = (ui&0x0000000000FF0000)>>16;
    c[6] = (ui&0x000000000000FF00)>>8;
    c[7] = (ui&0x00000000000000FF);
}  

#ifdef __BIG_ENDIAN_BYTEORDER
#define HTONLL(Data)    (Data)
#define NTOHLL(Data)    (Data)
#elif __LITTLE_ENDIAN_BYTEORDER
#define HTONLL(Data)    ( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
                        | (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
                        | (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
                        | (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#define NTOHLL(Data)    ( (((Data)&0x00000000000000FF) << 56) | (((Data)&0x000000000000FF00) << 40) \
                        | (((Data)&0x0000000000FF0000) << 24) | (((Data)&0x00000000FF000000) << 8)  \
                        | (((Data)&0x000000FF00000000) >> 8)  | (((Data)&0x0000FF0000000000) >> 24) \
                        | (((Data)&0x00FF000000000000) >> 40) | (((Data)&0xFF00000000000000) >> 56))

#else
#error "Endian is not specified"
#endif

typedef    struct _TARGET_DATA {
    xbool    bPresent;
    xint8    NRRWHost;
    xint8    NRROHost;
    xint32    TargetDataHi;
    xint32    TargetDataLow;
    
/*    // IDE Info.*/
    xbool            bLBA;
    xbool            bLBA48;
    unsigned xint64    SectorCount;
} TARGET_DATA, *PTARGET_DATA;

/* // Global Variable.*/
xint32            HPID;
xint16            RPID;
xint32            iTag;
int                NRTarget;
char            CHAP_C[4];
unsigned        requestBlocks;
TARGET_DATA        PerTarget[NR_MAX_TARGET];
unsigned xint16    HeaderEncryptAlgo;
unsigned xint16    DataEncryptAlgo;
int                iSessionPhase;
char    iUserPassword[] = HASH_KEY_USER;
char    iSuperPassword[] = HASH_KEY_SUPER;

#define PrintError(format, args...) sal_debug_print( args)
#define WSAGetLastError()
#define SOCKET int

#ifndef isascii
#define in_range(c, lo, up)  ((char)c >= lo && (char)c <= up)
#define isascii(c)           in_range(c, 0x20, 0x7f)
#define isdigit(c)           in_range(c, '0', '9')
#define isxdigit(c)          (isdigit(c) || in_range(c, 'a', 'f') || in_range(c, 'A', 'F'))
#define islower(c)           in_range(c, 'a', 'z')
#define isspace(c)           (c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v')
#endif        

int LanScsiCli(char* mac, int target, int opt);
void* lanscsiclimain(void* param)
{
    LanScsiCli((char*)param,0,0);
    return 0;
}

void lanscsi_dump(char* buf, int size)
{
        int i;
        for(i=0;i<size;i++) {
            sal_debug_print("%02x", (unsigned int)(buf[i]&0x0ff));
        }    
        sal_debug_print("\n");
}

int 
CliRecvIt(
       SOCKET    sock,
       char*    buf, 
       int        size
       )
{
    int res;
    int len = size;

/*    sal_debug_print("CliRecvIt size=%d\n", size); */
    while (len > 0) {
        if ((res = lpx_recv(sock, buf, len,0)) <= 0) {
            sal_debug_print("CliRecvIt failed\n");
            return res;    
        }
        len -= res;
        buf += res;
    }
    return size;
}

int 
CliSendIt(
       SOCKET    sock,
       char*    buf, 
       int        size
       )
{
    int res;
    int len = size;

    while (len > 0) {
        if ((res = lpx_send(sock, buf, len, 0)) <= 0) {
            sal_debug_print("CliSendIt Failed\n");
            return res;
        }
        len -= res;
        buf += res;
    }
    
    return size;
}

int
CliReadReply(
            SOCKET            connSock,
            char*            pBuffer,
            PLANSCSI_PDU_POINTERS    pPdu
            )
{
    int        iResult, iTotalRecved = 0;
    char*    pPtr = pBuffer;

/*    // Read Header.*/
    iResult = CliRecvIt(
        connSock,
        pPtr,
        sizeof(LANSCSI_H2R_PDU_HEADER)
        );
    
    if(iResult < 0) {
        sal_debug_print( "ReadRequest: Can't Recv Header...\n");

        return iResult;
    } else if(iResult == 0) {
        sal_debug_print( "ReadRequest: Disconnected...\n");
        
        return iResult;
    } else
        iTotalRecved += iResult;

    pPdu->u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pPtr;

    pPtr += sizeof(LANSCSI_H2R_PDU_HEADER);

    if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
        && HeaderEncryptAlgo != 0) {
        Decrypt32(
            (unsigned char*)pPdu->u.pH2RHeader,
            sizeof(LANSCSI_H2R_PDU_HEADER),
            (unsigned char *)&CHAP_C,
            (unsigned char *)&iUserPassword
            );
/*        sal_debug_print("Read head(decryped):\n");
        lanscsi_dump((unsigned char*)pPdu->u.pH2RHeader, sizeof(LANSCSI_H2R_PDU_HEADER));*/
    } else {
/*        sal_debug_print("Read head(not decryped):\n");
        lanscsi_dump((unsigned char*)pPdu->u.pH2RHeader, sizeof(LANSCSI_H2R_PDU_HEADER));        */
    }
    
/*    // Read AHS.*/
    if(ntohs(pPdu->u.pH2RHeader->AHSLen) > 0) {
        iResult = CliRecvIt(
            connSock,
            pPtr,
            ntohs(pPdu->u.pH2RHeader->AHSLen)
            );
        if(iResult < 0) {
            sal_debug_print( "ReadRequest: Can't Recv AHS...\n");

            return iResult;
        } else if(iResult == 0) {
            sal_debug_print( "ReadRequest: Disconnected...\n");

            return iResult;
        } else
            iTotalRecved += iResult;
    
        pPdu->pAHS = pPtr;

        pPtr += ntohs(pPdu->u.pH2RHeader->AHSLen);
    }

/*    // Read Header Dig.*/
    pPdu->pHeaderDig = NULL;

/*    // Read Data segment.*/
    if(ntohl(pPdu->u.pH2RHeader->DataSegLen) > 0) {
        iResult = CliRecvIt(
            connSock,
            pPtr,
            ntohl(pPdu->u.pH2RHeader->DataSegLen)
            );
        if(iResult < 0) {
            sal_debug_print( "ReadRequest: Can't Recv Data segment...\n");

            return iResult;
        } else if(iResult == 0) {
            sal_debug_print( "ReadRequest: Disconnected...\n");

            return iResult;
        } else 
            iTotalRecved += iResult;
        
        pPdu->pDataSeg = pPtr;
        
        pPtr += ntohl(pPdu->u.pH2RHeader->DataSegLen);
        
        if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
            && DataEncryptAlgo != 0) {
            
            Decrypt32(
                (unsigned char*)pPdu->pDataSeg,
                ntohl(pPdu->u.pH2RHeader->DataSegLen),
                (unsigned char *)&CHAP_C,
                (unsigned char *)&iUserPassword
                );
        }
/*        lanscsi_dump((unsigned char*)pPdu->pDataSeg,ntohl(pPdu->u.pH2RHeader->DataSegLen)); */
    }
/*    // Read Data Dig.*/
    pPdu->pDataDig = NULL;
    
    return iTotalRecved;
}

int
CliSendRequest(
            SOCKET            connSock,
            PLANSCSI_PDU_POINTERS    pPdu
            )
{
    PLANSCSI_H2R_PDU_HEADER pHeader;
    int                        iDataSegLen, iResult;

    pHeader = pPdu->u.pH2RHeader;
    iDataSegLen = ntohl(pHeader->DataSegLen);

    
/*    // Encrypt Header.*/
    
    if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
        && HeaderEncryptAlgo != 0) {
        
        Encrypt32(
            (unsigned char*)pHeader,
            sizeof(LANSCSI_H2R_PDU_HEADER),
            (unsigned char *)&CHAP_C,
            (unsigned char*)&iUserPassword
            );
    }
    
    
    /* // Encrypt Header.*/
    
    if(iSessionPhase == FLAG_FULL_FEATURE_PHASE
        && DataEncryptAlgo != 0
        && iDataSegLen > 0) {
        
        Encrypt32(
            (unsigned char*)pPdu->pDataSeg,
            iDataSegLen,
            (unsigned char *)&CHAP_C,
            (unsigned char*)&iUserPassword
            );
    }
/*    sal_debug_print("CliSendRequest: %d %d\n", sizeof(LANSCSI_H2R_PDU_HEADER) , iDataSegLen);*/
/*    // Send Request.*/
    iResult = CliSendIt(
        connSock,
        (char*)pHeader,
        sizeof(LANSCSI_H2R_PDU_HEADER) + iDataSegLen
        );
    if(iResult < 0) {
        PrintError(WSAGetLastError(), "CliSendRequest: Send Request ");
        return -1;
    }
    
    return 0;
}

xint8 CliLogin_buffer[MAX_REQUEST_SIZE];
int
CliLogin(
      SOCKET            connsock,
      xuchar                cLoginType,
      xint32            iUserID,
      xuchar*    iKey     /* 8 byte length key */
      )
{
    xint8                                *PduBuffer=CliLogin_buffer;
    PLANSCSI_LOGIN_REQUEST_PDU_HEADER    pLoginRequestPdu;
    PLANSCSI_LOGIN_REPLY_PDU_HEADER        pLoginReplyHeader;
    PBIN_PARAM_SECURITY                    pParamSecu;
    PBIN_PARAM_NEGOTIATION                pParamNego;
    PAUTH_PARAMETER_CHAP                pParamChap;
    LANSCSI_PDU_POINTERS                pdu;
    int                                    iSubSequence;
    int                                    iResult;
    unsigned int                         CHAP_I;
    
/*    // Init.*/
    iSubSequence = 0;
    iSessionPhase = FLAG_SECURITY_PHASE;

/*    // First Packet.*/
    sal_memset(PduBuffer, 0, MAX_REQUEST_SIZE);
    
    pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
    
    pLoginRequestPdu->Opcode = LOGIN_REQUEST;
    pLoginRequestPdu->HPID = htonl(HPID);
    pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST);
    pLoginRequestPdu->CSubPacketSeq = htons(iSubSequence);
    pLoginRequestPdu->PathCommandTag = htonl(iTag);
    pLoginRequestPdu->ParameterType = 1;
    pLoginRequestPdu->ParameterVer = 0;
    pLoginRequestPdu->VerMax = 0;
    pLoginRequestPdu->VerMin = 0;
    
    pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
    
    pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
    pParamSecu->LoginType = cLoginType;
    pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
    
/*    // Send Request.*/
    pdu.u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
    pdu.pDataSeg = (char *)pParamSecu;

    if(CliSendRequest(connsock, &pdu) != 0) {
        PrintError(WSAGetLastError(), "Login: Send First Request ");
        return -1;
    }
/*    // Read Request.*/
    iResult = CliReadReply(connsock, (char*)PduBuffer, &pdu);
    if(iResult < 0) {
        sal_debug_print( "[LanScsiCli]login: First Can't Read Reply.\n");
        return -1;
    }
/*    // Check Request Header.*/
    pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.u.pR2HHeader;
    if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
        || (pLoginReplyHeader->u.s.T != 0)
        || (pLoginReplyHeader->u.s.CSG != FLAG_SECURITY_PHASE)
        || (pLoginReplyHeader->u.s.NSG != FLAG_SECURITY_PHASE)
        || (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
        || (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
        || (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
        
        sal_debug_print( "[LanScsiCli]login: BAD First Reply Header.\n");
        return -1;
    }
    
    if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        sal_debug_print( "[LanScsiCli]login: First Failed.\n");
        return -1;
    }
    
/*    // Store Data.*/
    RPID = ntohs(pLoginReplyHeader->RPID);
    
    pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
    sal_debug_print("[LanScsiCli]login: Version %d Auth %d\n", 
        pLoginReplyHeader->VerActive, 
        ntohs(pParamSecu->AuthMethod)
        );
    
/*    // Second Packet.*/

    sal_memset(PduBuffer, 0, MAX_REQUEST_SIZE);
    
    pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
    
    pLoginRequestPdu->Opcode = LOGIN_REQUEST;
    pLoginRequestPdu->HPID = htonl(HPID);
    pLoginRequestPdu->RPID = htons(RPID);
    pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST);
    ++iSubSequence;
    pLoginRequestPdu->CSubPacketSeq = htons(iSubSequence);
    pLoginRequestPdu->PathCommandTag = htonl(iTag);
    pLoginRequestPdu->ParameterType = 1;
    pLoginRequestPdu->ParameterVer = 0;
    
    pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
    
    pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
    pParamSecu->LoginType = cLoginType;
    pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
    
    pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->u.AuthParameter;
    pParamChap->CHAP_A = ntohl(HASH_ALGORITHM_MD5);
    
/*    // Send Request.*/
    pdu.u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
    pdu.pDataSeg = (char *)pParamSecu;

    if(CliSendRequest(connsock, &pdu) != 0) {
        PrintError(WSAGetLastError(), "[LanScsiCli]Login: Send Second Request ");
        return -1;
    }

/*    // Read Request.*/
    iResult = CliReadReply(connsock, (char*)PduBuffer, &pdu);
    if(iResult < 0) {
        sal_debug_print( "[LanScsiCli]login: Second Can't Read Reply.\n");
        return -1;
    }
    
/*    // Check Request Header.*/
    pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.u.pR2HHeader;
    if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
        || (pLoginReplyHeader->u.s.T != 0)
        || (pLoginReplyHeader->u.s.CSG != FLAG_SECURITY_PHASE)
        || (pLoginReplyHeader->u.s.NSG != FLAG_SECURITY_PHASE)
        || (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
        || (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
        || (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
        
        sal_debug_print( "[LanScsiCli]login: BAD Second Reply Header.\n");
        return -1;
    }
    
    if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        sal_debug_print( "[LanScsiCli]login: Second Failed.\n");
        return -1;
    }
    
/*    // Check Data segment.*/
    if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)    /* Minus AuthParamter[1] */
        || (pdu.pDataSeg == NULL)) {
        
        sal_debug_print( "[LanScsiCli]login: BAD Second Reply Data.\n");
        return -1;
    }    
    pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
    if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
        || pParamSecu->LoginType != cLoginType
        || pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {
        
        sal_debug_print( "[LanScsiCli]login: BAD Second Reply Parameters.\n");
        return -1;
    }
    
/*    // Store Challenge.    */
    pParamChap = &pParamSecu->u.ChapParam;
    CHAP_I = ntohl(pParamChap->CHAP_I);
    sal_memcpy(CHAP_C, pParamChap->CHAP_C, 4);  /* endian fix */
    
    sal_debug_print("[LanScsiCli]login: Hash %d, Challenge %x:%x:%x:%x\n", 
        pParamChap->CHAP_A,
        CHAP_C[0], CHAP_C[1], CHAP_C[2], CHAP_C[3]
        );

    
/*    // Third Packet.*/
    
    sal_memset(PduBuffer, 0, MAX_REQUEST_SIZE);
    
    pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
    pLoginRequestPdu->Opcode = LOGIN_REQUEST;
    pLoginRequestPdu->u.s.T = 1;
    pLoginRequestPdu->u.s.CSG = FLAG_SECURITY_PHASE;
    pLoginRequestPdu->u.s.NSG = FLAG_LOGIN_OPERATION_PHASE;
    pLoginRequestPdu->HPID = htonl(HPID);
    pLoginRequestPdu->RPID = htons(RPID);
    pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST);
    ++iSubSequence;
    pLoginRequestPdu->CSubPacketSeq = htons(iSubSequence);
    pLoginRequestPdu->PathCommandTag = htonl(iTag);
    pLoginRequestPdu->ParameterType = 1;
    pLoginRequestPdu->ParameterVer = 0;
    
    pParamSecu = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
    
    pParamSecu->ParamType = BIN_PARAM_TYPE_SECURITY;
    pParamSecu->LoginType = cLoginType;
    pParamSecu->AuthMethod = htons(AUTH_METHOD_CHAP);
    
    pParamChap = (PAUTH_PARAMETER_CHAP)pParamSecu->u.AuthParameter;
    pParamChap->CHAP_A = htonl(HASH_ALGORITHM_MD5);
    pParamChap->CHAP_I = htonl(CHAP_I);
    pParamChap->CHAP_N = htonl(iUserID);
    
    Hash32To128((unsigned char*)CHAP_C, (unsigned char*)pParamChap->CHAP_R, iKey);
    
    
/*    // Send Request.*/
    pdu.u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
    pdu.pDataSeg = (char *)pParamSecu;

    if(CliSendRequest(connsock, &pdu) != 0) {
        PrintError(WSAGetLastError(), "Login: Send Third Request ");
        return -1;
    }
    
    /* // Read Request.*/
    iResult = CliReadReply(connsock, (char*)PduBuffer, &pdu);
    if(iResult < 0) {
        sal_debug_print( "[LanScsiCli]login: Third Can't Read Reply.\n");
        return -1;
    }
    
/*    // Check Request Header.*/
    pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.u.pR2HHeader;
    if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
        || (pLoginReplyHeader->u.s.T == 0)
        || (pLoginReplyHeader->u.s.CSG != FLAG_SECURITY_PHASE)
        || (pLoginReplyHeader->u.s.NSG != FLAG_LOGIN_OPERATION_PHASE)
        || (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
        || (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
        || (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
        
        sal_debug_print( "[LanScsiCli]login: BAD Third Reply Header.\n");
        return -1;
    }
    
    if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        sal_debug_print( "[LanScsiCli]login: Third Failed.\n");
        return -1;
    }
    
/*    // Check Data segment.*/
    if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)    /* Minus AuthParamter[1]*/
        || (pdu.pDataSeg == NULL)) {
        
        sal_debug_print( "[LanScsiCli]login: BAD Third Reply Data.\n");
        return -1;
    }    
    pParamSecu = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
    if(pParamSecu->ParamType != BIN_PARAM_TYPE_SECURITY
        || pParamSecu->LoginType != cLoginType
        || pParamSecu->AuthMethod != htons(AUTH_METHOD_CHAP)) {
        
        sal_debug_print( "[LanScsiCli]login: BAD Third Reply Parameters.\n");
        return -1;
    }
    
    iSessionPhase = FLAG_LOGIN_OPERATION_PHASE;

/*    // Fourth Packet.*/
    sal_memset(PduBuffer, 0, MAX_REQUEST_SIZE);
    
    pLoginRequestPdu = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)PduBuffer;
    pLoginRequestPdu->Opcode = LOGIN_REQUEST;
    pLoginRequestPdu->u.s.T = 1;
    pLoginRequestPdu->u.s.CSG = FLAG_LOGIN_OPERATION_PHASE;
    pLoginRequestPdu->u.s.NSG = FLAG_FULL_FEATURE_PHASE;
    pLoginRequestPdu->HPID = htonl(HPID);
    pLoginRequestPdu->RPID = htons(RPID);
    pLoginRequestPdu->DataSegLen = htonl(BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST);
    ++iSubSequence;
    pLoginRequestPdu->CSubPacketSeq = htons(iSubSequence);
    pLoginRequestPdu->PathCommandTag = htonl(iTag);
    pLoginRequestPdu->ParameterType = 1;
    pLoginRequestPdu->ParameterVer = 0;
    
    pParamNego = (PBIN_PARAM_NEGOTIATION)&PduBuffer[sizeof(LANSCSI_LOGIN_REQUEST_PDU_HEADER)];
    
    pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;
    
/*    // Send Request.*/
    pdu.u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pLoginRequestPdu;
    pdu.pDataSeg = (char *)pParamNego;

    if(CliSendRequest(connsock, &pdu) != 0) {
        PrintError(WSAGetLastError(), "Login: Send Fourth Request ");
        return -1;
    }
    
/*    // Read Request.*/
    iResult = CliReadReply(connsock, (char*)PduBuffer, &pdu);
    if(iResult < 0) {
        sal_debug_print( "[LanScsiCli]login: Fourth Can't Read Reply.\n");
        return -1;
    }
    
/*    // Check Request Header.*/
    pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)pdu.u.pR2HHeader;
    if((pLoginReplyHeader->Opcode != LOGIN_RESPONSE)
        || (pLoginReplyHeader->u.s.T == 0)
        || ((pLoginReplyHeader->u.Flags & LOGIN_FLAG_CSG_MASK) != (FLAG_LOGIN_OPERATION_PHASE << 2))
        || ((pLoginReplyHeader->u.Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)
        || (pLoginReplyHeader->VerActive > LANSCSI_CURRENT_VERSION)
        || (pLoginReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
        || (pLoginReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
        
        sal_debug_print( "[LanScsiCli]login: BAD Fourth Reply Header.\n");
        return -1;
    }
    
    if(pLoginReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        sal_debug_print( "[LanScsiCli]login: Fourth Failed.\n");
        return -1;
    }
    
/*    // Check Data segment.*/
    if((ntohl(pLoginReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY)    /* Minus AuthParamter[1] */
        || (pdu.pDataSeg == NULL)) {
        
        sal_debug_print( "[LanScsiCli]login: BAD Fourth Reply Data.\n");
        return -1;
    }    
    pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
    if(pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION) {
        sal_debug_print( "[LanScsiCli]login: BAD Fourth Reply Parameters.\n");
        return -1;
    }
    
    sal_debug_print("[LanScsiCli]login: Hw Type %d, Hw Version %d, NRSlots %d, W %d, MT %d ML %d\n", 
        pParamNego->HWType, pParamNego->HWVersion,
        ntohl(pParamNego->NRSlot), ntohl(pParamNego->MaxBlocks),
        ntohl(pParamNego->MaxTargetID), ntohl(pParamNego->MaxLUNID)
        );
    sal_debug_print("[LanScsiCli]login: Head Encrypt Algo %d, Head Digest Algo %d, Data Encrypt Algo %d, Data Digest Algo %d\n",
        ntohs(pParamNego->HeaderEncryptAlgo),
        ntohs(pParamNego->HeaderDigestAlgo),
        ntohs(pParamNego->DataEncryptAlgo),
        ntohs(pParamNego->DataDigestAlgo)
        );

    requestBlocks = ntohl(pParamNego->MaxBlocks);
    HeaderEncryptAlgo = ntohs(pParamNego->HeaderEncryptAlgo);
    DataEncryptAlgo = ntohs(pParamNego->DataEncryptAlgo);

    iSessionPhase = FLAG_FULL_FEATURE_PHASE;

    return 0;
}

static xint8 CliTextTargetList_pduBuffer[MAX_REQUEST_SIZE];
int
CliTextTargetList(
               SOCKET    connsock
               )
{
    xint8                                *PduBuffer = CliTextTargetList_pduBuffer;
    PLANSCSI_TEXT_REQUEST_PDU_HEADER    pRequestHeader;
    PLANSCSI_TEXT_REPLY_PDU_HEADER        pReplyHeader;
    PBIN_PARAM_TARGET_LIST                pParam;
    LANSCSI_PDU_POINTERS                pdu;
    int                    iResult, i;
    
    sal_memset(PduBuffer, 0, MAX_REQUEST_SIZE);
    
    pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
    pRequestHeader->Opcode = TEXT_REQUEST;
    pRequestHeader->u.s.F = 1;
    pRequestHeader->HPID = htonl(HPID);
    pRequestHeader->RPID = htons(RPID);
    pRequestHeader->CPSlot = 0;
    pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST);
    pRequestHeader->AHSLen = 0;
    pRequestHeader->CSubPacketSeq = 0;
    ++iTag;
    pRequestHeader->PathCommandTag = htonl(iTag);
    pRequestHeader->ParameterType = PARAMETER_TYPE_BINARY;
    pRequestHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
    
/*    // Make Parameter.*/
    pParam = (PBIN_PARAM_TARGET_LIST)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
    pParam->ParamType = BIN_PARAM_TYPE_TARGET_LIST;
    
/*    // Send Request.*/
    pdu.u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
    pdu.pDataSeg = (char *)pParam;

    if(CliSendRequest(connsock, &pdu) != 0) {
        PrintError(WSAGetLastError(), "CliTextTargetList: Send First Request ");
        return -1;
    }
    
/*    // Read Request.*/
    iResult = CliReadReply(connsock, (char*)PduBuffer, &pdu);
#ifdef __WIN32__
    if(iResult == SOCKET_ERROR) {
#else
    if(iResult < 0) {
#endif
        sal_debug_print( "[LanScsiCli]CliTextTargetList: Can't Read Reply.\n");
        return -1;
    }
    
    pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.u.pR2HHeader;

/*    // Check Request Header.*/
    if((pReplyHeader->Opcode != TEXT_RESPONSE)
        || (pReplyHeader->u.s.F == 0)
        || (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
        || (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
        
        sal_debug_print( "[LanScsiCli]CliTextTargetList: BAD Reply Header.\n");
        return -1;
    }
    
    if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        sal_debug_print( "[LanScsiCli]CliTextTargetList: Failed.\n");
        return -1;
    }
    
    if(ntohl(pReplyHeader->DataSegLen) < BIN_PARAM_SIZE_REPLY) {
        sal_debug_print( "[LanScsiCli]CliTextTargetList: No Data Segment.\n");
        return -1;        
    }


    pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
    if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_LIST) {
        sal_debug_print( "TEXT: Bad Parameter Type.\n");
        return -1;            
    }
    sal_debug_print("[LanScsiCli]CliTextTargetList: NR Targets : %d\n", pParam->NRTarget);
    NRTarget = pParam->NRTarget;
    
    for(i = 0; i < pParam->NRTarget; i++) {
        PBIN_PARAM_TARGET_LIST_ELEMENT    pTarget;
        int                                iTargetId;
        
        pTarget = &pParam->PerTarget[i];
        iTargetId = ntohl(pTarget->TargetID);

        sal_debug_print("pTarget:\n");
        lanscsi_dump((char*)pTarget, sizeof(BIN_PARAM_TARGET_LIST));


        sal_debug_print("[LanScsiCli]CliTextTargetList: Target ID: %d, NR_RW: %d, NR_RO: %d, Data: 0x%08x%08x \n",  
            ntohl(pTarget->TargetID), 
            pTarget->NRRWHost,
            pTarget->NRROHost,
            pTarget->TargetDataHi,
            pTarget->TargetDataLow
            );
        PerTarget[iTargetId].bPresent = TRUE;
        PerTarget[iTargetId].NRRWHost = pTarget->NRRWHost;
        PerTarget[iTargetId].NRROHost = pTarget->NRROHost;
        PerTarget[iTargetId].TargetDataHi = pTarget->TargetDataHi;
        PerTarget[iTargetId].TargetDataLow = pTarget->TargetDataLow;

    }
    
    return 0;
}

static xint8 CliTextTargetData_buffer[MAX_REQUEST_SIZE];
int
CliTextTargetData(
               SOCKET    connsock,
               xuchar    cGetorSet,
               xuint32        TargetID
               )
{
    xint8                                *PduBuffer = CliTextTargetData_buffer;
    PLANSCSI_TEXT_REQUEST_PDU_HEADER    pRequestHeader;
    PLANSCSI_TEXT_REPLY_PDU_HEADER        pReplyHeader;
    PBIN_PARAM_TARGET_DATA                pParam;
    LANSCSI_PDU_POINTERS                pdu;
    int                                    iResult;
    
    sal_memset(PduBuffer, 0, MAX_REQUEST_SIZE);
    
    pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;
    pRequestHeader->Opcode = TEXT_REQUEST;
    pRequestHeader->u.s.F = 1;
    pRequestHeader->HPID = htonl(HPID);
    pRequestHeader->RPID = htons(RPID);
    pRequestHeader->CPSlot = 0;
    pRequestHeader->DataSegLen = htonl(BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST);
    pRequestHeader->AHSLen = 0;
    pRequestHeader->CSubPacketSeq = 0;
    ++iTag;
    pRequestHeader->PathCommandTag = htonl(iTag);
    pRequestHeader->ParameterType = PARAMETER_TYPE_BINARY;
    pRequestHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
    
/*    // Make Parameter.*/
    pParam = (PBIN_PARAM_TARGET_DATA)&PduBuffer[sizeof(LANSCSI_H2R_PDU_HEADER)];
    pParam->ParamType = BIN_PARAM_TYPE_TARGET_DATA;
    pParam->GetOrSet = cGetorSet;
    pParam->TargetDataHi = PerTarget[TargetID].TargetDataHi;
    pParam->TargetDataLow = PerTarget[TargetID].TargetDataLow;

    pParam->TargetID = htonl(TargetID);
    sal_debug_print("TargetID %d, 0x%08x%08x\n", ntohl(pParam->TargetID), pParam->TargetDataHi,pParam->TargetDataLow);

/*    // Send Request.*/
    pdu.u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
    pdu.pDataSeg = (char *)pParam;

    if(CliSendRequest(connsock, &pdu) != 0) {
        PrintError(WSAGetLastError(), "CliTextTargetData: Send First Request ");
        return -1;
    }
    
/*    // Read Request.*/
    iResult = CliReadReply(connsock, (char*)PduBuffer, &pdu);
    if(iResult < 0) {
        sal_debug_print( "[LanScsiCli]CliTextTargetData: Can't Read Reply.\n");
        return -1;
    }
    
/*    // Check Request Header.*/
    pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)pdu.u.pR2HHeader;


    if((pReplyHeader->Opcode != TEXT_RESPONSE)
        || (pReplyHeader->u.s.F == 0)
        || (pReplyHeader->ParameterType != PARAMETER_TYPE_BINARY)
        || (pReplyHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
        
        sal_debug_print( "[LanScsiCli]CliTextTargetData: BAD Reply Header.\n");
        return -1;
    }
    
    if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        sal_debug_print( "[LanScsiCli]CliTextTargetData: Failed.\n");
        return -1;
    }
    
    if(pReplyHeader->DataSegLen < BIN_PARAM_SIZE_REPLY) {
        sal_debug_print( "[LanScsiCli]CliTextTargetData: No Data Segment.\n");
        return -1;        
    }
    pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;

    if(pParam->ParamType != BIN_PARAM_TYPE_TARGET_DATA) {
        sal_debug_print( "CliTextTargetData: Bad Parameter Type. %d\n", pParam->ParamType);
/*    //    return -1;            */
    }

    PerTarget[TargetID].TargetDataHi = pParam->TargetDataHi;
    PerTarget[TargetID].TargetDataLow = pParam->TargetDataLow;

    sal_debug_print("[LanScsiCli]CliTextTargetList: TargetID : %d, GetorSet %d, Target Data 0x%08x%08x\n", 
        ntohl(pParam->TargetID), pParam->GetOrSet, PerTarget[TargetID].TargetDataHi, PerTarget[TargetID].TargetDataLow);
    
    return 0;
}

xint8 CliVenderCommand_buffer[MAX_REQUEST_SIZE];
int
CliVenderCommand(
              SOCKET            connsock,
              xuchar            cOperation,
              char*                pParameter
              )
{
    xint8                                *PduBuffer = CliVenderCommand_buffer;
    PLANSCSI_VENDOR_REQUEST_PDU_HEADER    pRequestHeader;
    PLANSCSI_VENDOR_REPLY_PDU_HEADER    pReplyHeader;
    LANSCSI_PDU_POINTERS                pdu;
    int                                    iResult;
    
    sal_memset(PduBuffer, 0, MAX_REQUEST_SIZE);
    
    pRequestHeader = (PLANSCSI_VENDOR_REQUEST_PDU_HEADER)PduBuffer;
    pRequestHeader->Opcode = VENDOR_SPECIFIC_COMMAND;
    pRequestHeader->u.s.F = 1;
    pRequestHeader->HPID = htonl(HPID);
    pRequestHeader->RPID = htons(RPID);
    pRequestHeader->CPSlot = 0;
    pRequestHeader->DataSegLen = 0;
    pRequestHeader->AHSLen = 0;
    pRequestHeader->CSubPacketSeq = 0;
    ++iTag;
    pRequestHeader->PathCommandTag = htonl(iTag);
    pRequestHeader->VenderID = ntohs(NKC_VENDOR_ID);
    pRequestHeader->VenderOpVersion = VENDOR_OP_CURRENT_VERSION;
    pRequestHeader->VenderOp = cOperation;
    sal_memcpy(pRequestHeader->VenderParameter, pParameter, 8);
/*    sal_debug_print("CliVenderCommand: Operation %d, Parameter %lld\n", cOperation, NTOHLL(*pParameter)); */

/*    // Send Request.*/
    pdu.u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

    if(CliSendRequest(connsock, &pdu) != 0) {
        PrintError(WSAGetLastError(), "CliVenderCommand: Send First Request ");
        return -1;
    }
    
/*    // Read Request.*/
    iResult = CliReadReply(connsock, (char*)PduBuffer, &pdu);
    if(iResult < 0) {
        sal_debug_print( "[LanScsiCli]CliVenderCommand: Can't Read Reply.\n");
        return -1;
    }
    
/*    // Check Request Header.*/
    pReplyHeader = (PLANSCSI_VENDOR_REPLY_PDU_HEADER)pdu.u.pR2HHeader;


    if((pReplyHeader->Opcode != VENDOR_SPECIFIC_RESPONSE)
        || (pReplyHeader->u.s.F == 0)) {
        
        sal_debug_print( "[LanScsiCli]CliVenderCommand: BAD Reply Header. %d 0x%x\n", pReplyHeader->Opcode, pReplyHeader->u.s.F);
        return -1;
    }
    
    if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        sal_debug_print( "[LanScsiCli]CliVenderCommand: Failed.\n");
        return -1;
    }
    
    sal_memcpy(pParameter, pReplyHeader->VenderParameter, 8);

    return 0;
}

xint8 CliLogout_buffer[MAX_REQUEST_SIZE];
int
CliLogout(
       SOCKET    connsock
       )
{
    xint8                                *PduBuffer=CliLogout_buffer;
    PLANSCSI_LOGOUT_REQUEST_PDU_HEADER    pRequestHeader;
    PLANSCSI_LOGOUT_REPLY_PDU_HEADER    pReplyHeader;
    LANSCSI_PDU_POINTERS                pdu;
    int                                    iResult;
    
    sal_memset(PduBuffer, 0, MAX_REQUEST_SIZE);
    
    pRequestHeader = (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)PduBuffer;
    pRequestHeader->Opcode = LOGOUT_REQUEST;
    pRequestHeader->u.s.F = 1;
    pRequestHeader->HPID = htonl(HPID);
    pRequestHeader->RPID = htons(RPID);
    pRequestHeader->CPSlot = 0;
    pRequestHeader->DataSegLen = 0;
    pRequestHeader->AHSLen = 0;
    pRequestHeader->CSubPacketSeq = 0;
    ++iTag;
    pRequestHeader->PathCommandTag = htonl(iTag);
    
/*    // Send Request.*/
    pdu.u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;

    if(CliSendRequest(connsock, &pdu) != 0) {

        PrintError(WSAGetLastError(), "[LanScsiCli]CliLogout: Send Request ");
        return -1;
    }
    
/*    // Read Request.*/
    iResult = CliReadReply(connsock, (char*)PduBuffer, &pdu);
    if(iResult < 0) {
        sal_debug_print( "[LanScsiCli]CliLogout: Can't Read Reply.\n");
        return -1;
    }
    
    /* // Check Request Header.*/
    pReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)pdu.u.pR2HHeader;

    if((pReplyHeader->Opcode != LOGOUT_RESPONSE)
        || (pReplyHeader->u.s.F == 0)) {
        
        sal_debug_print( "[LanScsiCli]CliLogout: BAD Reply Header.\n");
        return -1;
    }
    
    if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        sal_debug_print( "[LanScsiCli]CliLogout: Failed.\n");
        return -1;
    }
    
    iSessionPhase = FLAG_SECURITY_PHASE;

    return 0;
}

int
CliIdeCommand(
           SOCKET    connsock,
           xint32    TargetId,
           xint64    LUN,
           xuchar    Command,
           xint64    Location,
           xint16    SectorCount,
           xint8    Feature,
           char*    pData
           )
{
    xint8                            PduBuffer[MAX_REQUEST_SIZE];
    PLANSCSI_IDE_REQUEST_PDU_HEADER    pRequestHeader;
    PLANSCSI_IDE_REPLY_PDU_HEADER    pReplyHeader;
    LANSCSI_PDU_POINTERS            pdu;
    int                                iResult;
    unsigned xint8                    iCommandReg;
    
/*    // Make Request.*/
    sal_memset(PduBuffer, 0, MAX_REQUEST_SIZE);
    
    pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER)PduBuffer;
    pRequestHeader->Opcode = IDE_COMMAND;
    pRequestHeader->u1.s.F = 1;
    pRequestHeader->HPID = htonl(HPID);
    pRequestHeader->RPID = htons(RPID);
    pRequestHeader->CPSlot = 0;
    pRequestHeader->DataSegLen = 0;
    pRequestHeader->AHSLen = 0;
    pRequestHeader->CSubPacketSeq = 0;
    ++iTag;
    pRequestHeader->PathCommandTag = htonl(iTag);
    pRequestHeader->TargetID = htonl(TargetId);
    pRequestHeader->LUNHi = pRequestHeader->LUNLow = 0;
    
/*    // Using Target ID. LUN is always 0.*/
    pRequestHeader->u2.s.DEV = TargetId;
    pRequestHeader->Feature = 0;            

    switch(Command) {
    case WIN_READ:
        {
            pRequestHeader->u1.s.R = 1;
            pRequestHeader->u1.s.W = 0;
            
            if(PerTarget[TargetId].bLBA48 == TRUE) {
                pRequestHeader->Command = WIN_READDMA_EXT;
            } else {
                pRequestHeader->Command = WIN_READDMA;
            }
        }
        break;
    case WIN_WRITE:
        {
            pRequestHeader->u1.s.R = 0;
            pRequestHeader->u1.s.W = 1;
            
            if(PerTarget[TargetId].bLBA48 == TRUE) {
                pRequestHeader->Command = WIN_WRITEDMA_EXT;
            } else {
                pRequestHeader->Command = WIN_WRITEDMA;
            }
        }
        break;
    case WIN_VERIFY:
        {
            pRequestHeader->u1.s.R = 0;
            pRequestHeader->u1.s.W = 0;
            
            if(PerTarget[TargetId].bLBA48 == TRUE) {
                pRequestHeader->Command = WIN_VERIFY_EXT;
            } else {
                pRequestHeader->Command = WIN_VERIFY;
            }
        }
        break;
    case WIN_IDENTIFY:
        {
            pRequestHeader->u1.s.R = 1;
            pRequestHeader->u1.s.W = 0;
            
            pRequestHeader->Command = WIN_IDENTIFY;
        }
        break;
    case WIN_SETFEATURES:
        {
            pRequestHeader->u1.s.R = 0;
            pRequestHeader->u1.s.W = 0;
            
            pRequestHeader->Feature = Feature;
            pRequestHeader->SectorCount_Curr = (unsigned xint8)SectorCount;
            pRequestHeader->Command = WIN_SETFEATURES;

            sal_debug_print( "[LanScsiCli]CliIdeCommand: SET Features Sector Count 0x%x\n", pRequestHeader->SectorCount_Curr);
        }
        break;
    default:
        sal_debug_print( "[LanScsiCli]CliIdeCommand: Not Supported IDE Command.\n");
        return -1;
    }
        
    if((Command == WIN_READ)
        || (Command == WIN_WRITE)
        || (Command == WIN_VERIFY)){
        
        if(PerTarget[TargetId].bLBA == FALSE) {
            sal_debug_print( "[LanScsiCli]CliIdeCommand: CHS not supported...\n");
            return -1;
        }
        
        pRequestHeader->u2.s.LBA = 1;
        
        if(PerTarget[TargetId].bLBA48 == TRUE) {
            
            pRequestHeader->LBALow_Curr = (xint8)(Location);
            pRequestHeader->LBAMid_Curr = (xint8)(Location >> 8);
            pRequestHeader->LBAHigh_Curr = (xint8)(Location >> 16);
            pRequestHeader->LBALow_Prev = (xint8)(Location >> 24);
            pRequestHeader->LBAMid_Prev = (xint8)(Location >> 32);
            pRequestHeader->LBAHigh_Prev = (xint8)(Location >> 40);
            
            pRequestHeader->SectorCount_Curr = (xint8)SectorCount;
            pRequestHeader->SectorCount_Prev = (xint8)(SectorCount >> 8);
            
        } else {
            
            pRequestHeader->LBALow_Curr = (xint8)(Location);
            pRequestHeader->LBAMid_Curr = (xint8)(Location >> 8);
            pRequestHeader->LBAHigh_Curr = (xint8)(Location >> 16);
            pRequestHeader->u2.s.LBAHeadNR = (xint8)(Location >> 24);
            
            pRequestHeader->SectorCount_Curr = (xint8)SectorCount;
        }
    }

/*    // Backup Command.*/
    iCommandReg = pRequestHeader->Command;

/*    // Send Request.*/
    pdu.u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pRequestHeader;
    
    if(CliSendRequest(connsock, &pdu) != 0) {
        PrintError(WSAGetLastError(), "CliIdeCommand: Send Request ");
        return -1;
    }
    
/*    // If Write, Send Data.*/
    if(Command == WIN_WRITE) {

/*        // Encrypt Data.*/
        if(DataEncryptAlgo != 0) {
            Encrypt32(
                (unsigned char*)pData,
                SectorCount * 512,
                (unsigned char *)&CHAP_C,
                iUserPassword
                );
        }

        iResult = CliSendIt(
            connsock,
            pData,
            SectorCount * 512
            );
#ifdef __WIN32__
        if(iResult == SOCKET_ERROR) {
#else
        if(iResult < 0) {
#endif
            PrintError(WSAGetLastError(), "CliIdeCommand: Send data for WRITE ");
            return -1;
        }
    }
    
/*    // If Read, Identify Op... Read Data.*/
    switch(Command) {
    case WIN_READ:
        {
            iResult = CliRecvIt(
                connsock, 
                pData, 
                SectorCount * 512
                );
            if(iResult <= 0) {
                PrintError(WSAGetLastError(), "CliIdeCommand: Receive Data for READ ");
                return -1;
            }

/*            // Decrypt Data.*/

            if(DataEncryptAlgo != 0) {
                
                Decrypt32(
                    (unsigned char*)pData,
                    SectorCount * 512,
                    CHAP_C,
                    iUserPassword
                    );
            }
        }
        break;
    case WIN_IDENTIFY:
        {

            iResult = CliRecvIt(
                connsock, 
                pData, 
                512
                );
            if(iResult <= 0) {
                PrintError(WSAGetLastError(), "CliIdeCommand: Receive Data for IDENTIFY ");
                return -1;
            }

/*            // Decrypt Data.*/
            if(DataEncryptAlgo != 0) {

                Decrypt32(
                    (unsigned char*)pData,
                    512,
                    CHAP_C,
                    iUserPassword
                    );
            }
        }
        break;
    default:
        break;
    }
    
/*    // Read Reply.*/
    iResult = CliReadReply(connsock, (char*)PduBuffer, &pdu);
#ifdef __WIN32__
    if(iResult == SOCKET_ERROR) {
#else
    if(iResult < 0) {
#endif
        sal_debug_print( "[LanScsiCli]CliIdeCommand: Can't Read Reply.\n");
        return -1;
#ifdef __WIN32__
    } else if(iResult == WAIT_TIMEOUT) {
        sal_debug_print( "[LanScsiCli]CliIdeCommand: Time out...\n");
        return WAIT_TIMEOUT;
#endif
    }
    
/*    // Check Request Header.*/
    pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER)pdu.u.pR2HHeader;

    if((pReplyHeader->Opcode != IDE_RESPONSE)
        || (pReplyHeader->u1.s.F == 0)
        || (pReplyHeader->Command != iCommandReg)) {
        
        sal_debug_print( "[LanScsiCli]CliIdeCommand: BAD Reply Header. Flag: 0x%x, Req. Command: 0x%x Rep. Command: 0x%x\n", 
            pReplyHeader->u1.Flags, iCommandReg, pReplyHeader->Command);
        return -1;
    }
    
    if(pReplyHeader->Response != LANSCSI_RESPONSE_SUCCESS) {
        sal_debug_print( "[LanScsiCli]CliIdeCommand: Failed. Response 0x%x %d %d Req. Command: 0x%x Rep. Command: 0x%x\n", 
            pReplyHeader->Response, ntohl(pReplyHeader->DataTransferLength), ntohl(pReplyHeader->DataSegLen),
            iCommandReg, pReplyHeader->Command
            );
        return -1;
    }
    
    return 0;
}

void
CliConvertString(
              char*    result,
              char*    source,
              int    size
              )
{
    int    i;

    for(i = 0; i < size / 2; i++) {
        result[i * 2] = source[i * 2 + 1];
        result[i * 2 + 1] = source[i * 2];
    }
    result[size] = '\0';
    
}

int
CliLba_capacity_is_ok(
                   struct hd_driveid *id
                   )
{
    unsigned xint32    lba_sects, chs_sects, head, tail;

    if((id->command_set_2 & 0x0400) && (id->cfs_enable_2 & 0x0400)) {
        /* 48 Bit Drive. */
        return TRUE;
    }

    /*
        The ATA spec tells large drivers to return
        C/H/S = 16383/16/63 independent of their size.
        Some drives can be jumpered to use 15 heads instead of 16.
        Some drives can be jumpered to use 4092 cyls instead of 16383
    */
    if((id->cyls == 16383 || (id->cyls == 4092 && id->cur_cyls== 16383)) 
        && id->sectors == 63 
        && (id->heads == 15 || id->heads == 16)
        && id->lba_capacity >= (unsigned)(16383 * 63 * id->heads))
        return TRUE;
    
    lba_sects = id->lba_capacity;
    chs_sects = id->cyls * id->heads * id->sectors;

    /* Perform a rough sanity check on lba_sects: within 10% is OK */
    if((lba_sects - chs_sects) < chs_sects / 10) {
        return TRUE;
    }

    /* Some drives have the word order reversed */
    head = ((lba_sects >> 16) & 0xffff);
    tail = (lba_sects & 0xffff);
    lba_sects = (head | (tail << 16));
    if((lba_sects - chs_sects) < chs_sects / 10) {
        id->lba_capacity = lba_sects;
/*        DEBUG_MSG(1, "CliLba_capacity_is_ok: Capacity reversed....\n"); */
        return TRUE;
    }
 
    return FALSE;
}

/* 
When struct hd_driveid is transferred, low byte of a word is received first.
Needs fixing for big-endian system 
See table 24.IDENTIFY DEVICE Information of ATA/ATAPI-6 r1b 
*/
void Clihd_driveid_byte_order_fix(struct hd_driveid* hid)
{
#if __BIG_ENDIAN__
    int i;    
    hid->config = letobes(hid->config);
    hid->cyls = letobes(hid->cyls);
    hid->heads = letobes(hid->heads);
    hid->track_bytes = letobes(hid->track_bytes);
    hid->sector_bytes = letobes(hid->sector_bytes);
    hid->sectors = letobes(hid->sectors);
    hid->vendor0 = letobes(hid->vendor0);
    hid->vendor1 = letobes(hid->vendor2);
    /* serial_no */
    hid->buf_type = letobes(hid->buf_type);
    hid->buf_size = letobes(hid->buf_size);
    hid->ecc_bytes = letobes(hid->ecc_bytes);
    /* fw_rev, model, max_multsect, vendor3 */
    hid->dword_io = letobes(hid->dword_io);
    /* vendor4, capability */
    hid->reserved50 = letobes(hid->reserved50);
    /* vendor5, tPIO, vendor6, tDMA */
    hid->field_valid = letobes(hid->field_valid);
    hid->cur_cyls = letobes(hid->cur_cyls);
    hid->cur_heads = letobes(hid->cur_heads);
    hid->cur_sectors = letobes(hid->cur_sectors);
    hid->cur_capacity0 = letobes(hid->cur_capacity1);
    /* multsect, multsect_valid */
    hid->lba_capacity = letobel(hid->lba_capacity);
    hid->dma_1word = letobes(hid->dma_1word);
    hid->dma_mword = letobes(hid->dma_mword);
    hid->eide_pio_modes = letobes(hid->eide_pio_modes);
    hid->eide_dma_min = letobes(hid->eide_dma_min);
    hid->eide_dma_time = letobes(hid->eide_dma_time);
    hid->eide_pio = letobes(hid->eide_pio);
    hid->eide_pio_iordy = letobes(hid->eide_pio_iordy);
    hid->words69_70[0] = letobes(hid->words69_70[0]);
    hid->words69_70[1] = letobes(hid->words69_70[1]);
    hid->words71_74[0] = letobes(hid->words71_74[0]);
    hid->words71_74[1] = letobes(hid->words71_74[1]);
    hid->words71_74[2] = letobes(hid->words71_74[2]);
    hid->words71_74[3] = letobes(hid->words71_74[3]);
    hid->queue_depth = letobes(hid->queue_depth);
    hid->words76_79[0] = letobes(hid->words76_79[0]);
    hid->words76_79[1] = letobes(hid->words76_79[1]);
    hid->words76_79[2] = letobes(hid->words76_79[2]);
    hid->words76_79[3] = letobes(hid->words76_79[3]);
    hid->major_rev_num = letobes(hid->major_rev_num);
    hid->minor_rev_num = letobes(hid->minor_rev_num);
    hid->command_set_1 = letobes(hid->command_set_1);
    hid->command_set_2 = letobes(hid->command_set_2);
    hid->cfsse = letobes(hid->cfsse);
    hid->cfs_enable_1 = letobes(hid->cfs_enable_1);
    hid->cfs_enable_2 = letobes(hid->cfs_enable_2);
    hid->csf_default = letobes(hid->csf_default);
    hid->dma_ultra = letobes(hid->dma_ultra);
    hid->trseuc = letobes(hid->trseuc);
    hid->trsEuc = letobes(hid->trsEuc);
    hid->CurAPMvalues = letobes(hid->CurAPMvalues);
    hid->mprc = letobes(hid->mprc);
    hid->hw_config = letobes(hid->hw_config);
    hid->acoustic = letobes(hid->acoustic);
    hid->msrqs = letobes(hid->msrqs);
    hid->sxfert = letobes(hid->sxfert);        
    hid->sal = letobes(hid->sal);
    hid->spg = letobel(hid->spg);
    hid->lba_capacity_2 = letobell(hid->lba_capacity_2);        
    for(i=0;i<22;i++) {
        hid->words104_125[i] = letobes(hid->words104_125[i]);
    }
    hid->last_lun = letobes(hid->last_lun);
    hid->word127 = letobes(hid->word127);
    hid->dlf = letobes(hid->dlf);        
    hid->csfo = letobes(hid->csfo);
    for(i=0;i<26;i++) {
        hid->words130_155[i] = letobes(hid->words130_155[i]);
    }    
    hid->word156 = letobes(hid->word156);    
    for(i=0;i<3;i++) {
        hid->words157_159[i] = letobes(hid->words157_159[i]);
    }    
    hid->cfa_power = letobes(hid->cfa_power);    
    for(i=0;i<14;i++) {
        hid->words161_175[i] = letobes(hid->words161_175[i]);
    }
    for(i=0;i<31;i++) {
        hid->words176_205[i] = letobes(hid->words176_205[i]);
    }    
    for(i=0;i<48;i++) {
        hid->words206_254[i] = letobes(hid->words206_254[i]);
    }                    
    hid->integrity_word = letobes(hid->integrity_word);    
#endif
}
int
CliGetDiskInfo(
            SOCKET    connsock,
            xuint32    TargetId
            )
{
    struct hd_driveid    info;
    int                    iResult;
    char                buffer[41];
    
/*    // identify.*/
    if((iResult = CliIdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, (char*)&info)) != 0) {
        sal_debug_print( "[LanScsiCli]CliGetDiskInfo: Identify Failed...\n");
        return iResult;
    }

    Clihd_driveid_byte_order_fix(&info);
    
    sal_debug_print("[LanScsiCli]CliGetDiskInfo: Target ID %d, Major 0x%x, Minor 0x%x, Capa 0x%x\n", 
        TargetId, info.major_rev_num, info.minor_rev_num, info.capability);
    
    sal_debug_print("[LanScsiCli]CliGetDiskInfo: DMA 0x%x, U-DMA 0x%x\n", 
        info.dma_mword, 
        info.dma_ultra);
    
/*    // DMA Mode.*/

    if(!(info.dma_mword & 0x0004)) {
        sal_debug_print( "Not Support DMA mode 2...\n");
        return -1;
    }

    if(!(info.dma_mword & 0x0400)) {
/*        // Set to DMA mode 2*/
        if((iResult = CliIdeCommand(connsock, TargetId, 0, WIN_SETFEATURES, 0, XFER_MW_DMA_2, SETFEATURES_XFER, NULL)) != 0) {
            sal_debug_print( "[LanScsiCli]CliGetDiskInfo: Set Feature Failed...\n");
            return iResult;
        }

/*        // identify.*/
        if((iResult = CliIdeCommand(connsock, TargetId, 0, WIN_IDENTIFY, 0, 0, 0, (char*)&info)) != 0) {
            sal_debug_print( "[LanScsiCli]CliGetDiskInfo: Identify Failed...\n");
            return iResult;
        }
        Clihd_driveid_byte_order_fix(&info);
        sal_debug_print("[LanScsiCli]CliGetDiskInfo: DMA 0x%x, U-DMA 0x%x\n", 
            info.dma_mword, 
            info.dma_ultra);
    }

    CliConvertString((char*)buffer, (char*)info.serial_no, 20);
    sal_debug_print("Serial No: %s\n", buffer);
    
    CliConvertString((char*)buffer, (char*)info.fw_rev, 8);
    sal_debug_print("Firmware rev: %s\n", buffer);
    
    sal_memset(buffer, 0, 41);
    sal_strncpy(buffer, (char*)info.model, 40);
    CliConvertString((char*)buffer, (char*)info.model, 40);
    sal_debug_print("Model No: %s\n", buffer);
    

/*    // Support LBA?*/

    if(info.capability & 0x02)
        PerTarget[TargetId].bLBA = TRUE;
    else
        PerTarget[TargetId].bLBA = FALSE;
    

/*    // Calc Capacity.*/

    if(info.command_set_2 & 0x0400 && info.cfs_enable_2 & 0x0400) {    /* Support LBA48bit */
        PerTarget[TargetId].bLBA48 = TRUE;
        PerTarget[TargetId].SectorCount = info.lba_capacity_2;
    } else {
        PerTarget[TargetId].bLBA48 = FALSE;
        
        if((info.capability & 0x02) && CliLba_capacity_is_ok(&info)) {
            PerTarget[TargetId].SectorCount = info.lba_capacity;
        }
        
        PerTarget[TargetId].SectorCount = info.lba_capacity;    
    }
    sal_debug_print("CAP 2 %d<<64+%d,", (int)(info.lba_capacity_2>>32), (int)(info.lba_capacity_2 & 0x0FFFFFFFF));
    sal_debug_print("CAP %d\n", info.lba_capacity);
    
    sal_debug_print("LBA %d, LBA48 %d, Number of Sectors: %lld\n", 
        PerTarget[TargetId].bLBA, 
        PerTarget[TargetId].bLBA48, 
        PerTarget[TargetId].SectorCount);
    return 0;
}

/* // CliDiscovery*/

int
CliDiscovery(
          SOCKET    connsock
          )
{
    int    iResult;
    char key[] = HASH_KEY_USER;
/*    //////////////////////////////////////////////////////////
    //
    // Login Phase...
    // 
*/
    if((iResult = CliLogin(connsock, LOGIN_TYPE_DISCOVERY, 0, key)) != 0) {
        sal_debug_print( "[LanScsiCli]CliDiscovery: Login Failed...\n");
        return iResult;
    }

    if((iResult = CliTextTargetList(connsock)) != 0) {
        sal_debug_print( "[LanScsiCli]CliDiscovery: Text Failed\n");
        return iResult;
    }

/*    ///////////////////////////////////////////////////////////////
    //
    // CliLogout Packet.
    // 
*/

    if((iResult = CliLogout(connsock)) != 0) {
        sal_debug_print( "[LanScsiCli]CliDiscovery: CliLogout Failed...\n");
        return iResult;
    }

    return 0;
}

int Clisscanf_lpx_addr(char *host, struct sockaddr_lpx *slpx)
{
    xuint32 hi = 0;
    xuint32 lo = 0;
    unsigned int c;

    for (;;) {
        c= *host++;
        if (isxdigit(c)) {
            hi = (hi << 4) | ((lo >> 28) & 0x0F);
            c -= '0';
            if(c > 9)
                c = (c + '0' - 'A' + 10) & 0x0F;
            lo = (lo << 4) | c;
        } else if(c == ':')
            continue;
        else
            break;
    }

    slpx->slpx_node[0] = hi >> 8;
    slpx->slpx_node[1] = hi;
    slpx->slpx_node[2] = lo >> 24;
    slpx->slpx_node[3] = lo >> 16;
    slpx->slpx_node[4] = lo >> 8;
    slpx->slpx_node[5] = lo;

    return 0;
}
    
int Cliopennet(char *host, int fport, int bport)
{
    int sock;
    struct sockaddr_lpx slpx;
    int result;

    if ((sock = lpx_socket(LPX_TYPE_STREAM, 0)) < 0) {
        sal_debug_print("Can not open SOCK\n");
        return -1;
    }
        
    sal_memset(&slpx, 0, sizeof(slpx));
    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = htons(fport);

    result = lpx_bind(sock, &slpx, sizeof(slpx));
    if(result < 0) {
        perror("AF_LPX: bind: ");
        return -1;
    }
    
    Clisscanf_lpx_addr(host, &slpx);
    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = htons(bport);

    sal_debug_print("From %4x To %02X:%02X:%02X:%02X:%02X:%02X::%4X\n",
        fport,
        slpx.slpx_node[0], slpx.slpx_node[1],
        slpx.slpx_node[2], slpx.slpx_node[3],
        slpx.slpx_node[4], slpx.slpx_node[5],
        bport);

    result = lpx_connect(sock, &slpx, sizeof(slpx));
    if(result < 0) {
        perror("AF_LPX: connect: ");
        return -1;
    }
    return sock;
}

int LanScsiCli(char* diskaddr, int utarget, int cmdopt)
{
    SOCKET                connsock;
    
    int                    iResult;
    unsigned xint32                i;
    unsigned xint32                j;
    
    int                    iTargetID = 0; 
    unsigned            UserID;
    sal_tick                    start_time, finish_time;
    unsigned xint8            cCommand = 0;

    xuchar*    data = sal_malloc(MAX_DATA_BUFFER_SIZE);
    xuchar*    data2 = sal_malloc(MAX_DATA_BUFFER_SIZE);
    xuchar*    buffer = sal_malloc(MAX_DATA_BUFFER_SIZE);

    sal_msleep(2);
    connsock = Cliopennet(diskaddr, 0, LPX_PORT_NUMBER);
    if (connsock <=0) {
        sal_debug_print("connection open failed\n");
        return 0;
    } else {
        sal_debug_print("sock fd=%d\n", connsock);
    }    
/*    // Init.*/
    
    iTag = 0;
    HPID = 0;
    HeaderEncryptAlgo = 0;
    DataEncryptAlgo = 0;
/*    iUserPassword = HASH_KEY_USER; */

    if(CliDiscovery(connsock) != 0) {
        return -1;
    }
/*    // Close Socket.*/    
    lpx_close(connsock);

/*    /////////////////////////////////////////////////////
    //
    // Normal 
    // */

    connsock = Cliopennet(diskaddr, 0, LPX_PORT_NUMBER);

    iTargetID = utarget;
/*    // Login...*/

    switch(iTargetID) {
    case 0:
        UserID = FIRST_TARGET_RO_USER;
        iTargetID = 0;
        break;
    case 1:
        UserID = FIRST_TARGET_RW_USER;
        iTargetID = 0;
        break;
    case 10:
        UserID = SECOND_TARGET_RO_USER;
        iTargetID = 1;
        break;
    case 11:
        UserID = SECOND_TARGET_RW_USER;
        iTargetID = 1;
        break;
    case 101:
        UserID = SUPERVISOR;
        cCommand = VENDOR_OP_SET_MAX_RET_TIME;
        break;
    case 102:
        UserID = SUPERVISOR;
        cCommand = VENDOR_OP_SET_MAX_CONN_TIME;
        break;
    case 103:
        UserID = SUPERVISOR;
        cCommand = VENDOR_OP_GET_MAX_RET_TIME;
        break;
    case 104:
        UserID = SUPERVISOR;
        cCommand = VENDOR_OP_GET_MAX_CONN_TIME;
        break;
    case 111:
        UserID = SUPERVISOR;
        cCommand = VENDOR_OP_SET_SUPERVISOR_PW;
        break;
    case 112:
        UserID = SUPERVISOR;
        cCommand = VENDOR_OP_SET_USER_PW;
        break;
    case 199:
        UserID = SUPERVISOR;
        cCommand = VENDOR_OP_RESET;
        break;
    default:
        sal_debug_print( "[LanScsiCli]main: Bad Target ID...\n");
        return -1 ;

    }

//    sal_debug_print("Press Enter...\n");
//    getchar();
    sal_debug_print("---------------------------------\n");
    if(UserID == SUPERVISOR) {

        char    Parameter[8] = {0};
        
        sal_debug_print( "SUPERVISOR MODE!!!\n");

        if((iResult = CliLogin(connsock, LOGIN_TYPE_NORMAL, SUPERVISOR, iUserPassword)) != 0) {
            char super_key[] = HASH_KEY_SUPER;
            sal_debug_print( "[LanScsiCli]main: Normal Key Login Failed...\n");
            
            if((iResult = CliLogin(connsock, LOGIN_TYPE_NORMAL, SUPERVISOR, super_key)) != 0) {
                sal_debug_print( "[LanScsiCli]main: Super Key Login Failed...\n");
                return iResult;
            }
        }

        sal_debug_print( "[LanScsiCli]main: End Login...\n");

        switch(cCommand) {
        case VENDOR_OP_SET_MAX_RET_TIME:
            sal_debug_print( "[LanScsiCli]main: VENDOR_OP_SET_MAX_RET_TIME...\n");
            MAKE_CHARS_FROM_UINT64(Parameter, cmdopt);
            break;
        case VENDOR_OP_SET_MAX_CONN_TIME:
            sal_debug_print( "[LanScsiCli]main: VENDOR_OP_SET_MAX_CONN_TIME...\n");
            MAKE_CHARS_FROM_UINT64(Parameter, cmdopt);            
            break;
        case VENDOR_OP_GET_MAX_RET_TIME:
            sal_debug_print( "[LanScsiCli]main: VENDOR_OP_GET_MAX_RET_TIME...\n");
            break;
        case VENDOR_OP_GET_MAX_CONN_TIME:
            sal_debug_print( "[LanScsiCli]main: VENDOR_OP_GET_MAX_CONN_TIME...\n");
            break;
        case VENDOR_OP_SET_SUPERVISOR_PW:
            sal_debug_print( "[LanScsiCli]main: VENDOR_OP_SET_SUPERVISOR_PW...\n");
            sal_memcpy(Parameter, iUserPassword, 8);
            break;
        case VENDOR_OP_SET_USER_PW:
            sal_debug_print( "[LanScsiCli]main: VENDOR_OP_SET_USER_PW...\n");
            sal_memcpy(Parameter, iSuperPassword, 8);
            break;
        case VENDOR_OP_RESET:
            sal_debug_print( "[LanScsiCli]main: Reset...\n");
            CliVenderCommand(connsock, VENDOR_OP_RESET, Parameter);
            return 0;
        default:
            sal_debug_print( "[LanScsiCli]main: Bad Command...%d\n", cCommand);
            return -1;
        }
    
/*        // Perform Operation.*/
        CliVenderCommand(connsock, cCommand, Parameter);
        sal_debug_print( "[LanScsiCli]main: End Vender Command...\n");
        
/*        // Must Reset.*/
        CliVenderCommand(connsock, VENDOR_OP_RESET, Parameter);
        
        return 0;

    }

    if((iResult = CliLogin(connsock, LOGIN_TYPE_NORMAL, UserID, iUserPassword)) != 0) {
        sal_debug_print( "[LanScsiCli]main: Login Failed...\n");
        return iResult;
    }

//    sal_debug_print("Press Enter...\n");
//    getchar();
    sal_debug_print("---------------------------------\n");

    for(i = 0; i < 10; i++) {
        PerTarget[iTargetID].TargetDataHi = 0;
        PerTarget[iTargetID].TargetDataLow = i;
        
        if(UserID & 0x00010000 || UserID & 0x00020000) {        
            if((iResult = CliTextTargetData(connsock, PARAMETER_OP_SET, iTargetID)) != 0) {
                sal_debug_print( "[LanScsiCli]main: Text TargetData set Failed...\n");
                return iResult;
                
            }
        }
        
        if((iResult = CliTextTargetData(connsock, PARAMETER_OP_GET, iTargetID)) != 0) {
            sal_debug_print( "[LanScsiCli]main: Text TargetData get Failed...\n");
            return iResult;
            
        }
        
        if(i != (int)PerTarget[iTargetID].TargetDataLow) {
/* //            sal_debug_print("Mismatch; i %d %d\n", i, PerTarget[iTargetID].TargetData);*/
        }
    }
    
    if((iResult = CliGetDiskInfo(connsock, iTargetID)) != 0) {
        sal_debug_print( "[LanScsiCli]main: Identify Failed... Master\n");
    } else {
        unsigned xint32    Source, a;

        start_time = sal_time_msec()/1000;
        
        sal_debug_print("Max Blocks %d\n", requestBlocks);
        sal_debug_print("Setting request block size to 16k\n");        
        requestBlocks = 32;
        
        Source = 0;
        a = 0;
        a -= 1;
        sal_debug_print("Will loop to %d\n", (int)(((int)PerTarget[iTargetID].SectorCount) / requestBlocks));
        for(i = (((int)PerTarget[iTargetID].SectorCount) / requestBlocks/ 8) * (PerTarget[iTargetID].NRROHost);
            i < (((int)PerTarget[iTargetID].SectorCount) / requestBlocks); i++) {
            
            for(j = 0; j < requestBlocks * 512; j++)
                data[j] = (char)i;
            data[0] = (xuchar)iTargetID;

            sal_memcpy(buffer, data, requestBlocks * 512);
            
            if(UserID & 0x00010000 || UserID & 0x00020000) {
/*                // Write. */
                if((iResult = CliIdeCommand(connsock, iTargetID, 0, WIN_WRITE, i * requestBlocks, requestBlocks, 0, (char*)data)) != 0) {
                    sal_debug_print( "[LanScsiCli]main: WRITE Failed... Sector %d\n", (int)i);
                    return iResult;
                }

            } else {
/*                // Read.*/
                if((iResult = CliIdeCommand(connsock, iTargetID, 0, WIN_READ, i * requestBlocks, requestBlocks, 0, (char*)data2)) != 0) {
                    sal_debug_print( "[LanScsiCli]main: READ Failed... Sector %d\n", (int)i);
                    return iResult;
                }                
            }
            if (i%100==0)
                sal_debug_print("%d\n", (int)i);
        }
        
        finish_time = sal_time_msec()/1000;
        
        sal_debug_print("----> Result) while %u seconds \n\n", (int)(finish_time - start_time));

    }
    
/*    // CliLogout Packet.*/

    if((iResult = CliLogout(connsock)) != 0) {
        sal_debug_print( "[LanScsiCli]main: CliLogout Failed...\n");
        return iResult;
    }
    
    sal_debug_print( "[LanScsiCli]main: CliLogout..\n");

    
/*    // Close Socket.*/
    
    lpx_close(connsock);
    return 0;
}
