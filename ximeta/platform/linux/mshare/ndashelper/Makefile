NDAS_ROOT?=$(shell pwd)/../../../..

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
MSCFLAGS:=-DLINUX -I$(NDAS_ROOT)/inc  -I. -D_ARM -DNDAS_WRITE=1 -I$(NDAS_CROSS_COMPILE_INCLUDE) -fomit-frame-pointer \
                        -idirafter -O2 -DDEBUGLEVEL=1 -MMD -c -DILGU_LINUX -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DDVD_RIP_HACK -DNDAS_MSHARE 
MSLDFLAGS= -elf2flt="s32768" -static

OBJECT= ndas_api.o ndas_helper.o
all:libndasHelper.o libndasHelper.a

libndasHelper.o : $(OBJECT)  
	$(MSHARE-LD) -o $@ $(MSLDFLAGS) -r $(OBJECT)

libndasHelper.a : $(OBJECT)
	$(MSHARE-AR) rcs $@ $(OBJECT)

ndas_helper.o: ndas_helper.cpp
	$(MSHARE-CPP) $(MSCFLAGS) $(MSLDFLAGS) $^  -o $@
ndas_api.o: ndas_api.c
	$(MSHARE-CC) $(MSCFLAGS) $(MSLDFLAGS) $^ -o $@
clean:
	rm -f libndasHelper.o
	rm -f ndas_api.o
	rm -f ndas_helper.o
