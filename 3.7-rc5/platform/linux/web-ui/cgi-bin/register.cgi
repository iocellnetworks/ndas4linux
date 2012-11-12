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
eval `echo $QUERY_STRING | sed -e 's|&|\n|g' | grep ^id[1-6]=`
echo "Registering \"$id6\" with \"$id1-$id2-$id3-$id4-$id5\" <br>"
RESULT=`/usr/sbin/ndasadmin register $id1-$id2-$id3-$id4-$id5 --name "$id6" 2>&1`
if [ $? -eq 0 ] ; then
	echo "<div class=title>$RESULT</div>"
else
	echo "<div class=title>Successfully registered</div>"
fi
cat <<EOF
<form method=GET action=/cgi-bin/list.php>
<input value="Back" type=submit>
</form>
</body>
</html>
EOF
