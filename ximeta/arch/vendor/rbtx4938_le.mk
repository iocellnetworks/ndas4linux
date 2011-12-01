NDAS_KERNEL_PATH:=/opt/montavista/previewkit/lsp/toshiba-rbhma4500-mips_fp_le-previewkit/linux-2.4.20_mvl31

RBTX4938_HEADER_INC= -I$(NDAS_KERNEL_PATH)/include \
	-D__KERNEL__ -DMODULE

rbtx-build-num = "\"rbtx 1\""
nxp-cflags += -O2 -DRBTX4938 $(RBTX4938_HEADER_INC) \
	-fno-builtin -nostdlib \
	-ffunction-sections -fdata-sections \
	-fno-strict-aliasing -fno-common -G 0 \
	-fomit-frame-pointer -mno-abicalls \
	-fno-pic -pipe -mabi=32 -march=r4600 \
	-Wa,-32 -Wa,-march=r4600 -Wa,-mips3 -Wa,--trap -mlong-calls \
	-DNDAS_DRV_BUILD=$(rbtx-build-num) 

NDAS_CROSS_COMPILE=/opt/montavista/previewkit/mips/fp_le/bin/mips_fp_le-
NDAS_CROSS_COMPILE=/opt/montavista/previewkit/mips/fp_le/bin/mips_fp_le-

ifneq ($(nxpo-debug),y)
endif

