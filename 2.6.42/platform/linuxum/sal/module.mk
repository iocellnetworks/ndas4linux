nxp-sal-src=$(wildcard $(nxp-root)/platform/linuxum/sal/*.c)
nxp-sal-obj=$(nxp-sal-src:$(nxp-root)/%.c=$(nxp-build)/%.o)
nxp-sal-cmd=$(nxp-sal-src:$(nxp-root)/%.c=$(nxp-build)/%.cmd)
nxp-sal-dep=$(nxp-sal-src:$(nxp-root)/%.c=$(nxp-build)/%.dep)

linuxum-sal-clean:
	rm -rf $(nxp-sal-obj) $(nxp-sal-cmd) $(nxp-sal-dep)
	