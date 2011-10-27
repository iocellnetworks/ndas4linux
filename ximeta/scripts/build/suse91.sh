#!/bin/bash
# SuSE 91. use gcc 3.3.x ( 3.3.3)
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
    echo $GCC33 \$\* > `pwd`/gcc;
    chmod a+x `pwd`/gcc;
    export PATH=`pwd`:$PATH;
    export NDAS_SUSE=true;
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.1/linux-2.6.4-52-default/ 2.6.4-52-default i586 suse9.1 suse91
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.1/linux-2.6.4-52-smp/ 2.6.4-52-smp i586 suse9.1 suse91
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.1/linux-2.6.4-52-bigsmp/ 2.6.4-52-bigsmp i586 suse9.1 suse91
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.1/linux-2.6.4-62-default/ 2.6.4-62-default i586 suse9.1 suse91
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.1/linux-2.6.4-62-smp/ 2.6.4-62-smp i586 suse9.1 suse91
    /bin/sh ./build-rpm.sh /usr/src/kernels/suse9.1/linux-2.6.4-62-bigsmp/ 2.6.4-62-bigsmp i586 suse9.1 suse91
)
