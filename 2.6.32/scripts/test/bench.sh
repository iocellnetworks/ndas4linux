echo Reading 4G 
cat /proc/uptime; dd if=/dev/ndas-50800904\:0 of=/dev/null bs=32k count=128k; cat /proc/uptime

echo Writing 4G 
cat /proc/uptime; dd if=/dev/zero of=/dev/ndas-50800904\:0 bs=32k skip=1M count=128k; cat /proc/uptime

