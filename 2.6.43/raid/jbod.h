#ifndef _RAID_JBOD_H_
#define _RAID_JBOD_H_
#include <ndasuser/ndaserr.h>
#include "netdisk/sdev.h"
#include "netdisk/ndev.h"

struct jbod_io_s {
    sal_atomic count;
    int nr_io;
    ndas_error_t err;
    urb_ptr orgtir;
    logunit_t *sdev;
};

struct jbod_io_s * jbod_io_alloc();

void jbod_io_free(struct jbod_io_s * jio);

int jbod_op_status(logunit_t *sdev);

void jbod_op_enable(logunit_t *sdev, int flags);

void jbod_op_disable(logunit_t *sdev, ndas_error_t err);

void jbod_end_io(struct jbod_io_s *jio);

NDAS_CALL void jbod_has_ioed(int slot, ndas_error_t err,void* user_arg);

#endif /*_RAID_JBOD_H_ */

