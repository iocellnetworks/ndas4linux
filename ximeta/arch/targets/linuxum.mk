linuxum-dev:
	export NDAS_EXTRA_CFLAGS="-DDEBUG_MEMORY_LEAK"
	make EXPORT_LOCAL=y nxpo-debug=y nxpo-sio=y nxpo-asy=y nxpo-hix=y nxpo-embedded=n nxpo-bpc=y nxpo-persist=y nxpo-automated=y nxpo-pnp=y nxpo-raid=n all

linuxum-rel:
	make EXPORT_LOCAL=y nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-hix=y nxpo-debug=n nxpo-embedded=n nxpo-bpc=y nxpo-persist=y nxpo-automated=y nxpo-probe=y all

linuxum-emu:
	make nxp-os=linuxum nxpo-release=n nxpo-debug=n nxpo-uni=y nxpo-sio=n nxpo-hix=n nxpo-asy=y nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-automated=y nxpo-emu=y nxpo-nolanscsi=y all

linuxum-emu-debug:
	make nxp-os=linuxum nxpo-release=n nxpo-debug=y nxpo-uni=y nxpo-sio=n nxpo-hix=n nxpo-asy=y nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-automated=y nxpo-emu=y nxpo-nolanscsi=y all


linuxum-clean:
	make nxp-os=linuxum clean


