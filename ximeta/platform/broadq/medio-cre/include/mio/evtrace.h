
#ifndef __MIO_EVENT_TRACE_H__
#define __MIO_EVENT_TRACE_H__
#include <mio/evsink.h>
#include <mio/trace.h>
#include <locale>
using namespace std;

class MioEventTrace : public MioEventSink {
public:
    MioEventTrace(TracePackage& trace, const locale& locale);
    virtual ~MioEventTrace();

    virtual void accept(MioEvent& event, const char *format, va_list args);

protected:
    const locale& myLocale;
    TracePackage& myTrace;
};

#endif // __MIO_EVENT_TRACE_H__
