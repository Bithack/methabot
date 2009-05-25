<?php
session_start();
include "../config.php";
include "../metha.class.php";
if ($_SERVER['REQUEST_METHOD'] == "POST") {
    $m = new Metha;
    if (!$m->connect($config['master-host'], $config['master-port'])) {
        header("Location: ../?p=login&msg=".urlencode("Could not contact the master server, perhaps the system has been shut down temporarily.")."&error=1");
    } else if ($m->authenticate($_POST['user'], $_POST['pass'])) {
        $_SESSION['authenticated'] = 1;
        $_SESSION['user'] = $_POST['user'];
        $_SESSION['pass'] = $_POST['pass'];
        header("Location: ../?p=overview&msg=".urlencode("Successfully logged in"));
    } else {
        $_SESSION['authenticated'] = 0;
        $_SESSION['user'] = 0;
        $_SESSION['pass'] = 0;
        header("Location: ../?p=login&msg=".urlencode("Wrong username and/or password")."&error=1");
    }
}
?>
