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
		ifeq ($(nxp-cpu),x86)
			nxp-cflags += -m32 -Wp, -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Os -pipe -msoft-float -mregparm=3 -freg-struct-return -mpreferred-stack-boundary=2 -ffreestanding -maccumulate-outgoing-args -DCONFIG_AS_CFI=1 -DCONFIG_AS_CFI_SIGNAL_FRAME=1 -fomit-frame-pointer -g #-fno-stack-protector -Wdeclaration-after-statement -Wno-pointer-sign -mtune=generic 
		endif
		ifeq ($(nxp-cpu),x86_64)
			nxp-cflags += -Wp, -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Os  -fno-stack-protector -m64 -mtune=generic -mno-red-zone -mcmodel=kernel -funit-at-a-time -maccumulate-outgoing-args  -DCONFIG_AS_CFI=1 -DCONFIG_AS_CFI_SIGNAL_FRAME=1 -pipe -Wno-sign-compare -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -mno-sse2 -mno-3dnow  -fno-omit-frame-pointer -fno-optimize-sibling-calls -g -Wdeclaration-after-statement -Wno-pointer-sign 
		endif
	endif #ifeq ($(nxp-vendor),)
	
	
	
	ifeq ($(nxp-vendor),whiterussian)
		nxp-cflags+=-D__KERNEL__ -I$(NDAS_KERNEL_PATH)/include -Wall -Wstrict-prototypes -Wno-trigraphs -Os -fno-strict-aliasing -fno-common -fomit-frame-pointer -funit-at-a-time -I$(NDAS_KERNEL_PATH)/include/asm/gcc -G 0 -mno-abicalls -fno-pic -pipe -finline-limit=100000 -mabi=32 -march=mips32 -Wa:-32 -Wa:-march=mips32 -Wa:-mips32 -Wa:--trap -nostdinc -iwithprefix 
	endif #ifeq ($(nxp-vendor),whiterussian)
	
	ifeq ($(nxp-vendor),kamikaze)
		ifeq ($(nxp-cpu),mipsel)
			nxp-cflags+=-Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Os  -mabi=32 -G 0 -mno-abicalls -fno-pic -pipe -msoft-float -ffreestanding  -march=mips32 -Wa,-mips32 -Wa,--trap -ffreestanding -fomit-frame-pointer  -fno-stack-protector -funit-at-a-time -Wdeclaration-after-statement -Wno-pointer-sign
		endif
	
		ifeq ($(nxp-cpu),i386)
			nxp-cflags+=-D__KERNEL__ -Wall -Wstrict-prototypes -Wno-trigraphs -O2 -fno-strict-aliasing -fno-common -fomit-frame-pointer -pipe -mpreferred-stack-boundary=2 -march=i486 -iwithprefix 
		endif
	endif #ifeq ($(nxp-vendor),kamikaze)

endif #ifeq ($(nxpo-ndaslinux),y)

ndaslinux:
	export NDAS_DEBUG=y; \
	export NDAS_EXTRA_CFLAGS="-DNDAS_HIX -DNDAS_EMU $(NDAS_EXTRA_CFLAGS)"; \
	make nxp-os=linux nxpo-ndaslinux=y nxpo-uni=y nxpo-debug=y tarball

ndaslinux64:
	export NDAS_DEBUG=y; \
	export NDAS_EXTRA_CFLAGS="-DNDAS_HIX $(NDAS_EXTRA_CFLAGS)";	\
	make nxp-os=linux nxpo-ndaslinux=y nxpo-uni=y nxp-cpu=x86_64	\
		nxpo-debug=y tarball

ndaslinux-test:
	export NDAS_DEBUG=y;	\
	export NDAS_KERNEL_PATH=/root/workspace/kernels/linux-2.6.25;	\
	export NDAS_KERNEL_VERSION=2.6.25;	\
	export NDAS_EXTRA_CFLAGS="-DNDAS_HIX $(NDAS_EXTRA_CFLAGS)";	\
	make nxp-os=linux nxpo-ndaslinux=y nxpo-uni=y nxpo-debug=y tarball
