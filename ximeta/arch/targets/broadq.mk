broadq-dev:
	make nxp-os=broadq nxp-cpu=ps2 nxpo-debug=y nxpo-pnp=y all
broadq:
	make nxp-os=broadq nxp-cpu=ps2 XPLAT_OBFUSCATE=y XPLAT_OPTIMIZATION="-O0" nxpo-pnp=y all
broadq-clean:
	make nxp-os=broadq nxp-cpu=ps2 clean
