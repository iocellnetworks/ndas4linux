#!/bin/sh /usr/bin/php-wrapper
<?php
session_start();
// check user
$curuserid = $_SESSION['USERID'];
$path_param = urldecode($_POST['path']);
if(!isset($curuserid) || empty($curuserid) || empty($path_param))
{
	echo "<h3>You don't have the right permission.</h3>";
	exit;
}
?>
<html>
<head>
<title>Upload</title>
</head>
<body>
<?php
	echo "Target Directory: ".$path_param;
?>
<form method=POST enctype=multipart/form-data action=upload.php>
<label for=file>File:</label>
<input type=file name=file>
<input type=hidden name=path value=<?php echo urlencode($path_param); ?>>
<input type=submit value=Upload>
</form>
</body>
</html>
