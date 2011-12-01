BIN=/opt/tools-install/bin
#SYABAS_SDK=$(HOME)/xplat/SYABAS3_SDK
SYABAS_SDK=..

CC=$(BIN)/arm-elf-gcc
AR=$(BIN)/arm-elf-ar

INCLUDE = -I$(SYABAS_SDK) -I$(SYABAS_SDK)/os -I$(SYABAS_SDK)/tcpip/include \
          -I$(SYABAS_SDK)/file_sys -I$(SYABAS_SDK)/upnp -I$(SYABAS_SDK)/syabas3\
          -I/usr/local/arm-elf.1.10.0/include -I/opt/tools-install/lib/gcc-lib/arm-elf/2.95.3/include -I./inc
CFLAGS= $(INCLUDE) -O2 -fomit-frame-pointer -fno-builtin -nostdinc -nostdlib -ffunction-sections \
        -fdata-sections -DUCOSII -D_ARM -DTCPIP_PORT -DSYABAS -DUPNP -DNDAS

NDAS_SAL_SRCS=ndbench.c $(addprefix ./sal/,debug.c mem.c net.c sync.c thread.c libc.c sal.c time.c)
NDAS_SAL_OBJS = $(NDAS_SAL_SRCS:.c=.o)

# Select between debug version and release version
USE_DEBUG_VER=n

ifeq ($(USE_DEBUG_VER), y)
BASE_LIBNDAS = libndas-debug.a
CFLAGS+=-DDEBUG -DNDAS_DEBUG
else
BASE_LIBNDAS = libndas-release.a
endif

TARGET_LIBNDAS = libndas.a

all: $(TARGET_LIBNDAS)

$(TARGET_LIBNDAS): $(BASE_LIBNDAS) $(NDAS_SAL_OBJS)
	rm -rf $@
	cp $(BASE_LIBNDAS) $@ 
	$(AR) r $@ $(NDAS_SAL_OBJS)
	
%.o: %.c
	$(CC) -c -o $@ $^ $(CFLAGS)

clean: 
	rm -f $(NDAS_SAL_OBJS) $(TARGET_LIBNDAS)

