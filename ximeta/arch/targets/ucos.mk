ucos-debug:
	make nxpo-debug=y nxp-os=ucosii nxp-cpu=arm nxp-vendor=syabas nxpo-pnp=y nxpo-sio=y nxpo-asy=n nxpo-embedded=y all
ucos-release:
	make nxpo-release=y nxpo-debug=n nxp-os=ucosii nxp-cpu=arm nxp-vendor=syabas nxpo-pnp=y nxpo-sio=y nxpo-asy=n nxpo-embedded=y all
ucos-clean:
	make nxp-os=ucosii nxp-cpu=arm TARGET_VENDDOR=syabas clean
