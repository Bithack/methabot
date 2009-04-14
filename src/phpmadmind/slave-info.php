<?php
$info = simplexml_load_string($m->slave_info((int)$_GET['id']));
$clients = simplexml_load_string($m->list_clients((int)$_GET['id']));
if ($info) {
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
    <input type="button" value="Stop all clients" />
    <input type="button" value="Continue all" />
    <input type="button" value="Kill" />
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
              "<td>%s</td>".
            "</tr>",
            $x?" class=\"odd\"":"",
            $id,
            $id,
            $c->user);
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
