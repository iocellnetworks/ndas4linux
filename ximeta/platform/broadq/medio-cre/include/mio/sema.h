
#ifndef __MIO_SEMA_H__
#define __MIO_SEMA_H__
#include <_ansi.h>


_BEGIN_STD_C
extern int    MioSemaInit(int *semaID, int initValue);
extern void    MioSemaTerm(int *semaID);
extern int  MioSemaGetCount(int semaID);
extern int  MioSemaGetCountInt(int semaID);
extern void MioSemaWait(int semaID);
extern void MioSemaPrint(int semaID);
extern void    MioSemaSignal(int semaID);
extern void    MioSemaSignalInt(int semaID);
_END_STD_C

#endif // __MIO_SEMA_H__
