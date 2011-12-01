orion-exports:
	@if [[ \
		-z $$NDAS_KERNEL_VERSION || \
		-z $$NDAS_KERNEL_PATH || \
		-z $$NDAS_CROSS_COMPILE || \
		-z $$NDAS_KERNEL_ARCH \
		]] ; then \
		echo "*** Followings must be exported ***"; \
		echo "*** Edit copy of target_orion.sh.default ***"; \
		echo "NDAS_KERNEL_VERSION (= 2.6.6)"; \
		echo "NDAS_KERNEL_PATH (ex: /linux-2.6.6"; \
		echo "NDAS_CROSS_COMPILE (ex: /toolchain/bin/arm-elf-)"; \
		echo "NDAS_KERNEL_ARCH (ex: arm)"; \
		exit 1; \
	fi

orion-release: orion-exports
	make -s nxp-os=linux nxp-cpu=arm nxp-vendor=orion nxpo-release=y nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=n nxpo-bpc=y nxpo-persist=y nxpo-automated=y nxpo-emu=y complete 

orion-debug: orion-exports
	make nxp-os=linux nxp-cpu=arm nxp-vendor=orion nxpo-debug=n nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=n nxpo-bpc=y nxpo-persist=y complete

orion-emu-rel: orion-exports
	make nxp-os=linux nxp-cpu=arm nxp-vendor=orion nxpo-release=y nxpo-debug=n nxpo-sio=n nxpo-hix=n nxpo-asy=y \
		nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-automated=y nxpo-emu=y nxpo-nolanscsi=y nxpo-embedded=y complete

orion-emu-debug: orion-exports
	NDAS_DEBUG=y make nxp-os=linux nxp-cpu=arm nxp-vendor=orion nxpo-release=n nxpo-debug=y nxpo-sio=n nxpo-hix=n nxpo-asy=y \
		nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-automated=y nxpo-emu=y nxpo-embedded=y nxpo-nolanscsi=y complete

orion-clean:
	make nxp-os=linux nxp-cpu=arm nxp-vendor=orion nxpo-debug=y nxpo-pnp=y clean

