/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2005, XIMETA, Inc., IRVINE, CA, USA.
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
*/
#include <linux/module.h> // EXPORT_NO_SYMBOLS, MODULE_LICENSE
#include <linux/version.h> // LINUX_VERSION_CODE, KERNEL_VERSION
#include <linux/init.h> // module_init, module_exit
#include <linux/config.h> // CONFIG_SMP
#include <linux/netdevice.h> // struct netdevice, register_netdevice_notifier, unregister_netdevice_notifier
#include <linux/net.h> // sock_create
#include <net/datalink.h> // struct datalink_proto
#include <net/sock.h> // sock_no_socketpair, sock_no_setsockopt, sock_no_getsockopt, struct sock
#include <linux/timer.h> // timer

#include "sal/sal.h"
#include "sal/libc.h"

#include "sal/debug.h"
#include "sal/thread.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,10))
MODULE_LICENSE("Proprietary, Send bug reports to support@ximeta.com");
#endif
MODULE_AUTHOR("Ximeta, Inc.");

#include "../../../netdisk/lanscsi.h"
#include "../../../netdisk/binaryparameters.h"
#include "../../../netdisk/hdreg.h"
#include "../../../netdisk/hash.h"

#define _NDDEV_LIST_H  /* prevent including of netdisk's list.h */
#include "../../../inc/lpx/lpxutil.h"
#include "ndasuser/ndasuser.h"
#define SOCKET    int

#undef LANSCSI_CURRENT_VERSION
#define LANSCSI_CURRENT_VERSION thisHWVersion

#define NETDISK_LPX_PORT_NUMBER   10000
#define NETDISK_LPX_BROADCAST_PORT_NUMBER 10001
#define NETDISK_LPX_PNP_PORT_NUMBER   10002


#define    NR_MAX_TARGET            2
#define MAX_DATA_BUFFER_SIZE    64 * 1024
#define DEFAULT_DISK_SIZE        1024 * 1024 * 10
#define MAX_CONNECTION            16

/* Make xuint64 type number from 8 byte char array(in network byte order) */
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

// Assume little endian
#define HTONLL(Data)    ( (((Data) & 0x00000000000000FF) << 56) | (((Data) & 0x000000000000FF00) << 40) \
                        | (((Data) & 0x0000000000FF0000) << 24) | (((Data) & 0x00000000FF000000) << 8)  \
                        | (((Data) & 0x000000FF00000000) >> 8)  | (((Data) & 0x0000FF0000000000) >> 24) \
                        | (((Data) & 0x00FF000000000000) >> 40) | (((Data) & 0xFF00000000000000) >> 56))

#define NTOHLL(Data)    ( (((Data) & 0x00000000000000FF) << 56) | (((Data) & 0x000000000000FF00) << 40) \
                        | (((Data) & 0x0000000000FF0000) << 24) | (((Data) & 0x00000000FF000000) << 8)  \
                        | (((Data) & 0x000000FF00000000) >> 8)  | (((Data) & 0x0000FF0000000000) >> 24) \
                        | (((Data) & 0x00FF000000000000) >> 40) | (((Data) & 0xFF00000000000000) >> 56))

// fix me: include proper header
#define htons(x) ({\
    xuint16 __x = (x); \
    ((xuint16) \
        ( \
            ( ( ((xuint16)(__x)) & (xuint16)0x00ffU) << 8) | \
            ( ( ((xuint16)(__x)) & (xuint16)0xff00U) >> 8) \
        ) \
    ); \
})
#define ntohs(x) htons(x)
#define ntohl(x) ({\
    xuint32 __x = (x); \
        ((xuint32)( \
        ((((xuint32)(__x)) & (xuint32)0x00ffU) << 24) | \
        ((((xuint32)(__x)) & (xuint32)0xff00U) << 8) | \
        ((((xuint32)(__x)) & (xuint32)0xff0000U) >> 8) | \
        ((((xuint32)(__x)) & (xuint32)0xff000000U) >> 24) )); \
})
#define htonl(x) ntohl(x)


#define NETDISK_DEV_LOCK_COUNT 4

typedef struct _LOCK_TABLE_ENTRY {
    xbool Locked;
    int     count;
    char owner[6];
} LOCK_TABLE_ENTRY, *PLOCK_TABLE_ENTRY;

typedef    struct _TARGET_DATA {
    xbool            bPresent; //
    xbool            bLBA;    //
    xbool            bLBA48;  //
    xint8            NRRWHost; //
    xint8            NRROHost; //
    
    unsigned xint32    TargetDataHi;
    unsigned xint32    TargetDataLow;
    char *            ExportDev;
    struct file *        ExportFilp;
    
    // IDE Info.
    xint64            Size;
    LOCK_TABLE_ENTRY LockTable[NETDISK_DEV_LOCK_COUNT];

} TARGET_DATA, *PTARGET_DATA;

typedef struct    _SESSION_DATA {
    SOCKET            connSock;
    unsigned xint16    TargetID;
    unsigned xint32    LUN;
    int                iSessionPhase;
    unsigned xint16    CSubPacketSeq;
    unsigned xint8    iLoginType;
    unsigned xint16    CPSlot;
    unsigned        PathCommandTag;
    unsigned        iUser;
    unsigned xint32    HPID;
    unsigned xint16    RPID;
    unsigned        CHAP_I;
    unsigned char     CHAP_C[4];
    xbool            bIncCount;
    unsigned char    iPassword[8];
    char            Addr[6];
} SESSION_DATA, *PSESSION_DATA;

typedef struct _PROM_DATA {
    unsigned xint64 MaxConnTime;
    unsigned xint64 MaxRetTime;
    char    UserPasswd[8];
    char    SuperPasswd[8];
} PROM_DATA, *PPROM_DATA;
// Global Variable.
xint16            G_RPID = 0;

// to support version 1.1, 2.0 20040426
unsigned xint8     thisHWVersion;

TARGET_DATA        *PerTarget[NR_MAX_TARGET];
PROM_DATA        *Prom;

unsigned xint16    HeaderEncryptAlgo = 1;
unsigned xint16    DataEncryptAlgo = 1;

#define DEBUG_PRINT(x...) printk(x)

static int ndemu_rand(void)
{
    static int i=1;
    return i++;
}
static int filp_lseek64(struct file* filp, xuint64 offset)
{
    filp->f_pos = offset;
    return 0;
}
static int filp_read(struct file* filp, char* buf, int len)
{
    filp->f_op->read(filp, buf, len,&filp->f_pos);
//    do_generic_file_read(filp, &filp->f_pos, 
    return len;
}
static int filp_write(struct file* filp, char* buf, int len)
{
    filp->f_op->write(filp, buf, len,&filp->f_pos);
    return len;
}

    
static xuint64 size_autodetect(struct file* filp)
{
    printk("Disk size = 0x%llx\n", filp->f_dentry->d_inode->i_size);
    return (xuint64)filp->f_dentry->d_inode->i_size;
}

static int EmuRecvIt(
       SOCKET    sock,
       void*    buf, 
       int        size
       )
{
    int res;
    int len = size;
    while (len > 0) {
        if ((res = lpx_recv(sock, buf, len, 0)) <= 0) {
            printk("EmuRecvIt failed : %d\n", res);
            if (len!=size) {/* some bytes are read */
                printk("Return read bytes %d\n", size - len);
                return size - len;
            }
            return res;
        }
        len -= res;
        buf += res;
    }

    return size;
}

static int EmuSendIt(
       SOCKET    sock,
       char*    buf, 
       int        size
       )
{
    int res;
    int len = size;

    while (len > 0) {
        if ((res = lpx_send(sock, buf, len, 0)) <= 0) {
            printk("Sendit failed : %d\n", res);
            return res;
        }
        len -= res;
        buf += res;
    }

    return size;
}

int
EmuReadRequest(
            SOCKET            connSock,
            char*            pBuffer,
            PLANSCSI_PDU    pPdu,
            PSESSION_DATA    pSessionData
            )
{
    int        iResult, iTotalRecved = 0;
    char*    pPtr = pBuffer;

    // Read Header.
    iResult = EmuRecvIt(
        connSock,
        pPtr,
        sizeof(LANSCSI_H2R_PDU_HEADER)
        );
    if(iResult < 0) {
        printk("EmuReadRequest: Can't Recv Header...\n");
        return iResult;
    } else if(iResult == 0) {
        printk("EmuReadRequest: Disconnected...\n");
        return iResult;
    } else
        iTotalRecved += iResult;

    pPdu->u.pH2RHeader = (PLANSCSI_H2R_PDU_HEADER)pPtr;

    pPtr += sizeof(LANSCSI_H2R_PDU_HEADER);

    if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
        && HeaderEncryptAlgo != 0) {

        Decrypt32(
            (unsigned char*)pPdu->u.pH2RHeader,
            sizeof(LANSCSI_H2R_PDU_HEADER),
            pSessionData->CHAP_C,
            pSessionData->iPassword
            );
    }

    // Read AHS.
    if(ntohs(pPdu->u.pH2RHeader->AHSLen) > 0) {
        iResult = EmuRecvIt(
            connSock,
            pPtr,
            ntohs(pPdu->u.pH2RHeader->AHSLen)
            );
        if(iResult < 0) {
            printk("EmuReadRequest: Can't Recv AHS...\n");
            return iResult;
        } else if(iResult == 0) {
            printk("EmuReadRequest: Disconnected...\n");

            return iResult;
        } else
            iTotalRecved += iResult;
    
        pPdu->pAHS = pPtr;

        pPtr += ntohs(pPdu->u.pH2RHeader->AHSLen);

        // to support version 1.1, 2.0

        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
            thisHWVersion == LANSCSI_VERSION_2_0) {
            if (pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
                && HeaderEncryptAlgo != 0) {

                Decrypt32(
                    (unsigned char*)pPdu->pAHS,
                    ntohs(pPdu->u.pH2RHeader->AHSLen),
//                    sizeof(pPdu->u.pR2HHeader->AHSLen),
                    (unsigned char *)&pSessionData->CHAP_C,
                    (unsigned char *)&pSessionData->iPassword
                    );
            }
        }

        // end of supporting version
    }

    // Read Header Dig.
    pPdu->pHeaderDig = NULL;

    // Read Data segment.
    if(ntohl(pPdu->u.pH2RHeader->DataSegLen) > 0) {
        iResult = EmuRecvIt(
            connSock,
            pPtr,
            ntohl(pPdu->u.pH2RHeader->DataSegLen)
            );
        if(iResult < 0) {
            printk("EmuReadRequest: Can't Recv Data segment...\n");
            return iResult;
        } else if(iResult == 0) {
            printk("EmuReadRequest: Disconnected...\n");

            return iResult;
        } else 
            iTotalRecved += iResult;
        
        pPdu->pDataSeg = pPtr;
        
        pPtr += ntohl(pPdu->u.pH2RHeader->DataSegLen);

        
        if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE
            && DataEncryptAlgo != 0) {
            
            Decrypt32(
                (unsigned char*)pPdu->pDataSeg,
                ntohl(pPdu->u.pH2RHeader->DataSegLen),
                pSessionData->CHAP_C,
                pSessionData->iPassword
                );
        }

    }
    
    // Read Data Dig.
    pPdu->pDataDig = NULL;
    
    return iTotalRecved;
}


struct SESSION_THREAD_PARAM {
    SOCKET sock;
    struct sockaddr_lpx slpx;
};


void*  
SessionThreadProc(
                void* param
                  )
{
    SESSION_DATA            SessionData;
    PSESSION_DATA            pSessionData;
    int                iResult;
    char                PduBuffer[MAX_REQUEST_SIZE];
    LANSCSI_PDU            pdu;
    PLANSCSI_H2R_PDU_HEADER        pRequestHeader;
    PLANSCSI_R2H_PDU_HEADER        pReplyHeader;
    int                 count, i;
    int                dma_mword = 0x207;
    // to support version 1.1, 2.0
    xuchar                ucParamType = 0;
    SOCKET sock = ((struct SESSION_THREAD_PARAM*)param)->sock;
    struct sockaddr_lpx slpx = ((struct SESSION_THREAD_PARAM*)param)->slpx;
    
    //
    // Init variables...
    //

    pSessionData = &SessionData;
    pSessionData->connSock = sock;
    memcpy(pSessionData->Addr, slpx.slpx_node, 6);

    pSessionData->CSubPacketSeq = 0;    
    pSessionData->bIncCount = FALSE;
    pSessionData->iSessionPhase = FLAG_SECURITY_PHASE;
        
    while(pSessionData->iSessionPhase != LOGOUT_PHASE) {

        //
        // Read Request.
        //
        iResult = EmuReadRequest(pSessionData->connSock, PduBuffer, &pdu, pSessionData);
        if(iResult <= 0) {
            printk("Session(%02x): Can't Read Request.\n", 0xff & pSessionData->Addr[5]);
            
            pSessionData->iSessionPhase = LOGOUT_PHASE;
            continue;
            //goto EndSession;
        }

        pRequestHeader = pdu.u.pH2RHeader;
        printk("Session(%02x): Received(code=%d)\n", 0xff & pSessionData->Addr[5], pRequestHeader->Opcode);
        
        switch(pRequestHeader->Opcode) {
        case LOGIN_REQUEST:
            {
                PLANSCSI_LOGIN_REQUEST_PDU_HEADER    pLoginRequestHeader = NULL;
                PLANSCSI_LOGIN_REPLY_PDU_HEADER        pLoginReplyHeader = NULL;
                PBIN_PARAM_SECURITY                    pSecurityParam = NULL;
                PAUTH_PARAMETER_CHAP                pAuthChapParam = NULL;
                PBIN_PARAM_NEGOTIATION                pParamNego = NULL;
                
                pLoginReplyHeader = (PLANSCSI_LOGIN_REPLY_PDU_HEADER)PduBuffer;
                pLoginRequestHeader = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)pRequestHeader;
                
                if(pSessionData->iSessionPhase == FLAG_FULL_FEATURE_PHASE) {
                    // Bad Command...
                    printk("Session2: Bad Command.\n");
                    pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    
                    goto MakeLoginReply;
                } 
                
                // Check Header.
                if((pLoginRequestHeader->VerMin > LANSCSI_CURRENT_VERSION)
                    || (pLoginRequestHeader->ParameterType != PARAMETER_TYPE_BINARY)
                    || (pLoginRequestHeader->ParameterVer != PARAMETER_CURRENT_VERSION)) {
                    // Bad Parameter...
                    printk("Session2: Bad Parameter.\n");
                    
                    pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_VERSION_MISMATCH;
                    goto MakeLoginReply;
                }
                
                // Check Sub Packet Sequence.
                if(ntohs(pLoginRequestHeader->CSubPacketSeq) != pSessionData->CSubPacketSeq) {
                    // Bad Sub Sequence...
                    printk("Session2: Bad Sub Packet Sequence. H %d R %d\n",
                        pSessionData->CSubPacketSeq,
                        ntohs(pLoginRequestHeader->CSubPacketSeq));
                    
                    pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    goto MakeLoginReply;
                }
                
                // Check Port...
                if(pRequestHeader->CSubPacketSeq > 0) {
                    if((pSessionData->HPID != (unsigned)ntohl(pLoginRequestHeader->HPID))
                        || (pSessionData->RPID != ntohs(pLoginRequestHeader->RPID))
                        || (pSessionData->CPSlot != ntohs(pLoginRequestHeader->CPSlot))
                        || (pSessionData->PathCommandTag != (unsigned)ntohl(pLoginRequestHeader->PathCommandTag))) {
                        
                        printk("Session2: Bad Port parameter.\n");
                        
                        pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                        goto MakeLoginReply;
                    }
                }
                
                switch(ntohs(pRequestHeader->CSubPacketSeq)) {
                case 0:
                    {
                        printk("*** First ***\n");
                        printk("VerMax is %d, VerMin is %d, ", pLoginRequestHeader->VerMax, pLoginRequestHeader->VerMin);
                        printk("DataSegLen is %d, AHSLen is %d\n", ntohl(pLoginRequestHeader->DataSegLen), ntohs(pLoginRequestHeader->AHSLen));
                        // Check Flag.
                        if((pLoginRequestHeader->u.s.T != 0)
                            || (pLoginRequestHeader->u.s.CSG != FLAG_SECURITY_PHASE)
                            || (pLoginRequestHeader->u.s.NSG != FLAG_SECURITY_PHASE)) {
                            printk("Session: BAD First Flag.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                            goto MakeLoginReply;
                        }
                        
                        // Check Parameter.

                        // to support version 1.1, 2.0
                        if (pLoginRequestHeader->VerMax == LANSCSI_VERSION_1_0) {
                        if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST)    // Minus AuthParameter[1]
                            || (pdu.pDataSeg == NULL)) {                            
                            printk("Session: BAD First Request Data.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                            goto MakeLoginReply;
                        }    
                        }
                        if (pLoginRequestHeader->VerMax == LANSCSI_VERSION_1_1 ||
                            pLoginRequestHeader->VerMax == LANSCSI_VERSION_2_0) {
                            if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST)    // Minus AuthParameter[1]
                                || (pdu.pAHS == NULL)) {                            
                                printk("Session: BAD First Request Data.\n");
                                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                                goto MakeLoginReply;
                            }    
                        }


                        if (pLoginRequestHeader->VerMax == LANSCSI_VERSION_1_0) {
                        pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
                        }
                        if (pLoginRequestHeader->VerMax == LANSCSI_VERSION_1_1 ||
                            pLoginRequestHeader->VerMax == LANSCSI_VERSION_2_0) {
                            pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pAHS;
                        }
                        // end of supporting version

                        if(pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) {
                            printk("Session: BAD First Request Parameter.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                            goto MakeLoginReply;
                        }
                        
                        // Login Type.
                        if((pSecurityParam->LoginType != LOGIN_TYPE_NORMAL) 
                            && (pSecurityParam->LoginType != LOGIN_TYPE_DISCOVERY)) {
                            printk("Session: BAD First Login Type.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                            goto MakeLoginReply;
                        }
                        
                        // Auth Type.
                        if(!(ntohs(pSecurityParam->AuthMethod) & AUTH_METHOD_CHAP)) {
                            printk("Session: BAD First Auth Method.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                            goto MakeLoginReply;
                        }
                        
                        // Store Data.
                        pSessionData->HPID = ntohl(pLoginRequestHeader->HPID);
                        pSessionData->CPSlot = ntohs(pLoginRequestHeader->CPSlot);
                        pSessionData->PathCommandTag = ntohl(pLoginRequestHeader->PathCommandTag);
                        
                        pSessionData->iLoginType = pSecurityParam->LoginType;
                        
                        // Assign RPID...
                        pSessionData->RPID = G_RPID;
                        
                        printk("[LanScsiEmu] Version Min %d, Auth Method %d, Login Type %d, HWVersion %d\n",
                            pLoginRequestHeader->VerMin, ntohs(pSecurityParam->AuthMethod), pSecurityParam->LoginType, thisHWVersion);
                        
                        // Make Reply.
                        pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
                        pLoginReplyHeader->u.s.T = 0;
                        pLoginReplyHeader->u.s.CSG = FLAG_SECURITY_PHASE;
                        pLoginReplyHeader->u.s.NSG = FLAG_SECURITY_PHASE;
                        pLoginReplyHeader->VerActive = thisHWVersion;

                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                            thisHWVersion == LANSCSI_VERSION_2_0) {
                            pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
                        }
                        // end of supporting version
                        
                        pSecurityParam = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
                        pSecurityParam->AuthMethod = htons(AUTH_METHOD_CHAP);
                    }
                    break;
                case 1:
                    {
                        printk("*** Second ***\n");
                        // Check Flag.
                        if((pLoginRequestHeader->u.s.T != 0)
                            || (pLoginRequestHeader->u.s.CSG != FLAG_SECURITY_PHASE)
                            || (pLoginRequestHeader->u.s.NSG != FLAG_SECURITY_PHASE)) {
                            printk("Session: BAD Second Flag.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                            goto MakeLoginReply;
                        }
                        
                        // Check Parameter.

                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST)    // Minus AuthParameter[1]
                            || (pdu.pDataSeg == NULL)) {
                            
                            printk("Session: BAD Second Request Data.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                            goto MakeLoginReply;
                        }    
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                            thisHWVersion == LANSCSI_VERSION_2_0) {
                            if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST)    // Minus AuthParameter[1]
                                || (pdu.pAHS == NULL)) {
                            
                                printk("Session: BAD Second Request Data.\n");
                                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                                goto MakeLoginReply;
                            }    
                        }

                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                            thisHWVersion == LANSCSI_VERSION_2_0) {
                            pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pAHS;
                        }
                        // end of supporting version

                        if((pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) 
                            || (pSecurityParam->LoginType != pSessionData->iLoginType)
                            || (ntohs(pSecurityParam->AuthMethod) != AUTH_METHOD_CHAP)) {
                            
                            printk("Session: BAD Second Request Parameter.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                            goto MakeLoginReply;
                        }
                        
                        // Hash Algorithm.
                        pAuthChapParam = (PAUTH_PARAMETER_CHAP)pSecurityParam->u.AuthParameter;
                        if(!(ntohl(pAuthChapParam->CHAP_A) & HASH_ALGORITHM_MD5)) {
                            printk("Session: Not Supported HASH Algorithm.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                            goto MakeLoginReply;
                        }
                        
                        // Store Data.
                        pSessionData->CHAP_I = ntohl(pAuthChapParam->CHAP_I);
                        
                        // Create Challenge
                        {
                            unsigned int tmp = (ndemu_rand() << 16) + ndemu_rand();
                            pSessionData->CHAP_C[0] = (tmp>>24) & 0x0ff;
                            pSessionData->CHAP_C[1] = (tmp>>16) & 0x0ff;
                            pSessionData->CHAP_C[2] = (tmp>>8) & 0x0ff;
                            pSessionData->CHAP_C[3] = tmp & 0x0ff;
                        }
                        
                        // Make Header
                        pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
                        pLoginReplyHeader->u.s.T = 0;
                        pLoginReplyHeader->u.s.CSG = FLAG_SECURITY_PHASE;
                        pLoginReplyHeader->u.s.NSG = FLAG_SECURITY_PHASE;

                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                            thisHWVersion == LANSCSI_VERSION_2_0) {
                            pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
                        }
                        // end of supporting version
                        
                        pSecurityParam = (PBIN_PARAM_SECURITY)&PduBuffer[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
                        pAuthChapParam = &pSecurityParam->u.ChapParam;
                        pSecurityParam->u.ChapParam.CHAP_A = htonl(HASH_ALGORITHM_MD5);
                        memcpy(pSecurityParam->u.ChapParam.CHAP_C, pSessionData->CHAP_C, 4);
                        
/*                        printk("CHAP_C %d\n", pSessionData->CHAP_C);*/
                    }
                    break;
                case 2:
                    {                        
                        printk("*** Third ***\n");
                        // Check Flag.
                        if((pLoginRequestHeader->u.s.T == 0)
                            || (pLoginRequestHeader->u.s.CSG != FLAG_SECURITY_PHASE)
                            || (pLoginRequestHeader->u.s.NSG != FLAG_LOGIN_OPERATION_PHASE)) {
                            printk("Session: BAD Third Flag.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                            goto MakeLoginReply;
                        }
                        
                        // Check Parameter.

                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)    // Minus AuthParameter[1]
                            || (pdu.pDataSeg == NULL)) {
                            
                            printk("Session: BAD Third Request Data.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                            goto MakeLoginReply;
                        }    
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                            thisHWVersion == LANSCSI_VERSION_2_0) {
                            if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST)    // Minus AuthParameter[1]
                                || (pdu.pAHS == NULL)) {
                            
                                printk("Session: BAD Third Request Data.\n"); 
                                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                                goto MakeLoginReply;
                            }    
                        }

                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pDataSeg;
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                            thisHWVersion == LANSCSI_VERSION_2_0) {
                            pSecurityParam = (PBIN_PARAM_SECURITY)pdu.pAHS;
                        }
                        // end of supporting version

                        if((pSecurityParam->ParamType != BIN_PARAM_TYPE_SECURITY) 
                            || (pSecurityParam->LoginType != pSessionData->iLoginType)
                            || (ntohs(pSecurityParam->AuthMethod) != AUTH_METHOD_CHAP)) {
                            
                            printk("Session: BAD Third Request Parameter.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                            goto MakeLoginReply;
                        }
                        pAuthChapParam = (PAUTH_PARAMETER_CHAP)pSecurityParam->u.AuthParameter;
                        if(!(ntohl(pAuthChapParam->CHAP_A) == HASH_ALGORITHM_MD5)) {
                            printk("Session: Not Supported HASH Algorithm.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                            goto MakeLoginReply;
                        }
                        if((unsigned)ntohl(pAuthChapParam->CHAP_I) != pSessionData->CHAP_I) {
                            printk("Session: Bad CHAP_I.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                            goto MakeLoginReply;
                        }
                        
                        // Store User ID(Name)
                        pSessionData->iUser = ntohl(pAuthChapParam->CHAP_N);
                        
                        switch(pSessionData->iLoginType) {
                        case LOGIN_TYPE_NORMAL:
                            {
//                                xbool    bRW = FALSE;

                                // Target Exist?
                                if(pSessionData->iUser & 0x00000001) {    // Use Target0
                                    if(PerTarget[0]->bPresent == FALSE) {
                                        printk("Session: No Target.\n");
                                        pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
                                        goto MakeLoginReply;
                                    }

                                }
                                if(pSessionData->iUser & 0x00000002) {    // Use Target1
                                    if(PerTarget[0]->bPresent == FALSE) {
                                        printk("Session: No Target.\n");
                                        pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
                                        goto MakeLoginReply;
                                    }
                                }

                                // Select Passwd.
                                if((pSessionData->iUser & 0x00010001)
                                    || (pSessionData->iUser & 0x00020002)) {
                                    const char key[8] = HASH_KEY_USER;
                                    memcpy(pSessionData->iPassword, key, 8);
                                } else {
                                    const char key[8] = HASH_KEY_USER;
                                    memcpy(pSessionData->iPassword, key, 8);
                                }

                                // Increse Login User Count.
                                if(pSessionData->iUser & 0x00000001) {    // Use Target0
                                    if(pSessionData->iUser &0x00010000) {
                                        if(PerTarget[0]->NRRWHost > 0) {
                                            printk("Session: Already RW. Logined\n");
                                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                                            goto MakeLoginReply;
                                        }
                                        PerTarget[0]->NRRWHost++;
                                    } else {
                                        PerTarget[0]->NRROHost++;
                                    }
                                }
                                if(pSessionData->iUser & 0x00000002) {    // Use Target0
                                    if(pSessionData->iUser &0x00020000) {
                                        if(PerTarget[1]->NRRWHost > 0) {
                                            printk("Session: Already RW. Logined\n");
                                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                                            goto MakeLoginReply;
                                        }
                                        PerTarget[1]->NRRWHost++;
                                    } else {
                                        PerTarget[1]->NRROHost++;
                                    }
                                }
                                pSessionData->bIncCount = TRUE;
                            }
                            break;
                        case LOGIN_TYPE_DISCOVERY:
                            {
                                const char key[8] = HASH_KEY_USER;
                                memcpy(pSessionData->iPassword, key, 8);                                
                            }
                            break;
                        default:
                            break;
                        }
                        
                        //
                        // Check CHAP_R
                        //
                        {
                            unsigned char    result[16] = { 0 };

                            Hash32To128(
                                (unsigned char*)&pSessionData->CHAP_C, 
                                result, 
                                (unsigned char*)&pSessionData->iPassword
                                );
                            if(memcmp(result, pAuthChapParam->CHAP_R, 16) != 0) {
                                printk( "Auth Failed.\n");
                                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_AUTH_FAILED;
                                goto MakeLoginReply;                            
                            }
                        }
                        
                        // Make Reply.
                        pLoginReplyHeader->u.s.T = 1;
                        pLoginReplyHeader->u.s.CSG = FLAG_SECURITY_PHASE;
                        pLoginReplyHeader->u.s.NSG = FLAG_LOGIN_OPERATION_PHASE;
                        pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;

                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                            thisHWVersion == LANSCSI_VERSION_2_0) {
                            pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
                        }
                        // end of supporting version
                        
                        // Set Phase.
                        pSessionData->iSessionPhase = FLAG_LOGIN_OPERATION_PHASE;
                    }
                    break;
                case 3:
                    {
                        printk( "*** Fourth ***\n");
                        // Check Flag.
                        if((pLoginRequestHeader->u.s.T == 0)
                            || (pLoginRequestHeader->u.s.CSG != FLAG_LOGIN_OPERATION_PHASE)
                            || ((pLoginRequestHeader->u.Flags & LOGIN_FLAG_NSG_MASK) != FLAG_FULL_FEATURE_PHASE)) {
                            printk( "Session: BAD Fourth Flag.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                            goto MakeLoginReply;
                        }

                        // Check Parameter.

                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        if((ntohl(pLoginRequestHeader->DataSegLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST)    // Minus AuthParameter[1]
                            || (pdu.pDataSeg == NULL)) {
                            
                            printk( "Session: BAD Fourth Request Data.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                            goto MakeLoginReply;
                        }    
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                            thisHWVersion == LANSCSI_VERSION_2_0) {
                            if((ntohs(pLoginRequestHeader->AHSLen) < BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST)    // Minus AuthParameter[1]
                                || (pdu.pAHS == NULL)) {
                            
                                printk( "Session: BAD Fourth Request Data.\n");
                                pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                                goto MakeLoginReply;
                            }    
                        }

                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pDataSeg;
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                            thisHWVersion == LANSCSI_VERSION_2_0) {
                            pParamNego = (PBIN_PARAM_NEGOTIATION)pdu.pAHS;
                        }
                        // end of supporting version

                        if((pParamNego->ParamType != BIN_PARAM_TYPE_NEGOTIATION)) {
                            printk( "Session: BAD Fourth Request Parameter.\n");
                            pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                            goto MakeLoginReply;
                        }
                        
                        // Make Reply.
                        pLoginReplyHeader->u.s.T = 1;
                        pLoginReplyHeader->u.s.CSG = FLAG_LOGIN_OPERATION_PHASE;
                        pLoginReplyHeader->u.s.NSG = FLAG_FULL_FEATURE_PHASE;
                        pLoginReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;

                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pLoginReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                            thisHWVersion == LANSCSI_VERSION_2_0) {
                            pLoginReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
                        }
                        // end of supporting version
                        
                        pParamNego = (PBIN_PARAM_NEGOTIATION)&PduBuffer[sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER)];
                        pParamNego->ParamType = BIN_PARAM_TYPE_NEGOTIATION;
                        pParamNego->HWType = HW_TYPE_ASIC;
//                        pParamNego->HWVersion = HW_VERSION_CURRENT;
                        // fixed to support version 1.1, 2.0
                        pParamNego->HWVersion = thisHWVersion;
                        pParamNego->NRSlot = htonl(1);
                        pParamNego->MaxBlocks = htonl(128);
                        pParamNego->MaxTargetID = htonl(2);
                        pParamNego->MaxLUNID = htonl(1);
                        pParamNego->HeaderEncryptAlgo = htons(HeaderEncryptAlgo);
                        pParamNego->HeaderDigestAlgo = 0;
                        pParamNego->DataEncryptAlgo = htons(DataEncryptAlgo);
                        pParamNego->DataDigestAlgo = 0;
                    }
                    break;
                default:
                    {
                        printk( "Session: BAD Sub-Packet Sequence.\n");
                        pLoginReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                        goto MakeLoginReply;
                    }
                    break;
                }
MakeLoginReply:
                pSessionData->CSubPacketSeq = ntohs(pLoginRequestHeader->CSubPacketSeq) + 1;
                
                pLoginReplyHeader->Opcode = LOGIN_RESPONSE;
                pLoginReplyHeader->VerMax = LANSCSI_CURRENT_VERSION;
                pLoginReplyHeader->VerActive = LANSCSI_CURRENT_VERSION;
                pLoginReplyHeader->ParameterType = PARAMETER_TYPE_BINARY;
                pLoginReplyHeader->ParameterVer = PARAMETER_CURRENT_VERSION;
            }
            break;
        case LOGOUT_REQUEST:
            {
                PLANSCSI_LOGOUT_REQUEST_PDU_HEADER    pLogoutRequestHeader;
                PLANSCSI_LOGOUT_REPLY_PDU_HEADER    pLogoutReplyHeader;
                
                pLogoutReplyHeader = (PLANSCSI_LOGOUT_REPLY_PDU_HEADER)PduBuffer;
                
                if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
                    // Bad Command...
                    printk( "Session2: LOGOUT Bad Command.\n");
                    pLogoutReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    
                    goto MakeLogoutReply;
                } 
                
                // Check Header.
                pLogoutRequestHeader = (PLANSCSI_LOGOUT_REQUEST_PDU_HEADER)pRequestHeader;
                if((pLogoutRequestHeader->u.s.F == 0)
                    || (pSessionData->HPID != (unsigned)ntohl(pLogoutRequestHeader->HPID))
                    || (pSessionData->RPID != ntohs(pLogoutRequestHeader->RPID))
                    || (pSessionData->CPSlot != ntohs(pLogoutRequestHeader->CPSlot))
                    || (0 != ntohs(pLogoutRequestHeader->CSubPacketSeq))) {
                    
                    printk( "Session2: LOGOUT Bad Port parameter.\n");
                    
                    pLogoutReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                    goto MakeLogoutReply;
                }
                
                // Do Logout.
                if(pSessionData->iLoginType == LOGIN_TYPE_NORMAL) {
                    // Something to do...
                }
                
                pSessionData->iSessionPhase = LOGOUT_PHASE;
                
                // Make Reply.
                pLogoutReplyHeader->u.s.F = 1;
                pLogoutReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
                pLogoutReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //0;
                
MakeLogoutReply:
                pLogoutReplyHeader->Opcode = LOGOUT_RESPONSE;
            }
            break;
        case TEXT_REQUEST:
            {
                PLANSCSI_TEXT_REQUEST_PDU_HEADER    pRequestHeader;
                PLANSCSI_TEXT_REPLY_PDU_HEADER        pReplyHeader;
                
                pReplyHeader = (PLANSCSI_TEXT_REPLY_PDU_HEADER)PduBuffer;
                
                printk( "Session: Text request\n");

                if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
                    // Bad Command...
                    printk( "Session2: TEXT_REQUEST Bad Command.\n");
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    
                    goto MakeTextReply;
                }
                
                // Check Header.
                pRequestHeader = (PLANSCSI_TEXT_REQUEST_PDU_HEADER)PduBuffer;                
                if((pRequestHeader->u.s.F == 0)
                    || (pSessionData->HPID != (unsigned)ntohl(pRequestHeader->HPID))
                    || (pSessionData->RPID != ntohs(pRequestHeader->RPID))
                    || (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
                    || (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
                    
                    printk( "Session2: TEXT Bad Port parameter.\n");
                    
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                    goto MakeTextReply;
                }
                
                // Check Parameter.

                // to support version 1.1, 2.0 
                if (thisHWVersion == LANSCSI_VERSION_1_0) {
                if(ntohl(pRequestHeader->DataSegLen) < 4) {    // Minimum size.
                    printk( "Session: TEXT No Data seg.\n");
                    
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                    goto MakeTextReply;
                }
                }
                if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                    thisHWVersion == LANSCSI_VERSION_2_0) {
                    if(ntohs(pRequestHeader->AHSLen) < 4) {    // Minimum size.
                        printk( "Session: TEXT No Data seg.\n");
                    
                        pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                        goto MakeTextReply;
                    }
                }
                // end of supporting version

                // to support version 1.1, 2.0 
                if (thisHWVersion == LANSCSI_VERSION_1_0) {
                    ucParamType = ((PBIN_PARAM)pdu.pDataSeg)->ParamType;
                } else if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                    thisHWVersion == LANSCSI_VERSION_2_0) {
                    ucParamType = ((PBIN_PARAM)pdu.pAHS)->ParamType;
                } else {
                    printk("Unknown HW version %d\n", thisHWVersion);
                }
                // end of supporting version

//                switch(((PBIN_PARAM)pdu.pDataSeg)->ParamType) {
                switch(ucParamType) {
                case BIN_PARAM_TYPE_TARGET_LIST:
                    {
                        PBIN_PARAM_TARGET_LIST    pParam = NULL;
                        
                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pParam = (PBIN_PARAM_TARGET_LIST)pdu.pDataSeg;
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                                thisHWVersion == LANSCSI_VERSION_2_0) {
                            pParam = (PBIN_PARAM_TARGET_LIST)pdu.pAHS;
                        }
                        // end of supporting version

                        count = 0;
                        for(i=0;i<NR_MAX_TARGET;i++) 
                            if(PerTarget[i]->bPresent) count ++;
                        pParam->NRTarget = count;
                        for(i = 0; i < pParam->NRTarget; i++) {
                            pParam->PerTarget[i].TargetID = htonl(i);
                            pParam->PerTarget[i].NRRWHost = PerTarget[i]->NRRWHost;
                            pParam->PerTarget[i].NRROHost = PerTarget[i]->NRROHost;
                            pParam->PerTarget[i].TargetDataHi = PerTarget[i]->TargetDataHi;
                            pParam->PerTarget[i].TargetDataLow = PerTarget[i]->TargetDataLow;
                        }
                        
                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //htonl(4 + 8 * NRTarget);
                    }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                                thisHWVersion == LANSCSI_VERSION_2_0) {
                            pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY); //htonl(4 + 8 * NRTarget);
                        }
                        // end of supporting version

                    }
                    break;
                case BIN_PARAM_TYPE_TARGET_DATA:
                    {
                        PBIN_PARAM_TARGET_DATA pParam = NULL;
                        
                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pParam = (PBIN_PARAM_TARGET_DATA)pdu.pDataSeg;
                        }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                                thisHWVersion == LANSCSI_VERSION_2_0) {
                            pParam = (PBIN_PARAM_TARGET_DATA)pdu.pAHS;
                        }
                        // end of supporting version
                        
                        if(pParam->GetOrSet == PARAMETER_OP_SET) {
                            if(ntohl(pParam->TargetID) == 0) {
                                if(!(pSessionData->iUser & 0x00000001) 
                                    ||!(pSessionData->iUser & 0x00010000)) {
                                    printk( "No Access Right\n");
                                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                                    goto MakeTextReply;
                                }
                                PerTarget[0]->TargetDataHi = pParam->TargetDataHi;
                                PerTarget[0]->TargetDataLow = pParam->TargetDataLow;                                
                            } else if(ntohl(pParam->TargetID) == 1) {
                                if(!(pSessionData->iUser & 0x00000002) 
                                    ||!(pSessionData->iUser & 0x00020000)) {
                                    printk( "No Access Right\n");
                                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                                    goto MakeTextReply;
                                }
                                PerTarget[1]->TargetDataHi = pParam->TargetDataHi;
                                PerTarget[1]->TargetDataLow = pParam->TargetDataLow;
                            } else {
                                printk( "No Access Right\n");
                                pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                                goto MakeTextReply;
                            }
                        } else {
                            if(ntohl(pParam->TargetID) == 0) {
                                if(!(pSessionData->iUser & 0x00000001)) {
                                    printk( "No Access Right\n");
                                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                                    goto MakeTextReply;
                                }
                                pParam->TargetDataHi = PerTarget[0]->TargetDataHi;
                                pParam->TargetDataLow = PerTarget[0]->TargetDataLow;
                            } else if(ntohl(pParam->TargetID) == 1) {
                                if(!(pSessionData->iUser & 0x00000002)) {
                                    printk( "No Access Right\n");
                                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                                    goto MakeTextReply;
                                }
                                pParam->TargetDataHi = PerTarget[1]->TargetDataHi;
                                pParam->TargetDataLow = PerTarget[1]->TargetDataLow;
                            } else {
                                printk( "No Access Right\n");
                                pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                                goto MakeTextReply;
                            }
                        }

                        
                        // to support version 1.1, 2.0 
                        if (thisHWVersion == LANSCSI_VERSION_1_0) {
                        pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY);
                    }
                        if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                                thisHWVersion == LANSCSI_VERSION_2_0) {
                            pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY);
                        }
                        // end of supporting version

                    }
                    break;
                default:
                    break;
                }
                
        
                // to support version 1.1, 2.0 
                if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pReplyHeader->DataSegLen = htonl(BIN_PARAM_SIZE_REPLY); //htonl(sizeof(BIN_PARAM_TARGET_DATA));
                }
                if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                        thisHWVersion == LANSCSI_VERSION_2_0) {
                    pReplyHeader->AHSLen = htons(BIN_PARAM_SIZE_REPLY); //htonl(sizeof(BIN_PARAM_TARGET_DATA));
                }
                // end of supporting version

MakeTextReply:
                pReplyHeader->Opcode = TEXT_RESPONSE;
                
            }
            break;
        case DATA_H2R:
            {
                if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
                    // Bad Command...
                }
            }
            break;
        case IDE_COMMAND:
            {
            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                PLANSCSI_IDE_REQUEST_PDU_HEADER    pRequestHeader;
                PLANSCSI_IDE_REPLY_PDU_HEADER    pReplyHeader;
                xuchar    data[MAX_DATA_BUFFER_SIZE] = { 0 };
                xint64    Location;
                unsigned SectorCount;
                int    iUnitDisk;
                
                pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER)PduBuffer;
                pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER)PduBuffer;                
                
                //
                // Convert Location and Sector Count.
                //
                Location = 0;
                SectorCount = 0;

                iUnitDisk = ntohl(pRequestHeader->TargetID);

                if(PerTarget[iUnitDisk]->bLBA48 == TRUE) {
                    Location = ((xint64)pRequestHeader->LBAHigh_Prev << 40)
                        + ((xint64)pRequestHeader->LBAMid_Prev << 32)
                        + ((xint64)pRequestHeader->LBALow_Prev << 24)
                        + ((xint64)pRequestHeader->LBAHigh_Curr << 16)
                        + ((xint64)pRequestHeader->LBAMid_Curr << 8)
                        + ((xint64)pRequestHeader->LBALow_Curr);
                    SectorCount = ((unsigned)pRequestHeader->SectorCount_Prev << 8)
                        + (pRequestHeader->SectorCount_Curr);
                } else {
                    Location = ((xint64)pRequestHeader->u2.s.LBAHeadNR << 24)
                        + ((xint64)pRequestHeader->LBAHigh_Curr << 16)
                        + ((xint64)pRequestHeader->LBAMid_Curr << 8)
                        + ((xint64)pRequestHeader->LBALow_Curr);
                    
                    SectorCount = pRequestHeader->SectorCount_Curr;
                }
                
                if(pSessionData->iLoginType != LOGIN_TYPE_NORMAL) {
                    // Bad Command...
                    printk( "Session2: IDE_COMMAND Not Normal Login.\n");
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    
                    goto MakeIDEReply;
                }
                
                if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
                    // Bad Command...
                    printk( "Session2: IDE_COMMAND Bad Command.\n");
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    
                    goto MakeIDEReply;
                }
                
                // Check Header.
                if((pRequestHeader->u1.s.F == 0)
                    || (pSessionData->HPID != (unsigned)ntohl(pRequestHeader->HPID))
                    || (pSessionData->RPID != ntohs(pRequestHeader->RPID))
                    || (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
                    || (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
                    
                    printk( "Session2: IDE Bad Port parameter.\n");
                    
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                    goto MakeIDEReply;
                }
                
                // Request for existed target?
                if(PerTarget[ntohl(pRequestHeader->TargetID)]->bPresent == FALSE) {
                    printk( "Session2: Target Not exist.\n");
                    
                    pReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
                    goto MakeIDEReply;
                }
                
                // LBA48 command? 
                if((PerTarget[iUnitDisk]->bLBA48 == FALSE) &&
                    ((pRequestHeader->Command == WIN_READDMA_EXT)
                    || (pRequestHeader->Command == WIN_WRITEDMA_EXT))) {
                    printk( "Session2: Bad Command.\n");
                    
                    pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
                    goto MakeIDEReply;
                }
                

                switch(pRequestHeader->Command) {
                case WIN_READDMA:
                case WIN_READDMA_EXT:
                    {
                        printk( "READ: Location %lld, Sector Count %d\n", Location, SectorCount);
                        
                        //
                        // Check Bound.
                        //
                        if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)]->Size) 
                        {
                            printk( "READ: Location = %lld, Sector_Size = %lld, TargetID = %d, Out of bound\n", Location + SectorCount, PerTarget[ntohl(pRequestHeader->TargetID)]->Size >> 9, ntohl(pRequestHeader->TargetID));
                            pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                            goto MakeIDEReply;
                        }
                        
                        filp_lseek64(PerTarget[iUnitDisk]->ExportFilp, Location * 512);
                        filp_read(PerTarget[iUnitDisk]->ExportFilp, data, SectorCount * 512);
                    }
                    break;
                case WIN_WRITEDMA:
                case WIN_WRITEDMA_EXT:
                    {
                        printk( "WRITE: Location %lld, Sector Count %d\n", Location, SectorCount);
                        
                        //
                        // Check access right.
                        // 1.1 and 2.0 grant write access of read connection.
                        //
                        if(thisHWVersion==0 && pSessionData->iUser != FIRST_TARGET_RW_USER
                            && pSessionData->iUser != SECOND_TARGET_RW_USER) {
                            printk( "Session2: No Write right...\n");
                            
                            pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                            goto MakeIDEReply;
                        }
                        
                        //
                        // Check Bound.
                        //
                        if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)]->Size) 
                        {
                            printk( "WRITE: Out of bound\n");
                            pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                            goto MakeIDEReply;
                        }
                        
                        // Receive Data.
                        iResult = EmuRecvIt(
                            pSessionData->connSock,
                            (char*)data,
                            SectorCount * 512
                            );
                        if(iResult < 0) {
                            printk( "EmuReadRequest: Can't Recv Data...\n");
                            pSessionData->iSessionPhase = LOGOUT_PHASE;
                            continue;
                            //goto EndSession;
                        }

                        //
                        // Decrypt Data.
                        //
                        if(DataEncryptAlgo != 0) {
                            Decrypt32(
                                (unsigned char*)data,
                                SectorCount * 512,
                                (unsigned char *)&pSessionData->CHAP_C,
                                (unsigned char *)&pSessionData->iPassword
                                );
                        }
                        filp_lseek64(PerTarget[iUnitDisk]->ExportFilp, Location * 512);
                        filp_write(PerTarget[iUnitDisk]->ExportFilp, data, SectorCount * 512);
                    }
                    break;
                case WIN_VERIFY:
                case WIN_VERIFY_EXT:
                    {
                        printk( "Verify: Location %lld, Sector Count %d...\n", Location, SectorCount);
                        
                        //
                        // Check Bound.
                        //
                        if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)]->Size) 
                        {
                            printk( "Verify: Out of bound\n");
                            pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                            goto MakeIDEReply;
                        }
                    }
                    break;
                case WIN_SETFEATURES:
                    {
                        struct    hd_driveid    *pInfo;
                        int Feature, Mode;

                        printk( "set features:\n");
                        pInfo = (struct hd_driveid *)data;
                        Feature = pRequestHeader->Feature;
                        Mode = pRequestHeader->SectorCount_Curr;
                        switch(Feature) {
                            case SETFEATURES_XFER: 
                                if(Mode == XFER_MW_DMA_2) {
    dma_mword &= 0xf8ff;
    dma_mword |= (1 << ((Mode & 0x7) + 8));
}
                                break;
                            default:
                                break;
                        }
                                
                    }
                    break;
                case WIN_IDENTIFY:
                    {
                        struct    hd_driveid    *pInfo;
                        char    serial_no[20] = { '2', '1', '0', '3', 0};
                        char    firmware_rev[8] = {'.', '1', 0, '0', 0, };
                        char    model[40] = { 'a', 'L', 's', 'n', 's', 'c', 'E', 'i', 'u', 'm', 0, };
                        int     iUnitDisk = ntohl(pRequestHeader->TargetID);
                        
                        printk( "Identify:\n");
                        
                        pInfo = (struct hd_driveid *)data;
                        pInfo->lba_capacity = PerTarget[ntohl(pRequestHeader->TargetID)]->Size / 512;
                        pInfo->lba_capacity_2 = PerTarget[ntohl(pRequestHeader->TargetID)]->Size /512;
                        pInfo->heads = 255;
                        pInfo->sectors = 63;
                        pInfo->cyls = pInfo->lba_capacity / (pInfo->heads * pInfo->sectors);
                        if(PerTarget[iUnitDisk]->bLBA) pInfo->capability |= 0x0002;    // LBA
                        if(PerTarget[iUnitDisk]->bLBA48) { // LBA48
                            pInfo->cfs_enable_2 |= 0x0400;
                            pInfo->command_set_2 |= 0x0400;
                        }
                        pInfo->major_rev_num = 0x0004 | 0x0008 | 0x010;    // ATAPI 5
                        pInfo->dma_mword = dma_mword;
                        memcpy(pInfo->serial_no, serial_no, 20);
                        memcpy(pInfo->fw_rev, firmware_rev, 8);
                        memcpy(pInfo->model, model, 40);
                    }
                    break;
                default:
                    printk( "Not Supported Command 0x%x\n", pRequestHeader->Command);
                    pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
                    goto MakeIDEReply;
                }
                
                pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
MakeIDEReply:
                if(pRequestHeader->Command == WIN_IDENTIFY) {

                    //
                    // Encrption.
                    //
                    if(DataEncryptAlgo != 0) {
                        Encrypt32(
                            (unsigned char*)data,
                            512,
                            (unsigned char *)&pSessionData->CHAP_C,
                            (unsigned char *)&pSessionData->iPassword
                            );
                    }

                    // Send Data.
                    iResult = EmuSendIt(
                        pSessionData->connSock,
                        (char*)data,
                        512
                        );
                    if(iResult < 0) {
                        printk( "EmuReadRequest: Can't Send Identify Data...\n");
                        pSessionData->iSessionPhase = LOGOUT_PHASE;
                        continue;
                        //goto EndSession;
                    }
                    
                }
                
                if((pRequestHeader->Command == WIN_READDMA)
                    || (pRequestHeader->Command == WIN_READDMA_EXT)) {    
                    //
                    // Encrption.
                    //
                    if(DataEncryptAlgo != 0) {
                        Encrypt32(
                            (unsigned char*)data,
                            SectorCount * 512,
                            (unsigned char *)&pSessionData->CHAP_C,
                            (unsigned char *)&pSessionData->iPassword
                            );
                    }
                    // Send Data.
                    iResult = EmuSendIt(
                        pSessionData->connSock,
                        (char*)data,
                        SectorCount * 512
                        );
                    if(iResult < 0) {
                        printk( "EmuReadRequest: Can't Send READ Data...\n");
                        pSessionData->iSessionPhase = LOGOUT_PHASE;
                        continue;
                        //goto EndSession;
                    }
                    
                }
                pReplyHeader->Opcode = IDE_RESPONSE;
                

            } else if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                   thisHWVersion == LANSCSI_VERSION_2_0) {
                PLANSCSI_IDE_REQUEST_PDU_HEADER_V1    pRequestHeader;
                PLANSCSI_IDE_REPLY_PDU_HEADER_V1    pReplyHeader;
                xuchar data[MAX_DATA_BUFFER_SIZE] = { 0 };
                xint64    Location;
                unsigned SectorCount;
                int    iUnitDisk;
                
                pReplyHeader = (PLANSCSI_IDE_REPLY_PDU_HEADER_V1)PduBuffer;
                pRequestHeader = (PLANSCSI_IDE_REQUEST_PDU_HEADER_V1)PduBuffer;                
                
                //
                // Convert Location and Sector Count.
                //
                Location = 0;
                SectorCount = 0;

#ifdef __WIN32__
                if(ntohl(pRequestHeader->TargetID) == 0) {
                    iUnitDisk = iUnitDisk0;
                } else {
                    iUnitDisk = iUnitDisk1;
                }
#else
                iUnitDisk = ntohl(pRequestHeader->TargetID);
#endif

                if(PerTarget[iUnitDisk]->bLBA48 == TRUE) {
                    Location = ((xint64)pRequestHeader->LBAHigh_Prev << 40)
                        + ((xint64)pRequestHeader->LBAMid_Prev << 32)
                        + ((xint64)pRequestHeader->LBALow_Prev << 24)
                        + ((xint64)pRequestHeader->LBAHigh_Curr << 16)
                        + ((xint64)pRequestHeader->LBAMid_Curr << 8)
                        + ((xint64)pRequestHeader->LBALow_Curr);
                    SectorCount = ((unsigned)pRequestHeader->SectorCount_Prev << 8)
                        + (pRequestHeader->SectorCount_Curr);
                } else {
                    Location = ((xint64)pRequestHeader->u2.s.LBAHeadNR << 24)
                        + ((xint64)pRequestHeader->LBAHigh_Curr << 16)
                        + ((xint64)pRequestHeader->LBAMid_Curr << 8)
                        + ((xint64)pRequestHeader->LBALow_Curr);
                    
                    SectorCount = pRequestHeader->SectorCount_Curr;
                }
                
                if(pSessionData->iLoginType != LOGIN_TYPE_NORMAL) {
                    // Bad Command...
                    printk( "Session2: IDE_COMMAND Not Normal Login.\n");
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    
                    goto MakeIDEReply1;
                }
                
                if(pSessionData->iSessionPhase != FLAG_FULL_FEATURE_PHASE) {
                    // Bad Command...
                    printk( "Session2: IDE_COMMAND Bad Command.\n");
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_BAD_COMMAND;
                    
                    goto MakeIDEReply1;
                }
                
                // Check Header.
                if((pRequestHeader->u1.s.F == 0)
                    || (pSessionData->HPID != (unsigned)ntohl(pRequestHeader->HPID))
                    || (pSessionData->RPID != ntohs(pRequestHeader->RPID))
                    || (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
                    || (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
                    
                    printk( "Session2: IDE Bad Port parameter.\n");
                    
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                    goto MakeIDEReply1;
                }
                
                // Request for existed target?
#ifdef __WIN32__
                if(PerTarget[ntohl(pRequestHeader->TargetID)].bPresent == FALSE) {
#else
                if(PerTarget[ntohl(pRequestHeader->TargetID)]->bPresent == FALSE) {
#endif
                    printk( "Session2: Target Not exist.\n");
                    
                    pReplyHeader->Response = LANSCSI_RESPONSE_T_NOT_EXIST;
                    goto MakeIDEReply1;
                }
                
                // LBA48 command? 
#ifdef __WIN32__
                if((PerTarget[iUnitDisk].bLBA48 == FALSE) &&
#else
                if((PerTarget[iUnitDisk]->bLBA48 == FALSE) &&
#endif
                    ((pRequestHeader->Command == WIN_READDMA_EXT)
                    || (pRequestHeader->Command == WIN_WRITEDMA_EXT))) {
                    printk( "Session2: Bad Command.\n");
                    
                    pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
                    goto MakeIDEReply1;
                }
                

                switch(pRequestHeader->Command) {
                case WIN_READDMA:
                case WIN_READDMA_EXT:
                    {
                        // for debug
/*                        int i; */
                        xuchar    data2[MAX_DATA_BUFFER_SIZE] = { 0 };
                        printk( "READ: Location %lld, Sector Count %d\n", Location, SectorCount);
                        
                        //
                        // Check Bound.
                        //
                        if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)]->Size) 
                        {
                            printk( "READ: Location = %lld, Sector_Size = %lld, TargetID = %d, Out of bound\n", Location + SectorCount, PerTarget[ntohl(pRequestHeader->TargetID)]->Size >> 9, ntohl(pRequestHeader->TargetID));
                            pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                            goto MakeIDEReply1;
                        }
                        filp_lseek64(PerTarget[iUnitDisk]->ExportFilp, Location * 512);
                        filp_read(PerTarget[iUnitDisk]->ExportFilp, data, SectorCount * 512);
                        memcpy(data2, data, SectorCount*512);
                        Decrypt32(
                            (unsigned char*)data2,
                            SectorCount * 512,
                            (unsigned char *)&pSessionData->CHAP_C,
                            (unsigned char *)&pSessionData->iPassword
                            );
                    }
                    break;
                case WIN_WRITEDMA:
                case WIN_WRITEDMA_EXT:
                    {
                        printk( "WRITE: Location %lld, Sector Count %d\n", Location, SectorCount);
                        
                        //
                        // Check access right.
                        //
                        if(thisHWVersion==0 && pSessionData->iUser != FIRST_TARGET_RW_USER
                            && (pSessionData->iUser != SECOND_TARGET_RW_USER)) {
                            printk( "Session2: No Write right...\n");
                            
                            pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                            goto MakeIDEReply1;
                        }
                        
                        //
                        // Check Bound.
                        //
#ifdef __WIN32__
                        if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)].Size) 
#else
                        if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)]->Size) 
#endif
                        {
                            printk( "WRITE: Out of bound\n");
                            pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                            goto MakeIDEReply1;
                        }
                        
                        // Receive Data.
                        iResult = EmuRecvIt(
                            pSessionData->connSock,
                            (char*)data,
                            SectorCount * 512
                            );
                        if(iResult < 0) {
                            printk( "ReadRequest: Can't Recv Data...\n");
                            pSessionData->iSessionPhase = LOGOUT_PHASE;
                            continue;
                            //goto EndSession;
                        }

                        //
                        // Decrypt Data.
                        //
                        if(DataEncryptAlgo != 0) {
                            Decrypt32(
                                (unsigned char*)data,
                                SectorCount * 512,
                                (unsigned char *)&pSessionData->CHAP_C,
                                (unsigned char *)&pSessionData->iPassword
                                );
                        }
#ifdef __WIN32__
                        _lseeki64(iUnitDisk, Location * 512, SEEK_SET);
                        write(iUnitDisk, data, SectorCount * 512);
#else
                        filp_lseek64(PerTarget[iUnitDisk]->ExportFilp, Location * 512);
                        filp_write(PerTarget[iUnitDisk]->ExportFilp, data, SectorCount * 512);
#endif
                    }
                    break;
                case WIN_VERIFY:
                case WIN_VERIFY_EXT:
                    {
#ifdef __WIN32__
                        printk( "Verify: Location %I64d, Sector Count %d...\n", Location, SectorCount);
#else
                        printk( "Verify: Location %lld, Sector Count %d...\n", Location, SectorCount);
#endif
                        
                        //
                        // Check Bound.
                        //
#ifdef __WIN32__
                        if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)].Size) 
#else
                        if(((Location + SectorCount) * 512) > PerTarget[ntohl(pRequestHeader->TargetID)]->Size) 
#endif
                        {
                            printk( "Verify: Out of bound\n");
                            pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                            goto MakeIDEReply1;
                        }
                    }
                    break;
                case WIN_SETFEATURES:
                    {
                        struct    hd_driveid    *pInfo;
                        int Feature, Mode;

                        printk( "set features:\n");
                        pInfo = (struct hd_driveid *)data;
//                        Feature = pRequestHeader->Feature;
                        // fixed to support version 1.1, 2.0
                        Feature = pRequestHeader->Feature_Curr;
                        Mode = pRequestHeader->SectorCount_Curr;
                        switch(Feature) {
                            case SETFEATURES_XFER: 
                                if(Mode == XFER_MW_DMA_2) {
    dma_mword &= 0xf8ff;
    dma_mword |= (1 << ((Mode & 0x7) + 8));
}
                                break;
                            default:
                                break;
                        }
                                
                    }
                    break;
                // to support version 1.1, 2.0
                case WIN_SETMULT:
                    {
                        printk( "set multiple mode\n");
                    }
                    break;
                case WIN_CHECKPOWERMODE1:
                    {
                        int Mode;

                        Mode = pRequestHeader->SectorCount_Curr;
                        printk( "check power mode = 0x%02x\n", Mode);
                    }
                    break;
                case WIN_STANDBY:
                    {
                        printk( "standby\n");
                    }
                    break;
                case WIN_IDENTIFY:
                case WIN_PIDENTIFY:
                    {
                        struct    hd_driveid    *pInfo;
                        char    serial_no[20] = { '2', '1', '0', '3', 0};
                        char    firmware_rev[8] = {'.', '1', 0, '0', 0, };
                        char    model[40] = { 'a', 'L', 's', 'n', 's', 'c', 'E', 'i', 'u', 'm', 0, };
                        int     iUnitDisk = ntohl(pRequestHeader->TargetID);
                        
                        printk( "Identify:\n");
                        
                        pInfo = (struct hd_driveid *)data;
#ifdef __WIN32__    
                        pInfo->lba_capacity = PerTarget[iUnitDisk].Size / 512;
                        pInfo->lba_capacity_2 = PerTarget[iUnitDisk].Size /512;
                        pInfo->heads = 255;
                        pInfo->sectors = 63;
                        pInfo->cyls = pInfo->lba_capacity / (pInfo->heads * pInfo->sectors);
                        if(PerTarget[iUnitDisk].bLBA) pInfo->capability |= 0x0002;    // LBA
                        if(PerTarget[iUnitDisk].bLBA48) { // LBA48
                            pInfo->cfs_enable_2 |= 0x0400;
                            pInfo->command_set_2 |= 0x0400;
                        }
#else
                        pInfo->lba_capacity = PerTarget[ntohl(pRequestHeader->TargetID)]->Size / 512;
                        pInfo->lba_capacity_2 = PerTarget[ntohl(pRequestHeader->TargetID)]->Size /512;
                        pInfo->heads = 255;
                        pInfo->sectors = 63;
                        pInfo->cyls = pInfo->lba_capacity / (pInfo->heads * pInfo->sectors);
                        if(PerTarget[iUnitDisk]->bLBA) pInfo->capability |= 0x0002;    // LBA
                        if(PerTarget[iUnitDisk]->bLBA48) { // LBA48
                            pInfo->cfs_enable_2 |= 0x0400;
                            pInfo->command_set_2 |= 0x0400;
                        }
#endif
                        pInfo->major_rev_num = 0x0004 | 0x0008 | 0x010;    // ATAPI 5
                        pInfo->dma_mword = dma_mword;
                        memcpy(pInfo->serial_no, serial_no, 20);
                        memcpy(pInfo->fw_rev, firmware_rev, 8);
                        memcpy(pInfo->model, model, 40);
                    }
                    break;
                default:
                    printk( "Not Supported Command1 0x%x\n", pRequestHeader->Command);
                    pReplyHeader->Response = LANSCSI_RESPONSE_T_BAD_COMMAND;
                    goto MakeIDEReply1;
                }
                
                pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
MakeIDEReply1:
                if(pRequestHeader->Command == WIN_IDENTIFY) {

                    //
                    // Encrption.
                    //
                    if(DataEncryptAlgo != 0) {
                        Encrypt32(
                            (unsigned char*)data,
                            512,
                            (unsigned char *)&pSessionData->CHAP_C,
                            (unsigned char *)&pSessionData->iPassword
                            );
                    }

                    // Send Data.
                    iResult = EmuSendIt(
                        pSessionData->connSock,
                        (xuchar*)data,
                        512
                        );
#ifdef __WIN32__
                    if(iResult == SOCKET_ERROR) {
#else
                    if(iResult < 0) {
#endif
                        printk( "ReadRequest: Can't Send Identify Data...\n");
                        pSessionData->iSessionPhase = LOGOUT_PHASE;
                        continue;
                        //goto EndSession;
                    }
                    
                }
                
                if((pRequestHeader->Command == WIN_READDMA)
                    || (pRequestHeader->Command == WIN_READDMA_EXT)) {    
                    //
                    // Encrption.
                    //
                    if(DataEncryptAlgo != 0) {
                        Encrypt32(
                            (unsigned char*)data,
                            SectorCount * 512,
                            (unsigned char *)&pSessionData->CHAP_C,
                            (unsigned char *)&pSessionData->iPassword
                            );
                    }
                    // Send Data.
                    iResult = EmuSendIt(
                        pSessionData->connSock,
                        (xuchar*)data,
                        SectorCount * 512
                        );
#ifdef __WIN32__
                    if(iResult == SOCKET_ERROR) {
#else
                    if(iResult < 0) {
#endif
                        printk( "ReadRequest: Can't Send READ Data...\n");
                        pSessionData->iSessionPhase = LOGOUT_PHASE;
                        continue;
                        //goto EndSession;
                    }
                    
                }
                pReplyHeader->Opcode = IDE_RESPONSE;
            }
                
            }
            break;

        case VENDOR_SPECIFIC_COMMAND:
            {
                PLANSCSI_VENDOR_REQUEST_PDU_HEADER    pRequestHeader;
                PLANSCSI_VENDOR_REPLY_PDU_HEADER    pReplyHeader;
                int lock_index;
                PLOCK_TABLE_ENTRY lock;
                pReplyHeader = (PLANSCSI_VENDOR_REPLY_PDU_HEADER)PduBuffer;
                pRequestHeader = (PLANSCSI_VENDOR_REQUEST_PDU_HEADER)PduBuffer;                
                if((pRequestHeader->u.s.F == 0)
                    || (pSessionData->HPID != (unsigned)ntohl(pRequestHeader->HPID))
                    || (pSessionData->RPID != ntohs(pRequestHeader->RPID))
                    || (pSessionData->CPSlot != ntohs(pRequestHeader->CPSlot))
                    || (0 != ntohs(pRequestHeader->CSubPacketSeq))) {
                    
                    printk( "Session2: Vender Bad Port parameter.\n");
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                    goto MakeVenderReply;
                }

                if( (pRequestHeader->VenderID != htons(NKC_VENDOR_ID)) ||
                    (pRequestHeader->VenderOpVersion != VENDOR_OP_CURRENT_VERSION) 
                ) {
                    printk( "Session2: Vender Version don't match.\n");
                    pReplyHeader->Response = LANSCSI_RESPONSE_RI_COMMAND_FAILED;
                    goto MakeVenderReply;

                }
                switch(pRequestHeader->VenderOp) {
                    case VENDOR_OP_SET_MAX_RET_TIME:
                        Prom->MaxRetTime = NTOHLL(pRequestHeader->VenderParameter);
                        break;
                    case VENDOR_OP_SET_MAX_CONN_TIME:
                        Prom->MaxConnTime = NTOHLL(pRequestHeader->VenderParameter);
                        break;
                    case VENDOR_OP_GET_MAX_RET_TIME:
                        pRequestHeader->VenderParameter = HTONLL(Prom->MaxRetTime);
                        break;
                    case VENDOR_OP_GET_MAX_CONN_TIME:
                        pRequestHeader->VenderParameter = HTONLL(Prom->MaxConnTime);
                        break;
                    case VENDOR_OP_SET_SUPERVISOR_PW:
                        memcpy(Prom->SuperPasswd, (char*) &pRequestHeader->VenderParameter, 8);
                        break;
                    case VENDOR_OP_SET_USER_PW:
                        memcpy(Prom->UserPasswd, (char*) &pRequestHeader->VenderParameter, 8);
                        break;
                    case VENDOR_OP_RESET:
                        pSessionData->iSessionPhase = LOGOUT_PHASE;
                        break;
                    case VENDOR_OP_SET_SEMA:
                        lock_index = (NTOHLL(pRequestHeader->VenderParameter) >> 32) & 0x3;
                        printk("Lock index = %d\n", lock_index);
                        lock = &PerTarget[0]->LockTable[lock_index];
                        if (lock->Locked) {
                            // Already locked
                            if (memcmp(lock->owner, pSessionData->Addr, 6)==0) {
                                printk("Lock request from lock owner\n");
                                pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
                            } else {
                                printk("Already locked\n");
                                pReplyHeader->Response = LANSCSI_RESPONSE_T_COMMAND_FAILED;
                            }
                        } else {
                            lock->Locked = 1;
                            pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
                            memcpy(lock->owner, pSessionData->Addr, 6);
                            printk("Locked\n");
                        }
                        pRequestHeader->VenderParameter = HTONLL((xuint64)lock->count);
                        break;
                    case VENDOR_OP_FREE_SEMA:
                        lock_index = (NTOHLL(pRequestHeader->VenderParameter) >> 32) & 0x3;
                        printk("Lock index = %d\n", lock_index);
                        lock = &PerTarget[0]->LockTable[lock_index];
                        pRequestHeader->VenderParameter = HTONLL((xuint64)lock->count);
                        if (lock->Locked) {
                            if (memcmp(lock->owner, pSessionData->Addr, 6)==0) {
                                printk("Unlocked\n");
                                lock->Locked = 0;
                                lock->count++;
                            } else {
                                printk("Not a lock owner\n");    
                            }
                        } else {
                            printk("Tried to unlock not-locked lock\n");
                        }
                        pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
                        break;
                    case VENDOR_OP_GET_SEMA:
                        lock_index = (NTOHLL(pRequestHeader->VenderParameter) >> 32) & 0x3;
                        printk("Lock index = %d\n", lock_index);
                        lock = &PerTarget[0]->LockTable[lock_index];
                        pRequestHeader->VenderParameter = HTONLL((xuint64)lock->count);
                        pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
                        break;
                    case VENDOR_OP_GET_OWNER_SEMA:
                        lock_index = (NTOHLL(pRequestHeader->VenderParameter) >> 32) & 0x3;
                        printk("Lock index = %d\n", lock_index);
                        lock = &PerTarget[0]->LockTable[lock_index];
                        memcpy( (char*)(&pRequestHeader->VenderParameter)+2, lock->owner, 6);
                        pReplyHeader->Response = LANSCSI_RESPONSE_SUCCESS;
                        break;
                    default:
                        printk("Unknown vendor opcode %d\n", pRequestHeader->VenderOp);
                        break;
                }
MakeVenderReply:
                pReplyHeader->Opcode = VENDOR_SPECIFIC_RESPONSE;
            }
            break;
        case NOP_H2R:
            // Do not reply to NOP
            {
                continue; // restart while loop
            }
            break;
        default:
            printk("Unknown request code %d\n", pRequestHeader->Opcode);
            // Bad Command...
            break;
        }
        
        {
            unsigned xint32    iTemp =0;

            // Send Reply.
            pReplyHeader = (PLANSCSI_R2H_PDU_HEADER)PduBuffer;
            
            pReplyHeader->HPID = htonl(pSessionData->HPID);
            pReplyHeader->RPID = htons(pSessionData->RPID);
            pReplyHeader->CPSlot = htons(pSessionData->CPSlot);

            // to support version 1.1, 2.0 
            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                pReplyHeader->AHSLen = 0;
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                pReplyHeader->DataSegLen = 0;
            }
            // end of supporting version

            pReplyHeader->CSubPacketSeq = htons(pSessionData->CSubPacketSeq);
            pReplyHeader->PathCommandTag = htonl(pSessionData->PathCommandTag);
            

            // to support version 1.1, 2.0 
            if (thisHWVersion == LANSCSI_VERSION_1_0) {
                iTemp = sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER) + ntohl(pReplyHeader->DataSegLen);
            }
            if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                thisHWVersion == LANSCSI_VERSION_2_0) {
                iTemp = sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER) + ntohs(pReplyHeader->AHSLen);
            }
            // end of supporting version

            //
            // Encrypt.
            //
            if(pReplyHeader->Opcode != LOGIN_RESPONSE) {
                if(HeaderEncryptAlgo != 0) {
                    Encrypt32(
                        (unsigned char*)PduBuffer,
                        sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER),
                        (unsigned char *)&pSessionData->CHAP_C,
                        (unsigned char *)&pSessionData->iPassword
                        );            
                }

                // to support version 1.1, 2.0 
                if (thisHWVersion == LANSCSI_VERSION_1_0) {
                  if(DataEncryptAlgo != 0 
                      && ntohl(pReplyHeader->DataSegLen) != 0) {
                      Encrypt32(
                          (unsigned char*)PduBuffer + sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER),
                          iTemp,
                          (unsigned char *)&pSessionData->CHAP_C,
                          (unsigned char *)&pSessionData->iPassword
                          );
                  }
                }
                if (thisHWVersion == LANSCSI_VERSION_1_1 ||
                    thisHWVersion == LANSCSI_VERSION_2_0) {
                    // modified 20040608 by shlee
                    if(HeaderEncryptAlgo != 0 
//                    if(DataEncryptAlgo != 0 
                        && ntohs(pReplyHeader->AHSLen) != 0) {
                        Encrypt32(
                            (unsigned char*)PduBuffer + sizeof(LANSCSI_LOGIN_REPLY_PDU_HEADER),
                            iTemp,
                            (unsigned char *)&pSessionData->CHAP_C,
                            (unsigned char *)&pSessionData->iPassword
                            );
                    }
                }
                // end of supporting version

            }
            
            iResult = EmuSendIt(
                pSessionData->connSock,
                PduBuffer,
                iTemp
                );
            if(iResult < 0) {
                printk( "EmuReadRequest: Can't Send First Reply...\n");
                pSessionData->iSessionPhase = LOGOUT_PHASE;
                continue;
                //goto EndSession;
            }
        }
        
        if((pReplyHeader->Opcode == LOGIN_RESPONSE)
            && (pSessionData->CSubPacketSeq == 4)) {
            pSessionData->CSubPacketSeq = 0;
            pSessionData->iSessionPhase = FLAG_FULL_FEATURE_PHASE;
        }
    }
    
//EndSession:

    printk( "Session2: Logout Phase.\n");
    
    switch(pSessionData->iLoginType) {
    case LOGIN_TYPE_NORMAL:
        {
            if(pSessionData->bIncCount == TRUE) {
                
                // Decrese Login User Count.
                if(pSessionData->iUser & 0x00000001) {    // Use Target0
                    if(pSessionData->iUser &0x00010000) {
                        PerTarget[0]->NRRWHost--;
                    } else {
                        PerTarget[0]->NRROHost--;
                    }
                }
                if(pSessionData->iUser & 0x00000002) {    // Use Target0
                    if(pSessionData->iUser &0x00020000) {
                        PerTarget[1]->NRRWHost--;
                    } else {
                        PerTarget[1]->NRROHost--;
                    }
                }
            }
        }
        break;
    case LOGIN_TYPE_DISCOVERY:
    default:
        break;
    }
    lpx_close(pSessionData->connSock);
    pSessionData->connSock = -1;

    return 0;
}

typedef struct _PNP_MESSAGE {
    xuchar ucType;
    xuchar ucVersion;
} PNP_MESSAGE, *PPNP_MESSAGE;

//
// for broadcast
//
void* GenPnpMessage(void* pararm)
{
    struct sockaddr_lpx slpx;
    int s;
    int result;
    PNP_MESSAGE message;
/*    int i = 0;
    int broadcastPermission;
    int opt; */
    printk("GenPnpMessage Entered...\n");

    s = lpx_socket(LPX_TYPE_DATAGRAM, 0);
    if (s < 0) {
        printk( "Can't open socket for lpx: %d\n", s);
        return NULL;
    }

/*
    broadcastPermission = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST,
            (void *)&broadcastPermission, sizeof(broadcastPermission)) < 0) {
        printk( "Can't setsockopt for broadcast: %s\n", strerror(errno));
        return ;
    }
*/
/*
    opt = 1;
    result = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    if (result !=0) {
        DEBUG_PRINT("Failed to set SO_REUSEADDR\n");
    }
*/
    memset(&slpx, 0, sizeof(slpx));
    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = htons(NETDISK_LPX_BROADCAST_PORT_NUMBER);
//    memcpy(slpx.slpx_node, LPX_BROADCAST_NODE, LPX_NODE_LEN);
#if 0 
        slpx.slpx_node[0] = 0xFF;
        slpx.slpx_node[1] = 0xFF;
        slpx.slpx_node[2] = 0xFF;
        slpx.slpx_node[3] = 0xFF;
        slpx.slpx_node[4] = 0xFF;
        slpx.slpx_node[5] = 0xFF;
#endif

    memset(&message, 0, sizeof(message));
    message.ucType = 0;
    message.ucVersion = thisHWVersion;

    result = lpx_bind(s, &slpx, sizeof(slpx));
    if (result < 0) {
        printk( "Error while binding PNP socket...: %d\n", result);
        return NULL;
    }

    memset(&slpx, 0, sizeof(slpx));
    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = htons(NETDISK_LPX_PNP_PORT_NUMBER);
#if 1 
        slpx.slpx_node[0] = 0xFF;
        slpx.slpx_node[1] = 0xFF;
        slpx.slpx_node[2] = 0xFF;
        slpx.slpx_node[3] = 0xFF;
        slpx.slpx_node[4] = 0xFF;
        slpx.slpx_node[5] = 0xFF;
#endif

    while(1) {
        printk( "Sendmsg pnp message\n");
        result = lpx_sendto(s, &message, sizeof(message),
                0, &slpx, sizeof(slpx));
        if (result < 0) {
            printk( "Can't send broadcast message: %d\n", result);
            return NULL;
        }
        sal_msleep(1000);
    }
    return NULL;
}

int ndemu(char* dev, int version, int nr_unit)
{
    int    i;
    char *shmptr;
    int net, sock;
    struct sockaddr_lpx slpx;
    int result;
    unsigned int addr_len;
    sal_thread_id id;
    struct SESSION_THREAD_PARAM stparam;
    char buffer[512];

    sal_assert(nr_unit==1); // support single unit only
    
    shmptr = sal_malloc( sizeof(TARGET_DATA) * NR_MAX_TARGET);
    memset(shmptr, 0, sizeof(TARGET_DATA) * NR_MAX_TARGET);
    for(i=0;i<NR_MAX_TARGET;i++) {
        PerTarget[i] = (PTARGET_DATA)shmptr;
        shmptr += sizeof(TARGET_DATA);
    }

    Prom = (PPROM_DATA) sal_malloc(sizeof(PROM_DATA));

    Prom->MaxConnTime = 4999; // 5 sec
    Prom->MaxRetTime = 63; // 63 ms
    {
        char key_user[8] = HASH_KEY_USER;
        char key_super[8] = HASH_KEY_SUPER;
        sal_memcpy(Prom->UserPasswd, key_user, 8);
        sal_memcpy(Prom->SuperPasswd, key_super, 8);
    }
    for (i=0;i<nr_unit;i++) {
        PerTarget[i]->ExportDev = dev;
        printk("dev = %s\n", PerTarget[i]->ExportDev);
        PerTarget[i]->ExportFilp = filp_open(dev, O_RDWR, 0);
        if (IS_ERR(PerTarget[i]->ExportFilp)) {
            printk("Can not open ND: %s\n", dev);
            goto out_error;
        }
        PerTarget[i]->bLBA = TRUE;
        PerTarget[i]->bLBA48 = FALSE;
        PerTarget[i]->bPresent = TRUE;
        PerTarget[i]->NRRWHost = 0;
        PerTarget[i]->NRROHost = 0;
        PerTarget[i]->TargetDataHi = 0;
        PerTarget[i]->TargetDataLow = 0;
        PerTarget[i]->Size = size_autodetect(PerTarget[i]->ExportFilp);
        if (PerTarget[i]->Size == 0) {
            printk("Cannot get disk size, exiting\n");
            return 0;
        }
        memset(buffer, 0, 512);
        filp_lseek64(PerTarget[i]->ExportFilp, PerTarget[i]->Size - 512);
        filp_write(PerTarget[i]->ExportFilp, buffer, 512);
        filp_lseek64(PerTarget[i]->ExportFilp, 0);
    }

    // added to support version 1.1, 2.0    
    thisHWVersion = (unsigned xint8)version;

    if ((sock = lpx_socket(LPX_TYPE_STREAM, 0)) < 0) {
        printk("Can not open SOCK: %d\n", sock);
        goto out_error;
    } else {
        printk("sock fd= %d\n", sock);    
    }

    DEBUG_PRINT("Waiting for connections... \n");

/*    DEBUG_PRINT("bind, "); */

    memset(&slpx, 0, sizeof(slpx));
    slpx.slpx_family = AF_LPX;
    slpx.slpx_port = htons(NETDISK_LPX_PORT_NUMBER);

    result = lpx_bind(sock, &slpx, sizeof(slpx));
    if(result < 0) {
        printk("LPX bind error");
        goto out_error;
    }

/*    DEBUG_PRINT("listen, "); */
    result = lpx_listen(sock, MAX_CONNECTION);
    if(result < 0) {
        printk("lpx listen failed\n");
        goto out_error;
    }

    //
    // for broadcast
    //
    sal_thread_create(&id, "pnp",  0, 0, GenPnpMessage, NULL);

    while(1) {
        DEBUG_PRINT("accept, ");
        addr_len = sizeof(slpx);
        if ((net = lpx_accept(sock, &slpx, (unsigned int*)&addr_len)) < 0) {
            printk("LPX: accept failed%d ", net);
            goto out_error;
        }
        /* FIXME: passing of stparam is not thread safe */
        stparam.sock = net;
        stparam.slpx = slpx;
        printk("Accepted a connection from %02x:%02x:%02x:%02x:%02x:%02x\n", 
                0x0ff & slpx.slpx_node[0], 0x0ff & slpx.slpx_node[1], 0x0ff & slpx.slpx_node[2],
                0x0ff & slpx.slpx_node[3], 0x0ff & slpx.slpx_node[4], 0x0ff & slpx.slpx_node[5]);
        sal_thread_create(&id, "con session",  0, 0, SessionThreadProc, (void*)&stparam);
    }
out_error:
    
    return 0;
}

char* ndemu_file = "/home/sjcho/xplat/release/nds.img";

//module_param_named(dev, ndemu_file, charp, 0);

int ndemu_init(void) 
{
    ndas_register_network_interface("eth0");
    ndas_start();
//    init_ndasproc();

    printk("%s %s initilizing\n", __DATE__ , __TIME__);
    ndemu(ndemu_file, 2, 1);
    return 0;
}

void ndemu_exit(void) 
{    
//    cleanup_ndasproc();
    ndas_stop();
    ndas_unregister_network_interface("eth0");
}

module_init(ndemu_init);
module_exit(ndemu_exit);

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,0))
EXPORT_NO_SYMBOLS;
#endif

