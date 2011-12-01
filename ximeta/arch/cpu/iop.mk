XPLAT_CONFIG_ENDIAN_BYTEORDER:=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD:=LITTLE
PS2SDK ?=/usr/local/ps2dev/ps2sdk
PS2LIB ?=/usr/local/ps2dev/ps2lib

NDAS_CROSS_COMPILE:=iop-
ELF2IRX:= $(PS2LIB)/iop/utils/elf2irx/elf2irx

nxp-cflags:= -G0 -fno-builtin -miop -msoft-float \
					-I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include \
					-D_IOP
