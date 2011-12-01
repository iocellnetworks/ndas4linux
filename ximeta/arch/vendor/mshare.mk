MSHARE_CFLAGS= -D__KERNEL__ -DMODULE -DNDAS_MSHARE

nxp-cflags += $(MSHARE_CFLAGS) \
	-Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fno-common \
	-pipe -fno-builtin -D__linux__ -DNO_MM -mapcs-32 -march=armv4 -mtune=arm7tdmi -mshort-load-bytes\
	-msoft-float  -iwithprefix -nostdlib -I$(NDAS_KERNEL_PATH)/include # -nostdinc

nxp-public-cflags:=-D_ARM -DLINUX -DNDAS_MSHARE

NDAS_EXTRA_LDFLAGS:=$(NDAS_EXTRA_LDFLAGS) -elf2flt="-s32768"
