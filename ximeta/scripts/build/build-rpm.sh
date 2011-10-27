#
. xplat.env
if [ -z $4 ] ;then
	#echo "usage : $0 [kernel path] [kernel version] [architect] [category] [os]"
	exit 1 ;
fi
echo 
echo NDAS_KERNEL_PATH=$1
echo NDAS_KERNEL_VERSION=$2
if [ "${2}" == "`echo ${2} | sed -e 's|smp||'`" ] ; then
	export NKV=`echo $2 | sed -e 's|-|_|g'` ;
	SMP=0;
#	NAME=ndas;
#	RPM=/usr/src/redhat/RPMS/${3}/ndas-${NXP_VERSION}-${NKV}.${NXP_BUILD}.${3}.rpm;
else
	export NKV=`echo $2 | sed -e 's|-|_|g' -e 's|smp||g'` ;
	SMP=1;
#	NAME=ndas-smp;
#	RPM=/usr/src/redhat/RPMS/${3}/ndas-smp-${NXP_VERSION}-${NKV}.${NXP_BUILD}.${3}.rpm;
fi
export NDAS_KERNEL_PATH=$1 ; 
export NDAS_KERNEL_VERSION=$2 ; 
(
	cd $BASE 
	echo rpmbuild -tb ./build_x86_linux/ndas-${NXP_VERSION}-${NXP_BUILD}.tar.gz --target=$3 --nodeps from $BASE
	rpmbuild -tb ./build_x86_linux/ndas-${NXP_VERSION}-${NXP_BUILD}.tar.gz --target=$3 --nodeps
	if [ $?  -ne 0 ] ; then
		echo Fail to Build ndas-${NXP_VERSION}-${NXP_BUILD}.tar.gz
		exit $?;
	fi
	RPM=`cat /tmp/.file`
	if [ -z $5 ] ;then
		BIN=$RPM.bin;
	else
		BIN=${RPM/${NXP_BUILD}.${3}.rpm/${5}.${NXP_BUILD}.${3}.rpm}.bin
	fi
	/bin/sh ./platform/linux/gen-sfx.sh $RPM > $BIN
	if [ $?  -ne 0 ] ; then
		echo Fail to Build $RPM.bin
		exit $?;
	fi
        ssh -l jhpark code.ximeta.com mkdir -p dev/${NXP_VERSION}/${NXP_BUILD}/$4/
	scp $BIN $RELEASE/$NXP_BUILD/$4/ ;
)
