# linux 
linux64:
	make nxp-cpu=x86_64 nxp-os=linux nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n all
linux64-rel:
	make nxp-cpu=x86_64 nxp-os=linux nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n nxpo-release=y all
linux64-automated:
	make nxp-cpu=x86_64 nxp-os=linux nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n nxpo-release=y nxpo-automated=y all-automated

# linux driver 
# Without NDAS raid
linux64-dev:
	             make nxp-cpu=x86_64 nxp-os=linux nxpo-sio=y nxpo-hix=y nxpo-uni=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n nxpo-debug=y nxpo-probe=y nxpo-serial2id=y EXPORT_LOCAL=y all
linux64-devall:
	NDAS_DEBUG=y make nxp-cpu=x86_64 nxp-os=linux nxpo-sio=y nxpo-hix=y nxpo-uni=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n nxpo-debug=y nxpo-probe=y nxpo-serial2id=y EXPORT_LOCAL=y complete


linux64-emu-rel:
	             make nxp-cpu=x86_64 nxp-os=linux nxpo-emu=y nxpo-sio=n nxpo-hix=n nxpo-asy=y nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-release=y nxpo-automated=y nxpo-debug=n nxpo-nolanscsi=y emu
linux64-emu-debug:
	NDAS_DEBUG=y make nxp-cpu=x86_64 nxp-os=linux nxpo-emu=y nxpo-sio=n nxpo-hix=n nxpo-asy=y nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-release=n nxpo-automated=y nxpo-debug=y nxpo-nolanscsi=y emu

# With NDAS raid
linux64-raid-dev:
	make nxp-cpu=x86_64 EXPORT_LOCAL=y nxpo-debug=y nxp-os=linux nxpo-sio=y nxpo-hix=y nxpo-uni=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=y all

linux64-clean:
	make nxp-cpu=x86_64 nxp-os=linux clean

