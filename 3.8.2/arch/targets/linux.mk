# linux 
linux:
	make nxp-os=linux nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n all
# linux with xixfs support
linux-xix:
	make nxp-os=linux nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n nxpo-xixfsevent=y all
# linux  NDAS Raid is default
linux-automated:
	make nxp-os=linux nxpo-release=y nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y XPLAT_OBFUSCATE=y nxpo-bpc=y nxpo-raid=n nxpo-automated=y all-automated

linux-emu-rel:
	make nxp-os=linux nxpo-release=y nxpo-debug=n nxpo-sio=n nxpo-hix=n nxpo-asy=y nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-automated=y nxpo-emu=y nxpo-nolanscsi=y complete

linux-emu-embed:
	make nxp-os=linux nxpo-release=y nxpo-debug=n nxpo-sio=n nxpo-hix=n nxpo-asy=y nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-automated=y nxpo-emu=y nxpo-nolanscsi=y nxpo-embedded=y all-automated
	
linux-emu-debug:
	NDAS_DEBUG=y make nxp-os=linux nxpo-release=n nxpo-debug=y nxpo-sio=n nxpo-hix=n nxpo-asy=y nxpo-persist=n nxpo-pnp=n nxpo-bpc=y nxpo-raid=n nxpo-automated=y nxpo-emu=y nxpo-nolanscsi=y complete

linux-rel:
	make nxp-os=linux nxpo-release=y nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y XPLAT_OBFUSCATE=y nxpo-bpc=y nxpo-raid=n all

linux-rel-s:
	make -s nxp-os=linux nxpo-release=y nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y XPLAT_OBFUSCATE=y nxpo-bpc=y nxpo-raid=n all

linux-rel-pogo:
	make nxp-cpu=arm nxp-os=linux nxpo-release=y nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n all

linux-rel-noob:
	make nxp-os=linux nxpo-release=y nxpo-uni=y nxpo-sio=y nxpo-hix=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n all 

# linux driver 
# Without NDAS raid
linux-raid-dev:
	make EXPORT_LOCAL=y nxpo-debug=y nxp-os=linux nxpo-sio=y nxpo-hix=y nxpo-uni=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=y nxpo-probe=y all

# With NDAS raid
linux-dev:
	make EXPORT_LOCAL=y nxpo-debug=y nxp-os=linux nxpo-sio=y nxpo-hix=y nxpo-uni=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n nxpo-probe=y all

linux-devall:
	NDAS_DEBUG=y make EXPORT_LOCAL=y nxpo-debug=y nxp-os=linux nxpo-sio=y nxpo-hix=y nxpo-uni=y nxpo-asy=y nxpo-persist=y nxpo-pnp=y nxpo-bpc=y nxpo-raid=n nxpo-probe=y nxpo-emu=y complete

linux-clean:
	make nxp-os=linux clean

