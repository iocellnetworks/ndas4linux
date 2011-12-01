#
# Roku HD1000 release build
#
roku-rel-inline:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	export NDAS_EXTRA_CFLAGS="-D_MIPSEL -LINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_KERNEL_VERSION=2.4.18_dev-xilleon_x220 ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=roku XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=y nxpo-bpc=y nxpo-pnp=y nxpo-sio=y all complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

roku-automated:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	export NDAS_EXTRA_CFLAGS="-D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_KERNEL_VERSION=2.4.18_dev-xilleon_x220 ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=roku XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-bpc=y nxpo-automated=y nxpo-pnp=y nxpo-sio=y all-automated complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

roku-demo:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	export NDAS_EXTRA_CFLAGS="-D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_KERNEL_VERSION=2.4.18_dev-xilleon_x220 ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=roku XPLAT_OBFUSCATE=y nxpo-persist=n nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-bpc=y nxpo-automated=y nxpo-pnp=y nxpo-sio=y all-automated complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

roku-rel:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	export NDAS_EXTRA_CFLAGS="-D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_KERNEL_VERSION=2.4.18_dev-xilleon_x220 ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=roku XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-bpc=y nxpo-sio=y nxpo-embedded=n all complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

roku:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	export NDAS_EXTRA_CFLAGS="-DNDAS_IO_UNIT=48 -D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_KERNEL_VERSION=2.4.18_dev-xilleon_x220 ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=roku XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n all complete

#
# Roku HD1000 debug build
#
roku-dev:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	export NDAS_EXTRA_CFLAGS="-DNDAS_IO_UNIT=48 -D_MIPSEL -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_CROSS_COMPILE=mipsel-linux- ;\
	export NDAS_KERNEL_VERSION=2.4.18_dev-xilleon_x220 ;\
	export NDAS_DEBUG=y ; \
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=roku nxpo-debug=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-bpc=y nxpo-sio=y all complete

#
# Roku HD1000 clean
#
roku-clean:
	make nxp-os=linux nxp-cpu=mipsel clean
