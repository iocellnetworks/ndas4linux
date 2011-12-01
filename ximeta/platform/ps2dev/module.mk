######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################

#
# IO Processor FLAGS
#
#IOP_CFLAGS:= $(nxp-cflags) -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include
#NDAS_EXTRA_LDFLAGS:= -G0 -s -nostdlib -miop -nostartfiles -mabi=32 -L$(PS2LIB)/iop/lib -lkernel
NDAS_EXTRA_LDFLAGS:= -nostdlib -L$(PS2LIB)/iop/lib -lkernel
#
# Include sub modules
#
nxp-sal-path=platform/ps2dev/sal
nxpp-smapx-path=platform/ps2dev/smapx
nxpp-irx-path=platform/ps2dev/irx
EESAMPLE_PATH=platform/ps2dev/eesample

include $(nxp-sal-path)/module.mk
include $(nxpp-smapx-path)/module.mk
include $(nxpp-irx-path)/module.mk

###############################################
# Set Emotion Engine Binary output directory
# module.mk depends EE_OBJDIR,
# ee.mk depends module.mk
###############################################
EE_OBJDIR=ee_ps2dev
#
# include the ee sample application after ee.mk
#
include $(EESAMPLE_PATH)/module.mk
#
# for Emotion Engine application 
#
include platform/ps2dev/ee.mk

################################################
# for packaging for application developers. should be included after other module.mk
#include platform/ps2dev/package.mk

nxp-extra-obj:= $(nxpp-smapx-obj) $(nxpp-irx-obj) $(nxp-fat32lib-obj)
NDAS_EXTRA_CFLAGS+= -I$(nxpp-smapx-path) -I$(nxpp-irx-path)

$(nxp-build)/platform/ps2dev/.done: $(nxp-build)/$(nxpp-irx-path)/.done \
		$(nxp-build)/$(nxpp-smapx-path)/.done \
		$(nxp-build)/$(EESAMPLE_PATH)/.done
	@touch $@

$(nxp-build)/.clean.cmd:
	@echo rm -rf $(nxp-root)/$(EE_OBJDIR) > $@
	
$(nxp-build)/.done: $(nxp-build)/.clean.cmd \
		$(nxp-build)/platform/ps2dev/.done
	@touch $@
	
	
