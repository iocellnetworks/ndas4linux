
#ifndef __MIO_DEVICE_EVENTS_H__
#define __MIO_DEVICE_EVENTS_H__
#include <_ansi.h>

#define MIO_MAX_NEW_DEVICE_EVENTS 10
#define MIO_DEVICENAME_MAX 20

typedef char MioDeviceName[MIO_DEVICENAME_MAX+1];

typedef enum { // events concerning local device state changes
    MIO_EV_LOCAL_DEVICE_ADDED,
    MIO_EV_LOCAL_DEVICE_REMOVED,
} MioLocalDeviceChangeKind;

typedef struct _MioDeviceChangeEvent { // contents of a specific event
    MioLocalDeviceChangeKind why;
    MioDeviceName device;
} MioDeviceChangeEvent;

typedef struct _MioDeviceEventList { // all (unprocessed) device state change events
    int count;
    MioDeviceChangeEvent events[MIO_MAX_NEW_DEVICE_EVENTS];
} MioDeviceEventList;

_BEGIN_STD_C
extern void deviceEventsInit();
extern int deviceEventsRead(void *obuf, int count);
extern void notifyDeviceAvailable(const char *name);
extern void notifyDeviceRemoval(const char *name);
_END_STD_C

#endif // __MIO_DEVICE_EVENTS_H__
