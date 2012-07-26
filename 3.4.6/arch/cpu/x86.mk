XPLAT_CONFIG_ENDIAN_BYTEORDER=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD=LITTLE

nxp-cflags=-D_X86 \
	-minline-all-stringops -msoft-float \
	-fno-stack-protector  # gcc from some distribution has this flag on by default

