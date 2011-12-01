######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################
PROJECT_ROOT=$(shell pwd)

PS2LIB?= /usr/local/ps2dev/ps2lib
PS2SDK?= /usr/local/ps2dev/ps2sdk
PS2DEV?= /usr/local/ps2dev

CC:= $(PS2DEV)/iop/bin/iop-gcc
CFLAGS:= -Wall -g -DDEBUG -DPS2DEV -D__LITTLE_ENDIAN_BYTEORDER -D__LITTLE_ENDIAN_BITFIELD -D_IOP -G0 -I$(PROJECT_ROOT)/inc/ -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include 
# smapx.h
CFLAGS+= -I$(PROJECT_ROOT)/src/smapx
LD:= $(PS2DEV)/iop/bin/iop-ld
LDFLAGS:= -melf32elmip -L$(PS2LIB)/iop/lib

# OBJDIR is where the compiled object file is generated
nxp-build=$(PROJECT_ROOT)/out
# EE_OBJDIR is for emotion engine binaries
EE_OBJDIR=$(PROJECT_ROOT)/out/ee

# nxp-dist is where the final binary is generated
nxp-dist=$(OBJDIR)

# the reason there are two aliases for one directory 'out' is that
# the Makefile files of the sub-directory are re-used for both
# XIMETA internal build tree, and
# application developers build tree (while this Makefile is only for application developers build tree)

SAL_PATH=src/sal
SMAPX_PATH=src/smapx
IRX_PATH=sample/irx
EESAMPLE_PATH=sample/eesample
TEST_PATH=sample/test

SUBDIRS= src/sal src/smapx sample/irx sample/test sample/eesample

# default target
all: init compile ready_for_naplink

#
nxpp-src=
MODULE_OBJ= $(wildcard lib/*.o)
APPLICATION=
EE_SRC=
include $(patsubst %,%/module.mk,$(SUBDIRS))

include ee.mk
#
nxpp-obj = $(nxpp-src:%.c=$(OBJDIR)/%.o)
#

init:
	@echo Making output directories and ready object files to be linked 
	@mkdir -p $(OBJDIR)
	@mkdir -p $(EE_OBJDIR)
	@mkdir -p $(nxp-dist)
	@cp lib/*.o $(OBJDIR)
	@echo EE_SRC=\"$(EE_SRC)\"
	@echo EESAMPLE_SRC=\"$(EESAMPLE_SRC)\"
	@echo EE_OBJDIR=\"$(EE_OBJDIR)\"
	@echo EESAMPLE_PATH=\"$(EESAMPLE_PATH)\"
	@echo $(wildcard $(EESAMPLE_PATH)/*.c)
	
compile: $(MODULE_OBJ) $(APPLICATION)

$(OBJDIR)/%.d: %.c
	@mkdir -p $(@D)
	@./depend.sh $(@D) $(CFLAGS) -I$(<D) $< > $@
	@echo Generated Dependency Acycle Graph file $< 

ready_for_naplink:
	cp lib/*.irx out/ 

clean:
	@rm -rf $(OBJDIR)
	@rm -rf $(nxp-dist)
	@rm -rf $(EE_OBJDIR)
