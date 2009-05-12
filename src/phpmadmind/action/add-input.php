<?php
include "common.php";

if ($m->add_url($_POST["url"], $_POST['crawler']))
    report_back("The URL '".$_POST['url']."' was added successfully.");
else
    report_back("An error occurred when attempting to add URL '".$_POST['url']."'", 1);
?>
