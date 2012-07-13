#!/bin/bash
. version.mk
MACHINE=`echo $(uname -m) | sed -e 's|i6|x|g'`
NDAS_PATH=build_${MACHINE}_linux/ndas-$NDAS_VER_MAJOR.$NDAS_VER_MINOR-$NDAS_VER_BUILD
$NDAS_PATH/ndasadmin stop
/sbin/rmmod ndas_block.ko
/sbin/rmmod ndas_core.ko
/sbin/rmmod ndas_sal.ko

