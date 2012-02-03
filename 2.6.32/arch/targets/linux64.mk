# linux 
linux64:
	make nxp-cpu=x86_64 nxp-os=linux nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y XPLAT_OBFUSCATE=y nxpo-bpc=y nxpo-raid=n all
linux64-automated:
	make nxp-cpu=x86_64 nxp-os=linux nxpo-release=y nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y XPLAT_OBFUSCATE=y nxpo-bpc=y nxpo-raid=n nxpo-automated=y all-automated

linux64-rel:
	make nxp-cpu=x86_64 nxp-os=linux nxpo-release=y nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y XPLAT_OBFUSCATE=y nxpo-bpc=y nxpo-raid=n all

linux64-rel-noobf:
	make nxp-cpu=x86_64 nxp-os=linux nxpo-release=y nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n all 

# linux driver 
# Without NDAS raid
linux64-dev:
	make nxp-cpu=x86_64 EXPORT_LOCAL=y nxpo-debug=y nxp-os=linux nxpo-sio=y nxpo-hix=y nxpo-uni=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n nxpo-probe=y nxpo-serial2id=y all
linux64-devall:
	NDAS_DEBUG=y make nxp-cpu=x86_64 EXPORT_LOCAL=y nxpo-debug=y nxp-os=linux nxpo-sio=y nxpo-hix=y nxpo-uni=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n nxpo-probe=y nxpo-serial2id=y complete

linux64-emu-rel:
	make nxp-cpu=x86_64 nxp-os=linux nxpo-release=y nxpo-debug=n nxpo-sio=n nxpo-hix=n nxpo-asy=y nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-automated=y nxpo-emu=y nxpo-nolanscsi=y emu

linux64-emu-debug:
	NDAS_DEBUG=y make nxp-cpu=x86_64 nxp-os=linux nxpo-release=n nxpo-debug=y nxpo-sio=n nxpo-hix=n nxpo-asy=y nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-automated=y nxpo-emu=y nxpo-nolanscsi=y emu



# With NDAS raid
linux64-raid-dev:
	make nxp-cpu=x86_64 EXPORT_LOCAL=y nxpo-debug=y nxp-os=linux nxpo-sio=y nxpo-hix=y nxpo-uni=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=y all
linux64-clean:
	make nxp-cpu=x86_64 nxp-os=linux clean

