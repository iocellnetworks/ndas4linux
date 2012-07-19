cmd_build_x86_64_linux/netdisk/gen_uuid.o := gcc -Wp,-MD,build_x86_64_linux/netdisk/gen_uuid.o.d -D_X86_64 -fno-strict-aliasing -fno-common -ffreestanding -fomit-frame-pointer -march=k8 -mtune=nocona -mno-red-zone -mcmodel=kernel -pipe -fno-reorder-blocks -funit-at-a-time -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -fno-stack-protector -DLINUX -pipe -fno-builtin-sprintf -fno-builtin-log2 -fno-builtin-puts -fno-strict-aliasing -fno-common -fomit-frame-pointer -Wall -I/home/david/ndas4linux/3.3.4/inc -I/home/david/ndas4linux/3.3.4/inc/lspx -D__LITTLE_ENDIAN_BYTEORDER -D__LITTLE_ENDIAN_BITFIELD -D__LITTLE_ENDIAN__ -DNDAS_VER_MAJOR="3" -DNDAS_VER_MINOR="3" -DNDAS_VER_BUILD="4" -O2 -DXPLAT_SIO -DXPLAT_NDASHIX -DXPLAT_PNP -DXPLAT_ASYNC_IO -DXPLAT_RESTORE -DRELEASE -DXPLAT_BPC  -O2 -c -o build_x86_64_linux/netdisk/gen_uuid.o netdisk/gen_uuid.c

deps_build_x86_64_linux/netdisk/gen_uuid.o := \
  netdisk/gen_uuid.c \
  /home/david/ndas4linux/3.3.4/inc/sal/libc.h \
  /home/david/ndas4linux/3.3.4/inc/sal/linux/libc.h \
  /home/david/ndas4linux/3.3.4/inc/sal/sal.h \
  /home/david/ndas4linux/3.3.4/inc/ndasuser/ndaserr.h \
  /home/david/ndas4linux/3.3.4/inc/sal/linux/sal.h \
  /home/david/ndas4linux/3.3.4/inc/sal/thread.h \
  /home/david/ndas4linux/3.3.4/inc/sal/types.h \
  /home/david/ndas4linux/3.3.4/inc/sal/time.h \
  /home/david/ndas4linux/3.3.4/inc/sal/net.h \
  /home/david/ndas4linux/3.3.4/inc/sal/mem.h \
  netdisk/uuid.h \

build_x86_64_linux/netdisk/gen_uuid.o: $(deps_build_x86_64_linux/netdisk/gen_uuid.o)

$(deps_build_x86_64_linux/netdisk/gen_uuid.o):
