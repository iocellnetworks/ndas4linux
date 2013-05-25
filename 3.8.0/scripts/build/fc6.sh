# Fedora Core 6
if [ -z "$NXP_VERSION" ] ; then
	echo set NXP_VERSION:
	exit -1;
fi
if [ -z "$NXP_BUILD" ] ; then
	echo set NXP_BUILD:
	exit -1;
fi
. gcc.env
echo $GCC41 \$\* > `pwd`/gcc;
chmod a+x `pwd`/gcc;
export PATH=`pwd`:$PATH
export NDAS_REDHAT=y;
/bin/sh ./build-rpm.sh /usr/src/kernels/fc6/2.6.16-1.2074_FC6-i686 2.6.16-1.2074_FC6 i686 fc6
/bin/sh ./build-rpm.sh /usr/src/kernels/fc6/2.6.16-1.2083_FC6-i686 2.6.16-1.2083_FC6 i686 fc6
