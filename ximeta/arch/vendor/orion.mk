nxp-cflags += -Wall -Wstrict-prototypes -Wno-trigraphs \
	-ffreestanding -fno-strict-aliasing -fno-common \
	-fno-omit-frame-pointer \
	-mapcs -mno-sched-prolog -mlittle-endian -march=armv5te \
	-mtune=xscale -msoft-float -Wa,-mno-fpu \
	-Uarm  -iwithprefix -nostdlib \
	-ffunction-sections -mabi=aapcs-linux -mno-thumb-interwork \
	-Uarm -Wdeclaration-after-statement 

nxp-cflags += -DMODVERSIONS -DEXPORT_SYMTAB -DMODULE -DLINUX -D__KERNEL__ \
	-I$(NDAS_KERNEL_PATH)/include

nxp-public-cflags:=-D_ARM -DLINUX 

