# -------------------------------------------------------------------------
# Copyright (c) 2012 IOCELL Networks, Plainsboro, NJ, USA.
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
# may be distributed under the terms of the GNU General Public License (GPL v2),
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
include $(ndas_root)/version.mk

#
# Set the ndas version and ndas build
#
ndas_version=$(NDAS_VER_MAJOR).$(NDAS_VER_MINOR)
ndas_build=$(NDAS_VER_BUILD)
 
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
NDAS_KERNEL_MODULE_PATH?=$(DESTDIR)/lib/modules/$(NDAS_KERNEL_VERSION)/kernel/drivers/block/ndas
NDAS_ADMIN_PATH?=$(DESTDIR)/usr/sbin
NDAS_SHARE_PATH?=$(DESTDIR)/usr/share/ndas
NDAS_INITD_PATH?=$(DESTDIR)/etc/init.d

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
# In some cases, another toolchain is required for usermode.
#
NDAS_CROSS_COMPILE_UM?=$(NDAS_CROSS_COMPILE)

include $(ndas_root)/lib.mk
include $(ndas_root)/sal/module.mk
include $(ndas_root)/block/module.mk
include $(ndas_root)/scsi/module.mk
include $(ndas_root)/admin/module.mk

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
#
# flags for tools
#
ndas-app-cflags:=-DLINUX -I$(ndas_root)/inc
ndas-app-ldflags?=
ndas-kernel-cflags:=-DMODULE $(ndas-app-cflags) 
ndas-kernel-ldflags:=
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
#NDAS_DEBUG=y
ifdef NDAS_DEBUG
ndas-app-cflags+=-DDEBUG -g 
ndas-kernel-cflags+=-DDEBUG
#
# for 2.6.x kernel
#
EXTRA_CFLAGS+=-DDEBUG $(EXTRA_CFLAGS)
include $(ndas_root)/debuglevel.mk
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
ndas-cpu:=$(shell echo $(NDAS_KERNEL_ARCH) | sed -e 's/x86_64/X86_64/' -e 's/i?86/i386/' -e 's/i386/X86/' -e 's/x86/X86/' -e 's/mipsel/MIPSEL/' -e 's/mips/MIPS/' -e 's/cris/ARM/' -e 's/arm/ARM/')
ndas-app-cflags+=-D_$(ndas-cpu)
ndas-kernel-cflags+=-D_$(ndas-cpu)

#
# Linux Module driver file
#

# Vendor specific addon programs or scripts
-include $(ndas_root)/addon/module.mk

ifneq ($(xixfs),)
NDAS_EXTRA_CFLAGS+=-DXPLAT_XIXFS_EVENT -DXPLAT_XIXFS_EVENT_USER
endif

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
ndas-module-src:=ndas_sal_main.c ndas_core_main.c ndas_block_main.c ndas_scsi_main.c ndas_emu_main.c
ndas-module-obj:=ndas_sal_main.o ndas_core_main.o ndas_block_main.o ndas_scsi_main.o ndas_emu_main.o

#
# NDAS driver administration tools
#
all: ndas-kernel ndas-admin
install: install-kernel-modules install-admin

ifeq ($(wildcard $(ndas_root)/libndas.a.sfx),$(ndas_root)/libndas.a.sfx)
ifeq ($(I_agree_the_XIMETA_EULA),true)
$(ndas_root)/libndas.a: $(ndas_root)/libndas.a.sfx
	cd $(ndas_root) ; tail -n +119 $< > $@
else
$(ndas_root)/libndas.a: $(ndas_root)/libndas.a.sfx
	cd $(ndas_root) ; /bin/sh $<
endif
endif
#
# Build NDAS admin tool
#
ndas-admin: $(ndas-ndasadmin-out)

#
# For 2.4.x kernel
#
ifeq ($(shell echo $(NDAS_KERNEL_VERSION)| cut -d'.' -f1-2),2.4)
ARCH?=$(NDAS_KERNEL_ARCH)
#
# Kernel Modules for 2.4.x kernel
#
ndas-kernel-modules:=$(ndas_root)/ndas_sal.o \
                     $(ndas_root)/ndas_core.o \
                     $(ndas_root)/ndas_block.o \
                     $(ndas_root)/ndas_emu.o
                     
export-objs := $(ndas-sal-obj) ndas_core_main.o 
ndas-kernel: $(ndas-kernel-modules) 

install-kernel-modules: $(ndas_driver) 
	install -d -m 755 $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(ndas_root)/ndas_sal.o $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(ndas_root)/ndas_core.o $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(ndas_root)/ndas_block.o $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(ndas_root)/CREDITS.txt $(NDAS_KERNEL_MODULE_PATH)
	-depmod > /dev/null 2>&1
 
$(ndas_root)/ndas_sal.o: $(ndas-sal-obj) ndas_sal_main.o
	$(ndas_cmd_ld)

$(ndas_root)/ndas_block.o: $(ndas-block-obj) ndas_block_main.o
	$(ndas_cmd_ld)


$(ndas_root)/ndas_core.o: ndas_core_main.o $(ndas_root)/libndas.a
	$(ndas_cmd_ld)

$(ndas_root)/ndas_emu.o: ndas_emu_main.o
	$(ndas_cmd_ld)
	
ndas_cmd_24_cc= $(if $(filter $@, $(export-objs)), \
	$(ndas-cc) `[ -f $(NDAS_KERNEL_PATH)/include/linux/modversions.h ] && echo "-include $(NDAS_KERNEL_PATH)/include/linux/modversions.h"` \
		-DMODVERSIONS -DKBUILD_BASENAME=$(@:$(@D)/%.o=%) -DEXPORT_SYMTAB $(ndas-kernel-cflags) -c -o $@ $<, \
	$(ndas-cc) $(ndas-kernel-cflags) -c -o $@ $<) 
$(ndas-sal-obj) $(ndas-block-obj) $(ndas-module-obj): %.o : %.c
	$(ndas_cmd_24_cc)

emu-single: $(ndas_root)/ndas_emu_s.o

blk-single: $(ndas_root)/ndas_blk_s.o ndas-admin


$(ndas_root)/ndas_emu_s.o: $(ndas_root)/ndas_emu_single.o $(ndas-sal-obj) $(ndas_root)/libndas.a
	$(ndas_cmd_ld)

$(ndas_root)/ndas_blk_s.o: $(ndas_root)/ndas_blk_single.o $(ndas-sal-obj) $(ndas-block-obj) $(ndas_root)/libndas.a
	$(ndas_cmd_ld)


$(ndas_root)/ndas_emu_single.o: %.o : %.c
	$(ndas_cmd_24_cc)

$(ndas_root)/ndas_blk_single.o: %.o : %.c
	$(ndas_cmd_24_cc)

else

# 2.6 build rules

emu-single: $(ndas_root)/ndas_emu_s.ko

blk-single: $(ndas_root)/ndas_blk_s.ko ndas-admin

scsi-single: $(ndas_root)/ndas_scsi_s.ko ndas-admin
#
# Kernel Modules for 2.6.x kernel
#
ndas-kernel-modules:=$(ndas_root)/ndas_sal.ko \
                     $(ndas_root)/ndas_core.ko \
                     $(ndas_root)/ndas_block.ko \
                     $(ndas_root)/ndas_scsi.ko \
                     $(ndas_root)/ndas_emu.ko
                     
ndas-kernel: $(ndas-kernel-modules) 

install-kernel-modules: $(ndas_driver) 
	install -d -m 755 $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(ndas_root)/ndas_sal.ko $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(ndas_root)/ndas_core.ko $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(ndas_root)/ndas_block.ko $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(ndas_root)/ndas_scsi.ko $(NDAS_KERNEL_MODULE_PATH)
	install -m 644 $(ndas_root)/CREDITS.txt $(NDAS_KERNEL_MODULE_PATH)
	-depmod > /dev/null 2>&1
# module extension is *.ko in 2.6 kerenl
#
# Makefile for 2.6 kernel
#
ifneq ($(KERNELRELEASE),)
#
# This part will be handled by kernel makefile
#
# Added DEBUG define if applicable
EXTRA_CFLAGS := $(ndas-kernel-cflags)
ifneq ($(NDAS_REDHAT),)
EXTRA_CFLAGS += -DNDAS_REDHAT
endif

#
#obj-m is required for kernel makefile to build modules.
#

obj-m:= ndas_sal.o ndas_core.o ndas_block.o ndas_scsi.o ndas_emu.o

ifeq ($(CONFIG_EMU_SINGLE),m)
obj-m:= ndas_emu_s.o
endif

ifeq ($(CONFIG_BLK_SINGLE),m)
obj-m:= ndas_blk_s.o
endif

ifeq ($(CONFIG_SCSI_SINGLE),m)
obj-m:= ndas_scsi_s.o
endif

ndas_sal-objs:=  $(ndas-sal-obj) ndas_sal_main.o
ndas_core-objs:=  ndas.o ndas_core_main.o
ndas_emu-objs:=  ndas_emu_main.o
ndas_block-objs:=  $(ndas-block-obj) ndas_block_main.o
ndas_scsi-objs:=  $(ndas-scsi-obj) ndas_scsi_main.o
ndas_emu_s-objs:= ndas.o $(ndas-sal-obj) ndas_emu_single.o
ndas_blk_s-objs:= ndas.o $(ndas-sal-obj) $(ndas-block-obj) ndas_blk_single.o
ndas_scsi_s-objs:= ndas.o $(ndas-sal-obj) $(ndas-scsi-obj) ndas_scsi_single.o

$(ndas-block-obj) $(ndas-scsi-obj) $(ndas-sal-obj) $(ndas-module-obj): %.o : /%.c
	@mkdir -p $(@D)
	$(call if_changed,cc_o_c)

$(ndas_root)/ndas.o: $(ndas_root)/libndas.a
	cd $(ndas_root) ; $(AR) x $^ $(@F)

else # myself
$(ndas_root)/ndas_emu_s.ko: FORCE
	@echo Invoking make againt the kernel at $(NDAS_KERNEL_PATH)
	$(MAKE) -C $(NDAS_KERNEL_PATH) \
		SUBDIRS=$(ndas_root) \
		KBUILD_VERBOSE=1 \
		ndas_root=$(ndas_root) \
		CONFIG_EMU_SINGLE=m \
		modules

$(ndas_root)/ndas_blk_s.ko: FORCE
	@echo Invoking make againt the kernel at $(NDAS_KERNEL_PATH)
	$(MAKE) -C $(NDAS_KERNEL_PATH) \
		SUBDIRS=$(ndas_root) \
		KBUILD_VERBOSE=1 \
		ndas_root=$(ndas_root) \
		CONFIG_BLK_SINGLE=m \
		modules

$(ndas_root)/ndas_scsi_s.ko: FORCE
	@echo Invoking make againt the kernel at $(NDAS_KERNEL_PATH)
	$(MAKE) -C $(NDAS_KERNEL_PATH) \
		SUBDIRS=$(ndas_root) \
		KBUILD_VERBOSE=1 \
		ndas_root=$(ndas_root) \
		CONFIG_SCSI_SINGLE=m \
		modules



$(ndas_root)/ndas_sal.ko $(ndas_root)/ndas_core.ko $(ndas_root)/ndas_block.ko $(ndas_root)/ndas_scsi.ko $(ndas_root)/ndas_emu.ko: FORCE
	@echo Invoking make againt the kernel at $(NDAS_KERNEL_PATH)
	$(MAKE) -C $(NDAS_KERNEL_PATH) \
		SUBDIRS=$(ndas_root) \
		KBUILD_VERBOSE=1 \
		ndas_root=$(ndas_root) \
		modules

FORCE:

endif # myself
endif # 2.6

include $(ndas_root)/ipkg/module.mk

$(ndas-ndasadmin-out): $(ndas-ndasadmin-obj)
	$(ndas_cmd_app_ld) 	
	
$(ndas-ndasadmin-obj): $(ndas_root)/%.o : $(ndas_root)/%.c
	$(ndas_cmd_app_cc) 

install-admin: $(ndas-ndasadmin-out)
	install -d -m 755 $(NDAS_SHARE_PATH)
	install -s -m 755 $(ndas_root)/ndasadmin $(NDAS_ADMIN_PATH)
	install -m 755 $(ndas_root)/ndas $(NDAS_SHARE_PATH)
	install -m 755 $(ndas_root)/mknod.sh $(NDAS_SHARE_PATH)
	install -m 755 $(ndas_root)/ndas.suse $(NDAS_SHARE_PATH)
	install -m 755 $(ndas_root)/ndas.gentoo $(NDAS_SHARE_PATH)
	install -m 644 $(ndas_root)/README $(NDAS_SHARE_PATH)
	install -m 644 $(ndas_root)/INSTALL $(NDAS_SHARE_PATH)
	install -m 644 $(ndas_root)/EULA.txt $(NDAS_SHARE_PATH)
	if [ -f /etc/debian_version ]; then \
	    ln -sf $(NDAS_SHARE_PATH)/ndas $(NDAS_INITD_PATH)/ndas;\
	    /usr/sbin/update-rc.d ndas start 73 2 3 4 5 . stop 13 0 1 6 .; \
	else if [ -f /etc/SuSE-release ] ; then \
	    ln -sf $(NDAS_SHARE_PATH)/ndas.suse $(NDAS_INITD_PATH)/ndas;\
	    /sbin/chkconfig --add ndas;\
	else if [ -f /etc/gentoo-release ] ; then \
	    ln -sf $(NDAS_SHARE_PATH)/ndas.gentoo $(NDAS_INITD_PATH)/ndas;\
	    rc-update add ndas default; \
	else \
	    ln -sf $(NDAS_SHARE_PATH)/ndas $(NDAS_INITD_PATH)/ndas;\
	    /sbin/chkconfig --add ndas;\
	    /sbin/chkconfig --level 2345 ndas on ;\
	fi;\
	fi;\
	fi

clean:
	rm -rf $(ndas-ndasadmin-out) $(ndas-ndasadmin-obj) $(ndas-kernel-modules) $(ndas_root)/nd.ko $(ndas-sal-obj) $(ndas-block-obj) $(ndas-scsi-obj) $(ndas_root)/ndas.o
	find . \( -name 'ndas_*.o' -o -name '*.ko' -o -name '.*.cmd' -o -name '.*.d' -o -name '*.mod.c' -o -name '*.mod.o' -o -name '*.symvers' \) -type f -print | xargs rm -f
	rm -rf '.tmp_versions'
	rm -rf '.kernel.mk'
