<?php
include "../session.php";

function report_back($msg, $is_error=0)
{
    $s = $_SERVER['HTTP_REFERER'];
    $s = strstr($s, "?p=");
    if (($x = strpos($s, "&")))
        $s = substr($s, 0, $x);
    if ($is_error)
        header("Location: ../".$s."&error=1&msg=".urlencode($msg));
    else
        header("Location: ../".$s."&msg=".urlencode($msg));
}
?>
