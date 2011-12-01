
#ifndef __MIO_EVENTS_H__
#define __MIO_EVENTS_H__
#include <mio/sstring.h>
using namespace std;
#include <tamtypes.h>
#include <mio/modules.h>


enum MioEventLevel {
    MIO_EVENT_INFORMATIVE,
    MIO_EVENT_UNEXPECTED,
    MIO_EVENT_CATASTROPHIC,
    MIO_EVENT_MAX
};

class MioEvent {
public:
    MioEvent(const sstring& moduleName, MioEventLevel level);
    MioEvent(const MioModule& module, MioEventLevel level);
    virtual ~MioEvent();

    void print() const;

    MioEventLevel myLevel;
    sstring myModule;
    u64 myTimestamp;
    sstring myId;
    sstring myBody;
};

#endif // __MIO_EVENTS_H__
