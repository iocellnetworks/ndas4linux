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
$modpdir = $_POST['mpdir'];
$modpdirpw = $_POST['mpdirpw'];
$modify = $_POST['modify'];
echo "<title>Modify Folder Password</title>";
if(!empty($modpdir) && !empty($modpdirpw))
{
	if($modify == 'OK')
	{
		// modify user info
		$ufile = "netdisk.dir";
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
		$arrpdir = array();
		$arrpdirpw = array();
		while(!feof($fp))
		{
			if(fscanf($fp,"%s\t%s\n",$pdir,$pdirpw))
			{
				array_push($arrpdir,$pdir);
				array_push($arrpdirpw,$pdirpw);
			}
		}
		$index = array_search($modpdir,$arrpdir);
		if($index >= 0)
		{
			$arrpdirpw[$index] = $modpdirpw;
			// write to netdisk.user file
			rewind($fp);
			ftruncate($fp,0);
			$usercount = count($arrpdir);
			for($i=0; $i<$usercount;$i++)
			{
				$line = sprintf("%s\t%s\n",$arrpdir[$i],$arrpdirpw[$i]);
				//echo "<p>line: ".$line."</p>";
				fwrite($fp,$line);
			}
		}
		//
		flock($fp,LOCK_UN);
		fclose($fp);
		//
		if($index < 0)
		{
			echo "<h3>Folder '".$modpdir."' is not registered.</h3>";
		}
		else
		{
			echo"<script>
				opener.location.reload();
				//self.close();
				</script>";
			echo "<p>Folder '".$modpdir."' is successfully modified.</p>";
		}
	}
	else
	{
		echo "<form action=\"modpdir.php\" method=POST>";
		echo "<table>
			<td>Folder:</td><td><input type=text name=mpdir value=$modpdir></td>
			</tr>
			<tr>
			<td>Password:</td><td><input type=text name=mpdirpw value=$modpdirpw></td>
			</tr>
			</table>
			<br>
			<input type=hidden name=modify value=OK>
			<input type=submit value=\"Modify\">";
	}
}
else
{
	echo "<h3>Abnormal Acess</h3>";
}
?>
