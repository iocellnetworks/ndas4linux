/*
 -------------------------------------------------------------------------
 Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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
 revised by William Kim 04/10/2008
 -------------------------------------------------------------------------
*/

#ifndef _NDAS_FLOW_CONTROL_H_
#define _NDAS_FLOW_CONTROL_H_

typedef unsigned long long	ULONGLONG;
typedef long long			LONGLONG;

typedef unsigned long		ULONG;
typedef long				LONG;

typedef unsigned int		UINT32;

typedef void				VOID;

typedef ndas_error_t		NTSTATUS;

#define IN
#define OUT

#define NDAS_FC_LOG_OF_FRACTION_RATE	(6)
#define NDAS_FC_FRACTION_RATE			(1 << NDAS_FC_LOG_OF_FRACTION_RATE)

#define NDAS_FC_LOG_OF_IO_UNIT			(12)
#define NDAS_FC_IO_UNIT					(1 << NDAS_FC_LOG_OF_IO_UNIT)

#define NDAS_FC_MAX_IO_SIZE				(64 << 10)

#define NDAS_FC_DEFAULT_TROUGHPUT		(32L << 20 >> 3) /* 32 Mbps */

#define NDAS_FC_IO_TERM					(32)

C_ASSERT( 412, (NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT) % 8 == 0 );

typedef struct _NDAS_FC_STATISTICS {

	ULONG		TotalIoCount;

	ULONGLONG	IoSize[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];
	ULONGLONG	IoTime[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];
	ULONG		HitCount[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];

	ULONGLONG	TotalIoSize;
	ULONGLONG	TotalIoTime;

	ULONGLONG	TermIoSize[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];
	ULONGLONG	TermIoTime[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];
	ULONG		TermHitCount[NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT];

	ULONGLONG	TotalTermIoSize;
	ULONGLONG	TotalTermIoTime;

	ULONG		MinimumIdx;

	ULONG		PreviousIdx;
	UINT32		PreviousIoSize;

} NDAS_FC_STATISTICS, *PNDAS_FC_STATISTICS;

NTSTATUS
NdasFcInitialize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics
	);

UINT32
NdasFcChooseTransferSize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics,
	IN UINT32				IoSize
	);

VOID
NdasFcUpdateTrasnferSize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics,
	IN UINT32				UnitIoSize,
	IN UINT32				TotalIoSize,
	IN ULONG				StartTime,
	IN ULONG				EndTime
	);

#endif

