#
# Linksys WRT54G series release build
#

kamikaze:
	@if [[ -z $$KAMIKAZE_PATH ]] ; then \
		echo KAMIKAZE_PATH is not set; \
		exit 1; \
	fi
	export PATH=$(PATH):$(KAMIKAZE_PATH)/staging_dir_mipsel/bin:$(KAMIKAZE_PATH)/staging_dir_mipsel/usr/bin;			\
	export NDAS_KERNEL_PATH=$(KAMIKAZE_PATH)/build_mipsel/linux; 									\
	export NDAS_KERNEL_ARCH=mipsel; 												\
	export NDAS_KERNEL_VERSION=2.6.22; 												\
	export NDAS_CROSS_COMPILE=$(KAMIKAZE_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-; 					\
	export NDAS_CROSS_COMPILE_UM=$(KAMIKAZE_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-; 					\
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)";	\
	export NDAS_DEBUG=y; 														\
	make ARCH=mips CROSS_COMPILE=$(KAMIKAZE_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc- 					\
	-s nxp-os=linux nxp-cpu=mipsel nxp-vendor=kamikaze 										\
	XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n 									\
	nxpo-pnp=y nxpo-sio=y nxpo-automated=yes nxpo-bpc=y 										\
	ipkg
#
# Linksys WRT54G series clean
#

kamikaze-clean:
	make nxp-os=linux nxp-cpu=mipsel nxp-vendor=kamikaze clean

#
# Linksys WRT54G series release build
#

whiterussian:
	@if [[ -z $$WHITERUSSIAN_PATH ]] ; then \
		echo WHITERUSSIAN_PATH is not set; \
		exit 1; \
	fi
	export PATH=$(PATH):$(WHITERUSSIAN_PATH)/staging_dir_mipsel/bin:$(WHITERUSSIAN_PATH)/staging_dir_mipsel/usr/bin;		\
	export NDAS_KERNEL_PATH=$(WHITERUSSIAN_PATH)/build_mipsel/linux; 								\
	export NDAS_KERNEL_ARCH=mipsel; 												\
	export NDAS_KERNEL_VERSION=2.4.30; 												\
	export NDAS_CROSS_COMPILE=$(WHITERUSSIAN_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-; 					\
	export NDAS_CROSS_COMPILE_UM=$(WHITERUSSIAN_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc-; 					\
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls -DNDAS_SIGPENDING_OLD $(NDAS_EXTRA_CFLAGS)";	\
	export NDAS_DEBUG=y; 														\
	make ARCH=mips CROSS_COMPILE=$(WHITERUSSIAN_PATH)/staging_dir_mipsel/bin/mipsel-linux-uclibc- 					\
	-s nxp-os=linux nxp-cpu=mipsel nxp-vendor=whiterussian 										\
	XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n 									\
	nxpo-pnp=y nxpo-sio=y nxpo-automated=yes nxpo-bpc=y 										\
	ipkg
#
# Linksys WRT54G series clean
#

whiterussian-clean:
	make nxp-os=linux nxp-cpu=mipsel nxp-vendor=whiterussian clean




#
# Linksys WRT54G series release build
#
openwrt-rel-inline:
	@if [[ -z $$NDAS_OPENWRT_PATH ]] ; then \
		echo NDAS_OPENWRT_PATH is not set; \
		exit 1; \
	fi
	@export NDAS_KERNEL_PATH=$$NDAS_OPENWRT_PATH/build_mipsel/linux;\
	export NDAS_KERNEL_ARCH=mipsel;\
	export NDAS_KERNEL_VERSION=2.4.30;\
	export NDAS_CROSS_COMPILE=mipsel-linux-; \
	export NDAS_CROSS_COMPILE_UM=mipsel-linux-; \
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls -DNDAS_SIGPENDING_OLD $$NDAS_EXTRA_CFLAGS";\
	export PATH=$$PATH:$$NDAS_OPENWRT_PATH/staging_dir_mipsel/bin:$$NDAS_OPENWRT_PATH/staging_dir_mipsel/usr/bin; \
	export NDAS_CORE_EXTRA_CFLAGS="-isystem $$NDAS_KERNEL_PATH/include/ $$NDAS_CORE_EXTRA_CFLAGS" ;\
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=openwrt XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=y nxpo-pnp=y all nxpo-pnp=y nxpo-sio=y nxpo-automated=y ipkg
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

# Build tarball without license agreement
openwrt-automated:
	@if [[ -z $$NDAS_OPENWRT_PATH ]] ; then \
		echo NDAS_OPENWRT_PATH is not set; \
		exit 1; \
	fi
	export PATH=$$PATH:$$NDAS_OPENWRT_PATH/staging_dir_mipsel/bin:$$NDAS_OPENWRT_PATH/staging_dir_mipsel/usr/bin; \
	make nxp-os=linux nxp-cpu=mipsel nxp-vendor=openwrt XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y nxpo-automated=y all-automated

openwrt-rel:
	@if [[ -z $$NDAS_OPENWRT_PATH ]] ; then \
		echo NDAS_OPENWRT_PATH is not set; \
		exit 1; \
	fi
	@export NDAS_KERNEL_PATH=$$NDAS_OPENWRT_PATH/build_mipsel/linux;\
	export NDAS_KERNEL_ARCH=mipsel;\
	export NDAS_KERNEL_VERSION=2.4.30;\
	export NDAS_CROSS_COMPILE=mipsel-linux-uclibc-; \
	export NDAS_CROSS_COMPILE_UM=mipsel-linux-uclibc-; \
	export NDAS_EXTRA_CFLAGS="-iwithprefix include -DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls -DNDAS_SIGPENDING_OLD $$NDAS_EXTRA_CFLAGS";\
	export PATH=$$NDAS_OPENWRT_PATH/staging_dir_mipsel/bin:$$NDAS_OPENWRT_PATH/staging_dir_mipsel/usr/bin:$$PATH; \
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=openwrt XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y nxpo-automated=y ipkg
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

openwrt-regular:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	@if [[ -z $$NDAS_OPENWRT_PATH ]] ; then \
		echo NDAS_OPENWRT_PATH is not set; \
		exit 1; \
	fi
	@export NDAS_KERNEL_ARCH=mipsel;\
	export NDAS_KERNEL_VERSION=2.4.30;\
	export NDAS_CROSS_COMPILE=mipsel-linux-uclibc-; \
	export NDAS_CROSS_COMPILE_UM=mipsel-linux-uclibc-; \
	export NDAS_EXTRA_CFLAGS="-iwithprefix include -DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls -DNDAS_SIGPENDING_OLD $$NDAS_EXTRA_CFLAGS";\
	export PATH=:$$NDAS_OPENWRT_PATH/staging_dir_mipsel/bin:$$NDAS_OPENWRT_PATH/staging_dir_mipsel/usr/bin:$$PATH; \
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=openwrt XPLAT_OBFUSCATE=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-automated=yes nxpo-bpc=y all

#
# Linksys WRT54G series debug build
#
openwrt-debug:
	@if [[ -z $$NDAS_OPENWRT_PATH ]] ; then \
		echo NDAS_OPENWRT_PATH is not set; \
		exit 1; \
	fi
	@export NDAS_KERNEL_PATH=$$NDAS_OPENWRT_PATH/build_mipsel/linux;\
	export NDAS_KERNEL_ARCH=mipsel;\
	export NDAS_KERNEL_VERSION=2.4.30;\
	export NDAS_CROSS_COMPILE=mipsel-linux-uclibc-; \
	export NDAS_CROSS_COMPILE_UM=mipsel-linux-uclibc-; \
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=64 -DLINUX -DNO_DEBUG_ESP -mlong-calls -DNDAS_SIGPENDING_OLD $$NDAS_EXTRA_CFLAGS";\
	export PATH=$$PATH:$$NDAS_OPENWRT_PATH/staging_dir_mipsel/bin:$$NDAS_OPENWRT_PATH/staging_dir_mipsel/usr/bin; \
	export NDAS_DEBUG=y ; \
	make -s nxp-os=linux nxp-cpu=mipsel nxp-vendor=openwrt nxpo-debug=y nxpo-persist=y nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-sio=y nxpo-bpc=y ipkg

#
# Linksys WRT54G series clean
#
openwrt-clean:
	make nxp-os=linux nxp-cpu=mipsel nxp-vendor=openwrt clean
