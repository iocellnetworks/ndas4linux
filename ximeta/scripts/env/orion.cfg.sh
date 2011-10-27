export NDAS_CROSS_COMPILE="arm-none-linux-gnueabi-"
#export NDAS_EXTRA_CFLAGS="-I/home/sjcho/linux-feroceon_2_2_2/include -nostdinc -isystem /home/sjcho/arm-toolchain/bin/../lib/gcc/arm-none-linux-gnueabi/3.4.4/include -D__KERNEL__ -Iinclude  -mlittle-endian -Wall -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -Os -fno-omit-frame-pointer -ffunction-sections  -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mabi=aapcs-linux -mno-thumb-interwork -D__LINUX_ARM_ARCH__=5 -march=armv5t -mtune=xscale  -msoft-float -Uarm -Wdeclaration-after-statement  -DMODULE -DLINUX -I/home/sjcho/xplat/build_orion/ndas-1.1-10/inc  -D_ARM   -DMODULE -DKBUILD_BASENAME=ndas_block_main -DKBUILD_MODNAME=ndas_block"
export NDAS_KERNEL_ARCH="arm"
export NDAS_KERNEL_PATH="/home/sjcho/linux-feroceon_2_2_2"
export NDAS_KERNEL_VERSION="2.6.12"

