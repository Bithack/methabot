<?php
include "common.php";
if ($m->del_user($_GET["id"]))
    report_back("User deleted successfully");
else
    report_back("An error occured (".$m->status_code.")", 1);
?>
