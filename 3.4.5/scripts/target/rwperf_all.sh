for dir in nd1 nd2 nd3
do
echo "Entering " $dir
cd $dir
echo "==== Writing 256M ===="
rm -rf test256M.bin
sync; sync
tick_bef=`cat /proc/uptime`
dd if=/dev/zero of=test256M.bin count=8192 bs=32k > /dev/null
sync; sync; sync
tick_aft=`cat /proc/uptime`
../bwcalc 2048 $tick_bef $tick_aft

echo "==== Reading 256M ===="
tick_bef=`cat /proc/uptime`
cp test256M.bin /dev/null
tick_aft=`cat /proc/uptime`
../bwcalc 2048 $tick_bef $tick_aft

echo "==== RW 256M ===="
tick_bef=`cat /proc/uptime`
dd if=test256M.bin of=test256M_2.bin count=1024 bs=256k  > /dev/null
sync;sync; sync
tick_aft=`cat /proc/uptime`
../bwcalc 2048 $tick_bef $tick_aft
cd ..
done
