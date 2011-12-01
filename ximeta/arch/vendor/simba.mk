#
# Linksys WRT54G series
#
NDAS_CROSS_COMPILE?=/opt/hardhat-simba/devkit/mips/lexra_fp_be/bin/mips_lexra_fp_be-
NDAS_CROSS_COMPILE_UM?=/opt/hardhat-simba/devkit/mips/lexra_fp_be/bin/mips_lexra_fp_be-
NDAS_EXTRA_CLFAGS+="-DLINUX -D_MIPS -mlong-calls"
nxp-cflags+= -fomit-frame-pointer -fno-strict-aliasing -fno-common -G 0 -mno-abicalls -fno-pic -mips1  -pipe 

