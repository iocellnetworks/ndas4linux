DVICO_CFLAGS= -D__KERNEL__ -DMODULE

nxp-cflags += $(DVICO_CFLAGS) \
	-Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fno-common \
	-pipe -fno-builtin -D__linux__ -DNO_MM -mapcs-32 -march=armv4 -mtune=arm7tdmi -mshort-load-bytes\
	-msoft-float  -iwithprefix -nostdlib # -nostdinc

nxp-public-cflags:=-D_ARM -DLINUX

NDAS_EXTRA_LDFLAGS:=$(NDAS_EXTRA_LDFLAGS) -elf2flt="-s32768"
