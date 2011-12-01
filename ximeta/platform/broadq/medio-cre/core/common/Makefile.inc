.SUFFIXES: .e

#		TOP needs to be set before this makes any sense...

MEDIO			=	$(TOP)/medio-cre
MEDIO_INCLUDE	=	$(MEDIO)/include
MEDIO_CORE		=	$(MEDIO)/core
MEDIO_COMMON	=	$(MEDIO_CORE)/common
MEDIO_IOP		=	$(MEDIO_CORE)/iop
MEDIO_EE		=	$(MEDIO_CORE)/ee
MEDIO_UK		=	$(MEDIO_EE)/ukernel
MEDIO_CLIENT	=	$(MEDIO_EE)/client

NEWFONT			=	$(TOP)/newfont
LIBSTUB			=	$(TOP)/libstub
LIBSTUB			=	.
PS2SIGN			=	$(TOP)/ps2sign
CATALOG			=	$(TOP)/catalog
TRANSLATIONS	=	$(TOP)/translations
TRACE			=	$(TOP)/trace
SKIN			=	$(TOP)/skin
RW				=	/home/broadq/src/tools++
IP				=	$(TOP)/ip/ip
IPFS			=	$(TOP)/ipfs/ipfs
SMAP			=	$(TOP)/smap/smapnew
MOUNTD			=	$(TOP)/fs/mountd
USB				=	$(TOP)/usb

RM				=	rm -rf

IOPCC			=	ps2-gcc
IOPCXX			=	ps2-g++
IOPAR			=	ps2-ar
IOPLD			=	ps2-ld
IOPAS			=	ps2-gcc

EECC			=	ps2-gcc
EECXX			=	ps2-g++
EEAR			=	ps2-gcc
EELD			=	ps2-ld
EEAS			=	ps2-gcc

EECFLAGS		=	-mips3 -EL -O2 -G0
EECXXFLAGS		=	-mips3 -EL -O2 -G0
EESFLAGS		=	-xassembler-with-cpp -mips3 -EL -O0 -G0
EEDEFINES		=	-D__EE__
EEINCLUDES		=	\
					-I. -I$(TOP) \
					-I$(MEDIO_INCLUDE) -I$(MEDIO)/server \
					-I$(PS2LIB)/common/include -I$(PS2LIB)/ee/include
EELINK			=	$(EELD)
EELDFLAGS		=	-L$(PS2LIB)/ee/lib


IOPCFLAGS		=	-G0 -fno-builtin -miop -msoft-float -g -O0
IOPCXXFLAGS		=	-G0 -miop -msoft-float -fno-exceptions -fno-rtti -g -O0
IOPSFLAGS		=	-miop
IOPDEFINES		=	-D__IOP__
IOPINCLUDES		=	\
					-I. -I$(TOP) -I$(MEDIO_INCLUDE) \
					-I$(PS2LIB)/common/include -I$(PS2LIB)/iop/include
IOPLINK			=	$(IOPCC)
IOPLDFLAGS		=	-G0 -g -miop -nostdlib -nostartfiles

_all: all

# turn off default .o from .c rule
%.o: %.c
%: %.c

# rules to recreate dependency stuff
#FIXME: doesn't work for stuff in other dirs
%.id: %.c
	set -e; $(IOPCC) -M $(IOPINCLUDES) $(IOPDEFINES) $< \
        | sed 's/\($*\)\.o[ :]*/\1.iop.o $@ : /g' > $@; \
        [ -s $@ ] || rm -f $@
#	echo foo: 's/\($*\)\.o[ :]*/\1.iop.o $@ : /g' ; \

%.ed: %.c
	set -e; $(EECC) -M $(EEINCLUDES) $< \
        | sed 's/\($*\)\.o[ :]*/\1.ee.o $@ : /g' > $@; \
        [ -s $@ ] || rm -f $@

#ruled for compiling IOP stuff
%.iop.o: %.c
	$(IOPCC) $(IOPCFLAGS) $(IOPDEFINES) $(IOPINCLUDES) $(IOPCCEXTRAS) -c $< -o $@
%.iop.o: %.cc
	$(IOPCXX) $(IOPCXXFLAGS) $(IOPDEFINES) $(IOPINCLUDES) $(IOPCXXEXTRAS) -c $< -o $@
%.iop.o: %.s
	$(IOPAS) $(IOPSFLAGS) $(IOPDEFINES) $(IOPINCLUDES) $(IOPASEXTRAS) -c $< -o $@
%.iop.o: %.S
	$(IOPAS) $(IOPSFLAGS) $(IOPDEFINES) $(IOPINCLUDES) $(IOPASEXTRAS) -c $< -o $@
%.iop.s: %.c
	$(IOPCC) $(IOPCFLAGS) $(IOPDEFINES) $(IOPINCLUDES) $(IOPCCEXTRAS) -S $<
%.iop.s: %.cc
	$(IOPCXX) $(IOPCXXFLAGS) $(IOPDEFINES) $(IOPINCLUDES) $(IOPCXXEXTRAS) -S $<
%.iop.e: %.c
	$(IOPCC) $(IOPCFLAGS) $(IOPDEFINES) $(IOPINCLUDES) $(IOPCCEXTRAS) -E $<
%.iop.e: %.cc
	$(IOPCXX) $(IOPCXXFLAGS) $(IOPDEFINES) $(IOPINCLUDES) $(IOPCXXEXTRAS) -E $<

#rules for compiling EE stuff
%.ee.o: %.c
	$(EECC) $(EECFLAGS) $(EEDEFINES) $(EEINCLUDES) $(EECCEXTRAS) -c $< -o $@
%.ee.e: %.c
	$(EECC) -E $(EECFLAGS) $(EEDEFINES) $(EEINCLUDES) $(EECCEXTRAS) $< > $@
%.ee.e: %.cc
	$(EECXX) -E $(EECXXFLAGS) $(EEDEFINES) $(EEINCLUDES) $(EECXXEXTRAS) $< > $@
%.ee.s: %.c
	$(EECC) -S $(EECFLAGS) $(EEDEFINES) $(EEINCLUDES) $(EECCEXTRAS) $<
%.ee.s: %.cc
	$(EECXX) -S $(EECXXFLAGS) $(EEDEFINES) $(EEINCLUDES) $(EECXXEXTRAS) $<
%.ee.o: %.cc
	$(EECXX) $(EECXXFLAGS) $(EEDEFINES) $(EEINCLUDES) $(EECXXEXTRAS) -o $@ -c $< -o $@
%.ee.o: %.s
	$(EECC) $(EESFLAGS) $(EEDEFINES) $(EEINCLUDES) $(EEASEXTRAS) -c $< -o $@
%.ee.o: %.S
	$(EECC) $(EESFLAGS) $(EEDEFINES) $(EEINCLUDES) $(EEASEXTRAS) -c $< -o $@

