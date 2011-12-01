
# rules for handling iop usbbase stub functions

clean::
	$(RM) usbbase.defs usbbasestubs.S usbbasestubs.iop.o

usbbase.defs: $(USB)/usbbase.defs
	IFS=" ,"; cat $^ | while read macro function number; do echo "$$function"; done > /tmp/www
	ps2-nm -B partial.o | grep " U " | grep -w -f /tmp/www | cut -c12- > /tmp/www2
	grep -w -f /tmp/www2 < $^ > $@

usbbasestubs.S:
	ln -s $(USB)/usbbasestubs.S

usbbasestubs.iop.o: usbbasestubs.S partial.o usbbase.defs
