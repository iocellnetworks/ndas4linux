#

. ../../version.mk
. xplat.env

if [ -z $4 ] ;then
	echo "usage : $0 [kernel path] [kernel version] [tool path] [release name]"
	exit 1 ;
fi
(
	export NDAS_KERNEL_PATH=$1;
	export NDAS_KERNEL_VERSION=$2;
	export NDAS_KERNEL_ARCH=mipsel;
	export NDAS_CROSS_COMPILE=mipsel-linux-
	export NDAS_CROSS_COMPILE_UM=mipsel-linux-uclibc-
	export NDAS_EXTRA_CFLAGS="-DNAS_IO_UNIT=32 -DLINUX -DNO_DEBUG_ESP -mlong-calls -DNDAS_SIGPENDING_OLD"
	export PATH=$PATH:$3

	cd $BASE/build_openwrt/ndas-${NXP_VERSION}-${NXP_BUILD}/;
	IPKG_MODULE=$BASE/build_openwrt/ndas-${NXP_VERSION}-${NXP_BUILD}/ndas-kernel_${NXP_VERSION}-${NXP_BUILD}_mipsel.ipk;
	IPKG_ADMIN=$BASE/build_openwrt/ndas-${NXP_VERSION}-${NXP_BUILD}/ndas-admin_${NXP_VERSION}-${NXP_BUILD}_mipsel.ipk;
	BIN=$IPKG_MODULE.bin;
	make clean ipkg
	if [ $?  -ne 0 ] ; then
		echo Fail to Build $IPKG_MODULE or $IPKG_ADMIN
		exit $?;
	fi
        ssh -l release code.ximeta.com mkdir -p dev/${NXP_VERSION}/${NXP_BUILD}/openwrt
        ssh -l release code.ximeta.com mkdir -p dev/${NXP_VERSION}/${NXP_BUILD}/openwrt/$4
	scp $IPKG_MODULE $IPKG_ADMIN $RELEASE/$NXP_BUILD/openwrt/$4 ;
)
