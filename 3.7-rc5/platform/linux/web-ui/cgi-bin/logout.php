#!/bin/sh /usr/bin/php-wrapper
<?
// Login & Session example by sde
// logout.php

// you must start session before destroying it
session_start();
session_destroy();

echo "<img src=/img/logo.gif><br>";
echo "You have been successfully logged out.

<br><br>
You will now be returned to the login page.

<META HTTP-EQUIV=\"refresh\" content=\"0; URL=/index.html\"> ";
?> 
