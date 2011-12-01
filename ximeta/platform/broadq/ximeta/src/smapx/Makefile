IOP_LDFLAGS:= -nostdlib -L$(PS2LIB)/iop/lib -lkernel # -s

all: objdir $(nxp-dist)/ps2smapx.irx

objdir:
	@mkdir -p $(OBJDIR)

$(nxp-dist)/ps2smapx.irx: $(OBJDIR)/main.o $(OBJDIR)/smap.o $(OBJDIR)/imports.o $(OBJDIR)/exports.o
	$(CC) -o $@ $(IOP_LDFLAGS) $^

$(OBJDIR)/%.o : %.c
	$(CC) -o $@ -c $(CFLAGS) $^

# A rule to build imports.lst.
$(OBJDIR)/imports.o :imports.lst
	echo "#include \"irx_imports.h\"" > $(OBJDIR)/imports.c
	cat $< >> $(OBJDIR)/imports.c
	$(CC) -c $(CFLAGS) -I. $(OBJDIR)/imports.c -o $@
	-rm -f $(OBJDIR)/imports.c

# A rule to build exports.tab.
$(OBJDIR)/exports.o :exports.tab
	echo "#include \"irx.h\"" > $(OBJDIR)/exports.c
	cat $< >> $(OBJDIR)/exports.c
	$(CC) -c $(CFLAGS) $(OBJDIR)/exports.c -o $@
	-rm -f $(OBJDIR)/exports.c

clean:
	rm -rf $(OBJDIR)/*.o

