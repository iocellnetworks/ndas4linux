/*
 -------------------------------------------------------------------------
 Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
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

#include <linux/version.h>
#include <linux/module.h> 

#include <linux/delay.h>

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include "ndasflowcontrol.h"

#ifdef DEBUG

int	dbg_level_ndfc = DBG_LEVEL_NDFC;

#define dbg_call(l,x...) do { 								\
    if (l <= dbg_level_ndfc) {								\
        printk("%s|%d|%d|",__FUNCTION__, __LINE__,l); 	\
        printk(x); 											\
    } 														\
} while(0)

#else
#define dbg_call(l,x...) do {} while(0)
#endif


ULONG 
Log2 (
	ULONGLONG	Seed
	) 
{
	ULONG	Log2 = 0;

	while (Seed >>= 1) {

		Log2 ++;
	}
	
	return Log2;
}

NTSTATUS
NdasFcInitialize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics
	)
{
	LONG	idx;

	ULONG	test1;
	ULONG	test2;

	test1 = (ULONG)-1;
	test2 = 1;

	dbg_call( 2, "test2 - test1 = %lu\n", test2 - test1 );

	NDAS_BUG_ON( test2 - test1 != 2 );

	test1 = (ULONG)-3;
	test2 = (ULONG)-1;

	dbg_call( 2, "test2 - test1 = %lu\n", test2 - test1 );

	NDAS_BUG_ON( test2 - test1 != 2 );

	test1 = 0x7FFFFFFF;
	test2 = 0x80000001;

	dbg_call( 2, "test2 - test1 = %lu\n", test2 - test1 );

	NDAS_BUG_ON( test2 - test1 != 2 );

	dbg_call( 2, "enter\n" );

	memset( NdasFcStatistics, 0, sizeof(NDAS_FC_STATISTICS) );

	for (idx = 0; idx < (NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT); idx++) {

		NdasFcStatistics->IoSize[idx] =	NDAS_FC_DEFAULT_TROUGHPUT;
		NdasFcStatistics->IoTime[idx] = HZ;

		NdasFcStatistics->TotalIoSize += NdasFcStatistics->IoSize[idx];
		NdasFcStatistics->TotalIoTime += NdasFcStatistics->IoTime[idx];
	} 

	return NDAS_OK;
}

UINT32
NdasFcChooseTransferSize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics,
	IN UINT32				IoSize
	)
{
	ULONG	idx;
	ULONG	maxIdx;

	if (IoSize < ((NdasFcStatistics->MinimumIdx+1) << NDAS_FC_LOG_OF_IO_UNIT)) {

		return IoSize;
	}

	maxIdx = NdasFcStatistics->MinimumIdx;

	if (NdasFcStatistics->PreviousIoSize) {

		if (NdasFcStatistics->PreviousIoSize >= IoSize) {
 
			return (NdasFcStatistics->PreviousIdx+1) << NDAS_FC_LOG_OF_IO_UNIT;
		}

		maxIdx = NdasFcStatistics->PreviousIdx;
	};

	for (idx = maxIdx+1; idx < (NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT); idx++) {

		if (IoSize < ((idx+1) << NDAS_FC_LOG_OF_IO_UNIT)) {

			break;
		}

		if ((NdasFcStatistics->IoSize[idx] >> Log2(NdasFcStatistics->IoTime[idx])) >= 
			(NdasFcStatistics->IoSize[maxIdx] >> Log2(NdasFcStatistics->IoTime[maxIdx]))) {

			maxIdx = idx;
		}
	}

	NdasFcStatistics->PreviousIoSize = IoSize;
	NdasFcStatistics->PreviousIdx 	 = maxIdx;
	
	return (maxIdx+1) << NDAS_FC_LOG_OF_IO_UNIT;
}

VOID
NdasFcUpdateTrasnferSize (
	IN PNDAS_FC_STATISTICS	NdasFcStatistics,
	IN UINT32				UnitIoSize,
	IN UINT32				TotalIoSize,
	IN ULONG				StartTime,
	IN ULONG				EndTime
	)
{
	ULONG			unitIoIdx;
	ULONG			idx;

	if (UnitIoSize < ((NdasFcStatistics->MinimumIdx+1) << NDAS_FC_LOG_OF_IO_UNIT)) {

		return;
	}

	NDAS_BUG_ON( time_before(EndTime, StartTime) );

	unitIoIdx = (UnitIoSize >> NDAS_FC_LOG_OF_IO_UNIT) - 1;

	NdasFcStatistics->TotalTermIoSize += TotalIoSize;
	NdasFcStatistics->TotalTermIoTime += EndTime - StartTime;

	NdasFcStatistics->TotalIoCount ++;

	NdasFcStatistics->TermIoSize[unitIoIdx] += TotalIoSize;
	NdasFcStatistics->TermIoTime[unitIoIdx] += EndTime - StartTime;

	NdasFcStatistics->HitCount[unitIoIdx] ++;

	if (NdasFcStatistics->HitCount[unitIoIdx] % NDAS_FC_IO_TERM == 0) {

		NdasFcStatistics->PreviousIoSize = 0;

		NdasFcStatistics->IoSize[unitIoIdx] += NdasFcStatistics->TermIoSize[unitIoIdx];
		NdasFcStatistics->IoTime[unitIoIdx] += NdasFcStatistics->TermIoTime[unitIoIdx];

		NdasFcStatistics->TermIoSize[unitIoIdx] = 0;
		NdasFcStatistics->TermIoTime[unitIoIdx] = 0;

	} else if (NdasFcStatistics->TermIoTime[unitIoIdx] >= msecs_to_jiffies(100)) {	// for low speed

		if (((NdasFcStatistics->PreviousIdx+1) << NDAS_FC_LOG_OF_IO_UNIT) > (24 << 10)) { // up to 24 KByte

			NdasFcStatistics->PreviousIoSize -= 2;
		
		} else {

			NdasFcStatistics->PreviousIoSize = 0;
		} 
		
		NdasFcStatistics->IoSize[unitIoIdx] += NdasFcStatistics->TermIoSize[unitIoIdx];
		NdasFcStatistics->IoTime[unitIoIdx] += NdasFcStatistics->TermIoTime[unitIoIdx];

		NdasFcStatistics->TermIoSize[unitIoIdx] = 0;
		NdasFcStatistics->TermIoTime[unitIoIdx] = 0;
	}

	if (NdasFcStatistics->TotalIoCount % NDAS_FC_FRACTION_RATE == 0) {

		for (idx = 0; idx < (NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT); idx++) {

			if ((NdasFcStatistics->IoSize[idx] >> Log2(NdasFcStatistics->IoTime[idx])) >= 
				(NdasFcStatistics->TotalTermIoSize >> Log2(NdasFcStatistics->TotalTermIoTime))) {

				NdasFcStatistics->IoSize[idx] -= NdasFcStatistics->TotalTermIoSize >> NDAS_FC_LOG_OF_FRACTION_RATE;
				NdasFcStatistics->IoTime[idx] -= NdasFcStatistics->TotalTermIoTime >> NDAS_FC_LOG_OF_FRACTION_RATE;
			
			} else {

				NdasFcStatistics->IoSize[idx] += NdasFcStatistics->TotalTermIoSize >> NDAS_FC_LOG_OF_FRACTION_RATE;
				NdasFcStatistics->IoTime[idx] += NdasFcStatistics->TotalTermIoTime >> NDAS_FC_LOG_OF_FRACTION_RATE;
			}
		}

		NdasFcStatistics->TotalIoSize += NdasFcStatistics->TotalTermIoSize;
		NdasFcStatistics->TotalIoTime += NdasFcStatistics->TotalTermIoTime;

		NdasFcStatistics->TotalTermIoSize = 0;
		NdasFcStatistics->TotalTermIoTime = 0;

		NdasFcStatistics->PreviousIoSize = 0;

		if ((NdasFcStatistics->TotalIoSize >> (Log2(NdasFcStatistics->TotalIoTime) >> 
			Log2(msecs_to_jiffies(1000)))) <= NDAS_FC_DEFAULT_TROUGHPUT) {

			NdasFcStatistics->MinimumIdx = 0;

		} else {

			NdasFcStatistics->MinimumIdx = (8<<10) >> NDAS_FC_LOG_OF_IO_UNIT;
		}
	}

	if (NdasFcStatistics->TotalIoCount % (NDAS_FC_FRACTION_RATE << 3) == 0 || 
		(NdasFcStatistics->TotalIoCount % NDAS_FC_FRACTION_RATE == 0 && 
		 NdasFcStatistics->TermIoTime[unitIoIdx] >= msecs_to_jiffies(100))) {

		dbg_call( 1, "TotalIoSize = %lld, TotalIoTime = %lld, Throughput = %lld\n",
					  NdasFcStatistics->TotalIoSize, NdasFcStatistics->TotalIoTime,
					  NdasFcStatistics->TotalIoSize >> Log2(NdasFcStatistics->TotalIoTime) );

		for (idx = 0; idx < (NDAS_FC_MAX_IO_SIZE >> NDAS_FC_LOG_OF_IO_UNIT); idx+=4) {

			dbg_call( 1, "%3li, %6li, %7lld, %6lu " 
						 "%3li, %6li, %7lld, %6lu " 
						 "%3li, %6li, %7lld, %6lu " 
						 "%3li, %6li, %7lld, %6lu\n", 
						   idx,   (idx+1)<<NDAS_FC_LOG_OF_IO_UNIT, 
						   NdasFcStatistics->IoSize[idx] >> Log2(NdasFcStatistics->IoTime[idx]), NdasFcStatistics->HitCount[idx],
						   idx+1, (idx+2)<<NDAS_FC_LOG_OF_IO_UNIT, 
						   NdasFcStatistics->IoSize[idx+1] >> Log2(NdasFcStatistics->IoTime[idx+1]), NdasFcStatistics->HitCount[idx+1],
						   idx+2, (idx+3)<<NDAS_FC_LOG_OF_IO_UNIT, 
						   NdasFcStatistics->IoSize[idx+2] >> Log2(NdasFcStatistics->IoTime[idx+2]), NdasFcStatistics->HitCount[idx+2],
						   idx+3, (idx+4)<<NDAS_FC_LOG_OF_IO_UNIT, 
						   NdasFcStatistics->IoSize[idx+3] >> Log2(NdasFcStatistics->IoTime[idx+3]), NdasFcStatistics->HitCount[idx+3] );
		}
	}
}
