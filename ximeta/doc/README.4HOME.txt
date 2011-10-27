NDAS Driver and Management tool for 4HomeMedia CP1000
	Driver version 1.0.5 R3
	5/8/2007


0. Overview
   This driver is built to support 4HomeMedia CP1000 Linux kernel 2.6.15. This driver is composed of OS independent NDAS library("libndas.a" file. source code is not provided) and OS dependent source code. You can customize the source code according to the OS platform and requirement. And if you need changes in NDAS library or have feature request, please contact Ximeta, Inc. 

1. Building the driver
 * Set the enviroment varialbes properly
export NDAS_CROSS_COMPILE=/buildroot/build_mips_nofpu/staging_dir/bin/mips-linux-uclibc-
export NDAS_EXTRA_CFLAGS=-I/buildroot/build_mips_nofpu/staging_dir/bin/mips-linux-uclibc-
export NDAS_KERNEL_ARCH=mips
export NDAS_KERNEL_PATH=/buildroot/build_mips_nofpu/linux-2.6.15-pmc
export NDAS_KERNEL_VERSION=2.6.15
 * You may need to add mips-linux-objdump's path to PATH
 * Execute make. This will create ndas_sal.ko, ndas_core.ko, ndas_block.ko and ndasadmin
 
2. Installing & Running
 * Copy ndas_sal.ko, ndas_core.ko, ndas_block.ko and ndasadmin to target system.
 * insmod ndas_sal.ko
 * insmod ndas_core.ko
 * insmod ndas_block.ko
 * If the kernel support udev, mknod.sh is not required. Device will be created as needed.
 * Use ndasadmin to register/enable/mount NDAS devices. The following is sample shell commands that register and mount NDAS device.
---------------------------------
insmod ./ndas_sal.ko
insmod ./ndas_core.ko
insmod ./ndas_block.o
./ndasadmin start
./ndasadmin register BL8ES-32DGM-AR2LY-UQA1A-9A5FQ -n Disk1
./ndasadmin enable -s 1 -o w
mount /dev/ndas-0011100-0p1  /mnt/disk1
umount /mnt/disk1
./ndasadmin disable -s 1
./ndasadmin unregister -n Disk1
--------------------------------
 * NDAS block device file will be created as /dev/ndas-{serial}-0 for the whole device and /dev/ndas-{serial}-0p{partition} for each partition. 
 * How to register the device or how to determine the name of the device file depends on the NDAS driver's configuration. Please contact Ximeta to change these policies.
 * See http://code.ximeta.com/trac-ndas/wiki/Usage for detailed usage.

3. Customization
 * Some of the NDAS drivers and administration tool are provided and you can change them as you want. And it can be used as sample code.
