
TOP				=	../../..
MEDIO			=	$(TOP)/medio-cre/core
MEDIO_COMMON	=	$(MEDIO)/common

include $(MEDIO_COMMON)/Makefile.inc
IP				=	$(TOP)/ip

OBJS			=	debug.iop.o libc.iop.o mem.iop.o sal.iop.o sync.iop.o thread.iop.o time.iop.o dprintf.iop.o net.iop.o
OBJS			=	debug.iop.o libc.iop.o mem.iop.o sal.iop.o sync.iop.o thread.iop.o time.iop.o net.iop.o

THDBG_STUB =
TRACEFS_STUB	=	../../trace/tracestub.iop.o
TRACEFS_STUB	=

IOPCFLAGS += -DIOP_PRINTF=no_printf -D_IOP -D_PS2 -DPS2DEV -DDEBUG

IOPINCLUDES		=	\
					-I../../inc \
					-I$(IP) -I$(IP)/ip/lwip-0.5.3/src/include -I$(IP)/ip/lwip_iop/include \
					-I$(IP)/ip -I$(IP)/ip/lwip-0.5.3/src/include/ipv4 -I../../support/md5 \
					-I$(PS2LIB)/iop/include -I$(PS2LIB)/common/include

LIBS			=	\
					-L$(MEDIO_IOP) -lmioiop \
					-L$(PS2LIB)/iop/lib -lkernel \
					-lgcc

all: libxisal.a

libxisal.a: $(OBJS)
	$(RM) $@
	ps2-ar cruv $@ $(OBJS)

clean:
	rm -f $(IRXFILES) *.o libxisal.a

