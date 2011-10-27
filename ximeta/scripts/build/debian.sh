if [ -z "$NXP_VERSION" ] ; then
        echo set NXP_VERSION:
        exit -1;
fi
if [ -z "$NXP_BUILD" ] ; then
        echo set NXP_BUILD:
        exit -1;
fi
. gcc.env
# Debian 2.4.27-2-686
(
    echo $GCC34 \$\* > `pwd`/gcc
    chmod a+x `pwd`/gcc
    export PATH=`pwd`:$PATH
    export NDAS_DEBIAN=y; 
    export NDAS_EXTRA_CFLAGS="-DNDAS_SIGPENDING_OLD $NDAS_EXTRA_CFLAGS"; 
    /bin/sh ./build-rpm.sh /usr/src/kernels/debian/kernel-headers-2.4.27-2-686 2.4.27-2-686 i686 debian debian 
    /bin/sh ./build-rpm.sh /usr/src/kernels/debian/kernel-headers-2.4.27-2-686-smp 2.4.27-2-686-smp i686 debian debian 
)
# Debian 2.6.8 kernel need gcc 3.3
# gcc 3.3.x
(
    echo $GCC33 \$\* > `pwd`/gcc
    chmod a+x `pwd`/gcc
    export PATH=`pwd`:$PATH
    export NDAS_DEBIAN=y;
    /bin/sh ./build-rpm.sh /usr/src/kernels/debian/2.6.8-2-686_debian 2.6.8-2-686 i686 debian debian
)
# De	bian 2.6.12 kernel need gcc 4.0
(
    echo $GCC40 \$\* > `pwd`/gcc;
    chmod a+x `pwd`/gcc;
    export PATH=`pwd`:$PATH
    export NDAS_DEBIAN=y;
    /bin/sh ./build-rpm.sh /usr/src/kernels/debian/linux-headers-2.6.12-1-686 2.6.12-1-686 i686 debian debian
    /bin/sh ./build-rpm.sh /usr/src/kernels/debian/linux-headers-2.6.12-1-686-smp 2.6.12-1-686-smp i686 debian debian
# Debian 2.6.15 kernel need gcc 4.0
    /bin/sh ./build-rpm.sh /usr/src/kernels/debian/linux-headers-2.6.15-1-686 2.6.15-1-686 i686 debian debian
    /bin/sh ./build-rpm.sh /usr/src/kernels/debian/linux-headers-2.6.15-1-686-smp 2.6.15-1-686-smp i686 debian debian
)

