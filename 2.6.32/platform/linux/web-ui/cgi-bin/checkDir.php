#!/bin/sh /usr/bin/php-wrapper
<?php                                                                           
session_start();                                                                

//echo "<p> get path param: ".$_GET['path'];
//echo "<p> post path param: ".$_POST['path'];
//$dirkeys = unserialize(urldecode($_SESSION['DIRNAME'])); 
//$dirvals = unserialize(urldecode($_SESSION['DIRPASS'])); 
//print_r($dirkeys);
//print_r($dirvals);
//echo "<p> DIRNAME: ".$_SESSION['DIRNAME'];

function checkDirPass($path)
{
	// if user is admin, always return false to skip
	if($_SESSION['USERID'] == 'admin')
	{
		return false;
	}
	//echo "<p>checkDirPass: ".$path."</p>";
	if(empty($path))
	{
		echo "<p> path is emtpy </p>";
		exit;
	}
	// remove // -> /
	/*
	while(strstr($path,"//"))
	{
		str_replace("//", "/", $path);
	}
	*/
	if($path == $_SESSION['AUTHDIR'])
	{
		return false;
	}
	$dirkeys = unserialize(urldecode($_SESSION['DIRNAME'])); 
	//print_r($dirkeys);
//	return in_array($path,$dirkeys);
	if(in_array($path,$dirkeys))
	{
		//echo "true";
		$_SESSION['CDIR'] = $path;
		return true;
	}
	else
	{
		//echo "false";
		return false;
	}
}

$dirPath=urldecode($_GET['path']);
if ( !isset($_GET['path'])) {
    $dirPath=urldecode($_POST['path']);
}
// compare
if( checkDirPass($dirPath) )
//if( $dirPath != "/home")
{
	echo "<script>";
	echo "window.open('/cgi-bin/dirpass.php','dirpass','width=500, height=100')";
	echo "</script>";
}
else
{
	echo "<script>";
	echo "parent.location.href='file.php?path=$dirPath'";
	echo "</script>";
}
?>
