#!/bin/sh /usr/bin/php-wrapper
<?php
include('header.php');
?>
<?php
$admin = false;
if($_SESSION['USERID'] == 'admin')
{
//
echo "<table width=100%>";
echo "<tr><td align=left>";
echo "<h1>WAN IP</h1>";
$wanif=exec("nvram get wan_ifnames");
$ip= exec("ifconfig $wanif | head -2 | tail -1 | grep inet | cut -c21-35");
if ( $ip ) 
	echo $ip;
else
	echo "No IP Address. check the wan cable";
echo "</td></tr>
<tr><td>";
echo "<h1>List of the NDAS devices registered</h1>
<table border=1>";
if ( !is_file('/proc/ndas/devs') ) {
    die;
}
$ndas_devs = fopen("/proc/ndas/devs","r");
$count = 0;
if ( !$ndas_devs ) {
    die;
}
echo "<tr><td>Name</td><td>ID</td><td>Key</td>";
echo "<td>Slots</td><td>Status</td><td>Actions</td></tr>";
while ( $line = fgets($ndas_devs, 1024) ) {
    if ( $count == 0 ) {
       $count++;
       continue;
    }
    $name = strtok($line," \t\n");
    echo '<tr><td>'.$name.'</td>'; 
    $tok = strtok(" \t\n");
    echo '<td>'.$tok.'</td>'; 
    $tok = strtok(" \t\n");
    echo '<td>'.$tok.'</td>'; 
    $tok = strtok(" \t\n");
    $tok = strtok(" \t\n");
    $tok = strtok(" \t\n");
    $slot = strtok(" \t\n");
    
    if ( $slot == "" ) $slot = 'N/A';
    echo '<td>'.$slot."</td>"; 
    $status_file = "/proc/ndas/slots/".$slot."/info";
    if ( is_file($status_file) && $handle = fopen($status_file, "r") ) {
        fgets($handle);
        $line = fgets($handle);
        $status = strtok($line, " \t\n");
        fclose($handle);
    } else {
        $status = "N/A";
    }
    echo "<td>".$status."</td>";
    if ( $status == "Enabled") {
       echo "<td>";
       echo "<form action=/cgi-bin/disable.cgi method=get>";
       echo "<input value=Disable type=submit>";
       echo "<input name=slot value=\"".$slot."\" type=hidden>";
       echo "</form>";                                                      
       echo "<form action=/cgi-bin/manage.php method=get>";                 
       echo "<input value=\"Manage partitions\" type=submit>"; 
       echo "<input name=slot value=\"".$slot."\" type=hidden>";
       echo "</form>"; 
       echo "</td>";
    } else {
      echo "<td>";
      if ( $status == "Disabled" ) {
          echo "<form name=enable action=/cgi-bin/enable_share.cgi>";
          echo "<input value=Enable type=submit>";                   
          echo "<input name=slot value=\"".$slot."\" type=hidden>";   
          echo "</form>";                                          
      }
      echo "<form name=disable action=/cgi-bin/unregister.cgi method=GET>";
      echo "<input value=Unregister type=submit>";               
      echo "<input name=name value=\"".$name."\" type=hidden>";   
      echo "</form>"; 
      echo "</td></tr>";
    }
    $count++;
}
fclose($ndas_devs);
echo "</table>
</td></tr>
<tr><td>
<h1>Register a new NDAS device</h1>
<form name=register action=/cgi-bin/register.cgi method=GET>";
echo "<input name=register-button value=\"Register\" type=submit>";
echo "ID:
<input name=id1 id=id1 maxlength=5 size=5 type=text style=\"border: 1px solid rgb(112, 112, 112); width: 50px; height: 20px; background-color: rgb(255, 255, 255);\">-
<input name=id2 id=id2 maxlength=5 size=5 type=text style=\"border: 1px solid rgb(112, 112, 112); width: 50px; height: 20px; background-color: rgb(255, 255, 255);\">-
<input name=id3 id=id3 maxlength=5 size=5 type=text style=\"border: 1px solid rgb(112, 112, 112); width: 50px; height: 20px; background-color: rgb(255, 255, 255);\">-
<input name=id4 id=id4 maxlength=5 size=5 type=text style=\"border: 1px solid rgb(112, 112, 112); width: 50px; height: 20px; background-color: rgb(255, 255, 255);\"> KEY:
<input name=id5 id=key maxlength=5 size=5 type=text style=\"border: 1px solid rgb(112, 112, 112); width: 50px; height: 20px; background-color: rgb(255, 255, 255);\">";
echo "NAME:
<input name=id6 id=name type=text style=\"border: 1px solid rgb(112, 112, 112); width: 100px; height: 20px; background-color: rgb(255, 255, 255);\">";
echo "</form>
</td></tr>
<tr><td>
<h1>Mounted directories</h1>
<table border=1>
<tr><td>block</td><td>mounted path</td></tr>";
      $handle = fopen("/proc/mounts", "r"); 
      while ( $line = fgets($handle, 1024) ) {
        if ( strpos($line, "/dev/nd") === false ) continue;
        $tok = strtok($line, " \t\n");
        echo "<tr><td>".$tok."</td>";
        $tok = substr(strtok(" \t\n"),9);
        echo "<td>";
        echo "<a href=\"/cgi-bin/file.php?path=".$tok."\">".$tok."</a>";
        echo "</td></tr>";
      }
      echo "</td>\n";
      fclose($handle);
echo "</table>
</td></tr>
</table>
<br>
<a href=admin.php>Admin</a>";
}
else
{
	// user
	echo "<h1>Mounted directories</h1>";
	echo "<table border=1>
	<tr><td>Disk</td></tr>";
		  $handle = fopen("/proc/mounts", "r"); 
		  while ( $line = fgets($handle, 1024) ) {
			if ( strpos($line, "/dev/nd") === false ) continue;
			$tok = strtok($line, " \t\n");
			//echo "<tr><td>".$tok."</td>";
			$tok = substr(strtok(" \t\n"),9);
			echo "<tr><td>";
			echo "<a href=\"/cgi-bin/file.php?path=".$tok."\">".$tok."</a>";
			echo "</td></tr>";
		  }
		  fclose($handle);
		  echo "</td>\n";
	echo "</table>";
}
?>
<br>
<a href="/cgi-bin/logout.php">Logout</a>
</body>
</html>
