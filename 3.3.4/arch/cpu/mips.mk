XPLAT_CONFIG_ENDIAN_BYTEORDER=BIG
XPLAT_CONFIG_ENDIAN_BITFIELD=BIG

nxp-cflags:= -D_MIPS -DBUG_ALIGNMENT 

ifeq ($(nxp-os),linux)
nxp-cflags+=-mno-abicalls -fno-pic -msoft-float
endif

