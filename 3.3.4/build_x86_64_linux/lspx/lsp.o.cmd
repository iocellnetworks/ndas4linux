cmd_build_x86_64_linux/lspx/lsp.o := gcc -Wp,-MD,build_x86_64_linux/lspx/lsp.o.d -D_X86_64 -fno-strict-aliasing -fno-common -ffreestanding -fomit-frame-pointer -march=k8 -mtune=nocona -mno-red-zone -mcmodel=kernel -pipe -fno-reorder-blocks -funit-at-a-time -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -fno-stack-protector -DLINUX -pipe -fno-builtin-sprintf -fno-builtin-log2 -fno-builtin-puts -fno-strict-aliasing -fno-common -fomit-frame-pointer -Wall -I/home/david/ndas4linux/3.3.4/inc -I/home/david/ndas4linux/3.3.4/inc/lspx -D__LITTLE_ENDIAN_BYTEORDER -D__LITTLE_ENDIAN_BITFIELD -D__LITTLE_ENDIAN__ -DNDAS_VER_MAJOR="3" -DNDAS_VER_MINOR="3" -DNDAS_VER_BUILD="4" -O2 -DXPLAT_SIO -DXPLAT_NDASHIX -DXPLAT_PNP -DXPLAT_ASYNC_IO -DXPLAT_RESTORE -DRELEASE -DXPLAT_BPC  -O2 -c -o build_x86_64_linux/lspx/lsp.o lspx/lsp.c

deps_build_x86_64_linux/lspx/lsp.o := \
  lspx/lsp.c \
  lspx/lsp_type_internal.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lsp.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lspspecstring.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lsp_type.h \
  /usr/lib/gcc/x86_64-redhat-linux/4.7.0/include/stdint.h \
  /usr/lib/gcc/x86_64-redhat-linux/4.7.0/include/stdint-gcc.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lsp_spec.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/pshpack4.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lsp_ide_def.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lsp_host.h \
  /usr/include/string.h \
  /usr/include/features.h \
  /usr/include/sys/cdefs.h \
  /usr/include/bits/wordsize.h \
  /usr/include/gnu/stubs.h \
  /usr/include/gnu/stubs-64.h \
  /usr/lib/gcc/x86_64-redhat-linux/4.7.0/include/stddef.h \
  /usr/include/xlocale.h \
  /usr/include/bits/string.h \
  /usr/include/bits/string2.h \
  /usr/include/endian.h \
  /usr/include/bits/endian.h \
  /usr/include/bits/byteswap.h \
  /usr/include/bits/types.h \
  /usr/include/bits/typesizes.h \
  /usr/include/stdlib.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lsp_hash.h \
  lspx/lsp_binparm.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/pshpack4.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lsp_type.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/poppack.h \
  lspx/lsp_debug.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lsp_util.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lsp.h \
  /home/david/ndas4linux/3.3.4/inc/lspx/lsp.h \

build_x86_64_linux/lspx/lsp.o: $(deps_build_x86_64_linux/lspx/lsp.o)

$(deps_build_x86_64_linux/lspx/lsp.o):
