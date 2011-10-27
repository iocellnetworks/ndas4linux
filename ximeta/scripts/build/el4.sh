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

# CentOS 4.x or Redhat Enterprise Linux 4.x
# gcc 3.4.x
(
    export NDAS_REDHAT=y; 
    echo $GCC34 \$\* > `pwd`/gcc
    chmod a+x `pwd`/gcc
    export PATH=`pwd`:$PATH
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/2.6.9-22.EL-i686 2.6.9-22.EL i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/2.6.9-22.EL-smp-i686 2.6.9-22.ELsmp i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/2.6.9-22.0.1.EL-i686 2.6.9-22.0.1.EL i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/2.6.9-22.0.1.EL-smp-i686 2.6.9-22.0.1.ELsmp i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/2.6.9-22.0.2.EL-i686 2.6.9-22.0.2.EL i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/2.6.9-22.0.2.EL-smp-i686 2.6.9-22.0.2.ELsmp i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/linux-2.6.9-27.EL 2.6.9-27.EL i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/linux-2.6.9-27.ELsmp 2.6.9-27.ELsmp i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/linux-2.6.9-27.ELhugemem 2.6.9-27.ELhugemem i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/2.6.9-34.EL-i686 2.6.9-34.EL i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/2.6.9-34.EL-smp-i686 2.6.9-34.ELsmp i686 el4
    /bin/sh ./build-rpm.sh /usr/src/kernels/el4/2.6.9-34.EL-hugemem-i686 2.6.9-34.ELhugemem i686 el4
)
