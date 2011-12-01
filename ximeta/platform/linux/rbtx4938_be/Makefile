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
# The top most directory of ndas driver
ndas_root?=$(shell pwd)

NDAS_CROSS_COMPILE:=mips-unknown-linux-gnu-

NDAS_KERNEL_VERSION=2.6.10

# Guess kernel source path, if environment variable is not set
NDAS_KERNEL_PATH:=/root/src/linux-2.6.10-r4k/build_be

include $(ndas_root)/lib.mk
include $(ndas_root)/sal/module.mk
include $(ndas_root)/block/module.mk
include $(ndas_root)/admin/module.mk


#
# tools
#
ndas-cross:=$(NDAS_CROSS_COMPILE)
ndas-cc:=$(ndas-cross)gcc
ndas-cpp:=$(ndas-cross)g++
ndas-ld:=$(ndas-cross)ld
ndas-ar:=$(ndas-cross)ar
#
#
# flags for tools
#
ndas-app-cflags:=-DLINUX -I$(ndas_root)/inc -D_MIPS -DNDAS_WRITE=1
ndas-app-ldflags:= -static
ndas-kernel-cflags:= -DLINUX -I$(ndas_root)/inc -DNDAS_IO_UNIT=32 -D_MIPS -DXPLAT_EMBEDDED
ndas-kernel-ldflags:=$(call ndas_kernel_variable,LDFLAGS) -S
ifdef NDAS_DEBUG
ndas-app-cflags+= -DDEBUG 
ndas-kernel-cflags+= -DDEBUG
include $(ndas_root)/debuglevel.mk
endif

ndas_cmd_app_ld= $(ndas-cc) $(ndas-app-ldflags) -o $@ $^
ndas_cmd_app_cc= $(ndas-cc) $(ndas-app-cflags) -c -o $@ $<

#
# Linux Module driver file
#

# Vendor specific addon programs or scripts
-include $(ndas_root)/addon/module.mk

ndas-app-cflags+= $(NDAS_EXTRA_CFLAGS) $(ndas-addon-extra-cflags)
ndas-kernel-cflags+= $(NDAS_EXTRA_CFLAGS)

#
# Linux Module Source
#
ndas-module-src:=ndas_sal_main.c ndas_core_main.c ndas_block_main.c
ndas-module-obj:=ndas_sal_main.o ndas_core_main.o ndas_block_main.o

#
# XIMETA NDAS driver administration tools
#

ndas-driver:=$(ndas_root)/ndas_sal.ko $(ndas_root)/ndas_core.ko $(ndas_root)/ndas_block.ko
ndas-ndasadmin-out=ndasadmin
all: $(ndas-driver) $(ndas-ndasadmin-out)

# module extension is *.ko in 2.6 kerenl
#
# Makefile for 2.6 kernel
#
ifneq ($(KERNELRELEASE),)
EXTRA_CFLAGS += $(ndas-kernel-cflags)
#
# ndas main
#
# Added DEBUG define if applicable
obj-m:= ndas_sal.o ndas_core.o ndas_block.o
ndas_sal-objs:=  $(ndas-sal-obj) ndas_sal_main.o
ndas_core-objs:=  ndas.o ndas_core_main.o
ndas_block-objs:=  $(ndas-block-obj) ndas_block_main.o
ndas_export-objs := $(ndas-sal-obj) ndas_core_main.o 

$(ndas-block-obj) $(ndas-sal-obj) $(ndas-module-obj): %.o : /%.c
	@mkdir -p $(@D)
	$(call if_changed,cc_o_c)

$(ndas_root)/ndas.o: $(ndas_root)/libndas.a
	cd $(ndas_root) ; $(AR) x $^ $(@F)

else # myself

KDIR := $(NDAS_KERNEL_PATH)

$(ndas_root)/ndas_sal.ko $(ndas_root)/ndas_core.ko $(ndas_root)/ndas_block.ko: FORCE
	$(MAKE) SUBARCH=mips -C $(KDIR) \
		SUBDIRS=$(ndas_root) \
		KBUILD_VERBOSE=1 \
		ndas_root=$(ndas_root) \
		modules

FORCE:

endif # myself

$(ndas-ndasadmin-out): $(ndas-ndasadmin-obj)
	$(ndas_cmd_app_ld) 	
	
$(ndas-ndasadmin-obj): $(ndas_root)/%.o : $(ndas_root)/%.c
	$(ndas_cmd_app_cc) 	

clean:
	rm -rf $(ndas-ndasadmin-out) $(ndas-ndasadmin-obj) $(ndas-driver) $(ndas_root)/nd.ko $(ndas-sal-obj) $(ndas-block-obj) $(ndas-module-obj) $(ndas-module-obj) .*.cmd *.o *.mod.c .tmp_versions block/.*.cmd

