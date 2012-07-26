TARGET = ndasfat
OBJS = ndasfat.o
MDIR = drivers/misc

EXTRA_CFLAGS = -DEXPORT_SYMTAB 
EXTRA_CFLAGS += -I/root/netdisk_xplat/inc 
EXTRA_CFLAGS += -DLINUX -D_X86 -DXPLAT_ENABLE_EMBED_LINUX_OPT

CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
PWD = $(shell pwd)
DEST = /lib/modules/$(CURRENT)/kernel/$(MDIR)

CURRENT = 2.6.21-prep

obj-m      := $(TARGET).o 
$(TARGET)-objs := ndasfat-driver.o namei.o cache.o dir.o \
		  fatent.o file.o inode.o misc.o ndft.o secondary.o secondarythread.o lfslib.o md5.o

default:
	make -C $(KDIR) SUBDIRS=$(PWD) modules

$(TARGET).o: $(OBJS)
	$(LD) $(LD_RFLAG) -r -o $@ $(OBJS)

ifneq (,$(findstring 2.4.,$(CURRENT)))
install:
	su -c "cp -v $(TARGET).o $(DEST) && /sbin/depmod -a"
else
install:
	su -c "cp -v $(TARGET).ko $(DEST) && /sbin/depmod -a"
endif

clean:
	-rm -f *.o *.ko .*.cmd .*.flags *.mod.c

-include $(KDIR)/Rules.make
