/*****************************************************************************
 * ioctl.h: DVD ioctl replacement function
 *****************************************************************************
 * Copyright (C) 1999-2001 VideoLAN
 * $Id: ioctl.h,v 1.14 2003/01/16 22:58:29 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@zoy.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

int ioctl_ReadCopyright     ( int, int, int * );
int ioctl_ReadDiscKey       ( int, int *, uint8_t * );
int ioctl_ReadTitleKey      ( int, int *, int, uint8_t * );
int ioctl_ReportAgid        ( int, int * );
int ioctl_ReportChallenge   ( int, int *, uint8_t * );
int ioctl_ReportKey1        ( int, int *, uint8_t * );
int ioctl_ReportASF         ( int, int *, int * );
int ioctl_InvalidateAgid    ( int, int * );
int ioctl_SendChallenge     ( int, int *, uint8_t * );
int ioctl_SendKey2          ( int, int *, uint8_t * );
int ioctl_ReportRPC         ( int, int *, int *, int * );
int ioctl_SendRPC           ( int, int );

#define DVD_KEY_SIZE 5
#define DVD_CHALLENGE_SIZE 10
#define DVD_DISCKEY_SIZE 2048


/*****************************************************************************
 * Common macro, win32 specific
 *****************************************************************************/
#ifdef ILGU_WINODWS
#define INIT_SPTD( TYPE, SIZE ) \
    DWORD tmp; \
    SCSI_PASS_THROUGH_DIRECT sptd; \
    uint8_t p_buffer[ (SIZE) ]; \
    memset( &sptd, 0, sizeof( SCSI_PASS_THROUGH_DIRECT ) ); \
    sptd.Length = sizeof( SCSI_PASS_THROUGH_DIRECT ); \
    sptd.DataBuffer = p_buffer; \
    sptd.DataTransferLength = (SIZE); \
    WinInitSPTD( &sptd, (TYPE) );
#define SEND_SPTD( DEV, SPTD, TMP ) \
    (DeviceIoControl( (HANDLE)(DEV), IOCTL_SCSI_PASS_THROUGH_DIRECT, \
                      (SPTD), sizeof( SCSI_PASS_THROUGH_DIRECT ), \
                      (SPTD), sizeof( SCSI_PASS_THROUGH_DIRECT ), \
                      (TMP), NULL ) ? 0 : -1)
#define INIT_SSC( TYPE, SIZE ) \
    struct SRB_ExecSCSICmd ssc; \
    uint8_t p_buffer[ (SIZE)+1 ]; \
    memset( &ssc, 0, sizeof( struct SRB_ExecSCSICmd ) ); \
    ssc.SRB_BufPointer = (char *)p_buffer; \
    ssc.SRB_BufLen = (SIZE); \
    WinInitSSC( &ssc, (TYPE) );


/*****************************************************************************
 * Various DVD I/O tables
 *****************************************************************************/
    /* The generic packet command opcodes for CD/DVD Logical Units,
     * From Table 57 of the SFF8090 Ver. 3 (Mt. Fuji) draft standard. */
#   define GPCMD_READ_DVD_STRUCTURE 0xad
#   define GPCMD_REPORT_KEY         0xa4
#   define GPCMD_SEND_KEY           0xa3
    /* DVD struct types */
#   define DVD_STRUCT_PHYSICAL      0x00
#   define DVD_STRUCT_COPYRIGHT     0x01
#   define DVD_STRUCT_DISCKEY       0x02
#   define DVD_STRUCT_BCA           0x03
#   define DVD_STRUCT_MANUFACT      0x04
    /* Key formats */
#   define DVD_REPORT_AGID          0x00
#   define DVD_REPORT_CHALLENGE     0x01
#   define DVD_SEND_CHALLENGE       0x01
#   define DVD_REPORT_KEY1          0x02
#   define DVD_SEND_KEY2            0x03
#   define DVD_REPORT_TITLE_KEY     0x04
#   define DVD_REPORT_ASF           0x05
#   define DVD_SEND_RPC             0x06
#   define DVD_REPORT_RPC           0x08
#   define DVD_INVALIDATE_AGID      0x3f

/*****************************************************************************
 * win32 ioctl specific
 *****************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define IOCTL_DVD_START_SESSION         CTL_CODE(FILE_DEVICE_DVD, 0x0400, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_READ_KEY              CTL_CODE(FILE_DEVICE_DVD, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_SEND_KEY              CTL_CODE(FILE_DEVICE_DVD, 0x0402, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_END_SESSION           CTL_CODE(FILE_DEVICE_DVD, 0x0403, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_GET_REGION            CTL_CODE(FILE_DEVICE_DVD, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_SEND_KEY2             CTL_CODE(FILE_DEVICE_DVD, 0x0406, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DVD_READ_STRUCTURE        CTL_CODE(FILE_DEVICE_DVD, 0x0450, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE(FILE_DEVICE_CONTROLLER, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define DVD_CHALLENGE_KEY_LENGTH        (12 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_BUS_KEY_LENGTH              (8 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_TITLE_KEY_LENGTH            (8 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_DISK_KEY_LENGTH             (2048 + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_RPC_KEY_LENGTH              (sizeof(DVD_RPC_KEY) + sizeof(DVD_COPY_PROTECT_KEY))
#define DVD_ASF_LENGTH                  (sizeof(DVD_ASF) + sizeof(DVD_COPY_PROTECT_KEY))

#define DVD_COPYRIGHT_MASK              0x00000040
#define DVD_NOT_COPYRIGHTED             0x00000000
#define DVD_COPYRIGHTED                 0x00000040

#define DVD_SECTOR_PROTECT_MASK         0x00000020
#define DVD_SECTOR_NOT_PROTECTED        0x00000000
#define DVD_SECTOR_PROTECTED            0x00000020

#define SCSI_IOCTL_DATA_OUT             0
#define SCSI_IOCTL_DATA_IN              1

typedef ULONG DVD_SESSION_ID, *PDVD_SESSION_ID;

typedef enum DVD_STRUCTURE_FORMAT {
    DvdPhysicalDescriptor,
    DvdCopyrightDescriptor,
    DvdDiskKeyDescriptor,
    DvdBCADescriptor,
    DvdManufacturerDescriptor,
    DvdMaxDescriptor
} DVD_STRUCTURE_FORMAT, *PDVD_STRUCTURE_FORMAT;

typedef struct DVD_READ_STRUCTURE {
    LARGE_INTEGER BlockByteOffset;
    DVD_STRUCTURE_FORMAT Format;
    DVD_SESSION_ID SessionId;
    UCHAR LayerNumber;
} DVD_READ_STRUCTURE, *PDVD_READ_STRUCTURE;

typedef struct _DVD_COPYRIGHT_DESCRIPTOR {
    UCHAR CopyrightProtectionType;
    UCHAR RegionManagementInformation;
    USHORT Reserved;
} DVD_COPYRIGHT_DESCRIPTOR, *PDVD_COPYRIGHT_DESCRIPTOR;

typedef enum
{
    DvdChallengeKey = 0x01,
    DvdBusKey1,
    DvdBusKey2,
    DvdTitleKey,
    DvdAsf,
    DvdSetRpcKey = 0x6,
    DvdGetRpcKey = 0x8,
    DvdDiskKey = 0x80,
    DvdInvalidateAGID = 0x3f
} DVD_KEY_TYPE;

typedef struct _DVD_COPY_PROTECT_KEY
{
    ULONG KeyLength;
    DVD_SESSION_ID SessionId;
    DVD_KEY_TYPE KeyType;
    ULONG KeyFlags;
    union
    {
        struct
        {
            ULONG FileHandle;
            ULONG Reserved;   // used for NT alignment
        };
        LARGE_INTEGER TitleOffset;
    } Parameters;
    UCHAR KeyData[0];
} DVD_COPY_PROTECT_KEY, *PDVD_COPY_PROTECT_KEY;

typedef struct _DVD_ASF
{
    UCHAR Reserved0[3];
    UCHAR SuccessFlag:1;
    UCHAR Reserved1:7;
} DVD_ASF, * PDVD_ASF;

typedef struct _DVD_RPC_KEY
{
    UCHAR UserResetsAvailable:3;
    UCHAR ManufacturerResetsAvailable:3;
    UCHAR TypeCode:2;
    UCHAR RegionMask;
    UCHAR RpcScheme;
    UCHAR Reserved2[1];
} DVD_RPC_KEY, * PDVD_RPC_KEY;

typedef struct _SCSI_PASS_THROUGH_DIRECT
{
    USHORT Length;
    UCHAR ScsiStatus;
    UCHAR PathId;
    UCHAR TargetId;
    UCHAR Lun;
    UCHAR CdbLength;
    UCHAR SenseInfoLength;
    UCHAR DataIn;
    ULONG DataTransferLength;
    ULONG TimeOutValue;
    PVOID DataBuffer;
    ULONG SenseInfoOffset;
    UCHAR Cdb[16];
} SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

#endif


