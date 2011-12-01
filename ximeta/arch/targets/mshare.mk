MSHARE_DIST_BIN=ndas_sal.o ndas_core.o ndas_block.o ndasadmin
MSHARE_DIST_DES=dist/
MSHARE_MAKE_COMMON_PARAMS:=nxp-os=linux nxp-cpu=arm nxp-vendor=mshare nxpo-sio=y nxpo-pnp=y \
		nxpo-asy=y nxpo-hix=n nxpo-embedded=n nxpo-juke=y nxpo-bpc=y nxpo-persist=y \
		nxpo-automated=y nxpo-probe=y nxpo-serial2id=y\
		nxp-build=$(MSHARE_ARMUTILS_PATH)/build_arm/ndas nxp-dist=$(MSHARE_ARMUTILS_PATH)/build_arm/ndas/dist \
		nxpo-nomemblkpriv=y \
		nxpo-embedded=y

mshare-dev:
	make -C $(MSHARE_ARMUTILS_PATH) romfs
	make -C $(MSHARE_ARMUTILS_PATH) cramfs
	@echo Type "download serial romfs gz" on serial console
	@read
	uuencode $(MSHARE_ARMUTILS_PATH)/bin/cramfs-config-envision-EM8620L-romfs.bin.gz x> /dev/ttyS0

mshare-package:
	make -C $(nxp-build)/ndas-$(nxp-build-version) clean
	tar zcf mshare.$(nxp-build-version).tgz -C $(nxp-build)/mshare_linux ndas-$(nxp-build-version)

mshare-exports:
	@if [[ \
		-z $$NDAS_KERNEL_VERSION || \
		-z $$NDAS_KERNEL_PATH || \
		-z $$MSHARE_ARMUTILS_PATH || \
		-z $$NDAS_CROSS_COMPILE \
		]] ; then \
		echo "*** Followings must be exported ***"; \
		echo "*** Edit copy of target_mshare.sh.default ***"; \
		echo "NDAS_KERNEL_VERSION (= 2.4.22)"; \
		echo "NDAS_KERNEL_PATH (ex: /linux-2.4.22-em86xx"; \
		echo "MSHARE_ARMUTILS_PATH (ex: /armutils_2.4.80"; \
		echo "NDAS_CROSS_COMPILE (ex: /toolchain/bin/arm-elf-)"; \
		exit 1; \
	fi

mshare-tarball: mshare-exports
	make -s nxpo-release=y $(MSHARE_MAKE_COMMON_PARAMS) all

mshare-release: mshare-exports
	make -s nxpo-release=y $(MSHARE_MAKE_COMMON_PARAMS) complete

mshare-emu-only: mshare-exports
	make -s nxpo-release=y $(MSHARE_MAKE_COMMON_PARAMS)  XPLAT_OBFUSCATE=y nxpo-uni=y nxpo-sio=n nxpo-persist=n nxpo-pnp=n nxpo-raid=n nxpo-emu=y nxpo-nolanscsi=y nxpo-juke=n nxpo-probe=n complete

mshare-emu-only-debug: mshare-exports
	make -s nxpo-release=n $(MSHARE_MAKE_COMMON_PARAMS) nxpo-debug=y nxpo-uni=y nxpo-sio=n nxpo-persist=n nxpo-pnp=n nxpo-raid=n nxpo-emu=y nxpo-nolanscsi=y nxpo-juke=n nxpo-probe=n complete


mshare-with-emu: mshare-exports
	make -s nxpo-release=y $(MSHARE_MAKE_COMMON_PARAMS) XPLAT_OBFUSCATE=y nxpo-emu=y nxpo-juke=n complete

mshare-with-emu-debug: mshare-exports
	make -s nxpo-release=n $(MSHARE_MAKE_COMMON_PARAMS) nxpo-debug=y nxpo-emu=y nxpo-juke=n complete


mshare-debug: mshare-exports
	make nxpo-debug=y $(MSHARE_MAKE_COMMON_PARAMS) complete

mshare-clean:
	make $(MSHARE_MAKE_COMMON_PARAMS) clean 

