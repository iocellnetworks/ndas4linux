#ifndef _RAID_REDUNDANCY_H_
#define _RAID_REDUNDANCY_H_

#include <sal/sal.h>
#include <sal/types.h>
#include <sal/mem.h>

#include <ndasuser/ndaserr.h>

#include "raid/raid.h"

struct rddc_disk_s {
    xuint64 header;
    /* 1 if the data is not uptodated*/
    int defective;
    /* 1 if the disk is spare */
    int spare;
};

struct rddc_recovery {
    int nr_blocks;
    struct sal_mem_block *blocks;

    int is_locked;
    /* start sector number that is being recovered */
    xuint64 start;
    /* # of sector number that is being recovered */
    xuint32 nr_sectors;
    /* # of the read io that has completed/notify the result */
    struct list_head pending;
    sal_atomic count;
    urb_ptr tir[0];
};

struct rddc_s {
    /* # of disks that is running */
    sal_atomic running;
    int nr_spare;
    /* 1 if the hot recovery is needed */
    int recovery;
    int sectors_per_bit_shift;
    sal_spinlock mutex;
    ndas_permission_request_handler perm_fn;
    /* disk index that is defective */
    int defect;
    /* disk index that should be recovered */
    int target;
    /* # of I/O queued in the raid slot to adjust the speed of recovery */
    sal_atomic nr_queued_io;
    /* */
    struct rddc_recovery *rcvr;
    
    dpc_func            rcvr_main;
    ndas_io_done        rcvr_has_read;
    
    struct rddc_disk_s disks[0];
};

inline static
void memxor(char *dst, char *src, int sz) 
{
    int *d = (int *)dst, *s = (int *)src;
    sal_assert((sz / sizeof(int)) * sizeof(int) == sz);
    sz /= sizeof(int);
    
    while(--sz>=0) {
        d[sz] ^= s[sz];
    }    
}

#define rddc_is_locked(sdev) (rddc_conf(sdev)->rcvr && rddc_conf(sdev)->rcvr->is_locked)

struct rddc_s * rddc_alloc(int disks);

void rddc_free(struct rddc_s *ret);


static inline
struct rddc_s *rddc_conf(logunit_t *sdev)
{
    sal_assert(sdev);
    sal_assert(sdev->private);
    return (struct rddc_s *) SDEV_RAID(sdev)->private;
}
static inline
struct rddc_disk_s *rddc_disk(logunit_t *sdev,int index)
{
    return &rddc_conf(sdev)->disks[index];
}

ndas_error_t rddc_member_disable(logunit_t *sdev, int index);
    
void rddc_op_enable(logunit_t *sdev, int flag);

void rddc_op_disable(logunit_t *sdev, ndas_error_t err);

void rddc_device_changed(logunit_t *sdev, int slot, int online); 

/* 
 * Mark the bitmap in memory
 * req has the sector number for virtual raid system (not for the pysical disk)
 */
void rddc_mark_bitmap(logunit_t *sdev, ndas_io_request_ptr req, int shift);

#endif /* _RAID_REDUNDANCY_H_ */
