# Target rules for EM86xx. For Media share, use mshare.mk
EM86XX_DIST_BIN=ndas_sal.o ndas_core.o ndas_block.o ndasadmin
EM86XX_DIST_DES=dist/
EM86XX_MAKE_COMMON_PARAMS:=nxp-os=linux nxp-cpu=arm nxp-vendor=em86xx nxpo-sio=y nxpo-pnp=y \
		nxpo-asy=y nxpo-hix=n nxpo-bpc=y \
		nxpo-automated=y nxpo-probe=y nxpo-nomemblkpriv=y
#		nxp-build=$(EM86XX_ARMUTILS_PATH)/build_arm/ndas nxp-dist=$(EM86XX_ARMUTILS_PATH)/build_arm/ndas/dist

em86xx-package:
	make -C $(nxp-build)/ndas-$(nxp-build-version) clean
	tar zcf em86xx.$(nxp-build-version).tgz -C $(nxp-build)/em86xx_linux ndas-$(nxp-build-version)

em86xx-exports:
	@if [[ \
		-z $$NDAS_KERNEL_VERSION || \
		-z $$NDAS_KERNEL_PATH || \
		-z $$EM86XX_ARMUTILS_PATH || \
		-z $$NDAS_CROSS_COMPILE \
		]] ; then \
		echo "*** Followings must be exported ***"; \
		echo "NDAS_KERNEL_VERSION (= 2.4.22)"; \
		echo "NDAS_KERNEL_PATH (ex: /linux-2.4.22-em86xx"; \
		echo "EM86XX_ARMUTILS_PATH (ex: /armutils_2.4.80"; \
		echo "NDAS_CROSS_COMPILE (ex: /toolchain/bin/arm-elf-)"; \
		exit 1; \
	fi

em86xx-tarball: em86xx-exports
	make -s nxpo-release=y $(EM86XX_MAKE_COMMON_PARAMS) all

em86xx-rel: em86xx-exports
	make -s nxpo-release=y $(EM86XX_MAKE_COMMON_PARAMS) complete

em86xx-emu-only: em86xx-exports
	make -s nxpo-release=y $(EM86XX_MAKE_COMMON_PARAMS)  XPLAT_OBFUSCATE=y nxpo-uni=y nxpo-sio=n nxpo-persist=n nxpo-pnp=n nxpo-raid=n nxpo-emu=y nxpo-nolanscsi=y nxpo-juke=n nxpo-probe=n emu

em86xx-emu-only-debug: em86xx-exports
	export NDAS_DEBUG=y ; \
	make -s nxpo-release=n $(EM86XX_MAKE_COMMON_PARAMS) nxpo-debug=y nxpo-uni=y nxpo-sio=n nxpo-persist=n nxpo-pnp=n nxpo-raid=n nxpo-emu=y nxpo-nolanscsi=y nxpo-juke=n nxpo-probe=n emu

em86xx-with-emu: em86xx-exports
	make -s nxpo-release=y $(EM86XX_MAKE_COMMON_PARAMS) XPLAT_OBFUSCATE=y nxpo-emu=y nxpo-juke=n complete

em86xx-with-emu-debug: em86xx-exports
	export NDAS_DEBUG=y ; \
	make -s nxpo-release=n $(EM86XX_MAKE_COMMON_PARAMS) nxpo-debug=y nxpo-emu=y nxpo-juke=n complete


em86xx-debug: em86xx-exports
	export NDAS_DEBUG=y ; \
	make nxpo-debug=y $(EM86XX_MAKE_COMMON_PARAMS) complete

em86xx-clean:
	make $(EM86XX_MAKE_COMMON_PARAMS) clean 

v1-rel: em86xx-exports
	make -s nxpo-release=y nxpo-embedded=y $(EM86XX_MAKE_COMMON_PARAMS) complete

v1-debug: em86xx-exports
	export NDAS_DEBUG=y ; \
	make nxpo-debug=y nxpo-embedded=y $(EM86XX_MAKE_COMMON_PARAMS) complete

v1-with-emu: em86xx-exports
	make -s nxpo-release=y nxpo-embedded=y $(EM86XX_MAKE_COMMON_PARAMS) XPLAT_OBFUSCATE=y nxpo-emu=y nxpo-juke=n complete

v1-with-emu-debug: em86xx-exports
	make -s nxpo-release=n $(EM86XX_MAKE_COMMON_PARAMS) nxpo-debug=y nxpo-emu=y nxpo-juke=n complete

