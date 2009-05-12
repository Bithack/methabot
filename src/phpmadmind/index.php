<?php
session_start();
include("version.php");
include_once("session.php");

if (isset($_GET['do'])) {
    switch ($_GET['do']) {
        case "login":
            if ($_SERVER['REQUEST_METHOD'] == "POST") {
                $m = new Metha;
                if ($m->connect($config['master-host'], $config['master-port']) 
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
            break;

        case "logout":
            session_destroy();
            break;
    }
}
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN"
 "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
    <head>
        <title><?=$config["title"]?></title>
        <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
        <link rel="stylesheet" type="text/css" href="style.css" />
    </head> 
    <body>
        <div id="header">
          <div id="header-inner">
            <?php if ($_SESSION['authenticated'] == 1) { ?><span id="login-info">Welcome, <strong><?=$_SESSION['user']?></strong> [<a href="?do=logout">Log out</a>]</span><?php } ?>
            <h1><?=$config["header"]?></h1>
          </div>
        </div>
        <div id="container">
            <div id="content">
            <?php if ($_SESSION['authenticated'] == 1) { ?>
                <div id="left">
                    <ul style="clear: both;">
                        <li><a href="?p=overview"><img src="img/overview.png" /> Overview</a></li>
                        <li><a href="?p=input-list"><img src="img/input-data.png" /> Input Data</a></li>
                        <li><a href="?p=session-list"><img src="img/sessions.png" /> Sessions</a></li>
                        <li><a href="?p=slave-list"><img src="img/slave-list.png" /> Slave List</a></li>
                        <?php if ($level >= 1024) { ?>
                        <li>&nbsp;</li>
                        <li><a href="?p=users"><img src="img/users.png" /> Users</a></li>
                        <li><a href="?p=show-config"><img src="img/configuration.png" /> Configuration</a></li>
                        <?php } ?>
                    </ul>
                </div>
                <div id="content-inner">
                <?php
                    if (isset($_GET['msg'])) {
                        if (!$_GET['error']) 
                            echo "<div class=\"msg\">".htmlspecialchars($_GET['msg'])."</div>";
                        else
                            echo "<div class=\"error\">".htmlspecialchars($_GET['msg'])."</div>";
                    }
                ?>
            <?php } else { ?>
                <div id="content-login">
            <?php } ?>
                    <?php
                    include ($page);
                    ?>
                </div>
            </div>
            <div id="footer"><a href="http://metha-sys.org/"><img src="img/methanol-fueled.png" /></a></div>
        </div>
    </body>
</html>
<?php
if ($m) $m->disconnect();
?>
