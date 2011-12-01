
# rules for handling iop smap stub functions

clean::
	$(RM) smap.defs smap_stub.S smap_stub.iop.o

smap.defs: $(SMAP)/smap.defs
	IFS=" ,"; cat $^ | while read macro function number; do echo "$$function"; done > /tmp/www
	ps2-nm -B partial.o | grep " U " | grep -w -f /tmp/www | cut -c12- > /tmp/www2
	grep -w -f /tmp/www2 < $^ > $@

smap_stub.S:
	ln -s $(SMAP)/smap_stub.S

smap_stub.iop.o: smap_stub.S partial.o smap.defs
