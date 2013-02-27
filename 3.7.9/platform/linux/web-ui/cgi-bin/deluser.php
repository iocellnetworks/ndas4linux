#!/bin/sh /usr/bin/php-wrapper
<?php
session_start();
// check admin
$curuserid = $_SESSION['USERID'];
if(!isset($curuserid) || empty($curuserid) || $curuserid !="admin")
{
	echo "<h3>You don't have the right permission.</h3>";
	exit;
}
$deluserid = $_POST['deluserid'];
echo "<title>Delete User</title>";
if(!empty($deluserid))
{
	// delete user info
	$ufile = "netdisk.user";
	if(!file_exists($ufile))
	{
		echo "<h3>user information file is not exist: ".$ufile."</h3>";
		exit;
	}
	$fp = fopen($ufile,"r+");
	if(!$fp)
	{
		echo "<h3>Cannot open user information file: ".$ufile."</h3>";
		exit;
	}
	if(!flock($fp, LOCK_EX))
	{
		echo "<h3>Other connection is using the user information file</h3>";
		exit;
	}
	$find = false;
	$arruserid = array();
	$arruserpw = array();
	while(!feof($fp))
	{
		if(fscanf($fp,"%s\t%s\n",$userid,$userpw))
		{
			array_push($arruserid,$userid);
			array_push($arruserpw,$userpw);
			//echo "<p>userid: ".$userid."</p>";
			//$finduser = true;
		}
	}
	$index = array_search($deluserid,$arruserid);
	if($index >= 0)
	{
		$arruserpw[$index] = $moduserpw;
		// write to netdisk.user file
		rewind($fp);
		ftruncate($fp,0);
		$usercount = count($arruserid);
		for($i=0; $i<$usercount;$i++)
		{
			if($arruserid[$i] != $deluserid)
			{
				$line = sprintf("%s\t%s\n",$arruserid[$i],$arruserpw[$i]);
				fwrite($fp,$line);
			}
		}
	}
	flock($fp,LOCK_UN);
	fclose($fp);
	//
	if($index < 0)
	{
		echo "<h3>User ID '".$deluserid."' is not exist.</h3>";
	}
	else
	{
		echo"<script>
			opener.location.href='user_manage.php'
			//self.close();
			</script>";
		echo "<p>User ID '".$deluserid."' is successfully deleted.</p>";
	}
}
else
{
	echo "<h3>Abnormal Acess</h3>";
}
echo "<a href=\"javascript:self.close()\">Close</a>";
?>
