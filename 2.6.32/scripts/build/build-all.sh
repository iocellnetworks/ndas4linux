#!/bin/bash
#

# Update 
svn co http://svn/repos/netdisk_xplat/trunk ../../

. ../../version.mk

export NXP_VERSION="$NDAS_VER_MAJOR.$NDAS_VER_MINOR";
export NXP_BUILD="$NDAS_VER_BUILD";

rm -rf ~/.rpmmacros
. xplat.env

echo Builing version $NXP_VERSION build $NXP_BUILD
# linux tarball
/bin/sh ./xplat.sh x86 linux linux 
if [ ! $? ] ; then
    echo Fail to Build
    exit $?;
fi

# roku
#(
#    . roku.env ; 
#    /bin/sh ./xplat.sh mipsel linux roku ${NXP_BUILD} roku
#)
# OpenWRT
# Build tarball
export NDAS_OPENWRT_PATH=/home/sjcho/openwrt/whiterussian-0.9
/bin/sh ./xplat.sh mipsel linux openwrt
# Build ipkg files 
/bin/sh ./build-ipkg.sh ${NDAS_OPENWRT_PATH}/build_mipsel/linux 2.4.30 ${NDAS_OPENWRT_PATH}/staging_dir_mipsel/bin:${NDAS_OPENWRT_PATH}/staging_dir_mipsel/usr/bin whiterussian-0.9

# Update current build link
#ssh release@ndas4linux.iocellnetworks.com rm dev/current ";" ln -sf ./${NXP_VERSION}/${NXP_BUILD} dev/current

