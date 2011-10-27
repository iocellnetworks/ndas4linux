######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################

include scripts/lib.mk
include version.mk

# global configuration C language header file for debuging and etc.
CONFIG_H=inc/dbgcfg.h

nxp-root=$(shell pwd)
nxp-dist?= dist

ifeq ($(nxpo-ndaslinux),y)
nxp-build?= build_ndas_$(nxp-target)
else
nxp-build?= build_$(nxp-target)
endif

nxp-lib-internal= $(nxp-build)/ndas-internal.o
nxp-lib-obj= $(nxp-build)/ndas.o
nxp-lib= $(nxp-dist)/libndas.a

CC=$(NDAS_CROSS_COMPILE)gcc
LD=$(NDAS_CROSS_COMPILE)ld
AR=$(NDAS_CROSS_COMPILE)ar
AS=$(NDAS_CROSS_COMPILE)as
OBJCOPY=$(NDAS_CROSS_COMPILE)objcopy
NM=$(NDAS_CROSS_COMPILE)nm
CPP=$(NDAS_CROSS_COMPILE)g++

nxp-cflags += -Wall -I$(nxp-root)/inc -I$(nxp-root)/inc/lspx \
	-D__$(XPLAT_CONFIG_ENDIAN_BYTEORDER)_ENDIAN_BYTEORDER \
	-D__$(XPLAT_CONFIG_ENDIAN_BITFIELD)_ENDIAN_BITFIELD \
	-D__$(XPLAT_CONFIG_ENDIAN_BITFIELD)_ENDIAN__ \
	-DNDAS_VER_MAJOR="$(NDAS_VER_MAJOR)" \
	-DNDAS_VER_MINOR="$(NDAS_VER_MINOR)" \
	-DNDAS_VER_BUILD="$(NDAS_VER_BUILD)"
XPLAT_LDFLAGS :=-x

default_rule: help FORCE

ifeq ($(nxpo-xixfsevent), y)
nxp-cflags += -DXPLAT_XIXFS_EVENT
endif

ifneq ($(nxpo-ndaslinux),y)

ifeq ($(nxpo-debug),y)
nxp-cflags += -DDEBUG  -O0 -g -DEXPORT_LOCAL
else
ifndef XPLAT_OPTIMIZATION
nxp-cflags += -O2 
endif
XPLAT_OPTIMIZATION?= -O2
endif

endif

ifeq ($(nxpo-sio),y)
nxp-cflags+= -DXPLAT_SIO
endif
ifeq ($(nxpo-hix),y)
nxp-cflags+= -DXPLAT_NDASHIX
endif
ifeq ($(nxpo-pnp),y)
nxp-cflags+= -DXPLAT_PNP
endif
ifeq ($(nxpo-serial2id),y)
nxp-cflags+= -DXPLAT_SERIAL2ID
endif
ifeq ($(nxpo-embedded),y)
nxp-cflags += -DXPLAT_EMBEDDED
endif
ifeq ($(nxpo-asy),y)
nxp-cflags += -DXPLAT_ASYNC_IO
endif
ifeq ($(nxpo-persist),y)
nxp-cflags += -DXPLAT_RESTORE
endif
ifeq ($(nxpo-release),y)
nxp-cflags += -DRELEASE
endif
ifeq ($(EXPORT_LOCAL),y)
nxp-cflags += -DEXPORT_LOCAL
endif
ifeq ($(nxpo-raid),y)
nxp-cflags += -DXPLAT_RAID
endif
ifeq ($(nxpo-juke),y)
nxp-cflags += -DXPLAT_JUKEBOX
endif
ifeq ($(nxpo-bpc),y)
nxp-cflags += -DXPLAT_BPC
endif
ifeq ($(nxpo-probe),y)
nxp-cflags += -DNDAS_PROBE
endif
ifeq ($(nxpo-emu),y)
nxp-cflags += -DNDAS_EMU
endif
ifeq ($(nxpo-nolanscsi),y)
nxp-cflags += -DNDAS_NO_LANSCSI
endif
ifeq ($(nxpo-nomemblkpriv),y)
nxp-cflags += -DNDAS_MEMBLK_NO_PRIVATE
endif
ifeq ($(nxpo-samsung-stb),y)
nxp-cflags += -DXPLAT_SAMSUNG_STB
endif

# include each module's module.mk

ifneq ($(nxpo-ndaslinux),y)

nxp-subdirs += xlib lpx netdisk lspx raid platform/$(nxp-os) jukebox xixfsevent #fat32lib
include $(patsubst %,$(nxp-root)/%/module.mk,$(nxp-subdirs))

else

nxp-subdirs += lspx platform/$(nxp-os)
include $(patsubst %,$(nxp-root)/%/module.mk,$(nxp-subdirs))

endif

#
# NDAS_EXTRA_CFLAGS,XPLAT_LDFLAGS are set by platform/$(nxp-os)/module.mk
#
nxp-cflags +=$(NDAS_EXTRA_CFLAGS)
XPLAT_LDFLAGS +=$(NDAS_EXTRA_LDFLAGS)

quiet_nxp_cmd_cc_o_c = CC      $@
      nxp_cmd_cc_o_c = $(CC) -Wp,-MD,$@.d $(nxp-cflags) 	\
		   $(XPLAT_OPTIMIZATION) -c -o $@ $<

quiet_nxp_cmd_cpp_o_cpp = CPP     $@
      nxp_cmd_cpp_o_cpp = $(CPP) -Wp,-MD,$@.d $(nxp-cflags) 	\
		   $(XPLAT_OPTIMIZATION) -c -o $@ $<

quiet_nxp_cmd_ld_o_o = LD      $@
      nxp_cmd_ld_o_o = $(LD) $(XPLAT_LDFLAGS) -r -o $@ $(filter-out FORCE,$^)


quiet_nxp_cmd_cc_O0_o_c = CC      $@
      nxp_cmd_cc_O0_o_c = $(CC) -Wp,-MD,$@.d $(nxp-cflags) 	\
		   -O0 -c -o $@ $<

# default target is help message

# Build rule to build SDK
all: init compile

init: $(CONFIG_H) scripts/basic/fixdep $(nxp-dist) $(nxp-build) 

$(CONFIG_H): $(CONFIG_H).default
	if [ ! -f $@ ] ; then \
		cp $< $@; \
	fi
	@echo modify $@ for your flavor

compile: init 

#
# Compilation for lpx, netdisk, lspx, xlib
# In linux, these object should be compiled 
#
nxp-normal-obj+= $(patsubst $(nxp-root)/%, %, $(nxp-lpx-obj) $(nxp-netdisk-obj) $(nxp-lspx-obj) $(nxp-xlib-obj)) $(nxp-raid-obj) $(nxp-jukebox-obj) $(nxp-xixfsevent-obj) 
# On some compiler, there is a bug with O2/O1 option.
# In that case, only source that might be compiled in wrong way, 
# should be compiler with -O0 option
nxp-nonopt-obj:= 
#nxp-normal-obj:= $(filter-out $(nxp-nonopt-obj) $(nxp-normal-obj))
$(nxp-normal-obj): $(nxp-build)/%.o : %.c scripts/basic/fixdep $(CONFIG_H)
	$(call nxp_if_changed_dep,cc_o_c)

$(nxp-nonopt-obj): $(nxp-build)/%.o : %.c scripts/basic/fixdep $(CONFIG_H)
	$(call nxp_if_changed_dep,cc_O0_o_c)

#ifneq ($(wildcard $(nxp-normal-obj:%.o=%.o.cmd)),)
-include $(nxp-normal-obj:%.o=%.o.cmd)
#endif
#
# In linux, $(nxp-sal-obj) $(nxp-extra-obj) should be compiled over the specific kernel
# $(nxp-extra-obj) specified by platform/$(nxp-os)/module.mk 
#
$(nxp-sal-obj) $(nxp-extra-obj): $(nxp-build)/%.o : %.c
	$(call nxp_if_changed,cc_o_c)

#
# 
_symbols=$(strip $(shell $(NM) --defined-only $(1) | cut -d ' ' -f3 | sort |uniq))
_public_symbols=$(filter ndas_% xixfs_% lpx_stm_backlog_recv,$(call _symbols,$(1)))
_private_symbols=$(filter-out $(call _public_symbols,$(1)),$(call _symbols,$(1))) 
# Obfuscate the (defined) symbols in the objfile
# $(1) : input objfile before obfuscation
# $(2) : output objfile after obfuscation
# $(3) : obfuscation map file
obfuscate=$(OBJCOPY) $(shell NUM=0; \
	rm -f $(2) ; \
	for s in $(call _private_symbols, $(1)) ; \
	do \
		echo --redefine-sym $$s=$${NUM}x ; \
		echo $$s $${NUM}x >> $(1)-obfus.map ; \
		NUM=$$(($${NUM} + 1)) ; \
	done) $(1) $(2)

$(nxp-lib-internal): $(nxp-netdisk-obj) \
		$(nxp-lspx-obj) \
		$(nxp-raid-obj) \
		$(nxp-xlib-obj) \
		$(nxp-lpx-obj) \
		$(nxp-jukebox-obj)\
		$(nxp-xixfsevent-obj) FORCE
	@mkdir -p $(@D)
	$(call nxp_if_changed,ld_o_o)
ifdef XPLAT_OBFUSCATE
$(nxp-lib-obj): $(nxp-lib-internal) $(nxp-netdisk-obj) \
		$(nxp-lspx-obj) \
		$(nxp-raid-obj) \
                $(nxp-xlib-obj) \
                $(nxp-lpx-obj) \
		$(nxp-jukebox-obj)\
		$(nxp-xixfsevent-obj) FORCE
	$(call obfuscate,$<,$@)
else
$(nxp-lib-obj): $(nxp-lib-internal) $(nxp-netdisk-obj) \
		$(nxp-raid-obj) \
		$(nxp-lspx-obj) \
                $(nxp-xlib-obj) \
                $(nxp-lpx-obj) \
		$(nxp-jukebox-obj)\
		$(nxp-xixfsevent-obj)  
	cp $< $@
endif


$(nxp-lib): $(nxp-lib-obj) FORCE
	@mkdir -p $(@D)
	$(AR) cru $@ $(filter-out FORCE,$^)

lib: $(nxp-lib)

$(nxp-dist):
	mkdir -p $@
ifneq ($(nxp-dist),$(nxp-build))
$(nxp-build):
	mkdir -p $@
endif

target_error:
	@echo "Invalid target"

# use default value if not exist
scripts/basic/fixdep: scripts/basic/fixdep.c
	cc -O2 -o $@ $^

distclean: clean

clean: os-clean
	@echo "Cleaning target for" $(nxp-target)
	rm -rf scripts/basic/fixdep $(nxp-normal-obj) $(nxp-normal-obj:%.o=%.o.cmd) \
		$(nxp-lib-internal) $(nxp-lib-obj) \
		$(nxp-extra-obj) \
		$(nxp-lib) $(nxp-lib-internal:%.o=%.o.cmd)

FORCE:

.PHONY: clean iop_ps2dev arm_ucosii all init compile target_error FORCE lib

help:
	@echo  \* Preconfigured Build Targets
	@echo linux-rel: x86 linux release version
	@echo linux-dev: x86 linux debug version
	@echo linux-clean: x86 linux clean
	@echo linux64-rel, linux64-dev: x86_64 linux version
	@echo em86xx-rel: EM86xx release version. You need to set additional environment \
		values to build this target.
	@echo linuxum-rel, linuxum-dev: Usermode driver and test application
	@echo Look for arch/targets to find out more preconfigured rules for various targets.
	@echo
	@echo  \* Option descriptions
	@echo nxpo-debug : set to enable the debug output 
	@echo XPLAT_OBFUSCATE : set to obfuscate the internal symbols 
	@echo XPLAT_OPTIMIZATION : set to force to change the compiler optimization option 
	@echo 		\(default: non if nxpo-debug is not set, -O2 if set\)
	@echo nxpo-hix: set to use NDAS Host Infomation eXchange protocol
	@echo nxpo-sio: set to use signalled I/O for lpx. currently datagram version is implmented
	@echo nxpo-pnp: set to include the server that accept the pnp messages from NDAS device
	@echo nxpo-uni: set to generate the universal linux driver.
	@echo nxpo-asy: set to support asynchrous block IO
	@echo nxpo-persist: set to support the persistency of registration data
	@echo nxpo-raid: set to support NDAS raid.
	@echo nxpo-release: set if want to release a official release binary
	@echo nxpo-automated: set to build a release version but via batch script\(doesn\'t make any change if nxpo-debug is set\)
	@echo nxpo-probe: set to supportt probe command
	@echo nxpo-emu: set to include emulator
	@echo nxpo-nolanscsi: set to exclude lanscsi module from core.
	@echo

