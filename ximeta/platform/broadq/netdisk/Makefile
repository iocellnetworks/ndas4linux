TOP				=	..
MEDIO			=	$(TOP)/medio-cre
MEDIO_CORE		=	$(MEDIO)/core
MEDIO_COMMON	=	$(MEDIO_CORE)/common
MEDIO_INCLUDE	=	$(MEDIO)/include
MEDIO_IOP		=	$(MEDIO_CORE)/iop
BROADQ_TRACE?=$(TOP)/trace
include				$(MEDIO_COMMON)/Makefile.inc

EXTRA_OBJS		=	\
					$(TOP)/ximeta/lib/libndas.a \
					mediostub.iop.o \
					smap_stub.iop.o \
					ip_stub.iop.o

COMMON_CODE		=	\
					bcache.c argparser.cc

IOPINCLUDES		=	\
  					-D_IOP -DPS2DEV \
					-I../ximeta/inc \
					-I. -I$(TOP) \
					-I$(MEDIO)/include \
					-I$(PS2LIB)/iop/include -I$(PS2LIB)/common/include

LIBS			=	\
					-L$(TOP)/ximeta/src/sal -lxisal \
					-L$(PS2LIB)/iop/lib -lkernel \
					-lgcc

goo = \
		stuff



XI_OBJS			=	\
  delay.iop.o \
					$(BROADQ_TRACE)/mediostub.iop.o \
					netdisk.iop.o bcache.iop.o args.iop.o argparser.iop.o \
					$(TOP)/ximeta/lib/libndas.a \
					$(IP)/ip_stub.iop.o $(TOP)/smap/smapnew/smap_stub.iop.o \
					$(MEDIO_IOP)/modules.iop.o $(MEDIO_IOP)/mutex.iop.o $(MEDIO_IOP)/sema.iop.o \
					$(MEDIO_IOP)/vsnprintf.iop.o $(MEDIO_IOP)/utils.iop.o $(MEDIO_IOP)/cdlist.iop.o \
					$(MEDIO_IOP)/features.iop.o \
					$(MEDIO_IOP)/printf.iop.o \
					$(MEDIO_IOP)/trace.iop.o \
					$(MEDIO_IOP)/_*.o

all: netdisk.irx netdisk.full.irx

clean::
	rm -f netdisk.irx netdisk.full.irx ximeta.irx *.o *.id  
	
netdisk.irx: delay.iop.o netdisk.iop.o bcache.iop.o args.iop.o argparser.iop.o $(EXTRA_OBJS)
	$(IOPLINK) -o $@ -s $(IOPLDFLAGS) $^ $(LIBS)

netdisk.full.irx: delay.iop.o netdisk.iop.o bcache.iop.o args.iop.o argparser.iop.o $(EXTRA_OBJS)
	$(IOPLINK) -o netdisk.full.irx $(IOPLDFLAGS) $^ $(LIBS)

ximeta.irx: $(XI_OBJS)
	$(IOPLINK) -o $@ -s $(IOPLDFLAGS) $^ $(LIBS)
	@mkdir -p $(BROADQ_TRACE)
	cp ximeta.irx $(BROADQ_TRACE)

foo:	foo.iop.o
	$(IOPLINK) -o $@ -T lk2 $(XIOPLDFLAGS) $^

partial.o: delay.iop.o netdisk.iop.o bcache.iop.o args.iop.o argparser.iop.o $(TOP)/ximeta/lib/libndas.a
	$(IOPLINK) -r -o $@ -nostdlib $(IOPLDFLAGS) $^ $(TOP)/ximeta/src/sal/libxisal.a

$(COMMON_CODE):
	ln -s $(MEDIO_COMMON)/$@

include				$(MEDIO_COMMON)/smapstubs.mk
include				$(MEDIO_COMMON)/ipstubs.mk
include				$(MEDIO_COMMON)/mediostubs.mk
