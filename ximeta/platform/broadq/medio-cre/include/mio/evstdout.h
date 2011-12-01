
#ifndef __MIO_EVENT_STDOUT_H__
#define __MIO_EVENT_STDOUT_H__
#include <mio/evsink.h>
#include <locale>
using namespace std;

typedef int (*MioStdoutWriter)(void *cookie, const unsigned char *msg, int length);

class MioEventStdout : public MioEventSink {
public:
    MioEventStdout(const locale& locale, MioStdoutWriter writer, void *cookie);
    virtual ~MioEventStdout();

    virtual void accept(MioEvent& event, const char *format, va_list args);

protected:
    const locale& myLocale;
    MioStdoutWriter myWriter;
    void *myWriterCookie;
};

#endif // __MIO_EVENT_STDOUT_H__
