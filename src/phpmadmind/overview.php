<?php
$info = $m->system_info();
$info = simplexml_load_string($info);
$num_slaves = "num-slaves";
$num_sessions = "num-sessions";
$t = (int)$info->uptime;

$uptime = "";
$hours = (int)($t/3600);
$min = (int)(($t % 3600)/ 60);
$sec = $t%60;
$uptime = "$hours h, $min min, $sec seconds";

?>
<h2>[master]/overview</h2>
<div class="content-layer">
  <table>
    <tr><th>Master uptime</th><td><?=$uptime?></td></tr>
    <tr><th>Address</th><td><?=$info->address?></td></tr>
    <tr><th>Connected Slaves</th><td><?=$info->$num_slaves?></td></tr>
  </table>
</div>

<div class="content-layer">
<h3>Global Statistics</h3>
  <table>
    <tr><th>Total Session Count</th><td><?=$info->$num_sessions?></td></tr>
  </table>
</div>
