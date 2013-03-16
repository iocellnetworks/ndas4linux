#!/bin/sh /usr/bin/php-wrapper
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en-AU"><head>
      <meta http-equiv="content-type" content="application/xhtml+xml; charset=UTF-8">
      <link rel="stylesheet" href="styles.css" type="text/css"><style type="text/css" id="_noscript_styled">.-noscript-blocked { border: 1px solid red !important; background: white url("chrome://noscript/skin/icon32.png") no-repeat left top !important; opacity: 0.6 !important; }</style></head>
            
<body>
<img src="http://www.ximeta.com/img/logo.gif"><br><br>
<?php
$path = urldecode($_POST['path']);
$uploaddir = '/mnt/ndas/' . $path. '/' ;
if( empty($path) || ! $_FILES['file']['name'])
{
   echo "No file is specified\n";
}
else
{
   $uploadfile = $uploaddir . basename($_FILES['file']['name']);

   if(move_uploaded_file($_FILES['file']['tmp_name'], $uploadfile))
   {
      echo "File \""; 
      print($_FILES['file']['name']);
      echo "\" is successfully uploaded into the directory \""; 
      print($path);
      echo "\"\n";
   }
   else
   {
      echo "No file is specified\n";
   }
}
?>
<br>
<?php
//	echo "<form method=GET";
//	echo "      action=/cgi-bin/file.php>";
//	echo "<input type=hidden name=path value=\"";
 //       print(urlencode($path)); 
  //      echo "\">";
?>
<!--
<input value="Back" type=submit>
</form>
-->
<input type=button onclick="javascript:self.close()" value=Close>
</body>
</html>

