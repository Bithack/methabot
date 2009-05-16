<?php
class Metha
{
    private $fp;
    private $connected;
    public  $status_code;

    function __construct()
    {
    }

    function __destruct()
    {
        $this->disconnect();
    }

    function connect($host="127.0.0.1", $port=5505)
    {
        if ($this->connected)
            return false;
        if (false !== ($this->fp = @fsockopen($host, $port)))
            return true;
        return false;
    }

    function disconnect()
    {
        if (!$this->connected)
            return false;

        $this->connected = false;
        return fclose($fp);
    }

    function authenticate($user, $pass)
    {
        if (false == $this->send("AUTH user $user $pass"))
            return false;
        if (false == ($r = $this->getline()))
            return false;
        if ((int)$r < 100 || (int)$r >= 200) {
            $this->status_code = $r;
            return false;
        }

        return $this->get_sized_reply("HELLO");
    }
    function del_user($id)
    {
        if (false == $this->send("USERDEL $id"))
            return false;
        if (false == $r = $this->getline())
            return false;
        if ((int)$r < 100 || (int)$r >= 200) {
            $this->status_code = $r;
            return false;
        }

        return true;
    }
    function passwd($password, $id=false)
    {
        if (!$id)
            $cmd = "PASSWD $password";
        else
            $cmd = "PASSWD-ID $id $password";
        if (false == $this->send($cmd))
            return false;
        if (false == $r = $this->getline())
            return false;
        if ((int)$r < 100 || (int)$r >= 200) {
            $this->status_code = $r;
            return false;
        }

        return true;
    }

    function add_url($url, $crawler="default")
    {
        if (false == $this->send("ADD $crawler $url"))
            return false;
        if (false == $r = $this->getline())
            return false;
        if ((int)$r < 100 || (int)$r >= 200) {
            $this->status_code = $r;
            return false;
        }

        return true;
    }
    function add_user($email, $pass, $fullname, $level, $extra)
    {
        $o = "$email\n$pass\n$fullname\n$level\n$extra";
        if (false == $this->send("USERADD ".strlen($o)))
            return false;
        $this->raw_send($o);
        if (false == $r = $this->getline())
            return false;
        if ((int)$r < 100 || (int)$r >= 200) {
            $this->status_code = $r;
            return false;
        }

        return true;
    }

    function get_config()
    {
        return $this->get_sized_reply("SHOW-CONFIG");
    }

    function session_report($id)
    {
        return $this->get_sized_reply("SESSION-REPORT $id");
    }

    function list_slaves()
    {
        return $this->get_sized_reply("LIST-SLAVES 0");
    }

    function list_users($start=0, $limit=10)
    {
        return $this->get_sized_reply("LIST-USERS $start $limit");
    }

    function list_sessions()
    {
        return $this->get_sized_reply("LIST-SESSIONS 0 20");
    }

    function list_input()
    {
        return $this->get_sized_reply("LIST-INPUT");
    }

    function slave_info($slave)
    {
        return $this->get_sized_reply("SLAVE-INFO $slave");
    }

    function client_info($hash)
    {
        return $this->get_sized_reply("CLIENT-INFO $hash");
    }

    function session_info($id)
    {
        return $this->get_sized_reply("SESSION-INFO $id");
    }

    function list_clients($slave)
    {
        return $this->get_sized_reply("LIST-CLIENTS ".$slave);
    }

    function kill_all($id)
    {
        return $this->send("KILL-ALL $id");
    }

    function system_info()
    {
        return $this->get_sized_reply("SYSTEM-INFO");
    }

    private function raw_send($msg)
    {
        return @fwrite($this->fp, "$msg");
    }

    private function send($msg)
    {
        return @fwrite($this->fp, "$msg\n");
    }

    private function getline()
    {
        return @fgets($this->fp);
    }

    /**
     * Send 'command' to the server, expect a reply back in
     * the form of:
     * <status> <size>\n
     * Where 'size' is the size in bytes of the whole buffer
     * returned by the server, this buffer is returned to
     * the caller.
     */
    private function get_sized_reply($command)
    {
        $this->send($command);
        $r = $this->getline();
        $r = explode(' ', $r);
        $len = (int)@$r[1];
        $status = (int)@$r[0];
        if ($status != 100)
            return false;
        $count = 0;
        $x = "";
        do {
            $b = @fread($this->fp, $len-$count);
            $count += strlen($b);
            $x .= $b;
        } while ($count < $len);
        return $x;
    }
}
?>
