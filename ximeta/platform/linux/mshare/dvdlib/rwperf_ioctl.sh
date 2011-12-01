
echo "==== Reading 256M ===="
tick_bef=`cat /proc/uptime`
./test_ioctl /dev/nda1 32 154112
tick_aft=`cat /proc/uptime`
../bwcalc 2048 $tick_bef $tick_aft

