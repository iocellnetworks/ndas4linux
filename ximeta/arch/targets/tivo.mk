#
# TiVo 5.4 (2.4.20 kernel) for TCD540x0 SA-Series2.5/NightLight 
#
tivo-rel-inline:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	@if [[ -z $$NDAS_KERNEL_VERSION ]]; then \
		export NDAS_KERNEL_VERSION=2.4.20;\
	fi; \
	export NDAS_KERNEL_ARCH=mips;\
	export NDAS_CORE_EXTRA_CFLAGS="-isystem $(NDAS_KERNEL_PATH)/include $(NDAS_CORE_EXTRA_CFLAGS) " ;\
	export NDAS_EXTRA_CFLAGS="-D_MIPS -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	make -s nxp-os=linux nxp-cpu=mips nxp-vendor=tivo XPLAT_OBFUSCATE=y nxpo-persist=n nxpo-asy=y nxpo-release=y nxpo-embedded=y nxpo-pnp=y nxpo-probe=n nxpo-sio=y nxpo-automated=y all complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

tivo-automated:
	export NDAS_KERNEL_ARCH=mips;\
	export NDAS_EXTRA_CFLAGS="-DNDAS_SIGPENDING_OLD=y -DNAS_IO_UNIT=16 -D_MIPS -DLINUX -DNO_DEBUG_ESP -mlong-calls -DNDAS_PROBE=y $(NDAS_EXTRA_CFLAGS)" ;\
	make nxp-os=linux nxp-cpu=mips nxp-vendor=tivo XPLAT_OBFUSCATE=y nxpo-persist=n nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-pnp=y nxpo-probe=n nxpo-sio=y nxpo-bpc=y nxpo-automated=y all-automated

tivo-rel:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	@if [[ -z $$NDAS_KERNEL_VERSION ]]; then \
		export NDAS_KERNEL_VERSION=2.4.20;\
	fi; \
	export NDAS_KERNEL_ARCH=mips;\
	export NDAS_EXTRA_CFLAGS="-DNDAS_SIGPENDING_OLD=y -DNAS_IO_UNIT=16 -D_MIPS -DLINUX -DNO_DEBUG_ESP -mlong-calls -DNDAS_PROBE=y $(NDAS_EXTRA_CFLAGS)" ;\
	make -s nxp-os=linux nxp-cpu=mips nxp-vendor=tivo XPLAT_OBFUSCATE=y nxpo-persist=n nxpo-asy=y nxpo-release=y nxpo-embedded=n nxpo-pnp=y nxpo-probe=n nxpo-sio=y nxpo-bpc=y nxpo-automated=y all complete
	@echo Generated ndas-$(nxp-build-version)-$(nxp-build-number).tar.gz

tivo-regular:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	@if [[ -z $$NDAS_KERNEL_VERSION ]]; then \
		export NDAS_KERNEL_VERSION=2.4.20;\
	fi; \
	export NDAS_KERNEL_ARCH=mips;\
	export NDAS_EXTRA_CFLAGS="-DNDAS_IO_UNIT=16 -D_MIPS -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	make -s nxp-os=linux nxp-cpu=mips nxp-vendor=tivo XPLAT_OBFUSCATE=y nxpo-persist=n nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-probe=n nxpo-sio=y all complete

#
# TiVo 5.4 (2.4.20 kernel) for TCD540x0 SA-Series2.5/NightLight 
#
tivo-dev:
	@if [[ -z $$NDAS_KERNEL_PATH ]] ; then \
		echo NDAS_KERNEL_PATH is not set; \
		exit 1; \
	fi
	@if [[ -z $$NDAS_KERNEL_VERSION ]]; then \
		export NDAS_KERNEL_VERSION=2.4.20;\
	fi; \
	export NDAS_KERNEL_ARCH=mips;\
	export NDAS_EXTRA_CFLAGS="-DNDAS_SIGPENDING_OLD=y -DNDAS_IO_UNIT=16 -D_MIPS -DLINUX -DNO_DEBUG_ESP -mlong-calls $(NDAS_EXTRA_CFLAGS)" ;\
	export NDAS_DEBUG=y ; \
	make -s nxp-os=linux nxp-cpu=mips nxp-vendor=tivo nxpo-debug=y nxpo-persist=n nxpo-asy=y nxpo-embedded=n nxpo-pnp=y nxpo-probe=n nxpo-sio=y nxpo-bpc=y all complete

#
# TiVo 5.4 (2.4.20 kernel) for TCD540x0 SA-Series2.5/NightLight
#
tivo-clean:
	make nxp-os=linux nxp-cpu=mips nxp-vendor=tivo clean
