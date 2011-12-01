######################################################################
# Copyright ( C ) 2002-2005 XIMETA, Inc.
# All rights reserved.
######################################################################
EE_PREFIX = ee-
EE_CC = $(EE_PREFIX)gcc
EE_CXX= $(EE_PREFIX)g++
EE_AS = $(EE_PREFIX)as
EE_LD = $(EE_PREFIX)ld
EE_AR = $(EE_PREFIX)ar
EE_OBJCOPY = $(EE_PREFIX)objcopy
EE_STRIP = $(EE_PREFIX)strip

EE_BIN = $(nxp-dist)/eesample.elf
EE_OBJS = eetest.o graph.o font.o
EE_LIBS = -lps2ip -lc -lpad

EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I. -I$(PS2LIB)/ee/include 
EE_CFLAGS := -D__LITTLE_ENDIAN_BYTEORDER -D__LITTLE_ENDIAN_BITFIELD -D_EE -mips3 -EL -O2 -Wall

EE_LDFLAGS := -L$(PS2SDK)/ee/lib $(EE_LDFLAGS)
EE_LIBS += -lkernel -lsyscall -lc


all: $(EE_BIN)

$(EE_BIN) : $(EE_OBJS) $(PS2SDK)/ee/startup/crt0.o
	$(EE_CC) -nostartfiles -T$(PS2SDK)/ee/startup/linkfile $(EE_LDFLAGS) \
		-o $(EE_BIN) $(PS2SDK)/ee/startup/crt0.o $(EE_OBJS) $(EE_LIBS)
%.o : %.c
	$(EE_CC) $(EE_CFLAGS) $(EE_INCS) -c $< -o $@

clean:
	rm -f #.elf #.o #.a $(EE_BIN)

