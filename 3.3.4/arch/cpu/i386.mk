XPLAT_CONFIG_ENDIAN_BYTEORDER=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD=LITTLE
nxp-cflags=-D_X86 \
	-mpreferred-stack-boundary=2 \
	-minline-all-stringops \
	-mregparm=3 \
	-msoft-float

