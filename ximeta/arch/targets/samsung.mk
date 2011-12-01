samsung-rel: #release
	make nxp-os=linux nxp-cpu=mips nxp-vendor=samsung nxpo-debug=n nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-bpc=y nxpo-hix=n nxpo-embedded=y nxpo-automated=y nxpo-nomemblkpriv=y nxpo-samsung-stb=y all

samsung-dev:
	make nxp-os=linux nxp-cpu=mips nxp-vendor=samsung nxpo-debug=y nxpo-sio=y nxpo-pnp=y nxpo-asy=y nxpo-bpc=y nxpo-hix=n nxpo-embedded=y nxpo-nomemblkpriv=y nxpo-samsung-stb=y all

samsung-clean:
	make nxp-os=linux nxp-cpu=mips nxp-vendor=samsung clean

