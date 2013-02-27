#!/bin/sh /usr/bin/php-wrapper
<?php
include ('auth.php');
?>
<html>
<head>
</head>
<script language="javascript">
function addUser()
{
	window.open("adduser.php","_adduser","width=300,height=300");
	//window.open("popup.html","_adduser","width=300,height=300");
}
function modUser()
{
	//window.open("moduser.php","_moduser","width=300,height=300");
	window.open("","_moduser","width=300,height=300");
	//window.open("","_moduser");
	//window.open("popup.html","_adduser","width=300,height=300");
}
function delUser()
{
	window.open("","_deluser","width=300,height=300");
}
</script>
<?php
//$id = array('user1', 'user2', 'user3');
//$pw = array('pass1', 'pass2', 'pass3');
// open user info file
//$ufile = "/cgi-bin/netdisk.user";
$ufile = "netdisk.user";
if(!file_exists($ufile))
{
	echo "<h3>user information file is not exist: ".$ufile."</h3>";
	exit;
}
$fp = fopen($ufile,"r");
if(!$fp)
{
	echo "<h3>Cannot open user information file: ".$ufile."</h3>";
	exit;
}
echo "<h1>Registerd User List</h3>";
echo "<table border=1>";
echo "<tr><td>ID</td><td>Password</td><td>Delete</td><td>Modify</td>";
while(!feof($fp))
{
	if(fscanf($fp,"%s\t%s\n",$userid,$userpw))
	{
		//echo "<p>userid: ".$userid." userpw: ".$userpw."</p>";
		echo "<tr><td>".$userid."</td>";
		echo "<td>".$userpw."</td>";
		if($userid != 'admin')
		{
			/*
			echo "<td><form method=POST action=deluser.php>";
			echo "<input type=hidden name=deluserid value=\"$userid\">";
			echo "<input type=submit value=Delete class=btn></form></td>";
			*/
			echo "<td><form target=_deluser method=POST action=deluser.php 
				onSubmit=\"javascript:delUser()\">";
			echo "<input type=hidden name=deluserid value=\"$userid\">";
			echo "<input type=submit value=Delete class=btn></form></td>";
			//
			echo "<td><form target=_moduser method=POST action=moduser.php 
				onSubmit=\"javascript:modUser()\">";
			echo "<input type=hidden name=moduserid value=\"$userid\">";
			echo "<input type=hidden name=moduserpw value=\"$userpw\">";
			echo "<input type=submit value=Modify class=btn></form></td>";
			//echo "<td><a href=\"javascript:modUser()\">Modify</a></td>";
		}
	}
}
fclose($fp);
echo "</table>";
echo "<br>";
echo "<a href=\"javascript:addUser()\">Add User</a>";
echo "<br>";
echo "<a href=\"/cgi-bin/list.php\">Home</a>";
?>
</html>
