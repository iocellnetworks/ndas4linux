#
# tools
#
ndas-cross:=$(NDAS_CROSS_COMPILE)
MSHARE-CC:=$(ndas-cross)gcc
MSHARE-CPP:=$(ndas-cross)g++
MSHARE-LD:=$(ndas-cross)ld
MSHARE-AR:=$(ndas-cross)ar

#
#
# flags for tools
#
MSCFLAGS:=-DLINUX -I$(NDAS_ROOT)/inc -D_ARM -DNDAS_WRITE=1 -I$(NDAS_CROSS_COMPILE_INCLUDE) -fomit-frame-pointer \
                        -idirafter -O2 -DDEBUGLEVEL=1 -MMD -c -DILGU_LINUX -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DDVD_RIP_HACK 
MSLDFLAGS= -elf2flt="s32768" -static


export MSHARE-CC MSHARE-CPP MSHARE-LD MSHARE-AR MSCFLAGS MSLDFLAGS 

OBJECT= dvdcsslib/dvdcsslib.o  dvdreadlib/dvdreadlib.o  dvdbackup.o
all:libdvd.o test_ioctl bwcalc

libdvd.o : $(OBJECT)  
	$(MSHARE-LD) -o $@ $(MSLDFLAGS) -r $(OBJECT)

test_ioctl: test_ioctl.c
	$(MSHARE-CC) $(MSCFLAGS) $(MSLDFLAGS) test_ioctl.c  -o test_ioctl 

bwcalc: bwcalc.c
	$(MSHARE-CC) $(MSCFLAGS) $(MSLDFLAGS) bwcalc.c  -o bwcalc

dvdbackup.o: dvdbackup.c
	$(MSHARE-CC)  $(MSCFLAGS) -c -o $@ $<
dvdreadlib/dvdreadlib.o :
	@for i in dvdreadlib ; do make -C $$i ; done
dvdcsslib/dvdcsslib.o:
	@for i in dvdcsslib ; do make -C $$i ; done
clean:
	make -C dvdreadlib clean
	make -C dvdcsslib clean
	rm -f *.o
	rm -f test_ioctl
	rm -f bwcalc	
