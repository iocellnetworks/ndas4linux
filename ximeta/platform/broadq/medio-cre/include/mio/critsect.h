
#ifndef __MIO_CRITICAL_SECTION_H__
#define __MIO_CRITICAL_SECTION_H__
#include <mio/mutex.h>

class MioCriticalSection : public MioMutex {
public:
    MioCriticalSection() { MioMutexInit(this); };
    void lock() { MioMutexLock(this); };
    void unlock() { MioMutexUnlock(this); };
    ~MioCriticalSection() {  MioMutexTerm(this); };
};

#endif // __MIO_CRITICAL_SECTION_H__
