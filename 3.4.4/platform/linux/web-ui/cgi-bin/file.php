#!/bin/sh /usr/bin/php-wrapper
<?php
include ("auth.php");
$path_param=urldecode($_GET['path']);
if ( !isset($_GET['path'])) {
    $path_param=urldecode($_POST['path']);
}
if ( strpos($path_param, '/') != 0 ) {
    $path_param = '/' . $path_param;
}
if ( strlen($path_param) > 1 && strpos($path_param, '/', strlen($path_param) - 1) == strlen($path_param) - 1 ) {
    $path_param = substr($path_param, 0, strlen($path_param) - 1);
}
$dir='/mnt/ndas'.$path_param;
if ( $dir == '/mnt/ndas/' )
{
    $dir = '/mnt/ndas';
}
?>
<iframe name="check" frameborder="0" width="0%" height="0%"></iframe>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
<title>Index of <?php print($path_param); ?></title>
    <style type="text/css">
    a, a:active {text-decoration: none; color: blue;}
    a:visited {color: #48468F;}
    a:hover, a:focus {text-decoration: underline; color: red;}
    body {background-color: #F5F5F5;}
    h2 {margin-bottom: 12px;}
    table {margin-left: 12px;}
    th, td { font-family: "Courier New", Courier, monospace; font-size: 10pt; text-align: left;}
    th { font-weight: bold; padding-right: 14px; padding-bottom: 3px;}
    td {padding-right: 14px;}
    td.s, th.s {text-align: right;}
    div.list { background-color: white; border-top: 1px solid #646464; border-bottom: 1px solid #646464; padding-top: 10px; padding-bottom: 14px;}
    div.foot { font-family: "Courier New", Courier, monospace; font-size: 10pt; color: #787878; padding-top: 4px;}
    form { margin-bottom:0; }                                                   
        input.btn {                                                                 
        font-family:'trebuchet ms',helvetica,sans-serif;                         
        font-size:10;                                                            
        font-weight:bold;                                                        
        border:1px solid;                                                        
        fond-color: #3f3f3f;                                                     
        background: #efffff;                                                     
        height: 18 ;                                                             
    }                                                                           
    .css_browse_button {                                                        
        font-family:'trebuchet ms',helvetica,sans-serif;                         
        font-size:10;                                                            
        font-weight:bold;                                                        
        border:1px solid;                                                        
        height: 18 ;                                                             
    } 
    </style>
</head>
<!--                                                    
<script language="JavaScript" src="/zuprogress/progress.js"></script>
-->  
<script language="javascript">
function createDir()
{
	//window.open("create_folder.php","_cdir","width=300,height=300");
	window.open("","_cdir","width=350,height=200");
}
function upload()
{
	var rnum = Math.floor(Math.random() * 10000);
	window.open("","up"+rnum,"width=400,height=200");
	document.fup.target="up"+rnum;
	document.fup.submit();
}
</script>
<body>
<img src="/img/logo.gif"><br><br>
<?php
if ( !is_dir($dir) ) { 
    die("No such directory - ".$path_param);
}
?>
<h3>Index of <?php echo $path_param; ?></h3>
<div class=list>
<table cellpadding=0 cellspacing=0>
<thead>
  <tr><b>
    <th>Name</th>
    <th>Size</th>
    <th>Type</th>
    <th>Actions</th>
  </tr>
</thead>
<?php
$dh = opendir($dir);
if ( !$dh ) {
    die;
}
while ( $entry = readdir($dh) )
{
    if( $entry == '.' )
		continue;
    if( $entry == '..' )
	{
        $e_file = dirname($dir);
        $e_file_param = dirname($path_param);
    }
	else
	{
    	$e_file = $dir.'/'.$entry;
        $e_file_param = $path_param.'/'.$entry;
    }
        
    if ( is_dir($e_file) ) { 
?>
  <tr><td>
    <?php
        echo "<a href=\"/cgi-bin/checkDir.php?path=";
        echo urlencode($e_file_param);
		echo "\" target=\"check\">";
        if ( $entry == '..' )
			echo 'Parent directory';
        else 
            echo $entry; 
        echo "</a>";
    ?>
      </td>
      <td>-</td>
      <td>Directory</td>
      <td><form method=GET action=/cgi-bin/delete.php>
            <input type=hidden name=file value="<?php echo urlencode($e_file_param); ?>">
            <input type=submit value=Delete class=btn></form>
      </td>
  </tr>
<?php
    } else {
?>

  <tr><td><a href="/ndas<?php echo $e_file_param; ?>">
                <?php echo $entry; ?></a></td>
      <td><?php echo filesize($e_file); ?></td>
      <td><?php echo filetype($e_file); ?></td>
      <td><form method=GET action=/cgi-bin/delete.php>
            <input type=hidden name=file value="<?php echo urlencode($e_file_param); ?>">
            <input type=submit value=Delete class=btn></form>
      </td>
  </tr>
<?php
   } 
}
?>
	</table>
</div>
<!--
<form method=POST enctype=multipart/form-data action=/cgi-bin/upload.php>
<label for=file>File:</label>
<input type=file name=file>
<input type=hidden name=path value=<?php echo urlencode($path_param); ?>>
<input type=submit value=Upload>
</form>
-->
<br>
<table>
<tr>
<td>
<form name=fup method=POST action=upfile.php>
<input type=hidden name=path value=<?php echo urlencode($path_param); ?>>
<input type=button value="Upload" onclick="javascript:upload()">
</form></td>
<td>
<form method=POST target=_cdir action=create_dir.php onSubmit="javascript:createDir()">
<input type=hidden name=path value=<?php echo urlencode($path_param); ?>>
<input type=submit value="Create Directory">
</form></td>
<td><input type=button onclick="javascript:location.reload()" value=Refresh></td>
</tr>
</table>
<form method=GET action=/cgi-bin/list.php>
<input value="Back" type=submit>
</form><br>
<a href="/cgi-bin/logout.php">Logout</a> 
</body>
</html>
