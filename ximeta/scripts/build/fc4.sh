# Fedora Core 4
if [ -z "$NXP_VERSION" ] ; then
	echo set NXP_VERSION:
	exit -1;
fi
if [ -z "$NXP_BUILD" ] ; then
	echo set NXP_BUILD:
	exit -1;
fi
. gcc.env
echo $GCC40 \$\* > `pwd`/gcc;
chmod a+x `pwd`/gcc;
export PATH=`pwd`:$PATH
export NDAS_REDHAT=y;
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.11-1.1369_FC4-i686 2.6.11-1.1369_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.11-1.1369_FC4-smp-i686 2.6.11-1.1369_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.11-1.1369_FC4-xen0-i686 2.6.11-1.1369_FC4xen0 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.11-1.1369_FC4-xenU-i686 2.6.11-1.1369_FC4xenU i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.12-1.1398_FC4-i686 2.6.12-1.1398_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.12-1.1447_FC4-i686 2.6.12-1.1447_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.13-1.1526_FC4-i686 2.6.13-1.1526_FC4 i686 fc4 
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.13-1.1526_FC4-smp-i686 2.6.13-1.1526_FC4smp i686 fc4 
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.13-1.1532_FC4-i686 2.6.13-1.1532_FC4 i686 fc4 
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.13-1.1532_FC4-smp-i686 2.6.13-1.1532_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.14-1.1644_FC4-i686 2.6.14-1.1644_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.14-1.1644_FC4-smp-i686 2.6.14-1.1644_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.14-1.1653_FC4-i686 2.6.14-1.1653_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.14-1.1653_FC4-smp-i686 2.6.14-1.1653_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.14-1.1656_FC4-i686 2.6.14-1.1656_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.14-1.1656_FC4-smp-i686 2.6.14-1.1656_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.15-1.1830_FC4-i686 2.6.15-1.1830_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.15-1.1830_FC4-smp-i686 2.6.15-1.1830_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.15-1.1831_FC4-i686 2.6.15-1.1831_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.15-1.1831_FC4-smp-i686 2.6.15-1.1831_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.15-1.1833_FC4-i686 2.6.15-1.1833_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.15-1.1833_FC4-smp-i686 2.6.15-1.1833_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2069_FC4-i686 2.6.16-1.2069_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2069_FC4-smp-i686 2.6.16-1.2069_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2096_FC4-i686 2.6.16-1.2096_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2096_FC4-smp-i686 2.6.16-1.2096_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2107_FC4-i686 2.6.16-1.2107_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2107_FC4-smp-i686 2.6.16-1.2107_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2108_FC4-i686 2.6.16-1.2108_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2108_FC4-smp-i686 2.6.16-1.2108_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2111_FC4-i686 2.6.16-1.2111_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2111_FC4-smp-i686 2.6.16-1.2111_FC4smp i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2115_FC4-i686 2.6.16-1.2115_FC4 i686 fc4
/bin/sh ./build-rpm.sh /usr/src/kernels/fc4/2.6.16-1.2115_FC4-smp-i686 2.6.16-1.2115_FC4smp i686 fc4
