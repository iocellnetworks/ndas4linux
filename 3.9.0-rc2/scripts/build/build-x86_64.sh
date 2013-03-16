#!/bin/bash
#


# Update 
svn co http://svn/repos/netdisk_xplat/trunk ../../
. ../../version.mk

export NXP_VERSION="$NDAS_VER_MAJOR.$NDAS_VER_MINOR";
export NXP_BUILD="$NDAS_VER_BUILD";

rm -rf ~/.rpmmacros
. ./xplat.env

echo Builing x86_64 version $NXP_VERSION build $NXP_BUILD

# linux tarball
/bin/sh ./xplat.sh x86_64 linux linux64 
if [ ! $? ] ; then
    echo Fail to Build
    exit $?;
fi

