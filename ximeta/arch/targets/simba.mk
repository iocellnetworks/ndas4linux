#
# Micronas Simba series release build
#
simba-rel-inline:
	export NDAS_CORE_EXTRA_CFLAGS="-isystem $(NDAS_KERNEL_PATH)/include/ $(NDAS_CORE_EXTRA_CFLAGS) " ;\
	export NDAS_EXTRA_CFLAGS="-D_MIPS -DLINUX -DNO_DEBUG_ESP $(NDAS_EXTRA_CFLAGS)" ;\
	make -s nxp-os=linux nxp-cpu=mips nxp-vendor=simba XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=y nxpo-pnp=y all nxpo-pnp=y nxpo-sio=y nxpo-automated=y complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

simba-rel:
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=64 -D_MIPS -DLINUX -DNO_DEBUG_ESP $(NDAS_EXTRA_CFLAGS)" ;\
	make -s nxp-os=linux nxp-cpu=mips nxp-vendor=simba XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y nxpo-automated=y all complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

simba-regular:
	export NDAS_EXTRA_CFLAGS="-DNDAS_IO_UNIT=64 -D_MIPS -DLINUX -DNO_DEBUG_ESP $(NDAS_EXTRA_CFLAGS)" ;\
	make -s nxp-os=linux nxp-cpu=mips nxp-vendor=simba XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y all complete

#
# Micronas Simba series debug build
#
simba-dev:
	export NDAS_EXTRA_CFLAGS="-DNDAS_IO_UNIT=64 -D_MIPS -DLINUX -DNO_DEBUG_ESP $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_DEBUG=y ; \
	make -s nxp-os=linux nxp-cpu=mips nxp-vendor=simba nxpo-debug=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y all complete

#
# Micronas Simba series clean
#
simba-clean:
	make nxp-os=linux nxp-cpu=mips nxp-vendor=simba clean
