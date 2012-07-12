#ifndef _LOCKMGMT_H_
#define _LOCKMGMT_H_

#include "xplatcfg.h"
#include <sal/types.h>
#include "netdisk/conn.h"


#define NDAS_MAX_DEVICE_LOCK_COUNT 4 /* for HW version 1.1 and version 2.0 */

#define NDAS_DEVLOCK_ID_NONE		0
#define NDAS_DEVLOCK_ID_XIFS		1
#define NDAS_DEVLOCK_ID_EXTLOCK	2
#define NDAS_DEVLOCK_ID_BUFFLOCK	3

typedef struct _NDAS_DEVLOCK_STATUS {
	xuchar	Acquired:1;				// Lock acquired
	xuchar	AddressRangeValid:1;	// Valid is IOs' address range that requires lock acquisition
	xuchar	Lost:1;					// Lock lost during reconnection.
	xuchar	Reserved1:5;
	xuchar	Reserved2[3];
	xuint64	StartingAddress;		// IO address range
	xuint64	EndingAddress;			// IO address range
} NDAS_DEVLOCK_STATUS, *PNDAS_DEVLOCK_STATUS;

typedef struct _NDAS_DEVLOCK_INFO {
    xuint32				AcquiredLockCount;
    xuint32				LostLockCount;
    // Index value is lock ID.
    NDAS_DEVLOCK_STATUS	DevLockStatus[NDAS_MAX_DEVICE_LOCK_COUNT];
} NDAS_DEVLOCK_INFO;




typedef struct _BUFFLOCK_CONTROL {

#if 0
        // System timer resolution. time per tick ( time is in 100 nanosecond unit )
	xuint32			TimerResolution;
#endif
	// All lock operation should take place in quantum scale.
	sal_msec	Quantum;

	// Priority wait time
	sal_msec	PriorityWaitTime;

	// Maximum time of IO operations with the buffer lock acquisition.
	sal_msec	AcquisitionMaxTime;
	sal_msec	AcquisitionExpireTime; // CurrentTime + AcquisitionMaxTime

	// Maximum amount of the IO size with the buffer lock acquisition.
	xuint32			MaxIOBytes;

	// Accumulated IO length
	xuint64			AccumulatedIOBytes;

	// Release the buffer lock if there is no IO for a while.
	sal_msec	IoIdleTimeOut;

	// Set if idle routine is expired.
	// Clear if the buffer lock request arrives.
	xbool			IoIdle;

	// IO idle event
	// The IO thread waits for this event.
    dpc_id                 IoIdleTimerExpire;

	//        sal_semaphore      IoIdleTimerExpire;

	// Host count connected to the NDAS device including myself.
	xuint32			ConnectedHosts;

	// The number of the buffer lock acquisition from all connected hosts 
	// after the NDAS device powers up.
	xuint32			CurrentLockCount;

	//
	// Acquisition request pending count
	//
	xuint32			MyRequestCount;

	// Lock acquisition priority
	xuint32			LockCountWhenReleased;
	xuint32			RequestCountWhenReleased;
	xuint32			Priority;

	// Indicate that buffer lock collision control works.
	xbool			BufferLockConrol;

	// Indicate that buffer lock is controlled by the parent node.
	xbool			BufferLockParent;

} BUFFLOCK_CONTROL, *PBUFFLOCK_CONTROL;


void
LockCacheAllLocksLost(
    IN NDAS_DEVLOCK_INFO*    LockInfo
);

xbool
LockCacheAcquiredLocksExistsExceptForBufferLock(
    IN NDAS_DEVLOCK_INFO*    LockInfo
);

xbool
LockCacheCheckLostLockIORange(
    IN NDAS_DEVLOCK_INFO*    LockInfo,
    IN xuint64        StartingAddress,
    IN xuint64        EndingAddress
);


ndas_error_t
WaitForBufferLockRelease(
    IN PBUFFLOCK_CONTROL BuffLockCtl,
    IN uconn_t*    conn,
    OUT xuchar*            LockData,
    OUT xbool*        LockAcquired
);

ndas_error_t
NdasAcquireBufferLock(
    IN PBUFFLOCK_CONTROL BuffLockCtl,
    IN uconn_t* conn,
    IN NDAS_DEVLOCK_INFO* LockInfo,
    OUT xuchar*            LockData,
    IN sal_msec   TimeOut
);

ndas_error_t
NdasReleaseBufferLock(
    IN PBUFFLOCK_CONTROL BuffLockCtl,
    IN uconn_t*    conn,
    IN NDAS_DEVLOCK_INFO* LockInfo,
    OUT xuchar*            LockData,
    IN sal_msec   TimeOut,
    IN xbool            Force,
    IN xuint32            TransferredIoLength
);

/* To do: remove udev from StartIoIdleTimer to remove dependency on udev */
struct _udev_t;

ndas_error_t
StartIoIdleTimer(
    PBUFFLOCK_CONTROL BuffLockCtl,
    struct _udev_t* udev
);

ndas_error_t
StopIoIdleTimer(
    PBUFFLOCK_CONTROL BuffLockCtl
);

ndas_error_t
EnterBufferLockIoIdle(
    IN PBUFFLOCK_CONTROL BuffLockCtl,
    IN uconn_t*    conn,
    IN NDAS_DEVLOCK_INFO* LockInfo
);

ndas_error_t
LMInitialize(
    IN uconn_t* conn,
    IN PBUFFLOCK_CONTROL    BuffLockCtl,
    IN xbool                InitialState
);

void
LMEnable(
    IN PBUFFLOCK_CONTROL    BuffLockCtl,
    IN xbool                Activate,
    IN xbool                ParentControl
);

ndas_error_t
LMDestroy(
    IN PBUFFLOCK_CONTROL BuffLockCtl
);

ndas_error_t
OpAcquireDevLock(
    IN uconn_t*  conn,
    IN xuint32  LockIndex,
    OUT xuchar* LockData,
    IN sal_msec TimeOut,
    IN xbool            RetryWhenFailure
);

ndas_error_t
OpReleaseDevLock(
    IN uconn_t*    conn,
    IN xuint32            LockIndex,
    IN xuchar*            LockData,
    IN sal_msec   TimeOut
);

#define TARGETDATA_REQUEST_COUNT_MASK	0x0000000000ffffffLL


#endif
