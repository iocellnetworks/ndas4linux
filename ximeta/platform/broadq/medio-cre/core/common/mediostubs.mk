
# rules for handling iop stub functions

clean::
	$(RM) medio.defs mediostub.S mediostub.iop.o

medio.defs: $(MEDIO_IOP)/medio.defs
	IFS=" ,"; cat $^ | while read macro function number; do echo "$$function"; done > /tmp/www
	ps2-nm -B partial.o | grep " U " | grep -w -f /tmp/www | cut -c12- > /tmp/www2
	grep -w -f /tmp/www2 < $^ > $@

mediostub.S:
	ln -s $(MEDIO_IOP)/mediostub.S

mediostub.iop.o: mediostub.S partial.o medio.defs
