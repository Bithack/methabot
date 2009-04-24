<?php
$info = simplexml_load_string($m->slave_info((int)$_GET['id']));
$clients = simplexml_load_string($m->list_clients((int)$_GET['id']));
if ($info) {
    $id = $info->attributes()->for;
    $id = (int)substr($id, strpos($id, '-')+1);
    if (isset($_GET['do']) && $_GET['do'] == 'disconnect-all')
        $m->kill_all($id);
?>
<h2>[<?=$info->attributes()->for?>]/info</h2>
<div class="content-layer">
    <h3>Status</h3>
    <table>
        <tr><th>Address</th><td><?=$info->address?></td></tr>
    </table>
</div>
<div class="content-layer">
    <h3>Signals</h3>
    <a class="button" href="#">Kill</a>
    <a class="button" href="#">Stop all clients</a>
    <a class="button" href="#">Continue all</a>
    <a class="button" href="?p=slave-info&amp;id=<?=$id?>&amp;do=disconnect-all">Disconnect all clients</a>
</div>
<div class="content-layer">
    <h3>Clients</h3>
<?php
if (!$clients)
    echo "No client connected.";
else {
?>
    <table>
<?php
$x = 0;
foreach ($clients->client as $c) {
    $id = $c->attributes()->id;
    printf(
            "<tr%s>".
              "<td><a class=\"id\" href=\"?p=client-info&amp;id=%s\">%s</a></td>".
              "<td class=\"sep\">%s@%s</td>".
              "<td><em>%s</em></td>".
            "</tr>",
            $x?" class=\"odd\"":"",
            $id,
            $id,
            $c->user, $c->address, $c->status==1?"running":"idle");
    $x = !$x;
}
?>
    </table>
<?php
}
?>
</div>
<?php
} else {
?>
<h2>error</h2>
Slave not found.
<?php
}
?>
