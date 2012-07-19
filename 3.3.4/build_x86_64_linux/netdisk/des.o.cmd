cmd_build_x86_64_linux/netdisk/des.o := gcc -Wp,-MD,build_x86_64_linux/netdisk/des.o.d -D_X86_64 -fno-strict-aliasing -fno-common -ffreestanding -fomit-frame-pointer -march=k8 -mtune=nocona -mno-red-zone -mcmodel=kernel -pipe -fno-reorder-blocks -funit-at-a-time -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -fno-stack-protector -DLINUX -pipe -fno-builtin-sprintf -fno-builtin-log2 -fno-builtin-puts -fno-strict-aliasing -fno-common -fomit-frame-pointer -Wall -I/home/david/ndas4linux/3.3.4/inc -I/home/david/ndas4linux/3.3.4/inc/lspx -D__LITTLE_ENDIAN_BYTEORDER -D__LITTLE_ENDIAN_BITFIELD -D__LITTLE_ENDIAN__ -DNDAS_VER_MAJOR="3" -DNDAS_VER_MINOR="3" -DNDAS_VER_BUILD="4" -O2 -DXPLAT_SIO -DXPLAT_NDASHIX -DXPLAT_PNP -DXPLAT_ASYNC_IO -DXPLAT_RESTORE -DRELEASE -DXPLAT_BPC  -O2 -c -o build_x86_64_linux/netdisk/des.o netdisk/des.c

deps_build_x86_64_linux/netdisk/des.o := \
  netdisk/des.c \
  netdisk/des.h \
  /home/david/ndas4linux/3.3.4/inc/sal/types.h \

build_x86_64_linux/netdisk/des.o: $(deps_build_x86_64_linux/netdisk/des.o)

$(deps_build_x86_64_linux/netdisk/des.o):
