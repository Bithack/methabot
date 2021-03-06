<?php
$id = (int)$_GET['id'];
$info = simplexml_load_string($m->session_info($id));
if ($info) {
?><h2>[#<?=$id?>]/info</h2>
<div class="content-layer">
  <table style="float:left;">
    <tr><th>Started</th><td><?=$info->started?></td></tr>
    <tr><th>Last Update</th><td><?=$info->updated?></td></tr>
  </table>
  <table style="float:left;">
    <tr><th>Client</th><td><a class="id" href="?p=client-info&id=<?=$info->client?>"><?=$info->client?></a></td></tr>
    <tr><th>Current state</th><td class="state"><?=$info->state?></td></tr>
  </table>
</div>
<div class="content-layer">
  <table>
    <tr><th>Input</th><td><?=$info->crawler?> &larr; <span class="input"><?=htmlspecialchars($info->input)?></span></td></tr>
<?php foreach ($config['counters'] as $type => $name) {
    $type = "num-".$type;
    $val = $info->$type;
    if ($val === false || !strlen($val))
        $val = "N/A";
    ?>
    <tr><th><?=$name?></th><td><?=$val?></td></tr>
<?php } ?>
  </table>
<div class="content-layer">
<?php if ($info->state == 'done') { ?>
<div class="content-layer">
  <h3>Session Report</h3>
  <div class="report">
  <?php
  $s = $m->session_report($id);
  if ($s === false || !strlen($s))
      echo "<em>No session report was generated.</em>";
  else echo $s;
  ?>
  </div>
</div>
<?php
}
?>

<?php
} else {
    echo "<em>No such session.</em>";
}
?>
