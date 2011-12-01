DVICO_MAKE_COMMON_PARAMS:=nxp-os=linux nxp-cpu=arm nxp-vendor=dvico nxpo-serial=y \
		nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=n nxpo-juke=n \
		nxpo-bpc=y nxpo-persist=y nxpo-automated=y nxpo-probe=y	

dvico-package:
	make -C $(nxp-build)/ndas-$(nxp-build-version) clean
	tar zcf ndas.dvico-$(nxp-build-version).tgz -C $(nxp-build)/dvico_linux ndas-$(nxp-build-version)

dvico-exports:
	@if [[ \
		-z $$NDAS_KERNEL_VERSION || \
		-z $$NDAS_KERNEL_PATH || \
		-z $$NDAS_CROSS_COMPILE \
		]] ; then \
		echo "*** Followings must be exported ***"; \
		echo "NDAS_KERNEL_VERSION (= 2.4.22)"; \
		echo "NDAS_KERNEL_PATH (ex: /linux-2.4.22-em86xx"; \
		echo "NDAS_CROSS_COMPILE (ex: /toolchain/bin/arm-elf-)"; \
		exit 1; \
	fi

dvico-tarball: dvico-exports
	make nxpo-release=y $(DVICO_MAKE_COMMON_PARAMS) all

dvico-release: dvico-exports
	make $(DVICO_MAKE_COMMON_PARAMS) all
	NDAS_EXTRA_CFLAGS="-DNDAS_IO_UNIT=32 -D_ARM -DNDAS_SIGPENDING_OLD" \
	NDAS_EXTRA_LDFLAGS="-elf2flt=\"-s32768\""    \
	make ndas_version=$(nxp-build-version) ndas_build=$(nxp-build-number) -C build_dvico/ndas-$(nxp-build-version)-$(nxp-build-number)

dvico-emu-only: dvico-exports
	make nxpo-release=y $(DVICO_MAKE_COMMON_PARAMS)  XPLAT_OBFUSCATE=y nxpo-uni=y nxpo-sio=n nxpo-persist=n nxpo-pnp=n nxpo-raid=n nxpo-emu=y nxpo-nolanscsi=y nxpo-juke=n nxpo-probe=n complete

dvico-emu-only-debug: dvico-exports
	make nxpo-release=n $(DVICO_MAKE_COMMON_PARAMS) nxpo-debug=y nxpo-uni=y nxpo-sio=n nxpo-persist=n nxpo-pnp=n nxpo-raid=n nxpo-emu=y nxpo-nolanscsi=y nxpo-juke=n nxpo-probe=n complete


dvico-with-emu: dvico-exports
	make nxpo-release=y $(DVICO_MAKE_COMMON_PARAMS) XPLAT_OBFUSCATE=y nxpo-emu=y nxpo-juke=n complete

dvico-with-emu-debug: dvico-exports
	make nxpo-release=n $(DVICO_MAKE_COMMON_PARAMS) nxpo-debug=y nxpo-emu=y nxpo-juke=n complete


dvico-debug: dvico-exports
	make nxpo-debug=y $(DVICO_MAKE_COMMON_PARAMS) all
	NDAS_EXTRA_CFLAGS="-DNDAS_IO_UNIT=16 -DNDAS_SERIAL=y -D_ARM -DNDAS_SIGPENDING_OLD" \
	NDAS_EXTRA_LDFLAGS="-elf2flt=\"-s32768\""    \
	NDAS_DEBUG=y \
	make ndas_version=$(nxp-build-version) ndas_build=$(nxp-build-number) -C build_dvico/ndas-$(nxp-build-version)-$(nxp-build-number)

dvico-clean:
	make $(DVICO_MAKE_COMMON_PARAMS) clean 

