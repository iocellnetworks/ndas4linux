\rm -rf build_x86_linux
cd xplat
make linux
cd /home/sjcho/build_x86_linux/ndas-1.0.4-0
unset NDAS_DEBUG
make
cd ../..

