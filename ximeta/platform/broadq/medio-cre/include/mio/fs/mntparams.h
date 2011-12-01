
#ifndef __MIO_FS_MOUNT_PARAMS_H__
#define __MIO_FS_MOUNT_PARAMS_H__
#include <tamtypes.h>
#include <mio/sstring.h>

struct mountParams {
    sstring rawDeviceName;
    sstring mountPointName;
    u64 lba;
};

#endif // __MIO_FS_MOUNT_PARAMS_H__
