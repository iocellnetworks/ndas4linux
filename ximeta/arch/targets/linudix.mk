#
# Linudix Camera
#

linudix-rel-inline:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	export NDAS_CORE_EXTRA_CFLAGS="-isystem $(NDAS_KERNEL_PATH)/include/ $(NDAS_CORE_EXTRA_CFLAGS) " ;\
	export NDAS_EXTRA_CFLAGS="-D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_KERNEL_VERSION=2.4.21;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=linudix XPLAT_OBFUSCATE=y nxpo-persist=n nxpo-asy=y nxpo-release=y nxpo-embedded=y nxpo-pnp=y all nxpo-pnp=y nxpo-sio=y nxpo-automated=y complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

linudix-rel:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	export NDAS_EXTRA_CFLAGS="-DNDAS_SIGPENDING_OLD -DNDAS_SERIAL -DNAS_IO_UNIT=32 -D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_KERNEL_VERSION=2.4.21;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=linudix XPLAT_OBFUSCATE=n nxpo-persist=n nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y nxpo-automated=y all complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

linudix-regular:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	export NDAS_EXTRA_CFLAGS="-DNDAS_IO_UNIT=64 -D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_KERNEL_VERSION=2.4.21;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=linudix XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y all complete

#
# Linksys WRT54G series debug build
#
linudix-dev:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	export NDAS_EXTRA_CFLAGS="-DNDAS_SIGPENDING_OLD -DNDAS_SERIAL -DNDAS_IO_UNIT=32 -D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_DEBUG=y ; \
	export NDAS_KERNEL_VERSION=2.4.21;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=linudix nxpo-debug=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y all complete

#
# Linksys WRT54G series clean
#
linudix-clean:
	make nxp-os=linux nxp-cpu=mipsel nxp-vendor=linudix clean
