#ifndef        _CRC_
#define        _CRC_

#include "sal/types.h"

void            CRC_init_table(xuint32 *table);
xuint32    CRC_reflect(xuint32 ref, char ch);
xuint32    CRC_calculate(unsigned char *buffer, xuint32 size);

#endif
