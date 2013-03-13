#!/bin/sh
cat <<EOF

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en-AU">
<head>
    <meta http-equiv="content-type" content="application/xhtml+xml; charset=UTF-8" />
    <link rel="stylesheet" href="styles.css" type="text/css">
</head>
<body>
<img src="http://www.ximeta.com/img/logo.gif"><br><br>
EOF
eval `echo $QUERY_STRING | sed -e 's|%2F|/|g' -e 's|&|\n|g' | grep ^mount_`

echo "Mounting \"$mount_devi\" to \"$mount_path\" <br>"
RESULT=`/www/cgi-bin/mkdir_ndas.sh -p /mnt/ndas/$mount_path; /www/cgi-bin/mount_ndas.sh $mount_devi /mnt/ndas/$mount_path" 2>&1`
if [ $? -eq 0 ] ; then
	echo "<div class=title>Successfully mounted</div>"
else
	echo "<div class=title>$RESULT</div>"
fi
echo "<form method=GET action=/cgi-bin/manage.php>"
echo "<input name=slot value=\"$mount_slot\" type=hidden>"
cat <<EOF
<input value="Back" type=submit>
</form>
</body>
</html>
EOF
