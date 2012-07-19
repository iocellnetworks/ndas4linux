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
if [ "$slot" -eq "" ] ;then
    echo "<div class=title>No slot number is assigned</div>"
else
    echo "Enabling slot \"$slot\" to read and write<br>" 
    echo "<div class=title>"
    /usr/sbin/ndasadmin enable -s $slot -o s 2>&1 | head -1
fi
cat <<EOF
</div>
<form method=GET action=/cgi-bin/list.php>
<input value="Back" type=submit>
</form>
</body>
</html>
EOF
