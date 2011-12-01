axis-exports:
	@if [[ \
		-z $$NDAS_KERNEL_VERSION || \
		-z $$NDAS_KERNEL_PATH || \
		-z $$NDAS_CROSS_COMPILE \
		]] ; then \
		echo "*** Followings must be exported ***"; \
		echo "*** Edit copy of target_axis.sh.default ***"; \
		echo "NDAS_KERNEL_VERSION (= 2.6.6)"; \
		echo "NDAS_KERNEL_PATH (ex: /linux-2.6.6"; \
		echo "NDAS_CROSS_COMPILE (ex: /usr/local/cris/bin/cris-axis-)"; \
		exit 1; \
	fi

axis-release: axis-exports
	make -s nxp-os=linux nxp-cpu=cris nxp-vendor=axis nxpo-release=y nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=n nxpo-bpc=y nxpo-persist=y nxpo-automated=y complete 

axis-debug: axis-exports
	make nxp-os=linux nxp-cpu=cris nxp-vendor=axis nxpo-debug=n nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=n nxpo-embedded=n nxpo-bpc=y nxpo-persist=y complete

axis-clean:
	make nxp-os=linux nxp-cpu=cris nxp-vendor=axis nxpo-debug=y nxpo-pnp=y clean

