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
$moduserid = $_POST['moduserid'];
$moduserpw = $_POST['moduserpw'];
$modify = $_POST['modify'];
echo "<title>Modify User</title>";
//echo "moduserid: ".$moduserid.", moduserpw: ".$moduserpw;
if(!empty($moduserid) && !empty($moduserpw))
{
	if($modify == 'OK')
	{
		// modify user info
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
		$index = array_search($moduserid,$arruserid);
		if($index >= 0)
		{
			$arruserpw[$index] = $moduserpw;
			// write to netdisk.user file
			rewind($fp);
			ftruncate($fp,0);
			$usercount = count($arruserid);
			for($i=0; $i<$usercount;$i++)
			{
				$line = sprintf("%s\t%s\n",$arruserid[$i],$arruserpw[$i]);
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
			echo "<h3>User ID '".$moduserid."' is not exist.</h3>";
		}
		else
		{
			echo"<script>
				opener.location.href='user_manage.php'
				//self.close();
				</script>";
			echo "<p>User ID '".$moduserid."' is successfully modified.</p>";
		}
	}
	else
	{
		echo "<form action=\"moduser.php\" method=POST>";
		echo "<table>
			<td>User ID:</td><td>$moduserid</td>
			</tr>
			<tr>
			<td>Password:</td><td><input type=text name=moduserpw value=$moduserpw></td>
			</tr>
			</table>
			<br>
			<input type=hidden name=moduserid value=$moduserid>
			<input type=hidden name=modify value=OK>
			<input type=submit value=\"Modify\">";
	}
}
else
{
	echo "<h3>Abnormal Acess</h3>";
}
?>
