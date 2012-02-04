<?php                                                                           
//$users = array('ximeta' => 'a', 'guest' => 'bbb');
function AuthUser($iuserid,$iuserpw)
{
	$ufile = "netdisk.user";
	if(!file_exists($ufile))
	{
		echo "<h3>user information file is not exist: ".$ufile."</h3>";
		exit;
	}
	$fp = fopen($ufile,"r");
	if(!$fp)
	{
		echo "<h3>Cannot open user information file: ".$ufile."</h3>";
		exit;
	}
	//$arruserid = array();
	//$arruserpw = array();
	while(!feof($fp))
	{
		if(fscanf($fp,"%s\t%s\n",$userid,$userpw))
		{
			if($userid == $iuserid)
			{
				if($userpw == $iuserpw)
				{
					return true;
				}
				break;
			}
			//array_push($arruserid,$userid);
			//array_push($arruserpw,$userpw);
		}
	}
	fclose($fp);
	return false;
}
//
function readDirFile()
{
	$conf = "netdisk.dir";
	if(!file_exists($conf))
		return;
	$fp = fopen($conf,"r");
	if(!$fp)
		return;
	$dirkeys = array();
	$dirvals = array();
	for($i=1; !feof($fp); $i++)
	{
	if(fscanf($fp,"%s\t%s\n",$dir,$pass))
	{
	array_push($dirkeys,$dir);
	array_push($dirvals,$pass);
	}
	}
	fclose($fp);
	$_SESSION['DIRNAME'] = urlencode(serialize($dirkeys));
	$_SESSION['DIRPASS'] = urlencode(serialize($dirvals));
}
//
session_start();                                                                
if ( $_SESSION['AUTH'] != "TRUE")
{                                             
    $userid = $_POST['userid'];
    $password = $_POST['password'];
	/*
    if ( isset($userid) )
	{
         if ($users[$userid] == $password) {
                $_SESSION['userid'] = $userid;
                $_SESSION['password'] = $password;
         	$_SESSION['AUTH'] = "TRUE";
         } else {
         	echo "<h2>Fail to login</h2><br>";
         }
    }     
	*/
	if(!empty($userid))
	{
		//$index = array_search($userid,$arruserid);
		//if($index >=0 && $arruserpw[$index] == $password)
		if(AuthUser($userid,$password))
		{
			$_SESSION['AUTH'] = "TRUE";
			$_SESSION['USERID'] = $userid;
			readDirFile();
		}
		else
		{
			echo "<h2>Invalid user id or password</h2><br>";
		}
	}
	
    if( $_SESSION['AUTH'] != "TRUE")
	{                                             
?>                                                                              
    <img src="/img/logo.gif">
    <h3>IOCELL Networks Personal Web Hard</h3>
    <form action="<?php echo $_SERVER["SCRIPT_NAME"]; ?>" method=POST>                                  
    <table>
     <tr>
        <td>User ID:</td><td><input type=text name=userid></td>
     </tr>
     <tr>
        <td>Password:</td><td><input type=text name=password></td>                                
     </tr>
     </table>
     <input type=submit value="Login">                                           
<?php
	//if ( isset($_GET['path']) ) {
     //       echo "<input type=hidden name=path value=\"".$_GET['path']."\">";
	//}
	if ( isset($_POST['path']) ) {
            echo "<input type=hidden name=path value=\"".$_POST['path']."\">";
	}
?>
    </form>                                                                         
<?php                                                                           
        exit;                                                                   
    }
}                                                                               
?>        
