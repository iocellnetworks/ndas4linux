#SYABAS_SDK=$(nxp-root)/SYABAS3_SDK
#SYABAS_ARM_HEADER_INC=-I$(SYABAS_SDK) -I$(SYABAS_SDK)/os -I$(SYABAS_SDK)/tcpip/include -I/usr/local/arm-elf.1.10.0/include -I/opt/tools-install/lib/gcc-lib/arm-elf/2.95.3/include
METABRAIN_HEADER_INC=
metabrain-build-num = "\"metabrain 1\""
nxp-cflags += -O2 -DMETABRAIN $(METABRAIN_HEADER_INC) \
	-fno-builtin -nostdinc -nostdlib \
	-ffunction-sections -fdata-sections \
	-fno-strict-aliasing -fno-common -G 0 \
	-fomit-frame-pointer -mno-abicalls \
	-fno-pic -pipe -mabi=32 -march=r4600 \
	-Wa,-32 -Wa,-march=r4600 -Wa,-mips3 -Wa,--trap -mlong-calls \
	-DNDAS_DRV_BUILD=$(metabrain-build-num) 

NDAS_CROSS_COMPILE=/opt/crosstool/mips-unknown-linux-gnu/gcc-3.4.3-glibc-2.3.2/bin/mips-unknown-linux-gnu-
NDAS_CROSS_COMPILE=/opt/crosstool/mips-unknown-linux-gnu/gcc-3.4.3-glibc-2.3.2/bin/mips-unknown-linux-gnu-

ifneq ($(nxpo-debug),y)
endif

