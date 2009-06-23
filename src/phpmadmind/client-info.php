<?php
$token = preg_replace("[^a-f0-9]", "a", strtolower($_GET['id']));
$info = simplexml_load_string($m->client_info($token));
if ($info) {
?><h2>[<?=$token?>]/info</h2>
<div class="content-layer">
    <h3>Status</h3>
    <table>
    <tr><th>Address</th><td><?=$info->address?></td></tr>
    <tr><th>Username</th><td><?=$info->user?></td></tr>
    <tr><th>Slave</th><td><a class="id" href="?p=slave-info&id=<?=substr($info->slave, strpos($info->slave, "-")+1)?>"><?=$info->slave?></a></td></tr>
    <tr><th>Status</th><td><em>
    <?php
    if ($info->status == 1) {
        $sess_id = (int)$info->session;
        echo "running <a class=\"id\" href=\"?p=session-info&amp;id=$sess_id\">session #$sess_id</a>";
    } else
        echo "idle";
    ?>
    </em></td></tr>
    </table>
</div>
<?php
} else {
    echo "<em>No such client connected.</em>";
}
?>
