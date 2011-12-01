#/bin/sh
LKV=`uname -r | cut -c1-3`
NR_SLOT=8
NDAS_SPECIAL_FILE=/dev/ndas
ROOT=/
MAJOR=60
if [ ${LKV} = "2.6" ] ; then
    NR_PART=15
    SPECIAL_MINOR=$((${NR_SLOT} * 16))
else
    NR_PART=7
    SPECIAL_MINOR=0
fi 

if [ ! -e ${NDAS_SPECIAL_FILE} ] ; then
    mknod ${NDAS_SPECIAL_FILE} b ${MAJOR} ${SPECIAL_MINOR}
else
    rm -rf ${NDAS_SPECIAL_FILE}
    mknod ${NDAS_SPECIAL_FILE} b ${MAJOR} ${SPECIAL_MINOR}
fi
# if udev is installed, we don't create block device files
if [ -e /sbin/udev ] ; then
    exit
fi

DEVICE_LIST="${ROOT}/dev/nda ${ROOT}/dev/ndb ${ROOT}/dev/ndc 
    ${ROOT}/dev/ndd ${ROOT}/dev/nde ${ROOT}/dev/ndf 
    ${ROOT}/dev/ndg ${ROOT}/dev/ndh ${ROOT}/dev/ndi 
    ${ROOT}/dev/ndj ${ROOT}/dev/ndk ${ROOT}/dev/ndl 
    ${ROOT}/dev/ndm ${ROOT}/dev/ndn ${ROOT}/dev/ndo 
    ${ROOT}/dev/ndp ${ROOT}/dev/ndq ${ROOT}/dev/ndr 
    ${ROOT}/dev/nds ${ROOT}/dev/ndt ${ROOT}/dev/ndu 
    ${ROOT}/dev/ndv ${ROOT}/dev/ndw ${ROOT}/dev/ndx 
    ${ROOT}/dev/ndy ${ROOT}/dev/ndz"

MINOR=0
SLOT=1

for DEVICE in $DEVICE_LIST
do
    if [ ! -e ${DEVICE} ] ; then
        mknod ${DEVICE} b ${MAJOR} ${MINOR} 
    else
        rm -rf ${DEVICE}
        mknod ${DEVICE} b ${MAJOR} ${MINOR} 
    fi

    MINOR=$((${MINOR} + 1))
    
    NRMINOR=1
    while [ ${NRMINOR} -le ${NR_PART} ];
    do
        if [ ! -e ${DEVICE}${NRMINOR} ] ; then
            mknod ${DEVICE}${NRMINOR} b ${MAJOR} ${MINOR} 
        else
            rm -rf ${DEVICE}${NRMINOR}
            mknod ${DEVICE}${NRMINOR} b ${MAJOR} ${MINOR} 
        fi
        NRMINOR=$((${NRMINOR} + 1))
        MINOR=$((${MINOR} + 1))
    done
    SLOT=$((${SLOT} + 1))
    if [ ${SLOT} -ge ${NR_SLOT} ] ; then
        exit
    fi
done
