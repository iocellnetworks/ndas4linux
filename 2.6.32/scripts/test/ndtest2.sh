NEW_FILE_SUF=$(date +%H%M)

./start_ndas.sh
sleep 15
echo Mounting..
mount /dev/ndas-00110921\:0p1 mnt1
mount /dev/ndas-50800904\:0p1 mnt2

echo Copying files...
cp mnt1/greece.avi mnt2/greece${NEW_FILE_SUF}.avi
cp mnt2/ls.tgz mnt1/ls${NEW_FILE_SUF}.tgz

echo Comparing file 1...
cmp mnt1/greece.avi mnt2/greece${NEW_FILE_SUF}.avi
echo Comparing file 2...
cmp mnt2/ls.tgz mnt1/ls${NEW_FILE_SUF}.tgz
vlc mnt1/greece.avi &
sleep 5
vlc mnt2/greece${NEW_FILE_SUF}.avi &
sleep 5
vlc mnt2/greece${NEW_FILE_SUF}.avi &
sleep 300
killall vlc

sync
sync
echo Unmounting..
umount mnt2
umount mnt1
./stop_ndas.sh

