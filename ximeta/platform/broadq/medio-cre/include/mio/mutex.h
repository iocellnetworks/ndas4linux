
#ifndef __MIO_MUTEX_H__
#define __MIO_MUTEX_H__
#include <_ansi.h>

#if defined(__cplusplus)
class MioMutex {
public:
    MioMutex();
    ~MioMutex();

    void init();
    void term();

    void lock();
    void unlock();

    int isLocked() const;
    int isLockedInt() const;

    void print() const;

private:
    int id;
};

#else

typedef struct _MioMutex {
    int id;
} MioMutex;
#endif

_BEGIN_STD_C
extern void    MioMutexInit(MioMutex *self);
extern void    MioMutexTerm(MioMutex *self);
extern void MioMutexLock(MioMutex *self);
extern void MioMutexPrint(const MioMutex *self);
extern int    MioMutexIsLocked(const MioMutex *self);
extern int    MioMutexIsLockedInt(const MioMutex *self);
extern void    MioMutexUnlock(MioMutex *self);
_END_STD_C

#endif // __MIO_MUTEX_H__
