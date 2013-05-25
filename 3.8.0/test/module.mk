nxp-test-path=test
nxp-test-src=$(wildcard $(nxp-root)/$(nxp-test-path)/*.c)
nxp-test-obj=$(nxp-test-src:$(nxp-root)/%.c=$(nxp-build)/%.o)

#$(nxp-test-obj) : $(nxp-build)/%.o : %.c
#	@mkdir -p $(@D)
#	$(CC) $(nxp-cflags) -c -o $@ $<

