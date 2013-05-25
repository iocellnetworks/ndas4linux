#ifndef __NDAS_SCSI_FMT_H__
#define __NDAS_SCSI_FMT_H__

//
// Generic 6-Byte CDB
//
typedef union _CDB {
	struct _CDB6GENERIC {
	   xuint8  OperationCode;
#if defined(__LITTLE_ENDIAN_BITFIELD)       
	   xuint8  Immediate : 1;
	   xuint8  CommandUniqueBits : 4;
	   xuint8  LogicalUnitNumber : 3;
#else
	   xuint8  LogicalUnitNumber : 3;
          xuint8  CommandUniqueBits : 4;
          xuint8  Immediate : 1;
#endif
	   xuint8  CommandUniqueBytes[3];
#if defined(__LITTLE_ENDIAN_BITFIELD)       
	   xuint8  Link : 1;
	   xuint8  Flag : 1;
	   xuint8  Reserved : 4;
	   xuint8  VendorUnique : 2;
#else
	   xuint8  VendorUnique : 2;
	   xuint8  Reserved : 4;
	   xuint8  Flag : 1;       
	   xuint8  Link : 1;
#endif
	}  CDB6GENERIC;

	//
	// Standard 6-byte CDB
	//

	struct _CDB6READWRITE {
	    xuint8 OperationCode;    // 0x08, 0x0A - SCSIOP_READ, SCSIOP_WRITE
#if defined(__LITTLE_ENDIAN_BITFIELD)       	    
	    xuint8 LogicalBlockMsb1 : 5;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 LogicalBlockMsb1 : 5;
#endif
	    xuint8 LogicalBlockMsb0;
	    xuint8 LogicalBlockLsb;
	    xuint8 TransferBlocks;
	    xuint8 Control;
	}  CDB6READWRITE;

	//
	// SCSI-1 Inquiry CDB
	//

	struct _CDB6INQUIRY {
	    xuint8 OperationCode;    // 0x12 - SCSIOP_INQUIRY
#if defined(__LITTLE_ENDIAN_BITFIELD)       	    	    
	    xuint8 Reserved1 : 5;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 PageCode;
	    xuint8 IReserved;
	    xuint8 AllocationLength;
	    xuint8 Control;
	}  CDB6INQUIRY;

	//
	// SCSI-3 Inquiry CDB
	//

	struct _CDB6INQUIRY3 {
	    xuint8 OperationCode;    // 0x12 - SCSIOP_INQUIRY
#if defined(__LITTLE_ENDIAN_BITFIELD)       	    		    
	    xuint8 EnableVitalProductData : 1;
	    xuint8 CommandSupportData : 1;
	    xuint8 Reserved1 : 6;
#else
	    xuint8 Reserved1 : 6;
	    xuint8 CommandSupportData : 1;
	    xuint8 EnableVitalProductData : 1;
#endif
	    xuint8 PageCode;
	    xuint8 Reserved2;
	    xuint8 AllocationLength;
	    xuint8 Control;
	}  CDB6INQUIRY3;

	struct _CDB6VERIFY {
	    xuint8 OperationCode;    // 0x13 - SCSIOP_VERIFY
#if defined(__LITTLE_ENDIAN_BITFIELD)       	    		    
	    xuint8 Fixed : 1;
	    xuint8 ByteCompare : 1;
	    xuint8 Immediate : 1;
	    xuint8 Reserved : 2;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved : 2;
           xuint8 Immediate : 1;
           xuint8 ByteCompare : 1;
	    xuint8 Fixed : 1;
#endif
	    xuint8 VerificationLength[3];
	    xuint8 Control;
	}  CDB6VERIFY;

	//
	// SCSI Format CDB
	//

	struct _CDB6FORMAT {
	    xuint8 OperationCode;    // 0x04 - SCSIOP_FORMAT_UNIT
#if defined(__LITTLE_ENDIAN_BITFIELD)      	    
	    xuint8 FormatControl : 5;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 FormatControl : 5;
#endif
	    xuint8 FReserved1;
	    xuint8 InterleaveMsb;
	    xuint8 InterleaveLsb;
	    xuint8 FReserved2;
	}  CDB6FORMAT;

	//
	// Standard 10-byte CDB

	struct _CDB10 {
	    xuint8 OperationCode;
#if defined(__LITTLE_ENDIAN_BITFIELD)      	            
	    xuint8 RelativeAddress : 1;
	    xuint8 Reserved1 : 2;
	    xuint8 ForceUnitAccess : 1;
	    xuint8 DisablePageOut : 1;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 DisablePageOut : 1;	    
	    xuint8 ForceUnitAccess : 1;
           xuint8 Reserved1 : 2;
	    xuint8 RelativeAddress : 1;   
#endif
	    xuint8 LogicalBlockByte0;
	    xuint8 LogicalBlockByte1;
	    xuint8 LogicalBlockByte2;
	    xuint8 LogicalBlockByte3;
	    xuint8 Reserved2;
	    xuint8 TransferBlocksMsb;
	    xuint8 TransferBlocksLsb;
	    xuint8 Control;
	}  CDB10;

	//
	// Standard 12-byte CDB
	//

	struct _CDB12 {
	    xuint8 OperationCode;
#if defined(__LITTLE_ENDIAN_BITFIELD)      	                 
	    xuint8 RelativeAddress : 1;
	    xuint8 Reserved1 : 2;
	    xuint8 ForceUnitAccess : 1;
	    xuint8 DisablePageOut : 1;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 DisablePageOut : 1;
	    xuint8 ForceUnitAccess : 1;
	    xuint8 Reserved1 : 2;
	    xuint8 RelativeAddress : 1;
#endif
	    xuint8 LogicalBlock[4];
	    xuint8 TransferLength[4];
	    xuint8 Reserved2;
	    xuint8 Control;
	} CDB12;



	//
	// Standard 16-byte CDB
	//

	struct _CDB16 {
	    xuint8 OperationCode;
#if defined(__LITTLE_ENDIAN_BITFIELD)      	                         
	    xuint8 Reserved1        : 3;
	    xuint8 ForceUnitAccess  : 1;
	    xuint8 DisablePageOut   : 1;
	    xuint8 Protection       : 3;
#else
	    xuint8 Protection       : 3;
	    xuint8 DisablePageOut   : 1;
	    xuint8 ForceUnitAccess  : 1;
	    xuint8 Reserved1        : 3;
#endif
	    xuint8 LogicalBlock[8];
	    xuint8 TransferLength[4];
	    xuint8 Reserved2;
	    xuint8 Control;
	}  CDB16;



	struct _READ_BUFFER {
		xuint8 OperationCode;
  #if defined(__LITTLE_ENDIAN_BITFIELD)            
		xuint8 Mode:5;
		xuint8 Reserved:3;
  #else
  		xuint8 Reserved:3;
  		xuint8 Mode:5;
  #endif
		xuint8 BufferId;
		xuint8 BufferOffset[3];
		xuint8 AllocationLength[3];
		xuint8 Control;
	} READ_BUFFER;


	//
	// CD Rom Audio CDBs
	//

	struct _PAUSE_RESUME {
	    xuint8 OperationCode;    // 0x4B - SCSIOP_PAUSE_RESUME
#if defined(__LITTLE_ENDIAN_BITFIELD)   	    
	    xuint8 Reserved1 : 5;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 Reserved2[6];
	    xuint8 Action;
	    xuint8 Control;
	}  PAUSE_RESUME;

	//
	// Read Table of Contents
	//

	struct _READ_TOC {
	    xuint8 OperationCode;    // 0x43 - SCSIOP_READ_TOC
#if defined(__LITTLE_ENDIAN_BITFIELD)   	    	    
	    xuint8 Reserved0 : 1;
	    xuint8 Msf : 1;
	    xuint8 Reserved1 : 3;
	    xuint8 LogicalUnitNumber : 3;
        
	    xuint8 Format2 : 4;
	    xuint8 Reserved2 : 4;
#else

	    xuint8 LogicalUnitNumber : 3;
           xuint8 Reserved1 : 3;
           xuint8 Msf : 1;
	    xuint8 Reserved0 : 1;

           xuint8 Reserved2 : 4;
	    xuint8 Format2 : 4;
#endif
	    xuint8 Reserved3[3];
	    xuint8 StartingTrack;
	    xuint8 AllocationLength[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)           
	    xuint8 Control : 6;
	    xuint8 Format : 2;
#else
	    xuint8 Format : 2;
	    xuint8 Control : 6;
#endif
	}  READ_TOC;

	struct _READ_DISK_INFORMATION {
	    xuint8 OperationCode;    // 0x51 - SCSIOP_READ_DISC_INFORMATION
#if defined(__LITTLE_ENDIAN_BITFIELD)           	    
	    xuint8 Reserved1 : 5;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 Reserved2[5];
	    xuint8 AllocationLength[2];
	    xuint8 Control;
	}  READ_DISK_INFORMATION, READ_DISC_INFORMATION;

	struct _READ_TRACK_INFORMATION {
	    xuint8 OperationCode;    // 0x52 - SCSIOP_READ_TRACK_INFORMATION
#if defined(__LITTLE_ENDIAN_BITFIELD)           	   	    
	    xuint8 Track : 2;
	    xuint8 Reserved4 : 3;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved4 : 3;
	    xuint8 Track : 2;
#endif
	    xuint8 BlockAddress[4];  // or Track Number
	    xuint8 Reserved3;
	    xuint8 AllocationLength[2];
	    xuint8 Control;
	}   READ_TRACK_INFORMATION;

	struct _RESERVE_TRACK_RZONE {
	    xuint8 OperationCode;    // 0x53 - SCSIOP_RESERVE_TRACK_RZONE
	    xuint8 Reserved1[4];
	    xuint8 ReservationSize[4];
	    xuint8 Control;
	}   RESERVE_TRACK_RZONE;

	struct _SEND_OPC_INFORMATION {
	    xuint8 OperationCode;    // 0x54 - SCSIOP_SEND_OPC_INFORMATION
#if defined(__LITTLE_ENDIAN_BITFIELD)       	    
	    xuint8 DoOpc    : 1;     // perform OPC
	    xuint8 Reserved : 7;
#else
	    xuint8 Reserved : 7;
	    xuint8 DoOpc    : 1;     // perform OPC
#endif
	    xuint8 Reserved1[5];
	    xuint8 ParameterListLength[2];
	    xuint8 Reserved2;
	}   SEND_OPC_INFORMATION;

	struct _REPAIR_TRACK {
	    xuint8 OperationCode;    // 0x58 - SCSIOP_REPAIR_TRACK
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 Immediate : 1;
	    xuint8 Reserved1 : 7;
#else
	    xuint8 Reserved1 : 7;
	    xuint8 Immediate : 1;
#endif
	    xuint8 Reserved2[2];
	    xuint8 TrackNumber[2];
	    xuint8 Reserved3[3];
	    xuint8 Control;
	}   REPAIR_TRACK;

	struct _CLOSE_TRACK {
	    xuint8 OperationCode;    // 0x5B - SCSIOP_CLOSE_TRACK_SESSION
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 Immediate : 1;
	    xuint8 Reserved1 : 7;
        
	    xuint8 Track     : 1;
	    xuint8 Session   : 1;
	    xuint8 Reserved2 : 6;
#else
	    xuint8 Reserved1 : 7;
	    xuint8 Immediate : 1;

	    xuint8 Reserved2 : 6;   
	    xuint8 Session   : 1;        
	    xuint8 Track     : 1;
#endif
	    xuint8 Reserved3;
	    xuint8 TrackNumber[2];
	    xuint8 Reserved4[3];
	    xuint8 Control;
	}   CLOSE_TRACK;

	struct _READ_BUFFER_CAPACITY {
	    xuint8 OperationCode;    // 0x5C - SCSIOP_READ_BUFFER_CAPACITY
#if defined(__LITTLE_ENDIAN_BITFIELD)  		    
	    xuint8 BlockInfo : 1;
	    xuint8 Reserved1 : 7;
#else
	    xuint8 Reserved1 : 7;
	    xuint8 BlockInfo : 1;
#endif
	    xuint8 Reserved2[5];
	    xuint8 AllocationLength[2];
	    xuint8 Control;
	}   READ_BUFFER_CAPACITY;

	struct _SEND_CUE_SHEET {
	    xuint8 OperationCode;    // 0x5D - SCSIOP_SEND_CUE_SHEET
	    xuint8 Reserved[5];
	    xuint8 CueSheetSize[3];
	    xuint8 Control;
	}   SEND_CUE_SHEET;

	struct _READ_HEADER {
	    xuint8 OperationCode;    // 0x44 - SCSIOP_READ_HEADER
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 Reserved1 : 1;
	    xuint8 Msf : 1;
	    xuint8 Reserved2 : 3;
	    xuint8 Lun : 3;
 #else
 	    xuint8 Lun : 3;
 	    xuint8 Reserved2 : 3;
	    xuint8 Msf : 1;
 	    xuint8 Reserved1 : 1;
 #endif
	    xuint8 LogicalBlockAddress[4];
	    xuint8 Reserved3;
	    xuint8 AllocationLength[2];
	    xuint8 Control;
	}   READ_HEADER;

	struct _PLAY_AUDIO {
	    xuint8 OperationCode;    // 0x45 - SCSIOP_PLAY_AUDIO
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 Reserved1 : 5;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 StartingBlockAddress[4];
	    xuint8 Reserved2;
	    xuint8 PlayLength[2];
	    xuint8 Control;
	}   PLAY_AUDIO;

	struct _PLAY_AUDIO_MSF {
	    xuint8 OperationCode;    // 0x47 - SCSIOP_PLAY_AUDIO_MSF
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 Reserved1 : 5;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 Reserved2;
	    xuint8 StartingM;
	    xuint8 StartingS;
	    xuint8 StartingF;
	    xuint8 EndingM;
	    xuint8 EndingS;
	    xuint8 EndingF;
	    xuint8 Control;
	}   PLAY_AUDIO_MSF;

	struct _BLANK_MEDIA {
	    xuint8 OperationCode;    // 0xA1 - SCSIOP_BLANK
#if defined(__LITTLE_ENDIAN_BITFIELD) 	    
	    xuint8 BlankType : 3;
	    xuint8 Reserved1 : 1;
	    xuint8 Immediate : 1;
	    xuint8 Reserved2 : 3;
#else
	    xuint8 Reserved2 : 3;
	    xuint8 Immediate : 1;
 	    xuint8 Reserved1 : 1;       
	    xuint8 BlankType : 3;
#endif
	    xuint8 AddressOrTrack[4];
	    xuint8 Reserved3[5];
	    xuint8 Control;
	}  BLANK_MEDIA;

	struct _PLAY_CD {
	    xuint8 OperationCode;    // 0xBC - SCSIOP_PLAY_CD
#if defined(__LITTLE_ENDIAN_BITFIELD) 	    
	    xuint8 Reserved1 : 1;
	    xuint8 CMSF : 1;         // LBA = 0, MSF = 1
	    xuint8 ExpectedSectorType : 3;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 ExpectedSectorType : 3;
	    xuint8 CMSF : 1;         // LBA = 0, MSF = 1        
	    xuint8 Reserved1 : 1;
#endif
	    union {
	        struct _LBA {
	            xuint8 StartingBlockAddress[4];
	            xuint8 PlayLength[4];
	        } LBA;

	        struct _MSF {
	            xuint8 Reserved1;
	            xuint8 StartingM;
	            xuint8 StartingS;
	            xuint8 StartingF;
	            xuint8 EndingM;
	            xuint8 EndingS;
	            xuint8 EndingF;
	            xuint8 Reserved2;
	        } MSF;
	    }u1;
#if defined(__LITTLE_ENDIAN_BITFIELD) 	    
	    xuint8 Audio : 1;
	    xuint8 Composite : 1;
	    xuint8 Port1 : 1;
	    xuint8 Port2 : 1;
	    xuint8 Reserved2 : 3;
	    xuint8 Speed : 1;
#else
            xuint8 Speed : 1;
            xuint8 Reserved2 : 3;
            xuint8 Port2 : 1;
            xuint8 Port1 : 1;
            xuint8 Composite : 1; 
            xuint8 Audio : 1;
#endif
	    xuint8 Control;
	}   PLAY_CD;

	struct _SCAN_CD {
	    xuint8 OperationCode;    // 0xBA - SCSIOP_SCAN_CD
#if defined(__LITTLE_ENDIAN_BITFIELD) 	    	    
	    xuint8 RelativeAddress : 1;
	    xuint8 Reserved1 : 3;
	    xuint8 Direct : 1;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Direct : 1;
	    xuint8 Reserved1 : 3;        
	    xuint8 RelativeAddress : 1;
#endif
	    xuint8 StartingAddress[4];
	    xuint8 Reserved2[3];
#if defined(__LITTLE_ENDIAN_BITFIELD) 	         
	    xuint8 Reserved3 : 6;
	    xuint8 Type : 2;
#else
	    xuint8 Type : 2;
	    xuint8 Reserved3 : 6;
#endif
	    xuint8 Reserved4;
	    xuint8 Control;
	}   SCAN_CD;

	struct _STOP_PLAY_SCAN {
	    xuint8 OperationCode;    // 0x4E - SCSIOP_STOP_PLAY_SCAN
#if defined(__LITTLE_ENDIAN_BITFIELD) 	    	    	    
	    xuint8 Reserved1 : 5;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 Reserved2[7];
	    xuint8 Control;
	}   STOP_PLAY_SCAN;


	//
	// Read SubChannel Data
	//

	struct _SUBCHANNEL {
	    xuint8 OperationCode;    // 0x42 - SCSIOP_READ_SUB_CHANNEL
#if defined(__LITTLE_ENDIAN_BITFIELD) 	    
	    xuint8 Reserved0 : 1;
	    xuint8 Msf : 1;
	    xuint8 Reserved1 : 3;
	    xuint8 LogicalUnitNumber : 3;
        
	    xuint8 Reserved2 : 6;
	    xuint8 SubQ : 1;
	    xuint8 Reserved3 : 1;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 3;
	    xuint8 Msf : 1;        
	    xuint8 Reserved0 : 1;

	    xuint8 Reserved3 : 1;    
	    xuint8 SubQ : 1;        
	    xuint8 Reserved2 : 6;
#endif
	    xuint8 Format;
	    xuint8 Reserved4[2];
	    xuint8 TrackNumber;
	    xuint8 AllocationLength[2];
	    xuint8 Control;
	}   SUBCHANNEL;

	//
	// Read CD. Used by Atapi for raw sector reads.
	//

	struct _READ_CD {
	    xuint8 OperationCode;    // 0xBE - SCSIOP_READ_CD
#if defined(__LITTLE_ENDIAN_BITFIELD) 	    	    
	    xuint8 RelativeAddress : 1;
	    xuint8 Reserved0 : 1;
	    xuint8 ExpectedSectorType : 3;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 ExpectedSectorType : 3;
	    xuint8 Reserved0 : 1;
	    xuint8 RelativeAddress : 1;
#endif
	    xuint8 StartingLBA[4];
	    xuint8 TransferBlocks[3];
 #if defined(__LITTLE_ENDIAN_BITFIELD) 	        
	    xuint8 Reserved2 : 1;
	    xuint8 ErrorFlags : 2;
	    xuint8 IncludeEDC : 1;
	    xuint8 IncludeUserData : 1;
	    xuint8 HeaderCode : 2;
	    xuint8 IncludeSyncData : 1;
        
	    xuint8 SubChannelSelection : 3;
	    xuint8 Reserved3 : 5;
#else
            xuint8 IncludeSyncData : 1;
            xuint8 HeaderCode : 2;
            xuint8 IncludeUserData : 1;
            xuint8 IncludeEDC : 1;
            xuint8 ErrorFlags : 2;
            xuint8 Reserved2 : 1;

            xuint8 Reserved3 : 5;        
            xuint8 SubChannelSelection : 3;
#endif
	    xuint8 Control;
	}   READ_CD;

	struct _READ_CD_MSF {
	    xuint8 OperationCode;    // 0xB9 - SCSIOP_READ_CD_MSF
 #if defined(__LITTLE_ENDIAN_BITFIELD) 	       	    
	    xuint8 RelativeAddress : 1;
	    xuint8 Reserved1 : 1;
	    xuint8 ExpectedSectorType : 3;
	    xuint8 Lun : 3;
 #else
 	    xuint8 Lun : 3;
 	    xuint8 ExpectedSectorType : 3;
	    xuint8 Reserved1 : 1;        
 	    xuint8 RelativeAddress : 1;
 #endif
	    xuint8 Reserved2;
	    xuint8 StartingM;
	    xuint8 StartingS;
	    xuint8 StartingF;
	    xuint8 EndingM;
	    xuint8 EndingS;
	    xuint8 EndingF;
  #if defined(__LITTLE_ENDIAN_BITFIELD)        
	    xuint8 Reserved4 : 1;
	    xuint8 ErrorFlags : 2;
	    xuint8 IncludeEDC : 1;
	    xuint8 IncludeUserData : 1;
	    xuint8 HeaderCode : 2;
	    xuint8 IncludeSyncData : 1;
        
	    xuint8 SubChannelSelection : 3;
	    xuint8 Reserved5 : 5;
 #else
            xuint8 IncludeSyncData : 1;
            xuint8 HeaderCode : 2;
            xuint8 IncludeUserData : 1;
            xuint8 IncludeEDC : 1;
            xuint8 ErrorFlags : 2;
            xuint8 Reserved4 : 1;
            
            xuint8 Reserved5 : 5;       
            xuint8 SubChannelSelection : 3;

 #endif
	    xuint8 Control;
	}   READ_CD_MSF;

	//
	// Plextor Read CD-DA
	//

	struct _PLXTR_READ_CDDA {
	    xuint8 OperationCode;    // Unknown -- vendor-unique?
 #if defined(__LITTLE_ENDIAN_BITFIELD) 	       	    
	    xuint8 Reserved0 : 5;
	    xuint8 LogicalUnitNumber :3;
 #else
 	    xuint8 LogicalUnitNumber :3;
 	    xuint8 Reserved0 : 5;
 #endif
	    xuint8 LogicalBlockByte0;
	    xuint8 LogicalBlockByte1;
	    xuint8 LogicalBlockByte2;
	    xuint8 LogicalBlockByte3;
	    xuint8 TransferBlockByte0;
	    xuint8 TransferBlockByte1;
	    xuint8 TransferBlockByte2;
	    xuint8 TransferBlockByte3;
	    xuint8 SubCode;
	    xuint8 Control;
	}   PLXTR_READ_CDDA;

	//
	// NEC Read CD-DA
	//

	struct _NEC_READ_CDDA {
	    xuint8 OperationCode;    // Unknown -- vendor-unique?
	    xuint8 Reserved0;
	    xuint8 LogicalBlockByte0;
	    xuint8 LogicalBlockByte1;
	    xuint8 LogicalBlockByte2;
	    xuint8 LogicalBlockByte3;
	    xuint8 Reserved1;
	    xuint8 TransferBlockByte0;
	    xuint8 TransferBlockByte1;
	    xuint8 Control;
	}   NEC_READ_CDDA;

	//
	// Mode sense
	//

	struct _MODE_SENSE {
	    xuint8 OperationCode;    // 0x1A - SCSIOP_MODE_SENSE
#if defined(__LITTLE_ENDIAN_BITFIELD) 	  	    
	    xuint8 Reserved1 : 3;
	    xuint8 Dbd : 1;
	    xuint8 Reserved2 : 1;
	    xuint8 LogicalUnitNumber : 3;
        
	    xuint8 PageCode : 6;
	    xuint8 Pc : 2;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved2 : 1;
 	    xuint8 Dbd : 1;       
	    xuint8 Reserved1 : 3;
        
	    xuint8 Pc : 2;        
	    xuint8 PageCode : 6;
#endif
	    xuint8 Reserved3;
	    xuint8 AllocationLength;
	    xuint8 Control;
	}   MODE_SENSE;

	struct _MODE_SENSE10 {
	    xuint8 OperationCode;    // 0x5A - SCSIOP_MODE_SENSE10
#if defined(__LITTLE_ENDIAN_BITFIELD) 	  	    
	    xuint8 Reserved1 : 3;
	    xuint8 Dbd : 1;
	    xuint8 Reserved2 : 1;
	    xuint8 LogicalUnitNumber : 3;
        
	    xuint8 PageCode : 6;
	    xuint8 Pc : 2;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved2 : 1;
 	    xuint8 Dbd : 1;       
	    xuint8 Reserved1 : 3;
        
	    xuint8 Pc : 2;        
	    xuint8 PageCode : 6;
#endif
	    xuint8 Reserved3[4];
	    xuint8 AllocationLength[2];
	    xuint8 Control;
	}   MODE_SENSE10;

	//
	// Mode select
	//

	struct _MODE_SELECT {
	    xuint8 OperationCode;    // 0x15 - SCSIOP_MODE_SELECT
#if defined(__LITTLE_ENDIAN_BITFIELD) 	  	    
	    xuint8 SPBit : 1;
	    xuint8 Reserved1 : 3;
	    xuint8 PFBit : 1;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 PFBit : 1;
	    xuint8 Reserved1 : 3;        
	    xuint8 SPBit : 1;
#endif
	    xuint8 Reserved2[2];
	    xuint8 ParameterListLength;
	    xuint8 Control;
	}   MODE_SELECT;

	struct _MODE_SELECT10 {
	    xuint8 OperationCode;    // 0x55 - SCSIOP_MODE_SELECT10
#if defined(__LITTLE_ENDIAN_BITFIELD) 	  	    
	    xuint8 SPBit : 1;
	    xuint8 Reserved1 : 3;
	    xuint8 PFBit : 1;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 PFBit : 1;
	    xuint8 Reserved1 : 3;        
	    xuint8 SPBit : 1;
#endif
	    xuint8 Reserved2[5];
	    xuint8 ParameterListLength[2];
	    xuint8 Control;
	}   MODE_SELECT10;

	struct _LOCATE {
	    xuint8 OperationCode;    // 0x2B - SCSIOP_LOCATE
#if defined(__LITTLE_ENDIAN_BITFIELD) 		    
	    xuint8 Immediate : 1;
	    xuint8 CPBit : 1;
	    xuint8 BTBit : 1;
	    xuint8 Reserved1 : 2;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 2;
	    xuint8 BTBit : 1;
	    xuint8 CPBit : 1;        
	    xuint8 Immediate : 1;
#endif
	    xuint8 Reserved3;
	    xuint8 LogicalBlockAddress[4];
	    xuint8 Reserved4;
	    xuint8 Partition;
	    xuint8 Control;
	}   LOCATE;

	struct _LOGSENSE {
	    xuint8 OperationCode;    // 0x4D - SCSIOP_LOG_SENSE
#if defined(__LITTLE_ENDIAN_BITFIELD)	    
	    xuint8 SPBit : 1;
	    xuint8 PPCBit : 1;
	    xuint8 Reserved1 : 3;
	    xuint8 LogicalUnitNumber : 3;
        
	    xuint8 PageCode : 6;
	    xuint8 PCBit : 2;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 3;
	    xuint8 PPCBit : 1;
	    xuint8 SPBit : 1;
        
	    xuint8 PCBit : 2;        
	    xuint8 PageCode : 6;
#endif
	    xuint8 Reserved2;
	    xuint8 Reserved3;
	    xuint8 ParameterPointer[2];
	    xuint8 AllocationLength[2];
	    xuint8 Control;
	}   LOGSENSE;

	struct _LOGSELECT {
	    xuint8 OperationCode;    // 0x4C - SCSIOP_LOG_SELECT
#if defined(__LITTLE_ENDIAN_BITFIELD)	    
	    xuint8 SPBit : 1;
	    xuint8 PPCBit : 1;
	    xuint8 Reserved1 : 3;
	    xuint8 LogicalUnitNumber : 3;
        
	    xuint8 PageCode : 6;
	    xuint8 PCBit : 2;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 3;
	    xuint8 PPCBit : 1;
	    xuint8 SPBit : 1;
        
	    xuint8 PCBit : 2;        
	    xuint8 PageCode : 6;
#endif
	    xuint8 Reserved2[4];
	    xuint8 ParameterListLength[2];
	    xuint8 Control;
	}   LOGSELECT;

	struct _PRINT {
	    xuint8 OperationCode;    // 0x0A - SCSIOP_PRINT
#if defined(__LITTLE_ENDIAN_BITFIELD)	    	    
	    xuint8 Reserved : 5;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved : 5;
#endif
	    xuint8 TransferLength[3];
	    xuint8 Control;
	}   PRINT;

	struct _SEEK {
	    xuint8 OperationCode;    // 0x2B - SCSIOP_SEEK
#if defined(__LITTLE_ENDIAN_BITFIELD)	    	    
	    xuint8 Reserved : 5;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved : 5;
#endif
	    xuint8 LogicalBlockAddress[4];
	    xuint8 Reserved2[3];
	    xuint8 Control;
	}   SEEK;

	struct _ERASE {
	    xuint8 OperationCode;    // 0x19 - SCSIOP_ERASE
#if defined(__LITTLE_ENDIAN_BITFIELD)	    	    
	    xuint8 Long : 1;
	    xuint8 Immediate : 1;
	    xuint8 Reserved1 : 3;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 3;
	    xuint8 Immediate : 1;        
	    xuint8 Long : 1;
#endif
	    xuint8 Reserved2[3];
	    xuint8 Control;
	}   ERASE;

	struct _START_STOP {
	    xuint8 OperationCode;    // 0x1B - SCSIOP_START_STOP_UNIT
#if defined(__LITTLE_ENDIAN_BITFIELD)	    
	    xuint8 Immediate: 1;
	    xuint8 Reserved1 : 4;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 4;
	    xuint8 Immediate: 1;
#endif
	    xuint8 Reserved2[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)	    
	    xuint8 Start : 1;
	    xuint8 LoadEject : 1;
	    xuint8 Reserved3 : 6;
#else
	    xuint8 Reserved3 : 6;
	    xuint8 LoadEject : 1;
	    xuint8 Start : 1;
#endif
	    xuint8 Control;
	}   START_STOP;

	struct _MEDIA_REMOVAL {
	    xuint8 OperationCode;    // 0x1E - SCSIOP_MEDIUM_REMOVAL
#if defined(__LITTLE_ENDIAN_BITFIELD)	    	    
	    xuint8 Reserved1 : 5;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 Reserved2[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)	  
	    xuint8 Prevent : 1;
	    xuint8 Persistant : 1;
	    xuint8 Reserved3 : 6;
#else
	    xuint8 Persistant : 1;
	    xuint8 Reserved3 : 6;
	    xuint8 Prevent : 1;
#endif
	    xuint8 Control;
	}   MEDIA_REMOVAL;

	//
	// Tape CDBs
	//

	struct _SEEK_BLOCK {
	    xuint8 OperationCode;    // 0x0C - SCSIOP_SEEK_BLOCK
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	    
	    xuint8 Immediate : 1;
	    xuint8 Reserved1 : 7;
#else
	    xuint8 Reserved1 : 7;
	    xuint8 Immediate : 1;
#endif
	    xuint8 BlockAddress[3];
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	
	    xuint8 Link : 1;
	    xuint8 Flag : 1;
	    xuint8 Reserved2 : 4;
	    xuint8 VendorUnique : 2;
#else
	    xuint8 VendorUnique : 2;
	    xuint8 Reserved2 : 4;
	    xuint8 Flag : 1;        
	    xuint8 Link : 1;
#endif
	}   SEEK_BLOCK;

	struct _REQUEST_BLOCK_ADDRESS {
	    xuint8 OperationCode;    // 0x02 - SCSIOP_REQUEST_BLOCK_ADDR
	    xuint8 Reserved1[3];
	    xuint8 AllocationLength;
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	
	    xuint8 Link : 1;
	    xuint8 Flag : 1;
	    xuint8 Reserved2 : 4;
	    xuint8 VendorUnique : 2;
#else
	    xuint8 VendorUnique : 2;
	    xuint8 Reserved2 : 4;
	    xuint8 Flag : 1;        
	    xuint8 Link : 1;
#endif
	}   REQUEST_BLOCK_ADDRESS;

	struct _PARTITION {
	    xuint8 OperationCode;    // 0x0D - SCSIOP_PARTITION
#if defined(__LITTLE_ENDIAN_BITFIELD)	    
	    xuint8 Immediate : 1;
	    xuint8 Sel: 1;
	    xuint8 PartitionSelect : 6;
#else
	    xuint8 PartitionSelect : 6;
	    xuint8 Sel: 1;
	    xuint8 Immediate : 1;
#endif
	    xuint8 Reserved1[3];
	    xuint8 Control;
	}   PARTITION;

	struct _WRITE_TAPE_MARKS {
	    xuint8 OperationCode;    // Unknown -- vendor-unique?
#if defined(__LITTLE_ENDIAN_BITFIELD)	   	    
	    xuint8 Immediate : 1;
	    xuint8 WriteSetMarks: 1;
	    xuint8 Reserved : 3;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved : 3;
	    xuint8 WriteSetMarks: 1;
	    xuint8 Immediate : 1;
#endif
	    xuint8 TransferLength[3];
	    xuint8 Control;
	}   WRITE_TAPE_MARKS;

	struct _SPACE_TAPE_MARKS {
	    xuint8 OperationCode;    // Unknown -- vendor-unique?
#if defined(__LITTLE_ENDIAN_BITFIELD)	  	    
	    xuint8 Code : 3;
	    xuint8 Reserved : 2;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved : 2;
	    xuint8 Code : 3;
#endif
	    xuint8 NumMarksMSB ;
	    xuint8 NumMarks;
	    xuint8 NumMarksLSB;
	    union {
	        xuint8 value;
	        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)	  	                
	            xuint8 Link : 1;
	            xuint8 Flag : 1;
	            xuint8 Reserved : 4;
	            xuint8 VendorUnique : 2;
#else
	            xuint8 VendorUnique : 2;
	            xuint8 Reserved : 4;
	            xuint8 Flag : 1;
	            xuint8 Link : 1;
#endif
	        } Fields;
	    } Byte6;
	}   SPACE_TAPE_MARKS;

	//
	// Read tape position
	//

	struct _READ_POSITION {
	    xuint8 Operation;        // 0x43 - SCSIOP_READ_POSITION
#if defined(__LITTLE_ENDIAN_BITFIELD)	  		    
	    xuint8 BlockType:1;
	    xuint8 Reserved1:4;
	    xuint8 Lun:3;
#else
	    xuint8 Lun:3;
	    xuint8 Reserved1:4;
	    xuint8 BlockType:1;
#endif
	    xuint8 Reserved2[7];
	    xuint8 Control;
	}   READ_POSITION;

	//
	// ReadWrite for Tape
	//

	struct _CDB6READWRITETAPE {
	    xuint8 OperationCode;    // Unknown -- vendor-unique?
#if defined(__LITTLE_ENDIAN_BITFIELD)	  		    
	    xuint8 VendorSpecific : 5;
	    xuint8 Reserved : 3;
#else
	    xuint8 Reserved : 3;
	    xuint8 VendorSpecific : 5;
#endif
	    xuint8 TransferLenMSB;
	    xuint8 TransferLen;
	    xuint8 TransferLenLSB;
#if defined(__LITTLE_ENDIAN_BITFIELD)	         
	    xuint8 Link : 1;
	    xuint8 Flag : 1;
	    xuint8 Reserved1 : 4;
	    xuint8 VendorUnique : 2;
#else
	    xuint8 VendorUnique : 2;
	    xuint8 Reserved1 : 4;
 	    xuint8 Flag : 1;       
	    xuint8 Link : 1;
#endif
	}   CDB6READWRITETAPE;

	//
	// Medium changer CDB's
	//

	struct _INIT_ELEMENT_STATUS {
	    xuint8 OperationCode;    // 0x07 - SCSIOP_INIT_ELEMENT_STATUS
#if defined(__LITTLE_ENDIAN_BITFIELD)	     	    
	    xuint8 Reserved1 : 5;
	    xuint8 LogicalUnitNubmer : 3;
#else
	    xuint8 LogicalUnitNubmer : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 Reserved2[3];
#if defined(__LITTLE_ENDIAN_BITFIELD)	   
	    xuint8 Reserved3 : 7;
	    xuint8 NoBarCode : 1;
#else
	    xuint8 NoBarCode : 1;
	    xuint8 Reserved3 : 7;
#endif
	}   INIT_ELEMENT_STATUS;

	struct _INITIALIZE_ELEMENT_RANGE {
	    xuint8 OperationCode;    // 0xE7 - SCSIOP_INIT_ELEMENT_RANGE
	    xuint8 Range : 1;
	    xuint8 Reserved1 : 4;
	    xuint8 LogicalUnitNubmer : 3;
	    xuint8 FirstElementAddress[2];
	    xuint8 Reserved2[2];
	    xuint8 NumberOfElements[2];
	    xuint8 Reserved3;
	    xuint8 Reserved4 : 7;
	    xuint8 NoBarCode : 1;
	}   INITIALIZE_ELEMENT_RANGE;

	struct _POSITION_TO_ELEMENT {
	    xuint8 OperationCode;    // 0x2B - SCSIOP_POSITION_TO_ELEMENT
#if defined(__LITTLE_ENDIAN_BITFIELD)	     	    
	    xuint8 Reserved1 : 5;
	    xuint8 LogicalUnitNubmer : 3;
#else
	    xuint8 LogicalUnitNubmer : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 TransportElementAddress[2];
	    xuint8 DestinationElementAddress[2];
	    xuint8 Reserved2[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)	     	            
	    xuint8 Flip : 1;
	    xuint8 Reserved3 : 7;
#else
	    xuint8 Reserved3 : 7;
	    xuint8 Flip : 1;
#endif
	    xuint8 Control;
	}   POSITION_TO_ELEMENT;

	struct _MOVE_MEDIUM {
	    xuint8 OperationCode;    // 0xA5 - SCSIOP_MOVE_MEDIUM
	    xuint8 Reserved1 : 5;
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 TransportElementAddress[2];
	    xuint8 SourceElementAddress[2];
	    xuint8 DestinationElementAddress[2];
	    xuint8 Reserved2[2];
	    xuint8 Flip : 1;
	    xuint8 Reserved3 : 7;
	    xuint8 Control;
	}  MOVE_MEDIUM;

	struct _EXCHANGE_MEDIUM {
	    xuint8 OperationCode;    // 0xA6 - SCSIOP_EXCHANGE_MEDIUM
#if defined(__LITTLE_ENDIAN_BITFIELD)	     	    
	    xuint8 Reserved1 : 5;
	    xuint8 LogicalUnitNubmer : 3;
#else
	    xuint8 LogicalUnitNubmer : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 TransportElementAddress[2];
	    xuint8 SourceElementAddress[2];
	    xuint8 Destination1ElementAddress[2];
	    xuint8 Destination2ElementAddress[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)	       
	    xuint8 Flip1 : 1;
	    xuint8 Flip2 : 1;
	    xuint8 Reserved3 : 6;
#else
	    xuint8 Reserved3 : 6;
	    xuint8 Flip2 : 1;
	    xuint8 Flip1 : 1;
#endif
	    xuint8 Control;
	}   EXCHANGE_MEDIUM;

	struct _READ_ELEMENT_STATUS {
	    xuint8 OperationCode;    // 0xB8 - SCSIOP_READ_ELEMENT_STATUS
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	    
	    xuint8 ElementType : 4;
	    xuint8 VolTag : 1;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 VolTag : 1;
	    xuint8 ElementType : 4;
#endif
	    xuint8 StartingElementAddress[2];
	    xuint8 NumberOfElements[2];
	    xuint8 Reserved1;
	    xuint8 AllocationLength[3];
	    xuint8 Reserved2;
	    xuint8 Control;
	}   READ_ELEMENT_STATUS;

	struct _SEND_VOLUME_TAG {
	    xuint8 OperationCode;    // 0xB6 - SCSIOP_SEND_VOLUME_TAG
#if defined(__LITTLE_ENDIAN_BITFIELD)		    
	    xuint8 ElementType : 4;
	    xuint8 Reserved1 : 1;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 Reserved1 : 1;
	    xuint8 ElementType : 4;
#endif
	    xuint8 StartingElementAddress[2];
	    xuint8 Reserved2;
#if defined(__LITTLE_ENDIAN_BITFIELD)		            
	    xuint8 ActionCode : 5;
	    xuint8 Reserved3 : 3;
#else
	    xuint8 Reserved3 : 3;
	    xuint8 ActionCode : 5;
#endif
	    xuint8 Reserved4[2];
	    xuint8 ParameterListLength[2];
	    xuint8 Reserved5;
	    xuint8 Control;
	}   SEND_VOLUME_TAG;

	struct _REQUEST_VOLUME_ELEMENT_ADDRESS {
	    xuint8 OperationCode;    // Unknown -- vendor-unique?
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	    
	    xuint8 ElementType : 4;
	    xuint8 VolTag : 1;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 VolTag : 1;
	    xuint8 ElementType : 4;
#endif
	    xuint8 StartingElementAddress[2];
	    xuint8 NumberElements[2];
	    xuint8 Reserved1;
	    xuint8 AllocationLength[3];
	    xuint8 Reserved2;
	    xuint8 Control;
	}   REQUEST_VOLUME_ELEMENT_ADDRESS;

	//
	// Atapi 2.5 Changer 12-byte CDBs
	//

	struct _LOAD_UNLOAD {
	    xuint8 OperationCode;    // 0xA6 - SCSIOP_LOAD_UNLOAD_SLOT
#if defined(__LITTLE_ENDIAN_BITFIELD)	    
	    xuint8 Immediate : 1;
	    xuint8 Reserved1 : 4;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved1 : 4;
	    xuint8 Immediate : 1;
#endif
	    xuint8 Reserved2[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)	    
	    xuint8 Start : 1;
	    xuint8 LoadEject : 1;
	    xuint8 Reserved3: 6;
#else
	    xuint8 Reserved3: 6;
	    xuint8 LoadEject : 1;
	    xuint8 Start : 1;
#endif
	    xuint8 Reserved4[3];
	    xuint8 Slot;
	    xuint8 Reserved5[3];
	}   LOAD_UNLOAD;

	struct _MECH_STATUS {
	    xuint8 OperationCode;    // 0xBD - SCSIOP_MECHANISM_STATUS
#if defined(__LITTLE_ENDIAN_BITFIELD)		    
	    xuint8 Reserved : 5;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved : 5;
#endif
	    xuint8 Reserved1[6];
	    xuint8 AllocationLength[2];
	    xuint8 Reserved2[1];
	    xuint8 Control;
	}  MECH_STATUS;

	//
	// C/DVD 0.9 CDBs
	//

	struct _SYNCHRONIZE_CACHE10 {

	    xuint8 OperationCode;    // 0x35 - SCSIOP_SYNCHRONIZE_CACHE
#if defined(__LITTLE_ENDIAN_BITFIELD)
	    xuint8 RelAddr : 1;
	    xuint8 Immediate : 1;
	    xuint8 Reserved : 3;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved : 3;
	    xuint8 Immediate : 1;        
	    xuint8 RelAddr : 1;
#endif
	    xuint8 LogicalBlockAddress[4];   // Unused - set to zero
	    xuint8 Reserved2;
	    xuint8 BlockCount[2];            // Unused - set to zero
	    xuint8 Control;
	}   SYNCHRONIZE_CACHE10;

	struct _GET_EVENT_STATUS_NOTIFICATION {
	    xuint8 OperationCode;    // 0x4A - SCSIOP_GET_EVENT_STATUS_NOTIFICATION
#if defined(__LITTLE_ENDIAN_BITFIELD)
	    xuint8 Immediate : 1;
	    xuint8 Reserved : 4;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved : 4;
	    xuint8 Immediate : 1;
#endif
	    xuint8 Reserved2[2];
	    xuint8 NotificationClassRequest;
	    xuint8 Reserved3[2];
	    xuint8 EventListLength[2];

	    xuint8 Control;
	}  GET_EVENT_STATUS_NOTIFICATION;

	struct _GET_PERFORMANCE {
	    xuint8 OperationCode;    // 0xAC - SCSIOP_GET_PERFORMANCE
#if defined(__LITTLE_ENDIAN_BITFIELD)	    
	    xuint8 Except    : 2;
	    xuint8 Write     : 1;
	    xuint8 Tolerance : 2;
	    xuint8 Reserved0 : 3;
#else
	    xuint8 Reserved0 : 3;
	    xuint8 Tolerance : 2;
	    xuint8 Write     : 1;
	    xuint8 Except    :2;
#endif
	    xuint8 StartingLBA[4];
	    xuint8 Reserved1[2];
	    xuint8 MaximumNumberOfDescriptors[2];
	    xuint8 Type;
	    xuint8 Control;
	}  GET_PERFORMANCE;

	struct _READ_DVD_STRUCTURE {
	    xuint8 OperationCode;    // 0xAD - SCSIOP_READ_DVD_STRUCTURE
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	    
	    xuint8 Reserved1 : 5;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 RMDBlockNumber[4];
	    xuint8 LayerNumber;
	    xuint8 Format;
	    xuint8 AllocationLength[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)        
	    xuint8 Reserved3 : 6;
	    xuint8 AGID : 2;
#else
	    xuint8 AGID : 2;
	    xuint8 Reserved3 : 6;
#endif
	    xuint8 Control;
	}   READ_DVD_STRUCTURE;

	struct _SET_STREAMING {
	    xuint8 OperationCode;    // 0xB6 - SCSIOP_SET_STREAMING
	    xuint8 Reserved[8];
	    xuint8 ParameterListLength[2];
	    xuint8 Control;
	}   SET_STREAMING;

	struct _SEND_DVD_STRUCTURE {
	    xuint8 OperationCode;    // 0xBF - SCSIOP_SEND_DVD_STRUCTURE
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	    
	    xuint8 Reserved1 : 5;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 Reserved2[5];
	    xuint8 Format;
	    xuint8 ParameterListLength[2];
	    xuint8 Reserved3;
	    xuint8 Control;
	}  SEND_DVD_STRUCTURE;

	struct _SEND_KEY {
	    xuint8 OperationCode;    // 0xA3 - SCSIOP_SEND_KEY
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	    
	    xuint8 Reserved1 : 5;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 Reserved2[6];
	    xuint8 ParameterListLength[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	        
	    xuint8 KeyFormat : 6;
	    xuint8 AGID : 2;
#else
	    xuint8 AGID : 2;
	    xuint8 KeyFormat : 6;
#endif
	    xuint8 Control;
	}   SEND_KEY;

	struct _REPORT_KEY {
	    xuint8 OperationCode;    // 0xA4 - SCSIOP_REPORT_KEY
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	    
	    xuint8 Reserved1 : 5;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 LogicalBlockAddress[4];   // for title key
	    xuint8 Reserved2[2];
	    xuint8 AllocationLength[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	        
	    xuint8 KeyFormat : 6;
	    xuint8 AGID : 2;
#else
	    xuint8 AGID : 2;
	    xuint8 KeyFormat : 6;
#endif
	    xuint8 Control;
	}  REPORT_KEY;

	struct _SET_READ_AHEAD {
	    xuint8 OperationCode;    // 0xA7 - SCSIOP_SET_READ_AHEAD
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	    
	    xuint8 Reserved1 : 5;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 TriggerLBA[4];
	    xuint8 ReadAheadLBA[4];
	    xuint8 Reserved2;
	    xuint8 Control;
	}  SET_READ_AHEAD;

	struct _READ_FORMATTED_CAPACITIES {
	    xuint8 OperationCode;    // 0x23 - SCSIOP_READ_FORMATTED_CAPACITY
#if defined(__LITTLE_ENDIAN_BITFIELD)	 	    
	    xuint8 Reserved1 : 5;
	    xuint8 Lun : 3;
#else
	    xuint8 Lun : 3;
	    xuint8 Reserved1 : 5;
#endif
	    xuint8 Reserved2[5];
	    xuint8 AllocationLength[2];
	    xuint8 Control;
	}   READ_FORMATTED_CAPACITIES;

	//
	// SCSI-3
	//

	struct _REPORT_LUNS {
	    xuint8 OperationCode;    // 0xA0 - SCSIOP_REPORT_LUNS
	    xuint8 Reserved1[5];
	    xuint8 AllocationLength[4];
	    xuint8 Reserved2[1];
	    xuint8 Control;
	}   REPORT_LUNS;

	struct _PERSISTENT_RESERVE_IN {
	    xuint8 OperationCode;    // 0x5E - SCSIOP_PERSISTENT_RESERVE_IN
#if defined(__LITTLE_ENDIAN_BITFIELD)		    
	    xuint8 ServiceAction : 5;
	    xuint8 Reserved1 : 3;
#else
	    xuint8 Reserved1 : 3;
	    xuint8 ServiceAction : 5;
#endif
	    xuint8 Reserved2[5];
	    xuint8 AllocationLength[2];
	    xuint8 Control;
	}   PERSISTENT_RESERVE_IN;

	struct _PERSISTENT_RESERVE_OUT {
	    xuint8 OperationCode;    // 0x5F - SCSIOP_PERSISTENT_RESERVE_OUT
#if defined(__LITTLE_ENDIAN_BITFIELD)			    
	    xuint8 ServiceAction : 5;
	    xuint8 Reserved1 : 3;
        
	    xuint8 Type : 4;
	    xuint8 Scope : 4;
#else
	    xuint8 Reserved1 : 3;
	    xuint8 ServiceAction : 5;
        
	    xuint8 Scope : 4;        
	    xuint8 Type : 4;
#endif
	    xuint8 Reserved2[4];
	    xuint8 ParameterListLength[2]; // 0x18
	    xuint8 Control;
	}   PERSISTENT_RESERVE_OUT;

	//
	// MMC / SFF-8090 commands
	//

	struct _GET_CONFIGURATION {
	    xuint8 OperationCode;       // 0x46 - SCSIOP_GET_CONFIGURATION
#if defined(__LITTLE_ENDIAN_BITFIELD)			    
	    xuint8 RequestType : 2;     // SCSI_GET_CONFIGURATION_REQUEST_TYPE_*
	    xuint8 Reserved1   : 6;     // includes obsolete LUN field
#else
	    xuint8 Reserved1   : 6;     // includes obsolete LUN field
	    xuint8 RequestType : 2;     // SCSI_GET_CONFIGURATION_REQUEST_TYPE_*
#endif
	    xuint8 StartingFeature[2];
	    xuint8 Reserved2[3];
	    xuint8 AllocationLength[2];
	    xuint8 Control;
	}   GET_CONFIGURATION;

	struct _SET_CD_SPEED {
	    xuint8 OperationCode;       // 0xB8 - SCSIOP_SET_CD_SPEED
	    union {
	        xuint8 Reserved1;
	        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)                
	            xuint8 RotationControl : 2;
	            xuint8 Reserved3       : 6;
#else
	            xuint8 Reserved3       : 6;
	            xuint8 RotationControl : 2;
#endif
	        }ct1;
	    }u1;
	    xuint8 ReadSpeed[2];        // 1x == (75 * 2352)
	    xuint8 WriteSpeed[2];       // 1x == (75 * 2352)
	    xuint8 Reserved2[5];
	    xuint8 Control;
	}   SET_CD_SPEED;

	struct _READ12 {
	    xuint8 OperationCode;      // 0xA8 - SCSIOP_READ12
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 RelativeAddress   : 1;
	    xuint8 Reserved1         : 2;
	    xuint8 ForceUnitAccess   : 1;
	    xuint8 DisablePageOut    : 1;
	    xuint8 LogicalUnitNumber : 3;
#else
	    xuint8 LogicalUnitNumber : 3;
	    xuint8 DisablePageOut    : 1;
	    xuint8 ForceUnitAccess   : 1; 
	    xuint8 Reserved1         : 2;        
	    xuint8 RelativeAddress   : 1;
#endif
	    xuint8 LogicalBlock[4];
	    xuint8 TransferLength[4];

#if defined(__LITTLE_ENDIAN_BITFIELD)
	    xuint8 Reserved2 : 7;
	    xuint8 Streaming : 1;	
#else
           xuint8 Streaming : 1;
	    xuint8 Reserved2 : 7;
#endif
	    xuint8 Control;
	}   READ12;

	struct _WRITE12 {
	    xuint8 OperationCode;      // 0xAA - SCSIOP_WRITE12
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 RelativeAddress   : 1;
	    xuint8 Reserved1         : 1;
	    xuint8 EBP               : 1;
	    xuint8 ForceUnitAccess   : 1;
	    xuint8 DisablePageOut    : 1;
	    xuint8 LogicalUnitNumber : 3;
#else
            xuint8 LogicalUnitNumber : 3;
            xuint8 DisablePageOut    : 1;
            xuint8 ForceUnitAccess   : 1;
            xuint8 EBP               : 1;    
            xuint8 Reserved1         : 1;     
            xuint8 RelativeAddress   : 1;
#endif
	    xuint8 LogicalBlock[4];
	    xuint8 TransferLength[4];
#if defined(__LITTLE_ENDIAN_BITFIELD)
	    xuint8 Reserved2 : 7;
	    xuint8 Streaming : 1;	
#else
           xuint8 Streaming : 1;
	    xuint8 Reserved2 : 7;
#endif
	    xuint8 Control;
	}   WRITE12;

	//
	// 16-byte CDBs
	//

	struct _READ16 {
	    xuint8 OperationCode;      // 0x88 - SCSIOP_READ16
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 Reserved1         : 3;
	    xuint8 ForceUnitAccess   : 1;
	    xuint8 DisablePageOut    : 1;
	    xuint8 ReadProtect       : 3;
#else
	    xuint8 ReadProtect       : 3;
	    xuint8 DisablePageOut    : 1;
	    xuint8 ForceUnitAccess   : 1;        
	    xuint8 Reserved1         : 3;
#endif
	    xuint8 LogicalBlock[8];
	    xuint8 TransferLength[4];
#if defined(__LITTLE_ENDIAN_BITFIELD)
	    xuint8 Reserved2 : 7;
	    xuint8 Streaming : 1;	
#else
           xuint8 Streaming : 1;
	    xuint8 Reserved2 : 7;
#endif
	    xuint8 Control;
	}  READ16;

	struct _WRITE16 {
	    xuint8 OperationCode;      // 0x8A - SCSIOP_WRITE16
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 Reserved1         : 3;
	    xuint8 ForceUnitAccess   : 1;
	    xuint8 DisablePageOut    : 1;
	    xuint8 WriteProtect      : 3;
#else
	    xuint8 WriteProtect     : 3;
	    xuint8 DisablePageOut    : 1;
	    xuint8 ForceUnitAccess   : 1;        
	    xuint8 Reserved1         : 3;
#endif
	    xuint8 LogicalBlock[8];
	    xuint8 TransferLength[4];
#if defined(__LITTLE_ENDIAN_BITFIELD)
	    xuint8 Reserved2 : 7;
	    xuint8 Streaming : 1;	
#else
           xuint8 Streaming : 1;
	    xuint8 Reserved2 : 7;
#endif
	    xuint8 Control;
	}  WRITE16;

	struct _VERIFY16 {
	    xuint8 OperationCode;      // 0x8F - SCSIOP_VERIFY16
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 Reserved1         : 1;
	    xuint8 ByteCheck         : 1;
	    xuint8 BlockVerify       : 1;
	    xuint8 Reserved2         : 1;
	    xuint8 DisablePageOut    : 1;
	    xuint8 VerifyProtect     : 3;
#else
	    xuint8 VerifyProtect     : 3;
	    xuint8 DisablePageOut    : 1;
	    xuint8 Reserved2         : 1;
 	    xuint8 BlockVerify       : 1;
   	    xuint8 ByteCheck         : 1;    
	    xuint8 Reserved1         : 1;
#endif
	    xuint8 LogicalBlock[8];
	    xuint8 VerificationLength[4];
#if defined(__LITTLE_ENDIAN_BITFIELD)
	    xuint8 Reserved3 : 7;
	    xuint8 Streaming : 1;	
#else
           xuint8 Streaming : 1;
	    xuint8 Reserved3 : 7;
#endif
	    xuint8 Control;
	}   VERIFY16;

	struct _SYNCHRONIZE_CACHE16 {
	    xuint8 OperationCode;      // 0x91 - SCSIOP_SYNCHRONIZE_CACHE16
#if defined(__LITTLE_ENDIAN_BITFIELD)  		    
	    xuint8 Reserved1         : 1;
	    xuint8 Immediate         : 1;
	    xuint8 Reserved2         : 6;
#else
	    xuint8 Reserved2         : 6;
	    xuint8 Immediate         : 1;
	    xuint8 Reserved1         : 1;
#endif
	    xuint8 LogicalBlock[8];
	    xuint8 BlockCount[4];
	    xuint8 Reserved3;
	    xuint8 Control;
	}  SYNCHRONIZE_CACHE16;

	struct _READ_CAPACITY16 {
	    xuint8 OperationCode;      // 0x9E - SCSIOP_READ_CAPACITY16
#if defined(__LITTLE_ENDIAN_BITFIELD)  	    
	    xuint8 ServiceAction     : 5;
	    xuint8 Reserved1         : 3;
#else
	    xuint8 Reserved1         : 3;
	    xuint8 ServiceAction     : 5;
#endif
	    xuint8 LogicalBlock[8];
	    xuint8 BlockCount[4];
#if defined(__LITTLE_ENDIAN_BITFIELD)  	         
	    xuint8 PMI               : 1;
	    xuint8 Reserved2         : 7;
#else
	    xuint8 Reserved2         : 7;
	    xuint8 PMI               : 1;
#endif
	    xuint8 Control;
	}  READ_CAPACITY16;

	xuint32 AsUlong[4];
	xuint8 AsByte[16];

} __x_attribute_packed__  CDB, *PCDB;



////////////////////////////////////////////////////////////////////////////////
//
// GET_EVENT_STATUS_NOTIFICATION
//


#define NOTIFICATION_OPERATIONAL_CHANGE_CLASS_MASK  0x02
#define NOTIFICATION_POWER_MANAGEMENT_CLASS_MASK    0x04
#define NOTIFICATION_EXTERNAL_REQUEST_CLASS_MASK    0x08
#define NOTIFICATION_MEDIA_STATUS_CLASS_MASK        0x10
#define NOTIFICATION_MULTI_HOST_CLASS_MASK          0x20
#define NOTIFICATION_DEVICE_BUSY_CLASS_MASK         0x40


#define NOTIFICATION_NO_CLASS_EVENTS                  0x0
#define NOTIFICATION_OPERATIONAL_CHANGE_CLASS_EVENTS  0x1
#define NOTIFICATION_POWER_MANAGEMENT_CLASS_EVENTS    0x2
#define NOTIFICATION_EXTERNAL_REQUEST_CLASS_EVENTS    0x3
#define NOTIFICATION_MEDIA_STATUS_CLASS_EVENTS        0x4
#define NOTIFICATION_MULTI_HOST_CLASS_EVENTS          0x5
#define NOTIFICATION_DEVICE_BUSY_CLASS_EVENTS         0x6

typedef struct _NOTIFICATION_EVENT_STATUS_HEADER {
    xuint8 EventDataLength[2];
 #if defined(__LITTLE_ENDIAN_BITFIELD)     
    xuint8 NotificationClass : 3;
    xuint8 Reserved : 4;
    xuint8 NEA : 1;
#else
    xuint8 NEA : 1;
    xuint8 Reserved : 4;
    xuint8 NotificationClass : 3;
#endif
    xuint8 SupportedEventClasses;
} __x_attribute_packed__  NOTIFICATION_EVENT_STATUS_HEADER, *PNOTIFICATION_EVENT_STATUS_HEADER;


#define NOTIFICATION_OPERATIONAL_EVENT_NO_CHANGE         0x0
#define NOTIFICATION_OPERATIONAL_EVENT_CHANGE_REQUESTED  0x1
#define NOTIFICATION_OPERATIONAL_EVENT_CHANGE_OCCURRED   0x2

#define NOTIFICATION_OPERATIONAL_STATUS_AVAILABLE        0x0
#define NOTIFICATION_OPERATIONAL_STATUS_TEMPORARY_BUSY   0x1
#define NOTIFICATION_OPERATIONAL_STATUS_EXTENDED_BUSY    0x2

#define NOTIFICATION_OPERATIONAL_OPCODE_NONE             0x0
#define NOTIFICATION_OPERATIONAL_OPCODE_FEATURE_CHANGE   0x1
#define NOTIFICATION_OPERATIONAL_OPCODE_FEATURE_ADDED    0x2
#define NOTIFICATION_OPERATIONAL_OPCODE_UNIT_RESET       0x3
#define NOTIFICATION_OPERATIONAL_OPCODE_FIRMWARE_CHANGED 0x4
#define NOTIFICATION_OPERATIONAL_OPCODE_INQUIRY_CHANGED  0x5

//
// Class event data may be one (or none) of the following:
//


typedef struct _NOTIFICATION_OPERATIONAL_STATUS { // event class == 0x1
#if defined(__LITTLE_ENDIAN_BITFIELD)
    xuint8 OperationalEvent : 4;
    xuint8 Reserved1 : 4;

    xuint8 OperationalStatus : 4;
    xuint8 Reserved2 : 3;
    xuint8 PersistentPrevented : 1;
#else
    xuint8 Reserved1 : 4;
    xuint8 OperationalEvent : 4;

    xuint8 PersistentPrevented : 1;
    xuint8 Reserved2 : 3;    
    xuint8 OperationalStatus : 4;
#endif
    xuint8 Operation[2];
}  __x_attribute_packed__ NOTIFICATION_OPERATIONAL_STATUS, *PNOTIFICATION_OPERATIONAL_STATUS;



#define NOTIFICATION_POWER_EVENT_NO_CHANGE          0x0
#define NOTIFICATION_POWER_EVENT_CHANGE_SUCCEEDED   0x1
#define NOTIFICATION_POWER_EVENT_CHANGE_FAILED      0x2

#define NOTIFICATION_POWER_STATUS_ACTIVE            0x1
#define NOTIFICATION_POWER_STATUS_IDLE              0x2
#define NOTIFICATION_POWER_STATUS_STANDBY           0x3
#define NOTIFICATION_POWER_STATUS_SLEEP             0x4


typedef struct _NOTIFICATION_POWER_STATUS { // event class == 0x2
#if defined(__LITTLE_ENDIAN_BITFIELD)
    xuint8 PowerEvent : 4;
    xuint8 Reserved : 4;
#else
    xuint8 Reserved : 4;
    xuint8 PowerEvent : 4;
#endif
    xuint8 PowerStatus;
    xuint8 Reserved2[2];
}  __x_attribute_packed__ NOTIFICATION_POWER_STATUS, *PNOTIFICATION_POWER_STATUS;


#define NOTIFICATION_MEDIA_EVENT_NO_EVENT           0x0
#define NOTIFICATION_EXTERNAL_EVENT_NO_CHANGE       0x0
#define NOTIFICATION_EXTERNAL_EVENT_BUTTON_DOWN     0x1
#define NOTIFICATION_EXTERNAL_EVENT_BUTTON_UP       0x2
#define NOTIFICATION_EXTERNAL_EVENT_EXTERNAL        0x3 // respond with GET_CONFIGURATION?

#define NOTIFICATION_EXTERNAL_STATUS_READY          0x0
#define NOTIFICATION_EXTERNAL_STATUS_PREVENT        0x1

#define NOTIFICATION_EXTERNAL_REQUEST_NONE          0x0000
#define NOTIFICATION_EXTERNAL_REQUEST_QUEUE_OVERRUN 0x0001
#define NOTIFICATION_EXTERNAL_REQUEST_PLAY          0x0101
#define NOTIFICATION_EXTERNAL_REQUEST_REWIND_BACK   0x0102
#define NOTIFICATION_EXTERNAL_REQUEST_FAST_FORWARD  0x0103
#define NOTIFICATION_EXTERNAL_REQUEST_PAUSE         0x0104
#define NOTIFICATION_EXTERNAL_REQUEST_STOP          0x0106
#define NOTIFICATION_EXTERNAL_REQUEST_ASCII_LOW     0x0200
#define NOTIFICATION_EXTERNAL_REQUEST_ASCII_HIGH    0x02ff


typedef struct _NOTIFICATION_EXTERNAL_STATUS { // event class == 0x3
#if defined(__LITTLE_ENDIAN_BITFIELD)
    xuint8 ExternalEvent : 4;
    xuint8 Reserved1 : 4;
    
    xuint8 ExternalStatus : 4;
    xuint8 Reserved2 : 3;
    xuint8 PersistentPrevented : 1;
#else
    xuint8 Reserved1 : 4;
    xuint8 ExternalEvent : 4;

    xuint8 PersistentPrevented : 1; 
    xuint8 Reserved2 : 3;    
    xuint8 ExternalStatus : 4;
#endif
    xuint8 Request[2];
}  __x_attribute_packed__ NOTIFICATION_EXTERNAL_STATUS, *PNOTIFICATION_EXTERNAL_STATUS;


#define NOTIFICATION_MEDIA_EVENT_NO_CHANGE          0x0
#define NOTIFICATION_MEDIA_EVENT_EJECT_REQUEST      0x1
#define NOTIFICATION_MEDIA_EVENT_NEW_MEDIA          0x2
#define NOTIFICATION_MEDIA_EVENT_MEDIA_REMOVAL      0x3
#define NOTIFICATION_MEDIA_EVENT_MEDIA_CHANGE       0x4


typedef struct _NOTIFICATION_MEDIA_STATUS { // event class == 0x4
#if defined(__LITTLE_ENDIAN_BITFIELD)
    xuint8 MediaEvent : 4;
    xuint8 Reserved : 4;
#else
    xuint8 Reserved : 4;
    xuint8 MediaEvent : 4;
#endif
    union {
        xuint8 PowerStatus; // OBSOLETE -- was improperly named in NT5 headers
        xuint8 MediaStatus; // Use this for currently reserved fields
        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)            
            xuint8 DoorTrayOpen : 1;
            xuint8 MediaPresent : 1;
            xuint8 ReservedX    : 6; // do not reference this directly!
#else
            xuint8 ReservedX    : 6; // do not reference this directly!
            xuint8 MediaPresent : 1;            
            xuint8 DoorTrayOpen : 1;
#endif
        }ct;
    }u1;
    xuint8 StartSlot;
    xuint8 EndSlot;
}  __x_attribute_packed__ NOTIFICATION_MEDIA_STATUS, *PNOTIFICATION_MEDIA_STATUS;


#define NOTIFICATION_BUSY_EVENT_NO_EVENT               0x0
#define NOTIFICATION_MULTI_HOST_EVENT_NO_CHANGE        0x0
#define NOTIFICATION_MULTI_HOST_EVENT_CONTROL_REQUEST  0x1
#define NOTIFICATION_MULTI_HOST_EVENT_CONTROL_GRANT    0x2
#define NOTIFICATION_MULTI_HOST_EVENT_CONTROL_RELEASE  0x3

#define NOTIFICATION_MULTI_HOST_STATUS_READY           0x0
#define NOTIFICATION_MULTI_HOST_STATUS_PREVENT         0x1

#define NOTIFICATION_MULTI_HOST_PRIORITY_NO_REQUESTS   0x0
#define NOTIFICATION_MULTI_HOST_PRIORITY_LOW           0x1
#define NOTIFICATION_MULTI_HOST_PRIORITY_MEDIUM        0x2
#define NOTIFICATION_MULTI_HOST_PRIORITY_HIGH          0x3


typedef struct _NOTIFICATION_MULTI_HOST_STATUS { // event class == 0x5
#if defined(__LITTLE_ENDIAN_BITFIELD)
    xuint8 MultiHostEvent : 4;
    xuint8 Reserved1 : 4;
    xuint8 MultiHostStatus : 4;
    xuint8 Reserved2 : 3;
    xuint8 PersistentPrevented : 1;
#else
    xuint8 PersistentPrevented : 1;
    xuint8 Reserved2 : 3;
    xuint8 MultiHostStatus : 4;
    xuint8 Reserved1 : 4;    
    xuint8 MultiHostEvent : 4;
#endif
    xuint8 Priority[2];
}  __x_attribute_packed__ NOTIFICATION_MULTI_HOST_STATUS, *PNOTIFICATION_MULTI_HOST_STATUS;


#define NOTIFICATION_BUSY_EVENT_NO_EVENT            0x0
#define NOTIFICATION_BUSY_EVENT_NO_CHANGE           0x0
#define NOTIFICATION_BUSY_EVENT_BUSY                0x1

#define NOTIFICATION_BUSY_STATUS_NO_EVENT           0x0
#define NOTIFICATION_BUSY_STATUS_POWER              0x1
#define NOTIFICATION_BUSY_STATUS_IMMEDIATE          0x2
#define NOTIFICATION_BUSY_STATUS_DEFERRED           0x3


typedef struct _NOTIFICATION_BUSY_STATUS { // event class == 0x6
#if defined(__LITTLE_ENDIAN_BITFIELD)
    xuint8 DeviceBusyEvent : 4;
    xuint8 Reserved : 4;
#else
    xuint8 Reserved : 4;
    xuint8 DeviceBusyEvent : 4;
#endif
    xuint8 DeviceBusyStatus;
    xuint8 Time[2];
}  __x_attribute_packed__ NOTIFICATION_BUSY_STATUS, *PNOTIFICATION_BUSY_STATUS;


////////////////////////////////////////////////////////////////////////////////

//
// Read DVD Structure Definitions and Constants
//

#define DVD_FORMAT_LEAD_IN          0x00
#define DVD_FORMAT_COPYRIGHT        0x01
#define DVD_FORMAT_DISK_KEY         0x02
#define DVD_FORMAT_BCA              0x03
#define DVD_FORMAT_MANUFACTURING    0x04


typedef struct _READ_DVD_STRUCTURES_HEADER {
    xuint8 Length[2];
    xuint8 Reserved[2];
}  __x_attribute_packed__ READ_DVD_STRUCTURES_HEADER, *PREAD_DVD_STRUCTURES_HEADER;


//
// DiskKey, BCA & Manufacturer information will provide byte arrays as their
// data.
//

//
// CDVD 0.9 Send & Report Key Definitions and Structures
//

#define DVD_REPORT_AGID            0x00
#define DVD_CHALLENGE_KEY          0x01
#define DVD_KEY_1                  0x02
#define DVD_KEY_2                  0x03
#define DVD_TITLE_KEY              0x04
#define DVD_REPORT_ASF             0x05
#define DVD_INVALIDATE_AGID        0x3F


typedef struct _CDVD_KEY_HEADER {
    xuint8 DataLength[2];
    xuint8 Reserved[2];
}  __x_attribute_packed__ CDVD_KEY_HEADER, *PCDVD_KEY_HEADER;

typedef struct _CDVD_REPORT_AGID_DATA {
    xuint8 Reserved1[3];
#if defined(__LITTLE_ENDIAN_BITFIELD)    
    xuint8 Reserved2 : 6;
    xuint8 AGID : 2;
#else
    xuint8 AGID : 2;
    xuint8 Reserved2 : 6;
#endif
}  __x_attribute_packed__ CDVD_REPORT_AGID_DATA, *PCDVD_REPORT_AGID_DATA;

typedef struct _CDVD_CHALLENGE_KEY_DATA {
    xuint8 ChallengeKeyValue[10];
    xuint8 Reserved[2];
}  __x_attribute_packed__ CDVD_CHALLENGE_KEY_DATA, *PCDVD_CHALLENGE_KEY_DATA;

typedef struct _CDVD_KEY_DATA {
    xuint8 Key[5];
    xuint8 Reserved[3];
}  __x_attribute_packed__ CDVD_KEY_DATA, *PCDVD_KEY_DATA;

typedef struct _CDVD_REPORT_ASF_DATA {
    xuint8 Reserved1[3];
#if defined(__LITTLE_ENDIAN_BITFIELD)       
    xuint8 Success : 1;
    xuint8 Reserved2 : 7;
#else
    xuint8 Reserved2 : 7;
    xuint8 Success : 1;
#endif
}  __x_attribute_packed__ CDVD_REPORT_ASF_DATA, *PCDVD_REPORT_ASF_DATA;

typedef struct _CDVD_TITLE_KEY_HEADER {
    xuint8 DataLength[2];
    xuint8 Reserved1[1];
#if defined(__LITTLE_ENDIAN_BITFIELD)           
    xuint8 Reserved2 : 3;
    xuint8 CGMS : 2;
    xuint8 CP_SEC : 1;
    xuint8 CPM : 1;
    xuint8 Zero : 1;
#else
    xuint8 Zero : 1;
    xuint8 CPM : 1;
    xuint8 CP_SEC : 1;
    xuint8 CGMS : 2;
    xuint8 Reserved2 : 3;
#endif
    CDVD_KEY_DATA TitleKey;
}  __x_attribute_packed__  CDVD_TITLE_KEY_HEADER, *PCDVD_TITLE_KEY_HEADER;

//
// Read Formatted Capacity Data - returned in Big Endian Format
//



typedef struct _FORMATTED_CAPACITY_DESCRIPTOR {
    xuint8 NumberOfBlocks[4];
#if defined(__LITTLE_ENDIAN_BITFIELD)           
    xuint8 Maximum : 1;
    xuint8 Valid : 1;
    xuint8 Reserved:6;
#else
    xuint8 Reserved:6;
    xuint8 Valid : 1;
    xuint8 Maximum : 1;
#endif
    xuint8 BlockLength[3];
}  __x_attribute_packed__ FORMATTED_CAPACITY_DESCRIPTOR, *PFORMATTED_CAPACITY_DESCRIPTOR;

typedef struct _FORMATTED_CAPACITY_LIST {
    xuint8 Reserved[3];
    xuint8 CapacityListLength;
}  __x_attribute_packed__ FORMATTED_CAPACITY_LIST, *PFORMATTED_CAPACITY_LIST;


//
//      BLANK command blanking type codes
//

#define BLANK_FULL              0x0
#define BLANK_MINIMAL           0x1
#define BLANK_TRACK             0x2
#define BLANK_UNRESERVE_TRACK   0x3
#define BLANK_TAIL              0x4
#define BLANK_UNCLOSE_SESSION   0x5
#define BLANK_SESSION           0x6

//
// PLAY_CD definitions and constants
//

#define CD_EXPECTED_SECTOR_ANY          0x0
#define CD_EXPECTED_SECTOR_CDDA         0x1
#define CD_EXPECTED_SECTOR_MODE1        0x2
#define CD_EXPECTED_SECTOR_MODE2        0x3
#define CD_EXPECTED_SECTOR_MODE2_FORM1  0x4
#define CD_EXPECTED_SECTOR_MODE2_FORM2  0x5

//
// Read Disk Information Definitions and Capabilities
//

#define DISK_STATUS_EMPTY       0x00
#define DISK_STATUS_INCOMPLETE  0x01
#define DISK_STATUS_COMPLETE    0x02

#define LAST_SESSION_EMPTY              0x00
#define LAST_SESSION_INCOMPLETE         0x01
#define LAST_SESSION_RESERVED_DAMAGED   0x02
#define LAST_SESSION_COMPLETE           0x03

#define DISK_TYPE_CDDA          0x00
#define DISK_TYPE_CDI           0x10
#define DISK_TYPE_XA            0x20
#define DISK_TYPE_UNDEFINED     0xFF

//
//  Values for MrwStatus field.
//

#define DISC_BGFORMAT_STATE_NONE        0x0
#define DISC_BGFORMAT_STATE_INCOMPLETE  0x1
#define DISC_BGFORMAT_STATE_RUNNING     0x2
#define DISC_BGFORMAT_STATE_COMPLETE    0x3



typedef struct _OPC_TABLE_ENTRY {
    xuint8 Speed[2];
    xuint8 OPCValue[6];
}  __x_attribute_packed__ OPC_TABLE_ENTRY, *POPC_TABLE_ENTRY;

typedef struct _DISC_INFORMATION {

    xuint8 Length[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 DiscStatus        : 2;
    xuint8 LastSessionStatus : 2;
    xuint8 Erasable          : 1;
    xuint8 Reserved1         : 3;
#else
    xuint8 Reserved1         : 3;
    xuint8 Erasable          : 1;
    xuint8 LastSessionStatus : 2;
    xuint8 DiscStatus        : 2;
#endif
    xuint8 FirstTrackNumber;

    xuint8 NumberOfSessionsLsb;
    xuint8 LastSessionFirstTrackLsb;
    xuint8 LastSessionLastTrackLsb;
#if defined(__LITTLE_ENDIAN_BITFIELD)        
    xuint8 MrwStatus   : 2;
    xuint8 MrwDirtyBit : 1;
    xuint8 Reserved2   : 2;
    xuint8 URU         : 1;
    xuint8 DBC_V       : 1;
    xuint8 DID_V       : 1;
#else
    xuint8 DID_V       : 1;
    xuint8 DBC_V       : 1;
    xuint8 URU         : 1;
    xuint8 Reserved2   : 2; 
    xuint8 MrwDirtyBit : 1;
    xuint8 MrwStatus   : 2;
#endif
    xuint8 DiscType;
    xuint8 NumberOfSessionsMsb;
    xuint8 LastSessionFirstTrackMsb;
    xuint8 LastSessionLastTrackMsb;

    xuint8 DiskIdentification[4];
    xuint8 LastSessionLeadIn[4];     // HMSF
    xuint8 LastPossibleLeadOutStartTime[4]; // HMSF
    xuint8 DiskBarCode[8];

    xuint8 Reserved4;
    xuint8 NumberOPCEntries;
    OPC_TABLE_ENTRY OPCTable[ 1 ]; // can be many of these here....

} __x_attribute_packed__  DISC_INFORMATION, *PDISC_INFORMATION;

// TODO: Deprecate DISK_INFORMATION
//#if PRAGMA_DEPRECATED_DDK
//#pragma deprecated(_DISK_INFORMATION)  // Use DISC_INFORMATION, note size change
//#pragma deprecated( DISK_INFORMATION)  // Use DISC_INFORMATION, note size change
//#pragma deprecated(PDISK_INFORMATION)  // Use DISC_INFORMATION, note size change
//#endif

typedef struct _DISK_INFORMATION {
    xuint8 Length[2];
#if defined(__LITTLE_ENDIAN_BITFIELD) 
    xuint8 DiskStatus : 2;
    xuint8 LastSessionStatus : 2;
    xuint8 Erasable : 1;
    xuint8 Reserved1 : 3;
#else
    xuint8 Reserved1 : 3;
    xuint8 Erasable : 1;
    xuint8 LastSessionStatus : 2;
    xuint8 DiskStatus : 2;
#endif
    xuint8 FirstTrackNumber;
    xuint8 NumberOfSessions;
    xuint8 LastSessionFirstTrack;
    xuint8 LastSessionLastTrack;
#if defined(__LITTLE_ENDIAN_BITFIELD) 
    xuint8 Reserved2 : 5;
    xuint8 GEN : 1;
    xuint8 DBC_V : 1;
    xuint8 DID_V : 1;
#else
    xuint8 DID_V : 1;
    xuint8 DBC_V : 1;
    xuint8 GEN : 1;    
    xuint8 Reserved2 : 5;
#endif
    xuint8 DiskType;
    xuint8 Reserved3[3];

    xuint8 DiskIdentification[4];
    xuint8 LastSessionLeadIn[4];     // MSF
    xuint8 LastPossibleStartTime[4]; // MSF
    xuint8 DiskBarCode[8];

    xuint8 Reserved4;
    xuint8 NumberOPCEntries;
}    __x_attribute_packed__ DISK_INFORMATION, *PDISK_INFORMATION;



//
// Read Header definitions and structures
//

typedef struct _DATA_BLOCK_HEADER {
    xuint8 DataMode;
    xuint8 Reserved[4];
    union {
        xuint8 LogicalBlockAddress[4];
        struct {
            xuint8 Reserved;
            xuint8 M;
            xuint8 S;
            xuint8 F;
        } MSF;
    }u1;
}   __x_attribute_packed__ DATA_BLOCK_HEADER, *PDATA_BLOCK_HEADER;



#define DATA_BLOCK_MODE0    0x0
#define DATA_BLOCK_MODE1    0x1
#define DATA_BLOCK_MODE2    0x2

//
// Read TOC Format Codes
//

#define READ_TOC_FORMAT_TOC         0x00
#define READ_TOC_FORMAT_SESSION     0x01
#define READ_TOC_FORMAT_FULL_TOC    0x02
#define READ_TOC_FORMAT_PMA         0x03
#define READ_TOC_FORMAT_ATIP        0x04

// TODO: Deprecate TRACK_INFORMATION structure, use TRACK_INFORMATION2 instead

typedef struct _TRACK_INFORMATION {
    xuint8 Length[2];
    xuint8 TrackNumber;
    xuint8 SessionNumber;
    xuint8 Reserved1;
#if defined(__LITTLE_ENDIAN_BITFIELD)     
    xuint8 TrackMode : 4;
    xuint8 Copy      : 1;
    xuint8 Damage    : 1;
    xuint8 Reserved2 : 2;
    
    xuint8 DataMode : 4;
    xuint8 FP       : 1;
    xuint8 Packet   : 1;
    xuint8 Blank    : 1;
    xuint8 RT       : 1;
    
    xuint8 NWA_V     : 1;
    xuint8 Reserved3 : 7;
#else
    xuint8 Reserved2 : 2;
    xuint8 Damage    : 1;
    xuint8 Copy      : 1;
    xuint8 TrackMode : 4;

    xuint8 RT       : 1;
    xuint8 Blank    : 1;
    xuint8 Packet   : 1;    
    xuint8 FP       : 1;    
    xuint8 DataMode : 4;

    xuint8 Reserved3 : 7;   
    xuint8 NWA_V     : 1;
#endif
    xuint8 TrackStartAddress[4];
    xuint8 NextWritableAddress[4];
    xuint8 FreeBlocks[4];
    xuint8 FixedPacketSize[4];
}   __x_attribute_packed__  TRACK_INFORMATION, *PTRACK_INFORMATION;

// Second Revision Modifies:
// * Longer names for some fields
// * LSB to track/session number fields
// * LRA_V bit
// Second Revision Adds:
// * TrackSize
// * LastRecordedAddress
// * MSB to track/session
// * Two reserved bytes
// Total structure size increased by 12 (0x0C) bytes
typedef struct _TRACK_INFORMATION2 {

    xuint8 Length[2];
    xuint8 TrackNumberLsb;
    xuint8 SessionNumberLsb;

    xuint8 Reserved4;
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 TrackMode : 4;
    xuint8 Copy      : 1;
    xuint8 Damage    : 1;
    xuint8 Reserved5 : 2;
    
    xuint8 DataMode      : 4;
    xuint8 FixedPacket   : 1;
    xuint8 Packet        : 1;
    xuint8 Blank         : 1;
    xuint8 ReservedTrack : 1;

    xuint8 NWA_V     : 1;
    xuint8 LRA_V     : 1;
    xuint8 Reserved6 : 6;
#else
    xuint8 Reserved5 : 2;
    xuint8 Damage    : 1;
    xuint8 Copy      : 1;
    xuint8 TrackMode : 4;

    xuint8 ReservedTrack : 1;
    xuint8 Blank         : 1;
    xuint8 Packet        : 1;
    xuint8 FixedPacket   : 1;
    xuint8 DataMode      : 4;
    
    xuint8 Reserved6 : 6;
    xuint8 LRA_V     : 1;    
    xuint8 NWA_V     : 1;
#endif
    xuint8 TrackStartAddress[4];
    xuint8 NextWritableAddress[4];
    xuint8 FreeBlocks[4];
    xuint8 FixedPacketSize[4]; // blocking factor
    xuint8 TrackSize[4];
    xuint8 LastRecordedAddress[4];

    xuint8 TrackNumberMsb;
    xuint8 SessionNumberMsb;
    xuint8 Reserved7[2];

}    __x_attribute_packed__ TRACK_INFORMATION2, *PTRACK_INFORMATION2;

// Third Revision Adds
// * ReadCompatibilityLBA
// Total structure size increased by 4 bytes
typedef struct _TRACK_INFORMATION3 {

    xuint8 Length[2];
    xuint8 TrackNumberLsb;
    xuint8 SessionNumberLsb;

    xuint8 Reserved4;
#if defined(__LITTLE_ENDIAN_BITFIELD)             
    xuint8 TrackMode : 4;
    xuint8 Copy      : 1;
    xuint8 Damage    : 1;
    xuint8 Reserved5 : 2;
    
    xuint8 DataMode      : 4;
    xuint8 FixedPacket   : 1;
    xuint8 Packet        : 1;
    xuint8 Blank         : 1;
    xuint8 ReservedTrack : 1;
    
    xuint8 NWA_V     : 1;
    xuint8 LRA_V     : 1;
    xuint8 Reserved6 : 6;
#else
    xuint8 Reserved5 : 2;
    xuint8 Damage    : 1;
    xuint8 Copy      : 1;
    xuint8 TrackMode : 4;

    xuint8 ReservedTrack : 1;    
    xuint8 Blank         : 1;
    xuint8 Packet        : 1;
    xuint8 FixedPacket   : 1;
    xuint8 DataMode      : 4;

    xuint8 Reserved6 : 6;   
    xuint8 LRA_V     : 1;    
    xuint8 NWA_V     : 1;
#endif
    xuint8 TrackStartAddress[4];
    xuint8 NextWritableAddress[4];
    xuint8 FreeBlocks[4];
    xuint8 FixedPacketSize[4]; // blocking factor
    xuint8 TrackSize[4];
    xuint8 LastRecordedAddress[4];

    xuint8 TrackNumberMsb;
    xuint8 SessionNumberMsb;
    xuint8 Reserved7[2];
    xuint8 ReadCompatibilityLba[4];

}    __x_attribute_packed__ TRACK_INFORMATION3, *PTRACK_INFORMATION3;


typedef struct _PERFORMANCE_DESCRIPTOR {
#if defined(__LITTLE_ENDIAN_BITFIELD)     
    xuint8 RandomAccess         : 1;
    xuint8 Exact                : 1;
    xuint8 RestoreDefaults      : 1;
    xuint8 WriteRotationControl : 2;
    xuint8 Reserved1            : 3;
#else
    xuint8 Reserved1            : 3;
    xuint8 WriteRotationControl : 2;
    xuint8 RestoreDefaults      : 1;  
    xuint8 Exact                : 1;
    xuint8 RandomAccess         : 1;
#endif
    xuint8 Reserved[3];
    xuint8 StartLba[4];
    xuint8 EndLba[4];
    xuint8 ReadSize[4];
    xuint8 ReadTime[4];
    xuint8 WriteSize[4];
    xuint8 WriteTime[4];

}    __x_attribute_packed__ PERFORMANCE_DESCRIPTOR, *PPERFORMANCE_DESCRIPTOR;


//
// Command Descriptor Block constants.
//

#define CDB6GENERIC_LENGTH                   6
#define CDB10GENERIC_LENGTH                  10
#define CDB12GENERIC_LENGTH                  12

#define SETBITON                             1
#define SETBITOFF                            0

//
// Mode Sense/Select page constants.
//

#define MODE_PAGE_VENDOR_SPECIFIC       0x00
#define MODE_PAGE_ERROR_RECOVERY        0x01
#define MODE_PAGE_DISCONNECT            0x02
#define MODE_PAGE_FORMAT_DEVICE         0x03 // disk
#define MODE_PAGE_MRW                   0x03 // cdrom
#define MODE_PAGE_RIGID_GEOMETRY        0x04
#define MODE_PAGE_FLEXIBILE             0x05 // disk
#define MODE_PAGE_WRITE_PARAMETERS      0x05 // cdrom
#define MODE_PAGE_VERIFY_ERROR          0x07
#define MODE_PAGE_CACHING               0x08
#define MODE_PAGE_PERIPHERAL            0x09
#define MODE_PAGE_CONTROL               0x0A
#define MODE_PAGE_MEDIUM_TYPES          0x0B
#define MODE_PAGE_NOTCH_PARTITION       0x0C
#define MODE_PAGE_CD_AUDIO_CONTROL      0x0E
#define MODE_PAGE_DATA_COMPRESS         0x0F
#define MODE_PAGE_DEVICE_CONFIG         0x10
#define MODE_PAGE_XOR_CONTROL           0x10 // disk
#define MODE_PAGE_MEDIUM_PARTITION      0x11
#define MODE_PAGE_ENCLOSURE_SERVICES_MANAGEMENT 0x14
#define MODE_PAGE_EXTENDED              0x15
#define MODE_PAGE_EXTENDED_DEVICE_SPECIFIC 0x16
#define MODE_PAGE_CDVD_FEATURE_SET      0x18
#define MODE_PAGE_PROTOCOL_SPECIFIC_LUN 0x18
#define MODE_PAGE_PROTOCOL_SPECIFIC_PORT 0x19
#define MODE_PAGE_POWER_CONDITION       0x1A
#define MODE_PAGE_LUN_MAPPING           0x1B
#define MODE_PAGE_FAULT_REPORTING       0x1C
#define MODE_PAGE_CDVD_INACTIVITY       0x1D // cdrom
#define MODE_PAGE_ELEMENT_ADDRESS       0x1D
#define MODE_PAGE_TRANSPORT_GEOMETRY    0x1E
#define MODE_PAGE_DEVICE_CAPABILITIES   0x1F
#define MODE_PAGE_CAPABILITIES          0x2A // cdrom

#define MODE_SENSE_RETURN_ALL           0x3f

#define MODE_SENSE_CURRENT_VALUES       0x00
#define MODE_SENSE_CHANGEABLE_VALUES    0x40
#define MODE_SENSE_DEFAULT_VAULES       0x80
#define MODE_SENSE_SAVED_VALUES         0xc0


//
// SCSI CDB operation codes
//

// 6-byte commands:
#define SCSIOP_TEST_UNIT_READY          0x00
#define SCSIOP_REZERO_UNIT              0x01
#define SCSIOP_REWIND                   0x01
#define SCSIOP_REQUEST_BLOCK_ADDR       0x02
#define SCSIOP_REQUEST_SENSE            0x03
#define SCSIOP_FORMAT_UNIT              0x04
#define SCSIOP_READ_BLOCK_LIMITS        0x05
#define SCSIOP_REASSIGN_BLOCKS          0x07
#define SCSIOP_INIT_ELEMENT_STATUS      0x07
#define SCSIOP_READ6                    0x08
#define SCSIOP_RECEIVE                  0x08
#define SCSIOP_WRITE6                   0x0A
#define SCSIOP_PRINT                    0x0A
#define SCSIOP_SEND                     0x0A
#define SCSIOP_SEEK6                    0x0B
#define SCSIOP_TRACK_SELECT             0x0B
#define SCSIOP_SLEW_PRINT               0x0B
#define SCSIOP_SET_CAPACITY             0x0B // tape
#define SCSIOP_SEEK_BLOCK               0x0C
#define SCSIOP_PARTITION                0x0D
#define SCSIOP_READ_REVERSE             0x0F
#define SCSIOP_WRITE_FILEMARKS          0x10
#define SCSIOP_FLUSH_BUFFER             0x10
#define SCSIOP_SPACE                    0x11
#define SCSIOP_INQUIRY                  0x12
#define SCSIOP_VERIFY6                  0x13
#define SCSIOP_RECOVER_BUF_DATA         0x14
#define SCSIOP_MODE_SELECT              0x15
#define SCSIOP_RESERVE_UNIT             0x16
#define SCSIOP_RELEASE_UNIT             0x17
#define SCSIOP_COPY                     0x18
#define SCSIOP_ERASE                    0x19
#define SCSIOP_MODE_SENSE               0x1A
#define SCSIOP_START_STOP_UNIT          0x1B
#define SCSIOP_STOP_PRINT               0x1B
#define SCSIOP_LOAD_UNLOAD              0x1B
#define SCSIOP_RECEIVE_DIAGNOSTIC       0x1C
#define SCSIOP_SEND_DIAGNOSTIC          0x1D
#define SCSIOP_MEDIUM_REMOVAL           0x1E

// 10-byte commands
#define SCSIOP_READ_FORMATTED_CAPACITY  0x23
#define SCSIOP_READ_CAPACITY            0x25
#define SCSIOP_READ                     0x28
#define SCSIOP_WRITE                    0x2A
#define SCSIOP_SEEK                     0x2B
#define SCSIOP_LOCATE                   0x2B
#define SCSIOP_POSITION_TO_ELEMENT      0x2B
#define SCSIOP_WRITE_VERIFY             0x2E
#define SCSIOP_VERIFY                   0x2F
#define SCSIOP_SEARCH_DATA_HIGH         0x30
#define SCSIOP_SEARCH_DATA_EQUAL        0x31
#define SCSIOP_SEARCH_DATA_LOW          0x32
#define SCSIOP_SET_LIMITS               0x33
#define SCSIOP_READ_POSITION            0x34
#define SCSIOP_SYNCHRONIZE_CACHE        0x35
#define SCSIOP_COMPARE                  0x39
#define SCSIOP_COPY_COMPARE             0x3A
#define SCSIOP_WRITE_DATA_BUFF          0x3B
#define SCSIOP_READ_DATA_BUFF           0x3C
#define SCSIOP_WRITE_LONG               0x3F
#define SCSIOP_CHANGE_DEFINITION        0x40
#define SCSIOP_WRITE_SAME               0x41
#define SCSIOP_READ_SUB_CHANNEL         0x42
#define SCSIOP_READ_TOC                 0x43
#define SCSIOP_READ_HEADER              0x44
#define SCSIOP_REPORT_DENSITY_SUPPORT   0x44 // tape
#define SCSIOP_PLAY_AUDIO               0x45
#define SCSIOP_GET_CONFIGURATION        0x46
#define SCSIOP_PLAY_AUDIO_MSF           0x47
#define SCSIOP_PLAY_TRACK_INDEX         0x48
#define SCSIOP_PLAY_TRACK_RELATIVE      0x49
#define SCSIOP_GET_EVENT_STATUS         0x4A
#define SCSIOP_PAUSE_RESUME             0x4B
#define SCSIOP_LOG_SELECT               0x4C
#define SCSIOP_LOG_SENSE                0x4D
#define SCSIOP_STOP_PLAY_SCAN           0x4E
#define SCSIOP_XDWRITE                  0x50
#define SCSIOP_XPWRITE                  0x51
#define SCSIOP_READ_DISK_INFORMATION    0x51
#define SCSIOP_READ_DISC_INFORMATION    0x51 // proper use of disc over disk
#define SCSIOP_READ_TRACK_INFORMATION   0x52
#define SCSIOP_XDWRITE_READ             0x53
#define SCSIOP_RESERVE_TRACK_RZONE      0x53
#define SCSIOP_SEND_OPC_INFORMATION     0x54 // optimum power calibration
#define SCSIOP_MODE_SELECT10            0x55
#define SCSIOP_RESERVE_UNIT10           0x56
#define SCSIOP_RESERVE_ELEMENT          0x56
#define SCSIOP_RELEASE_UNIT10           0x57
#define SCSIOP_RELEASE_ELEMENT          0x57
#define SCSIOP_REPAIR_TRACK             0x58
#define SCSIOP_MODE_SENSE10             0x5A
#define SCSIOP_CLOSE_TRACK_SESSION      0x5B
#define SCSIOP_READ_BUFFER_CAPACITY     0x5C
#define SCSIOP_SEND_CUE_SHEET           0x5D
#define SCSIOP_PERSISTENT_RESERVE_IN    0x5E
#define SCSIOP_PERSISTENT_RESERVE_OUT   0x5F

// 12-byte commands
#define SCSIOP_REPORT_LUNS              0xA0
#define SCSIOP_BLANK                    0xA1
#define SCSIOP_ATA_PASSTHROUGH12        0xA1
#define SCSIOP_SEND_EVENT               0xA2
#define SCSIOP_SEND_KEY                 0xA3
#define SCSIOP_MAINTENANCE_IN           0xA3
#define SCSIOP_REPORT_KEY               0xA4
#define SCSIOP_MAINTENANCE_OUT          0xA4
#define SCSIOP_MOVE_MEDIUM              0xA5
#define SCSIOP_LOAD_UNLOAD_SLOT         0xA6
#define SCSIOP_EXCHANGE_MEDIUM          0xA6
#define SCSIOP_SET_READ_AHEAD           0xA7
#define SCSIOP_MOVE_MEDIUM_ATTACHED     0xA7
#define SCSIOP_READ12                   0xA8
#define SCSIOP_GET_MESSAGE              0xA8
#define SCSIOP_SERVICE_ACTION_OUT12     0xA9
#define SCSIOP_WRITE12                  0xAA
#define SCSIOP_SEND_MESSAGE             0xAB
#define SCSIOP_SERVICE_ACTION_IN12      0xAB
#define SCSIOP_GET_PERFORMANCE          0xAC
#define SCSIOP_READ_DVD_STRUCTURE       0xAD
#define SCSIOP_WRITE_VERIFY12           0xAE
#define SCSIOP_VERIFY12                 0xAF
#define SCSIOP_SEARCH_DATA_HIGH12       0xB0
#define SCSIOP_SEARCH_DATA_EQUAL12      0xB1
#define SCSIOP_SEARCH_DATA_LOW12        0xB2
#define SCSIOP_SET_LIMITS12             0xB3
#define SCSIOP_READ_ELEMENT_STATUS_ATTACHED 0xB4
#define SCSIOP_REQUEST_VOL_ELEMENT      0xB5
#define SCSIOP_SEND_VOLUME_TAG          0xB6
#define SCSIOP_SET_STREAMING            0xB6 // C/DVD
#define SCSIOP_READ_DEFECT_DATA         0xB7
#define SCSIOP_READ_ELEMENT_STATUS      0xB8
#define SCSIOP_READ_CD_MSF              0xB9
#define SCSIOP_SCAN_CD                  0xBA
#define SCSIOP_REDUNDANCY_GROUP_IN      0xBA
#define SCSIOP_SET_CD_SPEED             0xBB
#define SCSIOP_REDUNDANCY_GROUP_OUT     0xBB
#define SCSIOP_PLAY_CD                  0xBC
#define SCSIOP_SPARE_IN                 0xBC
#define SCSIOP_MECHANISM_STATUS         0xBD
#define SCSIOP_SPARE_OUT                0xBD
#define SCSIOP_READ_CD                  0xBE
#define SCSIOP_VOLUME_SET_IN            0xBE
#define SCSIOP_SEND_DVD_STRUCTURE       0xBF
#define SCSIOP_VOLUME_SET_OUT           0xBF
#define SCSIOP_INIT_ELEMENT_RANGE       0xE7

// 16-byte commands
#define SCSIOP_XDWRITE_EXTENDED16       0x80 // disk
#define SCSIOP_WRITE_FILEMARKS16        0x80 // tape
#define SCSIOP_REBUILD16                0x81 // disk
#define SCSIOP_READ_REVERSE16           0x81 // tape
#define SCSIOP_REGENERATE16             0x82 // disk
#define SCSIOP_EXTENDED_COPY            0x83
#define SCSIOP_RECEIVE_COPY_RESULTS     0x84
#define SCSIOP_ATA_PASSTHROUGH16        0x85
#define SCSIOP_ACCESS_CONTROL_IN        0x86
#define SCSIOP_ACCESS_CONTROL_OUT       0x87
#define SCSIOP_READ16                   0x88
#define SCSIOP_WRITE16                  0x8A
#define SCSIOP_READ_ATTRIBUTES          0x8C
#define SCSIOP_WRITE_ATTRIBUTES         0x8D
#define SCSIOP_WRITE_VERIFY16           0x8E
#define SCSIOP_VERIFY16                 0x8F
#define SCSIOP_PREFETCH16               0x90
#define SCSIOP_SYNCHRONIZE_CACHE16      0x91
#define SCSIOP_SPACE16                  0x91 // tape
#define SCSIOP_LOCK_UNLOCK_CACHE16      0x92
#define SCSIOP_LOCATE16                 0x92 // tape
#define SCSIOP_WRITE_SAME16             0x93
#define SCSIOP_ERASE16                  0x93 // tape
#define SCSIOP_READ_CAPACITY16          0x9E
#define SCSIOP_SERVICE_ACTION_IN16      0x9E
#define SCSIOP_SERVICE_ACTION_OUT16     0x9F


//
// If the IMMED bit is 1, status is returned as soon
// as the operation is initiated. If the IMMED bit
// is 0, status is not returned until the operation
// is completed.
//

#define CDB_RETURN_ON_COMPLETION   0
#define CDB_RETURN_IMMEDIATE       1

// end_ntminitape

//
// CDB Force media access used in extended read and write commands.
//

#define CDB_FORCE_MEDIA_ACCESS 0x08

//
// Denon CD ROM operation codes
//

#define SCSIOP_DENON_EJECT_DISC    0xE6
#define SCSIOP_DENON_STOP_AUDIO    0xE7
#define SCSIOP_DENON_PLAY_AUDIO    0xE8
#define SCSIOP_DENON_READ_TOC      0xE9
#define SCSIOP_DENON_READ_SUBCODE  0xEB

//
// SCSI Bus Messages
//

#define SCSIMESS_ABORT                0x06
#define SCSIMESS_ABORT_WITH_TAG       0x0D
#define SCSIMESS_BUS_DEVICE_RESET     0X0C
#define SCSIMESS_CLEAR_QUEUE          0X0E
#define SCSIMESS_COMMAND_COMPLETE     0X00
#define SCSIMESS_DISCONNECT           0X04
#define SCSIMESS_EXTENDED_MESSAGE     0X01
#define SCSIMESS_IDENTIFY             0X80
#define SCSIMESS_IDENTIFY_WITH_DISCON 0XC0
#define SCSIMESS_IGNORE_WIDE_RESIDUE  0X23
#define SCSIMESS_INITIATE_RECOVERY    0X0F
#define SCSIMESS_INIT_DETECTED_ERROR  0X05
#define SCSIMESS_LINK_CMD_COMP        0X0A
#define SCSIMESS_LINK_CMD_COMP_W_FLAG 0X0B
#define SCSIMESS_MESS_PARITY_ERROR    0X09
#define SCSIMESS_MESSAGE_REJECT       0X07
#define SCSIMESS_NO_OPERATION         0X08
#define SCSIMESS_HEAD_OF_QUEUE_TAG    0X21
#define SCSIMESS_ORDERED_QUEUE_TAG    0X22
#define SCSIMESS_SIMPLE_QUEUE_TAG     0X20
#define SCSIMESS_RELEASE_RECOVERY     0X10
#define SCSIMESS_RESTORE_POINTERS     0X03
#define SCSIMESS_SAVE_DATA_POINTER    0X02
#define SCSIMESS_TERMINATE_IO_PROCESS 0X11

//
// SCSI Extended Message operation codes
//

#define SCSIMESS_MODIFY_DATA_POINTER  0X00
#define SCSIMESS_SYNCHRONOUS_DATA_REQ 0X01
#define SCSIMESS_WIDE_DATA_REQUEST    0X03

//
// SCSI Extended Message Lengths
//

#define SCSIMESS_MODIFY_DATA_LENGTH   5
#define SCSIMESS_SYNCH_DATA_LENGTH    3
#define SCSIMESS_WIDE_DATA_LENGTH     2

//
// SCSI extended message structure
//


typedef struct _SCSI_EXTENDED_MESSAGE {
    xuint8 InitialMessageCode;
    xuint8 MessageLength;
    xuint8 MessageType;
    union _EXTENDED_ARGUMENTS {

        struct {
            xuint8 Modifier[4];
        } Modify;

        struct {
            xuint8 TransferPeriod;
            xuint8 ReqAckOffset;
        } Synchronous;

        struct{
            xuint8 Width;
        } Wide;
    }ExtendedArguments;
}   __x_attribute_packed__ SCSI_EXTENDED_MESSAGE, *PSCSI_EXTENDED_MESSAGE;

//
// SCSI bus status codes.
//

#define SCSISTAT_GOOD                  0x00
#define SCSISTAT_CHECK_CONDITION       0x02
#define SCSISTAT_CONDITION_MET         0x04
#define SCSISTAT_BUSY                  0x08
#define SCSISTAT_INTERMEDIATE          0x10
#define SCSISTAT_INTERMEDIATE_COND_MET 0x14
#define SCSISTAT_RESERVATION_CONFLICT  0x18
#define SCSISTAT_COMMAND_TERMINATED    0x22
#define SCSISTAT_QUEUE_FULL            0x28

//
// Enable Vital Product Data Flag (EVPD)
// used with INQUIRY command.
//

#define CDB_INQUIRY_EVPD           0x01

//
// Defines for format CDB
//

#define LUN0_FORMAT_SAVING_DEFECT_LIST 0
#define USE_DEFAULTMSB  0
#define USE_DEFAULTLSB  0

#define START_UNIT_CODE 0x01
#define STOP_UNIT_CODE  0x00

// begin_ntminitape

//
// Inquiry buffer structure. This is the data returned from the target
// after it receives an inquiry.
//
// This structure may be extended by the number of bytes specified
// in the field AdditionalLength. The defined size constant only
// includes fields through ProductRevisionLevel.
//
// The NT SCSI drivers are only interested in the first 36 bytes of data.
//

#define INQUIRYDATABUFFERSIZE 36


typedef struct _INQUIRYDATA {
#if defined(__LITTLE_ENDIAN_BITFIELD)    
    xuint8 DeviceType : 5;
    xuint8 DeviceTypeQualifier : 3;
    
    xuint8 DeviceTypeModifier : 7;
    xuint8 RemovableMedia : 1;
    
    union {
        xuint8 Versions;
        struct {
            xuint8 ANSIVersion : 3;
            xuint8 ECMAVersion : 3;
            xuint8 ISOVersion : 2;
        }ct;
    }u1;
    
    xuint8 ResponseDataFormat : 4;
    xuint8 HiSupport : 1;
    xuint8 NormACA : 1;
    xuint8 TerminateTask : 1;
    xuint8 AERC : 1;
    
    xuint8 AdditionalLength;
    xuint8 Reserved;
    xuint8 Addr16 : 1;               // defined only for SIP devices.
    xuint8 Addr32 : 1;               // defined only for SIP devices.
    xuint8 AckReqQ: 1;               // defined only for SIP devices.
    xuint8 MediumChanger : 1;
    xuint8 MultiPort : 1;
    xuint8 ReservedBit2 : 1;
    xuint8 EnclosureServices : 1;
    xuint8 ReservedBit3 : 1;
    
    xuint8 SoftReset : 1;
    xuint8 CommandQueue : 1;
    xuint8 TransferDisable : 1;      // defined only for SIP devices.
    xuint8 LinkedCommands : 1;
    xuint8 Synchronous : 1;          // defined only for SIP devices.
    xuint8 Wide16Bit : 1;            // defined only for SIP devices.
    xuint8 Wide32Bit : 1;            // defined only for SIP devices.
    xuint8 RelativeAddressing : 1;
#else
    xuint8 DeviceTypeQualifier : 3;
    xuint8 DeviceType : 5;

    xuint8 RemovableMedia : 1;  
    xuint8 DeviceTypeModifier : 7;


    union {
        xuint8 Versions;
        struct {
            xuint8 ISOVersion : 2;
            xuint8 ECMAVersion : 3;            
            xuint8 ANSIVersion : 3;
        }ct;
    }u1;

    xuint8 AERC : 1;
    xuint8 TerminateTask : 1;
    xuint8 NormACA : 1;
    xuint8 HiSupport : 1;
    xuint8 ResponseDataFormat : 4;

    xuint8 AdditionalLength;
    xuint8 Reserved;

    xuint8 ReservedBit3 : 1;
    xuint8 EnclosureServices : 1;
    xuint8 ReservedBit2 : 1;   
    xuint8 MultiPort : 1;
    xuint8 MediumChanger : 1;
    xuint8 AckReqQ: 1;               // defined only for SIP devices.
    xuint8 Addr32 : 1;               // defined only for SIP devices.      
    xuint8 Addr16 : 1;               // defined only for SIP devices.
    
    xuint8 RelativeAddressing : 1;
    xuint8 Wide32Bit : 1;            // defined only for SIP devices.
    xuint8 Wide16Bit : 1;            // defined only for SIP devices.
    xuint8 Synchronous : 1;          // defined only for SIP devices.
    xuint8 LinkedCommands : 1;
    xuint8 TransferDisable : 1;      // defined only for SIP devices.
    xuint8 CommandQueue : 1;     
    xuint8 SoftReset : 1;
#endif
    xuint8 VendorId[8];
    xuint8 ProductId[16];
    xuint8 ProductRevisionLevel[4];
    xuint8 VendorSpecific[20];
    xuint8 Reserved3[40];
}    __x_attribute_packed__  INQUIRYDATA, *PINQUIRYDATA;

//
// Inquiry defines. Used to interpret data returned from target as result
// of inquiry command.
//
// DeviceType field
//

#define DIRECT_ACCESS_DEVICE            0x00    // disks
#define SEQUENTIAL_ACCESS_DEVICE        0x01    // tapes
#define PRINTER_DEVICE                  0x02    // printers
#define PROCESSOR_DEVICE                0x03    // scanners, printers, etc
#define WRITE_ONCE_READ_MULTIPLE_DEVICE 0x04    // worms
#define READ_ONLY_DIRECT_ACCESS_DEVICE  0x05    // cdroms
#define SCANNER_DEVICE                  0x06    // scanners
#define OPTICAL_DEVICE                  0x07    // optical disks
#define MEDIUM_CHANGER                  0x08    // jukebox
#define COMMUNICATION_DEVICE            0x09    // network
// 0xA and 0xB are obsolete
#define ARRAY_CONTROLLER_DEVICE         0x0C
#define SCSI_ENCLOSURE_DEVICE           0x0D
#define REDUCED_BLOCK_DEVICE            0x0E    // e.g., 1394 disk
#define OPTICAL_CARD_READER_WRITER_DEVICE 0x0F
#define BRIDGE_CONTROLLER_DEVICE        0x10
#define OBJECT_BASED_STORAGE_DEVICE     0x11    // OSD
#define LOGICAL_UNIT_NOT_PRESENT_DEVICE 0x7F

#define DEVICE_QUALIFIER_ACTIVE         0x00
#define DEVICE_QUALIFIER_NOT_ACTIVE     0x01
#define DEVICE_QUALIFIER_NOT_SUPPORTED  0x03

//
// DeviceTypeQualifier field
//

#define DEVICE_CONNECTED 0x00

//
// Vital Product Data Pages
//

//
// Unit Serial Number Page (page code 0x80)
//
// Provides a product serial number for the target or the logical unit.
//

typedef struct _VPD_MEDIA_SERIAL_NUMBER_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)  
    xuint8 DeviceType : 5;
    xuint8 DeviceTypeQualifier : 3;
#else
    xuint8 DeviceTypeQualifier : 3;
    xuint8 DeviceType : 5;
#endif
    xuint8 PageCode;
    xuint8 Reserved;
    xuint8 PageLength;
}    __x_attribute_packed__ VPD_MEDIA_SERIAL_NUMBER_PAGE, *PVPD_MEDIA_SERIAL_NUMBER_PAGE;

typedef struct _VPD_SERIAL_NUMBER_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)  
    xuint8 DeviceType : 5;
    xuint8 DeviceTypeQualifier : 3;
#else
    xuint8 DeviceTypeQualifier : 3;
    xuint8 DeviceType : 5;
#endif
    xuint8 PageCode;
    xuint8 Reserved;
    xuint8 PageLength;
}    __x_attribute_packed__ VPD_SERIAL_NUMBER_PAGE, *PVPD_SERIAL_NUMBER_PAGE;


//
// Device Identification Page (page code 0x83)
// Provides the means to retrieve zero or more identification descriptors
// applying to the logical unit.
//


typedef enum _VPD_CODE_SET {
    VpdCodeSetReserved = 0,
    VpdCodeSetBinary = 1,
    VpdCodeSetAscii = 2,
    VpdCodeSetUTF8 = 3
}    __x_attribute_packed__ VPD_CODE_SET, *PVPD_CODE_SET;

typedef enum _VPD_ASSOCIATION {
    VpdAssocDevice = 0,
    VpdAssocPort = 1,
    VpdAssocTarget = 2,
    VpdAssocReserved1 = 3,
    VpdAssocReserved2 = 4       // bogus, only two bits
}    __x_attribute_packed__ VPD_ASSOCIATION, *PVPD_ASSOCIATION;

typedef enum _VPD_IDENTIFIER_TYPE {
    VpdIdentifierTypeVendorSpecific = 0,
    VpdIdentifierTypeVendorId = 1,
    VpdIdentifierTypeEUI64 = 2,
    VpdIdentifierTypeFCPHName = 3,
    VpdIdentifierTypePortRelative = 4,
    VpdIdentifierTypeTargetPortGroup = 5,
    VpdIdentifierTypeLogicalUnitGroup = 6,
    VpdIdentifierTypeMD5LogicalUnitId = 7,
    VpdIdentifierTypeSCSINameString = 8
}    __x_attribute_packed__ VPD_IDENTIFIER_TYPE, *PVPD_IDENTIFIER_TYPE;

typedef struct _VPD_IDENTIFICATION_DESCRIPTOR 
    {
#if defined(__LITTLE_ENDIAN_BITFIELD)     
    xuint8 CodeSet : 4;          // VPD_CODE_SET
    xuint8 Reserved : 4;
    
    xuint8 IdentifierType : 4;   // VPD_IDENTIFIER_TYPE
    xuint8 Association : 2;
    xuint8 Reserved2 : 2;
#else
    xuint8 Reserved : 4;
    xuint8 CodeSet : 4;          // VPD_CODE_SET

    xuint8 Reserved2 : 2;
    xuint8 Association : 2;
    xuint8 IdentifierType : 4;   // VPD_IDENTIFIER_TYPE
#endif
    xuint8 Reserved3;
    xuint8 IdentifierLength;
}    __x_attribute_packed__ VPD_IDENTIFICATION_DESCRIPTOR, *PVPD_IDENTIFICATION_DESCRIPTOR;

typedef struct _VPD_IDENTIFICATION_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)  
    xuint8 DeviceType : 5;
    xuint8 DeviceTypeQualifier : 3;
#else
    xuint8 DeviceTypeQualifier : 3;
    xuint8 DeviceType : 5;
#endif
    xuint8 PageCode;
    xuint8 Reserved;
    xuint8 PageLength;


    //
    // The following field is actually a variable length array of identification
    // descriptors.  Unfortunately there's no C notation for an array of
    // variable length structures so we're forced to just pretend.
    //
}    __x_attribute_packed__ VPD_IDENTIFICATION_PAGE, *PVPD_IDENTIFICATION_PAGE;

//
// Supported Vital Product Data Pages Page (page code 0x00)
// Contains a list of the vital product data page cods supported by the target
// or logical unit.
//

typedef struct _VPD_SUPPORTED_PAGES_PAGE {
 #if defined(__LITTLE_ENDIAN_BITFIELD)  
    xuint8 DeviceType : 5;
    xuint8 DeviceTypeQualifier : 3;
#else
    xuint8 DeviceTypeQualifier : 3;
    xuint8 DeviceType : 5;
#endif
    xuint8 PageCode;
    xuint8 Reserved;
    xuint8 PageLength;
}    __x_attribute_packed__ VPD_SUPPORTED_PAGES_PAGE, *PVPD_SUPPORTED_PAGES_PAGE;



#define VPD_MAX_BUFFER_SIZE         0xff

#define VPD_SUPPORTED_PAGES         0x00
#define VPD_SERIAL_NUMBER           0x80
#define VPD_DEVICE_IDENTIFIERS      0x83
#define VPD_MEDIA_SERIAL_NUMBER     0x84
#define VPD_SOFTWARE_INTERFACE_IDENTIFIERS 0x84
#define VPD_NETWORK_MANAGEMENT_ADDRESSES 0x85
#define VPD_EXTENDED_INQUIRY_DATA   0x86
#define VPD_MODE_PAGE_POLICY        0x87
#define VPD_SCSI_PORTS              0x88


//
// Persistent Reservation Definitions.
//

//
// PERSISTENT_RESERVE_* definitions
//

#define RESERVATION_ACTION_READ_KEYS                    0x00
#define RESERVATION_ACTION_READ_RESERVATIONS            0x01

#define RESERVATION_ACTION_REGISTER                     0x00
#define RESERVATION_ACTION_RESERVE                      0x01
#define RESERVATION_ACTION_RELEASE                      0x02
#define RESERVATION_ACTION_CLEAR                        0x03
#define RESERVATION_ACTION_PREEMPT                      0x04
#define RESERVATION_ACTION_PREEMPT_ABORT                0x05
#define RESERVATION_ACTION_REGISTER_IGNORE_EXISTING     0x06

#define RESERVATION_SCOPE_LU                            0x00
#define RESERVATION_SCOPE_ELEMENT                       0x02

#define RESERVATION_TYPE_WRITE_EXCLUSIVE                0x01
#define RESERVATION_TYPE_EXCLUSIVE                      0x03
#define RESERVATION_TYPE_WRITE_EXCLUSIVE_REGISTRANTS    0x05
#define RESERVATION_TYPE_EXCLUSIVE_REGISTRANTS          0x06

//
// Structures for reserve in command.
//


typedef struct {
    xuint8 Generation[4];
    xuint8 AdditionalLength[4];
}    __x_attribute_packed__ PRI_REGISTRATION_LIST, *PPRI_REGISTRATION_LIST;

typedef struct {
    xuint8 ReservationKey[8];
    xuint8 ScopeSpecificAddress[4];
    xuint8 Reserved;
 #if defined(__LITTLE_ENDIAN_BITFIELD)      
    xuint8 Type : 4;
    xuint8 Scope : 4;
#else
   xuint8 Scope : 4;
   xuint8 Type : 4;
#endif
    xuint8 Obsolete[2];
}    __x_attribute_packed__ PRI_RESERVATION_DESCRIPTOR, *PPRI_RESERVATION_DESCRIPTOR;

typedef struct {
    xuint8 Generation[4];
    xuint8 AdditionalLength[4];
}   __x_attribute_packed__  PRI_RESERVATION_LIST, *PPRI_RESERVATION_LIST;


//
// Structures for reserve out command.
//


typedef struct {
    xuint8 ReservationKey[8];
    xuint8 ServiceActionReservationKey[8];
    xuint8 ScopeSpecificAddress[4];
#if defined(__LITTLE_ENDIAN_BITFIELD)      
    xuint8 ActivatePersistThroughPowerLoss : 1;
    xuint8 Reserved1 : 7;
#else
    xuint8 Reserved1 : 7;
    xuint8 ActivatePersistThroughPowerLoss : 1;
#endif
    xuint8 Reserved2;
    xuint8 Obsolete[2];
}    __x_attribute_packed__ PRO_PARAMETER_LIST, *PPRO_PARAMETER_LIST;



//
// Sense Data Format
//


typedef struct _SENSE_DATA {
#if defined(__LITTLE_ENDIAN_BITFIELD)      
    xuint8 ErrorCode:7;
    xuint8 Valid:1;
    
    xuint8 SegmentNumber;

    xuint8 SenseKey:4;
    xuint8 Reserved:1;
    xuint8 IncorrectLength:1;
    xuint8 EndOfMedia:1;
    xuint8 FileMark:1;
#else
    xuint8 Valid:1;
    xuint8 ErrorCode:7;


    xuint8 SegmentNumber;

    xuint8 FileMark:1;
    xuint8 EndOfMedia:1;
    xuint8 IncorrectLength:1;
    xuint8 Reserved:1;
    xuint8 SenseKey:4;
#endif
    xuint8 Information[4];
    xuint8 AdditionalSenseLength;
    xuint8 CommandSpecificInformation[4];
    xuint8 AdditionalSenseCode;
    xuint8 AdditionalSenseCodeQualifier;
    xuint8 FieldReplaceableUnitCode;
    xuint8 SenseKeySpecific[3];
}    __x_attribute_packed__ SENSE_DATA, *PSENSE_DATA;


//
// Default request sense buffer size
//

#define SENSE_BUFFER_SIZE 18

//
// Maximum request sense buffer size
//

#define MAX_SENSE_BUFFER_SIZE 255

//
// Maximum number of additional sense bytes.
//

#define MAX_ADDITIONAL_SENSE_BYTES (MAX_SENSE_BUFFER_SIZE - SENSE_BUFFER_SIZE)

//
// Sense codes
//

#define SCSI_SENSE_NO_SENSE         0x00
#define SCSI_SENSE_RECOVERED_ERROR  0x01
#define SCSI_SENSE_NOT_READY        0x02
#define SCSI_SENSE_MEDIUM_ERROR     0x03
#define SCSI_SENSE_HARDWARE_ERROR   0x04
#define SCSI_SENSE_ILLEGAL_REQUEST  0x05
#define SCSI_SENSE_UNIT_ATTENTION   0x06
#define SCSI_SENSE_DATA_PROTECT     0x07
#define SCSI_SENSE_BLANK_CHECK      0x08
#define SCSI_SENSE_UNIQUE           0x09
#define SCSI_SENSE_COPY_ABORTED     0x0A
#define SCSI_SENSE_ABORTED_COMMAND  0x0B
#define SCSI_SENSE_EQUAL            0x0C
#define SCSI_SENSE_VOL_OVERFLOW     0x0D
#define SCSI_SENSE_MISCOMPARE       0x0E
#define SCSI_SENSE_RESERVED         0x0F

//
// Additional tape bit
//

#define SCSI_ILLEGAL_LENGTH         0x20
#define SCSI_EOM                    0x40
#define SCSI_FILE_MARK              0x80

//
// Additional Sense codes
//

#define SCSI_ADSENSE_NO_SENSE                              0x00
#define SCSI_ADSENSE_NO_SEEK_COMPLETE                      0x02
#define SCSI_ADSENSE_LUN_NOT_READY                         0x04
#define SCSI_ADSENSE_LUN_COMMUNICATION                     0x08
#define SCSI_ADSENSE_WRITE_ERROR                           0x0C
#define SCSI_ADSENSE_TRACK_ERROR                           0x14
#define SCSI_ADSENSE_SEEK_ERROR                            0x15
#define SCSI_ADSENSE_REC_DATA_NOECC                        0x17
#define SCSI_ADSENSE_REC_DATA_ECC                          0x18
#define SCSI_ADSENSE_PARAMETER_LIST_LENGTH                 0x1A
#define SCSI_ADSENSE_ILLEGAL_COMMAND                       0x20
#define SCSI_ADSENSE_ILLEGAL_BLOCK                         0x21
#define SCSI_ADSENSE_INVALID_CDB                           0x24
#define SCSI_ADSENSE_INVALID_LUN                           0x25
#define SCSI_ADSENSE_INVALID_FIELD_PARAMETER_LIST          0x26
#define SCSI_ADSENSE_WRITE_PROTECT                         0x27
#define SCSI_ADSENSE_MEDIUM_CHANGED                        0x28
#define SCSI_ADSENSE_BUS_RESET                             0x29
#define SCSI_ADSENSE_PARAMETERS_CHANGED                    0x2A
#define SCSI_ADSENSE_INSUFFICIENT_TIME_FOR_OPERATION       0x2E
#define SCSI_ADSENSE_INVALID_MEDIA                         0x30
#define SCSI_ADSENSE_NO_MEDIA_IN_DEVICE                    0x3a
#define SCSI_ADSENSE_POSITION_ERROR                        0x3b
#define SCSI_ADSENSE_OPERATING_CONDITIONS_CHANGED          0x3f
#define SCSI_ADSENSE_OPERATOR_REQUEST                      0x5a // see below
#define SCSI_ADSENSE_FAILURE_PREDICTION_THRESHOLD_EXCEEDED 0x5d
#define SCSI_ADSENSE_ILLEGAL_MODE_FOR_THIS_TRACK           0x64
#define SCSI_ADSENSE_COPY_PROTECTION_FAILURE               0x6f
#define SCSI_ADSENSE_POWER_CALIBRATION_ERROR               0x73
#define SCSI_ADSENSE_VENDOR_UNIQUE                         0x80 // and higher
#define SCSI_ADSENSE_MUSIC_AREA                            0xA0
#define SCSI_ADSENSE_DATA_AREA                             0xA1
#define SCSI_ADSENSE_VOLUME_OVERFLOW                       0xA7

// for legacy apps:
#define SCSI_ADWRITE_PROTECT                        SCSI_ADSENSE_WRITE_PROTECT
#define SCSI_FAILURE_PREDICTION_THRESHOLD_EXCEEDED  SCSI_ADSENSE_FAILURE_PREDICTION_THRESHOLD_EXCEEDED


//
// SCSI_ADSENSE_LUN_NOT_READY (0x04) qualifiers
//

#define SCSI_SENSEQ_CAUSE_NOT_REPORTABLE         0x00
#define SCSI_SENSEQ_BECOMING_READY               0x01
#define SCSI_SENSEQ_INIT_COMMAND_REQUIRED        0x02
#define SCSI_SENSEQ_MANUAL_INTERVENTION_REQUIRED 0x03
#define SCSI_SENSEQ_FORMAT_IN_PROGRESS           0x04
#define SCSI_SENSEQ_REBUILD_IN_PROGRESS          0x05
#define SCSI_SENSEQ_RECALCULATION_IN_PROGRESS    0x06
#define SCSI_SENSEQ_OPERATION_IN_PROGRESS        0x07
#define SCSI_SENSEQ_LONG_WRITE_IN_PROGRESS       0x08

//
// SCSI_ADSENSE_LUN_COMMUNICATION (0x08) qualifiers
//

#define SCSI_SENSEQ_COMM_FAILURE                 0x00
#define SCSI_SENSEQ_COMM_TIMEOUT                 0x01
#define SCSI_SENSEQ_COMM_PARITY_ERROR            0x02
#define SCSI_SESNEQ_COMM_CRC_ERROR               0x03
#define SCSI_SENSEQ_UNREACHABLE_TARGET           0x04

//
// SCSI_ADSENSE_WRITE_ERROR (0x0C) qualifiers
//
#define SCSI_SENSEQ_LOSS_OF_STREAMING            0x09
#define SCSI_SENSEQ_PADDING_BLOCKS_ADDED         0x0A


//
// SCSI_ADSENSE_NO_SENSE (0x00) qualifiers
//

#define SCSI_SENSEQ_FILEMARK_DETECTED 0x01
#define SCSI_SENSEQ_END_OF_MEDIA_DETECTED 0x02
#define SCSI_SENSEQ_SETMARK_DETECTED 0x03
#define SCSI_SENSEQ_BEGINNING_OF_MEDIA_DETECTED 0x04

//
// SCSI_ADSENSE_ILLEGAL_BLOCK (0x21) qualifiers
//

#define SCSI_SENSEQ_ILLEGAL_ELEMENT_ADDR 0x01

//
// SCSI_ADSENSE_POSITION_ERROR (0x3b) qualifiers
//

#define SCSI_SENSEQ_DESTINATION_FULL 0x0d
#define SCSI_SENSEQ_SOURCE_EMPTY     0x0e

//
// SCSI_ADSENSE_INVALID_MEDIA (0x30) qualifiers
//

#define SCSI_SENSEQ_INCOMPATIBLE_MEDIA_INSTALLED 0x00
#define SCSI_SENSEQ_UNKNOWN_FORMAT 0x01
#define SCSI_SENSEQ_INCOMPATIBLE_FORMAT 0x02
#define SCSI_SENSEQ_CLEANING_CARTRIDGE_INSTALLED 0x03


//
// SCSI_ADSENSE_OPERATING_CONDITIONS_CHANGED (0x3f) qualifiers
//

#define SCSI_SENSEQ_TARGET_OPERATING_CONDITIONS_CHANGED 0x00
#define SCSI_SENSEQ_MICROCODE_CHANGED                   0x01
#define SCSI_SENSEQ_OPERATING_DEFINITION_CHANGED        0x02
#define SCSI_SENSEQ_INQUIRY_DATA_CHANGED                0x03
#define SCSI_SENSEQ_COMPONENT_DEVICE_ATTACHED           0x04
#define SCSI_SENSEQ_DEVICE_IDENTIFIER_CHANGED           0x05
#define SCSI_SENSEQ_REDUNDANCY_GROUP_MODIFIED           0x06
#define SCSI_SENSEQ_REDUNDANCY_GROUP_DELETED            0x07
#define SCSI_SENSEQ_SPARE_MODIFIED                      0x08
#define SCSI_SENSEQ_SPARE_DELETED                       0x09
#define SCSI_SENSEQ_VOLUME_SET_MODIFIED                 0x0A
#define SCSI_SENSEQ_VOLUME_SET_DELETED                  0x0B
#define SCSI_SENSEQ_VOLUME_SET_DEASSIGNED               0x0C
#define SCSI_SENSEQ_VOLUME_SET_REASSIGNED               0x0D
#define SCSI_SENSEQ_REPORTED_LUNS_DATA_CHANGED          0x0E
#define SCSI_SENSEQ_ECHO_BUFFER_OVERWRITTEN             0x0F
#define SCSI_SENSEQ_MEDIUM_LOADABLE                     0x10
#define SCSI_SENSEQ_MEDIUM_AUXILIARY_MEMORY_ACCESSIBLE  0x11


//
// SCSI_ADSENSE_OPERATOR_REQUEST (0x5a) qualifiers
//

#define SCSI_SENSEQ_STATE_CHANGE_INPUT     0x00 // generic request
#define SCSI_SENSEQ_MEDIUM_REMOVAL         0x01
#define SCSI_SENSEQ_WRITE_PROTECT_ENABLE   0x02
#define SCSI_SENSEQ_WRITE_PROTECT_DISABLE  0x03

//
// SCSI_ADSENSE_COPY_PROTECTION_FAILURE (0x6f) qualifiers
//
#define SCSI_SENSEQ_AUTHENTICATION_FAILURE                          0x00
#define SCSI_SENSEQ_KEY_NOT_PRESENT                                 0x01
#define SCSI_SENSEQ_KEY_NOT_ESTABLISHED                             0x02
#define SCSI_SENSEQ_READ_OF_SCRAMBLED_SECTOR_WITHOUT_AUTHENTICATION 0x03
#define SCSI_SENSEQ_MEDIA_CODE_MISMATCHED_TO_LOGICAL_UNIT           0x04
#define SCSI_SENSEQ_LOGICAL_UNIT_RESET_COUNT_ERROR                  0x05

//
// SCSI_ADSENSE_POWER_CALIBRATION_ERROR (0x73) qualifiers
//

#define SCSI_SENSEQ_POWER_CALIBRATION_AREA_ALMOST_FULL 0x01
#define SCSI_SENSEQ_POWER_CALIBRATION_AREA_FULL        0x02
#define SCSI_SENSEQ_POWER_CALIBRATION_AREA_ERROR       0x03
#define SCSI_SENSEQ_PMA_RMA_UPDATE_FAILURE             0x04
#define SCSI_SENSEQ_PMA_RMA_IS_FULL                    0x05
#define SCSI_SENSEQ_PMA_RMA_ALMOST_FULL                0x06




//
// Read Capacity Data - returned in Big Endian format
//


typedef struct _READ_CAPACITY_DATA {
    xuint32 LogicalBlockAddress;
    xuint32 BytesPerBlock;
}    __x_attribute_packed__ READ_CAPACITY_DATA, *PREAD_CAPACITY_DATA;




typedef struct _READ_CAPACITY_DATA_EX {
    xuint64 LogicalBlockAddress;
    xuint32 BytesPerBlock;
}    __x_attribute_packed__ READ_CAPACITY_DATA_EX, *PREAD_CAPACITY_DATA_EX;



//
// Read Block Limits Data - returned in Big Endian format
// This structure returns the maximum and minimum block
// size for a TAPE device.
//


typedef struct _READ_BLOCK_LIMITS {
    xuint8 Reserved;
    xuint8 BlockMaximumSize[3];
    xuint8 BlockMinimumSize[2];
}    __x_attribute_packed__ READ_BLOCK_LIMITS_DATA, *PREAD_BLOCK_LIMITS_DATA;



typedef struct _READ_BUFFER_CAPACITY_DATA {
    xuint8 DataLength[2];
    xuint8 Reserved1;
#if defined(__LITTLE_ENDIAN_BITFIELD)          
    xuint8 BlockDataReturned : 1;
    xuint8 Reserved4         : 7;
#else
    xuint8 Reserved4         : 7;
    xuint8 BlockDataReturned : 1;
#endif
    xuint8 TotalBufferSize[4];
    xuint8 AvailableBufferSize[4];
}    __x_attribute_packed__ READ_BUFFER_CAPACITY_DATA, *PREAD_BUFFER_CAPACITY_DATA;


//
// Mode data structures.
//

//
// Define Mode parameter header.
//


typedef struct _MODE_PARAMETER_HEADER {
    xuint8 ModeDataLength;
    xuint8 MediumType;
    xuint8 DeviceSpecificParameter;
    xuint8 BlockDescriptorLength;
}   __x_attribute_packed__ MODE_PARAMETER_HEADER, *PMODE_PARAMETER_HEADER;

typedef struct _MODE_PARAMETER_HEADER10 {
    xuint8 ModeDataLength[2];
    xuint8 MediumType;
    xuint8 DeviceSpecificParameter;
    xuint8 Reserved[2];
    xuint8 BlockDescriptorLength[2];
}   __x_attribute_packed__ MODE_PARAMETER_HEADER10, *PMODE_PARAMETER_HEADER10;

#define MODE_FD_SINGLE_SIDE     0x01
#define MODE_FD_DOUBLE_SIDE     0x02
#define MODE_FD_MAXIMUM_TYPE    0x1E
#define MODE_DSP_FUA_SUPPORTED  0x10
#define MODE_DSP_WRITE_PROTECT  0x80

//
// Define the mode parameter block.
//

typedef struct _MODE_PARAMETER_BLOCK {
    xuint8 DensityCode;
    xuint8 NumberOfBlocks[3];
    xuint8 Reserved;
    xuint8 BlockLength[3];
}   __x_attribute_packed__ MODE_PARAMETER_BLOCK, *PMODE_PARAMETER_BLOCK;


//
// Define Disconnect-Reconnect page.
//



typedef struct _MODE_DISCONNECT_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)          
    xuint8 PageCode : 6;
    xuint8 Reserved : 1;
    xuint8 PageSavable : 1;
#else
    xuint8 PageSavable : 1;
    xuint8 Reserved : 1;
    xuint8 PageCode : 6;
#endif
    xuint8 PageLength;
    xuint8 BufferFullRatio;
    xuint8 BufferEmptyRatio;
    xuint8 BusInactivityLimit[2];
    xuint8 BusDisconnectTime[2];
    xuint8 BusConnectTime[2];
    xuint8 MaximumBurstSize[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)        
    xuint8 DataTransferDisconnect : 2;
    xuint8 Reserved8:6;
#else
    xuint8 Reserved8:6;
    xuint8 DataTransferDisconnect : 2;
#endif
    xuint8 Reserved2[3];
}   __x_attribute_packed__ MODE_DISCONNECT_PAGE, *PMODE_DISCONNECT_PAGE;


//
// Define mode caching page.
//


typedef struct _MODE_CACHING_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 PageCode : 6;
    xuint8 Reserved : 1;
    xuint8 PageSavable : 1;
    
    xuint8 PageLength;
    
    xuint8 ReadDisableCache : 1;
    xuint8 MultiplicationFactor : 1;
    xuint8 WriteCacheEnable : 1;
    xuint8 Reserved2 : 5;
    
    xuint8 WriteRetensionPriority : 4;
    xuint8 ReadRetensionPriority : 4;
#else
    xuint8 PageSavable : 1;
    xuint8 Reserved : 1;
    xuint8 PageCode : 6;

    xuint8 PageLength;

    xuint8 Reserved2 : 5;
    xuint8 WriteCacheEnable : 1;
    xuint8 MultiplicationFactor : 1;   
    xuint8 ReadDisableCache : 1;

    xuint8 ReadRetensionPriority : 4;   
    xuint8 WriteRetensionPriority : 4; 
#endif
    xuint8 DisablePrefetchTransfer[2];
    xuint8 MinimumPrefetch[2];
    xuint8 MaximumPrefetch[2];
    xuint8 MaximumPrefetchCeiling[2];
}   __x_attribute_packed__ MODE_CACHING_PAGE, *PMODE_CACHING_PAGE;


//
// Define write parameters cdrom page
//

typedef struct _MODE_CDROM_WRITE_PARAMETERS_PAGE2 {
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 PageCode : 6;             // 0x05
    xuint8 Reserved : 1;
    xuint8 PageSavable : 1;
    
    xuint8 PageLength;               // 0x32 ??

    xuint8 WriteType                 : 4;
    xuint8 TestWrite                 : 1;
    xuint8 LinkSizeValid             : 1;
    xuint8 BufferUnderrunFreeEnabled : 1;
    xuint8 Reserved2                 : 1;
    
    xuint8 TrackMode                 : 4;
    xuint8 Copy                      : 1;
    xuint8 FixedPacket               : 1;
    xuint8 MultiSession              : 2;
    
    xuint8 DataBlockType             : 4;
    xuint8 Reserved3                 : 4;
    
    xuint8 LinkSize;
    xuint8 Reserved4;

    xuint8 HostApplicationCode       : 6;
    xuint8 Reserved5                 : 2;
#else
    xuint8 PageSavable : 1;
    xuint8 Reserved : 1;
    xuint8 PageCode : 6;             // 0x05

    xuint8 PageLength;               // 0x32 ??

    xuint8 Reserved2                 : 1;
    xuint8 BufferUnderrunFreeEnabled : 1;
    xuint8 LinkSizeValid             : 1;
    xuint8 TestWrite                 : 1;
    xuint8 WriteType                 : 4;
    
    xuint8 MultiSession              : 2;   
    xuint8 FixedPacket               : 1;
    xuint8 Copy                      : 1;
    xuint8 TrackMode                 : 4;

    xuint8 Reserved3                 : 4;
    xuint8 DataBlockType             : 4;
    
    xuint8 LinkSize;
    xuint8 Reserved4;

    xuint8 Reserved5                 : 2;
    xuint8 HostApplicationCode       : 6;
#endif
    xuint8 SessionFormat;
    xuint8 Reserved6;
    xuint8 PacketSize[4];
    xuint8 AudioPauseLength[2];
    xuint8 MediaCatalogNumber[16];
    xuint8 ISRC[16];
    xuint8 SubHeaderData[4];
}   __x_attribute_packed__  MODE_CDROM_WRITE_PARAMETERS_PAGE2, *PMODE_CDROM_WRITE_PARAMETERS_PAGE2;




//
// Define the MRW mode page for CDROM device types
//

typedef struct _MODE_MRW_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)       
    xuint8 PageCode : 6; // 0x03
    xuint8 Reserved : 1;
    xuint8 PageSavable : 1;
    
    xuint8 PageLength;   //0x06
    xuint8 Reserved1;
    
    xuint8 LbaSpace  : 1;
    xuint8 Reserved2 : 7;
#else
    xuint8 PageSavable : 1;
    xuint8 Reserved : 1;
    xuint8 PageCode : 6; // 0x03

    xuint8 PageLength;   //0x06
    xuint8 Reserved1;

    xuint8 Reserved2 : 7;
    xuint8 LbaSpace  : 1;
#endif
    xuint8 Reserved3[4];
}    __x_attribute_packed__ MODE_MRW_PAGE, *PMODE_MRW_PAGE;


//
// Define mode flexible disk page.
//


typedef struct _MODE_FLEXIBLE_DISK_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)       
    xuint8 PageCode : 6;
    xuint8 Reserved : 1;
    xuint8 PageSavable : 1;
#else
    xuint8 PageSavable : 1;
    xuint8 Reserved : 1;
    xuint8 PageCode : 6; 
#endif
    xuint8 PageLength;
    xuint8 TransferRate[2];
    xuint8 NumberOfHeads;
    xuint8 SectorsPerTrack;
    xuint8 BytesPerSector[2];
    xuint8 NumberOfCylinders[2];
    xuint8 StartWritePrecom[2];
    xuint8 StartReducedCurrent[2];
    xuint8 StepRate[2];
    xuint8 StepPluseWidth;
    xuint8 HeadSettleDelay[2];
    xuint8 MotorOnDelay;
    xuint8 MotorOffDelay;
#if defined(__LITTLE_ENDIAN_BITFIELD)           
    xuint8 Reserved2 : 5;
    xuint8 MotorOnAsserted : 1;
    xuint8 StartSectorNumber : 1;
    xuint8 TrueReadySignal : 1;
    
    xuint8 StepPlusePerCyclynder : 4;
    xuint8 Reserved3 : 4;
    
    xuint8 WriteCompenstation;
    xuint8 HeadLoadDelay;
    xuint8 HeadUnloadDelay;
    
    xuint8 Pin2Usage : 4;
    xuint8 Pin34Usage : 4;
    
    xuint8 Pin1Usage : 4;
    xuint8 Pin4Usage : 4;
#else
    xuint8 TrueReadySignal : 1;
    xuint8 StartSectorNumber : 1;
    xuint8 MotorOnAsserted : 1;
    xuint8 Reserved2 : 5;

    xuint8 Reserved3 : 4;
    xuint8 StepPlusePerCyclynder : 4;
    
    xuint8 WriteCompenstation;
    xuint8 HeadLoadDelay;
    xuint8 HeadUnloadDelay;

    xuint8 Pin34Usage : 4;
    xuint8 Pin2Usage : 4;

    xuint8 Pin4Usage : 4;
    xuint8 Pin1Usage : 4;
#endif
    xuint8 MediumRotationRate[2];
    xuint8 Reserved4[2];
}    __x_attribute_packed__ MODE_FLEXIBLE_DISK_PAGE, *PMODE_FLEXIBLE_DISK_PAGE;


//
// Define mode format page.
//

typedef struct _MODE_FORMAT_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)           
    xuint8 PageCode : 6;
    xuint8 Reserved : 1;
    xuint8 PageSavable : 1;
#else
    xuint8 PageSavable : 1;
    xuint8 Reserved : 1;
    xuint8 PageCode : 6; 
#endif
    xuint8 PageLength;
    xuint8 TracksPerZone[2];
    xuint8 AlternateSectorsPerZone[2];
    xuint8 AlternateTracksPerZone[2];
    xuint8 AlternateTracksPerLogicalUnit[2];
    xuint8 SectorsPerTrack[2];
    xuint8 BytesPerPhysicalSector[2];
    xuint8 Interleave[2];
    xuint8 TrackSkewFactor[2];
    xuint8 CylinderSkewFactor[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)               
    xuint8 Reserved2 : 4;
    xuint8 SurfaceFirst : 1;
    xuint8 RemovableMedia : 1;
    xuint8 HardSectorFormating : 1;
    xuint8 SoftSectorFormating : 1;
#else
    xuint8 SoftSectorFormating : 1;
    xuint8 HardSectorFormating : 1;
    xuint8 RemovableMedia : 1;
    xuint8 SurfaceFirst : 1;  
    xuint8 Reserved2 : 4; 
#endif
    xuint8 Reserved3[3];
}    __x_attribute_packed__ MODE_FORMAT_PAGE, *PMODE_FORMAT_PAGE;


//
// Define rigid disk driver geometry page.
//


typedef struct _MODE_RIGID_GEOMETRY_PAGE {
 #if defined(__LITTLE_ENDIAN_BITFIELD)           
    xuint8 PageCode : 6;
    xuint8 Reserved : 1;
    xuint8 PageSavable : 1;
#else
    xuint8 PageSavable : 1;
    xuint8 Reserved : 1;
    xuint8 PageCode : 6; 
#endif
    xuint8 PageLength;
    xuint8 NumberOfCylinders[3];
    xuint8 NumberOfHeads;
    xuint8 StartWritePrecom[3];
    xuint8 StartReducedCurrent[3];
    xuint8 DriveStepRate[2];
    xuint8 LandZoneCyclinder[3];
 #if defined(__LITTLE_ENDIAN_BITFIELD)               
    xuint8 RotationalPositionLock : 2;
    xuint8 Reserved2 : 6;
 #else
    xuint8 Reserved2 : 6;
    xuint8 RotationalPositionLock : 2;
 #endif
    xuint8 RotationOffset;
    xuint8 Reserved3;
    xuint8 RoataionRate[2];
    xuint8 Reserved4[2];
}   __x_attribute_packed__ MODE_RIGID_GEOMETRY_PAGE, *PMODE_RIGID_GEOMETRY_PAGE;


//
// Define read write recovery page
//


typedef struct _MODE_READ_WRITE_RECOVERY_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)           
    xuint8 PageCode : 6;
    xuint8 Reserved1 : 1;
    xuint8 PSBit : 1;
    
    xuint8 PageLength;
    
    xuint8 DCRBit : 1;
    xuint8 DTEBit : 1;
    xuint8 PERBit : 1;
    xuint8 EERBit : 1;
    xuint8 RCBit : 1;
    xuint8 TBBit : 1;
    xuint8 ARRE : 1;
    xuint8 AWRE : 1;
#else
    xuint8 PSBit : 1;
    xuint8 Reserved1 : 1;
    xuint8 PageCode : 6;

    xuint8 PageLength;

    xuint8 AWRE : 1;
    xuint8 ARRE : 1;
    xuint8 TBBit : 1;
    xuint8 RCBit : 1;
    xuint8 EERBit : 1;
    xuint8 PERBit : 1;
    xuint8 DTEBit : 1;
    xuint8 DCRBit : 1;
#endif
    xuint8 ReadRetryCount;
    xuint8 Reserved4[4];
    xuint8 WriteRetryCount;
    xuint8 Reserved5[3];

}    __x_attribute_packed__ MODE_READ_WRITE_RECOVERY_PAGE, *PMODE_READ_WRITE_RECOVERY_PAGE;


//
// Define read recovery page - cdrom
//


typedef struct _MODE_READ_RECOVERY_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 PageCode : 6;
    xuint8 Reserved1 : 1;
    xuint8 PSBit : 1;
    
    xuint8 PageLength;

    xuint8 DCRBit : 1;
    xuint8 DTEBit : 1;
    xuint8 PERBit : 1;
    xuint8 Reserved2 : 1;
    xuint8 RCBit : 1;
    xuint8 TBBit : 1;
    xuint8 Reserved3 : 2;
 #else
    xuint8 PSBit : 1;
    xuint8 Reserved1 : 1;
    xuint8 PageCode : 6;

    xuint8 PageLength;

    xuint8 Reserved3 : 2;
    xuint8 TBBit : 1;
    xuint8 RCBit : 1;
    xuint8 Reserved2 : 1;
    xuint8 PERBit : 1;
    xuint8 DTEBit : 1;
    xuint8 DCRBit : 1; 
 #endif
    xuint8 ReadRetryCount;
    xuint8 Reserved4[4];

}   __x_attribute_packed__  MODE_READ_RECOVERY_PAGE, *PMODE_READ_RECOVERY_PAGE;



//
// Define Informational Exception Control Page. Used for failure prediction
//


typedef struct _MODE_INFO_EXCEPTIONS
{
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 PageCode : 6;
    xuint8 Reserved1 : 1;
    xuint8 PSBit : 1;
#else
    xuint8 PSBit : 1;
    xuint8 Reserved1 : 1;
    xuint8 PageCode : 6;
#endif
    xuint8 PageLength;

    union
    {
        xuint8 Flags;
        struct
        {
#if defined(__LITTLE_ENDIAN_BITFIELD)            
            xuint8 LogErr : 1;
            xuint8 Reserved2 : 1;
            xuint8 Test : 1;
            xuint8 Dexcpt : 1;
            xuint8 Reserved3 : 3;
            xuint8 Perf : 1;
#else
            xuint8 Perf : 1;
            xuint8 Reserved3 : 3;
            xuint8 Dexcpt : 1;
            xuint8 Test : 1;
            xuint8 Reserved2 : 1;
            xuint8 LogErr : 1;
#endif
        }ct;
    }u1;
#if defined(__LITTLE_ENDIAN_BITFIELD)            
    xuint8 ReportMethod : 4;
    xuint8 Reserved4 : 4;
#else
    xuint8 Reserved4 : 4;
    xuint8 ReportMethod : 4;
#endif
    xuint8 IntervalTimer[4];
    xuint8 ReportCount[4];

}   __x_attribute_packed__  MODE_INFO_EXCEPTIONS, *PMODE_INFO_EXCEPTIONS;


//
// Begin C/DVD 0.9 definitions
//

//
// Power Condition Mode Page Format
//


typedef struct _POWER_CONDITION_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 PageCode : 6;
    xuint8 Reserved1 : 1;
    xuint8 PSBit : 1;
#else
    xuint8 PSBit : 1;
    xuint8 Reserved1 : 1;
    xuint8 PageCode : 6;
#endif
    xuint8 PageLength;           // 0x0A
    xuint8 Reserved2;
#if defined(__LITTLE_ENDIAN_BITFIELD)            
    xuint8 Standby : 1;
    xuint8 Idle : 1;
    xuint8 Reserved3 : 6;
#else
    xuint8 Reserved3 : 6;
    xuint8 Idle : 1;
    xuint8 Standby : 1;
#endif
    xuint8 IdleTimer[4];
    xuint8 StandbyTimer[4];
}    __x_attribute_packed__ POWER_CONDITION_PAGE, *PPOWER_CONDITION_PAGE;


//
// CD-Audio Control Mode Page Format
//


typedef struct _CDDA_OUTPUT_PORT {
#if defined(__LITTLE_ENDIAN_BITFIELD)    
    xuint8 ChannelSelection : 4;
    xuint8 Reserved : 4;
#else
    xuint8 Reserved : 4;
    xuint8 ChannelSelection : 4;
#endif
    xuint8 Volume;
} CDDA_OUTPUT_PORT, *PCDDA_OUTPUT_PORT;

typedef struct _CDAUDIO_CONTROL_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 PageCode : 6;
    xuint8 Reserved1 : 1;
    xuint8 PSBit : 1;
#else
    xuint8 PSBit : 1;
    xuint8 Reserved1 : 1;
    xuint8 PageCode : 6;
#endif

    xuint8 PageLength;       // 0x0E
#if defined(__LITTLE_ENDIAN_BITFIELD)  
    xuint8 Reserved2 : 1;
    xuint8 StopOnTrackCrossing : 1;         // Default 0
    xuint8 Immediate : 1;    // Always 1
    xuint8 Reserved3 : 5;
#else
    xuint8 Reserved3 : 5;
    xuint8 Immediate : 1;    // Always 1
    xuint8 StopOnTrackCrossing : 1;         // Default 0    
    xuint8 Reserved2 : 1;
#endif
    xuint8 Reserved4[3];
    xuint8 Obsolete[2];

    CDDA_OUTPUT_PORT CDDAOutputPorts[4];

}    __x_attribute_packed__ CDAUDIO_CONTROL_PAGE, *PCDAUDIO_CONTROL_PAGE;


#define CDDA_CHANNEL_MUTED      0x0
#define CDDA_CHANNEL_ZERO       0x1
#define CDDA_CHANNEL_ONE        0x2
#define CDDA_CHANNEL_TWO        0x4
#define CDDA_CHANNEL_THREE      0x8

//
// C/DVD Feature Set Support & Version Page
//


typedef struct _CDVD_FEATURE_SET_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 PageCode : 6;
    xuint8 Reserved1 : 1;
    xuint8 PSBit : 1;
#else
    xuint8 PSBit : 1;
    xuint8 Reserved1 : 1;
    xuint8 PageCode : 6;
#endif

    xuint8 PageLength;       // 0x16

    xuint8 CDAudio[2];
    xuint8 EmbeddedChanger[2];
    xuint8 PacketSMART[2];
    xuint8 PersistantPrevent[2];
    xuint8 EventStatusNotification[2];
    xuint8 DigitalOutput[2];
    xuint8 CDSequentialRecordable[2];
    xuint8 DVDSequentialRecordable[2];
    xuint8 RandomRecordable[2];
    xuint8 KeyExchange[2];
    xuint8 Reserved2[2];
}    __x_attribute_packed__ CDVD_FEATURE_SET_PAGE, *PCDVD_FEATURE_SET_PAGE;


//
// CDVD Inactivity Time-out Page Format
//


typedef struct _CDVD_INACTIVITY_TIMEOUT_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 PageCode : 6;
    xuint8 Reserved1 : 1;
    xuint8 PSBit : 1;
#else
    xuint8 PSBit : 1;
    xuint8 Reserved1 : 1;
    xuint8 PageCode : 6;
#endif

    xuint8 PageLength;       // 0x08
    xuint8 Reserved2[2];
#if defined(__LITTLE_ENDIAN_BITFIELD)     
    xuint8 SWPP : 1;
    xuint8 DISP : 1;
    xuint8 Reserved3 : 6;
#else
    xuint8 Reserved3 : 6;
    xuint8 DISP : 1;
    xuint8 SWPP : 1;
#endif
    xuint8 Reserved4;
    xuint8 GroupOneMinimumTimeout[2];
    xuint8 GroupTwoMinimumTimeout[2];
}    __x_attribute_packed__ CDVD_INACTIVITY_TIMEOUT_PAGE, *PCDVD_INACTIVITY_TIMEOUT_PAGE;


//
// CDVD Capabilities & Mechanism Status Page
//

#define CDVD_LMT_CADDY              0
#define CDVD_LMT_TRAY               1
#define CDVD_LMT_POPUP              2
#define CDVD_LMT_RESERVED1          3
#define CDVD_LMT_CHANGER_INDIVIDUAL 4
#define CDVD_LMT_CHANGER_CARTRIDGE  5
#define CDVD_LMT_RESERVED2          6
#define CDVD_LMT_RESERVED3          7



typedef struct _CDVD_CAPABILITIES_PAGE {
#if defined(__LITTLE_ENDIAN_BITFIELD)         
    xuint8 PageCode : 6;
    xuint8 Reserved1 : 1;
    xuint8 PSBit : 1;
#else
    xuint8 PSBit : 1;
    xuint8 Reserved1 : 1;
    xuint8 PageCode : 6;
#endif

    xuint8 PageLength;       // >= 0x18      // offset 1
    
#if defined(__LITTLE_ENDIAN_BITFIELD)   
    xuint8 CDRRead : 1;
    xuint8 CDERead : 1;
    xuint8 Method2 : 1;
    xuint8 DVDROMRead : 1;
    xuint8 DVDRRead : 1;
    xuint8 DVDRAMRead : 1;
    xuint8 Reserved2 : 2;                    // offset 2

    xuint8 CDRWrite : 1;
    xuint8 CDEWrite : 1;
    xuint8 TestWrite : 1;
    xuint8 Reserved3 : 1;
    xuint8 DVDRWrite : 1;
    xuint8 DVDRAMWrite : 1;
    xuint8 Reserved4 : 2;                    // offset 3

    xuint8 AudioPlay : 1;
    xuint8 Composite : 1;
    xuint8 DigitalPortOne : 1;
    xuint8 DigitalPortTwo : 1;
    xuint8 Mode2Form1 : 1;
    xuint8 Mode2Form2 : 1;
    xuint8 MultiSession : 1;
    xuint8 BufferUnderrunFree : 1;                    // offset 4

    xuint8 CDDA : 1;
    xuint8 CDDAAccurate : 1;
    xuint8 RWSupported : 1;
    xuint8 RWDeinterleaved : 1;
    xuint8 C2Pointers : 1;
    xuint8 ISRC : 1;
    xuint8 UPC : 1;
    xuint8 ReadBarCodeCapable : 1;           // offset 5

    xuint8 Lock : 1;
    xuint8 LockState : 1;
    xuint8 PreventJumper : 1;
    xuint8 Eject : 1;
    xuint8 Reserved6 : 1;
    xuint8 LoadingMechanismType : 3;         // offset 6

    xuint8 SeparateVolume : 1;
    xuint8 SeperateChannelMute : 1;
    xuint8 SupportsDiskPresent : 1;
    xuint8 SWSlotSelection : 1;
    xuint8 SideChangeCapable : 1;
    xuint8 RWInLeadInReadable : 1;
    xuint8 Reserved7 : 2;                    // offset 7
#else
    xuint8 Reserved2 : 2;                    // offset 2
    xuint8 DVDRAMRead : 1;
    xuint8 DVDRRead : 1;
    xuint8 DVDROMRead : 1;
    xuint8 Method2 : 1;
    xuint8 CDERead : 1; 
    xuint8 CDRRead : 1;

    xuint8 Reserved4 : 2;                    // offset 3
    xuint8 DVDRAMWrite : 1;
    xuint8 DVDRWrite : 1;
    xuint8 Reserved3 : 1;
    xuint8 TestWrite : 1;
    xuint8 CDEWrite : 1;
    xuint8 CDRWrite : 1;

    xuint8 BufferUnderrunFree : 1;                    // offset 4
    xuint8 MultiSession : 1;
    xuint8 Mode2Form2 : 1;
    xuint8 Mode2Form1 : 1;
    xuint8 DigitalPortTwo : 1;
    xuint8 DigitalPortOne : 1;
    xuint8 Composite : 1;
    xuint8 AudioPlay : 1;

    xuint8 ReadBarCodeCapable : 1;           // offset 5
    xuint8 UPC : 1;
    xuint8 ISRC : 1;
    xuint8 C2Pointers : 1;
    xuint8 RWDeinterleaved : 1;
    xuint8 RWSupported : 1;
    xuint8 CDDAAccurate : 1;  
    xuint8 CDDA : 1;
    
    xuint8 LoadingMechanismType : 3;         // offset 6
    xuint8 Reserved6 : 1;
    xuint8 Eject : 1;
    xuint8 PreventJumper : 1;
    xuint8 LockState : 1;
    xuint8 Lock : 1;

    xuint8 Reserved7 : 2;                    // offset 7
    xuint8 RWInLeadInReadable : 1;
    xuint8 SideChangeCapable : 1;
    xuint8 SWSlotSelection : 1;
    xuint8 SupportsDiskPresent : 1;
    xuint8 SeperateChannelMute : 1;
    xuint8 SeparateVolume : 1;
#endif
    union {
        xuint8 ReadSpeedMaximum[2];
        xuint8 ObsoleteReserved[2];          // offset 8
    }u1;

    xuint8 NumberVolumeLevels[2];            // offset 10
    xuint8 BufferSize[2];                    // offset 12

    union {
        xuint8 ReadSpeedCurrent[2];
        xuint8 ObsoleteReserved2[2];         // offset 14
    }u2;
    xuint8 ObsoleteReserved3;                // offset 16
#if defined(__LITTLE_ENDIAN_BITFIELD)   
    xuint8 Reserved8 : 1;
    xuint8 BCK : 1;
    xuint8 RCK : 1;
    xuint8 LSBF : 1;
    xuint8 Length : 2;
    xuint8 Reserved9 : 2;                    // offset 17
#else
    xuint8 Reserved9 : 2;                    // offset 17
    xuint8 Length : 2;   
    xuint8 LSBF : 1;
    xuint8 RCK : 1;
    xuint8 BCK : 1;
    xuint8 Reserved8 : 1;
#endif
    union {
        xuint8 WriteSpeedMaximum[2];
        xuint8 ObsoleteReserved4[2];         // offset 18
    }u3;
    union {
        xuint8 WriteSpeedCurrent[2];
        xuint8 ObsoleteReserved11[2];        // offset 20
    }u4;

    //
    // NOTE: This mode page is two bytes too small in the release
    //       version of the Windows2000 DDK.  it also incorrectly
    //       put the CopyManagementRevision at offset 20 instead
    //       of offset 22, so fix that with a nameless union (for
    //       backwards-compatibility with those who "fixed" it on
    //       their own by looking at Reserved10[]).
    //

    union {
        xuint8 CopyManagementRevision[2];    // offset 22
        xuint8 Reserved10[2];
    }u5;
    //xuint8 Reserved12[2];                    // offset 24

}    __x_attribute_packed__ CDVD_CAPABILITIES_PAGE, *PCDVD_CAPABILITIES_PAGE;


typedef struct _LUN_LIST {
    xuint8 LunListLength[4]; // sizeof LunSize * 8
    xuint8 Reserved[4];
}    __x_attribute_packed__ LUN_LIST, *PLUN_LIST;



#define LOADING_MECHANISM_CADDY                 0x00
#define LOADING_MECHANISM_TRAY                  0x01
#define LOADING_MECHANISM_POPUP                 0x02
#define LOADING_MECHANISM_INDIVIDUAL_CHANGER    0x04
#define LOADING_MECHANISM_CARTRIDGE_CHANGER     0x05

//
// end C/DVD 0.9 mode page definitions

//
// Mode parameter list block descriptor -
// set the block length for reading/writing
//
//

#define MODE_BLOCK_DESC_LENGTH               8
#define MODE_HEADER_LENGTH                   4
#define MODE_HEADER_LENGTH10                 8


typedef struct _MODE_PARM_READ_WRITE {

   MODE_PARAMETER_HEADER  ParameterListHeader;  // List Header Format
   MODE_PARAMETER_BLOCK   ParameterListBlock;   // List Block Descriptor

}    __x_attribute_packed__ MODE_PARM_READ_WRITE_DATA, *PMODE_PARM_READ_WRITE_DATA;


// end_ntminitape

//
// CDROM audio control (0x0E)
//

#define CDB_AUDIO_PAUSE 0
#define CDB_AUDIO_RESUME 1

#define CDB_DEVICE_START 0x11
#define CDB_DEVICE_STOP 0x10

#define CDB_EJECT_MEDIA 0x10
#define CDB_LOAD_MEDIA 0x01

#define CDB_SUBCHANNEL_HEADER      0x00
#define CDB_SUBCHANNEL_BLOCK       0x01

#define CDROM_AUDIO_CONTROL_PAGE   0x0E
#define MODE_SELECT_IMMEDIATE      0x04
#define MODE_SELECT_PFBIT          0x10

#define CDB_USE_MSF                0x01


typedef struct _PORT_OUTPUT {
    xuint8 ChannelSelection;
    xuint8 Volume;
}    __x_attribute_packed__ PORT_OUTPUT, *PPORT_OUTPUT;

typedef struct _AUDIO_OUTPUT {
    xuint8 CodePage;
    xuint8 ParameterLength;
    xuint8 Immediate;
    xuint8 Reserved[2];
    xuint8 LbaFormat;
    xuint8 LogicalBlocksPerSecond[2];
    PORT_OUTPUT PortOutput[4];
}    __x_attribute_packed__ AUDIO_OUTPUT, *PAUDIO_OUTPUT;


//
// Multisession CDROM
//

#define GET_LAST_SESSION 0x01
#define GET_SESSION_DATA 0x02;

//
// Atapi 2.5 changer
//


typedef struct _MECHANICAL_STATUS_INFORMATION_HEADER {
#if defined(__LITTLE_ENDIAN_BITFIELD)   
    xuint8 CurrentSlot : 5;
    xuint8 ChangerState : 2;
    xuint8 Fault : 1;
    
    xuint8 Reserved : 5;
    xuint8 MechanismState : 3;
#else
    xuint8 Fault : 1;
    xuint8 ChangerState : 2;
    xuint8 CurrentSlot : 5;

    xuint8 MechanismState : 3;
    xuint8 Reserved : 5;
#endif
    xuint8 CurrentLogicalBlockAddress[3];
    xuint8 NumberAvailableSlots;
    xuint8 SlotTableLength[2];
}    __x_attribute_packed__ MECHANICAL_STATUS_INFORMATION_HEADER, *PMECHANICAL_STATUS_INFORMATION_HEADER;

typedef struct _SLOT_TABLE_INFORMATION {
    xuint8 DiscChanged : 1;
    xuint8 Reserved : 6;
    xuint8 DiscPresent : 1;
    xuint8 Reserved2[3];
}    __x_attribute_packed__ SLOT_TABLE_INFORMATION, *PSLOT_TABLE_INFORMATION;

typedef struct _MECHANICAL_STATUS {
    MECHANICAL_STATUS_INFORMATION_HEADER MechanicalStatusHeader;
    SLOT_TABLE_INFORMATION SlotTableInfo[1];
}    __x_attribute_packed__ MECHANICAL_STATUS, *PMECHANICAL_STATUS;



// begin_ntminitape

//
// Tape definitions
//


typedef struct _TAPE_POSITION_DATA {
#if defined(__LITTLE_ENDIAN_BITFIELD)   
    xuint8 Reserved1:2;
    xuint8 BlockPositionUnsupported:1;
    xuint8 Reserved2:3;
    xuint8 EndOfPartition:1;
    xuint8 BeginningOfPartition:1;
#else
    xuint8 BeginningOfPartition:1;
    xuint8 EndOfPartition:1;
    xuint8 Reserved2:3;
    xuint8 BlockPositionUnsupported:1;
    xuint8 Reserved1:2;
#endif
    xuint8 PartitionNumber;
    xuint16 Reserved3;
    xuint8 FirstBlock[4];
    xuint8 LastBlock[4];
    xuint8 Reserved4;
    xuint8 NumberOfBlocks[3];
    xuint8 NumberOfBytes[4];
}    __x_attribute_packed__ TAPE_POSITION_DATA, *PTAPE_POSITION_DATA;




typedef union _EIGHT_BYTE {

    struct {
        xuint8 Byte0;
        xuint8 Byte1;
        xuint8 Byte2;
        xuint8 Byte3;
        xuint8 Byte4;
        xuint8 Byte5;
        xuint8 Byte6;
        xuint8 Byte7;
    }bytes;

    xuint64 AsULongLong;
} __x_attribute_packed__ EIGHT_BYTE, *PEIGHT_BYTE;

typedef union _FOUR_BYTE {

    struct {
        xuint8 Byte0;
        xuint8 Byte1;
        xuint8 Byte2;
        xuint8 Byte3;
    }bytes;

    xuint32 AsULong;
} __x_attribute_packed__ FOUR_BYTE, *PFOUR_BYTE;

typedef union _TWO_BYTE {

    struct {
        xuint8 Byte0;
        xuint8 Byte1;
    }bytes;

    xuint16 AsUShort;
} __x_attribute_packed__ TWO_BYTE, *PTWO_BYTE;

#ifdef __LITTLE_ENDIAN_BYTEORDER
//
// Byte reversing macro for converting
// between big- and little-endian formats
//

#define REVERSE_BYTES_64(Destination, Source) {           \
    PEIGHT_BYTE d = (PEIGHT_BYTE)(Destination);             \
    PEIGHT_BYTE s = (PEIGHT_BYTE)(Source);                  \
    d->bytes.Byte7 = s->bytes.Byte0;                                    \
    d->bytes.Byte6 = s->bytes.Byte1;                                    \
    d->bytes.Byte5 = s->bytes.Byte2;                                    \
    d->bytes.Byte4 = s->bytes.Byte3;                                    \
    d->bytes.Byte3 = s->bytes.Byte4;                                    \
    d->bytes.Byte2 = s->bytes.Byte5;                                    \
    d->bytes.Byte1 = s->bytes.Byte6;                                    \
    d->bytes.Byte0 = s->bytes.Byte7;                                    \
}

#define REVERSE_BYTES_32(Destination, Source) {                \
    PFOUR_BYTE d = (PFOUR_BYTE)(Destination);               \
    PFOUR_BYTE s = (PFOUR_BYTE)(Source);                    \
    d->bytes.Byte3 = s->bytes.Byte0;                                    \
    d->bytes.Byte2 = s->bytes.Byte1;                                    \
    d->bytes.Byte1 = s->bytes.Byte2;                                    \
    d->bytes.Byte0 = s->bytes.Byte3;                                    \
}

#define REVERSE_BYTES_16(Destination, Source) {          \
    PTWO_BYTE d = (PTWO_BYTE)(Destination);                 \
    PTWO_BYTE s = (PTWO_BYTE)(Source);                      \
    d->bytes.Byte1 = s->bytes.Byte0;                                    \
    d->bytes.Byte0 = s->bytes.Byte1;                                    \
}

//
// Byte reversing macro for converting
// USHORTS from big to little endian in place
//

#define REVERSE_16(Short) {          \
    xuint8 tmp;                          \
    PTWO_BYTE w = (PTWO_BYTE)(Short);   \
    tmp = w->bytes.Byte0;                     \
    w->bytes.Byte0 = w->bytes.Byte1;                \
    w->bytes.Byte1 = tmp;                     \
    }

//
// Byte reversing macro for convering
// ULONGS between big & little endian in place
//

#define REVERSE_32(Long) {            \
    xuint8 tmp;                          \
    PFOUR_BYTE l = (PFOUR_BYTE)(Long);  \
    tmp = l->bytes.Byte3;                     \
    l->bytes.Byte3 = l->bytes.Byte0;                \
    l->bytes.Byte0 = tmp;                     \
    tmp = l->bytes.Byte2;                     \
    l->bytes.Byte2 = l->bytes.Byte1;                \
    l->bytes.Byte1 = tmp;                     \
    }
#else
#define REVERSE_BYTES_64(Destination, Source) { \
	*Destination = *Source; \
	}

#define REVERSE_BYTES_32(Destination, Source) \
	*Destination = *Source; \
	}

#define REVERSE_BYTES_16(Destination, Source) { \
	*Destination = *Source \
	}

#define REVERSE_16(Short) { \
	Short; \
	}
#define REVERSE_32(Long) { \
	Long; \
	}

#endif //#ifdef __LITTLE_ENDIAN_BYTEORDER

#define NDAS_IO_STATUS_SUCCESS					0x00	// CCB completed without error
#define NDAS_IO_STATUS_BUSY						0x05	// busy.
#define NDAS_IO_STATUS_STOP						0x23	// device stopped.
#define NDAS_IO_STATUS_UNKNOWN_STATUS			0x02
#define NDAS_IO_STATUS_NOT_EXIST				0x10    // No Target or Connection is disconnected.
#define NDAS_IO_STATUS_INVALID_COMMAND			0x11	// Invalid command, out of range, unknown command
#define NDAS_IO_STATUS_DATA_OVERRUN				0x12
#define NDAS_IO_STATUS_COMMAND_FAILED			0x13
#define NDAS_IO_STATUS_RESET					0x14
#define NDAS_IO_STATUS_COMMUNICATION_ERROR		0x21
#define NDAS_IO_STATUS_COMMMAND_DONE_SENSE		0x22
#define NDAS_IO_STATUS_COMMMAND_DONE_SENSE2		0x24
#define NDAS_IO_STATUS_NO_ACCESS				0x25	// No access right to execute this ccb operation.
#define NDAS_IO_STATUS_LOST_LOCK				0x30

#define NDAS_VENDOR_ID		"NDAS"
#define NDAS_VENDOR_ID_ATA	"ATA     "

#endif  /*#ifndef __NDAS_SCSI_FMT_H__ */
