
#ifndef __MIO_RMS_H__
#define __MIO_RMS_H__
#include <_ansi.h>
#include <mio/devevents.h>

typedef void (*MioRMSNewPartitionHandler)(const MioDeviceName deviceName, const char *partitionName);

#define MAX_NEW_DEVICES 10
#define MAX_DEVICE_NAME_LENGTH 20

_BEGIN_STD_C
extern void MioRMSCheckNewDevices(int fd, MioRMSNewPartitionHandler handler);
_END_STD_C

#endif // __MIO_RMS_H__
