
#ifndef __MIO_EVENT_LOG_H__
#define __MIO_EVENT_LOG_H__
#include <mio/evsink.h>
#include <mio/msgfacet.h>
#include <mio/sstring.h>
using namespace std;

class MioEventLog {
public:
    MioEventLog();
    virtual ~MioEventLog();

    void send(MioEvent& event, const sstring& id, const char *format, ...);

    static void setLogEvents(MioEventSink& logEvents);

protected:
    static MioEventSink *ourLogEvents;
};

extern MioEventLog allLogEvents;

#endif // __MIO_EVENT_LOG_H__
