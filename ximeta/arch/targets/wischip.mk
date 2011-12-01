#
# Micronas Wischip series release build
#
wischip-rel-inline:
	export NDAS_CORE_EXTRA_CFLAGS="-isystem $(NDAS_KERNEL_PATH)/include/ $(NDAS_CORE_EXTRA_CFLAGS) " ;\
	export NDAS_EXTRA_CFLAGS="-D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_KERNEL_VERSION=2.6.12-rc2 ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=wischip XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-sio=y nxpo-release=y nxpo-embedded=y nxpo-pnp=y nxpo-probe=y all nxpo-automated=y complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

wischip-rel:
	export NDAS_EXTRA_CFLAGS="-D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_KERNEL_VERSION=2.6.12-rc2 ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=wischip XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y nxpo-probe=y nxpo-automated=y all complete

wischip-regular:
	export NDAS_EXTRA_CFLAGS="-D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_KERNEL_VERSION=2.6.12-rc2 ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=wischip XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y all complete

#
# Micronas Wischip series debug build
#
wischip-debug:
	export NDAS_EXTRA_CFLAGS="-D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_DEBUG=y ; \
	export NDAS_KERNEL_VERSION=2.6.12-rc2 ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=wischip nxpo-debug=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y all complete

#
# Micronas Wischip series clean
#
wischip-clean:
	make nxp-os=linux nxp-cpu=mipsel nxp-vendor=wischip clean
