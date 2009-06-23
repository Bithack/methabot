<?php
include "common.php";
if ($m->add_user(
            $_POST["user"], $_POST['pass'],
            $_POST['fullname'],
            $_POST['level'], $_POST['extra']))
    report_back("The user '".htmlspecialchars($_POST['user'])."' was added successfully.");
else
    report_back("An error occured when attempting to add user '".$_POST['user']."'!", 1);
?>
