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
$adduserid = $_POST['userid'];
$adduserpw = $_POST['userpw'];

if(empty($adduserid) || empty($adduserpw))
{
	echo "<form action=\"adduser.php\" method=POST>";
	echo "<table>
		<td>User ID:</td><td><input type=text name=userid value=$adduserid></td>
		</tr>
		<tr>
		<td>Password:</td><td><input type=text name=userpw value=$adduserpw></td>
		</tr>
		</table>
		<input type=submit value=\"Add\">";
	if(empty($adduserid) && !empty($adduserpw))
	{
		echo "<p>Please type userid</p>";
	}
	else if(!empty($adduserid) && empty($adduserpw))
	{
		echo "<p>Please type password</p>";
	}
}
else
{
	//echo "<p>adduserid: ".$adduserid." adduserpw: ".$adduserpw."</p>";
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
	while(!feof($fp))
	{
		if(fscanf($fp,"%s\t%s\n",$userid,$userpw) && $userid == $adduserid)
		{
			//echo "<p>userid: ".$userid."</p>";
			$find = true;
			break;
		}
	}
	if($find == false)
	{
		// create a new user
		fseek($fp,0,SEEK_END);
		$newuser = sprintf("%s\t%s",$adduserid,$adduserpw);
		//echo "<p>newuser: ".$newuser."</p>";
		fwrite($fp,$newuser);
		//fwrite($fp,"user3	pass3");
		//fwrite($fp,"%s\t\%s",$adduserid,$adduserpw);
	}
	flock($fp,LOCK_UN);
	fclose($fp);
	if($find)
	{
		echo "<h3>User ID '".$adduserid."' is already exist.</h3>";
	}
	else
	{
		echo"<script>
			opener.location.href='user_manage.php'
			//self.close();
			</script>";
		echo "<p>User ID '".$adduserid."' is successfully created.</p>";
	}
}
?>
