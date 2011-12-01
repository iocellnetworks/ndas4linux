;/*
; import stub, link ip_stub.iop.o with any other .irx files that want to use
; functions from this module
;*/

;/* quick macro for creating stubs */
.MACRO STUB NAME,ID
        .globl   \NAME
        \NAME:
        j       $31
        li      $0,\ID
.ENDM

	.text
	.set	noreorder

	.local	exp_stub
exp_stub:
	.word	0x41e00000
	.word	0
	.word	0x00000101
	.ascii	"IPfo\0\0"
	.align	2

#include "ip.defs"

	.word	0
	.word	0

