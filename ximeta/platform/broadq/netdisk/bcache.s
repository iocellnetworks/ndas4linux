	.section .mdebug.eabi32
	.previous
	.file 1 "bcache.c"
	.section	.debug_abbrev,"",@progbits
$Ldebug_abbrev0:
	.section	.debug_info,"",@progbits
$Ldebug_info0:
	.section	.debug_line,"",@progbits
$Ldebug_line0:
	.text
$Ltext0:
	.file 2 "../medio-cre/include/mio/cdlist.h"
	.file 3 "../medio-cre/include/mio/bcache.h"
	.file 4 "../medio-cre/include/mio/features.h"
	.file 5 "../medio-cre/include/mio/modules.h"
	.file 6 "../medio-cre/include/mio/utils.h"
	.rdata
	.align	2
	.type	EIO,@object
	.size	EIO,4
EIO:
	.word	5
	.align	2
	.type	ENOMEM,@object
	.size	ENOMEM,4
ENOMEM:
	.word	12
	.align	2
	.type	ENODEV,@object
	.size	ENODEV,4
ENODEV:
	.word	19
	.align	2
	.type	EINVAL,@object
	.size	EINVAL,4
EINVAL:
	.word	22
	.align	2
	.type	EROFS,@object
	.size	EROFS,4
EROFS:
	.word	30
	.align	2
	.type	EPIPE,@object
	.size	EPIPE,4
EPIPE:
	.word	32
	.align	2
	.type	EBADE,@object
	.size	EBADE,4
EBADE:
	.word	52
	.align	2
	.type	ENOSYS,@object
	.size	ENOSYS,4
ENOSYS:
	.word	88
	.align	2
$LC0:
	.ascii	"bcache.c\000"
	.data
	.align	3
	.type	thisModule,@object
	.size	thisModule,16
thisModule:
	.word	14
	.word	$LC0
	.dword	0x0
	.rdata
	.align	2
$LC1:
	.ascii	"buffer cache init, addr: %#x, size: %u\n\000"
	.align	2
$LC2:
	.ascii	"%s: Initializing buffer cache at %#x with %u entries.\000"
	.align	2
$LC3:
	.ascii	"buffer cache pool: %#x\n\000"
	.align	2
$LC4:
	.ascii	"buffer cache: no more memory, size: %u\n\000"
	.text
	.align	2
	.globl	MioBufferCacheInit
$LFB1:
	.loc 1 34 0
	.ent	MioBufferCacheInit
MioBufferCacheInit:
	.frame	$fp,32,$31		# vars= 16, regs= 3/0, args= 0, extra= 0
	.mask	0xc0010000,-8
	.fmask	0x00000000,0
	subu	$sp,$sp,32
$LCFI0:
	sw	$31,24($sp)
$LCFI1:
	sw	$fp,20($sp)
$LCFI2:
	sw	$16,16($sp)
$LCFI3:
	move	$fp,$sp
$LCFI4:
	sw	$4,0($fp)
	sw	$5,4($fp)
$LBB2:
	.loc 1 37 0
	la	$4,$LC1
	lw	$5,0($fp)
	lw	$6,4($fp)
	jal	printf
	.loc 1 38 0
	li	$4,1966080			# 0x1e0000
	ori	$4,$4,0x8480
	jal	DelayThread
	.loc 1 39 0
	la	$4,$LC1
	lw	$5,0($fp)
	lw	$6,4($fp)
	jal	printf
	.loc 1 41 0
	la	$4,thisModule
	jal	MioModuleIsTracing
	beq	$2,$0,$L10
	.loc 1 42 0
	la	$4,$LC2
	la	$5,$LC0
	lw	$6,0($fp)
	lw	$7,4($fp)
	jal	printf
$L10:
	.loc 1 46 0
	lw	$4,0($fp)
	li	$5,12			# 0xc
	jal	bzero
	.loc 1 50 0
	lw	$16,0($fp)
	lw	$3,4($fp)
	li	$2,24			# 0x18
	mult	$3,$2
	mflo	$2
	move	$4,$2
	jal	malloc
	sw	$2,8($16)
	.loc 1 51 0
	lw	$2,0($fp)
	la	$4,$LC3
	lw	$5,8($2)
	jal	printf
	.loc 1 52 0
	lw	$2,0($fp)
	lw	$2,8($2)
	bne	$2,$0,$L11
	.loc 1 53 0
	la	$4,$LC4
	lw	$5,4($fp)
	jal	printf
	.loc 1 54 0
	b	$L9
$L11:
	.loc 1 56 0
	li	$4,1966080			# 0x1e0000
	ori	$4,$4,0x8480
	jal	DelayThread
	.loc 1 57 0
	lw	$4,0($fp)
	lw	$3,4($fp)
	li	$2,24			# 0x18
	mult	$3,$2
	mflo	$2
	lw	$4,8($4)
	move	$5,$2
	jal	bzero
	.loc 1 61 0
	sw	$0,8($fp)
$L12:
	lw	$2,8($fp)
	lw	$3,4($fp)
	slt	$2,$2,$3
	bne	$2,$0,$L15
	b	$L9
$L15:
	.loc 1 62 0
	lw	$4,0($fp)
	lw	$3,8($fp)
	li	$2,24			# 0x18
	mult	$3,$2
	mflo	$3
	lw	$2,8($4)
	addu	$2,$3,$2
	lw	$4,0($fp)
	move	$5,$2
	jal	MioDoublyLinkedListInsertAtFront
	.loc 1 61 0
	lw	$2,8($fp)
	addu	$2,$2,1
	sw	$2,8($fp)
	b	$L12
$LBE2:
	.loc 1 64 0
$L9:
	move	$sp,$fp
	lw	$31,24($sp)
	lw	$fp,20($sp)
	lw	$16,16($sp)
	addu	$sp,$sp,32
	j	$31
	.end	MioBufferCacheInit
$LFE1:
$Lfe1:
	.size	MioBufferCacheInit,$Lfe1-MioBufferCacheInit
	.rdata
	.align	2
$LC5:
	.ascii	"%s: Destroying buffer cache at %x\000"
	.text
	.align	2
	.globl	MioBufferCacheTerm
$LFB2:
	.loc 1 77 0
	.ent	MioBufferCacheTerm
MioBufferCacheTerm:
	.frame	$fp,32,$31		# vars= 16, regs= 2/0, args= 0, extra= 0
	.mask	0xc0000000,-12
	.fmask	0x00000000,0
	subu	$sp,$sp,32
$LCFI5:
	sw	$31,20($sp)
$LCFI6:
	sw	$fp,16($sp)
$LCFI7:
	move	$fp,$sp
$LCFI8:
	sw	$4,0($fp)
$LBB3:
	.loc 1 78 0
	la	$4,thisModule
	jal	MioModuleIsTracing
	beq	$2,$0,$L17
$LBB4:
	.loc 1 79 0
	la	$4,$LC5
	la	$5,$LC0
	lw	$6,0($fp)
	jal	printf
$LBE4:
$L17:
	.loc 1 82 0
	lw	$2,0($fp)
	lw	$2,8($2)
	beq	$2,$0,$L18
	lw	$2,0($fp)
	lw	$4,8($2)
	jal	free
$L18:
	.loc 1 83 0
	lw	$4,0($fp)
	li	$5,12			# 0xc
	jal	bzero
	.loc 1 84 0
	move	$sp,$fp
	lw	$31,20($sp)
	lw	$fp,16($sp)
	addu	$sp,$sp,32
	j	$31
$LBE3:
	.end	MioBufferCacheTerm
$LFE2:
$Lfe2:
	.size	MioBufferCacheTerm,$Lfe2-MioBufferCacheTerm
	.rdata
	.align	2
$LC6:
	.ascii	"%s: Add buffer cache[0x%x] entry: %s\000"
	.text
	.align	2
	.globl	MioBufferCacheAddEntry
$LFB3:
	.loc 1 100 0
	.ent	MioBufferCacheAddEntry
MioBufferCacheAddEntry:
	.frame	$fp,144,$31		# vars= 128, regs= 2/0, args= 0, extra= 0
	.mask	0xc0000000,-12
	.fmask	0x00000000,0
	subu	$sp,$sp,144
$LCFI9:
	sw	$31,132($sp)
$LCFI10:
	sw	$fp,128($sp)
$LCFI11:
	move	$fp,$sp
$LCFI12:
	sw	$4,0($fp)
	sw	$5,4($fp)
$LBB5:
	.loc 1 101 0
	lw	$2,0($fp)
	lw	$2,4($2)
	sw	$2,8($fp)
	.loc 1 103 0
	la	$4,thisModule
	jal	MioModuleIsTracing
	beq	$2,$0,$L20
$LBB6:
	.loc 1 105 0
	lw	$2,8($fp)
	addu	$2,$2,8
	addu	$3,$fp,16
	move	$4,$2
	move	$5,$3
	li	$6,100			# 0x64
	jal	MioBufferCacheEntryAsString
	la	$4,$LC6
	la	$5,$LC0
	lw	$6,0($fp)
	move	$7,$2
	jal	printf
$LBE6:
$L20:
	.loc 1 111 0
	lw	$4,0($fp)
	lw	$5,8($fp)
	jal	MioDoublyLinkedListRemove
	.loc 1 112 0
	lw	$2,8($fp)
	addu	$2,$2,8
	move	$4,$2
	jal	MioBufferCacheEntryTerm
	.loc 1 116 0
	lw	$4,8($fp)
	lw	$3,4($fp)
	lw	$2,0($3)
	sw	$2,8($4)
	lw	$2,4($3)
	sw	$2,12($4)
	lw	$2,8($3)
	sw	$2,16($4)
	lw	$2,12($3)
	sw	$2,20($4)
	.loc 1 118 0
	lw	$4,0($fp)
	lw	$5,8($fp)
	jal	MioDoublyLinkedListInsertAtFront
	.loc 1 119 0
	move	$sp,$fp
	lw	$31,132($sp)
	lw	$fp,128($sp)
	addu	$sp,$sp,144
	j	$31
$LBE5:
	.end	MioBufferCacheAddEntry
$LFE3:
$Lfe3:
	.size	MioBufferCacheAddEntry,$Lfe3-MioBufferCacheAddEntry
	.align	2
	.globl	MioBufferCacheContains
$LFB4:
	.loc 1 137 0
	.ent	MioBufferCacheContains
MioBufferCacheContains:
	.frame	$fp,176,$31		# vars= 160, regs= 2/0, args= 0, extra= 0
	.mask	0xc0000000,-12
	.fmask	0x00000000,0
	subu	$sp,$sp,176
$LCFI13:
	sw	$31,164($sp)
$LCFI14:
	sw	$fp,160($sp)
$LCFI15:
	move	$fp,$sp
$LCFI16:
	sw	$4,0($fp)
	sw	$6,8($fp)
	sw	$7,12($fp)
	sw	$8,16($fp)
$LBB7:
	.loc 1 138 0
	lw	$2,0($fp)
	lw	$2,0($2)
	sw	$2,20($fp)
	.loc 1 140 0
$L22:
	lw	$2,20($fp)
	bne	$2,$0,$L24
	b	$L23
$L24:
$LBB8:
	.loc 1 141 0
	lw	$2,20($fp)
	sw	$2,24($fp)
	.loc 1 142 0
	lw	$2,24($fp)
	lw	$2,16($2)
	beq	$2,$0,$L25
	.loc 1 143 0
	lw	$2,24($fp)
	sw	$2,148($fp)
	lw	$3,148($fp)
	lw	$2,12($3)
	lw	$3,12($fp)
	sltu	$2,$3,$2
	bne	$2,$0,$L25
	lw	$8,148($fp)
	lw	$3,12($8)
	lw	$2,12($fp)
	bne	$3,$2,$L27
	lw	$9,148($fp)
	lw	$2,8($9)
	lw	$3,8($fp)
	sltu	$2,$3,$2
	bne	$2,$0,$L25
$L27:
	lw	$5,24($fp)
	lw	$4,24($fp)
	lw	$2,20($4)
	move	$3,$0
	lw	$4,8($5)
	lw	$5,12($5)
	addu	$8,$2,$4
	sltu	$6,$8,$4
	addu	$9,$3,$5
	addu	$9,$9,$6
	sw	$8,152($fp)
	sw	$9,156($fp)
	lw	$2,12($fp)
	lw	$9,156($fp)
	sltu	$2,$2,$9
	bne	$2,$0,$L28
	lw	$2,12($fp)
	lw	$3,156($fp)
	bne	$2,$3,$L25
	lw	$2,8($fp)
	lw	$8,152($fp)
	sltu	$2,$2,$8
	bne	$2,$0,$L28
	b	$L25
$L28:
	.loc 1 145 0
	lw	$4,16($fp)
	lw	$3,24($fp)
	lw	$2,8($3)
	sw	$2,0($4)
	lw	$2,12($3)
	sw	$2,4($4)
	lw	$2,16($3)
	sw	$2,8($4)
	lw	$2,20($3)
	sw	$2,12($4)
	.loc 1 146 0
	la	$4,thisModule
	jal	MioModuleIsTracing
	.loc 1 154 0
	lw	$2,0($fp)
	lw	$3,0($2)
	lw	$2,20($fp)
	beq	$3,$2,$L30
	.loc 1 155 0
	lw	$4,0($fp)
	lw	$5,20($fp)
	jal	MioDoublyLinkedListRemove
	.loc 1 156 0
	lw	$4,0($fp)
	lw	$5,20($fp)
	jal	MioDoublyLinkedListInsertAtFront
$L30:
	.loc 1 158 0
	li	$9,1			# 0x1
	sw	$9,144($fp)
	b	$L21
$L25:
	.loc 1 161 0
	lw	$2,20($fp)
	lw	$2,4($2)
	sw	$2,20($fp)
$LBE8:
	b	$L22
$L23:
	.loc 1 164 0
	sw	$0,144($fp)
$LBE7:
	.loc 1 165 0
$L21:
	lw	$2,144($fp)
	move	$sp,$fp
	lw	$31,164($sp)
	lw	$fp,160($sp)
	addu	$sp,$sp,176
	j	$31
	.end	MioBufferCacheContains
$LFE4:
$Lfe4:
	.size	MioBufferCacheContains,$Lfe4-MioBufferCacheContains
	.rdata
	.align	2
$LC7:
	.ascii	"%s[0x%x]= \000"
	.text
	.align	2
	.globl	MioBufferCachePrint
$LFB5:
	.loc 1 178 0
	.ent	MioBufferCachePrint
MioBufferCachePrint:
	.frame	$fp,160,$31		# vars= 144, regs= 2/0, args= 0, extra= 0
	.mask	0xc0000000,-12
	.fmask	0x00000000,0
	subu	$sp,$sp,160
$LCFI17:
	sw	$31,148($sp)
$LCFI18:
	sw	$fp,144($sp)
$LCFI19:
	move	$fp,$sp
$LCFI20:
	sw	$4,0($fp)
	sw	$5,4($fp)
$LBB9:
	.loc 1 179 0
	lw	$2,0($fp)
	lw	$2,0($2)
	sw	$2,8($fp)
	.loc 1 181 0
$L32:
	lw	$2,8($fp)
	bne	$2,$0,$L34
	b	$L31
$L34:
$LBB10:
	.loc 1 183 0
	lw	$2,8($fp)
	sw	$2,128($fp)
	.loc 1 184 0
	addu	$2,$fp,16
	move	$4,$2
	la	$5,$LC7
	lw	$6,4($fp)
	lw	$7,8($fp)
	jal	sprintf
	.loc 1 185 0
	lw	$2,128($fp)
	addu	$2,$2,8
	addu	$3,$fp,16
	move	$4,$2
	move	$5,$3
	jal	MioBufferCacheEntryPrint
	.loc 1 186 0
	lw	$2,8($fp)
	lw	$2,4($2)
	sw	$2,8($fp)
$LBE10:
	b	$L32
$LBE9:
	.loc 1 188 0
$L31:
	move	$sp,$fp
	lw	$31,148($sp)
	lw	$fp,144($sp)
	addu	$sp,$sp,160
	j	$31
	.end	MioBufferCachePrint
$LFE5:
$Lfe5:
	.size	MioBufferCachePrint,$Lfe5-MioBufferCachePrint
	.align	2
	.globl	MioBufferCacheEntryInit
$LFB6:
	.loc 1 200 0
	.ent	MioBufferCacheEntryInit
MioBufferCacheEntryInit:
	.frame	$fp,32,$31		# vars= 16, regs= 2/0, args= 0, extra= 0
	.mask	0xc0000000,-12
	.fmask	0x00000000,0
	subu	$sp,$sp,32
$LCFI21:
	sw	$31,20($sp)
$LCFI22:
	sw	$fp,16($sp)
$LCFI23:
	move	$fp,$sp
$LCFI24:
	sw	$4,0($fp)
$LBB11:
	.loc 1 201 0
	lw	$4,0($fp)
	li	$5,16			# 0x10
	jal	bzero
	.loc 1 202 0
	move	$sp,$fp
	lw	$31,20($sp)
	lw	$fp,16($sp)
	addu	$sp,$sp,32
	j	$31
$LBE11:
	.end	MioBufferCacheEntryInit
$LFE6:
$Lfe6:
	.size	MioBufferCacheEntryInit,$Lfe6-MioBufferCacheEntryInit
	.align	2
	.globl	MioBufferCacheEntryTerm
$LFB7:
	.loc 1 215 0
	.ent	MioBufferCacheEntryTerm
MioBufferCacheEntryTerm:
	.frame	$fp,32,$31		# vars= 16, regs= 2/0, args= 0, extra= 0
	.mask	0xc0000000,-12
	.fmask	0x00000000,0
	subu	$sp,$sp,32
$LCFI25:
	sw	$31,20($sp)
$LCFI26:
	sw	$fp,16($sp)
$LCFI27:
	move	$fp,$sp
$LCFI28:
	sw	$4,0($fp)
$LBB12:
	.loc 1 216 0
	lw	$2,0($fp)
	lw	$2,8($2)
	beq	$2,$0,$L37
	lw	$2,0($fp)
	lw	$4,8($2)
	jal	free
$L37:
	.loc 1 217 0
	lw	$4,0($fp)
	li	$5,16			# 0x10
	jal	bzero
	.loc 1 218 0
	move	$sp,$fp
	lw	$31,20($sp)
	lw	$fp,16($sp)
	addu	$sp,$sp,32
	j	$31
$LBE12:
	.end	MioBufferCacheEntryTerm
$LFE7:
$Lfe7:
	.size	MioBufferCacheEntryTerm,$Lfe7-MioBufferCacheEntryTerm
	.rdata
	.align	2
$LC8:
	.ascii	"%s%s\000"
	.text
	.align	2
	.globl	MioBufferCacheEntryPrint
$LFB8:
	.loc 1 232 0
	.ent	MioBufferCacheEntryPrint
MioBufferCacheEntryPrint:
	.frame	$fp,144,$31		# vars= 128, regs= 2/0, args= 0, extra= 0
	.mask	0xc0000000,-12
	.fmask	0x00000000,0
	subu	$sp,$sp,144
$LCFI29:
	sw	$31,132($sp)
$LCFI30:
	sw	$fp,128($sp)
$LCFI31:
	move	$fp,$sp
$LCFI32:
	sw	$4,0($fp)
	sw	$5,4($fp)
$LBB13:
	.loc 1 235 0
	addu	$2,$fp,16
	lw	$4,0($fp)
	move	$5,$2
	li	$6,100			# 0x64
	jal	MioBufferCacheEntryAsString
	la	$4,$LC8
	lw	$5,4($fp)
	move	$6,$2
	jal	printf
	.loc 1 236 0
	move	$sp,$fp
	lw	$31,132($sp)
	lw	$fp,128($sp)
	addu	$sp,$sp,144
	j	$31
$LBE13:
	.end	MioBufferCacheEntryPrint
$LFE8:
$Lfe8:
	.size	MioBufferCacheEntryPrint,$Lfe8-MioBufferCacheEntryPrint
	.rdata
	.align	2
$LC9:
	.ascii	"offset: %lld, size: %d, ram: 0x%x\000"
	.text
	.align	2
	.globl	MioBufferCacheEntryAsString
$LFB9:
	.loc 1 250 0
	.ent	MioBufferCacheEntryAsString
MioBufferCacheEntryAsString:
	.frame	$fp,32,$31		# vars= 16, regs= 2/0, args= 0, extra= 0
	.mask	0xc0000000,-12
	.fmask	0x00000000,0
	subu	$sp,$sp,32
$LCFI33:
	sw	$31,20($sp)
$LCFI34:
	sw	$fp,16($sp)
$LCFI35:
	move	$fp,$sp
$LCFI36:
	sw	$4,0($fp)
	sw	$5,4($fp)
	sw	$6,8($fp)
$LBB14:
	.loc 1 251 0
	lw	$2,0($fp)
	lw	$3,0($fp)
	lw	$7,0($fp)
	lw	$4,4($fp)
	lw	$5,8($fp)
	la	$6,$LC9
	lw	$8,0($2)
	lw	$9,4($2)
	lw	$10,12($3)
	lw	$11,8($7)
	jal	snprintf
	.loc 1 253 0
	lw	$2,4($fp)
$LBE14:
	.loc 1 254 0
	move	$sp,$fp
	lw	$31,20($sp)
	lw	$fp,16($sp)
	addu	$sp,$sp,32
	j	$31
	.end	MioBufferCacheEntryAsString
$LFE9:
$Lfe9:
	.size	MioBufferCacheEntryAsString,$Lfe9-MioBufferCacheEntryAsString
	.file 7 "/home/broadq/src/ps2lib-2.1/common/include/tamtypes.h"
	.section	.debug_frame,"",@progbits
$Lframe0:
	.4byte	$LECIE0-$LSCIE0
$LSCIE0:
	.4byte	0xffffffff
	.byte	0x1
	.ascii	"\000"
	.uleb128 0x1
	.sleb128 4
	.byte	0x40
	.byte	0xc
	.uleb128 0x1d
	.uleb128 0x0
	.align	2
$LECIE0:
$LSFDE0:
	.4byte	$LEFDE0-$LASFDE0
$LASFDE0:
	.4byte	$Lframe0
	.4byte	$LFB1
	.4byte	$LFE1-$LFB1
	.byte	0x4
	.4byte	$LCFI0-$LFB1
	.byte	0xe
	.uleb128 0x20
	.byte	0x4
	.4byte	$LCFI3-$LCFI0
	.byte	0x11
	.uleb128 0x10
	.sleb128 -4
	.byte	0x11
	.uleb128 0x1e
	.sleb128 -3
	.byte	0x11
	.uleb128 0x40
	.sleb128 -2
	.byte	0x4
	.4byte	$LCFI4-$LCFI3
	.byte	0xc
	.uleb128 0x1e
	.uleb128 0x20
	.align	2
$LEFDE0:
$LSFDE2:
	.4byte	$LEFDE2-$LASFDE2
$LASFDE2:
	.4byte	$Lframe0
	.4byte	$LFB2
	.4byte	$LFE2-$LFB2
	.byte	0x4
	.4byte	$LCFI5-$LFB2
	.byte	0xe
	.uleb128 0x20
	.byte	0x4
	.4byte	$LCFI7-$LCFI5
	.byte	0x11
	.uleb128 0x1e
	.sleb128 -4
	.byte	0x11
	.uleb128 0x40
	.sleb128 -3
	.byte	0x4
	.4byte	$LCFI8-$LCFI7
	.byte	0xc
	.uleb128 0x1e
	.uleb128 0x20
	.align	2
$LEFDE2:
$LSFDE4:
	.4byte	$LEFDE4-$LASFDE4
$LASFDE4:
	.4byte	$Lframe0
	.4byte	$LFB3
	.4byte	$LFE3-$LFB3
	.byte	0x4
	.4byte	$LCFI9-$LFB3
	.byte	0xe
	.uleb128 0x90
	.byte	0x4
	.4byte	$LCFI11-$LCFI9
	.byte	0x11
	.uleb128 0x1e
	.sleb128 -4
	.byte	0x11
	.uleb128 0x40
	.sleb128 -3
	.byte	0x4
	.4byte	$LCFI12-$LCFI11
	.byte	0xc
	.uleb128 0x1e
	.uleb128 0x90
	.align	2
$LEFDE4:
$LSFDE6:
	.4byte	$LEFDE6-$LASFDE6
$LASFDE6:
	.4byte	$Lframe0
	.4byte	$LFB4
	.4byte	$LFE4-$LFB4
	.byte	0x4
	.4byte	$LCFI13-$LFB4
	.byte	0xe
	.uleb128 0xb0
	.byte	0x4
	.4byte	$LCFI15-$LCFI13
	.byte	0x11
	.uleb128 0x1e
	.sleb128 -4
	.byte	0x11
	.uleb128 0x40
	.sleb128 -3
	.byte	0x4
	.4byte	$LCFI16-$LCFI15
	.byte	0xc
	.uleb128 0x1e
	.uleb128 0xb0
	.align	2
$LEFDE6:
$LSFDE8:
	.4byte	$LEFDE8-$LASFDE8
$LASFDE8:
	.4byte	$Lframe0
	.4byte	$LFB5
	.4byte	$LFE5-$LFB5
	.byte	0x4
	.4byte	$LCFI17-$LFB5
	.byte	0xe
	.uleb128 0xa0
	.byte	0x4
	.4byte	$LCFI19-$LCFI17
	.byte	0x11
	.uleb128 0x1e
	.sleb128 -4
	.byte	0x11
	.uleb128 0x40
	.sleb128 -3
	.byte	0x4
	.4byte	$LCFI20-$LCFI19
	.byte	0xc
	.uleb128 0x1e
	.uleb128 0xa0
	.align	2
$LEFDE8:
$LSFDE10:
	.4byte	$LEFDE10-$LASFDE10
$LASFDE10:
	.4byte	$Lframe0
	.4byte	$LFB6
	.4byte	$LFE6-$LFB6
	.byte	0x4
	.4byte	$LCFI21-$LFB6
	.byte	0xe
	.uleb128 0x20
	.byte	0x4
	.4byte	$LCFI23-$LCFI21
	.byte	0x11
	.uleb128 0x1e
	.sleb128 -4
	.byte	0x11
	.uleb128 0x40
	.sleb128 -3
	.byte	0x4
	.4byte	$LCFI24-$LCFI23
	.byte	0xc
	.uleb128 0x1e
	.uleb128 0x20
	.align	2
$LEFDE10:
$LSFDE12:
	.4byte	$LEFDE12-$LASFDE12
$LASFDE12:
	.4byte	$Lframe0
	.4byte	$LFB7
	.4byte	$LFE7-$LFB7
	.byte	0x4
	.4byte	$LCFI25-$LFB7
	.byte	0xe
	.uleb128 0x20
	.byte	0x4
	.4byte	$LCFI27-$LCFI25
	.byte	0x11
	.uleb128 0x1e
	.sleb128 -4
	.byte	0x11
	.uleb128 0x40
	.sleb128 -3
	.byte	0x4
	.4byte	$LCFI28-$LCFI27
	.byte	0xc
	.uleb128 0x1e
	.uleb128 0x20
	.align	2
$LEFDE12:
$LSFDE14:
	.4byte	$LEFDE14-$LASFDE14
$LASFDE14:
	.4byte	$Lframe0
	.4byte	$LFB8
	.4byte	$LFE8-$LFB8
	.byte	0x4
	.4byte	$LCFI29-$LFB8
	.byte	0xe
	.uleb128 0x90
	.byte	0x4
	.4byte	$LCFI31-$LCFI29
	.byte	0x11
	.uleb128 0x1e
	.sleb128 -4
	.byte	0x11
	.uleb128 0x40
	.sleb128 -3
	.byte	0x4
	.4byte	$LCFI32-$LCFI31
	.byte	0xc
	.uleb128 0x1e
	.uleb128 0x90
	.align	2
$LEFDE14:
$LSFDE16:
	.4byte	$LEFDE16-$LASFDE16
$LASFDE16:
	.4byte	$Lframe0
	.4byte	$LFB9
	.4byte	$LFE9-$LFB9
	.byte	0x4
	.4byte	$LCFI33-$LFB9
	.byte	0xe
	.uleb128 0x20
	.byte	0x4
	.4byte	$LCFI35-$LCFI33
	.byte	0x11
	.uleb128 0x1e
	.sleb128 -4
	.byte	0x11
	.uleb128 0x40
	.sleb128 -3
	.byte	0x4
	.4byte	$LCFI36-$LCFI35
	.byte	0xc
	.uleb128 0x1e
	.uleb128 0x20
	.align	2
$LEFDE16:
	.align	0
	.text
$Letext0:
	.section	.debug_info
	.4byte	0x7db
	.2byte	0x2
	.4byte	$Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	$Ldebug_line0
	.4byte	$Letext0
	.4byte	$Ltext0
	.4byte	$LC131
	.4byte	$LC132
	.4byte	$LC133
	.byte	0x1
	.uleb128 0x2
	.4byte	0x4e
	.4byte	$LC12
	.byte	0x8
	.byte	0x2
	.byte	0x5
	.uleb128 0x3
	.4byte	$LC10
	.byte	0x2
	.byte	0x6
	.4byte	0x4e
	.byte	0x2
	.byte	0x10
	.uleb128 0x0
	.uleb128 0x3
	.4byte	$LC11
	.byte	0x2
	.byte	0x7
	.4byte	0x4e
	.byte	0x2
	.byte	0x10
	.uleb128 0x4
	.byte	0x0
	.uleb128 0x4
	.byte	0x4
	.4byte	0x25
	.uleb128 0x2
	.4byte	0x7d
	.4byte	$LC13
	.byte	0x8
	.byte	0x2
	.byte	0xa
	.uleb128 0x3
	.4byte	$LC14
	.byte	0x2
	.byte	0xb
	.4byte	0x7d
	.byte	0x2
	.byte	0x10
	.uleb128 0x0
	.uleb128 0x3
	.4byte	$LC15
	.byte	0x2
	.byte	0xc
	.4byte	0x7d
	.byte	0x2
	.byte	0x10
	.uleb128 0x4
	.byte	0x0
	.uleb128 0x4
	.byte	0x4
	.4byte	0x25
	.uleb128 0x2
	.4byte	0xba
	.4byte	$LC16
	.byte	0x10
	.byte	0x3
	.byte	0x6
	.uleb128 0x3
	.4byte	$LC17
	.byte	0x3
	.byte	0x7
	.4byte	0xba
	.byte	0x2
	.byte	0x10
	.uleb128 0x0
	.uleb128 0x3
	.4byte	$LC18
	.byte	0x3
	.byte	0x8
	.4byte	0xc1
	.byte	0x2
	.byte	0x10
	.uleb128 0x8
	.uleb128 0x3
	.4byte	$LC19
	.byte	0x3
	.byte	0x9
	.4byte	0xc3
	.byte	0x2
	.byte	0x10
	.uleb128 0xc
	.byte	0x0
	.uleb128 0x5
	.ascii	"u64\000"
	.byte	0x8
	.byte	0x7
	.uleb128 0x6
	.byte	0x4
	.uleb128 0x5
	.ascii	"u32\000"
	.byte	0x4
	.byte	0x7
	.uleb128 0x2
	.4byte	0xf3
	.4byte	$LC20
	.byte	0x18
	.byte	0x3
	.byte	0xc
	.uleb128 0x3
	.4byte	$LC21
	.byte	0x3
	.byte	0xd
	.4byte	0x25
	.byte	0x2
	.byte	0x10
	.uleb128 0x0
	.uleb128 0x3
	.4byte	$LC22
	.byte	0x3
	.byte	0xe
	.4byte	0x83
	.byte	0x2
	.byte	0x10
	.uleb128 0x8
	.byte	0x0
	.uleb128 0x2
	.4byte	0x11c
	.4byte	$LC23
	.byte	0xc
	.byte	0x3
	.byte	0x11
	.uleb128 0x3
	.4byte	$LC24
	.byte	0x3
	.byte	0x12
	.4byte	0x54
	.byte	0x2
	.byte	0x10
	.uleb128 0x0
	.uleb128 0x3
	.4byte	$LC25
	.byte	0x3
	.byte	0x13
	.4byte	0x11c
	.byte	0x2
	.byte	0x10
	.uleb128 0x8
	.byte	0x0
	.uleb128 0x4
	.byte	0x4
	.4byte	0xca
	.uleb128 0x7
	.4byte	0x177
	.4byte	$LC38
	.byte	0x4
	.byte	0x4
	.byte	0xd
	.uleb128 0x8
	.4byte	$LC26
	.byte	0xa
	.uleb128 0x8
	.4byte	$LC27
	.byte	0xb
	.uleb128 0x8
	.4byte	$LC28
	.byte	0xc
	.uleb128 0x8
	.4byte	$LC29
	.byte	0xd
	.uleb128 0x8
	.4byte	$LC30
	.byte	0xe
	.uleb128 0x8
	.4byte	$LC31
	.byte	0xf
	.uleb128 0x8
	.4byte	$LC32
	.byte	0x10
	.uleb128 0x8
	.4byte	$LC33
	.byte	0x11
	.uleb128 0x8
	.4byte	$LC34
	.byte	0x12
	.uleb128 0x8
	.4byte	$LC35
	.byte	0x13
	.uleb128 0x8
	.4byte	$LC36
	.byte	0x14
	.uleb128 0x8
	.4byte	$LC37
	.byte	0x40
	.byte	0x0
	.uleb128 0x7
	.4byte	0x25c
	.4byte	$LC39
	.byte	0x4
	.byte	0x5
	.byte	0xe
	.uleb128 0x8
	.4byte	$LC40
	.byte	0x1
	.uleb128 0x8
	.4byte	$LC41
	.byte	0x2
	.uleb128 0x8
	.4byte	$LC42
	.byte	0x3
	.uleb128 0x8
	.4byte	$LC43
	.byte	0x4
	.uleb128 0x8
	.4byte	$LC44
	.byte	0x5
	.uleb128 0x8
	.4byte	$LC45
	.byte	0x6
	.uleb128 0x8
	.4byte	$LC46
	.byte	0x7
	.uleb128 0x8
	.4byte	$LC47
	.byte	0x8
	.uleb128 0x8
	.4byte	$LC48
	.byte	0x9
	.uleb128 0x8
	.4byte	$LC49
	.byte	0xa
	.uleb128 0x8
	.4byte	$LC50
	.byte	0xb
	.uleb128 0x8
	.4byte	$LC51
	.byte	0xc
	.uleb128 0x8
	.4byte	$LC52
	.byte	0xd
	.uleb128 0x8
	.4byte	$LC53
	.byte	0xe
	.uleb128 0x8
	.4byte	$LC54
	.byte	0xf
	.uleb128 0x8
	.4byte	$LC55
	.byte	0x10
	.uleb128 0x8
	.4byte	$LC56
	.byte	0x11
	.uleb128 0x8
	.4byte	$LC57
	.byte	0x12
	.uleb128 0x8
	.4byte	$LC58
	.byte	0x13
	.uleb128 0x8
	.4byte	$LC59
	.byte	0x14
	.uleb128 0x8
	.4byte	$LC60
	.byte	0x15
	.uleb128 0x8
	.4byte	$LC61
	.byte	0x16
	.uleb128 0x8
	.4byte	$LC62
	.byte	0x17
	.uleb128 0x8
	.4byte	$LC63
	.byte	0x18
	.uleb128 0x8
	.4byte	$LC64
	.byte	0x19
	.uleb128 0x8
	.4byte	$LC65
	.byte	0x1a
	.uleb128 0x8
	.4byte	$LC66
	.byte	0x1b
	.uleb128 0x8
	.4byte	$LC67
	.byte	0x1c
	.uleb128 0x8
	.4byte	$LC68
	.byte	0x1d
	.uleb128 0x8
	.4byte	$LC69
	.byte	0x1e
	.uleb128 0x8
	.4byte	$LC70
	.byte	0x1f
	.uleb128 0x8
	.4byte	$LC71
	.byte	0x20
	.uleb128 0x8
	.4byte	$LC72
	.byte	0x21
	.uleb128 0x8
	.4byte	$LC73
	.byte	0x22
	.uleb128 0x8
	.4byte	$LC74
	.byte	0x23
	.uleb128 0x8
	.4byte	$LC75
	.byte	0x50
	.byte	0x0
	.uleb128 0x2
	.4byte	0x293
	.4byte	$LC76
	.byte	0x10
	.byte	0x5
	.byte	0x4b
	.uleb128 0x3
	.4byte	$LC77
	.byte	0x5
	.byte	0x4c
	.4byte	0x177
	.byte	0x2
	.byte	0x10
	.uleb128 0x0
	.uleb128 0x3
	.4byte	$LC78
	.byte	0x5
	.byte	0x4d
	.4byte	0x293
	.byte	0x2
	.byte	0x10
	.uleb128 0x4
	.uleb128 0x3
	.4byte	$LC79
	.byte	0x5
	.byte	0x4e
	.4byte	0xba
	.byte	0x2
	.byte	0x10
	.uleb128 0x8
	.byte	0x0
	.uleb128 0x4
	.byte	0x4
	.4byte	0x299
	.uleb128 0x9
	.4byte	0x29e
	.uleb128 0xa
	.4byte	$LC80
	.byte	0x1
	.byte	0x5
	.uleb128 0x2
	.4byte	0x2ce
	.4byte	$LC81
	.byte	0x10
	.byte	0x6
	.byte	0x9
	.uleb128 0x3
	.4byte	$LC82
	.byte	0x6
	.byte	0xa
	.4byte	0x2ce
	.byte	0x2
	.byte	0x10
	.uleb128 0x0
	.uleb128 0x3
	.4byte	$LC83
	.byte	0x6
	.byte	0xb
	.4byte	0x2d5
	.byte	0x2
	.byte	0x10
	.uleb128 0x8
	.byte	0x0
	.uleb128 0xa
	.4byte	$LC84
	.byte	0x8
	.byte	0x5
	.uleb128 0x5
	.ascii	"int\000"
	.byte	0x4
	.byte	0x5
	.uleb128 0xb
	.4byte	0x363
	.byte	0x1
	.4byte	$LC87
	.byte	0x1
	.byte	0x22
	.byte	0x1
	.4byte	$LFB1
	.4byte	$LFE1
	.4byte	$LSFDE0
	.byte	0x1
	.byte	0x6e
	.uleb128 0xc
	.4byte	$LC85
	.byte	0x1
	.byte	0x21
	.4byte	0x363
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0xc
	.4byte	$LC86
	.byte	0x1
	.byte	0x21
	.4byte	0x2d5
	.byte	0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0xd
	.ascii	"i\000"
	.byte	0x1
	.byte	0x23
	.4byte	0x2d5
	.byte	0x2
	.byte	0x91
	.sleb128 8
	.uleb128 0xe
	.4byte	$LC25
	.byte	0x1
	.byte	0x24
	.4byte	0x11c
	.byte	0x2
	.byte	0x91
	.sleb128 12
	.uleb128 0xf
	.4byte	0x340
	.byte	0x1
	.4byte	$LC88
	.byte	0x1
	.byte	0x25
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.uleb128 0xf
	.4byte	0x353
	.byte	0x1
	.4byte	$LC89
	.byte	0x1
	.byte	0x26
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.uleb128 0x11
	.byte	0x1
	.4byte	$LC91
	.byte	0x1
	.byte	0x2e
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.byte	0x0
	.uleb128 0x4
	.byte	0x4
	.4byte	0xf3
	.uleb128 0xb
	.4byte	0x3bf
	.byte	0x1
	.4byte	$LC90
	.byte	0x1
	.byte	0x4d
	.byte	0x1
	.4byte	$LFB2
	.4byte	$LFE2
	.4byte	$LSFDE2
	.byte	0x1
	.byte	0x6e
	.uleb128 0xc
	.4byte	$LC85
	.byte	0x1
	.byte	0x4c
	.4byte	0x363
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0xf
	.4byte	0x3a5
	.byte	0x1
	.4byte	$LC91
	.byte	0x1
	.byte	0x53
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.uleb128 0x12
	.4byte	$LBB4
	.4byte	$LBE4
	.uleb128 0x11
	.byte	0x1
	.4byte	$LC88
	.byte	0x1
	.byte	0x4f
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.byte	0x0
	.byte	0x0
	.uleb128 0xb
	.4byte	0x42c
	.byte	0x1
	.4byte	$LC92
	.byte	0x1
	.byte	0x64
	.byte	0x1
	.4byte	$LFB3
	.4byte	$LFE3
	.4byte	$LSFDE4
	.byte	0x1
	.byte	0x6e
	.uleb128 0xc
	.4byte	$LC85
	.byte	0x1
	.byte	0x63
	.4byte	0x363
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0x13
	.ascii	"add\000"
	.byte	0x1
	.byte	0x63
	.4byte	0x42c
	.byte	0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0xe
	.4byte	$LC93
	.byte	0x1
	.byte	0x65
	.4byte	0x11c
	.byte	0x2
	.byte	0x91
	.sleb128 8
	.uleb128 0x12
	.4byte	$LBB6
	.4byte	$LBE6
	.uleb128 0xe
	.4byte	$LC94
	.byte	0x1
	.byte	0x68
	.4byte	0x432
	.byte	0x2
	.byte	0x91
	.sleb128 16
	.uleb128 0x11
	.byte	0x1
	.4byte	$LC88
	.byte	0x1
	.byte	0x69
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.byte	0x0
	.byte	0x0
	.uleb128 0x4
	.byte	0x4
	.4byte	0x83
	.uleb128 0x14
	.4byte	0x442
	.4byte	0x29e
	.uleb128 0x15
	.4byte	0x442
	.byte	0x63
	.byte	0x0
	.uleb128 0xa
	.4byte	$LC95
	.byte	0x4
	.byte	0x7
	.uleb128 0x16
	.4byte	0x4b7
	.byte	0x1
	.4byte	$LC96
	.byte	0x1
	.byte	0x89
	.byte	0x1
	.4byte	0x2d5
	.4byte	$LFB4
	.4byte	$LFE4
	.4byte	$LSFDE6
	.byte	0x1
	.byte	0x6e
	.uleb128 0xc
	.4byte	$LC85
	.byte	0x1
	.byte	0x88
	.4byte	0x363
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0xc
	.4byte	$LC97
	.byte	0x1
	.byte	0x88
	.4byte	0xba
	.byte	0x2
	.byte	0x91
	.sleb128 8
	.uleb128 0xc
	.4byte	$LC98
	.byte	0x1
	.byte	0x88
	.4byte	0x42c
	.byte	0x2
	.byte	0x91
	.sleb128 16
	.uleb128 0xd
	.ascii	"e\000"
	.byte	0x1
	.byte	0x8a
	.4byte	0x7d
	.byte	0x2
	.byte	0x91
	.sleb128 20
	.uleb128 0x12
	.4byte	$LBB8
	.4byte	$LBE8
	.uleb128 0xe
	.4byte	$LC93
	.byte	0x1
	.byte	0x8d
	.4byte	0x11c
	.byte	0x2
	.byte	0x91
	.sleb128 24
	.byte	0x0
	.byte	0x0
	.uleb128 0xb
	.4byte	0x530
	.byte	0x1
	.4byte	$LC99
	.byte	0x1
	.byte	0xb2
	.byte	0x1
	.4byte	$LFB5
	.4byte	$LFE5
	.4byte	$LSFDE8
	.byte	0x1
	.byte	0x6e
	.uleb128 0xc
	.4byte	$LC85
	.byte	0x1
	.byte	0xb1
	.4byte	0x363
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0xc
	.4byte	$LC100
	.byte	0x1
	.byte	0xb1
	.4byte	0x293
	.byte	0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0xd
	.ascii	"e\000"
	.byte	0x1
	.byte	0xb3
	.4byte	0x7d
	.byte	0x2
	.byte	0x91
	.sleb128 8
	.uleb128 0x12
	.4byte	$LBB10
	.4byte	$LBE10
	.uleb128 0xd
	.ascii	"pr\000"
	.byte	0x1
	.byte	0xb6
	.4byte	0x432
	.byte	0x2
	.byte	0x91
	.sleb128 16
	.uleb128 0xe
	.4byte	$LC93
	.byte	0x1
	.byte	0xb7
	.4byte	0x11c
	.byte	0x3
	.byte	0x91
	.sleb128 128
	.uleb128 0x11
	.byte	0x1
	.4byte	$LC101
	.byte	0x1
	.byte	0xb8
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.byte	0x0
	.byte	0x0
	.uleb128 0xb
	.4byte	0x569
	.byte	0x1
	.4byte	$LC102
	.byte	0x1
	.byte	0xc8
	.byte	0x1
	.4byte	$LFB6
	.4byte	$LFE6
	.4byte	$LSFDE10
	.byte	0x1
	.byte	0x6e
	.uleb128 0xc
	.4byte	$LC85
	.byte	0x1
	.byte	0xc7
	.4byte	0x42c
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0x11
	.byte	0x1
	.4byte	$LC91
	.byte	0x1
	.byte	0xc9
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.byte	0x0
	.uleb128 0xb
	.4byte	0x5a2
	.byte	0x1
	.4byte	$LC103
	.byte	0x1
	.byte	0xd7
	.byte	0x1
	.4byte	$LFB7
	.4byte	$LFE7
	.4byte	$LSFDE12
	.byte	0x1
	.byte	0x6e
	.uleb128 0xc
	.4byte	$LC85
	.byte	0x1
	.byte	0xd6
	.4byte	0x42c
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0x11
	.byte	0x1
	.4byte	$LC91
	.byte	0x1
	.byte	0xd9
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.byte	0x0
	.uleb128 0xb
	.4byte	0x5f7
	.byte	0x1
	.4byte	$LC104
	.byte	0x1
	.byte	0xe8
	.byte	0x1
	.4byte	$LFB8
	.4byte	$LFE8
	.4byte	$LSFDE14
	.byte	0x1
	.byte	0x6e
	.uleb128 0xc
	.4byte	$LC85
	.byte	0x1
	.byte	0xe7
	.4byte	0x42c
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0xc
	.4byte	$LC100
	.byte	0x1
	.byte	0xe7
	.4byte	0x293
	.byte	0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0xe
	.4byte	$LC94
	.byte	0x1
	.byte	0xe9
	.4byte	0x432
	.byte	0x2
	.byte	0x91
	.sleb128 16
	.uleb128 0x11
	.byte	0x1
	.4byte	$LC88
	.byte	0x1
	.byte	0xeb
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.byte	0x0
	.uleb128 0x16
	.4byte	0x650
	.byte	0x1
	.4byte	$LC105
	.byte	0x1
	.byte	0xfa
	.byte	0x1
	.4byte	0x293
	.4byte	$LFB9
	.4byte	$LFE9
	.4byte	$LSFDE16
	.byte	0x1
	.byte	0x6e
	.uleb128 0xc
	.4byte	$LC85
	.byte	0x1
	.byte	0xf9
	.4byte	0x42c
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0xc
	.4byte	$LC94
	.byte	0x1
	.byte	0xf9
	.4byte	0x650
	.byte	0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0xc
	.4byte	$LC106
	.byte	0x1
	.byte	0xf9
	.4byte	0x2d5
	.byte	0x2
	.byte	0x91
	.sleb128 8
	.uleb128 0x11
	.byte	0x1
	.4byte	$LC107
	.byte	0x1
	.byte	0xfb
	.4byte	0x2d5
	.byte	0x1
	.uleb128 0x10
	.byte	0x0
	.byte	0x0
	.uleb128 0x4
	.byte	0x4
	.4byte	0x29e
	.uleb128 0x17
	.ascii	"u8\000"
	.byte	0x7
	.byte	0xd
	.4byte	0x660
	.uleb128 0xa
	.4byte	$LC108
	.byte	0x1
	.byte	0x8
	.uleb128 0x17
	.ascii	"u16\000"
	.byte	0x7
	.byte	0xe
	.4byte	0x672
	.uleb128 0xa
	.4byte	$LC109
	.byte	0x2
	.byte	0x7
	.uleb128 0x17
	.ascii	"u32\000"
	.byte	0x7
	.byte	0xf
	.4byte	0x684
	.uleb128 0xa
	.4byte	$LC95
	.byte	0x4
	.byte	0x7
	.uleb128 0x17
	.ascii	"u64\000"
	.byte	0x7
	.byte	0x10
	.4byte	0x696
	.uleb128 0xa
	.4byte	$LC110
	.byte	0x8
	.byte	0x7
	.uleb128 0x17
	.ascii	"s8\000"
	.byte	0x7
	.byte	0x16
	.4byte	0x6a7
	.uleb128 0xa
	.4byte	$LC111
	.byte	0x1
	.byte	0x6
	.uleb128 0x17
	.ascii	"s16\000"
	.byte	0x7
	.byte	0x17
	.4byte	0x6b9
	.uleb128 0xa
	.4byte	$LC112
	.byte	0x2
	.byte	0x5
	.uleb128 0x17
	.ascii	"s32\000"
	.byte	0x7
	.byte	0x18
	.4byte	0x2d5
	.uleb128 0x17
	.ascii	"s64\000"
	.byte	0x7
	.byte	0x19
	.4byte	0x6d6
	.uleb128 0xa
	.4byte	$LC113
	.byte	0x8
	.byte	0x5
	.uleb128 0x18
	.4byte	$LC114
	.byte	0x2
	.byte	0x8
	.4byte	0x25
	.uleb128 0x18
	.4byte	$LC115
	.byte	0x2
	.byte	0xd
	.4byte	0x54
	.uleb128 0x18
	.4byte	$LC116
	.byte	0x3
	.byte	0xa
	.4byte	0x83
	.uleb128 0x18
	.4byte	$LC117
	.byte	0x3
	.byte	0xf
	.4byte	0xca
	.uleb128 0x18
	.4byte	$LC118
	.byte	0x3
	.byte	0x14
	.4byte	0xf3
	.uleb128 0x18
	.4byte	$LC119
	.byte	0x4
	.byte	0x1b
	.4byte	0x122
	.uleb128 0x18
	.4byte	$LC120
	.byte	0x5
	.byte	0x35
	.4byte	0x177
	.uleb128 0x18
	.4byte	$LC121
	.byte	0x5
	.byte	0x4f
	.4byte	0x25c
	.uleb128 0x18
	.4byte	$LC122
	.byte	0x6
	.byte	0xc
	.4byte	0x2a5
	.uleb128 0xd
	.ascii	"EIO\000"
	.byte	0x6
	.byte	0x17
	.4byte	0x751
	.byte	0x5
	.byte	0x3
	.4byte	EIO
	.uleb128 0x9
	.4byte	0x2d5
	.uleb128 0xe
	.4byte	$LC123
	.byte	0x6
	.byte	0x18
	.4byte	0x751
	.byte	0x5
	.byte	0x3
	.4byte	ENOMEM
	.uleb128 0xe
	.4byte	$LC124
	.byte	0x6
	.byte	0x19
	.4byte	0x751
	.byte	0x5
	.byte	0x3
	.4byte	ENODEV
	.uleb128 0xe
	.4byte	$LC125
	.byte	0x6
	.byte	0x1a
	.4byte	0x751
	.byte	0x5
	.byte	0x3
	.4byte	EINVAL
	.uleb128 0xe
	.4byte	$LC126
	.byte	0x6
	.byte	0x1b
	.4byte	0x751
	.byte	0x5
	.byte	0x3
	.4byte	EROFS
	.uleb128 0xe
	.4byte	$LC127
	.byte	0x6
	.byte	0x1c
	.4byte	0x751
	.byte	0x5
	.byte	0x3
	.4byte	EPIPE
	.uleb128 0xe
	.4byte	$LC128
	.byte	0x6
	.byte	0x1d
	.4byte	0x751
	.byte	0x5
	.byte	0x3
	.4byte	EBADE
	.uleb128 0xe
	.4byte	$LC129
	.byte	0x6
	.byte	0x1e
	.4byte	0x751
	.byte	0x5
	.byte	0x3
	.4byte	ENOSYS
	.uleb128 0xe
	.4byte	$LC130
	.byte	0x1
	.byte	0x14
	.4byte	0x72a
	.byte	0x5
	.byte	0x3
	.4byte	thisModule
	.byte	0x0
	.section	.debug_abbrev
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x10
	.uleb128 0x6
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.byte	0x0
	.byte	0x0
	.uleb128 0x2
	.uleb128 0x13
	.byte	0x1
	.uleb128 0x1
	.uleb128 0x13
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.byte	0x0
	.byte	0x0
	.uleb128 0x3
	.uleb128 0xd
	.byte	0x0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xa
	.byte	0x0
	.byte	0x0
	.uleb128 0x4
	.uleb128 0xf
	.byte	0x0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0x0
	.byte	0x0
	.uleb128 0x5
	.uleb128 0x24
	.byte	0x0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.byte	0x0
	.byte	0x0
	.uleb128 0x6
	.uleb128 0xf
	.byte	0x0
	.uleb128 0xb
	.uleb128 0xb
	.byte	0x0
	.byte	0x0
	.uleb128 0x7
	.uleb128 0x4
	.byte	0x1
	.uleb128 0x1
	.uleb128 0x13
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.byte	0x0
	.byte	0x0
	.uleb128 0x8
	.uleb128 0x28
	.byte	0x0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xb
	.byte	0x0
	.byte	0x0
	.uleb128 0x9
	.uleb128 0x26
	.byte	0x0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0x0
	.byte	0x0
	.uleb128 0xa
	.uleb128 0x24
	.byte	0x0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.byte	0x0
	.byte	0x0
	.uleb128 0xb
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x1
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x2001
	.uleb128 0x6
	.uleb128 0x40
	.uleb128 0xa
	.byte	0x0
	.byte	0x0
	.uleb128 0xc
	.uleb128 0x5
	.byte	0x0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0x0
	.byte	0x0
	.uleb128 0xd
	.uleb128 0x34
	.byte	0x0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0x0
	.byte	0x0
	.uleb128 0xe
	.uleb128 0x34
	.byte	0x0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0x0
	.byte	0x0
	.uleb128 0xf
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x1
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0x0
	.byte	0x0
	.uleb128 0x10
	.uleb128 0x18
	.byte	0x0
	.byte	0x0
	.byte	0x0
	.uleb128 0x11
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0x0
	.byte	0x0
	.uleb128 0x12
	.uleb128 0xb
	.byte	0x1
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.byte	0x0
	.byte	0x0
	.uleb128 0x13
	.uleb128 0x5
	.byte	0x0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0x0
	.byte	0x0
	.uleb128 0x14
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x1
	.uleb128 0x13
	.uleb128 0x49
	.uleb128 0x13
	.byte	0x0
	.byte	0x0
	.uleb128 0x15
	.uleb128 0x21
	.byte	0x0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0x0
	.byte	0x0
	.uleb128 0x16
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x1
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x2001
	.uleb128 0x6
	.uleb128 0x40
	.uleb128 0xa
	.byte	0x0
	.byte	0x0
	.uleb128 0x17
	.uleb128 0x16
	.byte	0x0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0x0
	.byte	0x0
	.uleb128 0x18
	.uleb128 0x16
	.byte	0x0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0x0
	.byte	0x0
	.byte	0x0
	.section	.debug_pubnames,"",@progbits
	.4byte	0xff
	.2byte	0x2
	.4byte	$Ldebug_info0
	.4byte	0x7df
	.4byte	0x2dc
	.ascii	"MioBufferCacheInit\000"
	.4byte	0x369
	.ascii	"MioBufferCacheTerm\000"
	.4byte	0x3bf
	.ascii	"MioBufferCacheAddEntry\000"
	.4byte	0x449
	.ascii	"MioBufferCacheContains\000"
	.4byte	0x4b7
	.ascii	"MioBufferCachePrint\000"
	.4byte	0x530
	.ascii	"MioBufferCacheEntryInit\000"
	.4byte	0x569
	.ascii	"MioBufferCacheEntryTerm\000"
	.4byte	0x5a2
	.ascii	"MioBufferCacheEntryPrint\000"
	.4byte	0x5f7
	.ascii	"MioBufferCacheEntryAsString\000"
	.4byte	0x0
	.section	.debug_aranges,"",@progbits
	.4byte	0x1c
	.2byte	0x2
	.4byte	$Ldebug_info0
	.byte	0x4
	.byte	0x0
	.2byte	0x0
	.2byte	0x0
	.4byte	$Ltext0
	.4byte	$Letext0-$Ltext0
	.4byte	0x0
	.4byte	0x0
	.section	.debug_str,"MS",@progbits,1
$LC79:
	.ascii	"featureIDs\000"
$LC44:
	.ascii	"MIO_MODULE_SECURITY_CC\000"
$LC118:
	.ascii	"MioBufferCache\000"
$LC37:
	.ascii	"MIO_FEATURE_MAX\000"
$LC26:
	.ascii	"MIO_FEATURE_SLOADER\000"
$LC61:
	.ascii	"MIO_MODULE_SFF8070I_C\000"
$LC72:
	.ascii	"MIO_MODULE_PSEUDOPROCS_CC\000"
$LC109:
	.ascii	"short unsigned int\000"
$LC97:
	.ascii	"userAddress\000"
$LC108:
	.ascii	"unsigned char\000"
$LC41:
	.ascii	"MIO_MODULE_ELFLOADER_CC\000"
$LC52:
	.ascii	"MIO_MODULE_USBIO_C\000"
$LC49:
	.ascii	"MIO_MODULE_USBENDPOINT_C\000"
$LC56:
	.ascii	"MIO_MODULE_DBLIST_C\000"
$LC43:
	.ascii	"MIO_MODULE_SIGNING_CC\000"
$LC66:
	.ascii	"MIO_MODULE_UTILS_C\000"
$LC105:
	.ascii	"MioBufferCacheEntryAsString\000"
$LC125:
	.ascii	"EINVAL\000"
$LC18:
	.ascii	"ramAddress\000"
$LC115:
	.ascii	"MioDoublyLinkedList\000"
$LC22:
	.ascii	"entry\000"
$LC30:
	.ascii	"MIO_FEATURE_VIDEO_PLAYER\000"
$LC90:
	.ascii	"MioBufferCacheTerm\000"
$LC63:
	.ascii	"MIO_MODULE_STORAGE_C\000"
$LC23:
	.ascii	"_MioBufferCache\000"
$LC53:
	.ascii	"MIO_MODULE_BCACHE_C\000"
$LC85:
	.ascii	"this\000"
$LC87:
	.ascii	"MioBufferCacheInit\000"
$LC33:
	.ascii	"MIO_FEATURE_USBMS\000"
$LC106:
	.ascii	"bufferSize\000"
$LC83:
	.ascii	"whence\000"
$LC94:
	.ascii	"buffer\000"
$LC39:
	.ascii	"_MioModuleID\000"
$LC107:
	.ascii	"snprintf\000"
$LC114:
	.ascii	"MioDoubleLink\000"
$LC40:
	.ascii	"MIO_MODULE_EXECVE_CC\000"
$LC35:
	.ascii	"MIO_FEATURE_USB\000"
$LC124:
	.ascii	"ENODEV\000"
$LC51:
	.ascii	"MIO_MODULE_USBINTERFACE_C\000"
$LC101:
	.ascii	"sprintf\000"
$LC46:
	.ascii	"MIO_MODULE_STAT_CC\000"
$LC60:
	.ascii	"MIO_MODULE_SEMA_C\000"
$LC34:
	.ascii	"MIO_FEATURE_FATFS\000"
$LC110:
	.ascii	"long unsigned int\000"
$LC102:
	.ascii	"MioBufferCacheEntryInit\000"
$LC78:
	.ascii	"moduleName\000"
$LC131:
	.ascii	"bcache.c\000"
$LC67:
	.ascii	"MIO_MODULE_UTIL_FUNCS_C\000"
$LC77:
	.ascii	"moduleID\000"
$LC71:
	.ascii	"MIO_MODULE_RMS_CC\000"
$LC25:
	.ascii	"pool\000"
$LC129:
	.ascii	"ENOSYS\000"
$LC127:
	.ascii	"EPIPE\000"
$LC13:
	.ascii	"_MioDoublyLinkedList\000"
$LC32:
	.ascii	"MIO_FEATURE_IMAGEVIEWER\000"
$LC12:
	.ascii	"_MioDoubleLink\000"
$LC76:
	.ascii	"_MioModule\000"
$LC112:
	.ascii	"short int\000"
$LC38:
	.ascii	"_MioFeatureID\000"
$LC91:
	.ascii	"bzero\000"
$LC84:
	.ascii	"long long int\000"
$LC54:
	.ascii	"MIO_MODULE_CBW_C\000"
$LC116:
	.ascii	"MioBufferCacheEntry\000"
$LC15:
	.ascii	"tail\000"
$LC24:
	.ascii	"list\000"
$LC126:
	.ascii	"EROFS\000"
$LC120:
	.ascii	"MioModuleID\000"
$LC103:
	.ascii	"MioBufferCacheEntryTerm\000"
$LC17:
	.ascii	"externalByteAddress\000"
$LC80:
	.ascii	"char\000"
$LC57:
	.ascii	"MIO_MODULE_FAT_C\000"
$LC31:
	.ascii	"MIO_FEATURE_LOGIN\000"
$LC132:
	.ascii	"/home/broadq/builds/08-26-2004/ps2-build/netdisk\000"
$LC75:
	.ascii	"MIO_MODULE_MAX\000"
$LC121:
	.ascii	"MioModule\000"
$LC36:
	.ascii	"MIO_FEATURE_STORAGE\000"
$LC62:
	.ascii	"MIO_MODULE_SPC2_C\000"
$LC128:
	.ascii	"EBADE\000"
$LC45:
	.ascii	"MIO_MODULE_VFORK_CC\000"
$LC92:
	.ascii	"MioBufferCacheAddEntry\000"
$LC59:
	.ascii	"MIO_MODULE_SCSI2_C\000"
$LC55:
	.ascii	"MIO_MODULE_CSW_C\000"
$LC47:
	.ascii	"MIO_MODULE_UPDATE_CC\000"
$LC21:
	.ascii	"links\000"
$LC88:
	.ascii	"printf\000"
$LC86:
	.ascii	"cacheSize\000"
$LC96:
	.ascii	"MioBufferCacheContains\000"
$LC27:
	.ascii	"MIO_FEATURE_PLAYLIST\000"
$LC42:
	.ascii	"MIO_MODULE_SIMPLE_FILEIO_CC\000"
$LC69:
	.ascii	"MIO_MODULE_MEDIO_CONFIG_CC\000"
$LC119:
	.ascii	"MioFeatureID\000"
$LC111:
	.ascii	"signed char\000"
$LC50:
	.ascii	"MIO_MODULE_USBCONFIG_C\000"
$LC73:
	.ascii	"MIO_MODULE_EVENT_BROADCASTER_CC\000"
$LC29:
	.ascii	"MIO_FEATURE_OGG_PLAYER\000"
$LC11:
	.ascii	"next\000"
$LC95:
	.ascii	"unsigned int\000"
$LC16:
	.ascii	"_MioBufferCacheEntry\000"
$LC130:
	.ascii	"thisModule\000"
$LC133:
	.ascii	"GNU C 3.2.2 -g\000"
$LC68:
	.ascii	"MIO_MODULE_DELIMITTED_BUFFER_CC\000"
$LC20:
	.ascii	"_MioBufferCacheListEntry\000"
$LC19:
	.ascii	"blockSize\000"
$LC98:
	.ascii	"found\000"
$LC123:
	.ascii	"ENOMEM\000"
$LC48:
	.ascii	"MIO_MODULE_PS2CLOCK_CC\000"
$LC28:
	.ascii	"MIO_FEATURE_MP3_PLAYER\000"
$LC65:
	.ascii	"MIO_MODULE_UFI_C\000"
$LC74:
	.ascii	"MIO_MODULE_ROMFS_C\000"
$LC10:
	.ascii	"prev\000"
$LC14:
	.ascii	"head\000"
$LC82:
	.ascii	"offset\000"
$LC104:
	.ascii	"MioBufferCacheEntryPrint\000"
$LC89:
	.ascii	"DelayThread\000"
$LC64:
	.ascii	"MIO_MODULE_TRACEFS_C\000"
$LC113:
	.ascii	"long int\000"
$LC81:
	.ascii	"_xseek\000"
$LC70:
	.ascii	"MIO_MODULE_MEDIO_MAIN_CC\000"
$LC99:
	.ascii	"MioBufferCachePrint\000"
$LC93:
	.ascii	"object\000"
$LC117:
	.ascii	"MioBufferCacheListEntry\000"
$LC122:
	.ascii	"xseek_t\000"
$LC100:
	.ascii	"prefix\000"
$LC58:
	.ascii	"MIO_MODULE_FATMAN_C\000"
	.ident	"GCC: (GNU) 3.2.2"
