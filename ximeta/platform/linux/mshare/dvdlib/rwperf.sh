
echo "==== Reading 256M ===="
tick_bef=`cat /proc/uptime`
dd if=/dev/nda1 of=/dev/null count=1024 bs=256k 
tick_aft=`cat /proc/uptime`
bwcalc 2048 $tick_bef $tick_aft


