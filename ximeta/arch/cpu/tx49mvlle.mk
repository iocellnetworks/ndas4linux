XPLAT_CONFIG_ENDIAN_BYTEORDER=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD=LITTLE

nxp-cflags:= -D_MIPS

NDAS_CROSS_COMPILE=mips_fp_le-
NDAS_CROSS_COMPILE=mips_fp_le-

ifeq ($(nxp-os),linux)
nxp-cflags+= -mlong-calls
endif

