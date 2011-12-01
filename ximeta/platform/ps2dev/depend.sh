#!/bin/sh
OUTDIR=`echo $1 | sed -e 's@/$@@'`
shift 1
case "$OUTDIR" in
"" | ".")
$CC -MM -MG "$@" |
sed -e 's@^\(.*\)\.o:@\1.d \1.o:@'
;;
*)
$CC -MM -MG "$@" |
sed -e "s@^\(.*\)\.o:@$OUTDIR/\1.o:@" 
echo "    ${CC} $@ -o \$@ -c "
$CC -MM -MG "$@" |
sed -e "s@^\(.*\)\.o:@$OUTDIR/\1.d:@" 
;;
esac
