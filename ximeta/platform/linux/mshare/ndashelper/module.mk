#-------------------------------------------------------------------------
# Copyright (c) 2002-2005, XIMETA, Inc., IRVINE, CA, USA.
# All rights reserved.
#
# LICENSE TERMS
#
# The free distribution and use of this software in both source and binary 
# form is allowed (with or without changes) provided that:
#
#   1. distributions of this source code include the above copyright 
#      notice, this list of conditions and the following disclaimer;
#
#   2. distributions in binary form include the above copyright
#      notice, this list of conditions and the following disclaimer
#      in the documentation and/or other associated materials;
#
#   3. the copyright holder's name is not used to endorse products 
#      built using this software without specific written permission. 
#      
# ALTERNATIVELY, provided that this notice is retained in full, this product
# may be distributed under the terms of the GNU General Public License (GPL),
# in which case the provisions of the GPL apply INSTEAD OF those given above.
# 
# DISCLAIMER
#
# This software is provided 'as is' with no explcit or implied warranties
# in respect of any properties, including, but not limited to, correctness 
# and fitness for purpose.
#-------------------------------------------------------------------------

nxpm-helper-path:=$(nxp-linux-addon-path)/ndashelper
nxpm-helper-src:=$(nxpm-helper-path)/ndas_api.c $(nxpm-helper-path)/ndas_helper.cpp
nxpm-helper-obj:=$(nxp-build)/$(nxpm-helper-path)/ndas_api.o $(nxp-build)/$(nxpm-helper-path)/ndas_helper.o

nxpm-helper-o:= $(nxp-build)/libndasHelper.o
nxpm-helper-a:= $(nxp-build)/libndasHelper.a

all: $(nxpm-helper-a) 
complete: $(nxpm-helper-a) 
$(nxpm-helper-o): $(nxpm-helper-obj)
	$(call nxp_if_changed,ld_o_o)

$(nxpm-helper-a): $(nxpm-helper-o)
	$(AR) rcs $@ $<

#
# To compile ndas_api.c
# TODO: This function should be used for any user level objects
#
nxp_cmd_helper_cc_o_c = $(CC) -Wp,-MD,$@.d $(nxp-public-cflags) \
		   -I $(nxpl-tarball-path)/inc	\
		   -I inc	\
                   $(XPLAT_OPTIMIZATION) -c -o $@ $<
#
# To compile ndas_helper.cpp
# TODO: This function should be used for any user level objects
#	
nxp_cmd_helper_cpp_o_cpp = $(CPP) -Wp,-MD,$@.d $(nxp-public-cflags) \
		   -I $(nxpl-tarball-path)/inc	\
		   -I inc	\
                   $(XPLAT_OPTIMIZATION) -c -o $@ $<

$(nxp-build)/$(nxpm-helper-path)/ndas_api.o: $(nxpm-helper-path)/ndas_api.c
	$(call nxp_if_changed,helper_cc_o_c)

$(nxp-build)/$(nxpm-helper-path)/ndas_helper.o: $(nxpm-helper-path)/ndas_helper.cpp
	$(call nxp_if_changed,helper_cpp_o_cpp)

clean-ndashelper:
	rm -f $(nxpm-helper-a) $(nxpm-helper-o)

clean: clean-ndashelper
