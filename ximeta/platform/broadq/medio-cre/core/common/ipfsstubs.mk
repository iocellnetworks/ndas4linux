
#	rules for handling iop ipfs stubs

clean::
	$(RM) ipfs.defs ipfsstub.S ipfsstub.iop.o

ipfs.defs: $(IPFS)/ipfs.defs
	IFS=" ,"; cat $^ | while read macro function number; do echo "$$function"; done > /tmp/www
	ps2-nm -B partial.o | grep " U " | grep -w -f /tmp/www | cut -c12- > /tmp/www2
	grep -w -f /tmp/www2 < $^ > $@

ipfsstub.S:
	ln -s $(IPFS)/ipfsstub.S

ipfsstub.iop.o: ipfsstub.S partial.o ipfs.defs
