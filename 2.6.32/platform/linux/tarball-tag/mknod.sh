#!/bin/sh
# -------------------------------------------------------------------------
# Copyright (c) 2002-2006, XIMETA, Inc., FREMONT, CA, USA.
# All rights reserved.
#
# LICENSE TERMS
#
# The free distribution and use of this software in both source and binary 
# form is allowed (with or without changes) provided that:
#
#   1. distributions of this source code include the above copyright 
#      notice, this list of conditions and the following disclaimer;
#
#   2. distributions in binary form include the above copyright
#      notice, this list of conditions and the following disclaimer
#      in the documentation and/or other associated materials;
#
#   3. the copyright holder's name is not used to endorse products 
#      built using this software without specific written permission. 
#      
# ALTERNATIVELY, provided that this notice is retained in full, this product
# may be distributed under the terms of the GNU General Public License (GPL v2),
# in which case the provisions of the GPL apply INSTEAD OF those given above.
# 
# DISCLAIMER
#
# This software is provided 'as is' with no explcit or implied warranties
# in respect of any properties, including, but not limited to, correctness 
# and fitness for purpose.
#-------------------------------------------------------------------------
LKV=`uname -r | cut -c1-3`
NDAS_CTRL_DEV=/dev/ndas
NDAS_CTRL_DEV_MAJOR=60
NDAS_CTRL_DEV_MINOR=0
ROOT=/
MAJOR=60


# if udev is installed, we don't create block device files
if [ -e /sbin/udevd ] ; then
    echo "Kernel supports udev. This script is not required"
    exit
fi

#
# To fix bug #53 of code.ximeta.com
#
RETRIAL=0
while [ ! -e /proc/ndas/max_slot ];
do
	sleep 1;
	RETRIAL=`expr ${RETRIAL} + 1`
	if [ $RETRIAL -ge 5 ] ; then
		break;
	fi
done

if [ ${LKV} = "2.6" ] ; then
    NR_SLOT=`cat /proc/ndas/max_slot`
    if [ -z "${NR_SLOT}" ] ; then
	NR_SLOT=16;
    fi
    NR_PART=15
else
    NR_SLOT=`cat /proc/ndas/max_slot`
    if [ -z "${NR_SLOT}" ] ; then
	NR_SLOT=32;
    fi
    NR_PART=7
fi 

if [ -e ${NDAS_CTRL_DEV} ] ; then
    rm -rf ${NDAS_CTRL_DEV}
fi

mknod -m644 ${NDAS_CTRL_DEV} c ${NDAS_CTRL_DEV_MAJOR} ${NDAS_CTRL_DEV_MINOR}

DEVICE_LIST="${ROOT}/dev/nda ${ROOT}/dev/ndb ${ROOT}/dev/ndc 
    ${ROOT}/dev/ndd ${ROOT}/dev/nde ${ROOT}/dev/ndf"

#
#Use following device list if you want to use devices more than 6
#
#DEVICE_LIST="${ROOT}/dev/nda ${ROOT}/dev/ndb ${ROOT}/dev/ndc 
#    ${ROOT}/dev/ndd ${ROOT}/dev/nde ${ROOT}/dev/ndf 
#    ${ROOT}/dev/ndg ${ROOT}/dev/ndh ${ROOT}/dev/ndi 
#    ${ROOT}/dev/ndj ${ROOT}/dev/ndk ${ROOT}/dev/ndl 
#    ${ROOT}/dev/ndm ${ROOT}/dev/ndn ${ROOT}/dev/ndo 
#    ${ROOT}/dev/ndp ${ROOT}/dev/ndq ${ROOT}/dev/ndr 
#    ${ROOT}/dev/nds ${ROOT}/dev/ndt ${ROOT}/dev/ndu 
#    ${ROOT}/dev/ndv ${ROOT}/dev/ndw ${ROOT}/dev/ndx 
#    ${ROOT}/dev/ndy ${ROOT}/dev/ndz"

MINOR=0
SLOT=1

for DEVICE in $DEVICE_LIST
do
    if [ -e ${DEVICE} ] ; then
        rm -rf ${DEVICE}
    fi
    mknod -m644 ${DEVICE} b ${MAJOR} ${MINOR} 

    MINOR=`expr ${MINOR} + 1`
    
    NRMINOR=1
    while [ ${NRMINOR} -le ${NR_PART} ];
    do
        if [ -e ${DEVICE}${NRMINOR} ] ; then
            rm -rf ${DEVICE}${NRMINOR}
        fi
        mknod -m644 ${DEVICE}${NRMINOR} b ${MAJOR} ${MINOR} 
        NRMINOR=`expr ${NRMINOR} + 1`
        MINOR=`expr ${MINOR} + 1`
    done
    SLOT=`expr ${SLOT} + 1`
    if [ ${SLOT} -ge ${NR_SLOT} ] ; then
        exit
    fi
done

