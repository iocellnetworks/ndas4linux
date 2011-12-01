XPLAT_CONFIG_ENDIAN_BYTEORDER=BIG
XPLAT_CONFIG_ENDIAN_BITFIELD=BIG

nxp-cflags:= -D_MIPS

NDAS_CROSS_COMPILE=mips_fp_be-
NDAS_CROSS_COMPILE=mips_fp_be-

ifeq ($(nxp-os),linux)
nxp-cflags+= -mlong-calls
endif

