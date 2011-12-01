######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################

#IOP_LDFLAGS:= -nostdlib -L$(PS2LIB)/iop/lib -lkernel # -s

nxpp-smapx-src:=$(wildcard $(nxpp-smapx-path)/*.c)
nxpp-smapx-obj:=$(nxpp-smapx-src:%.c=$(nxp-build)/%.o)

nxpp-smapx-module:=$(nxp-dist)/ps2smapx.irx

$(nxpp-smapx-module): $(nxpp-smapx-obj) $(nxp-build)/$(nxpp-smapx-path)/imports.o $(nxp-build)/$(nxpp-smapx-path)/exports.o
	$(call nxp_if_changed,ld_o_o)

$(nxp-build)/$(nxpp-smapx-path)/imports.c: $(nxpp-smapx-path)/imports.lst $(nxpp-smapx-path)/irx_imports.h
	echo "#include \"irx_imports.h\"" > $@
	cat  $(nxpp-smapx-path)/imports.lst >> $@
	
$(nxp-build)/$(nxpp-smapx-path)/imports.o: $(nxp-build)/$(nxpp-smapx-path)/imports.c 
	$(call nxp_if_changed,cc_o_c)
	
$(nxp-build)/$(nxpp-smapx-path)/exports.c: $(nxpp-smapx-path)/exports.tab
	echo "#include \"irx.h\"" > $@
	cat $< >> $@
	
$(nxp-build)/$(nxpp-smapx-path)/exports.o: $(nxp-build)/$(nxpp-smapx-path)/exports.c
	$(call nxp_if_changed,cc_o_c)
	
$(nxp-build)/$(nxpp-smapx-path)/.done: $(nxpp-smapx-module)
	@touch $@