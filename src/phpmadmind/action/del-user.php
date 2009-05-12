<?php
include "common.php";
if ($m->del_user($_POST["id"]))
    report_back("User deleted successfully");
else
    report_back("An error occured", 1);
?>
