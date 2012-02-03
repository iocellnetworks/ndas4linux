#
# Linksys WRT54G series
#
NDAS_CROSS_COMPILE?=mipsel-linux-
NDAS_CROSS_COMPILE?=mipsel-linux-
NDAS_CROSS_COMPILE_UM?=mipsel-linux-uclibc-
NDAS_EXTRA_CLFAGS+="-DNDAS_SIGPENDING_OLD -DLINUX -D_MIPSEL -DBCMDRIVER -DNDAS_IO_UNIT=16 -mlong-calls "
nxp-cflags+=-pipe -mabi=32 -march=mips32 -Wa,-32 -Wa,-march=mips32 -Wa,-mips32 -finline-limit=100000 -fno-common 
# WhiteRussian RC2
#nxp-cflags+= -pipe -Wa,-32 -Wa,-march=mips32 -Wa,-mips32 -fno-delayed-branch 

