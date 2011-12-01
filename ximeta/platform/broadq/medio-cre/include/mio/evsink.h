
#ifndef __MIO_EVENT_SINK_H__
#define __MIO_EVENT_SINK_H__
#include <stdarg.h>
#include <mio/sstring.h>
#include <mio/events.h>
using namespace std;

class MioEventSink {
public:
    MioEventSink();
    virtual ~MioEventSink();

    void send(MioEvent& event, const sstring& id, const char *format, ...);
    virtual void accept(MioEvent& event, const char *format, va_list args) = 0;
};

#endif // __MIO_EVENT_SINK_H__
