<?php
include "common.php";
if ($m->passwd($_GET["password"], $_GET["id"]))
    report_back("Password changed successfully");
else
    report_back("An error occured ({$m->status_code})", 1);
?>
