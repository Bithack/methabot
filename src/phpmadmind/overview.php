
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
<h3>Add URL</h3>
<form method="post" action="?p=overview&amp;do=add-url">
  <p>
    <input name="url" type="text" value="http://" />
    <a href="#" onclick="document.getElementById('adv').style.display='block'; this.style.display='none'; return false;">Advanced options &raquo;</a>
  </p>
  <div id="adv" style="display:none;">
  <table>
    <tr><th>Crawler</th><td><select name="crawler"><option value="default">default</option></select></td></tr>
    <tr><th>Scheduled at</th><td><input type="text" value="00-00-00 00:00:00" /></td></tr>
  </table>
  </div>
  <p><input type="submit" value="Add URL" /></p>
</form>
