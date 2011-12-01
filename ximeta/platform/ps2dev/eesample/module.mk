######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################

EESAMPLE_SRC=$(wildcard $(EESAMPLE_PATH)/*.c)
EESAMPLE_OBJ=$(EESAMPLE_SRC:%.c=$(EE_OBJDIR)/%.o)
EESAMPLE_APPLICATION=$(nxp-dist)/eesample.elf

EE_SRC+= $(EESAMPLE_SRC)

$(EESAMPLE_APPLICATION): $(EESAMPLE_OBJ) $(PS2SDK)/ee/startup/crt0.o
	$(call nxp_if_changed,ee_ld_o_o)
#	$(EE_CC) -nostartfiles -T$(PS2SDK)/ee/startup/linkfile $(EE_LDFLAGS) -o $@ $(EESAMPLE_OBJ) 

$(EESAMPLE_PATH)/.done: $(EESAMPLE_APPLICATION)