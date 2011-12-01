nxpu-src-files=debug.c mem.c libc.c sal.c sync.c time.c thread.c net.c 
nxp-sal-src=$(addprefix $(nxp-root)/platform/ucosii/sal/, $(nxpu-src-files))
nxp-sal-obj=$(nxp-sal-src:$(nxp-root)/%.c=$(nxp-build)/%.o)
all: $(nxp-lib) 

