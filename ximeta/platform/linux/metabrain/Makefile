################################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
################################################################################
# Makefile for NDAS metabrain build
# The top most directory of ndas driver
ndas_root?=$(shell pwd)

NDAS_CROSS_COMPILE:=/opt/crosstool/mips-unknown-linux-gnu/gcc-3.4.3-glibc-2.3.2/bin/mips-unknown-linux-gnu-
 
NDAS_KERNEL_VERSION=2.6.9

# Guess kernel source path, if environment variable is not set
NDAS_KERNEL_PATH:=/root/metabrain/linux-2.6.9

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
ndas-app-cflags:=-DLINUX -I$(ndas_root)/inc -D_MIPS
ndas-app-ldflags:=
ndas-kernel-cflags:=-DMODULE $(call ndas_kernel_variable,CFLAGS) \
	$(ndas-app-cflags) -I$(NDAS_KERNEL_PATH) \
	-I$(NDAS_KERNEL_PATH)/include \
 	-I$(NDAS_KERNEL_PATH)/include/asm-mips/mach-tx \
	-I$(NDAS_KERNEL_PATH)/include/asm-mips/mach-generic 
ndas-kernel-ldflags:=$(call ndas_kernel_variable,LDFLAGS) 
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
ndas-driver:=nd.o

# Vendor specific addon programs or scripts
-include $(ndas_root)/addon/module.mk

ndas-app-cflags+= $(NDAS_EXTRA_CFLAGS) $(ndas-addon-extra-cflags)
ndas-kernel-cflags+= $(NDAS_EXTRA_CFLAGS)

#
# Linux Module Source
#
ndas-module-src:=af_lpx.c
ndas-module-obj:=af_lpx.o


#
# XIMETA NDAS driver administration tools
#
all: $(ndas-driver) $(ndas-ndasadmin-out)

ifneq ($(shell echo $(NDAS_KERNEL_VERSION)| cut -d'.' -f1-2),2.6)

$(ndas-driver): $(ndas-sal-obj) $(ndas-block-obj) $(ndas-module-obj) libndas.a
	$(ndas_cmd_ld)
	
$(ndas-sal-obj) $(ndas-block-obj) $(ndas-module-obj): %.o : %.c
	$(ndas_cmd_cc)


else # 2.6
#
# Makefile for 2.6 kernel
#
ifneq ($(KERNELRELEASE),)
#
# ndas main
#
# Added DEBUG define if applicable
EXTRA_CFLAGS := $(ndas-app-cflags)
ifeq ($(wildcard /etc/fedora-release),/etc/fedora-release)
EXTRA_CFLAGS += -DNDAS_FEDORA
endif
obj-m:= nd.o
nd-objs:=  $(ndas-sal-obj) $(ndas-block-obj) $(ndas-module-obj) ndas.o

$(ndas-block-obj) $(ndas-sal-obj) $(ndas-module-obj): %.o : /%.c
	@mkdir -p $(@D)
	$(call if_changed,cc_o_c)

$(ndas_root)/ndas.o: $(ndas_root)/libndas.a
	cd $(ndas_root) ; $(AR) x $^ $(@F)

else # myself

#KDIR := /lib/modules/$(NDAS_KERNEL_VERSION)/build
KDIR := $(NDAS_KERNEL_PATH)

$(ndas_root)/nd.ko: FORCE
	$(MAKE) -C $(KDIR) SUBARCH=mips \
		SUBDIRS=$(ndas_root) \
		KBUILD_VERBOSE=1 \
		ndas_root=$(ndas_root) \
		modules

$(ndas-driver): $(ndas_root)/nd.ko
	cp $< $@

FORCE:

endif # myself
endif # 2.6

$(ndas-ndasadmin-out): $(ndas-ndasadmin-obj)
	$(ndas_cmd_app_ld) 	
	
$(ndas-ndasadmin-obj): $(ndas_root)/%.o : $(ndas_root)/%.c
	$(ndas_cmd_app_cc) 	

clean:
	rm -rf $(ndas-ndasadmin-out) $(ndas-ndasadmin-obj) $(ndas-driver) $(ndas_root)/nd.ko $(ndas-sal-obj) $(ndas-block-obj) $(ndas-module-obj) $(ndas-module-obj)
