#
# Linudix Network Camera
#
NDAS_CROSS_COMPILE?=/home/sjcho/linudix/hardhat/previewkit/mipsel/fp_le/bin/mipsel_fp_le-
#NDAS_CROSS_COMPILE?=/opt/montavista/previewkit/mipsel/fp_le/bin/mipsel_fp_le-
NDAS_CROSS_COMPILE_UM?=/home/sjcho/linudix/hardhat/previewkit/mipsel/fp_le/bin/mipsel_fp_le-
#NDAS_CROSS_COMPILE_UM?=/opt/montavista/previewkit/mipsel/fp_le/bin/mipsel_fp_le-
NDAS_EXTRA_CLFAGS+="-DNDAS_SIGPENDING_OLD -DLINUX -D_MIPSEL -mlong-calls -DNDAS_IO_UNIT=16 -DNDAS_SERIAL"
#nxp-cflags+= -pipe -Wa,--trap -Wa,-32 -Wa,-mips32 -fno-delayed-branch -mlong-calls
nxp-cflags+=-Wstrict-prototypes -fno-strict-aliasing -fno-common -fomit-frame-pointer -G 0 -mno-abicalls -fno-pic -pipe -mcpu=r4600 -mips2 -Wa,--trap   -nostdinc -mlong-calls


