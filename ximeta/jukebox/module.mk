nxp-jukebox-path=jukebox
nxp-jukebox-src=$(wildcard $(nxp-jukebox-path)/*.c)
nxp-jukebox-obj=$(nxp-jukebox-src:%.c=$(nxp-build)/%.o)
