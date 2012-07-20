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
$delpdir = $_POST['delpdir'];
echo "<title>Unregister Password Folder</title>";
if(!empty($delpdir))
{
	// delete password folder
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
	$arrdir = array();
	$arrpass = array();
	while(!feof($fp))
	{
		if(fscanf($fp,"%s\t%s\n",$pdir,$pdirpw))
		{
			array_push($arrdir,$pdir);
			array_push($arrpass,$pdirpw);
		}
	}
	$index = array_search($delpdir,$arrdir);
	if($index >= 0)
	{
		$arrpass[$index] = $moduserpw;
		// write to netdisk.dir file
		rewind($fp);
		ftruncate($fp,0);
		$dircount = count($arrdir);
		for($i=0; $i<$dircount;$i++)
		{
			if($arrdir[$i] != $delpdir)
			{
				$line = sprintf("%s\t%s\n",$arrdir[$i],$arrpass[$i]);
				fwrite($fp,$line);
			}
		}
	}
	flock($fp,LOCK_UN);
	fclose($fp);
	//
	if($index < 0)
	{
		echo "<h3>Folder '".$delpdir."' is already unregistered.</h3>";
	}
	else
	{
		echo"<script>
			opener.location.href='folder_manage.php'
			//self.close();
			</script>";
		echo "<p>Folder '".$delpdir."' is successfully unregistered.</p>";
	}
}
else
{
	echo "<h3>Abnormal Acess</h3>";
}
echo "<a href=\"javascript:self.close()\">Close</a>";
?>
