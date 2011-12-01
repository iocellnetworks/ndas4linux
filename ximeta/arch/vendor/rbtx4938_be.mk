RBTX4938_HEADER_INC= -I/root/src/linux-2.6.10-r4k/build_be/include \
	-I/root/src/linux-2.6.10-r4k/build_be/include2 \
	-I/root/src/linux-2.6.10-r4k/include \
	-I/root/src/linux-2.6.10-r4k/include/asm-mips/mach-tx \
	-I/root/src/linux-2.6.10-r4k/include/asm-mips/mach-generic \
	-D__KERNEL__ -DMODULE

rbtx-build-num = "\"rbtx 1\""
nxp-cflags += -O2 $(RBTX4938_HEADER_INC) \
	-fno-builtin -nostdlib \
	-ffunction-sections -fdata-sections \
	-fno-strict-aliasing -fno-common -G 0 \
	-fomit-frame-pointer -mno-abicalls \
	-fno-pic -pipe -mabi=32 -march=r4600 \
	-Wa,-32 -Wa,-march=r4600 -Wa,-mips3 -Wa,--trap -mlong-calls \
	-DNDAS_DRV_BUILD=$(rbtx-build-num) 

ifneq ($(nxpo-debug),y)
endif

