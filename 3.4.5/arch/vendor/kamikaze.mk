#
# Linksys WRT54G series
#

ifeq ($(nxp-cpu),mipsel)

#nxp-cflags+=-D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -Os -fno-strict-aliasing -fno-common -fomit-frame-pointer -funit-at-a-time -G 0 -mno-abicalls -fno-pic -pipe -finline-limit=100000 -mabi=32 -march=mips32 -Wa:-32 -Wa:-march=mips32 -Wa:-mips32 -Wa:--trap -iwithprefix 

nxp-cflags+=-Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Os  -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -ffreestanding  -march=mips32 -Wa,-mips32 -Wa,--trap -ffreestanding -fomit-frame-pointer  -fno-stack-protector -funit-at-a-time -Wdeclaration-after-statement -Wno-pointer-sign

#nxp-cflags+=-Wp,-MD, -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Os  -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -ffreestanding  -march=mips32 -Wa,-mips32 -Wa,--trap -ffreestanding -fomit-frame-pointer  -fno-stack-protector -funit-at-a-time -Wdeclaration-after-statement -Wno-pointer-sign

else

ifeq ($(nxp-cpu),i386)

nxp-cflags+=-D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=2 -march=i486 -iwithprefix 

endif

endif
