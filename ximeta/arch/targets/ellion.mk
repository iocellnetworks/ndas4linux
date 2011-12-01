# Target rules for Ellion. For Media share, use ellion.mk
ELLION_MAKE_COMMON_PARAMS:=nxp-os=linux nxp-cpu=mipsel nxp-vendor=ellion nxpo-sio=y nxpo-pnp=y \
		nxpo-asy=y nxpo-hix=n nxpo-bpc=y \
		nxpo-automated=y nxpo-probe=n nxpo-nomemblkpriv=y

ellion-package:
	make -C $(nxp-build)/ndas-$(nxp-build-version) clean
	tar zcf ellion.$(nxp-build-version).tgz -C $(nxp-build)/ellion_linux ndas-$(nxp-build-version)

ellion-exports:
	@if [[ \
		-z $$NDAS_KERNEL_VERSION || \
		-z $$NDAS_KERNEL_PATH || \
		-z $$NDAS_CROSS_COMPILE \
		]] ; then \
		echo "*** Followings must be exported ***"; \
		echo "NDAS_KERNEL_VERSION (= 2.6.12)"; \
		echo "NDAS_KERNEL_PATH (ex: /linux-2.6.12-ellion"; \
		echo "NDAS_CROSS_COMPILE (ex: /toolchain/bin/mipsel-linux-)"; \
		exit 1; \
	fi

ellion-tarball: ellion-exports
	make -s nxpo-release=y $(ELLION_MAKE_COMMON_PARAMS) all

ellion-rel: ellion-exports
	make -s nxpo-release=y $(ELLION_MAKE_COMMON_PARAMS) complete

ellion-emu-only: ellion-exports
	make -s nxpo-release=y $(ELLION_MAKE_COMMON_PARAMS)  XPLAT_OBFUSCATE=y nxpo-uni=y nxpo-sio=n nxpo-persist=n nxpo-pnp=n nxpo-raid=n nxpo-emu=y nxpo-nolanscsi=y nxpo-juke=n nxpo-probe=n emu

ellion-emu-only-debug: ellion-exports
	export NDAS_DEBUG=y ; \
	make -s nxpo-release=n $(ELLION_MAKE_COMMON_PARAMS) nxpo-debug=y nxpo-uni=y nxpo-sio=n nxpo-persist=n nxpo-pnp=n nxpo-raid=n nxpo-emu=y nxpo-nolanscsi=y nxpo-juke=n nxpo-probe=n emu

ellion-with-emu: ellion-exports
	make -s nxpo-release=y $(ELLION_MAKE_COMMON_PARAMS) XPLAT_OBFUSCATE=y nxpo-emu=y nxpo-juke=n complete

ellion-with-emu-debug: ellion-exports
	export NDAS_DEBUG=y ; \
	make -s nxpo-release=n $(ELLION_MAKE_COMMON_PARAMS) nxpo-debug=y nxpo-emu=y nxpo-juke=n complete


ellion-debug: ellion-exports
	export NDAS_DEBUG=y ; \
	make nxpo-debug=y $(ELLION_MAKE_COMMON_PARAMS) complete

ellion-clean:
	make $(ELLION_MAKE_COMMON_PARAMS) clean 

ellion-v1-rel: ellion-exports
	make -s nxpo-release=y nxpo-embedded=y $(ELLION_MAKE_COMMON_PARAMS) complete

ellion-v1-debug: ellion-exports
	export NDAS_DEBUG=y ; \
	make nxpo-debug=y nxpo-embedded=y $(ELLION_MAKE_COMMON_PARAMS) complete

ellion-v1-with-emu: ellion-exports
	make -s nxpo-release=y nxpo-embedded=y $(ELLION_MAKE_COMMON_PARAMS) XPLAT_OBFUSCATE=y nxpo-emu=y nxpo-juke=n complete

ellion-v1-with-emu-debug: ellion-exports
	make -s nxpo-release=n $(ELLION_MAKE_COMMON_PARAMS) nxpo-debug=y nxpo-emu=y nxpo-juke=n complete

