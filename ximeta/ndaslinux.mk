ifeq ($(nxpo-ndaslinux),y)

nxp-cpu?=x86

ifeq ($(nxp-vendor),)
nxp-target=$(nxp-cpu)_$(nxp-os)
else
nxp-target=$(nxp-vendor)
endif

ifeq ($(nxpo-debug),y)
nxp-cflags += -DDEBUG
endif

ifeq ($(nxp-cpu),x86)
XPLAT_CONFIG_ENDIAN_BYTEORDER=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD=LITTLE
endif

ifeq ($(nxp-cpu),x86_64)
XPLAT_CONFIG_ENDIAN_BYTEORDER=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD=LITTLE
endif

ifeq ($(nxp-cpu),mipsel)
XPLAT_CONFIG_ENDIAN_BYTEORDER=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD=LITTLE
endif

ifeq ($(nxp-cpu),mips)
XPLAT_CONFIG_ENDIAN_BYTEORDER=BIG
XPLAT_CONFIG_ENDIAN_BITFIELD=BIG
endif

ifeq ($(nxp-cpu),arm)
XPLAT_CONFIG_ENDIAN_BYTEORDER=LITTLE
XPLAT_CONFIG_ENDIAN_BITFIELD=LITTLE
endif


ifeq ($(nxp-vendor),)

ifneq ($(findstring 2.4.,$(shell uname -r)),)
nxp-cflags+=-D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=2 -iwithprefix 
endif

ifneq ($(findstring 2.5.,$(shell uname -r)),)
nxp-cflags+=-Wp,-MD,-D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -pipe -mpreferred-stack-boundary=2 -iwithprefix 
endif

ifneq ($(findstring 2.6.,$(shell uname -r)),)

ifeq ($(nxp-cpu),x86)

nxp-cflags += -m32 -Wp, -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Os -pipe -msoft-float -mregparm=3 -freg-struct-return -mpreferred-stack-boundary=2 -ffreestanding -maccumulate-outgoing-args -DCONFIG_AS_CFI=1 -DCONFIG_AS_CFI_SIGNAL_FRAME=1 -fomit-frame-pointer -g #-fno-stack-protector -Wdeclaration-after-statement -Wno-pointer-sign -mtune=generic 

endif

ifeq ($(nxp-cpu),x86_64)

nxp-cflags += -Wp, -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Os  -fno-stack-protector -m64 -mtune=generic -mno-red-zone -mcmodel=kernel -funit-at-a-time -maccumulate-outgoing-args  -DCONFIG_AS_CFI=1 -DCONFIG_AS_CFI_SIGNAL_FRAME=1 -pipe -Wno-sign-compare -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -mno-sse2 -mno-3dnow  -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -Wdeclaration-after-statement -Wno-pointer-sign 

endif

endif

endif #ifeq ($(nxp-vendor),)


ifeq ($(nxp-vendor),samsung)

nxp-cflags += -Wp, -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -O2  -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -ffreestanding  -march=mips32 -Wa,-mips32 -Wa,--trap -fomit-frame-pointer  -Wdeclaration-after-statement
endif

endif #ifeq ($(nxp-vendor),samsung)


ifeq ($(nxp-vendor),whiterussian)

nxp-cflags+=-D__KERNEL__ -I$(NDAS_KERNEL_PATH)/include -Wall -Wstrict-prototypes -Wno-trigraphs -Os -fno-strict-aliasing -fno-common -fomit-frame-pointer -funit-at-a-time -I$(NDAS_KERNEL_PATH)/include/asm/gcc -G 0 -mno-abicalls -fno-pic -pipe -finline-limit=100000 -mabi=32 -march=mips32 -Wa:-32 -Wa:-march=mips32 -Wa:-mips32 -Wa:--trap -nostdinc -iwithprefix 

endif #ifeq ($(nxp-vendor),whiterussian)


ifeq ($(nxp-vendor),kamikaze)

ifeq ($(nxp-cpu),mipsel)

#nxp-cflags+=-D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -Os -fno-strict-aliasing -fno-common -fomit-frame-pointer -funit-at-a-time -G 0 -mno-abicalls -fno-pic -pipe -finline-limit=100000 -mabi=32 -march=mips32 -Wa:-32 -Wa:-march=mips32 -Wa:-mips32 -Wa:--trap -iwithprefix 

nxp-cflags+=-Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Os  -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -ffreestanding  -march=mips32 -Wa,-mips32 -Wa,--trap -ffreestanding -fomit-frame-pointer  -fno-stack-protector -funit-at-a-time -Wdeclaration-after-statement -Wno-pointer-sign

#nxp-cflags+=-Wp,-MD, -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Os  -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -ffreestanding  -march=mips32 -Wa,-mips32 -Wa,--trap -ffreestanding -fomit-frame-pointer  -fno-stack-protector -funit-at-a-time -Wdeclaration-after-statement -Wno-pointer-sign

endif

ifeq ($(nxp-cpu),i386)

nxp-cflags+=-D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=2 -march=i486 -iwithprefix 

endif

endif #ifeq ($(nxp-vendor),kamikaze)


ifeq ($(nxp-vendor),crypto)

nxp-cflags += -D__KERNEL__ -DMODULE \
	-Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fno-common \
	-pipe -fno-builtin -D__linux__ -DNO_MM -mapcs-32 -march=armv4 -mtune=arm7tdmi -mshort-load-bytes\
	-msoft-float  -iwithprefix -nostdlib # -nostdinc

# Use if nxp-embedded option is on
nxp-cflags += -DMODVERSIONS -DEXPORT_SYMTAB -DMODULE -DLINUX -D__KERNEL__ -I$(NDAS_KERNEL_PATH)/include 

# This value will be used when tarball.mk builds complete or ipkg rule.
nxp-public-cflags:=-D_ARM -DLINUX 

endif #ifeq ($(nxp-vendor),crypto)


ifeq ($(nxp-vendor),alogics)

nxp-cflags += -D__KERNEL__ -DMODULE \
	-Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fno-common \
	-pipe -fno-builtin -D__linux__ -DNO_MM -mapcs-32 -march=armv4 -mtune=arm7tdmi -mshort-load-bytes\
	-msoft-float  -iwithprefix -nostdlib # -nostdinc

# Use if nxp-embedded option is on
nxp-cflags += -DMODVERSIONS -DEXPORT_SYMTAB -DMODULE -DLINUX -D__KERNEL__ -I$(NDAS_KERNEL_PATH)/include 

# This value will be used when tarball.mk builds complete or ipkg rule.
nxp-public-cflags:=-D_ARM -DLINUX 

endif #ifeq ($(nxp-vendor),alogics)

ifeq ($(nxp-vendor),syabas)
nxp-cflags+=-Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Os  -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -ffreestanding  -march=mips32 -Wa,-mips32 -Wa,--trap -ffreestanding -fomit-frame-pointer  -funit-at-a-time -Wdeclaration-after-statement
endif

ifeq ($(nxp-vendor),armutils)

nxp-cflags += -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fno-common -pipe -fno-builtin -D__linux__ -DNO_MM -mapcs-32 -march=armv4 -mtune=arm7tdmi -mshort-load-bytes -msoft-float -iwithprefix

endif #ifeq ($(nxp-vendor),armutils)



ndaslinux:
	export NDAS_DEBUG=y; 																\
	export NDAS_EXTRA_CFLAGS="-DNDAS_HIX -DNDAS_EMU $(NDAS_EXTRA_CFLAGS)"; 				\
	make nxp-os=linux nxpo-ndaslinux=y nxpo-uni=y nxpo-debug=y tarball

ndaslinux64:
	export NDAS_DEBUG=y; 																\
	export NDAS_EXTRA_CFLAGS="-DNDAS_HIX $(NDAS_EXTRA_CFLAGS)"; 						\
	make nxp-os=linux nxpo-ndaslinux=y nxpo-uni=y nxp-cpu=x86_64 nxpo-debug=y tarball

#make oldconfig;make modules_prepare

ndaslinux-test:
	export NDAS_DEBUG=y; 														\
	export NDAS_KERNEL_PATH=/root/workspace/kernels/linux-2.6.25;				\
	export NDAS_KERNEL_VERSION=2.6.25; 											\
	export NDAS_EXTRA_CFLAGS="-DNDAS_HIX $(NDAS_EXTRA_CFLAGS)"; 				\
	make nxp-os=linux nxpo-ndaslinux=y nxpo-uni=y nxpo-debug=y tarball

ndaskamikaze:
	@if [[ -z $$KAMIKAZE_PATH ]] ; then \
		echo KAMIKAZE_PATH is not set; 	\
		exit 1; 						\
	fi
	export NDAS_DEBUG=y; 																						\
	export PATH=$(PATH):$(KAMIKAZE_PATH)/staging_dir_mipsel/bin:$(KAMIKAZE_PATH)/staging_dir_mipsel/usr/bin;	\
	export NDAS_KERNEL_PATH=$(KAMIKAZE_PATH)/build_mipsel/linux; 												\
	export NDAS_KERNEL_ARCH=mipsel; 																			\
	export NDAS_KERNEL_VERSION=2.6.22; 																			\
	export NDAS_CROSS_COMPILE=$(KAMIKAZE_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-; 					\
	export NDAS_CROSS_COMPILE_UM=$(KAMIKAZE_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-; 					\
	export NDAS_EXTRA_CFLAGS="-DNDAS_HIX -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)";				\
	make ARCH=mips CROSS_COMPILE=$(KAMIKAZE_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc- 					\
	nxp-os=linux nxpo-ndaslinux=y nxp-cpu=mipsel nxp-vendor=kamikaze 											\
	nxpo-automated=yes nxpo-debug=y 																			\
	ipkg

ndaskamikaze24:
	@if [[ -z $$KAMIKAZE24_PATH ]] ; then 	\
		echo KAMIKAZE24_PATH is not set; 	\
		exit 1; 							\
	fi
	export NDAS_DEBUG=y; 																							\
	export PATH=$(PATH):$(KAMIKAZE24_PATH)/staging_dir_mipsel/bin:$(KAMIKAZE24_PATH)/staging_dir_mipsel/usr/bin;	\
	export NDAS_KERNEL_PATH=$(KAMIKAZE24_PATH)/build_mipsel/linux; 													\
	export NDAS_KERNEL_ARCH=mipsel; 																				\
	export NDAS_KERNEL_VERSION=2.4.34; 																				\
	export NDAS_CROSS_COMPILE=$(KAMIKAZE24_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-; 						\
	export NDAS_CROSS_COMPILE_UM=$(KAMIKAZE24_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-; 					\
	export NDAS_EXTRA_CFLAGS="-DLINUX -D_MIPSEL -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)";					\
	make ARCH=mips CROSS_COMPILE=$(KAMIKAZE24_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc- 					\
	nxp-os=linux nxpo-ndaslinux=y nxp-cpu=mipsel nxp-vendor=kamikaze 												\
	nxpo-automated=yes nxpo-debug=y	 																				\
	ipkg

ndaswhiterussian:
	@if [[ -z $$WHITERUSSIAN_PATH ]] ; then \
		echo WHITERUSSIAN_PATH is not set; 	\
		exit 1; 							\
	fi
	export NDAS_DEBUG=y; 																								\
	export PATH=$(PATH):$(WHITERUSSIAN_PATH)/staging_dir_mipsel/bin:$(WHITERUSSIAN_PATH)/staging_dir_mipsel/usr/bin;	\
	export NDAS_KERNEL_PATH=$(WHITERUSSIAN_PATH)/build_mipsel/linux; 													\
	export NDAS_KERNEL_ARCH=mipsel; 																					\
	export NDAS_KERNEL_VERSION=2.4.30; 																					\
	export NDAS_CROSS_COMPILE=$(WHITERUSSIAN_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-; 						\
	export NDAS_CROSS_COMPILE_UM=$(WHITERUSSIAN_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-; 						\
	export NDAS_EXTRA_CFLAGS="-DLINUX -D_MIPSEL -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)";						\
	make ARCH=mips CROSS_COMPILE=$(WHITERUSSIAN_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-  						\
	nxp-os=linux nxpo-ndaslinux=y nxp-cpu=mipsel nxp-vendor=whiterussian												\
	nxpo-automated=yes nxpo-debug=n	 																					\
	ipkg

ndassamsung:
	@if [[ -z $$SAMSUNG_PATH ]] ; then 	\
		echo SAMSUNG_PATH is not set; 	\
		exit 1; 						\
	fi
	export NDAS_DEBUG=y; 																	\
	export PATH=$(PATH):$(SAMSUNG_PATH)/toolchains/uclibc/bin								\
	export NDAS_KERNEL_PATH=$(SAMSUNG_PATH)/brcm/linux; 									\
	export NDAS_KERNEL_ARCH=mips; 															\
	export NDAS_KERNEL_VERSION=2.6.18.1; 													\
	export NDAS_CROSS_COMPILE=mips-linux-uclibc-; 											\
	export NDAS_CROSS_COMPILE_UM=mips-linux-uclibc-; 										\
	export NDAS_EXTRA_CFLAGS="-DNDAS_HIX -DNDAS_WRITE_BACK -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)";	\
	make ARCH=mips CROSS_COMPILE=$(SAMSUNG_PATH)/toolchains/uclibc/bin/mips-linux-uclibc- 	\
	nxp-os=linux nxpo-ndaslinux=y nxp-cpu=mips nxp-vendor=samsung 							\
	nxpo-automated=yes nxpo-debug=n	 														\
	tarball

ndasksamsung:
	@if [[ -z $$KAMIKAZE_PATH ]] ; then \
		echo KAMIKAZE_PATH is not set; 	\
		exit 1;							\
	fi
	export NDAS_DEBUG=; 																					\
	export PATH=$(PATH):$(KAMIKAZE_PATH)/staging_dir_mips/bin:$(KAMIKAZE_PATH)/staging_dir_mips/usr/bin;	\
	export NDAS_KERNEL_PATH=$(KAMIKAZE_PATH)/build_mips/linux; 												\
	export NDAS_KERNEL_ARCH=mips; 																			\
	export NDAS_KERNEL_VERSION=2.6.18.1; 																	\
	export NDAS_CROSS_COMPILE=$(KAMIKAZE_PATH)/staging_dir_mips/bin/mips-linux-uclibc-; 					\
	export NDAS_CROSS_COMPILE_UM=$(KAMIKAZE_PATH)/staging_dir_mips/bin/mips-linux-uclibc-; 					\
	export NDAS_EXTRA_CFLAGS="--DNDAS_HIX -DNDAS_WRITE_BACK -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)";	\
	make ARCH=mips CROSS_COMPILE=$(KAMIKAZE_PATH)/staging_dir_mips/bin/mips-linux-uclibc- 					\
	nxp-os=linux nxpo-ndaslinux=y nxp-cpu=mips nxp-vendor=samsung 											\
	nxpo-automated=yes nxpo-debug=n	 																		\
	tarball

ndascrypto:
	export NDAS_DEBUG=; \
	export PATH=$(PATH):/usr/local/arm/2.95.3/bin;	\
	export NDAS_KERNEL_PATH=/usr/src/linux.pr818s; \
	export NDAS_KERNEL_ARCH=arm; \
	export NDAS_KERNEL_VERSION=2.4.18;	\
	export NDAS_CROSS_COMPILE=/usr/local/arm/2.95.3/bin/arm-linux-;	\
	export NDAS_CROSS_COMPILE_UM=/usr/local/arm/2.95.3/bin/arm-linux-; \
	export NDAS_EXTRA_CFLAGS="-DNDAS_VID=0x20 -DNDAS_CRYPTO -DLINUX -DPROBE_BY_ID -DNO_DEBUG_ESP $(NDAS_EXTRA_CFLAGS)";	\
	make ARCH=arm \
	nxp-os=linux nxpo-ndaslinux=y nxp-cpu=arm nxp-vendor=crypto	\
	nxpo-automated=yes nxpo-debug=n	nxpo-probe=y \
	tarball

ndasalogics:
	export NDAS_DEBUG; \
	export PATH=$(PATH):/usr/local/arm/2.95.3/bin;	\
	export NDAS_KERNEL_PATH=/usr/src/linux-2.4.27-cryptotelecom; \
	export NDAS_KERNEL_ARCH=arm; \
	export NDAS_KERNEL_VERSION=2.4.27;	\
	export NDAS_CROSS_COMPILE=/usr/local/arm/2.95.3/bin/arm-linux-;	\
	export NDAS_CROSS_COMPILE_UM=/usr/local/arm/2.95.3/bin/arm-linux-; \
	export NDAS_EXTRA_CFLAGS="-DNDAS_VID=0x20 -DNDAS_CRYPTO -DLINUX -DPROBE_BY_ID -DNO_DEBUG_ESP $(NDAS_EXTRA_CFLAGS)";	\
	make ARCH=arm \
	nxp-os=linux nxpo-ndaslinux=y nxp-cpu=arm nxp-vendor=alogics	\
	nxpo-automated=yes nxpo-debug=n	nxpo-probe=y \
	tarball

ndasalogics_o:
	export NDAS_DEBUG; \
	export PATH=$(PATH):/usr/local/arm/2.95.3/bin;	\
	export NDAS_KERNEL_PATH=/usr/src/linux-2.4.27-muwit; \
	export NDAS_KERNEL_ARCH=arm; \
	export NDAS_KERNEL_VERSION=2.4.27;	\
	export NDAS_CROSS_COMPILE=/usr/local/arm/2.95.3/bin/arm-linux-;	\
	export NDAS_CROSS_COMPILE_UM=/usr/local/arm/2.95.3/bin/arm-linux-; \
	export NDAS_EXTRA_CFLAGS="-DNDAS_VID=0x20 -DNDAS_CRYPTO -DLINUX -DPROBE_BY_ID -DNO_DEBUG_ESP $(NDAS_EXTRA_CFLAGS)";	\
	make ARCH=arm \
	nxp-os=linux nxpo-ndaslinux=y nxp-cpu=arm nxp-vendor=alogics	\
	nxpo-automated=yes nxpo-debug=n	nxpo-probe=y \
	tarball

ndas_syabas:

ndas_syabas:
	export NDAS_DEBUG; \
	export PATH=$(PATH):/home2/syabas/smp86xx_toolchain.20080505/build_mipsel_nofpu/staging_dir/bin;	\
	export NDAS_KERNEL_PATH=/usr/src/linux-2.6.15; \
	export NDAS_KERNEL_ARCH=mipsel; \
	export NDAS_KERNEL_VERSION=2.6.15;	\
	export NDAS_CROSS_COMPILE=/home2/syabas/smp86xx_toolchain.20080505/build_mipsel_nofpu/staging_dir/bin/mipsel-linux-uclibc-;	\
	export NDAS_CROSS_COMPILE_UM=/home2/syabas/smp86xx_toolchain.20080505/build_mipsel_nofpu/staging_dir/bin/mipsel-linux-uclibc-;	\
	export NDAS_EXTRA_CFLAGS="-DLINUX -DPROBE_BY_ID -DNO_DEBUG_ESP $(NDAS_EXTRA_CFLAGS)";	\
	make ARCH=mips CROSS_COMPILE=/home2/syabas/smp86xx_toolchain.20080505/build_mipsel_nofpu/staging_dir/bin/mipsel-linux-uclibc- 	\
	nxp-os=linux nxpo-ndaslinux=y nxp-cpu=mipsel nxp-vendor=syabas	\
	nxpo-automated=yes nxpo-debug=n	nxpo-probe=y \
	tarball

ndasarmutils:
	@if [[ -z $$ARMUTILS_PATH ]] ; then 								\
		echo ARMUTILS_PATH is not set; 									\
		echo ex: export ARMUTILS_PATH=/root/workspace/armutils_2.8.0.2;	\
		exit 1;															\
	fi
	export NDAS_DEBUG;	 																					\
	export PATH=$(PATH):$(ARMUTILS_PATH)/toolchain/ccache:$(SYABAS_PATH)/toolchain/bin;						\
	export CCACHE_DIR=$(ARMUTILS_PATH)/cache;																\
	export NDAS_KERNEL_PATH=$(ARMUTILS_PATH)/build_arm/linux-2.4.22-em86xx; 								\
	export NDAS_KERNEL_ARCH=arm; 																			\
	export NDAS_KERNEL_VERSION=2.4.22; 																		\
	export NDAS_CROSS_COMPILE=$(ARMUTILS_PATH)/toolchain/bin/arm-elf-; 										\
	export NDAS_CROSS_COMPILE_UM=$(ARMUTILS_PATH)/toolchain/bin/arm-elf-;						 			\
	export NDAS_EXTRA_CFLAGS="-elf2flt="-s32768" -D__linux__ -DLINUX -DNO_DEBUG_ESP $(NDAS_EXTRA_CFLAGS)";	\
	make ARCH=arm CROSS_COMPILE=$(ARMUTILS_PATH)/toolchain/bin/arm-elf-										\
	nxp-os=linux nxpo-ndaslinux=y nxp-cpu=arm nxp-vendor=armutils											\
	nxpo-automated=yes nxpo-debug=n																			\
	tarball




