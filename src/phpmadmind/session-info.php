<?php
$id = (int)$_GET['id'];
$info = simplexml_load_string($m->session_info($id));
if ($info) {
?><h2>[#<?=$id?>]/info</h2>
<div class="content-layer">
  <table style="float:left;">
    <tr><th>Started</th><td><?=$info->started?></td></tr>
    <tr><th>Last Update</th><td><?=$info->latest?></td></tr>
    <tr><th>Current state</th><td class="state"><?=$info->state?></td></tr>
  </table>
  <table style="float:left;">
    <tr><th>Client</th><td><a class="id" href="?p=client-info&id=<?=$info->client?>"><?=$info->client?></a></td></tr>
    <tr><th>Input</th><td>http://metha-sys.org/</td></tr>
    <tr><th>Crawler</th><td>default</td></tr>
  </table>
</div>
<div class="content-layer">
  <h3>Session Report</h3>
  <div class="report">
  <?=$m->session_report($id)?>
  </div>
</div>
<?php
} else {
    echo "<em>No such session.</em>";
}
?>
