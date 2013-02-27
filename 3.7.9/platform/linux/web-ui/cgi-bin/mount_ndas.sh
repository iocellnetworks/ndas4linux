#!/bin/sh
equal() {
        case "$1" in
                "$2") return 0 ;;
                *) return 255 ;;
        esac
}
OPT="-o rw,async,noatime"
#for s in a b c d e f g h i j k
#do
#if equal "$1" "/dev/nd${s}1";then
#	OPT="-o ro,async,noatime"
#fi
#done
mount $OPT -t vfat $* > /tmp/l
