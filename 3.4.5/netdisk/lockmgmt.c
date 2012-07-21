/*
    Buffer lock managememt.
    Branched from http://liah/repos/ndas/fremont/trunk/src/drivers/ndasklib/lockmgmt.c
*/
#ifndef NDAS_NO_LANSCSI

#include "xplatcfg.h"
#include <sal/types.h>
#include <ndasuser/ndasuser.h>
#include <sal/debug.h>
#include "lockmgmt.h"
#include "lspx/lsp.h"
#include "netdisk/conn.h"
#include "udev.h"

#define DEBUG_LEVEL_LOCK 1
#undef INLINE
#define INLINE inline

#ifdef DEBUG
#define    debug_lock(l, x...)    do {\
    if(l <= DEBUG_LEVEL_LOCK) {     \
        sal_debug_print("UN|%d|%s|",l,__FUNCTION__); \
        sal_debug_println(x);    \
    } \
} while(0)    
#else
#define    debug_lock(l, x...)    do {} while(0)
#endif

//
// Lock ID map
// Maps LUR device lock ID to ndas device lock index depending on the target ID.
//

#define MAX_TARGET_ID    2
#define NDAS_NR_GPLOCK        (1 + 3)    // NDAS chip 1.1, 2.0 including null lock, buffer lock, and write previlege lock.
#define NDAS_NR_ADV_GPLOCK    (1 + 8)    // NDAS chip 2.5 including null lock.

// General purpose lock supported by NDAS chip 1.1, 2.0
xuint32 DevLockIdMap[MAX_TARGET_ID][NDAS_NR_GPLOCK] = {
    {-1, 2, 3, 0}, // Target 0 uses lock 0 for buffer lock
    {-1, 2, 3, 1}  // Target 0 uses lock 1 for buffer lock
};
// Advanced general purpose lock supported by NDAS chip 2.5
xuint32 DevLockIdMap_Adv[MAX_TARGET_ID][NDAS_NR_ADV_GPLOCK] = {
    {-1, 0, 1, 2, 3, 4, 5, 6, 7},
    {-1, 0, 1, 2, 3, 4, 5, 6, 7}
};

xuint32
LockIdToTargetLockIdx(xuint32 HwVesion, xuint32 TargetId, xuint32 LockId) {
    xuint32    lockIdx;
    //
    // Map the lock ID.
    //
    sal_assert(TargetId < MAX_TARGET_ID);
    if(HwVesion == NDAS_VERSION_1_1 ||
        HwVesion == NDAS_VERSION_2_0
        ) {
        lockIdx = DevLockIdMap[TargetId][LockId];
    } else {
        lockIdx = DevLockIdMap_Adv[TargetId][LockId];
    }

    return lockIdx;
}

//////////////////////////////////////////////////////////////////////////
//
// Lock operations
//

//
// Acquire a general purpose lock.
// NOTE: With RetryWhenFailure is FALSE, be aware that the lock-cleanup
//        workaround will not be performed in this function.
//
// return values
//    NDAS_ERROR_ACQUIRE_LOCK_FAILED : Lock is owned by another session.
//                            This is not communication error.
//
// Opxxxx is calling conn_xxx functions and it is not multi-thread safe.Should be called in ndiod thread or single thread.
//

ndas_error_t
OpAcquireDevLock(
    IN uconn_t*  conn,
    IN xuint32  LockIndex,
    OUT xuchar* LockData,
    IN sal_msec TimeOut,
    IN xbool            RetryWhenFailure
){
    ndas_error_t status = NDAS_ERROR_ACQUIRE_LOCK_FAILED;
    xuint32 lockContention;
    sal_msec    startTime;
    sal_msec    maximumWaitTime;

    lockContention = 0;
    startTime = sal_time_msec();

    if(TimeOut) {
        maximumWaitTime = TimeOut;
    } else {
        maximumWaitTime = 2 * SAL_MSEC_PER_SEC;
    }

    while(sal_tick_sub(sal_time_msec(),  sal_tick_add(startTime, maximumWaitTime))<=0) {
            status = conn_lock_operation(conn, NDAS_LOCK_TAKE, LockIndex, LockData, NULL);
//        status = udev_lock_operation(udev, NDAS_LOCK_TAKE, LockIndex, LockData, NULL);

        if(status == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {
            debug_lock(4, "LockIndex%u: Lock contention #%u!!!", LockIndex, lockContention);
        } else if(NDAS_SUCCESS(status)) {
            break;
        } else {
            break;
        }
        lockContention ++;

        //
        //    Clean up the lock on the NDAS device.
        //

        if(lockContention != 0 && (lockContention % 10000 == 0)) {
            conn_lock_operation(conn, NDAS_LOCK_BREAK, LockIndex, NULL, NULL);
//            udev_lock_operation(udev, NDAS_LOCK_BREAK, LockIndex, NULL, NULL);
        }

        if(RetryWhenFailure == FALSE) {
            break;
        }
    }
    if(status == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {
        debug_lock(1, "Lock denied. idx=%u lock contention=%u", LockIndex, lockContention);
    } else if(!NDAS_SUCCESS(status)) {
        debug_lock(1, "Failed to acquire lock idx=%u lock contention=%u", LockIndex, lockContention);
    } else {
        debug_lock(4, "Acquired lock idx=%u lock contention=%u", LockIndex, lockContention);
    }

    return status;
}

ndas_error_t
OpReleaseDevLock(
    IN uconn_t*    conn,
    IN xuint32            LockIndex,
    IN xuchar*            LockData,
    IN sal_msec   TimeOut
){
    ndas_error_t    status;

    status = conn_lock_operation(conn, NDAS_LOCK_GIVE, LockIndex, LockData, NULL);
//    status = udev_lock_operation(udev, NDAS_LOCK_GIVE, LockIndex, LockData, NULL);

    if(!NDAS_SUCCESS(status)) {
        debug_lock(4, "Failed to release lock idx=%d", LockIndex);
    } else {
        debug_lock(4, "Release lock idx=%d", LockIndex);
    }
    return status;
}

#if 0 /* not used */
static
ndas_error_t
OpQueryDevLockOwner(
    IN uconn_t*    conn,
    IN xuint32            LockIndex,
    IN xuchar*            LockOwner,
    IN sal_msec   TimeOut
){
    ndas_error_t    status;

    status = conn_lock_operation(conn, NDAS_LOCK_GET_OWNER, LockIndex, LockOwner, NULL);
/*    status = udev_lock_operation(
                udev,
                NDAS_LOCK_GET_OWNER,
                LockIndex,
                LockOwner,
                NULL);*/
    if(!NDAS_SUCCESS(status)) {
        debug_lock(1,"Failed to query lock owner idx=%d", LockIndex);
        return status;
    }

    return NDAS_OK;
}
#endif

static
ndas_error_t
OpGetDevLockData(
    IN uconn_t*    conn,
    IN xuint32            LockIndex,
    IN xuchar*            LockData,
    IN sal_msec   TimeOut
){
    ndas_error_t    status;

    status = conn_lock_operation(conn, NDAS_LOCK_GET_COUNT, LockIndex, LockData, NULL);
/*    status = udev_lock_operation(
                udev,
                NDAS_LOCK_GET_COUNT,
                LockIndex,
                LockData,
                NULL);*/
    if(!NDAS_SUCCESS(status)) {
        debug_lock(1, "Failed to get lock data idx=%d", LockIndex);
        return status;
    }

    return NDAS_OK;
}

//////////////////////////////////////////////////////////////////////////
//
// Lock cache
//

static
INLINE
xbool
LockCacheIsAcquired (
    IN NDAS_DEVLOCK_INFO*    LockInfo,
    IN xuint32		LockId
) {
	PNDAS_DEVLOCK_STATUS	lockStatus = &LockInfo->DevLockStatus[LockId];

	return lockStatus->Acquired;
}

static
INLINE
void
LockCacheSetDevLockAcquisition(
    IN NDAS_DEVLOCK_INFO*    LockInfo,
    IN xuint32        LockId,
    IN xbool        AddressRangeValid,
    IN xuint64        StartingAddress,
    IN xuint64        EndingAddress
) {
    PNDAS_DEVLOCK_STATUS    lockStatus = &LockInfo->DevLockStatus[LockId];

    sal_assert(lockStatus->Acquired == FALSE);
    sal_assert(LockInfo->AcquiredLockCount < NDAS_MAX_DEVICE_LOCK_COUNT);
    sal_assert(StartingAddress <= EndingAddress);

    LockInfo->AcquiredLockCount ++;
    lockStatus->Acquired = TRUE;
    if(lockStatus->Lost) {
        debug_lock(1, "%u: Cleared lost state.", LockId);
        LockInfo->LostLockCount --;
        lockStatus->Lost = FALSE;
    }
    if(AddressRangeValid) {
        lockStatus->AddressRangeValid = 1;
        lockStatus->StartingAddress = StartingAddress;
        lockStatus->EndingAddress = EndingAddress;
    } else {
        lockStatus->AddressRangeValid = 0;
    }
}

static
INLINE
void
LockCacheSetDevLockRelease(
    IN NDAS_DEVLOCK_INFO*    LockInfo,
    IN xuint32        LockId
){
    PNDAS_DEVLOCK_STATUS    lockStatus = &LockInfo->DevLockStatus[LockId];

    sal_assert(lockStatus->Acquired == TRUE);
    sal_assert(LockInfo->AcquiredLockCount > 0);

    LockInfo->AcquiredLockCount --;
    lockStatus->Acquired = FALSE;
    lockStatus->AddressRangeValid = 0;
}

static
INLINE
void
LockCacheInvalidateAddressRange(
    IN NDAS_DEVLOCK_INFO*    LockInfo,
    IN xuint32        LockId
){
    PNDAS_DEVLOCK_STATUS    lockStatus = &LockInfo->DevLockStatus[LockId];

    lockStatus->AddressRangeValid = 0;
}

static
INLINE
xbool
LockCacheIsLost(
    IN NDAS_DEVLOCK_INFO*    LockInfo,
    IN xuint32        LockId
){
    PNDAS_DEVLOCK_STATUS    lockStatus = &LockInfo->DevLockStatus[LockId];

    return lockStatus->Lost;
}

static
INLINE
void
LockCacheSetDevLockLoss(
    IN NDAS_DEVLOCK_INFO*    LockInfo,
    IN xuint32        LockId
){
    PNDAS_DEVLOCK_STATUS    lockStatus = &LockInfo->DevLockStatus[LockId];

    sal_assert(lockStatus->Acquired == TRUE);
    sal_assert(lockStatus->Lost == FALSE);
    sal_assert(LockInfo->LostLockCount < NDAS_MAX_DEVICE_LOCK_COUNT);

    LockCacheSetDevLockRelease(LockInfo, LockId);
    debug_lock(1, "Lost lock #%d", LockId);

    LockInfo->LostLockCount ++;
    lockStatus->Lost = TRUE;
}

static
INLINE
void
LockCacheClearDevLockLoss(
    IN NDAS_DEVLOCK_INFO*    LockInfo,
    IN xuint32        LockId
){
    PNDAS_DEVLOCK_STATUS    lockStatus = &LockInfo->DevLockStatus[LockId];

    sal_assert(lockStatus->Lost == TRUE);
    sal_assert(LockInfo->LostLockCount > 0);

    debug_lock(1, "Confirmed lost lock #%d", LockId);

    LockInfo->LostLockCount --;
    lockStatus->Lost = FALSE;
}


//
// Mark all lock lost during reconnection.
//

void
LockCacheAllLocksLost(
    IN NDAS_DEVLOCK_INFO*    LockInfo
){
    xuint32        lockId;

    for(lockId = 0; lockId < NDAS_MAX_DEVICE_LOCK_COUNT; lockId ++) {
        if(LockInfo->DevLockStatus[lockId].Acquired) {

            LockCacheSetDevLockLoss(LockInfo, lockId);
#ifdef DEBUG
            if(lockId == NDAS_DEVLOCK_ID_BUFFLOCK) {
                debug_lock( 1, "Lost Buf lock #%d", lockId);
            } else {
                debug_lock(1, "Lost lock #%d", lockId);
            }
#endif
        }
    }

#ifdef DEBUG
    if(LockInfo->LostLockCount == 0) {
        debug_lock(1, "No Lost lock");
    }
#endif
}

//
// Check to see if there are lock acquired except for the buffer lock.
//
// Called when link has changed. This case is not implemented in xplat.
//
xbool
LockCacheAcquiredLocksExistsExceptForBufferLock(
    IN NDAS_DEVLOCK_INFO*    LockInfo
){
    xuint32        lockId;

    for(lockId = 0; lockId < NDAS_MAX_DEVICE_LOCK_COUNT; lockId ++) {
        if(LockInfo->DevLockStatus[lockId].Acquired) {

            if(lockId != NDAS_DEVLOCK_ID_BUFFLOCK) {
                debug_lock(1, "Lost lock #%d", lockId);
                return TRUE;
            }
        }
    }

    return FALSE;
}


//
// Check if the IO range requires a lock that is lost.
//

xbool
LockCacheCheckLostLockIORange(
    IN NDAS_DEVLOCK_INFO*    LockInfo,
    IN xuint64        StartingAddress,
    IN xuint64        EndingAddress
){
    xuint32    lostLockCnt;
    xuint32    lockId;
    PNDAS_DEVLOCK_STATUS    devLockStatus;

    sal_assert(StartingAddress <= EndingAddress);

    lostLockCnt = LockInfo->LostLockCount;
    lockId = 0;
    while(lostLockCnt &&
        lockId < NDAS_MAX_DEVICE_LOCK_COUNT) {

        devLockStatus = &LockInfo->DevLockStatus[lockId];

        if(devLockStatus->Lost) {

            //
            // Check the intersection.
            //
            if(devLockStatus->AddressRangeValid) {

                if(StartingAddress < devLockStatus->StartingAddress) {
                    if(EndingAddress >= devLockStatus->StartingAddress) {
                        debug_lock(1, "The address range is in lost lock's address range(1).");
                        return TRUE;
                    }
                } else if(StartingAddress <= devLockStatus->EndingAddress) {
                    debug_lock(1, "The address range is in lost lock's address range(2).");
                    return TRUE;
                }
            }

            lostLockCnt --;

        }
        lockId++;
    }

    sal_assert(lostLockCnt == 0);

    return FALSE;
}

#if 0 /* Not used */
//////////////////////////////////////////////////////////////////////////
//
// Device lock control request dispatcher
//

ndas_error_t
LurnIdeDiskDeviceLockControl(
    IN PLURELATION_NODE        Lurn,
    IN PLURNEXT_IDE_DEVICE    IdeDisk,
    IN PCCB                    Ccb
){
    ndas_error_t                status;
    PLURN_DEVLOCK_CONTROL    devLockControl;
    xuint32                    lockIdx;
    xuint32                    lockDataLength;

    UNREFERENCED_PARAMETER(Lurn);

    if(Ccb->DataBufferLength < sizeof(LURN_DEVLOCK_CONTROL)) {
        Ccb->CcbStatus = CCB_STATUS_DATA_OVERRUN;
        return NDAS_OK;
    }
    status = NDAS_OK;

    //
    // Verify the lock operation
    //

    devLockControl = (PLURN_DEVLOCK_CONTROL)Ccb->DataBuffer;

    if(devLockControl->LockId == LURNDEVLOCK_ID_NONE) {

        sal_assert( FALSE );

        // Nothing to do.
        Ccb->CcbStatus = CCB_STATUS_SUCCESS; 
        return NDAS_OK;
    }

    switch(IdeDisk->udev.HwVersion) {
    case NDAS_VERSION_1_1:
    case NDAS_VERSION_2_0:
#ifdef DEBUG
    if (devLockControl->LockId == NDAS_DEVLOCK_ID_BUFFLOCK) {
    
        sal_assert( LsCcbIsFlagOn(Ccb, CCB_FLAG_CALLED_INTERNEL) );

    }
#endif

    if(devLockControl->LockId >= NDAS_NR_GPLOCK) {
        Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED; 
        return NDAS_OK;
    }
    if(devLockControl->AdvancedLock) {
        Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED; 
        return NDAS_OK;
    }
    lockDataLength = 4;

    break;


#if 0
    case LANSCSIIDE_VERSION_2_5:
        if(devLockControl->LockId >= NDAS_NR_ADV_GPLOCK) {
            Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED; 
            return NDAS_OK;
        }
        if(!devLockControl->AdvancedLock) {
            Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED; 
            return NDAS_OK;
        }
        lockDataLength = 64;
        break;
#endif

    case LANSCSIIDE_VERSION_1_0:
    default:

        sal_assert( FALSE );

        Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND; 
        return NDAS_OK;
    }

    // Check to see if the lock acquisition is required.
    if(    devLockControl->RequireLockAcquisition
        ) {
        if(IdeDisk->udev.lock_info.DevLockStatus[devLockControl->LockId].Acquired == FALSE) {
            Ccb->CcbStatus = CCB_STATUS_LOST_LOCK;
            return NDAS_OK;
        }
    }

    //
    // Map the lock ID.
    //

    sal_assert(IdeDisk->udev.conn.unit < MAX_TARGET_ID);
    if(IdeDisk->udev.HwVersion == NDAS_VERSION_1_1 ||
        IdeDisk->udev.HwVersion == NDAS_VERSION_2_0
        ) {
        lockIdx = DevLockIdMap[IdeDisk->udev.conn.unit][devLockControl->LockId];
    } else {
        lockIdx = DevLockIdMap_Adv[IdeDisk->udev.conn.unit][devLockControl->LockId];
    }

    //
    // Execute the lock operation
    //

    switch(devLockControl->LockOpCode) {
    case LURNLOCK_OPCODE_ACQUIRE: {
        sal_msec            timeOut;

        if(devLockControl->AddressRangeValid) {
            if(devLockControl->StartingAddress > devLockControl->EndingAddress) {
                Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
                return NDAS_OK;
            }
        }
        if(LockCacheIsLost(&IdeDisk->udev, devLockControl->LockId)) {
            //
            // The lock lost. Clear lost status by releasing it.
            //
//            Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
            Ccb->CcbStatus = CCB_STATUS_LOST_LOCK;

            return NDAS_OK;
        }

        if (devLockControl->LockId == NDAS_DEVLOCK_ID_BUFFLOCK) {

            sal_assert( LockCacheIsAcquired(&IdeDisk->udev,devLockControl->LockId) == FALSE ); 
        }

        //
        // Release the buffer lock to prevent deadlock or race condition
        // between the buffer and the others.
        //

        if(IdeDisk->BuffLockCtl.BufferLockConrol == TRUE) {
            if (devLockControl->LockId != NDAS_DEVLOCK_ID_BUFFLOCK) {
                status = NdasReleaseBufferLock(
                    &IdeDisk->BuffLockCtl,
                    IdeDisk->LanScsiSession,
                    &IdeDisk->udev,
                    NULL,
                    NULL,
                    TRUE,
                    0);
                if(!NDAS_SUCCESS(status)) {
                    Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
                    return status;
                }
            }
        }

        timeOut = devLockControl->ContentionTimeOut;
        status = OpAcquireDevLock(
                        IdeDisk->LanScsiSession,
                        lockIdx,
                        devLockControl->LockData, 
                        &timeOut,
                        TRUE);
        if(NDAS_SUCCESS(status)) {
            Ccb->CcbStatus = CCB_STATUS_SUCCESS;

                sal_assert( LockCacheIsAcquired(&IdeDisk->udev,devLockControl->LockId) == FALSE ); 
            //
            // The buffer lock acquired by outside of the IDE LURN.
            // Hand in the buffer lock control to the outside of the IDE LURN.
            // Turn off the collision control
            //
            if(devLockControl->LockId == NDAS_DEVLOCK_ID_BUFFLOCK && IdeDisk->BuffLockCtl.BufferLockConrol) {
                debug_lock(1, "Bufflock control off.");
                IdeDisk->BuffLockCtl.BufferLockConrol = FALSE;
            }

            //
            // Acquire success
            // Update the lock status
            // Update the lock status only if previously we released the lock.
            //
            if(LockCacheIsAcquired(&IdeDisk->udev, devLockControl->LockId) == FALSE) {

                LockCacheSetDevLockAcquisition(
                    &IdeDisk->udev,
                    devLockControl->LockId,
                    devLockControl->AddressRangeValid,
                    devLockControl->StartingAddress,
                    devLockControl->EndingAddress
                    );
            }
        } else if(status == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {
            Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
            status = NDAS_OK;
        } else {
            Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
        }
        return status;
    }
    case LURNLOCK_OPCODE_RELEASE: {

        if(LockCacheIsAcquired(&IdeDisk->udev, devLockControl->LockId) == FALSE) {
            if(LockCacheIsLost(&IdeDisk->udev, devLockControl->LockId)) {
                //
                // Clear lost status
                //
                LockCacheClearDevLockLoss(&IdeDisk->udev, devLockControl->LockId);
                Ccb->CcbStatus = CCB_STATUS_SUCCESS;
                return NDAS_OK;
            }

            //
            // Already released.
            //

            if (devLockControl->LockId == NDAS_DEVLOCK_ID_BUFFLOCK) {

                sal_assert( FALSE ); 

            }

            Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
            return NDAS_OK;
        }

        sal_assert( LockCacheIsAcquired(&IdeDisk->udev,devLockControl->LockId) == TRUE ); 

        //
        // Release success
        // Update the lock status only if previously we acquired the lock
        // whether the release request succeeds or not.
        //

        if(LockCacheIsAcquired(&IdeDisk->udev, devLockControl->LockId)) {
            LockCacheSetDevLockRelease(&IdeDisk->udev, devLockControl->LockId);
        }

        status = OpReleaseDevLock(
                        IdeDisk->LanScsiSession,
                        lockIdx,
                        devLockControl->LockData,
                        NULL);
        if(NDAS_SUCCESS(status)) {
            Ccb->CcbStatus = CCB_STATUS_SUCCESS;
        } else {
            Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
        }
        return status;
    }
    case LURNLOCK_OPCODE_QUERY_OWNER:
        status = OpQueryDevLockOwner(
                        IdeDisk->LanScsiSession,
                        lockIdx,
                        devLockControl->LockData,
                        NULL);
        if(NDAS_SUCCESS(status)) {
            Ccb->CcbStatus = CCB_STATUS_SUCCESS;
        } else {
            Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
        }
        return status;
    case LURNLOCK_OPCODE_GETDATA:        // Not yet implemented
        status = OpGetDevLockData(
                        IdeDisk->LanScsiSession,
                        lockIdx,
                        devLockControl->LockData,
                        NULL);

        if(NDAS_SUCCESS(status)) {
            Ccb->CcbStatus = CCB_STATUS_SUCCESS;
        } else {
            Ccb->CcbStatus = CCB_STATUS_COMMAND_FAILED;
        }
        return status;
    case LURNLOCK_OPCODE_SETDATA:        // Not yet implemented
    case LURNLOCK_OPCODE_BREAK:            // Not yet implemented
    default: ;
        Ccb->CcbStatus = CCB_STATUS_INVALID_COMMAND;
        return NDAS_OK;
    }

    return status;
}
#endif

//////////////////////////////////////////////////////////////////////////
//
// Buffer lock intention
//

//
// Increase the buffer lock acquisition request count.
// This is expensive operation due to four NDAS requests of lock and target data.
// Avoid to call this function during the buffer lock acquisition.
//

static
ndas_error_t
QueueBufferLockRequest(
    IN PBUFFLOCK_CONTROL    BuffLockCtl,
    IN uconn_t* conn,
    IN NDAS_DEVLOCK_INFO*   LockInfo,
    IN sal_msec       TimeOut
){
    ndas_error_t    status, release_status;
    TEXT_TARGET_DATA    targetData;
    xuint32        lockIdx;
    xuint32        devRequestCount;

    //
    // Map the lock ID.
    //

    lockIdx = LockIdToTargetLockIdx(
        conn->hwdata->hardware_version,
        conn->unit,
        NDAS_DEVLOCK_ID_EXTLOCK);

    //
    // Acquire the extension lock.
    //
    if(LockCacheIsAcquired(LockInfo, NDAS_DEVLOCK_ID_EXTLOCK) == FALSE) {
        status = OpAcquireDevLock(
            conn,
            lockIdx,
            NULL, 
            TimeOut,
            TRUE);
        if(NDAS_ERROR_ACQUIRE_LOCK_FAILED != status && !NDAS_SUCCESS(status)) {
            debug_lock(1, "OpAcquireDevLock() failed. STATUS=%08x", status);
            return status;
        }

        LockCacheSetDevLockAcquisition(LockInfo, NDAS_DEVLOCK_ID_EXTLOCK, FALSE, 0, 0);
    }

    //
    // Read the target data
    //

    status = conn_text_target_data(conn, FALSE, &targetData);
    
    if(!NDAS_SUCCESS(status)) {
        debug_lock(1, "conn_text_target_data() failed. STATUS=%08x", status);
        goto exit;
    }

    //
    // Set intention bit.
    // Do not touch other bit fields.
    //

    devRequestCount = (xuint32)(targetData & TARGETDATA_REQUEST_COUNT_MASK);
    // Update the intention count before increase.

    BuffLockCtl->MyRequestCount ++;
    devRequestCount ++;
    devRequestCount &= TARGETDATA_REQUEST_COUNT_MASK;
    targetData = (targetData & (~TARGETDATA_REQUEST_COUNT_MASK)) | devRequestCount;

    //
    // Write the target data
    //

    status = conn_text_target_data(conn, TRUE, &targetData);

    if(!NDAS_SUCCESS(status)) {
        debug_lock(1, "LspTextTartgetData() failed. STATUS=%08x", status);
        goto exit;
    }

    debug_lock(1, "Request count=%u", devRequestCount);

exit:
    //
    // Release the extension lock.
    // Do not override the status value.
    //

    release_status = OpReleaseDevLock(
        conn, 
        lockIdx,
        NULL,
        TimeOut);

    LockCacheSetDevLockRelease(LockInfo, NDAS_DEVLOCK_ID_EXTLOCK);

    if(!NDAS_SUCCESS(release_status)) {
        debug_lock(1, "OpReleaseDevLock() failed. STATUS=%08x", release_status);
    }

    return status;
}

//
// Retrieve the number of pending requests.
//
static
INLINE
void
UpdateRequestCount(
    IN PBUFFLOCK_CONTROL    BuffLockCtl,
    IN xuint32                RequestCount,
    OUT xulong*                PendingRequests
){
    xuint32 pendingRequests;

    if(RequestCount >= BuffLockCtl->RequestCountWhenReleased)
        pendingRequests = RequestCount - BuffLockCtl->RequestCountWhenReleased;
    else
        pendingRequests = RequestCount + 
        ((TARGETDATA_REQUEST_COUNT_MASK+1) - BuffLockCtl->RequestCountWhenReleased);

#ifdef DEBUG
    if(BuffLockCtl->RequestCountWhenReleased != RequestCount) {
        debug_lock(4, "Request count = %d", pendingRequests);
    }
#endif

    //
    // Set return value of the buffer lock request pending count.
    //
    if(PendingRequests)
        *PendingRequests = pendingRequests;
}

#if 0 /* not used */
static
ndas_error_t
GetBufferLockPendingRequests(
    IN PBUFFLOCK_CONTROL    BuffLockCtl,
    IN uconn_t*        conn,
    OUT xulong*      PendingRequests,
    IN sal_msec       TimeOut
){
    ndas_error_t    status;
    TEXT_TARGET_DATA    targetData;
    xuint32        requestCount;

    status = conn_text_target_data(conn, FALSE, &targetData);

    if(!NDAS_SUCCESS(status)) {
        debug_lock(1, "LspTextTartgetData() failed. %d", status);
        return status;
    }

    debug_lock(1, "TargetData:%lx", ((xuint64)targetData));
    //
    // Match the signature.
    // If not match, it might be interference by anonymous application.
    //
    requestCount = (xuint32)(targetData & TARGETDATA_REQUEST_COUNT_MASK);
    UpdateRequestCount(BuffLockCtl, requestCount, PendingRequests);

    return status;
}
#endif

//
// Retrieve the numbers of pending requests, RW hosts, and RO hosts.
//

static
ndas_error_t
GetBufferLockPendingRequestsWithHostInfo(
    IN PBUFFLOCK_CONTROL    BuffLockCtl,
    IN uconn_t*        conn,
    OUT xulong*                RequestCounts,
    OUT xulong*                PendingRequests,
    OUT xulong*                RWHostCount,
    OUT xulong*                ROHostCount,
    IN sal_msec       TimeOut
){
    ndas_error_t        status;
    lsp_text_target_list_t targetList;
    lsp_text_target_list_element_t*   targetEntry;
    xuint32            idx_targetentry;

    xbool            found;
    xuint32        requestCount;

    status = conn_text_target_list(conn, &targetList, sizeof(lsp_text_target_list_t));

    if(!NDAS_SUCCESS(status)) {
        debug_lock(1, "conn_text_target_list() failed. %d", status);
        return status;
    }

    sal_assert(targetList.number_of_elements <= 2);

    found = FALSE;
    for(idx_targetentry = 0;
        idx_targetentry < MAX_TARGET_ID && idx_targetentry < targetList.number_of_elements;
        idx_targetentry ++) {

        targetEntry = &targetList.elements[idx_targetentry];

        if(targetEntry->target_id == conn->unit) {
            xuint64 targetdata;

			// Network endian to host endian
            targetdata = lsp_ntohll(*((xuint64*)&targetEntry->target_data));
            requestCount = ((xuint32)targetdata & TARGETDATA_REQUEST_COUNT_MASK);
            if(RequestCounts)
                *RequestCounts = requestCount;
                debug_lock(2, "TargetData:0x%llu intentionCount:%u In-mem:%u",
                targetdata,
                requestCount,
                BuffLockCtl->RequestCountWhenReleased);
            UpdateRequestCount(BuffLockCtl, requestCount, PendingRequests);


            if(RWHostCount)
                *RWHostCount = targetEntry->rw_hosts;
            if(ROHostCount)
                *ROHostCount = targetEntry->ro_hosts;

            //
            // Workaround for NDAS chip 2.0 original
            // It does not return correct ReadOnly host count
            //
            if(    conn->hwdata->hardware_version == LSP_HARDWARE_VERSION_2_0 &&
                conn->hwdata->hardware_revision== LSP_HARDWARE_V20_REV_0) {

                if(RWHostCount)
                    *RWHostCount = 1;
                if(ROHostCount)
                    *ROHostCount = NDAS_MAX_CONNECTION_COUNT - 1;
            }

            found = TRUE;
        }
    }

    if(found == FALSE) {
        return NDAS_ERROR_NO_DEVICE;
    }

    return status;
}

//////////////////////////////////////////////////////////////////////////
//
// Quantum management
//

void
DelayForQuantum(
    PBUFFLOCK_CONTROL BuffLockCtl, xuint32 QuantumCount
) {
    if(QuantumCount == 0)
        return;
    sal_msleep(BuffLockCtl->Quantum * QuantumCount);
}

#define LMPRIORITY_NODE_IN_A_LOT            8
#define LMPRIORITY_SQUEEZE(PRIORITY)        (((PRIORITY) + LMPRIORITY_NODE_IN_A_LOT - 1)/LMPRIORITY_NODE_IN_A_LOT)

void
WaitForPriority(
    PBUFFLOCK_CONTROL BuffLockCtl, xuint32 Priority
) {
    if(Priority == 0)
        return;
    sal_msleep(BuffLockCtl->PriorityWaitTime * LMPRIORITY_SQUEEZE(Priority));
}

//////////////////////////////////////////////////////////////////////////
//
// Buffer lock
//


//
// Wait for a host to release the buffer lock.
// If the priority is the highest, will use try to acquire the buffer lock, and
// return the lock data.
//

ndas_error_t
WaitForBufferLockRelease(
    IN PBUFFLOCK_CONTROL BuffLockCtl,
    IN uconn_t*    conn,
    OUT xuchar*            LockData,
    OUT xbool*        LockAcquired
){
    ndas_error_t        status;
    sal_msec    startTime;
    xuint32            idx_trial;
    xuint32            lockCount;
    sal_msec    maximumWaitTime;
    xuint32            bufferLockIdx;

    if(LockAcquired == NULL)
        return NDAS_ERROR_INVALID_PARAMETER;
    *LockAcquired = FALSE;

    bufferLockIdx = LockIdToTargetLockIdx(
        conn->hwdata->hardware_version,
        conn->unit,
        NDAS_DEVLOCK_ID_BUFFLOCK);

    maximumWaitTime = BuffLockCtl->AcquisitionMaxTime * 3 / 2;
//    sal_assert(LURN_IDE_GENERIC_TIMEOUT >= maximumWaitTime);

    startTime = sal_time_msec();

    for(idx_trial = 0;
        sal_tick_sub(sal_time_msec(), startTime + maximumWaitTime) < 0;
        idx_trial++) {
#ifdef DEBUG
        if(idx_trial > 0) {
            debug_lock(1, "Trial #%u", idx_trial);
        }
#endif

        if(BuffLockCtl->Priority != 0 && BuffLockCtl->IoIdle == FALSE) {
//            sal_msec timeOut;
//            timeOut = -1 * BuffLockCtl->AcquisitionMaxTime;

            status = OpGetDevLockData(
                conn,
                bufferLockIdx,
                (xuchar*)&lockCount,
                0);
            if(!NDAS_SUCCESS(status)) {
                debug_lock(1, "LurnIdeDiskGetDevLockData() failed. STATUS=%d", status);
                DelayForQuantum(BuffLockCtl, 1);
                continue;
            }

            if(lockCount != BuffLockCtl->CurrentLockCount) {
                // Set return value.
                if(LockData)
                    *(xulong*)LockData = lockCount;
                BuffLockCtl->CurrentLockCount = lockCount;
                return NDAS_OK;
            } else {
                debug_lock(1, "Lock count did not change. %u", lockCount);
                DelayForQuantum(BuffLockCtl, 1);
                continue;
            }
        } else {

            //
            // If priority is the highest, try to acquire the lock right away.
            //

            status = OpAcquireDevLock(
                conn,
                bufferLockIdx,
                (xuchar*)&lockCount,
                0,
                FALSE);
            if(status == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {
                //
                // Lock count is valid.
                //
                if(LockData)
                    *(xulong*)LockData = lockCount;
                BuffLockCtl->CurrentLockCount = lockCount;
            }
            if(!NDAS_SUCCESS(status)) {
                debug_lock(1, "OpAcquireDevLock() failed. STATUS=%08x", (xuint32)status);
                DelayForQuantum(BuffLockCtl, 1);
                continue;
            }

            //
            // Lock acquired
            //

            *LockAcquired = TRUE;
            // Set return value.
            if(LockData)
                *(xulong*)LockData = lockCount;
            BuffLockCtl->CurrentLockCount = lockCount;

            return NDAS_OK;
        }
    }

    // Lock clean up for NDAS chip 2.0 original.
    conn_lock_operation(conn, NDAS_LOCK_BREAK, bufferLockIdx, NULL, NULL);

    debug_lock(1, "Timeout. Trial = %u LockCount=%u", idx_trial, lockCount);
    return NDAS_ERROR_TIME_OUT;
}

//
// Reset acquisition expire time and bytes.
//

static
INLINE
void
NdasResetAcqTimeAndAccIOBytes(
    IN PBUFFLOCK_CONTROL BuffLockCtl
){
    BuffLockCtl->AcquisitionExpireTime =
        sal_time_msec() + BuffLockCtl->AcquisitionMaxTime;
    BuffLockCtl->AccumulatedIOBytes = 0;
}

//
// Acquire the NDAS buffer lock.
//
// This will be called in the context of ndio thread. Safe to call conn_xx functions directly.
// 
ndas_error_t
NdasAcquireBufferLock(
    IN PBUFFLOCK_CONTROL BuffLockCtl,
    IN uconn_t* conn,
    IN NDAS_DEVLOCK_INFO* LockInfo,
    OUT xuchar*            LockData,
    IN sal_msec   TimeOut
){
    ndas_error_t            status = NDAS_ERROR;
    sal_msec        startTime;
    sal_msec       timeOut;
    xuint32                idx_trial;
    xuint32                lockCount;
    xbool                lockAcquired = FALSE;

    if(conn->unit >= MAX_TARGET_ID)
        return NDAS_ERROR_INVALID_PARAMETER;

    if(BuffLockCtl->BufferLockParent) {
        return NDAS_OK;
    }
    if(LockCacheIsAcquired(LockInfo, NDAS_DEVLOCK_ID_BUFFLOCK)) {
        debug_lock(4, "The buffer lock already acquired.");
        return NDAS_OK;
    }

    //
    // Buffer lock control is disabled.
    // Perform the lock request right away.
    //

    if(BuffLockCtl->BufferLockConrol == FALSE) {
        status = OpAcquireDevLock(
            conn,
            DevLockIdMap[conn->unit][NDAS_DEVLOCK_ID_BUFFLOCK],
            (xuchar*)&lockCount,
            TimeOut,
            TRUE);
        if(NDAS_SUCCESS(status)) {
            LockCacheSetDevLockAcquisition(LockInfo, NDAS_DEVLOCK_ID_BUFFLOCK, FALSE, 0, 0);
        }

        return status;
    }

    startTime = sal_time_msec();
    if(TimeOut == 0) {
        timeOut = CONN_DEFAULT_IO_TIMEOUT;
    } else {
        timeOut = TimeOut;
    }

    for(idx_trial = 0;
        sal_tick_sub(sal_time_msec(),  startTime + timeOut) <= 0;
        idx_trial++) {

        debug_lock(4, "Trial #%u", idx_trial);

        //
        // Increase the buffer lock request count to promote the lock holder
        // to release it.
        //

        status = QueueBufferLockRequest(BuffLockCtl, conn, LockInfo, 0);
        if(status == NDAS_ERROR_ACQUIRE_LOCK_FAILED) {
            debug_lock(1, "Extension lock denied. retry. STATUS=%08lx", (xulong)status);
            continue;
        }

        if(!NDAS_SUCCESS(status)) {

            sal_assert( status == NDAS_ERROR_NETWORK_FAIL);
            debug_lock(1, "SetBufferLockIntention() failed. STATUS=%08lx", (xulong) status);

            break;
        }

        //
        // Wait for the buffer lock to be released.
        //

        lockAcquired = FALSE;
        status = WaitForBufferLockRelease(BuffLockCtl, conn, (xuchar*)&lockCount, &lockAcquired);
        if(!NDAS_SUCCESS(status)) {
            debug_lock(1, "WaitForBufferLockRelease() failed. STATUS=%08lx", (xulong)status);
            //
            // Somebody hold the lock too long or hang.
            // Go to the higher priority and retry to queue buffer lock request.
            // No need to delay. Delay has already occurred in WaitForBufferLockRelease().
            //
            if(status == NDAS_ERROR_TIME_OUT) {
                if(BuffLockCtl->Priority > 0) {
                    BuffLockCtl->Priority --;
                    debug_lock(1, "Somebody hold the lock too long or hang."
                        " Go to the higher priority=%u. STATUS=%08lx", BuffLockCtl->Priority, (xulong)status);
                } else {
                    debug_lock(1, "Could not acquire the buffer lock at the highest priority."
                        "priority=%u. STATUS=%08lx", BuffLockCtl->Priority, (xulong)status);
                }
            }
            continue;
        }
#ifdef DEBUG
        debug_lock(4, "Priority=%u", BuffLockCtl->Priority);
#endif

        if(lockAcquired == FALSE) {
            //
            // Lock release detected.
            // Wait for the priority and send the lock request.
            //
            WaitForPriority(BuffLockCtl, BuffLockCtl->Priority);
            status = OpAcquireDevLock(
                conn,
                DevLockIdMap[conn->unit][NDAS_DEVLOCK_ID_BUFFLOCK],
                (xuchar*)&lockCount,
                TimeOut,
                FALSE);
        }
        if(NDAS_SUCCESS(status)) {

            debug_lock(1, "Trial #%u: Acquired. LockCnt=%u Priority=%u", idx_trial, lockCount, BuffLockCtl->Priority);

            LockCacheSetDevLockAcquisition(LockInfo, NDAS_DEVLOCK_ID_BUFFLOCK, FALSE, 0, 0);
            // Set return lock data.
            if(LockData)
                *(xulong*)LockData = lockCount;

            //
            // Reset acquisition expire time and bytes.
            //

            NdasResetAcqTimeAndAccIOBytes(BuffLockCtl);
            
            //
            // Exit loop with success status
            //
            break;
        } else {
            debug_lock(1, "LurnIdeDiskAcquireDevLock() failed. STATUS=%08x", (xuint32) status);
            //
            // Assume that collision occurs.
            // Go to the higher priority
            //
            if(BuffLockCtl->Priority > 0) {
                BuffLockCtl->Priority --;
            }
        }

        status = NDAS_ERROR_TIME_OUT;
    }

    return status;
}

static
INLINE
xuint32
LMGetPriority(
    IN xuint32    ConnectedHosts,
    IN xuint32    LockCountWhenReleased,
    IN xuint32    CurrentLockCount,
    IN xuint32    LastOthersRequests
){
    xuint32 diff;
    xuint32 pendingRequestPerLockAcq;
    xuint32    priority;

    diff = CurrentLockCount - LockCountWhenReleased;
    if(diff < 1) {
        // Minimum diff is 1 to prevent divide-by-zero fault.
        diff = 1;
    }

    // Calculate the pending request per lock acquisition with round-up.
    pendingRequestPerLockAcq = (LastOthersRequests + diff/2) /  diff;

    // Translate the pending request per lock acquisition to the priority.
    priority = pendingRequestPerLockAcq;
#if 0
    if(pendingRequestPerLockAcq > 1) {
        priority = pendingRequestPerLockAcq - 1;
    } else {
        priority = 0;
    }
#endif

    // Check the priority is in valid range.
    if(priority >= ConnectedHosts) {
        debug_lock(1, "Too big priority %u %u %u",
            priority, LastOthersRequests, diff);
        priority = ConnectedHosts - 1;
    }
#ifdef DEBUG
    debug_lock(4, "Priority=%u. pending req=%u diff=%u",
        priority,
        LastOthersRequests,
        diff
        );
#endif

    return priority;
}

//
// Release the NDAS buffer lock
//

ndas_error_t
NdasReleaseBufferLock(
    IN PBUFFLOCK_CONTROL BuffLockCtl,
    IN uconn_t*    conn,
    IN NDAS_DEVLOCK_INFO* LockInfo,
    OUT xuchar*            LockData,
    IN sal_msec   TimeOut,
    IN xbool            Force,
    IN xuint32            TransferredIoLength
){
    ndas_error_t    status;
    xbool        release;
    sal_msec    currentTime;

    if(conn->unit >= MAX_TARGET_ID)
        return NDAS_ERROR_INVALID_PARAMETER;
    if(BuffLockCtl->BufferLockParent) {
        return NDAS_OK;
    }

    // Accumulate the IO length
    //

    BuffLockCtl->AccumulatedIOBytes += TransferredIoLength;

    //
    // Clear lost status
    //
    if(LockCacheIsLost(LockInfo, NDAS_DEVLOCK_ID_BUFFLOCK)) {
        LockCacheClearDevLockLoss(LockInfo, NDAS_DEVLOCK_ID_BUFFLOCK);
    }

    if(LockCacheIsAcquired(LockInfo, NDAS_DEVLOCK_ID_BUFFLOCK) == FALSE) {
        debug_lock(1, "The buffer lock already released.");
        return NDAS_OK;
    }

    //
    // Check release lock request condition.
    //
    currentTime = sal_time_msec();
    if(BuffLockCtl->BufferLockConrol == FALSE) {
        // Release the buffer lock right away when BufferLock Control is off.
        release = TRUE;
    } else if(Force) {
        release = TRUE;
        debug_lock(2, "Force option on.  Acc=%llu Elap=%ld Diff=%ld",
            BuffLockCtl->AccumulatedIOBytes,
            (xulong)(currentTime - (BuffLockCtl->AcquisitionExpireTime - BuffLockCtl->AcquisitionMaxTime)),
            (xulong)(currentTime - BuffLockCtl->AcquisitionExpireTime)
            );
    } else if(BuffLockCtl->AcquisitionExpireTime <= currentTime) {
        release = TRUE;
        debug_lock(2, "Time over. Acc=%llu Elap=%ld Diff=%ld", 
            BuffLockCtl->AccumulatedIOBytes,
            (xulong)(currentTime - (BuffLockCtl->AcquisitionExpireTime - BuffLockCtl->AcquisitionMaxTime)),
            (xulong)(currentTime - BuffLockCtl->AcquisitionExpireTime)
            );
    } else if(BuffLockCtl->AccumulatedIOBytes >= BuffLockCtl->MaxIOBytes) {
        release = TRUE;
        debug_lock(2, "Bytes over. Acc=%llu Elap=%ld Diff=%ld", 
            BuffLockCtl->AccumulatedIOBytes,
            (xulong)(currentTime - (BuffLockCtl->AcquisitionExpireTime - BuffLockCtl->AcquisitionMaxTime)),
            (xulong)(currentTime - BuffLockCtl->AcquisitionExpireTime)
            );
    } else {
        release = FALSE;
    }

    if(release) {
        xuint32    lockCount;

        if(BuffLockCtl->BufferLockConrol) {
            xulong    pendingRequests = 0, othersPendingRequest;
            xulong    requestCount = 0;
            xulong    rwHosts = 0, roHosts = 0;

            status = GetBufferLockPendingRequestsWithHostInfo(
                BuffLockCtl,
                conn,
                &requestCount,
                &pendingRequests,
                &rwHosts,
                &roHosts,
                TimeOut);
            if(!NDAS_SUCCESS(status)) {
                debug_lock(1, "GetBufferLockPendingRequestsWithHostInfo() failed."
                    " STATUS=%08lx", (xulong) status);
                return status;
            }

            if(pendingRequests > BuffLockCtl->MyRequestCount) {
                othersPendingRequest = pendingRequests - BuffLockCtl->MyRequestCount;
            }
            else {
                othersPendingRequest = 0;
            }
#ifdef DEBUG
            if(othersPendingRequest) {
                debug_lock(1, "pending lock request = %lu my=%d", pendingRequests, BuffLockCtl->MyRequestCount);
            }
#endif
            // Update the host counters

            BuffLockCtl->ConnectedHosts = rwHosts + roHosts; // including myself

            //
            // Estimate the priority using the pending request count.
            // Reset the priority to the end of the request queue.
            //
            BuffLockCtl->Priority = LMGetPriority(
                BuffLockCtl->ConnectedHosts,
                BuffLockCtl->LockCountWhenReleased,
                BuffLockCtl->CurrentLockCount,
                othersPendingRequest);

            // Reset acquisition expire time and accumulated IO length
            // to start another acquisition period.
            // Reset request counts
            // Reset the LockCount
            BuffLockCtl->AcquisitionExpireTime = currentTime + BuffLockCtl->AcquisitionMaxTime;
            BuffLockCtl->AccumulatedIOBytes = 0;
            BuffLockCtl->RequestCountWhenReleased = requestCount;
            BuffLockCtl->MyRequestCount = 0;
            BuffLockCtl->LockCountWhenReleased = BuffLockCtl->CurrentLockCount;


            // If nobody wants the buffer lock, release the buffer lock later.
            if(othersPendingRequest == 0 && Force == FALSE) {
                debug_lock(4, "Nobody wants the buffer lock. Release the buffer lock later.");
                return NDAS_OK;
            }

        }

        // Send buffer lock release request.
        status = OpReleaseDevLock(
            conn,
            DevLockIdMap[conn->unit][NDAS_DEVLOCK_ID_BUFFLOCK],
            (xuchar*)&lockCount,
            TimeOut);
        if(NDAS_SUCCESS(status)) {
            if(LockData)
                *(xulong*)LockData = lockCount;

            debug_lock(1, "Buffer lock released. %u", lockCount);

            //
            // Release success
            // Update the lock status only if previously we acquired the lock.
            //

            LockCacheSetDevLockRelease(LockInfo, NDAS_DEVLOCK_ID_BUFFLOCK);

        } else {
            debug_lock(1, "Buffer lock release failed 0x%x", status);
        }
    } else {
        /* Don't need to release actual lock right now */
        status = NDAS_OK;
    }
    return status;
}

//////////////////////////////////////////////////////////////////////////
//
// Idle timer management
//

int IoIdleTimeExpiredHandler(void * param1, void *param2)
{
    udev_t* udev = (udev_t* ) param1;
    udev_io_idle_operation(udev);
    return 0;
}


/* Run in ndiod thread context */
ndas_error_t
StartIoIdleTimer(
    PBUFFLOCK_CONTROL BuffLockCtl,
    udev_t* udev
) {
    int ret;

    if (BuffLockCtl->BufferLockConrol == FALSE) {
        return NDAS_OK;
    }
    if(BuffLockCtl->IoIdleTimerExpire == DPC_INVALID_ID) {
        BuffLockCtl->IoIdleTimerExpire = dpc_create(
            DPC_PRIO_NORMAL,
            IoIdleTimeExpiredHandler,
            (void*)udev,
            NULL,
            NULL,
            DPCFLAG_DO_NOT_FREE);
        if(BuffLockCtl->IoIdleTimerExpire == DPC_INVALID_ID) {
            return NDAS_ERROR_OUT_OF_MEMORY;
        }
    }
//      BuffLockCtl->IoIdleTimerExpire can be not DPC_INVALID_ID, if it is expired.
//    sal_assert(BuffLockCtl->IoIdleTimerExpire == DPC_INVALID_ID);
    ret = dpc_queue(
        BuffLockCtl->IoIdleTimerExpire, BuffLockCtl->IoIdleTimeOut);

    return ret;
}

/* Run in ndiod thread context */
ndas_error_t
StopIoIdleTimer(
    PBUFFLOCK_CONTROL BuffLockCtl
) {
    if(BuffLockCtl->BufferLockConrol == FALSE) {
        return NDAS_OK;
    }

    // Clear IO idle state.
    BuffLockCtl->IoIdle = FALSE;

    dpc_cancel(BuffLockCtl->IoIdleTimerExpire);

    return NDAS_OK;
}

/* Called by ndiod thread when it received io idle event */
ndas_error_t
EnterBufferLockIoIdle(
    IN PBUFFLOCK_CONTROL BuffLockCtl,
    IN uconn_t*    conn,
    IN NDAS_DEVLOCK_INFO* LockInfo
){
    ndas_error_t    status;

    debug_lock(1, "Enter buffer lock IO idle.");

    if(BuffLockCtl->BufferLockConrol == FALSE) {
        return NDAS_OK;
    }

    // Release the buffer lock
    status = NdasReleaseBufferLock(BuffLockCtl, conn, LockInfo
        , NULL, 0, TRUE, 0);
    if(!NDAS_SUCCESS(status)) {
        debug_lock(1, 
            " ReleaseNdasBufferLock() failed. STATUS=%08x.",
            (int)status);
    }

    // Indicate IO idle state.
    BuffLockCtl->IoIdle = TRUE;

    return status;
}

//////////////////////////////////////////////////////////////////////////
//
// Initialization
//

#define LOCKMGMT_QUANTUM                    (16)        // 15.625 ms ( 1/64 second )
#define LOCKMGMT_MAX_ACQUISITION_TIME        (8 * LOCKMGMT_QUANTUM)    // 125 ms ( 1/8 second )
#define LOCKMGMT_MAX_ACQUISITION_BYTES        (8 * 1024 * 1024)        // 8 MBytes
#define LOCKMGMT_IOIDLE_TIMEOUT                (LOCKMGMT_MAX_ACQUISITION_TIME / 4)    // 31.25 ms
#define LOCKMGMT_PRIORITYWAITTIME            (LOCKMGMT_QUANTUM * 2)

ndas_error_t
LMInitialize(
    IN uconn_t* conn,
    IN PBUFFLOCK_CONTROL    BuffLockCtl,
    IN xbool                InitialState
){

    sal_memset(BuffLockCtl, 0, sizeof(BUFFLOCK_CONTROL));

    debug_lock(1, "LOCKMGMT_QUANTUM = %u", LOCKMGMT_QUANTUM);

    BuffLockCtl->Quantum = LOCKMGMT_QUANTUM;
    BuffLockCtl->PriorityWaitTime = LOCKMGMT_PRIORITYWAITTIME;
    BuffLockCtl->AcquisitionMaxTime = LOCKMGMT_MAX_ACQUISITION_TIME;
    BuffLockCtl->AcquisitionExpireTime = 0;
    BuffLockCtl->MaxIOBytes = LOCKMGMT_MAX_ACQUISITION_BYTES;
    BuffLockCtl->IoIdleTimeOut = LOCKMGMT_IOIDLE_TIMEOUT;
    BuffLockCtl->IoIdle = TRUE;
    BuffLockCtl->ConnectedHosts = 1; // add myself.
    BuffLockCtl->CurrentLockCount = 0;
    BuffLockCtl->RequestCountWhenReleased = 0;
    BuffLockCtl->Priority = 0;
    BuffLockCtl->IoIdleTimerExpire = DPC_INVALID_ID;
    BuffLockCtl->AccumulatedIOBytes = 0;
    BuffLockCtl->BufferLockConrol = InitialState;

#if defined(__NDAS_SCSI_DISABLE_LOCK_RELEASE_DELAY__)
    if (LURN_IS_ROOT_NODE(Lurn)) {
        BuffLockCtl->BufferLockParent = FALSE;    
    } else {
        BuffLockCtl->BufferLockParent = TRUE;
        // Must be off when the parent controls the buffer lock.
        BuffLockCtl->BufferLockConrol = FALSE;
    }
#else
    BuffLockCtl->BufferLockParent = FALSE;
#endif

#ifdef DEBUG
    if(BuffLockCtl->BufferLockParent)
        sal_assert(BuffLockCtl->BufferLockConrol == FALSE);
    debug_lock(0, "Buffer lock control:%x Controlled by parent:%x",
        BuffLockCtl->BufferLockConrol,
        BuffLockCtl->BufferLockParent);
#endif

    return NDAS_OK;
}

void
LMEnable(
    IN PBUFFLOCK_CONTROL    BuffLockCtl,
    IN xbool                Activate,
    IN xbool                ParentControl
) {
#ifdef DEBUG
    if(ParentControl) sal_assert(Activate == FALSE);
#endif
    BuffLockCtl->BufferLockParent = Activate;
    // Must be off when the parent controls the buffer lock.
    BuffLockCtl->BufferLockConrol = ParentControl;
}


ndas_error_t
LMDestroy(
    IN PBUFFLOCK_CONTROL BuffLockCtl
){
    if (BuffLockCtl->IoIdleTimerExpire != DPC_INVALID_ID) {
        dpc_cancel(BuffLockCtl->IoIdleTimerExpire);
        dpc_destroy(BuffLockCtl->IoIdleTimerExpire);
        BuffLockCtl->IoIdleTimerExpire = DPC_INVALID_ID;
    }
    return NDAS_OK;
}

#endif

