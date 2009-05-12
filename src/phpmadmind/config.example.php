<?php
/* replace the values in this file and
   rename it to config.php */

/*** Connection information ***/
$config["master-host"] = "127.0.0.1";
$config["master-port"] = 5505;

/*** Content ***/
$config["title"] = "Methanol Control Panel";
$config["header"] = "Methanol Control Panel";
$config["extra-name"] = false;

/**
 * Set the following option to an array of filetype
 * names that you wish to include counters for
 * in the session info page. By default, it counts
 * the amount of HTML pages, you can include counters
 * for any filetype you have defined in your system
 * configuration.
 **/
$config["counters"] = Array(
        "html" => "Pages",
        );
?>
