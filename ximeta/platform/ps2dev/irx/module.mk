######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################

nxpp-irx-import-obj= $(nxp-build)/$(nxpp-irx-path)/ndasirx_imports.o
nxpp-irx-export-obj= $(nxp-build)/$(nxpp-irx-path)/ndasirx_exports.o

nxpp-irx-src=$(nxpp-irx-path)/ndirx.c 
nxpp-irx-obj=$(nxpp-irx-src:%.c=$(nxp-build)/%.o) 

nxpp-irx-module=$(nxp-dist)/nd.irx

$(nxpp-irx-import-obj) $(nxpp-irx-export-obj):%.o: %.c
	$(call nxp_if_changed,cc_o_c)

# A rule to build imports.lst.
$(nxpp-irx-import-obj:%.o=%.c): $(nxpp-irx-path)/imports.lst
	echo "#include \"ndasirx_imports.h\"" > $@
	cat $< >> $@

# A rule to build exports.tab.
$(nxpp-irx-export-obj:%.o=%.c): $(nxpp-irx-path)/exports.tab 
	@mkdir -p $(@D)
	echo "#include \"ndasirx_exports.h\"" > $@
	cat $< >> $@

quiet_nxp_cmd_ld_irx_o = LD      $@
      nxp_cmd_ld_irx_o = $(LD) $(XPLAT_LDFLAGS) -o $@ $^

# ps2ndas.irx
# note that ps2dev binary is linked with libtest.o for the test purpose
$(nxpp-irx-module): $(nxp-fat32lib-obj) \
		$(nxpp-irx-export-obj) \
		$(nxpp-irx-import-obj) \
		$(nxpp-irx-obj) \
		$(nxp-sal-obj) \
		$(nxp-xlib-obj) \
		$(nxp-lpx-obj) \
		$(nxp-netdisk-obj) \
		$(nxp-test-obj) 
	#make $(nxp-fat32lib-obj)
	$(call nxp_if_changed,ld_irx_o)

$(nxp-build)/$(nxpp-irx-path)/.done: $(nxp-fat32lib-obj) $(nxpp-irx-module)
	@touch $@
