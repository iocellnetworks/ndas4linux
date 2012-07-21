#!/bin/bash
. version.mk
MACHINE=`echo $(uname -m) | sed -e 's|i6|x|g'`
NDAS_PATH=build_${MACHINE}_linux/ndas-$NDAS_VER_MAJOR.$NDAS_VER_MINOR-$NDAS_VER_BUILD
/sbin/insmod $NDAS_PATH/ndas_sal.ko
/sbin/insmod $NDAS_PATH/ndas_core.ko
/sbin/insmod $NDAS_PATH/ndas_block.ko
$NDAS_PATH/ndasadmin start

