ifeq (0, 1) 
TARGET 		:= ndas_lpx

obj-m      	:= $(TARGET).o

OBJS 			:= ndas_lpx.o
$(TARGET)-objs	:= af_lpx.o lpx_proc.o sysctl_net_lpx.o lpx_kernel.o

PWD = $(shell pwd)

MDIR = drivers/misc

EXTRA_CFLAGS = -DEXPORT_SYMTAB -I$(SUBDIRS)/../inc -I$(SUBDIRS)/../../../../inc

CURRENT = $(shell uname -r)
KDIR = /lib/modules/$(CURRENT)/build
DEST = /lib/modules/$(CURRENT)/kernel/$(MDIR)

ifeq (0, 1) 

CURRENT = 2.6.34
TOPDIR = /root/kamikaze_7.09/build_mipsel/linux

default:
	make -C $(TOPDIR) CROSS_COMPILE=/root/kamikaze_7.09/staging_dir_mipsel/bin/mipsel-linux-uclibc- ARCH=mips SUBDIRS=$(PWD) modules

else

CURRENT = 2.4.30
TOPDIR = /root/whiterussian-0.9/build_mipsel/linux

default:
	make -C $(TOPDIR) CROSS_COMPILE=/root/whiterussian-0.9/staging_dir_mipsel/bin/mipsel-linux-uclibc- ARCH=mips SUBDIRS=$(PWD) modules

$(TARGET).o: $($(TARGET)-objs)
	$(LD) $(LD_RFLAG) -r -o $@ $($(TARGET)-objs)

endif

ifneq (,$(findstring 2.4.,$(CURRENT)))
install:
	su -c "cp -v $(TARGET).o $(DEST) && /sbin/depmod -a"
else
install:
	su -c "cp -v $(TARGET).ko $(DEST) && /sbin/depmod -a"
endif

clean:
	-rm -f *.o *.ko .*.cmd .*.flags *.mod.c

-include $(TOPDIR)/Rules.make
endif