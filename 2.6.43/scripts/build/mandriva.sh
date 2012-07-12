# Mandriva 2006 with GCC 4.0.1
if [ -z "$NXP_VERSION" ] ; then
	echo set NXP_VERSION:
	exit -1;
fi
if [ -z "$NXP_BUILD" ] ; then
	echo set NXP_BUILD:
	exit -1;
fi
. gcc.env
echo $GCC40 \$\* > `pwd`/gcc
chmod a+x `pwd`/gcc
export PATH=`pwd`:$PATH
export NDAS_REDHAT=y;
/bin/sh ./build-rpm.sh /usr/src/kernels/mandriva/linux-2.6.12-12mdk/ 2.6.12-12mdk i686 mandriva.2006
/bin/sh ./build-rpm.sh /usr/src/kernels/mandriva/linux-2.6.12-12mdksmp/ 2.6.12-12mdksmp i686 mandriva.2006
/bin/sh ./build-rpm.sh /usr/src/kernels/mandriva/linux-2.6.12-12.18mdk/ 2.6.12-12.18mdk i686 mandriva.2006
/bin/sh ./build-rpm.sh /usr/src/kernels/mandriva/linux-2.6.12-12.18mdksmp/ 2.6.12-12.18mdksmp i686 mandriva.2006
