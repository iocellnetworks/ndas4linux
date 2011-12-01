KERNEL_PATH=/root/src/mvl31_tx49be
RBTX4938_HEADER_INC= -I$(KERNEL_PATH)/include \
	-I$(KERNEL_PATH)/include/asm-mipsel/mach-tx \
	-I$(KERNEL_PATH)/include/asm-mipsel/mach-generic \
	-D__KERNEL__ -DMODULE

rbtx-build-num = "\"rbtx 1\""
nxp-cflags += -G 0 -O2 $(RBTX4938_HEADER_INC) \
	-fno-builtin -nostdlib \
	-ffunction-sections -fdata-sections \
	-fno-strict-aliasing -fno-common \
	-fomit-frame-pointer -mno-abicalls \
	-fno-pic -pipe -mabi=32 -march=r4600 \
	-Wa,-32 -Wa,-march=r4600 -Wa,-mipsel3 -Wa,--trap -mlong-calls \
	-DNDAS_DRV_BUILD=$(rbtx-build-num) 

ifneq ($(nxpo-debug),y)
endif

