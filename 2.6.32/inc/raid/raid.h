#ifndef _RAID_RAID_H_
#define _RAID_RAID_H_

#include <ndasuser/bind.h>
#include "netdisk/list.h"
#include "netdisk/ndasdib.h"
#include "netdisk/sdev.h"

#define U64_ABS(x,y) (((x)>(y))?((x)-(y)):((y)-(x)))

/*
 event : NDAS_STATUS_ENABLE, NDAS_STATUS_DISABLE
 */
#define RAID_CMD_INIT_OR_RETURN_IF_CMD_UNDERGOING(sdev, event)
    



/* Arguments for connecting functions of raid */
struct raid_struct {

    void* private;
    
    /* 1 if a command is under going protected by op_mutex, */
    int op_undergo;
    /* # of ops count to be completed */
    sal_atomic op_count;
    sal_spinlock op_mutex;
    ndas_error_t op_err;
    int op_writable;
    int op_writeshared;
    void *user_arg;
};
#ifdef XPLAT_RAID
inline 
static 
struct raid_struct* SDEV_RAID(logunit_t *sdev)
{
    return (struct raid_struct*) sdev->private;
}
#endif

inline 
static 
ndas_raid_ptr SDEV_RAID_INFO(logunit_t *sdev)
{
    return (ndas_raid_ptr) sdev->info2;
}

inline
static 
int raid_find_index(logunit_t *sdev, int slot) 
{
    int i = 0;
    for ( i = 0; i < SDEV_RAID_INFO(sdev)->disks; i++)
        if ( sdev->disks[i] && sdev->disks[i]->info.slot == slot )
            return i;
    return -1;
}    

static inline int raid_next(logunit_t *sdev, int index) 
{
    if ( ++index >= SDEV_RAID_INFO(sdev)->disks ) index = 0;
    return index;
}

static inline int raid_prev(logunit_t *sdev, int index) 
{
    if ( index-- == 0 ) index = SDEV_RAID_INFO(sdev)->disks - 1;
    return index;
}

ndas_error_t raid_create(logunit_t *sdev, int disks, int *slots) ;
    
void raid_cleanup(logunit_t *bdev);

xbool dib_compare(NDAS_DIB_V2 *dib1, NDAS_DIB_V2 *dib2);
/* 
 * TRUE if the rmd2 is newer than the current rmd (sdev->nmd->rmd)
 */
xbool sdev_rmd_is_newer(logunit_t *sdev, NDAS_RAID_META_DATA *rmd2);

struct raid_struct* raid_struct_alloc(void);
void raid_struct_free(struct raid_struct* r5e);

void raid_disabled_on_failing_to_enable(int slot, ndas_error_t err, void *arg);

void raid_stop_disks(logunit_t *sdev, ndas_error_t err, xbool notify_enable, ndas_io_done disabled_func);

#endif /* _RAID_RAID_H_ */
