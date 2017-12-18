<?php

/* usage: curl --user atirage:januar14  -X POST -d "Cmd=value" http://192.168.0.150/monitor/kodi.php */
$title = "Wifi Led control";
$errmsg = '';
$sqlObj = new SQLite3('/var/lib/sqlite3/Greenhouse');
$Act_Id = "";

if(isset($_POST['Cmd']))
{/* submit */
	//get actuator id
	$sql = "SELECT Ind FROM Actuators WHERE Type = 'SOCKET_CTRL_LED'";
	do{
		$result = $sqlObj->query($sql);
		if($result instanceof Sqlite3Result)
		{
			while($res = $result->fetchArray(SQLITE3_ASSOC))
			{
				$Act_Id = $res;
				break;
			}
			break;
		}
		sleep(1);
	}while(1);

    //write to FIFO, wait for Feedback
    exec("pgrep aGreenHouse", $output, $return);
    if($return == 0)
    {//backend is running
        if(($fifo = fopen('/var/lib/aGreenHouse/cmdFIFO', 'w')) == false)
        {
            $errmsg = "Error opening command FIFO!";
        }
        else
        {
            $cmdStr = $Act_Id.' '.$_POST['Cmd']."\n";
            if(fwrite($fifo, $cmdStr) < strlen($cmdStr))
            {
                $errmsg = "Error writing to FIFO!";
                fclose($fifo);
            }
            else
            {
                fclose($fifo);
                if(($fifo = fopen('/var/lib/aGreenHouse/respFIFO', 'r')) == false)
                {
                    $errmsg = "Error opening response FIFO!";
                }
                else
                {
                    stream_set_timeout($fifo, 2);
                    $respStr = fread($fifo, 2);
                    $info = stream_get_meta_data($fifo);
                    //for debugging purposes: $errmsg = 'response received!';
                    fclose($fifo);
                    if ($info['timed_out'])
                    {
                        $errmsg = 'No response!';
                    } 
                    else
                    {
                        if($respStr != 'OK')
                        {
                            $errmsg = "Wrong response!";
                        }
                    }
                }
            }
        }
	}
    else
    {//backend not running
        $errmsg = "Actuator controlling server not running!";
    }
}
?>

<!DOCTYPE html>
<html>
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
        <title><?php echo $title; ?></title>
        <link rel="stylesheet" type="text/css" href="Styles/Stylesheet.css" />
        <link rel="stylesheet" href="//code.jquery.com/ui/1.10.4/themes/smoothness/jquery-ui.css">
    </head>
    <body>
		<form action="kodi.php" method="POST">
			<input type="radio" name="Cmd" value="1">Dim In <br>
                        <input type="radio" name="Cmd" value="2" checked>Dim Out <br>
			<input type="submit" value="Execute"> 
		</form>
		<br><font color="red"><?php echo $errmsg; ?></font>
    </body>
</html>