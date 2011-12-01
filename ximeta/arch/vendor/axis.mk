nxp-cflags +=-Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fno-builtin-sprintf -fomit-frame-pointer -mlinux 
nxp-public-cflags:=-D_CRIS -DLINUX
NDAS_EXTRA_LDFLAGS+=-mcrislinux
