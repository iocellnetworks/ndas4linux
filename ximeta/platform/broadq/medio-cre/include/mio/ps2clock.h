
#ifndef __MIO_PS2_CLOCK_H__
#define __MIO_PS2_CLOCK_H__
#include <rw/rwtime.h>

class MioPs2Clock {
public:
    static void initialize();
    static RWTime getTimeOfDay();

private:
    MioPs2Clock();
};

#endif // __MIO_PS2_CLOCK_H__
