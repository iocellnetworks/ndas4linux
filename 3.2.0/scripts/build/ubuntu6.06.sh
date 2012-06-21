#!/bin/bash
if [ -z "$NXP_VERSION" ] ; then
        echo set NXP_VERSION:
        exit -1;
fi
if [ -z "$NXP_BUILD" ] ; then
        echo set NXP_BUILD:
        exit -1;
fi
. gcc.env
# Ubuntu 6.06 uses gcc-4.03
(
    echo $GCC40 \$\* > `pwd`/gcc
    chmod a+x `pwd`/gcc
    export PATH=`pwd`:$PATH
    export NDAS_DEBIAN=y;
    /bin/sh ./build-rpm.sh /usr/src/kernels/ubuntu6.06/linux-headers-2.6.15-23-686 2.6.15-23-686 i686 ubuntu dapper
    rm `pwd`/gcc
)
