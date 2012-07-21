#!/bin/sh /usr/bin/php-wrapper
<?php
include ('auth.php');
?>
<html>
<title>Folder Management</title>
<head>
</head>
<script language="javascript">
function addPDir()
{
	window.open("addpdir.php","_addpdir","width=300,height=300");
	//window.open("popup.html","_adduser","width=300,height=300");
}
function modPDir()
{
	//window.open("moduser.php","_moduser","width=300,height=300");
	window.open("","_modpdir","width=300,height=300");
	//window.open("","_moduser");
	//window.open("popup.html","_adduser","width=300,height=300");
}
function delPDir()
{
	window.open("","_delpdir","width=300,height=300");
}
</script>
<?php
$ufile = "netdisk.dir";
if(!file_exists($ufile))
{
	echo "<h3>folder information file is not exist: ".$ufile."</h3>";
	exit;
}
$fp = fopen($ufile,"r");
if(!$fp)
{
	echo "<h3>Cannot open folder information file: ".$ufile."</h3>";
	exit;
}
echo "<h1>Password Folder List</h3>";
echo "<table border=1>";
echo "<tr><td>Folder</td><td>Password</td><td>Actions</td>";
while(!feof($fp))
{
	if(fscanf($fp,"%s\t%s\n",$folder,$fpass))
	{
		echo "<tr><td>".$folder."</td>";
		echo "<td>".$fpass."</td>";
			echo "<td><form target=_delpdir method=POST action=delpdir.php 
				onSubmit=\"javascript:delPDir()\">";
			echo "<input type=hidden name=delpdir value=\"$folder\">";
			echo "<input type=submit value=Unregister class=btn></form>";
			//
			echo "<form target=_modpdir method=POST action=modpdir.php 
				onSubmit=\"javascript:modPDir()\">";
			echo "<input type=hidden name=mpdir value=\"$folder\">";
			echo "<input type=hidden name=mpdirpw value=\"$fpass\">";
			echo "<input type=submit value=Modify class=btn></form></td>";
	}
}
fclose($fp);
echo "</table>";
echo "<br>";
echo "<a href=\"javascript:addPDir()\">Add Folder</a>";
echo "<br>";
echo "<a href=\"/cgi-bin/list.php\">Home</a>";
?>
</html>
