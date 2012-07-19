\rm -rf build_x86_linux
cd xplat
make linux-dev
cd /home/sjcho/build_x86_linux/ndas-1.0.4-0
export NDAS_DEBUG=y
make
cd ../..

