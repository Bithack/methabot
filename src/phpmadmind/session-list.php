<?php
$info = simplexml_load_string($m->list_sessions());
if ($info) {
?><h2>[master]/session-list</h2>
<div class="content-layer">
  <table>
  <?php $x=1; foreach ($info->session as $s) { ?>
    <tr<?=$x=!$x?" class=\"odd\"":""?>>
      <td class="session">
        <strong><a href="?p=session-info&amp;id=<?=$s->attributes()->id?>">[#<?=$s->attributes()->id?>]</a></strong>
        <?=$s->latest?>
        <span class="state"><?=$s->state?></span>
      </td>
      <td class="url-info"><?=$s->crawler." &lt;- <span class=\"input\">".$s->input."</span>"?></td>
      <td><a class="id" href="?p=client-info&amp;id=<?=$s->client?>"><?=$s->client?></a></td>
    </tr>
  <?php } ?>
  </table>
</div>
<?php
} else {
    echo "<em>No session found.</em>";
}
?>
