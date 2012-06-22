#include <linux/version.h>
#include <linux/module.h> 

#include <ndas_errno.h>
#include <ndas_ver_compat.h>
#include <ndas_debug.h>

#include     "crc.h"

static __u32 v_CRC_inited = false;
static __u32 lookup_table[256];

void CRC_init_table(__u32 *table)
{
    __u32    ulPolynomial = 0x04C11DB7;
    int                i,j;

    for (i = 0; i <= 0xFF; i++)
    {
        table[i] = CRC_reflect(i, 8) << 24;
        for (j = 0; j < 8; j++)
            table[i] = (table[i] << 1) ^ (table[i] & (1 << 31) ? ulPolynomial : 0);
        table[i] = CRC_reflect(table[i], 32);
    }
}


__u32 CRC_reflect(__u32 ref, char ch)
{
    __u32    value = 0;
    int                i;

    // Swap bit 0 for bit 7
    // bit 1 for bit 6, etc.
    for(i = 1; i < (ch + 1); i++)
    {
        if (ref & 1)
            value |= 1 << (ch - i);
        ref >>= 1;
    }
    return value;
}

__u32 CRC_calculate(unsigned char *buffer, __u32 size)
{
    __u32    CRC = 0xffffffff;
    if (!v_CRC_inited) {
        CRC_init_table(lookup_table);
        v_CRC_inited = true;
    }

        while (size--) {
//        printf("%10u %10lu : %10lu\n", *buffer, (CRC & 0xFF) ^ *buffer, CRC);
        CRC = (CRC >> 8) ^ lookup_table[(CRC & 0xFF) ^ *buffer++];
    }
//    printf("\n");
//    printf("%lu\n", CRC);
    return(CRC);
}

