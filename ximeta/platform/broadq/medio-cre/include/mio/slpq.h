
#ifndef __SLEEP_QUEUE_H__
#define __SLEEP_QUEUE_H__
#include <tamtypes.h>
#include <slist.h>
#include <mutex.h>

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

class MioSleepQueue : public MioSingleLinkedList {
public:
    MioSleepQueue(int maxQueueSize);
    virtual ~MioSleepQueue();

    void insert(int delayMS);
    void run(u64 now);
    void iRun(u64 now);
    virtual void insert(MioSleepQueueEntry *item, int nolock = 0);
    const char *asString() const;

protected:
    MioSleepQueueEntry *head() const;
    MioMutex myLock;
    MioSleepQueueEntry *myFree;
};

#endif // __SLEEP_QUEUE_H__
