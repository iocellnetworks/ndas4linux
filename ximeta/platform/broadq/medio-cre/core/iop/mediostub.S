.MACRO STUB NAME,ID
        .weak   \NAME
        .ent   \NAME
\NAME:
        j       $31
        li      $0,\ID
		.end	\NAME
.ENDM

	.text
	.set	noreorder


	.local	medio_stub
medio_stub:
	.word	0x41e00000
	.word	0
	.word	0x00000101
	.ascii	"medio\0\0\0"
	.align	2

#include "medio.defs"

	.word	0
	.word	0

