NEW_FILE_SUF=$(date +%H%M)

./start_ndas.sh
sleep 15
echo Mounting..
mount /dev/ndas-00110921-0p2 mnt1
mount /dev/ndas-50800904-0p2 mnt2
echo Copying files...
cp mnt1/norway.avi mnt2/nor${NEW_FILE_SUF}.avi
cp mnt2/Premium.m2p mnt1/pre${NEW_FILE_SUF}.m2p
echo Unmounting
umount mnt2
umount mnt1
./stop_ndas.sh

sleep 10

./start_ndas.sh
sleep 5
echo Mounting..
mount /dev/ndas-00110921-0p2 mnt1
mount /dev/ndas-50800904-0p2 mnt2
df
echo Comparing file 1...
cmp mnt1/norway.avi mnt2/nor${NEW_FILE_SUF}.avi
echo Comparing file 2...
cmp mnt2/Premium.m2p mnt1/pre${NEW_FILE_SUF}.m2p
vlc mnt1/norway.avi &
sleep 5
vlc mnt2/nor${NEW_FILE_SUF}.avi &
sleep 5
vlc mnt1/pre${NEW_FILE_SUF}.m2p &
sleep 5
vlc mnt2/Premium.m2p &
sleep 300
killall vlc
sync
sync
echo Unmounting..
umount mnt2
umount mnt1
./stop_ndas.sh

