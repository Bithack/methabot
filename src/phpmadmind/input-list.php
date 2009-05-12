<h2>[master]/input-list</h2>
<div class="content-layer">
<h3>Add URL</h3>
<form method="post" action="action/add-input.php">
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
</div>
<div class="content-layer">
<h3>Added by you</h3>
<?php
$info = simplexml_load_string($m->list_input());
if ($info) {
?>
  <table>
  <?php $x=1; foreach ($info->input as $i) { ?>
    <tr<?=$x=!$x?" class=\"odd\"":""?>>
      <td><?=$i->crawler?> &larr; <span class="input"><?=$i->value?></span></td>
      <td>[<a href="#">remove</a>] [<a href="#">force recrawl</a>]</td>
    </tr>
  <?php } ?>
  </table>
<?php
} else {
    echo "<em>No input data found.</em>";
}
?>
</div>
