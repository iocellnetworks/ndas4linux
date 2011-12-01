
# rules for handling iop stub functions

clean::
	$(RM) mountd.defs mountdstub.S mountdstub.iop.o

mountd.defs: $(MOUNTD)/mountd.defs
	IFS=" ,"; cat $^ | while read macro function number; do echo "$$function"; done > /tmp/www
	ps2-nm -B partial.o | grep " U " | grep -w -f /tmp/www | cut -c12- > /tmp/www2
	grep -w -f /tmp/www2 < $^ > $@

mountdstub.S:
	ln -s $(MOUNTD)/mountdstub.S

mountdstub.iop.o: mountdstub.S partial.o mountd.defs
