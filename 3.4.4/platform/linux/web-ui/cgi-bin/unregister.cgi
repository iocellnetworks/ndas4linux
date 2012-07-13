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
eval `echo $QUERY_STRING | sed -e 's|&|\n|g' | grep ^name=`
echo "Unregistering \"$name\" <br>" 
echo "<div class=title>"
/usr/sbin/ndasadmin unregister --name "$name" 2>&1
cat <<EOF
</div>
<form method=GET action=/cgi-bin/list.php>
<input value="Back" type=submit>
</form>
</body>
</html>
EOF
