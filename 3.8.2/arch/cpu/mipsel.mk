XPLAT_CONFIG_ENDIAN_BYTEORDER=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD=LITTLE

nxp-cflags:= -D_MIPSEL -DBUG_ALIGNMENT 

ifeq ($(nxp-os),linux)
nxp-cflags+=-G 0 -mno-abicalls -fno-pic -mlong-calls -Wa,--trap -msoft-float
#nxp-cflags+= -G 0 -mno-abicalls -mcpu=r4600 -mips2 -Wa,--trap -mlong-calls -fno-pic
endif

