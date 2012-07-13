#!/bin/sh /usr/bin/php-wrapper
<?php
session_start();
// check admin
$curuserid = $_SESSION['USERID'];
$path = urldecode($_POST['path']);
if(!isset($curuserid) || empty($curuserid) || empty($path))
{
	echo "<h3>You don't have the right permission.</h3>";
	exit;
}
?>
<html>
<head>
<title>Create Directory</title>
</head>
<body>
<?php
$uploaddir = '/mnt/ndas/' . $path. '/' ;

$newdir = $_POST['dir'];

if(empty($newdir))
{
	echo "Current Path: ".$path;
	echo "<form action=\"create_dir.php\" method=POST>";
	echo "<label for=file>Directory: </label>";
	echo "<input type=hidden name=path value=".urlencode($path).">";
	echo "<input type=text name=dir value=$newdir>";
	echo "<input type=submit value=\"Create\">";
}
else
{
	$newpath = $uploaddir . $newdir;
	//echo "<p>adduserid: ".$adduserid." adduserpw: ".$adduserpw."</p>";
	$ret = mkdir($newpath,0755);
	if($ret == TRUE)
	{
		echo "<h3>Directory '".$newdir."' is successfully created.</h3>";
	}
	else
	{
		echo "Failed to create directory: ".$newdir;
	}
	echo "<input type=button onclick=\"javascript:self.close()\" value=Close>";
}
?>
</body>
</html>
