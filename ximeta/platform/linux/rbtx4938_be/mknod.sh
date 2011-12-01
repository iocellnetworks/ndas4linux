#/bin/sh
    LINKDIR=/dev/ndas
    ROOT=
    MAJOR=60
    SPECIAL_MINOR=128
    NR_PART=15

    if [ ! -e ${LINKDIR} ] ; then
        mknod ${LINKDIR} b ${MAJOR} ${SPECIAL_MINOR}
    else
        rm -rf ${LINKDIR}
        mknod ${LINKDIR} b ${MAJOR} ${SPECIAL_MINOR}
    fi

    DEVICE_LIST="${ROOT}/dev/nda ${ROOT}/dev/ndb ${ROOT}/dev/ndc\
        ${ROOT}/dev/ndd ${ROOT}/dev/nde ${ROOT}/dev/ndf\
        ${ROOT}/dev/ndg ${ROOT}/dev/ndh"

    MINOR=0

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
    done
