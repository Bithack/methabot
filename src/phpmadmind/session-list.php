<h2>[master]/session-list</h2>
<?php
$info = str_replace("&", "&amp;", $m->list_sessions());
$info = simplexml_load_string($info);
if ($info) {
?>
<div class="content-layer">
  <table>
  <?php $x=1; foreach ($info->session as $s) { ?>
    <tr<?=$x=!$x?" class=\"odd\"":""?>>
      <td class="sep"> <strong><a class="id" href="?p=session-info&amp;id=<?=$s->attributes()->id?>">session #<?=$s->attributes()->id?></a></strong> </td>
      <td><?=$s->latest?></td>
      <td class="state sep"><?=$s->state?></td>
      <td class="sep"><?=$s->crawler." &larr; <span class=\"input\">".$s->input."</span>"?></td>
      <td>client: <a class="id" href="?p=client-info&amp;id=<?=$s->client?>"><?=substr($s->client, 0, 3)."...".substr($s->client, -3)?></a></td>
    </tr>
  <?php } ?>
  </table>
</div>
<?php
} else {
    echo "<em>No sessions found.</em>";
}
?>
