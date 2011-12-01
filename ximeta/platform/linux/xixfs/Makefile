# -------------------------------------------------------------------------
# Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
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
xixfs_root?=$(shell pwd)
xplat_path?=$(xixfs_root)/../../..

#
# Set the ndas version and ndas build
#
ndas_version?=1.0.0
ndas_build?=0
 
# sanity check
# NDAS_KERNEL_VERSION should be specified in the cross compilation
ifdef NDAS_CROSS_COMPILE
ifndef NDAS_KERNEL_VERSION
$(error NDAS_KERNEL_VERSION should be set eg: 2.4 or 2.6)
endif
endif

#
# Guess kernel source path, if environment variable is not set
#
NDAS_KERNEL_VERSION?=$(shell uname -r)
NDAS_KERNEL_PATH?=/lib/modules/$(NDAS_KERNEL_VERSION)/build
NDAS_KERNEL_ARCH?=$(shell uname -m | sed -e 's/i.86/i386/')
# The directory to install the ndas kernel modules
NDAS_KERNEL_MODULE_PATH?=/lib/modules/$(NDAS_KERNEL_VERSION)/kernel/drivers/block/ndas

ifneq ($(wildcard $(NDAS_KERNEL_PATH)), $(NDAS_KERNEL_PATH))
NDAS_KERNEL_PATH:=/usr/src/linux-$(shell uname -r | cut -c1-3)
endif
ifneq ($(wildcard $(NDAS_KERNEL_PATH)), $(NDAS_KERNEL_PATH))
NDAS_KERNEL_PATH:=/usr/src/linux-$(shell uname -r | cut -c1-5)
endif
ifneq ($(wildcard $(NDAS_KERNEL_PATH)), $(NDAS_KERNEL_PATH))
NDAS_KERNEL_PATH:=/usr/src/linux-$(shell uname -r )
endif
ifneq ($(wildcard $(NDAS_KERNEL_PATH)), $(NDAS_KERNEL_PATH))
NDAS_KERNEL_PATH:=/usr/src/linux
endif
ifneq ($(wildcard $(NDAS_KERNEL_PATH)), $(NDAS_KERNEL_PATH))
NDAS_KERNEL_PATH:=/usr/src/kernels/$(shell uname -r )-$(shell uname -m)
endif
#
# All the attempts failed. Please set it by manual
#
ifneq ($(wildcard $(NDAS_KERNEL_PATH)), $(NDAS_KERNEL_PATH))
$(error Fail to guess the kernel source path\
Please set NDAS_KERNEL_PATH to the kernel source directory)
endif
#
# sanity check
#
# if include/linux directory exist
ifneq ($(NDAS_KERNEL_PATH)/include/linux, $(wildcard $(NDAS_KERNEL_PATH)/include/linux))
$(error $(NDAS_KERNEL_PATH) is not a valid kernel path)
endif
#
# Guess NDAS_KERNEL_VERSION
#
NDAS_CROSS_COMPILE_UM?=$(NDAS_CROSS_COMPILE)

include $(xixfs_root)/lib.mk


#
# tools
#
ndas-cross:=$(NDAS_CROSS_COMPILE)
ndas-cross-um:=$(NDAS_CROSS_COMPILE_UM)
ndas-cc:=$(ndas-cross)gcc
ndas-cpp:=$(ndas-cross)g++
ndas-ld:=$(ndas-cross)ld
ndas-ar:=$(ndas-cross)ar
ndas-um-cc:=$(ndas-cross-um)gcc
ndas-um-cpp:=$(ndas-cross-um)g++
ndas-um-ld:=$(ndas-cross-um)ld
ndas-um-ar:=$(ndas-cross-um)ar

#
# Xixfs path
#

ndas-xixfs-path=
ndas-xixfs-core-path?=xixcore

ndas-xixfs-src:=$(wildcard $(xixfs_root)/$(ndas-xixfs-path)/*.c) 
# Exclude module stub code from source code. It will be included in $(ndas-module-src)
ndas-xixfs-src:=$(patsubst $(xixfs_root)/$(ndas-xixfs-path)/ndas_xixfs_main.c,, $(ndas-xixfs-src))
ndas-xixfs-obj:=$(patsubst $(xixfs_root)/$(ndas-xixfs-path)/%.c,%.o, $(ndas-xixfs-src))

ndas-xixfs-core-src:=$(wildcard $(xixfs_root)/$(ndas-xixfs-core-path)/core/*.c) $(wildcard $(xixfs_root)/$(ndas-xixfs-core-path)/sysdep/linux/*.c)
ndas-xixfs-core-obj:=$(patsubst $(xixfs_root)/%.c,%.o, $(ndas-xixfs-core-src))

#
# flags for tools
#
ndas-app-cflags:=-DLINUX -I$(xplat_path)/inc -DNDAS_WRITE=1
ndas-app-ldflags?=
#ADDED BY ILGU HONG
ndas-kernel-cflags:=-DMODULE $(ndas-app-cflags) -DXPLAT_XIXFS_EVENT -DXPLAT_XIXFS_EVENT_USER \
	-I$(xixfs_root)/$(ndas-xixfs-core-path)/include \
	-I$(xixfs_root)/../tarball/inc
ndas-kernel-ldflags:=

#
# Xixcore debug
#

ndas-kernel-cflags+= -D__XIXCORE_DEBUG__

#
# Retreive byte order string from linux kernel header.
#

LINUX_BYTE_ORDER_STRING_TEMP:=$(shell $(ndas-cc) -I$(NDAS_KERNEL_PATH)/include -E -dM $(NDAS_KERNEL_PATH)/include/asm/byteorder.h | grep "_ENDIAN")

LINUX_BYTE_ORDER_STRING:=$(findstring __LITTLE_ENDIAN, $(LINUX_BYTE_ORDER_STRING_TEMP))
LINUX_BYTE_ORDER_STRING+=$(findstring __BIG_ENDIAN, $(LINUX_BYTE_ORDER_STRING_TEMP))
LINUX_BYTE_ORDER_STRING:=$(strip $(LINUX_BYTE_ORDER_STRING))

ifeq ($(LINUX_BYTE_ORDER_STRING), __LITTLE_ENDIAN)
ndas-kernel-cflags+=-D__XIXCORE_BYTEORDER_LITTLE__
else
ifeq ($(LINUX_BYTE_ORDER_STRING), __BIG_ENDIAN)
ndas-kernel-cflags+=-D__XIXCORE_BYTEORDER_BIG__
else
$(error linux-tarball: could not retreive byte order string. $(LINUX_BYTE_ORDER_STRING))
endif
endif

#
# 64bit block-addressing
#

ndas-kernel-cflags+=-D__XIXCORE_BLOCK_LONG_ADDRESS__

#
# For 2.4.x kernel, CFLAGS, LDFLAGS are copied from the kernel source's Makefile
#
ifeq ($(shell echo $(NDAS_KERNEL_VERSION)| cut -d'.' -f1-2),2.4)
ndas-kernel-cflags+=$(call ndas_kernel_variable,CFLAGS)
ndas-kernel-ldflags+=$(call ndas_kernel_variable,LDFLAGS) 
endif
#
# Debug build
#

#ndas-app-cflags+=-DNDAS_ORG2423 
#ndas-kernel-cflags+=-DNDAS_ORG2423

ifdef NDAS_DEBUG
ndas-app-cflags+=-DDEBUG 
ndas-kernel-cflags+=-DDEBUG 
#
# for 2.6.x kernel
#
EXTRA_CFLAGS+=-DDEBUG $(EXTRA_CFLAGS)
-include $(xixfs_root)/debuglevel.mk
endif

#
# Use devfs
#
ifdef NDAS_DEVFS
ndas-app-cflags+=-DNDAS_DEVFS 
ndas-kernel-cflags+=-DNDAS_DEVFS
endif

#
# SUSE
#
ifneq ($(NDAS_SUSE),)
ndas-app-cflags+=-DNDAS_SUSE 
ndas-kernel-cflags+=-DNDAS_SUSE 
endif


#
# Cross Compilation
#
ndas-cpu:=$(shell echo $(NDAS_KERNEL_ARCH) | sed -e 's/x86_64/X86_64/' -e 's/i?86/i386/' -e 's/i386/X86/' -e 's/x86/X86/' -e 's/mipsel/MIPSEL/' -e 's/mips/MIPS/' -e 's/cris/ARM/')
ndas-app-cflags+=-D_$(ndas-cpu)
ndas-kernel-cflags+=-D_$(ndas-cpu)

#
# Linux Module driver file
#

# Vendor specific addon programs or scripts
-include $(xixfs_root)/addon/module.mk

NDAS_EXTRA_CFLAGS+=-DNDAS_VERSION=${ndas_version} -DNDAS_BUILD=${ndas_build}
ndas-app-cflags+=$(NDAS_EXTRA_CFLAGS) $(ndas-addon-extra-cflags)
ndas-app-ldflags+=$(NDAS_EXTRA_LDFLAGS)
ndas-kernel-cflags+=$(NDAS_EXTRA_CFLAGS)

#
# Compilation/ Linking command
#
ndas_cmd_cc=$(ndas-cc) $(ndas-kernel-cflags) -c -o $@ $<

ndas_cmd_ld=$(ndas-ld) -r -x $(ndas-kernel-ldflags) -o $@ $^

ndas_cmd_app_cc=$(ndas-um-cc) $(ndas-app-cflags) -c -o $@ $<

ndas_cmd_app_cpp=$(ndas-um-cpp) $(ndas-app-cflags) -c -o $@ $<

ndas-app-ldflags:=$(shell echo $(ndas-app-ldflags) | sed -e 's|^ *||')
ifeq ($(ndas-app-ldflags),)
ndas_cmd_app_ld=$(ndas-um-cc) $(ndas-app-cflags) -o $@ $^ 
else
ndas_cmd_app_ld=$(ndas-um-cc) $(ndas-app-cflags) -Wl,"$(ndas-app-ldflags)" -o $@ $^ 
endif

#
# Linux Module Source
#
ndas-module-src:=ndas_xixfs_main.c
ndas-module-obj:=ndas_xixfs_main.o

#
# XIMETA NDAS driver administration tools
#
all: ndas-kernel
install: install-kernel-modules

ifeq ($(wildcard $(xixfs_root)/libndas.a.sfx),$(xixfs_root)/libndas.a.sfx)
ifeq ($(I_agree_the_XIMETA_EULA),true)
$(xixfs_root)/libndas.a: $(xixfs_root)/libndas.a.sfx
	cd $(xixfs_root) ; tail -n +119 $< > $@
else
$(xixfs_root)/libndas.a: $(xixfs_root)/libndas.a.sfx
	cd $(xixfs_root) ; /bin/sh $<
endif
endif

#
# For 2.4.x kernel
#
ifeq ($(shell echo $(NDAS_KERNEL_VERSION)| cut -d'.' -f1-2),2.4)
ARCH?=$(NDAS_KERNEL_ARCH)
#
# Kernel Modules for 2.4.x kernel
#
ndas-kernel-modules:=$(xixfs_root)/ndas_xixfs.o

 
#export-objs := $(ndas-sal-obj) ndas_core_main.o $(ndas-block-obj) ndas_block_main.o 
ndas-kernel: $(ndas-kernel-modules) 

install-kernel-modules: $(ndas_driver) 
	install -d -m 755 $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(xixfs_root)/ndas_xixfs.o $(NDAS_KERNEL_MODULE_PATH)
	-depmod > /dev/null 2>&1
 
$(xixfs_root)/ndas_xixfs.o: $(ndas-xixfs-obj) $(ndas-xixfs-core-obj) ndas_xixfs_main.o
	$(ndas_cmd_ld)

ndas_cmd_24_cc= $(if $(filter $@, $(export-objs)), \
	$(ndas-cc) -include $(NDAS_KERNEL_PATH)/include/linux/modversions.h -DMODVERSIONS -DKBUILD_BASENAME=$(@:$(@D)/%.o=%) -DEXPORT_SYMTAB $(ndas-kernel-cflags) -c -o $@ $<, \
	$(ndas-cc) $(ndas-kernel-cflags) -c -o $@ $<) 

$(ndas-xixfs-obj) $(ndas-xixfs-core-obj): %.o : %.c 
	$(ndas_cmd_24_cc) 
else # 2.6
#
# Kernel Modules for 2.6.x kernel
#
ndas-kernel-modules:=$(xixfs_root)/ndas_xixfs.ko

ndas-kernel: $(ndas-kernel-modules) 

install-kernel-modules: $(ndas_driver) 
	install -d -m 755 $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(xixfs_root)/nads_xixfs.ko $(NDAS_KERNEL_MODULE_PATH) #added by ILGU HONG for xixfs
	-depmod > /dev/null 2>&1
# module extension is *.ko in 2.6 kerenl
#
# Makefile for 2.6 kernel
#
ifneq ($(KERNELRELEASE),)
#
# ndas main
#
# Added DEBUG define if applicable
EXTRA_CFLAGS := $(ndas-kernel-cflags)
ifneq ($(NDAS_REDHAT),)
EXTRA_CFLAGS += -DNDAS_REDHAT
endif

obj-m:= ndas_xixfs.o

ndas_xixfs-objs:=  $(ndas-xixfs-obj) $(ndas-xixfs-core-obj) ndas_xixfs_main.o

$(ndas-xixfs-obj): %.o : /%.c
	@mkdir -p $(@D)
	$(call if_changed,cc_o_c)

else # myself
$(xixfs_root)/ndas_xixfs.ko: FORCE
	@echo Invoking make againt the kernel at $(NDAS_KERNEL_PATH)
	$(MAKE) -C $(NDAS_KERNEL_PATH) \
		SUBDIRS=$(xixfs_root) \
		KBUILD_VERBOSE=1 \
		xixfs_root=$(xixfs_root) \
		xplat_path=$(xplat_path) \
		modules

FORCE:

endif # myself
endif # 2.6

-include $(xixfs_root)/ipkg/module.mk

clean-xixfs:
	rm -rf $(xixfs_root)/ndas_xixfs.ko $(ndas-xixfs-obj) $(ndas-xixfs-core-obj)

clean:
	rm -rf $(ndas-kernel-modules) $(ndas-xixfs-core-obj) $(ndas-xixfs-obj)
	find . \( -name 'ndas_*.o' -o -name '*.ko' -o -name '.*.cmd' -o -name '.*.d' -o -name '*.mod.c' -o -name '*.mod.o' -o -name '*.symvers' \) -type f -print | xargs rm -f
	rm -rf '.tmp_versions'
	
