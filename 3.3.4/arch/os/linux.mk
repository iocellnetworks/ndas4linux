# $LastChangedDate: 2004-08-17 15:28:45 -0700 (Tue, 17 Aug 2004) $
# $Author: sjcho $

ifneq ($(nxpo-ndaslinux),y)

nxp-cflags+= -DLINUX \
	-pipe \
	-fno-builtin-sprintf  \
	-fno-builtin-log2 \
	-fno-builtin-puts \
	-fno-strict-aliasing \
	-fno-common

ifeq ($(nxpo-debug),y)

else
nxp-cflags+= 	-fomit-frame-pointer
endif

ifdef nxpo-uni
ifeq ($(nxp-cpu), x86)
nxp-cflags+= -mpreferred-stack-boundary=2 \
	-mregparm=3 
endif	
endif

else

ifeq ($(nxp-vendor),)

ifneq ($(findstring 2.4.,$(shell uname -r)),)
nxp-cflags+=-D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=2 -iwithprefix 
endif

ifneq ($(findstring 2.5.,$(shell uname -r)),)
nxp-cflags+=-Wp,-MD,-D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -pipe -mpreferred-stack-boundary=2 -iwithprefix 
endif

endif

endif

