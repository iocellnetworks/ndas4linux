export XPLAT_SRC=../..
cp $XPLAT_SRC/netdisk/*.c $XPLAT_SRC/lpx/*.c $XPLAT_SRC/xlib/*.c $XPLAT_SRC/platform/linuxum/sal/*.c .
/usr/local/bin/cg_annotate --$1 --auto=yes --show=I1mr,D1mr,D1mw *.c > cm.txt
unix2dos cm.txt
