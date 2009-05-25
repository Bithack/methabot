<?php
session_start();
$_SESSION['authenticated'] = 0;
session_destroy();
header("Location: ../");
?>
