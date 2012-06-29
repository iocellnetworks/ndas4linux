# -----------------------------------------------------------------------------
# Copyright (c) 2011 IOCELL Networks Corp., Plainsboro NJ 08536, USA
# All rights reserved.
# -----------------------------------------------------------------------------

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
# include each module's module.mk

ifneq ($(nxpo-ndaslinux),y)

nxp-subdirs += xlib lpx netdisk lspx raid platform/$(nxp-os) xixfsevent
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
      nxp_cmd_cc_o_c = $(CC) -Wp,-MD,$@.d $(nxp-cflags)		\
		   $(XPLAT_OPTIMIZATION) -c -o $@ $<

quiet_nxp_cmd_cpp_o_cpp = CPP     $@
      nxp_cmd_cpp_o_cpp = $(CPP) -Wp,-MD,$@.d $(nxp-cflags)	\
		   $(XPLAT_OPTIMIZATION) -c -o $@ $<

quiet_nxp_cmd_ld_o_o = LD      $@
      nxp_cmd_ld_o_o = $(LD) $(XPLAT_LDFLAGS) -r -o $@ $(filter-out FORCE,$^)


quiet_nxp_cmd_cc_O0_o_c = CC      $@
      nxp_cmd_cc_O0_o_c = $(CC) -Wp,-MD,$@.d $(nxp-cflags)	\
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
nxp-normal-obj+= $(patsubst $(nxp-root)/%, %, $(nxp-lpx-obj) $(nxp-netdisk-obj) $(nxp-lspx-obj) $(nxp-xlib-obj)) $(nxp-raid-obj) $(nxp-xixfsevent-obj)
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
		$(nxp-xixfsevent-obj) FORCE
	@mkdir -p $(@D)
	$(call nxp_if_changed,ld_o_o)
ifdef XPLAT_OBFUSCATE
$(nxp-lib-obj): $(nxp-lib-internal) $(nxp-netdisk-obj) \
		$(nxp-lspx-obj) \
		$(nxp-raid-obj) \
      $(nxp-xlib-obj) \
      $(nxp-lpx-obj) \
		$(nxp-xixfsevent-obj) FORCE
	$(call obfuscate,$<,$@)
else
$(nxp-lib-obj): $(nxp-lib-internal) $(nxp-netdisk-obj) \
		$(nxp-raid-obj) \
		$(nxp-lspx-obj) \
      $(nxp-xlib-obj) \
      $(nxp-lpx-obj) \
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
	find . \( -name '*~' \) -type f -print | xargs rm -f

FORCE:

.PHONY: clean iop_ps2dev arm_ucosii all init compile target_error FORCE lib

help:
	@echo Export a build folder for different linux systems.
	@echo
	@echo Usage:
	@echo "	make [target] to export a preconfigured option."
	@echo "	make nxp-vendor=[vendor] to use options for certain products."
	@echo "Or set individual options from the lists below. Example: "
	@echo "	make nxp-cpu=arm nxp-os=linux nxpo-pnp=y nxpo-persist=y"
	@echo
	@echo "Current targets located in arch/targets are:"
	@echo "	linux.mk - provides following targets"
	@echo "		linux: unknown result"
	@echo "		linux-automated: unknown result"
	@echo "		linux-clean: x86 linux clean"
	@echo "		linux-dev: x86 linux debug version"
	@echo "		linux-devall: unknown difference with -dev"
	@echo "		linux-emu-rel: NDAS with emulator or maybe just the emulator?"
	@echo "		linux-emu-embed: emulator for embedded applications?"
	@echo "		linux-emu-debug: emulator with debug output?"
	@echo "		linux-noraid-dev: debug version without raid features? "
	@echo "		linux-rel: x86 linux release version"
	@echo "		linux-rel-noob: release version without obfuscation"
	@echo "		linux-rel-pogo: export arm version that runs on pink pogoplug"
	@echo "		linux-xix: adds ximeta's file system support"
	@echo "	linux64.mk - provides following targets"
	@echo "		linux64:"
	@echo "		linux64-automated:"
	@echo "		linux64-clean:"
	@echo "		linux64-dev: x86_64 linux debug version"
	@echo "		linux64-devall: not sure of the difference with -dev"
	@echo "		linux64-rel: x86_64 linux release version"
	@echo "		linux64-rel-noobf: x86_64 release without obfuscation"
	@echo "		linux64-raid-dev: add the existing NDAS RAID features"
	@echo "	linuxum.mk - user mode. (unchanged since kernel 2.6.16) Provides:"
	@echo "		linuxum-rel: Usermode driver and test application"
	@echo "		linuxum-dev: Usermode driver and test with debug output"
	@echo "	openwrt.mk - router version"
	@echo
	@echo Command line options
	@echo nxp-cpu=\[cpu\] to cross compile a certain architecture.
	@echo "	arch/cpu has makefiles for the following:"
	@echo "	arm  cris  i386  mipsel  mips  ppc  x86_64  x86"
	@echo
	@echo nxp-os=\[version\] create the system mode or usermode package.
	@echo "	arch/os has makefiles for the following:"
	@echo "	linux linuxum"
	@echo
	@echo nxp-vendor=\[mk filename\] to export for a particular project
	@echo "	arch/vendor has some targets devices. currently:"
	@echo "	em86xx.mk - would be for the MVIX media player. "
	@echo "	kamikaze.mk - Open WRT version?"
	@echo "	openwrt.mk  - Open WRT version?"
	@echo "	pogov2.mk - to cross compile the pogo plug pink model"
	@echo "	whiterussian.mk - Open WRT version?"
	@echo
	@echo nxpo-\[option]\=y to include various components and features
	@echo "	* Option descriptions"
	@echo "	nxpo-asy: set to support asynchrous block IO"
	@echo "	nxpo-automated: build a release version but via batch script"
	@echo "		(doesn't make any change if nxpo-debug is set)"
	@echo "	nxpo-debug : set to enable the debug output optimization option"
	@echo "		(default: non if nxpo-debug is not set, -O2 if set)"
	@echo "	nxpo-emu: set to include emulator"
	@echo "	nxpo-hix: set to use NDAS Host Infomation eXchange protocol"
	@echo "	nxpo-persist: set to support the persistency of registration data"
	@echo "	nxpo-ndaslinux: causes ndaslinux.mk to set various options"
	@echo "	nxpo-nolanscsi: set to exclude lanscsi module from core."
	@echo "	nxpo-nomemblkpriv: unknown purpose"
	@echo "	nxpo-pnp: set to include the server that accept the pnp messages"
	@echo "		from NDAS device."
	@echo "	nxpo-probe: set to supportt probe command"
	@echo "	nxpo-raid: set to support NDAS raid."
	@echo "	nxpo-release: set if want to release a official release binary"
	@echo "	nxpo-serial2id: unknown purpose"
	@echo "	nxpo-sio: set to use signalled I/O for lpx. currently datagram"
	@echo "		version is implmented"
	@echo "	nxpo-uni: set to generate the universal linux driver."
	@echo "	nxpo-vmware: include the login and test files for VMWARE"
	@echo "	nxpo-xixfsevent: unknown (related to Ximeta's file system?)"
	@echo "	"
	@echo "	all: unknown purpose"
	@echo "	complete: unknown purpose"
	@echo "	tarball: creates a distributable package in dist/"
	@echo "	EXPORT_LOCAL: exports LOCAL rather than LOCAL static. (xplatcfg.h)"
	@echo "	XPLAT_OBFUSCATE : set to obfuscate the internal symbols"
	@echo "	XPLAT_OPTIMIZATION : set to force to change the compiler"
	@echo
