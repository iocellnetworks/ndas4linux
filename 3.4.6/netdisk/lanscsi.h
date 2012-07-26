#ifndef __LANSCSI_H__
#define __LANSCSI_H__

#ifdef __WIN32__
#include <pshpack1.h>
#endif

#include "sal/types.h"


#define LANSCSI_VERSION_1_0            0
#define LANSCSI_VERSION_1_1            1
#define LANSCSI_VERSION_2_0            2

#ifdef XPLAT_SUPPORT_ONLY_LANSCSI_1_0
#define LANSCSI_CURRENT_VERSION        LANSCSI_VERSION_1_0
#elif defined(XPLAT_SUPPORT_LANSCSI_ONLY_1_1)
#define LANSCSI_CURRENT_VERSION        LANSCSI_VERSION_1_1
#else
#define LANSCSI_CURRENT_VERSION        LANSCSI_VERSION_2_0
#endif

#define LANSCSIIDE_VER20_REV_ORIGINAL		0x0000
#define LANSCSIIDE_VER20_REV_100M			0x0018
#define LANSCSIIDE_VER20_REV_1G			0x0010

#define    LPX_PORT_NUMBER                10000
#define LPX_PORT_NUMBER_PNP            10002

/*
// Operation Codes.
*/

/* Host to Remote */
#define NOP_H2R                        0x00
#define LOGIN_REQUEST                0x01
#define LOGOUT_REQUEST                0x02
#define    TEXT_REQUEST                0x03
#define    TASK_MANAGEMENT_REQUEST        0x04
#define    SCSI_COMMAND                0x05
#define    DATA_H2R                    0x06
#define    IDE_COMMAND                    0x08
#define    VENDOR_SPECIFIC_COMMAND        0x0F    

/* Remote to Host */
#define NOP_R2H                        0x10
#define LOGIN_RESPONSE                0x11
#define LOGOUT_RESPONSE                0x12
#define    TEXT_RESPONSE                0x13
#define    TASK_MANAGEMENT_RESPONSE    0x14
#define    SCSI_RESPONSE                0x15
#define    DATA_R2H                    0x16
#define READY_TO_TRANSFER            0x17
#define    IDE_RESPONSE                0x18
#define    VENDOR_SPECIFIC_RESPONSE    0x1F    

/*
// Error code.
*/
#define LANSCSI_RESPONSE_SUCCESS                0x00
#define IDE_RESPONSE_CHECK_CONDITION 0x02
#define IDE_RESPONSE_BUSY 0x08

#define LANSCSI_RESPONSE_RI_NOT_EXIST           0x10
#define LANSCSI_RESPONSE_RI_BAD_COMMAND         0x11
#define LANSCSI_RESPONSE_RI_COMMAND_FAILED      0x12
#define LANSCSI_RESPONSE_RI_VERSION_MISMATCH    0x13
#define LANSCSI_RESPONSE_RI_AUTH_FAILED         0x14
#define LANSCSI_RESPONSE_T_NOT_EXIST            0x20
#define LANSCSI_RESPONSE_T_BAD_COMMAND          0x21
#define LANSCSI_RESPONSE_T_COMMAND_FAILED       0x22
#define LANSCSI_RESPONSE_T_BROKEN_DATA            0x23
#define IDE_RESPONSE_TASK_ABORTED 0x40
#define LANSCSI_RESPONSE_VENDOR_SPECIFIC_MIN    0x80
#define LANSCSI_RESPONSE_VENDOR_SPECIFIC_MAX    0xff

#if !defined(__LITTLE_ENDIAN_BITFIELD) &&  !defined(__BIG_ENDIAN_BITFIELD)
#error "Endian bit-field not specified" 
#endif

/*
// Host To Remote PDU Format
*/
typedef struct _LANSCSI_H2R_PDU_HEADER {
    xuchar    Opcode;
    xuchar    Flags;
    xint16    Reserved1;
    
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;
    unsigned    InitiatorTaskTag;
    unsigned    DataTransferLength;
    unsigned    TargetID;
    xint32  LunHi;
    xint32  LunLow;

    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    unsigned    Reserved5;
} __x_attribute_packed__ LANSCSI_H2R_PDU_HEADER, *PLANSCSI_H2R_PDU_HEADER;

/*
// Host To Remote PDU Format
*/
typedef struct _LANSCSI_R2H_PDU_HEADER {
    xuchar    Opcode;
    xuchar    Flags;
    xuchar    Response;
    
    xuchar    Reserved1;
    
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;
    unsigned    InitiatorTaskTag;
    unsigned    DataTransferLength;
    unsigned    TargetID;
    xint32  LunHi;
    xint32  LunLow;
    
    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    unsigned    Reserved5;
}__x_attribute_packed__ LANSCSI_R2H_PDU_HEADER, *PLANSCSI_R2H_PDU_HEADER;

/*
// Basic PDU.
*/
typedef struct _LANSCSI_PDU {
    union {
        PLANSCSI_H2R_PDU_HEADER    pH2RHeader;
        PLANSCSI_R2H_PDU_HEADER    pR2HHeader;
    } __x_attribute_packed__ u;
    char*                    pAHS;
    char*                    pHeaderDig;
    char*                    pDataSeg;
    char*                    pDataDig;
} __x_attribute_packed__ LANSCSI_PDU_POINTERS, *PLANSCSI_PDU_POINTERS;

/*
// Login Operation.
*/

/* Stages. */
#define    FLAG_SECURITY_PHASE            0
#define    FLAG_LOGIN_OPERATION_PHASE    1
#define    FLAG_FULL_FEATURE_PHASE        3

#define LOGOUT_PHASE                4

/* Parameter Types. */
#define    PARAMETER_TYPE_TEXT        0x0
#define    PARAMETER_TYPE_BINARY    0x1

#define LOGIN_FLAG_CSG_MASK        0x0C
#define LOGIN_FLAG_NSG_MASK        0x03

/* Login Request. */
typedef struct _LANSCSI_LOGIN_REQUEST_PDU_HEADER {
    xuchar    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)        
            xuchar    NSG:2;
            xuchar    CSG:2;
            xuchar    FlagReserved:2;
            xuchar    C:1;
            xuchar    T:1;
#elif defined(__BIG_ENDIAN_BITFIELD)    
            xuchar    T:1;        
            xuchar    C:1;
            xuchar    FlagReserved:2;
            xuchar    CSG:2;
            xuchar    NSG:2;
#endif
        } __x_attribute_packed__ s;
        xuchar    Flags;
    } __x_attribute_packed__ u;
    
    xint16    Reserved1;
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;
    
    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    xint32  Reserved5Hi;
    xint32  Reserved5Low;
#if 0
    xint64    Reserved5;
#endif    

    xuchar    ParameterType;
    xuchar    ParameterVer;
    xuchar    VerMax;
    xuchar    VerMin;
    
    unsigned    Reserved6;
    unsigned    Reserved7;
    unsigned    Reserved8;
}__x_attribute_packed__ LANSCSI_LOGIN_REQUEST_PDU_HEADER, *PLANSCSI_LOGIN_REQUEST_PDU_HEADER;


/* Login Reply. */
typedef struct _LANSCSI_LOGIN_REPLY_PDU_HEADER {
    xuchar    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)        
            xuchar    NSG:2;
            xuchar    CSG:2;
            xuchar    FlagReserved:2;
            xuchar    C:1;
            xuchar    T:1;
#elif defined(__BIG_ENDIAN_BITFIELD)    
            xuchar    T:1;        
            xuchar    C:1;
            xuchar    FlagReserved:2;
            xuchar    CSG:2;
            xuchar    NSG:2;
#endif            
        } __x_attribute_packed__ s;
        xuchar    Flags;
    } __x_attribute_packed__ u;

    xuchar    Response;
    
    xuchar    Reserved1;
    
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    xint32  Reserved5Hi;
    xint32  Reserved5Low;
#if 0
    xint64    Reserved5;
#endif    
    
    xuchar    ParameterType;
    xuchar    ParameterVer;
    xuchar    VerMax;
    xuchar    VerActive;
    xuint16   Revision; // Added at 2.0G. Set to distinguish emulator. Use 0xe00 for emulator ver 0.
	
    xint16    	Reserved6;
    unsigned    Reserved7;
    unsigned    Reserved8;
}__x_attribute_packed__ LANSCSI_LOGIN_REPLY_PDU_HEADER, *PLANSCSI_LOGIN_REPLY_PDU_HEADER;

/*
// Logout Operation.
*/

/* Logout Request. */
typedef struct _LANSCSI_LOGOUT_REQUEST_PDU_HEADER {
    xuchar    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuchar    FlagReserved:7;
            xuchar    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuchar    F:1;
            xuchar    FlagReserved:7;
#endif            
        } __x_attribute_packed__ s;
        xuchar    Flags;
    } __x_attribute_packed__ u;
    
    xint16    Reserved1;
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;    
    unsigned    Reserved3;
    unsigned    Reserved4;
    xint32  Reserved5Hi;
    xint32  Reserved5Low;
#if 0
    xint64    Reserved5;
#endif    

    
    unsigned    Reserved6;
    unsigned    Reserved7;
    unsigned    Reserved8;
    unsigned    Reserved9;
}__x_attribute_packed__ LANSCSI_LOGOUT_REQUEST_PDU_HEADER, *PLANSCSI_LOGOUT_REQUEST_PDU_HEADER;

/* Logout Reply. */
typedef struct _LANSCSI_LOGOUT_REPLY_PDU_HEADER {
    xuchar    Opcode;
     
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuchar    FlagReserved:7;
            xuchar    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuchar    F:1;
            xuchar    FlagReserved:7;
#endif            
        } __x_attribute_packed__ s;
        xuchar    Flags;
    } __x_attribute_packed__ u;

    xuchar    Response;
    
    xuchar    Reserved1;
    
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;    
    unsigned    Reserved3;
    unsigned    Reserved4;
    xint32  Reserved5Hi;
    xint32  Reserved5Low;
#if 0
    xint64    Reserved5;
#endif    


    unsigned    Reserved6;
    unsigned    Reserved7;
    unsigned    Reserved8;
    unsigned    Reserved9;
} __x_attribute_packed__ LANSCSI_LOGOUT_REPLY_PDU_HEADER, *PLANSCSI_LOGOUT_REPLY_PDU_HEADER;

/*
// Text Operation.
*/

/* Text Request. */
typedef struct _LANSCSI_TEXT_REQUEST_PDU_HEADER {
    xuchar    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuchar    FlagReserved:7;
            xuchar    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuchar    F:1;
            xuchar    FlagReserved:7;
#endif            
        } __x_attribute_packed__ s;
        xuchar    Flags;
    } __x_attribute_packed__ u;
    
    xint16    Reserved1;
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    xint32  Reserved5Hi;
    xint32  Reserved5Low;
#if 0
    xint64    Reserved5;
#endif    

    
    xuchar    ParameterType;
    xuchar    ParameterVer;
    xint16    Reserved6;    
    
    unsigned    Reserved7;
    unsigned    Reserved8;
    unsigned    Reserved9;
}__x_attribute_packed__ LANSCSI_TEXT_REQUEST_PDU_HEADER, *PLANSCSI_TEXT_REQUEST_PDU_HEADER; 

/* Text Reply. */
typedef struct _LANSCSI_TEXT_REPLY_PDU_HEADER {
    xuchar    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuchar    FlagReserved:7;
            xuchar    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuchar    F:1;
            xuchar    FlagReserved:7;
#endif            
        } __x_attribute_packed__ s;
        xuchar    Flags;
    } __x_attribute_packed__ u;

    xuchar    Response;
    
    xuchar    Reserved1;
    
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;
    
    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    xint32  Reserved5Hi;
    xint32  Reserved5Low;
#if 0
    xint64    Reserved5;
#endif    

    
    xuchar    ParameterType;
    xuchar    ParameterVer;
    xint16    Reserved6;
    
    unsigned    Reserved7;
    unsigned    Reserved8;
    unsigned    Reserved9;
} __x_attribute_packed__ LANSCSI_TEXT_REPLY_PDU_HEADER, *PLANSCSI_TEXT_REPLY_PDU_HEADER;
 

/*
// IDE Operation.
*/

/* IDE Request. */
typedef struct _LANSCSI_IDE_REQUEST_PDU_HEADER {
    xuchar    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuchar    FlagReserved:5;
            xuchar    W:1;
            xuchar    R:1;
            xuchar    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuchar    F:1;
            xuchar    R:1;
            xuchar    W:1;
            xuchar    FlagReserved:5;
#endif            
        } __x_attribute_packed__ s;
        xuchar    Flags;
    } __x_attribute_packed__ u1;
    
    xint16    Reserved1;
    unsigned    HPID; // ignored
    xint16    RPID; // ignored
    xint16    CPSlot; // ignored
    unsigned    DataSegLen;
    xint16    AHSLen; // ignored
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;
    unsigned    InitiatorTaskTag; // ignored
    unsigned    DataTransferLength;    
    unsigned    TargetID;
    xint32  LUNHi;
    xint32  LUNLow;
#if 0
    xint64    LUN;
#endif    

    
    xuchar    Reserved2;
    xuchar    Feature;
    xuchar    SectorCount_Prev;
    xuchar    SectorCount_Curr;
    xuchar    LBALow_Prev;
    xuchar    LBALow_Curr;
    xuchar    LBAMid_Prev;
    xuchar    LBAMid_Curr;
    xuchar    LBAHigh_Prev;
    xuchar    LBAHigh_Curr;
    xuchar    Reserved3;
    
    union {
        xuchar    Device;
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuchar    LBAHeadNR:4;
            xuchar    DEV:1;
            xuchar    obs1:1;
            xuchar    LBA:1;
            xuchar    obs2:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuchar    obs2:1;
            xuchar    LBA:1;
            xuchar    obs1:1;
            xuchar    DEV:1;
            xuchar    LBAHeadNR:4;
#endif                
        } __x_attribute_packed__ s;
    }  __x_attribute_packed__ u2;

    xuchar    Reserved4;
    xuchar    Command;
    xint16    Reserved5;

} __x_attribute_packed__ LANSCSI_IDE_REQUEST_PDU_HEADER, *PLANSCSI_IDE_REQUEST_PDU_HEADER;

/* IDE Reply. */
typedef struct _LANSCSI_IDE_REPLY_PDU_HEADER {
    xuchar    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuchar    FlagReserved:5;
            xuchar    W:1;
            xuchar    R:1;
            xuchar    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuchar    F:1;
            xuchar    R:1;
            xuchar    W:1;
            xuchar    FlagReserved:5;
#endif            
        } __x_attribute_packed__ s;
        xuchar    Flags;
    } __x_attribute_packed__ u1;

    xuchar    Response;
    
    xuchar    Status;
    
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;
    unsigned    InitiatorTaskTag;
    unsigned    DataTransferLength;    
    unsigned    TargetID;
    xint32  LUNHi;
    xint32  LUNLow;
#if 0
    xint64    LUN;
#endif    

    
    xuchar    Reserved2;
    xuchar    Feature;
    xuchar    SectorCount_Prev;
    xuchar    SectorCount_Curr;
    xuchar    LBALow_Prev;
    xuchar    LBALow_Curr;
    xuchar    LBAMid_Prev;
    xuchar    LBAMid_Curr;
    xuchar    LBAHigh_Prev;
    xuchar    LBAHigh_Curr;
    xuchar    Reserved3;

    union {
        xuchar    Device;
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuchar    LBAHeadNR:4;
            xuchar    DEV:1;
            xuchar    obs1:1;
            xuchar    LBA:1;
            xuchar    obs2:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuchar    obs2:1;
            xuchar    LBA:1;
            xuchar    obs1:1;
            xuchar    DEV:1;
            xuchar    LBAHeadNR:4;
#endif                
        }  __x_attribute_packed__ s;
    } __x_attribute_packed__ u2; 

    xuchar    Reserved4;
    xuchar    Command;
    xint16    Reserved5;
} __x_attribute_packed__ LANSCSI_IDE_REPLY_PDU_HEADER, *PLANSCSI_IDE_REPLY_PDU_HEADER;

/*
// Vender Specific Operation.
*/

#define    NKC_VENDOR_ID                0x0001
#define    VENDOR_OP_CURRENT_VERSION    0x01

#define VENDOR_OP_SET_MAX_RET_TIME  0x01
#define VENDOR_OP_SET_MAX_CONN_TIME 0x02
#define VENDOR_OP_GET_MAX_RET_TIME  0x03
#define VENDOR_OP_GET_MAX_CONN_TIME 0x04
#define VENDOR_OP_SET_SEMA          0x05
#define VENDOR_OP_FREE_SEMA         0x06
#define VENDOR_OP_GET_SEMA          0x07
#define VENDOR_OP_GET_OWNER_SEMA    0x08
#define VENDOR_OP_SET_DYNAMIC_MAX_CONN_TIME         0x0a
#define VENDOR_OP_SET_SUPERVISOR_PW 0x11
#define VENDOR_OP_SET_USER_PW       0x12
#define VENDOR_OP_SET_ENC_OPT       0x13
#define VENDOR_OP_SET_STANBY_TIMER  0x14
#define VENDOR_OP_GET_STANBY_TIMER  0x15
#define VENDOR_OP_SET_DELAY         0x16
#define VENDOR_OP_GET_DELAY         0x17
#define VENDOR_OP_SET_DYNAMIC_MAX_RET_TIME          0x18
#define VENDOR_OP_GET_DYNAMIC_MAX_RET_TIME          0x19
#define VENDOR_OP_RESET             0xFF
#define VENDOR_OP_RESET_ADDR        0xFE

/* Vender Specific Request. */
typedef struct _LANSCSI_VENDOR_REQUEST_PDU_HEADER {
    xuchar    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)    
            xuchar    FlagReserved:7;
            xuchar    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuchar    F:1;
            xuchar    FlagReserved:7;
#endif            
        } __x_attribute_packed__ s;
        xuchar    Flags;
    }  __x_attribute_packed__ u;
    
    xint16    Reserved1;
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    xint64    Reserved5;
    
    xuint16    VenderID;
    xuint8    VenderOpVersion;
    xuint8    VenderOp;
	union {
	    char        Bytes[8];
	    xuint64		Int64;
	} __x_attribute_packed__ VendorParameter;
    
    unsigned    Reserved6;
} __x_attribute_packed__ LANSCSI_VENDOR_REQUEST_PDU_HEADER, *PLANSCSI_VENDOR_REQUEST_PDU_HEADER;

/* Vender Specific Reply.*/
typedef struct _LANSCSI_VENDOR_REPLY_PDU_HEADER {
    xuchar    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuchar    FlagReserved:7;
            xuchar    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuchar    F:1;
            xuchar    FlagReserved:7;
#endif
        } __x_attribute_packed__ s;
        xuchar    Flags;
    } __x_attribute_packed__ u;

    xuchar    Response;
    
    xuchar    Reserved1;
    
    unsigned    HPID;
    xint16    RPID;
    xint16    CPSlot;
    unsigned    DataSegLen;
    xint16    AHSLen;
    xint16    CSubPacketSeq;
    unsigned    PathCommandTag;
    
    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    xint64    Reserved5;
    
    xuint16    VenderID;
    xuint8    VenderOpVersion;
    xuint8    VenderOp;

    union {
        char        Bytes[8];
        xuint64     Int64;
    } __x_attribute_packed__ VendorParameter;
    
    unsigned    Reserved6;
} __x_attribute_packed__ LANSCSI_VENDOR_REPLY_PDU_HEADER, *PLANSCSI_VENDOR_REPLY_PDU_HEADER
;

// IDE Request.
typedef struct _LANSCSI_IDE_REQUEST_PDU_HEADER_V1 {
    xuint8    Opcode;            // Byte 0
    
    // Flags.
    union {                    // Byte 1
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
            xuint8    FlagReserved:5;
            xuint8    W:1;
            xuint8    R:1;
            xuint8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuint8    F:1;
            xuint8    R:1;
            xuint8    W:1;
            xuint8    FlagReserved:5;
#endif
        } __x_attribute_packed__ s;
        xuint8    Flags;
    } __x_attribute_packed__ u1;
    
    xuint16    Reserved1;    // Byte 2
    xuint32    HPID;        // Byte 4
    xuint16    RPID;        // Byte 8
    xuint16    CPSlot;        // Byte 10
    xuint32    DataSegLen;// Byte 12
    xuint16    AHSLen;        // Byte 16
    xuint16    CSubPacketSeq;// Byte 18
    xuint32    PathCommandTag;// Byte 20
    xuint32    InitiatorTaskTag;// Byte 24
    xuint32    DataTransferLength;    // Byte 28
    xuint32    TargetID;// Byte 32
    xuint32    LUNHi;// Byte 36
    xuint32    LUNLow;// Byte 40

    union {
#if defined(__LITTLE_ENDIAN_BITFIELD)    // Byte 44
        xuint32 COM_Reserved : 2;
        xuint32 COM_TYPE_E : 1;
        xuint32 COM_TYPE_R : 1;
        xuint32 COM_TYPE_W : 1;
        xuint32 COM_TYPE_D_P : 1;
        xuint32 COM_TYPE_K : 1;
        xuint32 COM_TYPE_P : 1;
#elif defined(__BIG_ENDIAN_BITFIELD)
        xuint32 COM_TYPE_P : 1;
        xuint32 COM_TYPE_K : 1;
        xuint32 COM_TYPE_D_P : 1;
        xuint32 COM_TYPE_W : 1;
        xuint32 COM_TYPE_R : 1;
        xuint32 COM_TYPE_E : 1;
        xuint32 COM_Reserved : 2;
#endif
        xuint32   COM_LENG:24;

        xuint32 CommTypeLength;
    } Comm;

    xuint8    Feature_Prev;  // Byte 48
    xuint8    Feature_Curr;  // Byte 49
    xuint8    SectorCount_Prev; // Byte 50
    xuint8    SectorCount_Curr; // Byte 51
    xuint8    LBALow_Prev;// Byte 52
    xuint8    LBALow_Curr;// Byte 53
    xuint8    LBAMid_Prev;// Byte 54
    xuint8    LBAMid_Curr;// Byte 55
    xuint8    LBAHigh_Prev;// Byte 56
    xuint8    LBAHigh_Curr;// Byte 57
    xuint8    Command;// Byte 58
    
    union {
        xuint8    Device;// Byte 59
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuint8    LBAHeadNR:4;
            xuint8    DEV:1;
            xuint8    obs1:1;
            xuint8    LBA:1;
            xuint8    obs2:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuint8    obs2:1;
            xuint8    LBA:1;
            xuint8    obs1:1;
            xuint8    DEV:1;
            xuint8    LBAHeadNR:4;
#endif            
        } __x_attribute_packed__ s;
    } __x_attribute_packed__ u2;

} __x_attribute_packed__ LANSCSI_IDE_REQUEST_PDU_HEADER_V1, *PLANSCSI_IDE_REQUEST_PDU_HEADER_V1;

// IDE Reply.
typedef struct _LANSCSI_IDE_REPLY_PDU_HEADER_V1 {
    xuint8    Opcode;
    
    // Flags.
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)            
            xuint8    FlagReserved:5;
            xuint8    W:1;
            xuint8    R:1;
            xuint8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuint8    F:1;
            xuint8    R:1;
            xuint8    W:1;
            xuint8    FlagReserved:5;
#endif
        } __x_attribute_packed__ s;
        xuint8    Flags;
    }__x_attribute_packed__ u1;

    xuint8    Response;
    
    xuint8    Status;
    
    xuint32    HPID;
    xuint16    RPID;
    xuint16    CPSlot;
    xuint32    DataSegLen;
    xuint16    AHSLen;
    xuint16    CSubPacketSeq;
    xuint32    PathCommandTag;
    xuint32    InitiatorTaskTag;
    xuint32    DataTransferLength;    
    xuint32    TargetID;
    xuint64    LUN;
    union {
#if defined(__LITTLE_ENDIAN_BITFIELD)    
        xuint32 COM_Reserved : 2;
        xuint32 COM_TYPE_E : 1;
        xuint32 COM_TYPE_R : 1;
        xuint32 COM_TYPE_W : 1;
        xuint32 COM_TYPE_D_P : 1;
        xuint32 COM_TYPE_K : 1;
        xuint32 COM_TYPE_P : 1;
        xuint32 COM_LENG : 24;
#elif defined(__BIG_ENDIAN_BITFIELD)
        xuint32 COM_LENG : 24;
        xuint32 COM_TYPE_P : 1;
        xuint32 COM_TYPE_K : 1;
        xuint32 COM_TYPE_D_P : 1;
        xuint32 COM_TYPE_W : 1;
        xuint32 COM_TYPE_R : 1;
        xuint32 COM_TYPE_E : 1;
        xuint32 COM_Reserved : 2;
#endif
        xuint32 CommTypeLength;
    } Comm;
    xuint8    Feature_Prev;
    xuint8    Feature_Curr;
    xuint8    SectorCount_Prev;
    xuint8    SectorCount_Curr;
    xuint8    LBALow_Prev;
    xuint8    LBALow_Curr;
    xuint8    LBAMid_Prev;
    xuint8    LBAMid_Curr;
    xuint8    LBAHigh_Prev;
    xuint8    LBAHigh_Curr;
    xuint8    Command;
    
    union {
        xuint8    Device;
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            xuint8    LBAHeadNR:4;
            xuint8    DEV:1;
            xuint8    obs1:1;
            xuint8    LBA:1;
            xuint8    obs2:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            xuint8    obs2:1;
            xuint8    LBA:1;
            xuint8    obs1:1;
            xuint8    DEV:1;
            xuint8    LBAHeadNR:4;
#endif
        } __x_attribute_packed__ s;
    } __x_attribute_packed__ u2;

}__x_attribute_packed__ LANSCSI_IDE_REPLY_PDU_HEADER_V1, *PLANSCSI_IDE_REPLY_PDU_HEADER_V1;


#ifdef __WIN32__
#include <poppack.h>
#endif

#define    MAX_REQUEST_SIZE    1500

#ifdef DEBUG

#define __LANSCSI_OPCODE(x) \
({ \
    static char* __lanscsi_opcode__[] = { \
    "NOP",\
    "LOGIN",\
    "LOGOUT",\
    "TEXT",\
    "TASK_MANAGEMENT",\
    "SCSI",\
    "DATA", \
    "0x07", \
    "IDE", \
    "0x09", "0x0a", "0x0b", "0x0c", "0x0d", "0x0e",\
    "VENDOR_SPECIFIC"\
    };\
    __lanscsi_opcode__[(x)&0xff]; \
})
#define DEBUG_LANSCSI_LOGIN_REQUEST_PDU_HEADER(x) \
({ \
      static char __buf__[1024]; \
      PLANSCSI_LOGIN_REQUEST_PDU_HEADER __plogin = (PLANSCSI_LOGIN_REQUEST_PDU_HEADER)(x); \
      if ( __plogin ) \
          sal_snprintf(__buf__,sizeof(__buf__),\
              "(%p)={opcode=%s,"\
              "NSG=%d,CSG=%d,C/continue=%d,T/Transition=%d,"\
              "HPID=0x%x,RPID=0x%x,"\
              "DataSegLen=0x%x,CSubPacketSeq=0x%x," \
              "Param-type=%s," \
              "Param-version=%d,VersionMax=%d,VersionMin=%d}", \
              __plogin, __LANSCSI_OPCODE(__plogin->Opcode), \
              __plogin->u.s.NSG, \
              __plogin->u.s.CSG, \
              __plogin->u.s.C, \
              __plogin->u.s.T,\
              g_ntohl(__plogin->HPID), g_ntohs(__plogin->RPID),\
              g_ntohl(__plogin->DataSegLen), g_ntohs(__plogin->CSubPacketSeq), \
              __plogin->ParameterType ? "Binary":"Text",\
              __plogin->ParameterVer, __plogin->VerMax, __plogin->VerMin \
          );\
      else \
          sal_snprintf(__buf__,sizeof(__buf__), "NULL");\
      (const char *) __buf__; \
})

#define DEBUG_LANSCSI_LOGIN_REPLY_PDU_HEADER(x) \
({ \
    static char __buf__[1024]; \
    PLANSCSI_LOGIN_REPLY_PDU_HEADER __p__ = x; \
    if ( __p__ ) \
        sal_snprintf(__buf__,sizeof(__buf__),\
            "(%p)={opcode=%s,"\
            "NSG=%d,CSG=%d,C=%d,T=%d,"\
            "DataSegLen=0x%x,CSubPacketSeq=0x%x,"\
            "Param-type=%s," \
            "Param-version=%d,VersionMax=%d,VerActive=%d}", \
            __p__, __LANSCSI_OPCODE(__p__->Opcode & ~(0x10)), \
            __p__->u.s.NSG, __p__->u.s.CSG,__p__->u.s.C, __p__->u.s.T,\
            g_ntohl(__p__->DataSegLen), \
            g_ntohs(__p__->CSubPacketSeq), \
            __p__->ParameterType ? "Binary":"Text",\
            __p__->ParameterVer, \
            __p__->VerMax, \
            __p__->VerActive \
        );\
    else \
        sal_snprintf(__buf__,sizeof(__buf__), "NULL");\
    (const char *) __buf__; \
})

#define DEBUG_LANSCSI_IDE_REQUEST(x) \
({ \
    static char __buf__[1024]; \
    PLANSCSI_IDE_REQUEST_PDU_HEADER __p__ = x; \
    if ( __p__ ) \
        sal_snprintf(__buf__,sizeof(__buf__),\
            "(%p)={opcode=%s,"\
            "F=%d,R=%d,W=%d,"\
             "DataSegLen=0x%x,"\
             "CSubPacketSeq=0x%x,"\
             "PathCommandTag=0x%x,"\
            "DataTransferLength=0x%x," \
            "TargetID=0x%x," \
            "LUN=0x%04x%04x,"\
            "Register={"\
            "Feature=0x%x,"\
            "SectorCount_Prev=0x%x,"\
            "SectorCount_Curr=0x%x,"\
            "LBALow_Prev=0x%x,"\
            "LBALow_Curr=0x%x,"\
            "LBAMid_Prev=0x%x,"\
            "LBAMid_Curr=0x%x,"\
            "LBAHigh_Prev=0x%x,"\
            "LBAHigh_Curr=0x%x,"\
            "Device=0x%x,"\
            "Command=0x%x"\
            "}}", \
            __p__, __LANSCSI_OPCODE(__p__->Opcode & ~(0x10)), \
            __p__->u1.s.F, __p__->u1.s.R,__p__->u1.s.W, \
             g_ntohl(__p__->DataSegLen), \
            g_ntohs(__p__->CSubPacketSeq), \
            g_ntohl(__p__->PathCommandTag), \
            g_ntohl(__p__->DataTransferLength), \
            g_ntohl(__p__->TargetID), \
            g_ntohl(__p__->LUNHi), \
            g_ntohl(__p__->LUNLow), \
            __p__->Feature, \
            __p__->SectorCount_Prev, \
            __p__->SectorCount_Curr, \
            __p__->LBALow_Prev, \
            __p__->LBALow_Curr, \
            __p__->LBAMid_Prev, \
            __p__->LBAMid_Curr, \
            __p__->LBAHigh_Prev, \
            __p__->LBAHigh_Curr, \
            __p__->u2.Device, \
            __p__->Command \
        );\
    else \
        sal_snprintf(__buf__,sizeof(__buf__), "NULL");\
    (const char *) __buf__; \
})
#else
#define DEBUG_LANSCSI_LOGIN_REQUEST_PDU_HEADER(x) ({ ""; })
#define DEBUG_LANSCSI_LOGIN_REPLY_PDU_HEADER(x) ({ ""; })
#define DEBUG_LANSCSI_IDE_REQUEST(x) ({ ""; })
#endif // DEBUG
#endif

