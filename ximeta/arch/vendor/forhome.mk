FORHOME_HEADER_INC= -I/home/sjcho/4home/buildroot/toolchain_build_mips_nofpu/linux/include \
	-I/home/sjcho/4home/buildroot/toolchain_build_mips_nofpu/uclibc/include \
	-D__KERNEL__ -DMODULE

forhome-build-num = "\"forhome r1\""
nxp-cflags +=\
	-nostdinc $(FORHOME_HEADER_INC) -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -O2 -fomit-frame-pointer -G 0 -mno-abicalls -fno-pic -pipe  -mabi=32 -march=mips32r2 -Wa,-32 -Wa,-march=mips32r2 -Wa,-mips32r2 -Wa,--trap -mno-branch-likely -mlong-calls 

NDAS_CROSS_COMPILE=/home/sjcho/4home/buildroot/build_mips_nofpu/staging_dir/bin/mips-linux-uclibc-


