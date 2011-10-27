#ifndef _NDASUSER_BIND_H_
#define _NDASUSER_BIND_H_

#include <sal/types.h>
#include <ndasuser/ndaserr.h>
#include <ndasuser/def.h>
/*
 * The raid related infomation of the slot.
 */
typedef struct _ndas_raid_info {
    int mode;
    int disks;
    /* minimum number of disks to be operatable*/
    int min_disks;

    int slots[0];
} ndas_raid_info, *ndas_raid_ptr;

/*
 * Query the bind information of the slot.
 * If info is not enough size to get the infomation, it returns without slot infomation.
 */
NDASUSER_API
ndas_error_t ndas_query_raid(int bind_slot, ndas_raid_ptr info, int size);

/* 
 * Create a new NDAS bind with the given disks
 * The disks will not be accessed as a single disk
 */
NDASUSER_API
ndas_error_t ndas_bind(int type, int nr_disks, int *slots);


/* 
 * Add Spare Disks
 */
NDASUSER_API
ndas_error_t ndas_add_spares(int bind_slot, int nr_disks, int *slots);


/* 
 * Remove Spare Disks
 */
NDASUSER_API
ndas_error_t ndas_remove_spare(int bind_slot, int spare_slot);

/* 
 *
 */
NDASUSER_API 
ndas_error_t ndas_remove_spares(int bind_slot);


/*
 * Get the infomation
 */
//ndas_error_t ndas_query_bind(int bind_slot, ndas_bind_ptr info);


/* 
 * Destroy NDAS bind
 * The data in the disks will be destroyed.
 */
NDASUSER_API 
ndas_error_t ndas_unbind(int bind_slot);


#endif // _NDASUSER_BIND_H_

