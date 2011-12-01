#
# Linksys WRT54G series
#
NDAS_CROSS_COMPILE?=/opt/wischip/mipsel-linux-gnu/gcc-3.3.4-glibc-2.3.2/bin/mipsel-linux-gnu-
NDAS_CROSS_COMPILE?=/opt/wischip/mipsel-linux-gnu/gcc-3.3.4-glibc-2.3.2/bin/mipsel-linux-gnu-
NDAS_CROSS_COMPILE_UM?=/opt/wischip/mipsel-linux-gnu/gcc-3.3.4-glibc-2.3.2/bin/mipsel-linux-gnu-
NDAS_EXTRA_CLFAGS+="-DLINUX -D_MIPSEL -mlong-calls"
nxp-cflags+= -pipe -Wa,--trap -Wa,-32 -Wa,-march=mips32 -Wa,-mips32 -fno-delayed-branch -mlong-calls 

