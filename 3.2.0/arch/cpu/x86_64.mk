XPLAT_CONFIG_ENDIAN_BYTEORDER=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD=LITTLE
nxp-cflags= -D_X86_64 \
	-fno-strict-aliasing -fno-common \
	-ffreestanding -fomit-frame-pointer -march=k8 -mtune=nocona \
	-mno-red-zone -mcmodel=kernel -pipe -fno-reorder-blocks \
	-funit-at-a-time -mno-sse -mno-mmx -mno-sse2 -mno-3dnow \
	-fno-stack-protector
#	-mpreferred-stack-boundary=4 \
#	-minline-all-stringops \
#	-mregparm=3 \
