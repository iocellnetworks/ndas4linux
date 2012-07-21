#ifndef _NDAS_DIB_H_
#define _NDAS_DIB_H_

#include <sal/types.h>
//
//    disk information format
//
//

/* Turn 1-byte structure alignment on */
/* Use poppack.h to restore previous or default alignment */

//
// cslee:
//
// Disk Information Block should be aligned to 512-byte (1 sector size)
//
// Checksum has not been used yet, so we can safely fix the size of
// the structure at this time without increasing the version of the block structure
//
// Note when you change members of the structure, you should increase the
// version of the structure and you SHOULD provide the migration
// path from the previous version.
// 

static const int SECTOR_SIZE = 512;

typedef xuint64 NDAS_BLOCK_LOCATION;

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_DIB_V1        = -1;        // Disk Information Block V1
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_DIB_V2        = -2;        // Disk Information Block V2
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_ENCRYPT    = -3;        // Content encryption information
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_2ND_RMD        = -4;        // RAID Meta data
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_1ST_RMD        = -5;        // RAID Meta data(for transaction)
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_OEM        = -0x0100;    // OEM Reserved
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_ADD_BIND    = -0x0200;    // Additional bind informations
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_BITMAP        = -0x0f00;    // Corruption Bitmap(Optional)
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_WRITE_LOG    = -0x1000;    // Last written sector(Optional)

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_XAREA            = 0x1000;    // Total X Area Size
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_SIZE_BITMAP            = 0x0800;    // Corruption Bitmap(Optional)

static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_MBR        = 0;        // Master Boot Record
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_USER        = 0x80;        // Partion 1 starts here
static const NDAS_BLOCK_LOCATION NDAS_BLOCK_LOCATION_LDM        = 0;        // depends on sizeUserSpace, last 1MB of user space

typedef xuint8 NDAS_DIB_DISK_TYPE;

#define NDAS_DIB_DISK_TYPE_SINGLE                0
#define NDAS_DIB_DISK_TYPE_MIRROR_MASTER        1
#define NDAS_DIB_DISK_TYPE_AGGREGATION_FIRST    2
#define NDAS_DIB_DISK_TYPE_MIRROR_SLAVE            11
#define NDAS_DIB_DISK_TYPE_AGGREGATION_SECOND    21
#define NDAS_DIB_DISK_TYPE_AGGREGATION_THIRD    22
#define NDAS_DIB_DISK_TYPE_AGGREGATION_FOURTH    23
//#define NDAS_DIB_DISK_TYPE_DVD        31
//#define NDAS_DIB_DISK_TYPE_VDVD            32
//#define NDAS_DIB_DISK_TYPE_MO            33
//#define NDAS_DIB_DISK_TYPE_FLASHCATD    34
#define NDAS_DIB_DISK_TYPE_INVALID    0x80
#define NDAS_DIB_DISK_TYPE_BIND        0xC0
//#define NDAS_DIB_DISK_TYPE_MEDIAJUKE				104

// extended type information is stored in DIBEXT
#define NDAS_DIB_DISK_TYPE_EXTENDED                80

typedef xuint8 NDAS_DIBEXT_DISK_TYPE;

static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_SINGLE = 0;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_MASTER = 1;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_AGGREGATION_FIRST    = 2;
static const NDAS_DIBEXT_DISK_TYPE NDAS_DIBEXT_DISK_TYPE_MIRROR_SLAVE = 11;

typedef unsigned char NDAS_DIB_DISKTYPE;
typedef unsigned char NDAS_DIBEXT_DISKTYPE;

//
// obsolete types (NDAS_DIB_USAGE_TYPE)
//
#define NDAS_DIB_USAGE_TYPE_HOME    0x00
#define NDAS_DIB_USAGE_TYPE_OFFICE    0x10

static const xint32 NDAS_DIB_SIGNATURE = 0xFE037A4E;
static const xuint8 NDAS_DIB_VERSION_MAJOR = 0;
static const xuint8 NDAS_DIB_VERSION_MINOR = 1;

typedef struct _NDAS_DIB {

    unsigned xint32    Signature;        // 4 (4)
    
    xuint8    MajorVersion;    // 1 (5)
    xuint8    MinorVersion;    // 1 (6)
    xuint8    reserved1[2];    // 1 * 2 (8)

    unsigned xint32    Sequence;        // 4 (12) ignore

    xuint8    EtherAddress[6];    // 1 * 6 (18) mine if not ignore
    xuint8    UnitNumber;        // 1 (19)
    xuint8    reserved2;        // 1 (20)

    xuint8    DiskType;        // 1 (21) 
    xuint8    PeerAddress[6];    // 1 * 6 (27)
    xuint8    PeerUnitNumber; // 1 (28)
    xuint8    reserved3;        // 1 (29)

    xuint8    UsageType;        // 1 (30) ignore
    xuint8    reserved4[3];    // 1 * 3 (33)

    unsigned char reserved5[512 - 37]; // should be 512 - ( 33 + 4 )

    xint32    Checksum;        // 4 (512) ignore

} __attribute__((packed)) NDAS_DIB, *PNDAS_DIB;

typedef struct _UNIT_DISK_LOCATION
{
    xuint8 MACAddr[6];
    xuint8 UnitNumber;
    xuint8 reserved;
} __attribute__((packed)) UNIT_DISK_LOCATION, *PUNIT_DISK_LOCATION;

#define DBG_DIB_UNITDISK(ud_loc) ({\
    static char buf[16]; \
    if ( ud_loc ) \
        sal_snprintf(buf,sizeof(buf),"%02x%02x%02x%02x%02x%02x:%d",\
            (ud_loc)->MACAddr[0],\
            (ud_loc)->MACAddr[1],\
            (ud_loc)->MACAddr[2],\
            (ud_loc)->MACAddr[3],\
            (ud_loc)->MACAddr[4],\
            (ud_loc)->MACAddr[5],\
            (ud_loc)->UnitNumber);\
    else \
        sal_snprintf(buf,sizeof(buf),"NULL");\
    buf;\
})

#define NDAS_MAX_UNITS_IN_BIND    16 // not system limit
#define NDAS_MAX_UNITS_IN_V2 32
#define NDAS_MAX_UNITS_IN_SECTOR 64

static const xuchar NDAS_DIB_V2_SIGNATURE[8] = {0x9f, 0x5f, 0x49, 0xf4, 0x9f, 0x4a, 0x40, 0x3f};

//static const xuint64 NDAS_DIB_V2_SIGNATURE = 0x3F404A9FF4495F9F;
//#define    DISK_INFORMATION_SIGNATURE_V2    0x3F404A9FF4495F9F
static const unsigned xint32 NDAS_DIB_VERSION_MAJOR_V2 = 1;
static const unsigned xint32 NDAS_DIB_VERSION_MINOR_V2 = 1;

/*
    Because prior NDAS service overwrites NDAS_BLOCK_LOCATION_DIB_V1 if Signature, version does not match,
    NDAS_DIB_V2 locates at NDAS_BLOCK_LOCATION_DIB_V2(-2).
*/
#define NMT_INVALID            0        // operation purpose only : must NOT written on disk
#define NMT_SAFE_RAID1        7        // operation purpose only : must NOT written on disk. used in bind operation only
#define NMT_VDVD            100        // virtual DVD
#define NMT_CDROM            101        // packet device, CD / DVD
#define NMT_OPMEM            102        // packet device, Magnetic Optical
#define NMT_FLASH            103        // block device, flash card

#define NMT_SINGLE            1        // unbound
#define NMT_MIRROR            2        // 2 disks without repair information. need to be converted to RAID1
#define NMT_AGGREGATE        3        // 2 or more
#define NMT_RAID0            6        // with repair
#define NMT_RAID1            4        // with repair
#define NMT_RAID4            5        // with repair
#define NMT_AOD                8        // Append only disk
#define NMT_MEDIAJUKE		104

// Disk Information Block V2
typedef struct _NDAS_DIB_V2 {
    union {
        struct  {
            xuchar    Signature[8];
            unsigned xint32    MajorVersion;
            unsigned xint32    MinorVersion;

            // sizeXArea + sizeLogicalSpace <= sizeTotalDisk
            xuint64    sizeXArea; // in sectors, always 2 * 1024 * 2 (2 MB)

            xuint64    sizeUserSpace; // in sectors

            xuint32    iSectorsPerBit; // dirty bitmap . default : 128
            xuint32    iMediaType; // NDAS Media Type
            xuint32    nDiskCount; // 1 ~ . physical disk units
            xuint32    iSequence; // 0 ~
            xuint8    AutoRecover; // Recover RAID automatically
            xuint8    Reserved0[3];
            xuint64    AodSblockLoc;
            xuint32    AodSblockLen;

            /* Per partition RW access */
            struct {
                
            } __attribute__((packed))  part[16];
        }__attribute__((packed)) s;
        unsigned char bytes_248[248];
    }__attribute__((packed)) u; // 248
    unsigned xint32 crc32; // 252
    unsigned xint32 crc32_unitdisks; // 256

    UNIT_DISK_LOCATION    UnitDisks[NDAS_MAX_UNITS_IN_V2]; // 256 bytes
} __attribute__((packed)) NDAS_DIB_V2, *PNDAS_DIB_V2;


//
// Unless ContentEncryptCRC32 matches the CRC32,
// no ContentEncryptXXX fields are valid.
//
// CRC32 is Only for ContentEncrypt Fields (LITTLE ENDIAN)
// CRC32 is calculated based on the following criteria
//
// p = ContentEncryptMethod;
// crc32(p, sizeof(ContentEncryptMethod + KeyLength + Key)
//
//
// Key is encrypted with DES to the size of
// NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH
// Unused portion of the ContentEncryptKey must be filled with 0
// before encryption
//
// UCHAR key_value[8] = {0x0B,0xBC,0xAB,0x45,0x44,0x01,0x65,0xF0};
//

#define    NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH        64        // 64 bytes. 512bits.
#define    NDAS_CONTENT_ENCRYPT_METHOD_NONE    0
#define    NDAS_CONTENT_ENCRYPT_METHOD_SIMPLE    1
#define    NDAS_CONTENT_ENCRYPT_METHOD_AES        2

#define NDAS_CONTENT_ENCRYPT_REV_MAJOR    0x0010
#define NDAS_CONTENT_ENCRYPT_REV_MINOR    0x0010

#define NDAS_CONTENT_ENCRYPT_REVISION ((NDAS_CONTENT_ENCRYPT_REV_MAJOR << 16) + NDAS_CONTENT_ENCRYPT_REV_MINOR)

typedef struct _NDAS_CONTENT_ENCRYPT_BLOCK {
    union {
        struct {
            xuint64    Signature;    // Little Endian
            // INVARIANT
            unsigned xint32 Revision;   // Little Endian
            // VARIANT BY REVISION (MAJOR)
            unsigned xint16    Method;        // Little Endian
            unsigned xint16    Reserved_0;    // Little Endian
            unsigned xint16    KeyLength;    // Little Endian
            unsigned xint16    Reserved_1;    // Little Endian
            union {
                xuint8    Key32[32 /8];
                xuint8    Key128[128 /8];
                xuint8    Key192[192 /8];
                xuint8    Key256[256 /8];
                xuint8    Key[NDAS_CONTENT_ENCRYPT_MAX_KEY_LENGTH];
            } __attribute__((packed)) u;
            xuint8 Fingerprint[16]; // 128 bit, KeyPairMD5 = md5(ks . kd)
        } __attribute__((packed)) s;
        unsigned char bytes_508[508];
    }__attribute__((packed)) u;
    unsigned xint32 CRC32;        // Little Endian
} __attribute__((packed)) NDAS_CONTENT_ENCRYPT_BLOCK, *PNDAS_CONTENT_ENCRYPT_BLOCK;

#define IS_NDAS_DIBV1_WRONG_VERSION(DIBV1) ((0 != (DIBV1).MajorVersion) || (0 == (DIBV1).MajorVersion && 1 != (DIBV1).MinorVersion))

#define IS_HIGHER_VERSION_V2(DIBV2) ((NDAS_DIB_VERSION_MAJOR_V2 < (DIBV2).MajorVersion) ||     (NDAS_DIB_VERSION_MAJOR_V2 == (DIBV2).MajorVersion &&     NDAS_DIB_VERSION_MINOR_V2 < (DIBV2).MinorVersion))

#define GET_TRAIL_SECTOR_COUNT_V2(DISK_COUNT) (    ((DISK_COUNT) > NDAS_MAX_UNITS_IN_V2) ?         (((DISK_COUNT) - NDAS_MAX_UNITS_IN_V2) / NDAS_MAX_UNITS_IN_SECTOR) +1 : 0)

// make older version should not access disks with NDAS_DIB_V2
#define NDAS_DIB_V1_INVALIDATE(DIB_V1)        \
    {    (DIB_V1).Signature        = NDAS_DIB_SIGNATURE;    \
        (DIB_V1).MajorVersion    = NDAS_DIB_VERSION_MAJOR;    \
        (DIB_V1).MinorVersion    = NDAS_DIB_VERSION_MINOR;                \
        (DIB_V1).DiskType        = NDAS_DIB_DISK_TYPE_INVALID;    }

typedef struct _LAST_WRITTEN_SECTOR {
    xuint64    logicalBlockAddress;
    xuint32    transferBlocks;
    xuint32    timeStamp;
} __attribute__((packed)) LAST_WRITTEN_SECTOR, *PLAST_WRITTEN_SECTOR;

typedef struct _LAST_WRITTEN_SECTORS {
    LAST_WRITTEN_SECTOR LWS[32];
} __attribute__((packed)) LAST_WRITTEN_SECTORS, *PLAST_WRITTEN_SECTORS;

/*
member of the NDAS_RAID_META_DATA structure
8 bytes of size
*/
#define RMD_UNIT_FAULT    0x01
#define RMD_UNIT_SPARE    0x02
typedef struct _NDAS_UNIT_META_DATA {
    xuint16    iUnitDeviceIdx; // Index in NDAS_DIB_V2.UnitDisks
    xuint8    UnitDeviceStatus; // NDAS_UNIT_META_BIND_STATUS_*
    xuint8    Reserved[5];
} __attribute__((packed)) NDAS_UNIT_META_DATA, *PNDAS_UNIT_META_DATA;

/*
NDAS_RAID_META_DATA structure contains status which changes by the change
of status of the RAID. For stable status, see NDAS_DIB_V2

NDAS_RAID_META_DATA has 512 bytes of size

NDAS_RAID_META_DATA is for Fault tolerant bind only. ATM, RAID 1 and 4.
*/


#define RMD_MAGIC 0xA01B210C10CDC301ULL

#define RMD_STATE_MOUNTED 0x00000001
#define RMD_STATE_UNMOUNTED 0x00000002

typedef struct _NDAS_RAID_META_DATA {
    union {
        struct {
            xuint64    magic;    // Little Endian
            xuint32 usn; // Universal sequence number increases 1 each writes.
            xuint32 state;
        } __attribute__((packed)) s;
        xuint8 bytes_248[248];
    } __attribute__((packed)) u; // 248
    xuint32 crc32; // 252
    xuint32 crc32_unitdisks; // 256

    NDAS_UNIT_META_DATA UnitMetaData[NDAS_MAX_UNITS_IN_V2]; // 256 bytes
} __attribute__((packed)) NDAS_RAID_META_DATA, *PNDAS_RAID_META_DATA;

#ifdef __PASS_RMD_CRC32_CHK__
#define IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) TRUE
#define IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD) TRUE
#else
#define IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) ((RMD).crc32 == CRC_FUNC((unsigned char *)&(RMD), sizeof((RMD).bytes_248)))
#define IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD) ((RMD).crc32_unitdisks == CRC_FUNC((unsigned char *)&(RMD).UnitMetaData, sizeof((RMD).UnitMetaData)))
#endif

#define IS_RMD_CRC_VALID(CRC_FUNC, RMD) (IS_RMD_DATA_CRC_VALID(CRC_FUNC, RMD) && IS_RMD_UNIT_CRC_VALID(CRC_FUNC, RMD))

#define SET_RMD_DATA_CRC(CRC_FUNC, RMD) ((RMD).crc32 = CRC_FUNC((unsigned char *)&(RMD), sizeof((RMD).bytes_248)))
#define SET_RMD_UNIT_CRC(CRC_FUNC, RMD) ((RMD).crc32_unitdisks = CRC_FUNC((unsigned char *)&(RMD).UnitMetaData, sizeof((RMD).UnitMetaData)))
#define SET_RMD_CRC(CRC_FUNC, RMD) {SET_RMD_DATA_CRC(CRC_FUNC, RMD); SET_RMD_UNIT_CRC(CRC_FUNC, RMD);}
#endif /* _NDAS_DIB_H_ */

