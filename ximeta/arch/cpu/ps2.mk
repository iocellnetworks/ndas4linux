XPLAT_CONFIG_ENDIAN_BYTEORDER:=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD:=LITTLE
PS2SDK ?=/usr/local/ps2-03-28-2004
PS2LIB ?=/usr/local/ps2lib-2.1

NDAS_CROSS_COMPILE:=ps2-
ELF2IRX:= $(PS2LIB)/iop/utils/elf2irx/elf2irx

nxp-cflags:= -G0 -fno-builtin -miop -msoft-float \
                     -I$(PS2LIB)/iop/include -I$(PS2LIB)/common/include \
					-D_IOP -D_PS2
XPLAT_EXTRA_LDFLAGS:= -L$(PS2LIB)/iop/lib

export PS2SDK PS2LIB
