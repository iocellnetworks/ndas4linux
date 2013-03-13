#!/bin/sh /usr/bin/php-wrapper
<?php
session_start();
$nextdir = $_SESSION['CDIR'];
if(isset($nextdir) && !empty($nextdir))
{
	echo "<h2>Folder: ".$nextdir."</h2>";
	$dirkeys = unserialize(urldecode($_SESSION['DIRNAME'])); 
	$dirvals = unserialize(urldecode($_SESSION['DIRPASS'])); 
	$index = array_search($nextdir,$dirkeys);
	$pass2 = $dirvals[$index];
	$pass = $_POST['pass'];
	//
	//if($pass && !empty($pass))
	if(!empty($pass) && $pass == $pass2)
	{
		//echo "<p> pass: ".$pass."</p>";
		//echo "<p> pass2: ".$pass2."</p>";
		//if($pass == $pass2)
		{
			$_SESSION['CDIR'] = '';
			$_SESSION['AUTHDIR'] = $nextdir;
				//parent.location.href='/cgi-bin/file.php?path=$nextdir'
			echo"<script>
				opener.parent.location.href='/cgi-bin/file.php?path=$nextdir'
				self.close();
				</script>";
		}
		//else
		//{
		//	 echo"<script>
		//		  alert('incorrect password')
		//			</script>";
		//	  exit; //비밀번호가 틀리면 실행을 모두 멈춤
		//}
	}
	else
	{
		if(!empty($pass) && $pass != $pass2)
		{
			echo"<script>
				alert('incorrect password')
				</script>";
		}
		echo"<form action=/cgi-bin/dirpass.php method=POST>
		Password: <input type=password name=pass>
		<input type=submit value=OK>
		</form>";
	}
}
else
{
	echo "<p>Access Denied.</p>";
}
?>
