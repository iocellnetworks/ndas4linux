#./mknod.sh
#echo Bringing up eth1
#ifconfig eth1 192.168.1.1
#sleep 5
insmod ndas_sal.ko; insmod ndas_core.ko; insmod ndas_block.ko
./ndasadmin start
./ndasadmin register 5J2TU-F9PSQ-J237K-PBEEU-B2JQ8 -n V1
./ndasadmin enable -s 1 -o w
mount -t vfat /dev/nda1 nd1
mount -t ext2 /dev/nda2 nd2
mount -t reiserfs /dev/nda3 nd3

