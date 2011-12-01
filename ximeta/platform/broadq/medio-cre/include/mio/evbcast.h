
#ifndef __MIO_EVENT_BROADCAST_H__
#define __MIO_EVENT_BROADCAST_H__
#include <stdarg.h>
#include <mio/evsink.h>
#include <mio/mlist.h>
using namespace std;

class MioEventBroadcaster : public MioEventSink {
public:
    MioEventBroadcaster();
    virtual ~MioEventBroadcaster();

    virtual void accept(MioEvent& event, const char *format, va_list args);

    void attach(MioEventSink& sink);
    void detach(MioEventSink& sink);

protected:
    mlist<MioEventSink*> mySinks;
};

#endif // __MIO_EVENT_BROADCAST_H__
