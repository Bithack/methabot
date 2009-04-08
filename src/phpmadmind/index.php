<?php
session_start();
require("metha.class.php");
include("version.php");
include("config.php");

$pages = Array("overview" => "overview.php",
               "latest" => "latest.php",
               "addr" => "addr.php",
               "show-config" => "show-config.php",
               "slave-info" => "slave-info.php",
               "slave-list" => "slave-list.php",
               "input-list" => "input-list.php",
               "session-info" => "session-info.php",
               "session-list" => "session-list.php",
               "client-info" => "client-info.php");

if ($_SERVER['REQUEST_METHOD'] == "POST") {
    if ($_GET['do'] == "login") {
        $m = new Metha;
        if ($m->connect($config['master']) 
               && $m->authenticate($_POST['user'], $_POST['pass'])) {
            $_SESSION['authenticated'] = 1;
            $_SESSION['user'] = $_POST['user'];
            $_SESSION['pass'] = $_POST['pass'];
        } else {
            $_SESSION['authenticated'] = 0;
            $_SESSION['user'] = 0;
            $_SESSION['pass'] = 0;
            $page = "login.php";
        }
    }
}

if ($_SESSION['authenticated'] == 1) {
    $m = new Metha;
    if (!$m->connect($config['master']))
        $page = "disconnected.php";
    else if (!$m->authenticate($_SESSION['user'], $_SESSION['pass']))
        $page = "login.php";
    else {
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
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN"
 "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
    <head>
        <title>phpMadmind Control Panel</title>
        <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
        <link rel="stylesheet" type="text/css" href="style.css" />
    </head> 
    <body>
        <div id="container">
            <div id="header">
                <?php if ($_SESSION['authenticated'] == 1) { ?><span id="header-info">Logged in as <strong><?=$_SESSION['user']?></strong> [<a href="#">Log out</a>]</span><?php } ?>
                <h1><?=$config["header"]?></h1>
            </div>
            <div id="content">
                <div id="left">
                    <h4>Navigation</h4>
                    <ul>
                        <li><a href="?p=overview">Overview</a></li>
                        <li><a href="?p=input-list">Input Data</a></li>
                    <!--</ul>
                    <h4>System</h4>
                    <ul>-->
                        <li><a href="?p=session-list">Sessions</a></li>
                        <li><a href="?p=slave-list">Slave List</a></li>
                        <li><a href="?p=show-config">Configuration</a></li>
                    </ul>
                </div>
                <div id="content-inner">
                    <?php include ($page); ?>
                </div>
            </div>
            <div id="footer"><a href="http://metha-sys.org/"><img src="img/methanol-fueled.png" /></a></div>
        </div>
    </body>
</html>
<?php
if ($m) $m->disconnect();
?>
