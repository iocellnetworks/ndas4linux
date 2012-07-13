nxp-netdisk-path=netdisk
nxp-netdisk-src=$(wildcard $(nxp-netdisk-path)/*.c)
nxp-netdisk-obj=$(nxp-netdisk-src:%.c=$(nxp-build)/%.o)
	
