<h2>[master]/slave-list</h2>
<table>
<?php
$slavexml = $m->list_slaves();
$sl = simplexml_load_string($slavexml);

if ($sl) {
    $x = 0;
    $c = 'num-clients';
    foreach ($sl->slave as $s) {
        printf("<tr%s><td><a class=\"id\" href=\"?p=slave-info&amp;id=%d\">%s-%d</a></td><td class=\"sep\">%s</td><td>%s</td></tr>",
                $x?" class=\"odd\"":"",
                $s->attributes()->id,
                $s->user,
                $s->attributes()->id,
                $s->address,
                $s->$c==1?$s->$c." connected client":$s->$c." connected clients"
                );
        $x = !$x;
    }
} else
    echo "No slave server connected.";
?>
</table>
