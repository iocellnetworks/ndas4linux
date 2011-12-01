nxp-cflags += -pipe -Wa,--trap -Wa,-32 -Wa,-march=mips32 -Wa,-mips32 -fno-delayed-branch -mlong-calls 

# Use if nxp-embedded option is on
nxp-cflags += -DMODVERSIONS -DEXPORT_SYMTAB -DMODULE -DLINUX -D__KERNEL__ -I$(NDAS_KERNEL_PATH)/include 

# This value will be used when tarball.mk builds complete or ipkg rule.
nxp-public-cflags:=-D_MIPSEL -DLINUX -DNDAS_MEMBLK_NO_PRIVATE

NDAS_EXTRA_LDFLAGS:=$(NDAS_EXTRA_LDFLAGS)

