#
# Topfield release build
#
topfield-rel-inline:
	@if [[ -z $$NDAS_TOPFIELD_PATH ]] ; then \
		echo NDAS_TOPFIELD_PATH is not set; \
		exit 1; \
	fi
	@export NDAS_KERNEL_PATH=$$NDAS_TOPFIELD_PATH/build_mipsel/linux;\
	export NDAS_KERNEL_ARCH=mipsel;\
	export NDAS_KERNEL_VERSION=2.6.12;\
	export NDAS_CROSS_COMPILE=mipsel-linux-; \
	export NDAS_CROSS_COMPILE_UM=mipsel-linux-; \
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls $$NDAS_EXTRA_CFLAGS";\
	export PATH=$$PATH:$$NDAS_TOPFIELD_PATH/staging_dir_mipsel/bin:$$NDAS_TOPFIELD_PATH/staging_dir_mipsel/usr/bin; \
	export NDAS_CORE_EXTRA_CFLAGS="-isystem $$NDAS_KERNEL_PATH/include/ $$NDAS_CORE_EXTRA_CFLAGS" ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=topfield XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=y nxpo-pnp=y all nxpo-pnp=y nxpo-sio=y nxpo-automated=y complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

topfield-automated:
	@if [[ -z $$NDAS_TOPFIELD_PATH ]] ; then \
		echo NDAS_TOPFIELD_PATH is not set; \
		exit 1; \
	fi
	@export NDAS_KERNEL_PATH=$$NDAS_TOPFIELD_PATH/build_mipsel/linux;\
	export NDAS_KERNEL_ARCH=mipsel;\
	export NDAS_KERNEL_VERSION=2.6.12;\
	export NDAS_CROSS_COMPILE=mipsel-linux-; \
	export NDAS_CROSS_COMPILE_UM=mipsel-linux-; \
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls $$NDAS_EXTRA_CFLAGS";\
	export PATH=$$PATH:$$NDAS_TOPFIELD_PATH/staging_dir_mipsel/bin:$$NDAS_TOPFIELD_PATH/staging_dir_mipsel/usr/bin; \
	make nxp-os=linux nxp-cpu=mipsel nxp-vendor=topfield XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y nxpo-automated=y ipkg

topfield-rel:
	export NDAS_KERNEL_ARCH=mipsel;\
	export NDAS_KERNEL_VERSION=2.6.12;\
	export NDAS_CROSS_COMPILE=mipsel-linux-; \
	export NDAS_CROSS_COMPILE_UM=mipsel-linux-; \
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls $$NDAS_EXTRA_CFLAGS";\
	export PATH=$$PATH:$$NDAS_CROSS_COMPILE_PATH; \
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=topfield XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y nxpo-automated=y complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

topfield-regular:
	@if [[ -z $$NDAS_TOPFIELD_PATH ]] ; then \
		echo NDAS_TOPFIELD_PATH is not set; \
		exit 1; \
	fi
	@export NDAS_KERNEL_PATH=$$NDAS_KERNEL_PATH/build_mipsel/linux;\
	export NDAS_KERNEL_ARCH=mipsel;\
	export NDAS_KERNEL_VERSION=2.6.12;\
	export NDAS_CROSS_COMPILE=mipsel-linux-; \
	export NDAS_CROSS_COMPILE_UM=mipsel-linux-; \
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls $$NDAS_EXTRA_CFLAGS";\
	export PATH=$$PATH:$$NDAS_TOPFIELD_PATH/staging_dir_mipsel/bin:$$NDAS_TOPFIELD_PATH/staging_dir_mipsel/usr/bin; \
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=topfield XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y all complete

#
# Linksys WRT54G series debug build
#
topfield-dev:
	@if [[ -z $$NDAS_TOPFIELD_PATH ]] ; then \
		echo NDAS_TOPFIELD_PATH is not set; \
		exit 1; \
	fi
	@export NDAS_KERNEL_PATH=$$NDAS_TOPFIELD_PATH/build_mipsel/linux;\
	export NDAS_KERNEL_ARCH=mipsel;\
	export NDAS_KERNEL_VERSION=2.6.12;\
	export NDAS_CROSS_COMPILE=mipsel-linux-; \
	export NDAS_CROSS_COMPILE_UM=mipsel-linux-; \
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls $$NDAS_EXTRA_CFLAGS";\
	export PATH=$$PATH:$$NDAS_TOPFIELD_PATH/staging_dir_mipsel/bin:$$NDAS_TOPFIELD_PATH/staging_dir_mipsel/usr/bin; \
	export NDAS_DEBUG=y ; \
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=topfield nxpo-debug=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y ipkg

#
# Linksys WRT54G series clean
#
topfield-clean:
	make nxp-os=linux nxp-cpu=mipsel nxp-vendor=topfield clean
