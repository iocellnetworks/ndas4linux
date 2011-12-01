nxp-cflags += -D__KERNEL__ -DMODULE \
	-Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fno-common \
	-pipe -fno-builtin -D__linux__ -DNO_MM -mapcs-32 -march=armv4 -mtune=arm7tdmi -mshort-load-bytes\
	-msoft-float  -iwithprefix -nostdlib # -nostdinc

# Use if nxp-embedded option is on
nxp-cflags += -DMODVERSIONS -DEXPORT_SYMTAB -DMODULE -DLINUX -D__KERNEL__ -I$(NDAS_KERNEL_PATH)/include 

# This value will be used when tarball.mk builds complete or ipkg rule.
nxp-public-cflags:=-D_ARM -DLINUX -DNDAS_EM86XX

NDAS_EXTRA_LDFLAGS:=$(NDAS_EXTRA_LDFLAGS) -elf2flt="-s32768"

