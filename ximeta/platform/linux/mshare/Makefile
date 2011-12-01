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

include $(ndas_root)/lib.mk
include $(ndas_root)/sal/module.mk
include $(ndas_root)/block/module.mk
include $(ndas_root)/admin/module.mk

ndas-dvdlib:=$(ndas_root)/admin/libdvd.o

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
ndas-app-cflags:=-DLINUX -I$(ndas_root)/inc -D_ARM -DNDAS_WRITE=1 -I$(NDAS_CROSS_COMPILE_INCLUDE) -fomit-frame-pointer \
			-idirafter -O2 -DDEBUGLEVEL=1 -MMD -c -DILGU_LINUX -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DDVD_RIP_HACK -DNDAS_MSHARE
ndas-app-ldflags:= -elf2flt="s32768" -static -DNDAS_MSHARE
ndas-kernel-cflags:= -DLINUX -D_ARM -DMODULE \
	$(call ndas_kernel_variable,CFLAGS) \
	-I$(NDAS_KERNEL_PATH)/include -DNDAS_IO_UNIT=32\
	$(ndas-app-cflags) -I$(NDAS_KERNEL_PATH) \
	-DNDAS_MSHARE # Add this fo jukebox
ndas-kernel-ldflags:=$(call ndas_kernel_variable,LDFLAGS) -S
ifdef NDAS_DEBUG
ndas-app-cflags+= -DDEBUG 
ndas-kernel-cflags+= -DDEBUG
# for 2.6
EXTRA_CFLAGS+= -DDEBUG
include $(ndas_root)/debuglevel.mk
endif

#
# Compilation/ Linking command
#
ndas_cmd_cc= $(ndas-cc) $(ndas-kernel-cflags) -c -o $@ $< 

ndas_cmd_ld= $(ndas-ld) -r -x $(ndas-kernel-ldflags) -o $@ $^

ndas_cmd_app_cc= $(ndas-cc) $(ndas-app-cflags) -c -o $@ $<

ndas_cmd_app_cpp= $(ndas-cpp) $(ndas-app-cflags) -c -o $@ $<

ndas_cmd_app_ld= $(ndas-cc) $(ndas-app-ldflags) -o $@ $^ 

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
ndas-module-src:=ndas_sal_main.c ndas_core_main.c ndas_block_main.c ndas_emu_main.c
ndas-module-obj:=ndas_sal_main.o ndas_core_main.o ndas_block_main.o ndas_emu_main.o

#
# XIMETA NDAS driver administration tools
#
all:

ifeq ($(wildcard $(ndas_root)/libndas.a.sfx),$(ndas_root)/libndas.a.sfx)
$(ndas_root)/libndas.a: $(ndas_root)/libndas.a.sfx
	cd $(ndas_root) ; /bin/sh $<
endif

ifneq ($(shell echo $(NDAS_KERNEL_VERSION)| cut -d'.' -f1-2),2.6)
ndas-driver:=$(ndas_root)/ndas_sal.o $(ndas_root)/ndas_core.o $(ndas_root)/ndas_block.o $(ndas_root)/ndas_emu.o
all: $(ndas-driver) $(ndas-ndasadmin-out)

export-objs := $(ndas-sal-obj) ndas_core_main.o 
 
$(ndas_root)/ndas_sal.o: $(ndas-sal-obj) ndas_sal_main.o
	$(ndas_cmd_ld)

$(ndas_root)/ndas_block.o: $(ndas-block-obj) ndas_block_main.o
	$(ndas_cmd_ld)

$(ndas_root)/ndas_core.o: ndas_core_main.o $(ndas_root)/libndas.a
	$(ndas_cmd_ld)

$(ndas_root)/ndas_emu.o: ndas_emu_main.o
	$(ndas_cmd_ld)
	
		
ndas_cmd_24_cc= $(if $(filter $@, $(export-objs)), \
	$(ndas-cc) -DMODVERSIONS -DKBUILD_BASENAME=$(@:$(@D)/%.o=%) -DEXPORT_SYMTAB $(ndas-kernel-cflags) -c -o $@ $<, \
	$(ndas-cc) $(ndas-kernel-cflags) -c -o $@ $<) 

$(ndas-sal-obj) $(ndas-block-obj) $(ndas-module-obj): %.o : %.c
	$(ndas_cmd_24_cc)

else # 2.6
ndas-driver:=$(ndas_root)/ndas_sal.ko $(ndas_root)/ndas_core.ko $(ndas_root)/ndas_block.ko $(ndas_root)/ndas_emu.ko
all: $(ndas-driver) $(ndas-ndasadmin-out)

# module extension is *.ko in 2.6 kerenl
#
# Makefile for 2.6 kernel
#
ifneq ($(KERNELRELEASE),)
#
# ndas main
#
# Added DEBUG define if applicable
obj-m:= ndas_sal.o ndas_core.o ndas_block.o ndas_emu.o 
ndas_sal-objs:=  $(ndas-sal-obj) ndas_sal_main.o
ndas_core-objs:=  ndas.o ndas_core_main.o
ndas_block-objs:=  $(ndas-block-obj) ndas_block_main.o
ndas_emu-objs:=  ndas_emu_main.o
export-objs := $(ndas-sal-obj) ndas_core_main.o 

$(ndas-block-obj) $(ndas-sal-obj) $(ndas-module-obj): %.o : /%.c
	@mkdir -p $(@D)
	$(call if_changed,cc_o_c)

$(ndas_root)/ndas.o: $(ndas_root)/libndas.a
	cd $(ndas_root) ; $(AR) x $^ $(@F)

else # myself

#KDIR := /lib/modules/$(NDAS_KERNEL_VERSION)/build
KDIR := $(NDAS_KERNEL_PATH)

$(ndas_root)/ndas_sal.ko $(ndas_root)/ndas_core.ko $(ndas_root)/ndas_block.ko $(ndas_root)/ndas_emu.ko: FORCE
	$(MAKE) -C $(KDIR) \
		SUBDIRS=$(ndas_root) \
		KBUILD_VERBOSE=1 \
		ndas_root=$(ndas_root) \
		modules

FORCE:

endif # myself
endif # 2.6

$(ndas-ndasadmin-out): $(ndas-ndasadmin-obj)  $(ndas-dvdlib)
	$(ndas_cmd_app_ld) 	
	
$(ndas-ndasadmin-obj): $(ndas_root)/%.o : $(ndas_root)/%.c
	$(ndas_cmd_app_cc) 	

clean:
	rm -rf $(ndas-ndasadmin-out) $(ndas-ndasadmin-obj) $(ndas-driver) $(ndas_root)/nd.ko $(ndas-sal-obj) $(ndas-block-obj) $(ndas-module-obj) $(ndas-module-obj) .kernel.mk
