######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################
IOP_CFLAGS:= $(CFLAGS) -I$(PS2SDK)/iop/include -I$(PS2SDK)/common/include 
IOP_LDFLAGS:= -G0 -s -nostdlib -miop -nostartfiles -mabi=32 -L$(PS2LIB)/iop/lib -lkernel 

all: objdir $(nxp-dist)/ps2ndas.irx

objdir:
	@mkdir -p $(OBJDIR)

LIB_NDAS_OBJ :=$(nxp-dist)/libndas.o 

$(nxp-dist)/ps2ndas.irx: $(OBJDIR)/ndirx.o $(OBJDIR)/ndasirx_imports.o $(OBJDIR)/ndasirx_exports.o  $(LIB_NDAS_OBJ)  $(nxp-dist)/libfat32.o $(nxp-dist)/libsal.o
	$(CC) -o $@ $(IOP_LDFLAGS) $^
#	cp $@ ../bins/
$(OBJDIR)/%.o : %.c
	$(CC) -o $@ -c $(CFLAGS) $^

# A rule to build imports.lst.
$(OBJDIR)/ndasirx_imports.o :imports.lst
	echo "#include \"ndasirx_imports.h\"" > $(OBJDIR)/imports.c
	cat $< >> $(OBJDIR)/imports.c
	$(CC) -c $(CFLAGS) -I. $(OBJDIR)/imports.c -o $@
	-rm -f $(OBJDIR)/imports.c

# A rule to build exports.tab.
$(OBJDIR)/ndasirx_exports.o :exports.tab
	echo "#include \"irx.h\"" > $(OBJDIR)/exports.c
	cat $< >> $(OBJDIR)/exports.c
	$(CC) -c $(CFLAGS) $(OBJDIR)/exports.c -o $@
	-rm -f $(OBJDIR)/exports.c

clean:
	rm -rf $(OBJDIR)/*.o

