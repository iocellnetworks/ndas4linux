#ifndef        _CRC_
#define        _CRC_

void	CRC_init_table(__u32 *table);
__u32	CRC_reflect(__u32 ref, char ch);
__u32	CRC_calculate(unsigned char *buffer, __u32 size);

#endif
