# Fedora Core 5
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
( 
    export NDAS_EXTRA_CFLAGS="-DNDAS_KERNEL_2_6_16";
    /bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.15-1.2054_FC5-i686 2.6.15-1.2054_FC5 i686 fc5
    /bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.15-1.2054_FC5smp-i686 2.6.15-1.2054_FC5smp i686 fc5
    /bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.15-1.2054_FC5kdump-i686 2.6.15-1.2054_FC5kdump i686 fc5
    /bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.15-1.2054_FC5xen0-i686 2.6.15-1.2054_FC5xen0 i686 fc5
    /bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.15-1.2054_FC5xenU-i686 2.6.15-1.2054_FC5xenU i686 fc5
)
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2080_FC5-i686 2.6.16-1.2080_FC5 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2080_FC5smp-i686 2.6.16-1.2080_FC5smp i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2080_FC5kdump-i686 2.6.16-1.2080_FC5kdump i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2080_FC5xen0-i686 2.6.16-1.2080_FC5xen0 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2080_FC5xenU-i686 2.6.16-1.2080_FC5xenU i686 fc5

/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2096_FC5-i686 2.6.16-1.2096_FC5 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2096_FC5smp-i686 2.6.16-1.2096_FC5smp i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2096_FC5kdump-i686 2.6.16-1.2096_FC5kdump i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2096_FC5xen0-i686 2.6.16-1.2096_FC5xen0 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2096_FC5xenU-i686 2.6.16-1.2096_FC5xenU i686 fc5

/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2107_FC5-i686 2.6.16-1.2107_FC5 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2107_FC5smp-i686 2.6.16-1.2107_FC5smp i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2107_FC5kdump-i686 2.6.16-1.2107_FC5kdump i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2107_FC5xen0-i686 2.6.16-1.2107_FC5xen0 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2107_FC5xenU-i686 2.6.16-1.2107_FC5xenU i686 fc5

/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2111_FC5-i686 2.6.16-1.2111_FC5 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2111_FC5smp-i686 2.6.16-1.2111_FC5smp i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2111_FC5kdump-i686 2.6.16-1.2111_FC5kdump i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2111_FC5xen0-i686 2.6.16-1.2111_FC5xen0 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2111_FC5xenU-i686 2.6.16-1.2111_FC5xenU i686 fc5

/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2122_FC5-i686 2.6.16-1.2122_FC5 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2122_FC5smp-i686 2.6.16-1.2122_FC5smp i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2122_FC5kdump-i686 2.6.16-1.2122_FC5kdump i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2122_FC5xen0-i686 2.6.16-1.2122_FC5xen0 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2122_FC5xenU-i686 2.6.16-1.2122_FC5xenU i686 fc5

/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2133_FC5-i686 2.6.16-1.2133_FC5 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2133_FC5smp-i686 2.6.16-1.2133_FC5smp i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2133_FC5kdump-i686 2.6.16-1.2133_FC5kdump i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2133_FC5xen0-i686 2.6.16-1.2133_FC5xen0 i686 fc5
/bin/sh ./build-rpm.sh /usr/src/kernels/fc5/2.6.16-1.2133_FC5xenU-i686 2.6.16-1.2133_FC5xenU i686 fc5
