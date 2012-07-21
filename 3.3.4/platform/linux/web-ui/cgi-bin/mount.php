#!/bin/sh /usr/bin/php-wrapper
<?
$device=$_GET['mount_devi'];
$path=$_GET['mount_path'];
$slot=$_GET['mount_slot'];
?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en-AU">
<head>
    <meta http-equiv="content-type" content="application/xhtml+xml; charset=UTF-8" />
    <link rel="stylesheet" href="styles.css" type="text/css">
</head>
<body>
<img src="http://www.ximeta.com/img/logo.gif"><br><br>

Mounting <?=$device?> to <?=$path?><br>
<?
$ret=0;
@mkdir("/mnt/ndas/$path");
$result = system("/www/cgi-bin/mount_ndas.sh $device /mnt/ndas/$path", &$ret);
if ( $ret == 0 ) { ?>
	<div class=title>Successfully mounted</div>
<? } else { ?>
	<div class=title><?=$result?></div>
<? } ?>
<form method=GET action=/cgi-bin/manage.php>
<input name=slot value="<?=$slot?>" type=hidden>
<input value="Back" type=submit>
</form>
</body>
</html>
