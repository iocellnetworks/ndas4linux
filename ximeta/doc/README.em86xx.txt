NDAS Driver and Management tool for EM86xx Linux kernel

0. Overview
   This driver is built to support EM8611 Linux 2.4. This driver is composed of OS independent NDAS library("libndas.a" file. source code is not provided) and OS dependent source code. You can customize the source code according to the OS platform and requirement. And if you need changes in NDAS library or have a feature to add, please contact Ximeta, Inc. 

1. Building the driver
 * Set following enviroment varialbes.
 * NDAS_MEMBLK_NO_PRIVATE define added for performace.
     Add it to NDAS_EXTRA_CFLAGS

export NDAS_CROSS_COMPILE=arm-elf-
export NDAS_KERNEL_VERSION=2.4
export NDAS_KERNEL_ARCH=arm
export NDAS_KERNEL_PATH=/armutils/build_arm/linux-2.4.22-em86xx
export NDAS_EXTRA_CFLAGS="-DNDAS_IO_UNIT=32 -D_ARM -DNDAS_SIGPENDING_OLD -DNDAS_MEMBLK_NO_PRIVATE"
export NDAS_EXTRA_LDFLAGS="-elf2flt=\"-s32768\""
   * Note: NDAS_EXTRA_CFLAGS's NDAS_IO_UNIT determines maximum transfer size in kbytes. You can increase it up to 64 but you may need to increase the number of socket buffers when configuring kernel.

 * If you want to use devfs, set following variable also.
export NDAS_DEVFS=y
 * Execute make. This will create ndas_sal.o, ndas_core.o, ndas_block.o and ndasadmin
 * Optionally you can build single module by running "make blk-single". It will create ndas_blk_s.o and ndasadmin

2. Installing & Running
 * Copy mknod.sh, ndas_sal.o, ndas_core.o, ndas_block.o (or ndas_blk_s.o instead of all .o files) and ndasadmin to target system.
 * Execute "insmod ndas_sal.o; insmod ndas_core.o; insmod ndas_block.o" or "insmod ndas_blk_s.o"
 * Execute mknod.sh. This may take a while and you may want to add device files created by this script to romfs.
 * ndasadmin start
 * Use ndasadmin to register/enable/mount NDAS devices. The following is a sample shell commands that mounts NDAS device.
---------------------------------
insmod ndas_sal.o
insmod ndas_core.o
insmod ndas_block.o
./ndasadmin start
./ndasadmin register 00115232 -n Disk1 # or ./ndasadmin register DBHVM-QMVQ2-F892G-0S12A-F5TGS -n ND1
./ndasadmin enable -s 1 -o r
mount /dev/nd/disc0/part1 /mnt/disk1  # or mount /dev/nda1 /mnt/disk1
./ndasadmin disable -s 1
./ndasadmin unregister -n ND1
--------------------------------
 * First NDAS device will be registered as /dev/hda if NDAS_DEVFS is not defined, and /dev/nd/disc0 if NDAS_DEVFS is defined.
 * First partition of first registered NDAS device is /dev/hda1 or /dev/nd/disc0/part1
 * Note that if you have only one registered device at a time, its slot number always is 1. In other cases, consult /proc/ndas/devices/<Device Name>/slots for exact slot number. 
 * See http://code.ximeta.com/trac-ndas/wiki/Usage for detailed usages. 
 
3. About serial number
 * Using serial, you can access NDAS device without entering NDAS ID/key. But it is limited to read-only access.
 * Serial numbers of NDAS devices can be retreived by using "ndasadmin probe" or "cat /proc/ndas/probed". 
 * The length of serial number is 8 for Ximeta's NDAS devices and 16 for some other OEM's NDAS devices.
 * Serial number is usually printed on the case of NDAS devices. OEM's NDAS device prints only first 8 characters of serial number. 
 * You can get a part of NDAS ID from serial using "ndas_query_ndas_id_by_serial" function. You can use this function to show the part of NDAs ID and let user input the last or use this key as identification number of NDAS device.

4. Tips
 * ndasadmin.c is a sample of controlling NDAS devices from user application. 
 * Slot number is allocated once device is registered and discovered by PNP process. If you want to ensure the device's slot number is allocated, use "ndas_detect_device" function. Slot number represents a possible block device. If a single NDAS device has multiple disks, there can be a multiple slot number. And multiple NDAS devices can constitue a single slot.
 * ndasadmin does not provide all information about NDAS devices. You can access additional information through /proc/ndas. Once device is registered, you can access the NDAS device info through /proc/ndas/devices/<device name>/. And you can access HDD info through /proc/ndas/slows/<slot number>/. Slot number can be obtained via /proc/ndas/devices/<device name>/slots or IOCTL_NDCMD_QUERY ioctrl.
 * If you want to get a additional information from user space, you can add proc entry to procfs.c.
