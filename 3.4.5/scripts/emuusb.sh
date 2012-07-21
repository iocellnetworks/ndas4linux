#!/bin/sh

#
# Find the internal USB disk using USB device number.
#

RET="-1"
for i in 0 1 2 3 4 5 6
do
	USBSTORAGE_DEVFILE=/proc/scsi/usb-storage-$i/$i
	if [ -r ${USBSTORAGE_DEVFILE} ]
	then
		grep 'Attached: Yes' /proc/scsi/usb-storage-$i/$i >/dev/null
		if [ "$?" = "0" ]
		then
			SCSIHOST=`sed /proc/scsi/usb-storage-$i/$i -n -e '/^[ \t]*Host scsi/ { s/^[ \t]*Host scsi// ; s/:.*$//p }'`
			USBDEVNO=`sed /proc/scsi/usb-storage-$i/$i -n -e '/^[ \t]*Usb devnum:*/{ s/^[ \t]*Usb devnum:[ \t]*//p ; s/[ \t\n]// }'`
			echo /dev/scsi/host${SCSIHOST}/bus0/target0/lun0/disc
			echo usb device number ${USBDEVNO}

			#
			# Specify the phsycal location in the grep.
			#
			grep -e "T:[ \\t]*Bus=01[ \\t]*Lev=01[ \\t]*Prnt=01[ \\t]*Port=00[ \\t]*Cnt=[0-9][0-9] Dev#=[ \\t]*${USBDEVNO}" /proc/bus/usb/devices > /dev/null
			if [ "$?" = "0" ]
			then
				echo found internal USB disk on SCSI host ${SCSIHOST}
				RET="0"
				break
			fi
		fi
	fi
done


#
# Start NDAS emulator
#

if [ ${RET} = "0" ]
then
	NDASEMU_DEVFILE=/dev/scsi/host${SCSIHOST}/bus0/target0/lun0/disc
	echo NDAS emulator is using ${NDASEMU_DEVFILE}
	insmod /tmp/ndas_emu_s.o dev=${NDASEMU_DEVFILE}
fi

