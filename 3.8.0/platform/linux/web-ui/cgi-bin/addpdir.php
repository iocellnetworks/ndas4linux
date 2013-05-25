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
$ndirpath= $_POST['dirpath'];
$ndirpw= $_POST['dirpw'];

if(empty($dirpath) || empty($dirpw))
{
	echo "<form action=\"addpdir.php\" method=POST>";
	echo "<table>
		<td>Folder:</td><td><input type=text name=dirpath value=$ndirpath></td>
		</tr>
		<tr>
		<td>Password:</td><td><input type=text name=dirpw value=$ndirpw></td>
		</tr>
		</table>
		<input type=submit value=\"Add\">";
	if(empty($dirpath) && !empty($dirpw))
	{
		echo "<p>Please type folder path</p>";
	}
	else if(!empty($dirpath) && empty($dirpw))
	{
		echo "<p>Please type password</p>";
	}
}
else
{
	//echo "<p>dirpath: ".$dirpath." dirpw: ".$dirpw."</p>";
	$ufile = "netdisk.dir";
	if(!file_exists($ufile))
	{
		echo "<h3>folder information file is not exist: ".$ufile."</h3>";
		exit;
	}
	$fp = fopen($ufile,"r+");
	if(!$fp)
	{
		echo "<h3>Cannot open folder information file: ".$ufile."</h3>";
		exit;
	}
	if(!flock($fp, LOCK_EX))
	{
		echo "<h3>Other connection is using the folder information file</h3>";
		exit;
	}
	$find = false;
	while(!feof($fp))
	{
		if(fscanf($fp,"%s\t%s\n",$dir1,$pw1) && $dir1 == $ndirpath)
		{
			//echo "<p>dir: ".$dir1."</p>";
			$find = true;
			break;
		}
	}
	if($find == false)
	{
		// create a new folder info
		fseek($fp,0,SEEK_END);
		$newfolder = sprintf("%s\t%s",$ndirpath,$ndirpw);
		fwrite($fp,$newfolder);
	}
	flock($fp,LOCK_UN);
	fclose($fp);
	if($find)
	{
		echo "<h3>Folder '".$ndirpath."' is already registered.</h3>";
	}
	else
	{
		echo"<script>
			opener.location.reload();
			//self.close();
			</script>";
		echo "<p>Folder '".$ndirpath."' is successfully registered.</p>";
	}
}
?>
