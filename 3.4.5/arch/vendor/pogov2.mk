# -----------------------------------------------------------------------------
# Copyright (c) IOCELL Networks Corp., Plainsboro NJ 08536, USA
# All rights reserved. 
# -----------------------------------------------------------------------------

# This worked to export a build folder that could be patched and run on the 
# PogoPlug Version 2

nxp-cflags += -D__KERNEL__ -DMODULE \
	-Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing \
	-fno-common -fno-common -pipe -fno-builtin -D__linux__ -DNO_MM -mapcs \
	-march=armv5te -mtune=arm7tdmi -msoft-float  -iwithprefix -nostdlib \
	# -nostdinc

# Use if nxp-embedded option is on
nxp-cflags += -DMODVERSIONS -DEXPORT_SYMTAB -DMODULE -DLINUX -D__KERNEL__ \
	-I$(NDAS_KERNEL_PATH)/include 

# This value will be used when tarball.mk builds complete or ipkg rule.
nxp-public-cflags:=-D_ARM -DLINUX 
