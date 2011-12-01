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

ndas-addon-path:=addon

ifndef ROKU_SDK_ROOT
ifeq ($(NDAS_CROSS_COMPILE),)
ROKU_SDK_ROOT?=/usr/share/rokudev/usr
else
$(error Set ROKU_SDK_ROOT where roku native library exist)
endif
endif

ifndef ROKU_LOCAL
ifeq ($(NDAS_CROSS_COMPILE),)
ROKU_LOCAL?=/usr/local
else
$(error Set ROKU_LOCAL where the /usr/local/ of the Roku HD1000 directory is. Copy from HD1000 if you don't have)
endif
endif

ndas-addon-src:=$(wildcard $(ndas-addon-path)/*.cc)
ndas-addon-obj:=$(ndas-addon-src:%.cc=%.o)
roku-gui-app:=ndasmount.roku

ndas-addon-extra-cflags:= -I $(ROKU_LOCAL)/include \
	-I $(ROKU_SDK_ROOT)/usr/local/include/roku -Iinc 
	
roku-ldflags:= -L$(ROKU_LOCAL)/lib \
	-lCascade -lmad -ldvbpsi -lDeschutesAppCommon -lHDMachineX225 -lacl -lcore -lCascadeCore \
	-Wl,-rpath-link 

roku-bins:= roku.tar.gz	

all: $(roku-gui-app) $(roku-bins)

$(roku-bins): $(roku-gui-app) InstallNetD.roku UninstallNetD.roku ndas.jpg ndas.startup.script $(ndas-driver) $(ndas-ndasadmin-out) swap
	chmod a+x $(wildcard *.roku)
	tar zcvf $@ $(^F)

swap:
	#dd if=/dev/zero of=$@ bs=1024 count=65536
	#mkswap $@
	touch swap
	
$(roku-gui-app): $(ndas-addon-obj) 
	$(ndas-cc) -o $@ $^ $(roku-ldflags)
	cp $(ndas-addon-path)/ndas.jpg $(@D)

$(ndas-addon-obj): %.o : %.cc 
	$(ndas_cmd_app_cpp)
	
clean: clean-roku-gui
clean-roku-gui:
	-rm -rf $(roku-gui-app) $(ndas-addon-obj)
