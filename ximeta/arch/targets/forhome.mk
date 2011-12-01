forhome-rel: #release
	export NDAS_KERNEL_VERSION=2.6.15
	export NDAS_KERNEL_PATH=/home/sjcho/4home/linux-2.6.15-pmc
	make nxp-os=linux nxp-cpu=mips nxp-vendor=forhome nxpo-debug=n nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-bpc=y nxpo-hix=n nxpo-embedded=n nxpo-automated=y all

forhome-dev:
	export NDAS_KERNEL_VERSION=2.6.15
	export NDAS_KERNEL_PATH=/home/sjcho/4home/linux-2.6.15-pmc
	make nxp-os=linux nxp-cpu=mips nxp-vendor=forhome nxpo-debug=y nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-bpc=y nxpo-hix=n nxpo-embedded=n all

forhome-clean:
	make nxp-os=linux nxp-cpu=mips nxp-vendor=forhome clean

