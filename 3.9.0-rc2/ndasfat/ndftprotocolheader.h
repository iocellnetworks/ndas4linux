#ifndef __NDFT_PROTOCOL_HEADER_H__
#define __NDFT_PROTOCOL_HEADER_H__

#define	DEFAULT_NDFT_PORT				((USHORT)0x0003)

//	NDFT current version

#define	NDFT_MAJVER		((USHORT)(0x0000))
#define	NDFT_MINVER		((USHORT)(0x0004))

// OS's types

#define OSTYPE_WINDOWS			0x0001
#define		OSTYPE_WIN98SE		0x0001
#define		OSTYPE_WIN2K		0x0002
#define		OSTYPE_WINXP		0x0003
#define		OSTYPE_WIN2003SERV	0x0004

#define	OSTYPE_LINUX			0x0002
#define		OSTYPE_LINUX22		0x0001
#define		OSTYPE_LINUX24		0x0002

#define	OSTYPE_MAC				0x0003
#define		OSTYPE_MACOS9		0x0001
#define		OSTYPE_MACOSX		0x0002


//	packet type masks

#define NDFTPKTTYPE_PREFIX		((USHORT)(0xF000))
#define	NDFTPKTTYPE_MAJTYPE		((USHORT)(0x0FF0))
#define	NDFTPKTTYPE_MINTYPE		((USHORT)(0x000F))

//	packet type prefixes

#define	NDFTPKTTYPE_REQUEST		((USHORT)(0x1000))	//	request packet
#define	NDFTPKTTYPE_REPLY		((USHORT)(0x2000))	//	reply packet
#define	NDFTPKTTYPE_DATAGRAM	((USHORT)(0x4000))	//	datagram packet
#define	NDFTPKTTYPE_BROADCAST	((USHORT)(0x8000))	//	broadcasting packet


typedef struct __NDFT_HEADER {

#define NDFT_PROTOCOL	{ 'N', 'D', 'F', 'T' }

	__u8	Protocol[4];
	
	__u16	NdftMajorVersion2;
	__u16	NdftMinorVersion2;
	
	__u16	OsMajorType2;
	__u16	OsMinorType2;

	__u16	Type2;

	__u16	Reserved;

	__u32	MessageSize4;

} __attribute__((packed)) NDFT_HEADER, *PNDFT_HEADER;

#define NDSC_ID_LENGTH	16

typedef struct _NDFT_PRIMARY_UPDATE {

	__u8	Type1;

	__u8	Reserved1;				// 2 bytes

	__u8	PrimaryNode[6];			// 8 bytes
	__u16	PrimaryPort;
	
	__u8	NetdiskNode[6];			// 16 bytes
	__u16	NetdiskPort;
	__u16	UnitDiskNo;				// 20 bytes
	
	__u8	NdscId[NDSC_ID_LENGTH];	// 36 bytes

} __attribute__((packed)) NDFT_PRIMARY_UPDATE, *PNDFT_PRIMARY_UPDATE;

#endif
