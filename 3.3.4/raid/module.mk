nxp-raid-path=raid
nxp-raid-src=$(wildcard $(nxp-raid-path)/*.c)
nxp-raid-obj=$(nxp-raid-src:%.c=$(nxp-build)/%.o)
	
