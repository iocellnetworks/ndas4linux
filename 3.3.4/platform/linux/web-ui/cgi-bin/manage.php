#!/bin/sh /usr/bin/php-wrapper
<?php
include("header.php");
if ( isset($_GET['slot']) ) {
    $slot = $_GET['slot'];
} else {
    $slot = $_POST['slot'];
}
if ( !isset($slot) )
{
	echo "slot is not set";
	die;
}
?>
<h1>Mount the file system or Unmount the file system</h1>
<table border=1>
<tr><td>block</td><td>type</td><td>mounting path</td><td>action</td></tr>
<?
$devfile="/dev/nd".chr(ord('a')+$slot-1);
$mount_path=array();
exec("fdisk -l ".$devfile." | grep ^".$devfile, &$mount_path);
for($i=0;$i<count($mount_path);$i++)
{
  $d=substr($mount_path[$i],0,9);
  $type=substr($mount_path[$i],55);
  $path=exec("cat /proc/mounts | grep '^".$d."' | cut -d' ' -f2 | sed -e 's|/mnt/ndas||'");
?>
<tr>
<td><?=$d?></td>
<td><?=$type?></td>
<? if ( $path == "") { ?>
<form method=GET action=mount.php>
<td><input name=mount_path></td>
<td>
<input name=mount_slot value="<?=$slot?>" type=hidden>
<input name=mount_devi value="<?=$d?>" type=hidden>
<input value=Mount type=submit>
</td>
</form>
<? } else {  ?>
<td><a href="/cgi-bin/file.php?path=<?=$path?>"><?=$path?></a></td>
<td>
<form method=GET action=unmount.cgi>
<input name=umount_slot value="<?=$slot?>" type=hidden>
<input name=umount_devi value="<?=$d?>" type=hidden>
<input name=umount_path value="<?=$path?>" type=hidden>
<input value=Unmount type=submit>
</form>
<? } ?>
</td>
</tr>
<? } ?>
</table>
<form method=GET action=/cgi-bin/list.php>
<input value="Back" type=submit>
</form>
Please note you can not add/delete/modify the partition, or format the partition from this router.<br>
You can manipulate partition that from the Desktop PC or other devices.<br>
<br>
<a href="/cgi-bin/logout.php">Logout</a> 
</body>
</html>
