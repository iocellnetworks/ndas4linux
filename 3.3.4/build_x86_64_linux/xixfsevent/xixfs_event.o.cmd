cmd_build_x86_64_linux/xixfsevent/xixfs_event.o := gcc -Wp,-MD,build_x86_64_linux/xixfsevent/xixfs_event.o.d -D_X86_64 -fno-strict-aliasing -fno-common -ffreestanding -fomit-frame-pointer -march=k8 -mtune=nocona -mno-red-zone -mcmodel=kernel -pipe -fno-reorder-blocks -funit-at-a-time -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -fno-stack-protector -DLINUX -pipe -fno-builtin-sprintf -fno-builtin-log2 -fno-builtin-puts -fno-strict-aliasing -fno-common -fomit-frame-pointer -Wall -I/home/david/ndas4linux/3.3.4/inc -I/home/david/ndas4linux/3.3.4/inc/lspx -D__LITTLE_ENDIAN_BYTEORDER -D__LITTLE_ENDIAN_BITFIELD -D__LITTLE_ENDIAN__ -DNDAS_VER_MAJOR="3" -DNDAS_VER_MINOR="3" -DNDAS_VER_BUILD="4" -O2 -DXPLAT_SIO -DXPLAT_NDASHIX -DXPLAT_PNP -DXPLAT_ASYNC_IO -DXPLAT_RESTORE -DRELEASE -DXPLAT_BPC  -O2 -c -o build_x86_64_linux/xixfsevent/xixfs_event.o xixfsevent/xixfs_event.c

deps_build_x86_64_linux/xixfsevent/xixfs_event.o := \
  xixfsevent/xixfs_event.c \
  /home/david/ndas4linux/3.3.4/inc/xplatcfg.h \
  /home/david/ndas4linux/3.3.4/inc/dbgcfg.h \
  /home/david/ndas4linux/3.3.4/inc/sal/libc.h \
  /home/david/ndas4linux/3.3.4/inc/sal/linux/libc.h \
  /home/david/ndas4linux/3.3.4/inc/sal/sal.h \
  /home/david/ndas4linux/3.3.4/inc/ndasuser/ndaserr.h \
  /home/david/ndas4linux/3.3.4/inc/sal/linux/sal.h \
  /home/david/ndas4linux/3.3.4/inc/sal/sync.h \
  /home/david/ndas4linux/3.3.4/inc/sal/time.h \
  /home/david/ndas4linux/3.3.4/inc/sal/types.h \
  /home/david/ndas4linux/3.3.4/inc/xlib/gtypes.h \
  /home/david/ndas4linux/3.3.4/inc/glibconfig.h \
    $(wildcard include/config/h/.h) \
  /home/david/ndas4linux/3.3.4/inc/sal/types.h \
  /home/david/ndas4linux/3.3.4/inc/lpx/lpx.h \
  /home/david/ndas4linux/3.3.4/inc/sal/net.h \
  /home/david/ndas4linux/3.3.4/inc/sal/mem.h \
  /home/david/ndas4linux/3.3.4/inc/lpx/lpxutil.h \
  /home/david/ndas4linux/3.3.4/inc/sal/debug.h \
  /home/david/ndas4linux/3.3.4/inc/sal/linux/debug.h \
  /home/david/ndas4linux/3.3.4/inc/xlib/dpc.h \
  /home/david/ndas4linux/3.3.4/inc/xlib/xbuf.h \
  /home/david/ndas4linux/3.3.4/inc/netdisk/list.h \
  /home/david/ndas4linux/3.3.4/inc/lpx/debug.h \
  /home/david/ndas4linux/3.3.4/inc/xixfsevent/xixfs_event.h \
  /home/david/ndas4linux/3.3.4/inc/ndasuser/def.h \

build_x86_64_linux/xixfsevent/xixfs_event.o: $(deps_build_x86_64_linux/xixfsevent/xixfs_event.o)

$(deps_build_x86_64_linux/xixfsevent/xixfs_event.o):
