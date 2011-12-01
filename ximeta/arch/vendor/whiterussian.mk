#
# Linksys WRT54G series
#

#nxp-cflags+=-D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -Os -fno-strict-aliasing -fno-common -fomit-frame-pointer -funit-at-a-time -G 0 -mno-abicalls -fno-pic -pipe -finline-limit=100000 -mabi=32 -march=mips32 -Wa:-32 -Wa:-march=mips32 -Wa:-mips32 -Wa:--trap -iwithprefix 

nxp-cflags+=-D__KERNEL__ -I$(NDAS_KERNEL_PATH)/include -Wall -Wstrict-prototypes -Wno-trigraphs -Os -fno-strict-aliasing -fno-common -fomit-frame-pointer -funit-at-a-time -I$(NDAS_KERNEL_PATH)/include/asm/gcc -G 0 -mno-abicalls -fno-pic -pipe -finline-limit=100000 -mabi=32 -march=mips32 -Wa:-32 -Wa:-march=mips32 -Wa:-mips32 -Wa:--trap -nostdinc -iwithprefix 
