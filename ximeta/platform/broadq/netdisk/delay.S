
#include <gdb/r5900_regs.h>

	.global delay
	.ent	delay
delay:
	subu	sp,sp,32
	sw		fp,0(sp)
	sw		ra,4(sp)
	jal		DelayThread
	lw		fp,0(sp)
	lw		ra,4(sp)
	jr		ra
	.end	delay
