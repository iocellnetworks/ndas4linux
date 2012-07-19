nxp-xixfsevent-path=xixfsevent
nxp-xixfsevent-src=$(wildcard $(nxp-xixfsevent-path)/*.c)
nxp-xixfsevent-obj=$(nxp-xixfsevent-src:%.c=$(nxp-build)/%.o)
	
