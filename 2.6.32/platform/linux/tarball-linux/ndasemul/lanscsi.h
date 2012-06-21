#ifndef __LANSCSI_H__
#define __LANSCSI_H__

#ifdef NDAS_EMU

#ifdef __WIN32__
#include <pshpack1.h>
#endif

#define __x_attribute_packed__ __attribute__((packed)) 

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
    __u8    Opcode;
    __u8    Flags;
    __s16    Reserved1;
    
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;
    unsigned    InitiatorTaskTag;
    unsigned    DataTransferLength;
    unsigned    TargetID;
    __s32  LunHi;
    __s32  LunLow;

    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    unsigned    Reserved5;
} __x_attribute_packed__ LANSCSI_H2R_PDU_HEADER, *PLANSCSI_H2R_PDU_HEADER;

/*
// Host To Remote PDU Format
*/
typedef struct _LANSCSI_R2H_PDU_HEADER {
    __u8    Opcode;
    __u8    Flags;
    __u8    Response;
    
    __u8    Reserved1;
    
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;
    unsigned    InitiatorTaskTag;
    unsigned    DataTransferLength;
    unsigned    TargetID;
    __s32  LunHi;
    __s32  LunLow;
    
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
    __u8    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)        
            __u8    NSG:2;
            __u8    CSG:2;
            __u8    FlagReserved:2;
            __u8    C:1;
            __u8    T:1;
#elif defined(__BIG_ENDIAN_BITFIELD)    
            __u8    T:1;        
            __u8    C:1;
            __u8    FlagReserved:2;
            __u8    CSG:2;
            __u8    NSG:2;
#endif
        } __x_attribute_packed__ s;
        __u8    Flags;
    } __x_attribute_packed__ u;
    
    __s16    Reserved1;
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;
    
    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    __s32  Reserved5Hi;
    __s32  Reserved5Low;
#if 0
    __u64    Reserved5;
#endif    

    __u8    ParameterType;
    __u8    ParameterVer;
    __u8    VerMax;
    __u8    VerMin;
    
    unsigned    Reserved6;
    unsigned    Reserved7;
    unsigned    Reserved8;
}__x_attribute_packed__ LANSCSI_LOGIN_REQUEST_PDU_HEADER, *PLANSCSI_LOGIN_REQUEST_PDU_HEADER;


/* Login Reply. */
typedef struct _LANSCSI_LOGIN_REPLY_PDU_HEADER {
    __u8    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)        
            __u8    NSG:2;
            __u8    CSG:2;
            __u8    FlagReserved:2;
            __u8    C:1;
            __u8    T:1;
#elif defined(__BIG_ENDIAN_BITFIELD)    
            __u8    T:1;        
            __u8    C:1;
            __u8    FlagReserved:2;
            __u8    CSG:2;
            __u8    NSG:2;
#endif            
        } __x_attribute_packed__ s;
        __u8    Flags;
    } __x_attribute_packed__ u;

    __u8    Response;
    
    __u8    Reserved1;
    
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    __s32  Reserved5Hi;
    __s32  Reserved5Low;
#if 0
    __u64    Reserved5;
#endif    
    
    __u8    ParameterType;
    __u8    ParameterVer;
    __u8    VerMax;
    __u8    VerActive;
    __u16   Revision; // Added at 2.0G. Set to distinguish emulator. Use 0xe00 for emulator ver 0.
	
    __s16    	Reserved6;
    unsigned    Reserved7;
    unsigned    Reserved8;
}__x_attribute_packed__ LANSCSI_LOGIN_REPLY_PDU_HEADER, *PLANSCSI_LOGIN_REPLY_PDU_HEADER;

/*
// Logout Operation.
*/

/* Logout Request. */
typedef struct _LANSCSI_LOGOUT_REQUEST_PDU_HEADER {
    __u8    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    FlagReserved:7;
            __u8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    F:1;
            __u8    FlagReserved:7;
#endif            
        } __x_attribute_packed__ s;
        __u8    Flags;
    } __x_attribute_packed__ u;
    
    __s16    Reserved1;
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;    
    unsigned    Reserved3;
    unsigned    Reserved4;
    __s32  Reserved5Hi;
    __s32  Reserved5Low;
#if 0
    __u64    Reserved5;
#endif    

    
    unsigned    Reserved6;
    unsigned    Reserved7;
    unsigned    Reserved8;
    unsigned    Reserved9;
}__x_attribute_packed__ LANSCSI_LOGOUT_REQUEST_PDU_HEADER, *PLANSCSI_LOGOUT_REQUEST_PDU_HEADER;

/* Logout Reply. */
typedef struct _LANSCSI_LOGOUT_REPLY_PDU_HEADER {
    __u8    Opcode;
     
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    FlagReserved:7;
            __u8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    F:1;
            __u8    FlagReserved:7;
#endif            
        } __x_attribute_packed__ s;
        __u8    Flags;
    } __x_attribute_packed__ u;

    __u8    Response;
    
    __u8    Reserved1;
    
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;    
    unsigned    Reserved3;
    unsigned    Reserved4;
    __s32  Reserved5Hi;
    __s32  Reserved5Low;
#if 0
    __u64    Reserved5;
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
    __u8    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    FlagReserved:7;
            __u8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    F:1;
            __u8    FlagReserved:7;
#endif            
        } __x_attribute_packed__ s;
        __u8    Flags;
    } __x_attribute_packed__ u;
    
    __s16    Reserved1;
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    __s32  Reserved5Hi;
    __s32  Reserved5Low;
#if 0
    __u64    Reserved5;
#endif    

    
    __u8    ParameterType;
    __u8    ParameterVer;
    __s16    Reserved6;    
    
    unsigned    Reserved7;
    unsigned    Reserved8;
    unsigned    Reserved9;
}__x_attribute_packed__ LANSCSI_TEXT_REQUEST_PDU_HEADER, *PLANSCSI_TEXT_REQUEST_PDU_HEADER; 

/* Text Reply. */
typedef struct _LANSCSI_TEXT_REPLY_PDU_HEADER {
    __u8    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    FlagReserved:7;
            __u8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    F:1;
            __u8    FlagReserved:7;
#endif            
        } __x_attribute_packed__ s;
        __u8    Flags;
    } __x_attribute_packed__ u;

    __u8    Response;
    
    __u8    Reserved1;
    
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;
    
    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    __s32  Reserved5Hi;
    __s32  Reserved5Low;
#if 0
    __u64    Reserved5;
#endif    

    
    __u8    ParameterType;
    __u8    ParameterVer;
    __s16    Reserved6;
    
    unsigned    Reserved7;
    unsigned    Reserved8;
    unsigned    Reserved9;
} __x_attribute_packed__ LANSCSI_TEXT_REPLY_PDU_HEADER, *PLANSCSI_TEXT_REPLY_PDU_HEADER;
 

/*
// IDE Operation.
*/

/* IDE Request. */
typedef struct _LANSCSI_IDE_REQUEST_PDU_HEADER {
    __u8    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    FlagReserved:5;
            __u8    W:1;
            __u8    R:1;
            __u8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    F:1;
            __u8    R:1;
            __u8    W:1;
            __u8    FlagReserved:5;
#endif            
        } __x_attribute_packed__ s;
        __u8    Flags;
    } __x_attribute_packed__ u1;
    
    __s16    Reserved1;
    unsigned    HPID; // ignored
    __s16    RPID; // ignored
    __s16    CPSlot; // ignored
    unsigned    DataSegLen;
    __s16    AHSLen; // ignored
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;
    unsigned    InitiatorTaskTag; // ignored
    unsigned    DataTransferLength;    
    unsigned    TargetID;
    __s32  LUNHi;
    __s32  LUNLow;
#if 0
    __u64    LUN;
#endif    

    
    __u8    Reserved2;
    __u8    Feature;
    __u8    SectorCount_Prev;
    __u8    SectorCount_Curr;
    __u8    LBALow_Prev;
    __u8    LBALow_Curr;
    __u8    LBAMid_Prev;
    __u8    LBAMid_Curr;
    __u8    LBAHigh_Prev;
    __u8    LBAHigh_Curr;
    __u8    Reserved3;
    
    union {
        __u8    Device;
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    LBAHeadNR:4;
            __u8    DEV:1;
            __u8    obs1:1;
            __u8    LBA:1;
            __u8    obs2:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    obs2:1;
            __u8    LBA:1;
            __u8    obs1:1;
            __u8    DEV:1;
            __u8    LBAHeadNR:4;
#endif                
        } __x_attribute_packed__ s;
    }  __x_attribute_packed__ u2;

    __u8    Reserved4;
    __u8    Command;
    __s16    Reserved5;

} __x_attribute_packed__ LANSCSI_IDE_REQUEST_PDU_HEADER, *PLANSCSI_IDE_REQUEST_PDU_HEADER;

/* IDE Reply. */
typedef struct _LANSCSI_IDE_REPLY_PDU_HEADER {
    __u8    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    FlagReserved:5;
            __u8    W:1;
            __u8    R:1;
            __u8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    F:1;
            __u8    R:1;
            __u8    W:1;
            __u8    FlagReserved:5;
#endif            
        } __x_attribute_packed__ s;
        __u8    Flags;
    } __x_attribute_packed__ u1;

    __u8    Response;
    
    __u8    Status;
    
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;
    unsigned    InitiatorTaskTag;
    unsigned    DataTransferLength;    
    unsigned    TargetID;
    __s32  LUNHi;
    __s32  LUNLow;
#if 0
    __u64    LUN;
#endif    

    
    __u8    Reserved2;
    __u8    Feature;
    __u8    SectorCount_Prev;
    __u8    SectorCount_Curr;
    __u8    LBALow_Prev;
    __u8    LBALow_Curr;
    __u8    LBAMid_Prev;
    __u8    LBAMid_Curr;
    __u8    LBAHigh_Prev;
    __u8    LBAHigh_Curr;
    __u8    Reserved3;

    union {
        __u8    Device;
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    LBAHeadNR:4;
            __u8    DEV:1;
            __u8    obs1:1;
            __u8    LBA:1;
            __u8    obs2:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    obs2:1;
            __u8    LBA:1;
            __u8    obs1:1;
            __u8    DEV:1;
            __u8    LBAHeadNR:4;
#endif                
        }  __x_attribute_packed__ s;
    } __x_attribute_packed__ u2; 

    __u8    Reserved4;
    __u8    Command;
    __s16    Reserved5;
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
    __u8    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)    
            __u8    FlagReserved:7;
            __u8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    F:1;
            __u8    FlagReserved:7;
#endif            
        } __x_attribute_packed__ s;
        __u8    Flags;
    }  __x_attribute_packed__ u;
    
    __s16    Reserved1;
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;

    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    __u64    Reserved5;
    
    __u16    VenderID;
    __u8    VenderOpVersion;
    __u8    VenderOp;
	union {
	    char        Bytes[8];
	    __u64		Int64;
	} __x_attribute_packed__ VendorParameter;
    
    unsigned    Reserved6;
} __x_attribute_packed__ LANSCSI_VENDOR_REQUEST_PDU_HEADER, *PLANSCSI_VENDOR_REQUEST_PDU_HEADER;

/* Vender Specific Reply.*/
typedef struct _LANSCSI_VENDOR_REPLY_PDU_HEADER {
    __u8    Opcode;
    
    /* Flags. */
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    FlagReserved:7;
            __u8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    F:1;
            __u8    FlagReserved:7;
#endif
        } __x_attribute_packed__ s;
        __u8    Flags;
    } __x_attribute_packed__ u;

    __u8    Response;
    
    __u8    Reserved1;
    
    unsigned    HPID;
    __s16    RPID;
    __s16    CPSlot;
    unsigned    DataSegLen;
    __s16    AHSLen;
    __s16    CSubPacketSeq;
    unsigned    PathCommandTag;
    
    unsigned    Reserved2;
    unsigned    Reserved3;
    unsigned    Reserved4;
    __u64    Reserved5;
    
    __u16    VenderID;
    __u8    VenderOpVersion;
    __u8    VenderOp;

    union {
        char        Bytes[8];
        __u64     Int64;
    } __x_attribute_packed__ VendorParameter;
    
    unsigned    Reserved6;
} __x_attribute_packed__ LANSCSI_VENDOR_REPLY_PDU_HEADER, *PLANSCSI_VENDOR_REPLY_PDU_HEADER
;

// IDE Request.
typedef struct _LANSCSI_IDE_REQUEST_PDU_HEADER_V1 {
    __u8    Opcode;            // Byte 0
    
    // Flags.
    union {                    // Byte 1
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
            __u8    FlagReserved:5;
            __u8    W:1;
            __u8    R:1;
            __u8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    F:1;
            __u8    R:1;
            __u8    W:1;
            __u8    FlagReserved:5;
#endif
        } __x_attribute_packed__ s;
        __u8    Flags;
    } __x_attribute_packed__ u1;
    
    __u16    Reserved1;    // Byte 2
    __u32    HPID;        // Byte 4
    __u16    RPID;        // Byte 8
    __u16    CPSlot;        // Byte 10
    __u32    DataSegLen;// Byte 12
    __u16    AHSLen;        // Byte 16
    __u16    CSubPacketSeq;// Byte 18
    __u32    PathCommandTag;// Byte 20
    __u32    InitiatorTaskTag;// Byte 24
    __u32    DataTransferLength;    // Byte 28
    __u32    TargetID;// Byte 32
    __u32    LUNHi;// Byte 36
    __u32    LUNLow;// Byte 40

    union {
#if defined(__LITTLE_ENDIAN_BITFIELD)    // Byte 44
        __u32 COM_Reserved : 2;
        __u32 COM_TYPE_E : 1;
        __u32 COM_TYPE_R : 1;
        __u32 COM_TYPE_W : 1;
        __u32 COM_TYPE_D_P : 1;
        __u32 COM_TYPE_K : 1;
        __u32 COM_TYPE_P : 1;
#elif defined(__BIG_ENDIAN_BITFIELD)
        __u32 COM_TYPE_P : 1;
        __u32 COM_TYPE_K : 1;
        __u32 COM_TYPE_D_P : 1;
        __u32 COM_TYPE_W : 1;
        __u32 COM_TYPE_R : 1;
        __u32 COM_TYPE_E : 1;
        __u32 COM_Reserved : 2;
#endif
        __u32   COM_LENG:24;

        __u32 CommTypeLength;
    } Comm;

    __u8    Feature_Prev;  // Byte 48
    __u8    Feature_Curr;  // Byte 49
    __u8    SectorCount_Prev; // Byte 50
    __u8    SectorCount_Curr; // Byte 51
    __u8    LBALow_Prev;// Byte 52
    __u8    LBALow_Curr;// Byte 53
    __u8    LBAMid_Prev;// Byte 54
    __u8    LBAMid_Curr;// Byte 55
    __u8    LBAHigh_Prev;// Byte 56
    __u8    LBAHigh_Curr;// Byte 57
    __u8    Command;// Byte 58
    
    union {
        __u8    Device;// Byte 59
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    LBAHeadNR:4;
            __u8    DEV:1;
            __u8    obs1:1;
            __u8    LBA:1;
            __u8    obs2:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    obs2:1;
            __u8    LBA:1;
            __u8    obs1:1;
            __u8    DEV:1;
            __u8    LBAHeadNR:4;
#endif            
        } __x_attribute_packed__ s;
    } __x_attribute_packed__ u2;

} __x_attribute_packed__ LANSCSI_IDE_REQUEST_PDU_HEADER_V1, *PLANSCSI_IDE_REQUEST_PDU_HEADER_V1;

// IDE Reply.
typedef struct _LANSCSI_IDE_REPLY_PDU_HEADER_V1 {
    __u8    Opcode;
    
    // Flags.
    union {
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)            
            __u8    FlagReserved:5;
            __u8    W:1;
            __u8    R:1;
            __u8    F:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    F:1;
            __u8    R:1;
            __u8    W:1;
            __u8    FlagReserved:5;
#endif
        } __x_attribute_packed__ s;
        __u8    Flags;
    }__x_attribute_packed__ u1;

    __u8    Response;
    
    __u8    Status;
    
    __u32    HPID;
    __u16    RPID;
    __u16    CPSlot;
    __u32    DataSegLen;
    __u16    AHSLen;
    __u16    CSubPacketSeq;
    __u32    PathCommandTag;
    __u32    InitiatorTaskTag;
    __u32    DataTransferLength;    
    __u32    TargetID;
    __u64    LUN;
    union {
#if defined(__LITTLE_ENDIAN_BITFIELD)    
        __u32 COM_Reserved : 2;
        __u32 COM_TYPE_E : 1;
        __u32 COM_TYPE_R : 1;
        __u32 COM_TYPE_W : 1;
        __u32 COM_TYPE_D_P : 1;
        __u32 COM_TYPE_K : 1;
        __u32 COM_TYPE_P : 1;
        __u32 COM_LENG : 24;
#elif defined(__BIG_ENDIAN_BITFIELD)
        __u32 COM_LENG : 24;
        __u32 COM_TYPE_P : 1;
        __u32 COM_TYPE_K : 1;
        __u32 COM_TYPE_D_P : 1;
        __u32 COM_TYPE_W : 1;
        __u32 COM_TYPE_R : 1;
        __u32 COM_TYPE_E : 1;
        __u32 COM_Reserved : 2;
#endif
        __u32 CommTypeLength;
    } Comm;
    __u8    Feature_Prev;
    __u8    Feature_Curr;
    __u8    SectorCount_Prev;
    __u8    SectorCount_Curr;
    __u8    LBALow_Prev;
    __u8    LBALow_Curr;
    __u8    LBAMid_Prev;
    __u8    LBAMid_Curr;
    __u8    LBAHigh_Prev;
    __u8    LBAHigh_Curr;
    __u8    Command;
    
    union {
        __u8    Device;
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
            __u8    LBAHeadNR:4;
            __u8    DEV:1;
            __u8    obs1:1;
            __u8    LBA:1;
            __u8    obs2:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
            __u8    obs2:1;
            __u8    LBA:1;
            __u8    obs1:1;
            __u8    DEV:1;
            __u8    LBAHeadNR:4;
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

#endif

