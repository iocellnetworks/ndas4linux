#!/bin/sh
cat <<EOF

<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
    <meta http-equiv="content-type" content="application/xhtml+xml; charset=UTF-8" />
    <link rel="stylesheet" href="styles.css" type="text/css">
</head>
<body>
<img src="http://www.ximeta.com/img/logo.gif"><br><br>
<table border=1>
EOF
eval `echo $QUERY_STRING | sed -e 's|&|\n|g' | grep ^slot=`
cat /proc/mounts > /www/cgi-bin/n
A_Z=abcdefghijklmnopqstuvwxyz
DEVICE_FILE=/dev/nd`echo $A_Z | cut -c$slot-$slot` 
MOUNTED=`df -k | grep ^$DEVICE_FILE | sed -e 's|^.* ||'`
export slot;
fdisk -l $DEVICE_FILE| grep ^$DEVICE_FILE | awk \
'BEGIN {
        print "<tr><td>block</td><td>type</td><td>mounting path</td><td>action</td></tr>\n"
}{
	print "<tr>";
	printf "<td>%s</td>", $1;
	printf "<td>%s %s</td>", $6,$7;
	"df -k | grep ^"$1" | sed -e \"s|^.* ||\" -e \"s|/mnt/ndas/||\"" | getline TT
	if ( TT == "" ) {
	    print "<form method=GET action=mount.cgi>\n";
	    print "<td>"
	    print "<input name=mount_path>\n";
	    print "</td>"
	    print "<td>"
	    printf "<input name=mount_slot value=\"1\" type=hidden>\n";
	    printf "<input name=mount_devi value=\"%s\" type=hidden>\n",$1;
	    printf "<input value=Mount type=submit>\n",$1;
	    print "</td>"
	    print "</form>\n";
	} else { 
	    printf "<td><a href=\"/cgi-bin/file.php?path=/%s\">/%s</a></td>", TT, TT;
	    print "<td>"
	    print "<form method=GET action=unmount.cgi>\n";
	    printf "<input name=umount_slot value=\"1\" type=hidden>\n";
	    printf "<input name=umount_devi value='%s' type=hidden>\n",$1;
	    printf "<input name=umount_path value='%s' type=hidden>\n",TT;
	    print "<input value=Unmount type=submit>\n";
	    print "</form>"
	    print "</td>"
	}
	print "</tr>\n"
}'
cat <<EOF
</table>
<form method=GET action=/cgi-bin/list.php>
<input value="Back" type=submit>
</form>
Please note you can not add/delete/modify the partition, or format the partition from this router.<br>
You should do that from the Desktop PC or other devices.<br>
As those tools can't not be included into the limited flash memory of the router.<br>
<br>
<a href="/cgi-bin/logout.php">Logout</a> 
</body>
</html>
EOF
