
# rules for handling ip stub functions

clean::
	$(RM) ip.defs ip_stub.S ip_stub.iop.o

ip.defs: $(IP)/ip.defs
	IFS=" ,"; cat $^ | while read macro function number; do echo "$$function"; done > /tmp/www
	ps2-nm -B partial.o | grep " U " | grep -w -f /tmp/www | cut -c12- > /tmp/www2
	grep -w -f /tmp/www2 < $^ > $@

ip_stub.S:
	ln -s $(IP)/ip_stub.S

ip_stub.iop.o: ip_stub.S partial.o ip.defs
