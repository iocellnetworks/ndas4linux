ifneq ($(nxpo-ndaslinux),y)


SYABAS_SDK=$(nxp-root)/../Syabas3_Buffalo_sdk
SYABAS_ARM_HEADER_INC=-I$(SYABAS_SDK) -I$(SYABAS_SDK)/os -I$(SYABAS_SDK)/tcpip/include -I/usr/local/arm-elf.1.10.0/include -I/opt/tools-install/lib/gcc-lib/arm-elf/2.95.3/include
syabas-build-num = "\"syabas 8\""
nxp-cflags += -O2 -DSYABAS $(SYABAS_ARM_HEADER_INC) \
	-fno-builtin -nostdinc -nostdlib \
	-ffunction-sections -fdata-sections \
	-DNDAS_DRV_BUILD=$(syabas-build-num)
	
NDAS_CROSS_COMPILE:=/opt/tools-install/bin/arm-elf-

ifneq ($(nxpo-debug),y)
nxp-cflags += -fomit-frame-pointer
endif

else

nxp-cflags += -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fno-common -pipe -fno-builtin -D__linux__ -DNO_MM -mapcs-32 -march=armv4 -mtune=arm7tdmi -mshort-load-bytes -msoft-float -iwithprefix


endif
