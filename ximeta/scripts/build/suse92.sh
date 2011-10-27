#!/bin/bash
# SuSE 9.2 use gcc 3.3.5
if [ -z "$NXP_VERSION" ] ; then
        echo set NXP_VERSION:
        exit -1;
fi
if [ -z "$NXP_BUILD" ] ; then
        echo set NXP_BUILD:
        exit -1;
fi
. gcc.env
(
    echo $GCC335 \$\* > `pwd`/gcc
    chmod a+x `pwd`/gcc
    export PATH=`pwd`:$PATH
    export NDAS_SUSE=true;
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24-obj/i386/default/ 2.6.8-24-default i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24-obj/i386/smp/ 2.6.8-24-smp i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24-obj/i386/xen/ 2.6.8-24-xen i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24-obj/i386/bigsmp/ 2.6.8-24-bigsmp i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24.10-obj/i386/default/ 2.6.8-24.10-default i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24.10-obj/i386/smp/ 2.6.8-24.10-smp i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24.10-obj/i386/xen/ 2.6.8-24.10-xen i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24.10-obj/i386/bigsmp/ 2.6.8-24.10-bigsmp i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24.19-obj/i386/default/ 2.6.8-24.19-default i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24.19-obj/i386/smp/ 2.6.8-24.19-smp i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24.19-obj/i386/xen/ 2.6.8-24.19-xen i686 suse9.2 suse92
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.2/linux-2.6.8-24.19-obj/i386/bigsmp/ 2.6.8-24.19-bigsmp i686 suse9.2 suse92
)
