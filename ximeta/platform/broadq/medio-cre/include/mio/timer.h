/*
 * timer.h - yup
 * $Id: timer.h,v 1.1 2002/10/20 07:00:02 akieschnick Exp $
 */

#ifndef TIMER_H
#define TIMER_H

void TimerInit();
void TimerStop();
void TimerPause();
void TimerUnpause();
long GetTicks();

#endif // TIMER_H
