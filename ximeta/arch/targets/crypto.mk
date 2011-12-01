crypto-exports:
	@if [[ \
		-z $$NDAS_KERNEL_VERSION || \
		-z $$NDAS_KERNEL_PATH || \
		-z $$NDAS_CROSS_COMPILE \
		]] ; then \
		echo "*** Followings must be exported ***"; \
		echo "*** Edit copy of target_crypto.sh.default ***"; \
		echo "NDAS_KERNEL_VERSION (= 2.6.6)"; \
		echo "NDAS_KERNEL_PATH (ex: /linux-2.6.6"; \
		echo "NDAS_CROSS_COMPILE (ex: /usr/local/arm/bin/arm-linux-)"; \
		exit 1; \
	fi

crypto-release: crypto-exports
	make -s nxp-os=linux nxp-cpu=arm nxp-vendor=crypto nxpo-release=y nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=n nxpo-bpc=y nxpo-persist=y nxpo-automated=y complete 

crypto-debug: crypto-exports
	make nxp-os=linux nxp-cpu=arm nxp-vendor=crypto nxpo-debug=n nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=n nxpo-bpc=y nxpo-persist=y complete

crypto-clean:
	make nxp-os=linux nxp-cpu=arm nxp-vendor=crypto nxpo-debug=y nxpo-pnp=y clean

