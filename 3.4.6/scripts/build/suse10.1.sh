#!/bin/bash
# SuSE 10.1 use gcc 4.1.0
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
    echo $GCC41 \$\* > `pwd`/gcc
    chmod a+x `pwd`/gcc
    export PATH=`pwd`:$PATH
    export NDAS_SUSE=true;
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse10.1/linux-2.6.16.13-4-obj/i386/default/ 2.6.16.13-4-default i586 suse10.1 suse101
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse10.1/linux-2.6.16.13-4-obj/i386/smp/ 2.6.16.13-4-smp i586 suse10.1 suse101
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse10.1/linux-2.6.16.13-4-obj/i386/xen/ 2.6.16.13-4-xen i586 suse10.1 suse101
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse10.1/linux-2.6.16.13-4-obj/i386/xen/ 2.6.16.13-4-xenpae i586 suse10.1 suse101
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse10.1/linux-2.6.16.13-4-obj/i386/bigsmp/ 2.6.16.13-4-bigsmp i586 suse10.1 suse101
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse10.1/linux-2.6.16.13-4-obj/i386/kdump/ 2.6.16.13-4-kdump i586 suse10.1 suse101
)
