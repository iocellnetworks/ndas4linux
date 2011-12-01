.MACRO STUB NAME,ID
        .globl   \NAME
        \NAME:
        j       $31
        li      $0,\ID
.ENDM



	.text
	.set	noreorder


	.local	smap_stub
smap_stub:
	.word	0x41e00000
	.word	0
	.word	0x00000101
	.ascii	"smapq\0\0\0"
	.align	2

#include "smap.defs"

	.word	0
	.word	0

