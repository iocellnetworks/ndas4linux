#!/bin/sh
cat <<EOF

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
    <meta http-equiv="content-type" content="application/xhtml+xml; charset=UTF-8" />
    <link rel="stylesheet" href="styles.css" type="text/css">
</head>
<body>
<img src="http://www.ximeta.com/img/logo.gif"><br><br>
EOF
eval `echo $QUERY_STRING | sed -e 's|&|\n|g' | grep ^slot=`
A_Z=abcdefghijklmnopqstuvwxyz
DEVICE_FILE=/dev/nd`echo $A_Z | cut -c$slot-$slot` 
LINES=`df -k | grep ^$DEVICE_FILE | cut -d' ' -f1`
FAIL=0
for s in $LINES;
do
    echo "Unmounting $s"
    RESULT=`umount -f $s 2>&1`
    if [ $? -eq 0 ]; then
        echo "...done<br>"
    else
    	echo "...failed: $RESULT<br>"
    	FAIL=1
    fi
done
if [ $FAIL -eq 0 ] ; then
    echo "Disabling slot \"$slot\"<br>"
    RESULT=`/usr/sbin/ndasadmin disable -s $slot" 2>&1`
    if [ $? -eq 0 ] ; then
        echo "<div class=title>Successfully disabled</div>"
    else
        echo "<div class=title>$RESULT</div>"
    fi
fi
cat <<EOF
<form method=GET action=/cgi-bin/list.php>
<input value="Back" type=submit>
</form>
</body>
</html>
EOF
