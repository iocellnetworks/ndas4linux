
#ifndef __SLEEP_QUEUE_H__
#define __SLEEP_QUEUE_H__
#include <_ansi.h>
#include <tamtypes.h>
#include <mio/slist.h>
#include <mio/mutex.h>

#if defined(__cplusplus)

class MioSleepQueueEntry : public MioSingleLink {
public:
    MioSleepQueueEntry(int ms = 0, int threadID = 0);
    ~MioSleepQueueEntry();
    const char *asString() const;

protected:
    MioSleepQueueEntry *next() const;
    friend class MioSleepQueue;
    u64 myExpiry;
    int myThreadID;
};

class MioSleepQueue {
public:
    MioSleepQueue(int maxQueueSize);
    virtual ~MioSleepQueue();

    void insert(int delayMS);
    void run(u64 now);
    void iRun(u64 now);
    const char *asString() const;

protected:
    static MioSleepQueueEntry *head(const MioSingleLinkedList& list);
    MioMutex myLock;
    MioSingleLinkedList myUsed;
    MioSingleLinkedList myFree;

private:
    void insert(MioSleepQueueEntry *item, int nolock = 0);
};

#endif

_BEGIN_STD_C
_EXFUN(void mio_SleepThread, (int delayMS));
_END_STD_C

#endif // __SLEEP_QUEUE_H__
