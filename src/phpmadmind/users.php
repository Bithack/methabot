<?php
$info = $m->system_info();
$info = simplexml_load_string($info);
$num_users = "num-users";
$num_sessions = "num-sessions";
$num_users = (int)$info->$num_users;
?>
<h2>[master]/users</h2>
<div class="content-layer">
<a class="button" href="#" onclick="document.getElementById('hidden-add').style.display='block';document.getElementById('hidden-search').style.display='none';return false;">Add new user</a>
<a class="button" href="#" onclick="document.getElementById('hidden-search').style.display='block';document.getElementById('hidden-add').style.display='none';return false;">Search for a user</a>
</div>
<div id="hidden-search" class="hidden-form content-layer">
<form method='get' action='?p=search'>
<div><input type="text" name="q"></div>
<p><input type="submit" value="Search"></p>
</form>
</div>
<div id="hidden-add" class="hidden-form content-layer">
<form method='post' action='action/add-user.php'>
<div><label for="user">Email:</label> <input type="text" name="user" id="user" /></div>
<div><label for="pass">Password:</label> <input type="password" name="pass" id="pass" /></div>
<div><label for="fullname">Full name:</label> <input type="text" name="fullname" id="fullname" /></div>
<div><label for="level">Security level:</label>
<select name="level" id="level">
  <option value="1">Guest, Read-only Access</option>
  <option selected="selected" value="2">Normal User, Permission to add URLs</option>
  <option value="1024">Manager, permission to manage other users</option>
  <option value="2048">Operator, permission to manage slaves and clients</option>
  <option value="8192">Administrator, all privileges</option>
</select>
</div>
<?php if (strlen($config['extra-name'])) { ?>
<div id="extra-sep"><label for="extra"><?=$config['extra-name']?>:</label> <input type="text" name="extra" id="extra"></div>
<?php } ?>
<p><input type="submit" value="Add User"></p>
</form>
</div>
<h3>User List</h3>
<?php
$start = (int)$_GET['start'];
$userxml = $m->list_users($start, 25);
$sl = simplexml_load_string($userxml);

$pagebtns = "<div style=\"padding-bottom: 6px;\">";
if ($start-25 >= 0)
   $pagebtns.="<div style=\"float:left;\"><a href=\"?p=users&start=".($start-25)."\">&laquo; Previous page</a></div>";
if ($start+25 < $num_users)
   $pagebtns.="<div style=\"float:right;\"><a href=\"?p=users&start=".($start+25)."\">Next page &raquo;</a></div>";
$pagebtns.="</div>";

if ($sl) {
    $x = 0;
    echo $pagebtns;
    echo "<table>";
    foreach ($sl->user as $s) {
        printf("<tr%s>".
                 "<td class=\"sep\"><a href=\"?p=user-info&amp;id=%d\">%s</a></td>".
                 "<td class=\"sep\">%s</td>".
                 "<td>%d</td>".
                 "<td><em>%s</em></td>".
                 "<td>[<a href=\"action/del-user.php?id=%d\" onclick=\"return confirm('Do you really want to delete this user?');\">delete</a>]".
                 " [<a href=\"#\" onclick=\"if (v = prompt('Enter new password')) {document.location.href='action/passwd.php?id=%d&amp;password='+v;} return false;\">passwd</a>]</td>".
               "</tr>",
                $x?" class=\"odd\"":"",
                $s->attributes()->id,
                $s->username,
                $s->fullname,
                $s->level,
                $s->extra,
                $s->attributes()->id,
                $s->attributes()->id
                );
        $x = !$x;
    }

    echo "</table>";
    echo $pagebtns;
} else
    echo "No users found.";
?>
