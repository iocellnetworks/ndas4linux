######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################
#
# EmotionEngine MACROs
#
EE_PREFIX = ee-
EE_CC = $(EE_PREFIX)gcc
EE_CXX= $(EE_PREFIX)g++
EE_AS = $(EE_PREFIX)as
EE_LD = $(EE_PREFIX)ld
EE_AR = $(EE_PREFIX)ar
EE_OBJCOPY = $(EE_PREFIX)objcopy
EE_STRIP = $(EE_PREFIX)strip

EE_SRC ?=
EE_LIB = -lps2ip -lc -lpad -lkernel -lsyscall -lc

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I. -I$(PS2LIB)/ee/include 
EE_CFLAGS := -D__LITTLE_ENDIAN_BYTEORDER -D__LITTLE_ENDIAN_BITFIELD -D_EE -mips3 -EL -O2 -Wall

EE_LDFLAGS := -L$(PS2SDK)/ee/lib $(EE_LDFLAGS) \
	-nostartfiles -T$(PS2SDK)/ee/startup/linkfile
	
 

EE_OBJ=$(EE_SRC:%.c=$(EE_OBJDIR)/%.o)
EE_DEP=$(EE_SRC:%.c=$(EE_OBJDIR)/%.d)

#include $(EE_DEP)

quiet_nxp_cmd_ee_cc_o_c = EE_CC      $@
      nxp_cmd_ee_cc_o_c = $(EE_CC) -Wp,-MD,$@.d $(EE_CFLAGS) 	\
		   -c -o $@ $<

quiet_nxp_cmd_ee_ld_o_o = EE_LD      $@
      nxp_cmd_ee_ld_o_o = $(EE_LD) $(EE_LDFLAGS) -o $@ $(filter-out FORCE,$^) $(EE_LIB) $(PS2SDK)/ee/startup/crt0.o

#$(EE_OBJDIR)/%.d: %.c
#	@mkdir -p $(@D)
#	CC=$(EE_CC) ./depend.sh $(@D) $(EE_CFLAGS) $(EE_INCS) -I$(@D) $< > $@
