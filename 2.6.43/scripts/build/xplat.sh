#!/bin/bash
#
. ../../version.mk
. ./xplat.env

if [ -z $3 ] ;then
	echo "usage : $0 (x86|mipsel) (linux) [linux/roku]"
	exit 1
fi
CPU=$1
OS=$2
TARGET=$3

if [ "$1" == "x86" ] ; then
TT=""
else
TT=.$CPU
fi

VER_STRING=$NDAS_VER_MAJOR.$NDAS_VER_MINOR-$NDAS_VER_BUILD
SRC_FILE=ndas-$VER_STRING.tar.gz
DES_FILE=ndas-$VER_STRING$TT.tar.gz

(
	cd $BASE
       echo Building $DES_FILE
	make nxp-os=${OS} $TARGET-clean $TARGET-automated 
	if [ $?  -ne 0 ] ; then
		echo Fail to compile
		exit $?;
	fi
	DIR=$RELEASE_PATH/$NXP_BUILD/$3;
	ssh -l $HOST_USER $HOST_NAME mkdir -p $DIR
	echo scp dist/$SRC_FILE $HOST_USER@$HOST_NAME:$DIR/$DES_FILE
	scp dist/$SRC_FILE $HOST_USER@$HOST_NAME:$DIR/$DES_FILE
	if [ $?  -ne 0 ] ; then
		echo Fail to copy
		exit $?;
	fi
)
