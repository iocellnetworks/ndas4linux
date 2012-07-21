#!/bin/sh /usr/bin/php-wrapper
<?php
include('header.php');
if ( ! $_GET['file'])
{
   echo "No file is specified\n";
} else {
   $delete_file = '/mnt/ndas/' . $_GET['file'] ;
   $is_d = is_dir($delete_file);
   if ( is_file($delete_file) && unlink($delete_file) || 
        is_dir($delete_file) && rmdir($delete_file) )
   {
          if ( $is_d ) 
              echo "Directory \""; 
          else
              echo "File \""; 
          print($_GET['file']);
          echo "\" is deleted\n"; 
   } else {
          echo "Fail to delete";
          print($_GET['file']);
   } 
}
?> <br>
<form method=GET
    action="/cgi-bin/file.php">
<input name=path type=hidden value="<?php echo dirname($_GET['file']); ?>">
<input value="Back" type=submit>
</form>
</body>
</html>

