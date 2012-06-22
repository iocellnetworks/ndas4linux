#ifndef _BINARY_PARAMETERS_H_
#define _BINARY_PARAMETERS_H_

#ifdef NDAS_EMU

#define MAX_NR_UNIT    4
/*
// Parameter Types.
*/
#define PARAMETER_CURRENT_VERSION    0

#define    BIN_PARAM_TYPE_SECURITY        0x1
#define    BIN_PARAM_TYPE_NEGOTIATION    0x2
#define    BIN_PARAM_TYPE_TARGET_LIST    0x3
#define    BIN_PARAM_TYPE_TARGET_DATA    0x4

#define BIN_PARAM_SIZE_LOGIN_FIRST_REQUEST        4
#define    BIN_PARAM_SIZE_LOGIN_SECOND_REQUEST        8
#define BIN_PARAM_SIZE_LOGIN_THIRD_REQUEST        32
#define BIN_PARAM_SIZE_LOGIN_FOURTH_REQUEST        28
#define    BIN_PARAM_SIZE_TEXT_TARGET_LIST_REQUEST        4
#define    BIN_PARAM_SIZE_TEXT_TARGET_DATA_REQUEST        16

#define BIN_PARAM_SIZE_LOGIN_FIRST_REPLY        4
#define    BIN_PARAM_SIZE_LOGIN_SECOND_REPLY        36
#define BIN_PARAM_SIZE_LOGIN_THIRD_REPLY        4
#define BIN_PARAM_SIZE_LOGIN_FOURTH_REPLY        28
#define    BIN_PARAM_SIZE_TEXT_TARGET_LIST_REPLY        4
#define    BIN_PARAM_SIZE_TEXT_TARGET_DATA_REPLY        36

#define BIN_PARAM_SIZE_REPLY                36

typedef struct _BIN_PARAM {
    xuchar    ParamType;
    xuchar    Reserved1;
    xint16    Reserved2;
} __x_attribute_packed__ BIN_PARAM, *PBIN_PARAM;
/*
// BIN_PARAM_TYPE_SECURITY.
*/

#define    LOGIN_TYPE_NORMAL        0x00
#define    LOGIN_TYPE_DISCOVERY        0xFF

/*
// CHAP ( Challenge Handshake Authentication Protocol ) Auth Paramter.
*/
#define    AUTH_METHOD_CHAP        0x0001

#define FIRST_TARGET_RO_USER    0x00000001u
#define FIRST_TARGET_RW_USER    0x00010001u
#define SECOND_TARGET_RO_USER    0x00000002u
#define SECOND_TARGET_RW_USER    0x00020002u
#define    SUPERVISOR                0xFFFFFFFFu

typedef struct _AUTH_PARAMETER_CHAP {
    xuint32 CHAP_A;         // algorithm type
    xuint32 CHAP_I;         // Identifier
    xuint32 CHAP_N;         // Name ( User ID )
    char        CHAP_R[4][4]; // Response
    char        CHAP_C[4]; // CHAP chalenge
} __x_attribute_packed__ AUTH_PARAMETER_CHAP, *PAUTH_PARAMETER_CHAP ;

#define HASH_ALGORITHM_MD5            0x00000005
#define CHAP_MAX_CHALLENGE_LENGTH    4

typedef struct _BIN_PARAM_SECURITY {
    xuchar    ParamType;
    xuchar    LoginType;
    xint16    AuthMethod;
    union {
        xuchar    AuthParameter[1];    /* Variable Length. */
        AUTH_PARAMETER_CHAP ChapParam;
    } __x_attribute_packed__ u;
} __x_attribute_packed__ BIN_PARAM_SECURITY, *PBIN_PARAM_SECURITY;

/*
// BIN_PARAM_TYPE_NEGOTIATION.    
*/
typedef struct _BIN_PARAM_NEGOTIATION {
    xuchar            ParamType;
    xuchar            HWType;
    xuchar            HWVersion;
    xuchar            Reserved;
    xuint32        NRSlot;
    xuint32        MaxBlocks;
    xuint32        MaxTargetID;
    xuint32        MaxLUNID;
    xuint16    HeaderEncryptAlgo;
    xuint16    HeaderDigestAlgo;
    xuint16    DataEncryptAlgo;
    xuint16    DataDigestAlgo;
} __x_attribute_packed__ BIN_PARAM_NEGOTIATION, *PBIN_PARAM_NEGOTIATION ;

#define HW_TYPE_ASIC            0x00
#define HW_TYPE_UNSPECIFIED        0xFF

#define HW_VERSION_CURRENT        0x00
#define HW_VERSION_UNSPECIFIIED    0xFF

/*
// BIN_PARAM_TYPE_TARGET_LIST.
*/
typedef    struct _BIN_PARAM_TARGET_LIST_ELEMENT {
    xuint32    TargetID;
    xuint8            NRRWHost;
    xuint8            NRROHost;
    xint16            Reserved1;
    xuint32    TargetDataHi;
    xuint32    TargetDataLow;
} __x_attribute_packed__ BIN_PARAM_TARGET_LIST_ELEMENT, *PBIN_PARAM_TARGET_LIST_ELEMENT ;

typedef struct _BIN_PARAM_TARGET_LIST {
    xuint8    ParamType;
    xuint8    NRTarget;
    xint16    Reserved1;
    BIN_PARAM_TARGET_LIST_ELEMENT    PerTarget[MAX_NR_UNIT];
} __x_attribute_packed__ BIN_PARAM_TARGET_LIST, *PBIN_PARAM_TARGET_LIST ;

/*
// BIN_PARAM_TYPE_TARGET_DATA.
*/

#define    PARAMETER_OP_GET    0x00
#define    PARAMETER_OP_SET    0xFF

typedef struct _BIN_PARAM_TARGET_DATA {
    xuint8    ParamType;
    xuint8    GetOrSet;
    xuint16    Reserved1;
    xuint32    TargetID;
    xuint32    TargetDataHi;
    xuint32    TargetDataLow;
} __x_attribute_packed__ BIN_PARAM_TARGET_DATA, *PBIN_PARAM_TARGET_DATA  ;

#ifdef DEBUG
#define __BIN_PARAM_TYPE(x)  \
({ \
    static const char* __bin_param_type__[] ={ \
        "0", "SECURITY", "NEGOTIATION", "TARGET_LIST", "TARGET_DATA" \
    };\
    ( (x) <= '\4' ) ? \
        __bin_param_type__[x]: \
        ({ \
            static char _b_[5];  \
            sal_snprintf(_b_,sizeof(_b_),"0x%02x",x); \
            (const char *) _b_;\
        }); \
})
#define DEBUG_AUTH_PARAMETER_CHAP(x) \
({ \
    static char __buf__[1024]; \
    PAUTH_PARAMETER_CHAP __pauth = (x); \
    if ( __pauth ) \
        sal_snprintf(__buf__,sizeof(__buf__),\
            "AUTH_PARAMETER_CHAP(%p)={CHAP_A/algo=%s," \
            "CHAP_I/challenge=0x%x," \
            "CHAP_N/userid=0x%x," \
            "CHAP_R=%s,CHAP_C=%s}", \
            __pauth, \
            g_ntohl(__pauth->CHAP_A) == HASH_ALGORITHM_MD5 ? \
                "HASH_ALGORITHM_MD5" : \
                ({ \
                    static char _b_[9];  \
                    sal_snprintf(_b_,sizeof(_b_),"0x%08x",g_ntohl(__pauth->CHAP_A)); \
                    (const char *) _b_;\
                }), \
            g_ntohl(__pauth->CHAP_I),\
            g_ntohl(__pauth->CHAP_N),\
            SAL_DEBUG_HEXDUMP((char*)__pauth->CHAP_R, 4*4), \
            SAL_DEBUG_HEXDUMP(__pauth->CHAP_C, 4) \
        );\
    else \
        sal_snprintf(__buf__,sizeof(__buf__), "NULL");\
    (const char *) __buf__; \
})
#else
#define DEBUG_AUTH_PARAMETER_CHAP(x) ({ ""; })
#endif // DEBUG

#ifdef DEBUG
#define DEBUG_BIN_PARAM_SECURITY(x) \
({ \
    static char __buf__[1024]; \
    PBIN_PARAM_SECURITY __p_secu = (x); \
    if ( __p_secu ) \
        sal_snprintf(__buf__,sizeof(__buf__),\
            "BIN_PARAM_SECURITY(%p)={"\
            "ParamType=%s,"\
            "LoginType=0x%x,"\
            "AuthMethod=0x%x,"\
            "ChapParam=%s}", \
            __p_secu, \
            __BIN_PARAM_TYPE(__p_secu->ParamType),\
            __p_secu->LoginType,\
            g_ntohs(__p_secu->AuthMethod), \
            DEBUG_AUTH_PARAMETER_CHAP(&(__p_secu->u.ChapParam)) \
        );\
    else \
        sal_snprintf(__buf__,sizeof(__buf__), "NULL");\
    (const char *) __buf__; \
})
#else
#define DEBUG_BIN_PARAM_SECURITY(x) ({ ""; })
#endif // DEBUG
#ifdef DEBUG
#define DEBUG_BIN_PARAM_NEGOTIATION(x) \
({ \
    static char __buf__[1024]; \
    PBIN_PARAM_NEGOTIATION __pnego = (x); \
    if ( __pnego ) \
        sal_snprintf(__buf__,sizeof(__buf__),\
            "(%p)={"\
            "ParamType=%s,"\
            "HWType=0x%x,"\
            "HWVersion=0x%x,"\
            "NRSlot=0x%x," \
            "MaxBlocks=0x%x," \
            "MaxTargetID=0x%x,"\
            "MaxLUNID=0x%x," \
            "HeaderEncryptAlgo=0x%x,"\
            "HeaderDigestAlgo=0x%x,"\
            "DataEncryptAlgo=0x%x,"\
            "DataDigestAlgo=0x%x}", \
            __pnego, \
            __BIN_PARAM_TYPE(__pnego->ParamType),\
            __pnego->HWType, \
            __pnego->HWVersion, \
            g_ntohl(__pnego->NRSlot), \
            g_ntohl(__pnego->MaxBlocks), \
            g_ntohl(__pnego->MaxTargetID), \
            g_ntohl(__pnego->MaxLUNID), \
            g_ntohs(__pnego->HeaderEncryptAlgo), \
            g_ntohs(__pnego->HeaderDigestAlgo), \
            g_ntohs(__pnego->DataEncryptAlgo), \
            g_ntohs(__pnego->DataDigestAlgo) \
        );\
    else \
        sal_snprintf(__buf__,sizeof(__buf__), "NULL");\
    (const char *) __buf__; \
})
#else
#define DEBUG_BIN_PARAM_NEGOTIATION(x) ({ ""; })
#endif // DEBUG
#endif

#endif

