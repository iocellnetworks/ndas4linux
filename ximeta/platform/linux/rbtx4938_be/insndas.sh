./mknod.sh
insmod ndas_sal.ko
insmod ndas_core.ko
insmod ndas_block.ko
./ndasadmin start
./ndasadmin register 

