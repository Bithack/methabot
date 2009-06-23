<?php
session_start();
include_once "metha.class.php";
include_once "config.php";

$pages = Array("overview" => "overview.php",
               "latest" => "latest.php",
               "addr" => "addr.php",
               "show-config" => "show-config.php",
               "slave-info" => "slave-info.php",
               "slave-list" => "slave-list.php",
               "input-list" => "input-list.php",
               "session-info" => "session-info.php",
               "session-list" => "session-list.php",
               "users" => "users.php",
               "client-info" => "client-info.php");

if ($_SESSION['authenticated'] == 1) {
    $m = new Metha;
    if (!$m->connect($config['master-host'], $config['master-port']))
        $page = "disconnected.php";
    else if (($hello = $m->authenticate($_SESSION['user'], $_SESSION['pass'])) === false)
        $page = "login.php";
    else {
        $x = "user-level";
        $level = (int)(simplexml_load_string($hello)->$x);

        if (!isset($_GET['p']))
            $page = "overview.php";
        else
            $page = @$pages[$_GET['p']];

        if (!$page)
            $page = "notfound.php";
    }
} else
    $page = "login.php";
?>
