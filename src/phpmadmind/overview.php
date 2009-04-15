
<?php
if ($_SERVER['REQUEST_METHOD'] == "POST") {
    if (@$_GET['do'] == "add-url") {
        if ($m->add_url($_POST["url"], $_POST['crawler']))
            echo "<div class=\"msg\">The URL '".htmlspecialchars($_POST['url'])."' was added successfully.</div>";
        else
            echo "<div class=\"error\">An error occurred when attempting to add URL '".htmlspecialchars($_POST['url'])."'.</div>";
    }
}
?>
<h2>[master]/overview</h2>
