#!/bin/sh /usr/bin/php-wrapper
<?php
if (1) {
    $username = $_POST['username'];
    $password = $_POST['password'];
    $AUTH="TRUE";
    session_start();
    $_SESSION['username'] = $username;
    $_SESSION['password'] = $password;
    $_SESSION['AUTH'] = "TRUE";
?>
<a href="/cgi-bin/list.php">Add/Remove NDAS disks</a><br>
<a href="/cgi-bin/file.php">Manage Files NDAS disks</a><br>
<?php
} else {
    echo "Fail to login";
}
?>
