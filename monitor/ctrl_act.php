<?php
include './Include/functions.php';
    
$content = '';
$title = "Actuator control";
$tabs = '';
$show_tabs = true;
$show_graphs = false;
$jscript = '';
$tab_ind = 0;
$sqlObj = new SQLite3('/var/lib/sqlite3/Greenhouse');
$grant = false;
$errmsg = '';
$actuators = array();
$sel = array();

//get actuators
$sql = "SELECT Ind, Location, Type, ControlFunc FROM Actuators";
do{
	$result = $sqlObj->query($sql);
	if($result instanceof Sqlite3Result)
	{
		while($res = $result->fetchArray(SQLITE3_ASSOC))
		{
    		$actuators[]=$res;
    		$sel[]='0';
		}
		break;
	}
	sleep(1);
}while(1);

if(isset($_POST['Tab']))
{/* submit from tab */
    $grant = true;
    $tab_ind = $_POST['Tab'];
    //validate inputs, write to FIFO, wait for Feedback
    if(is_numeric($_POST['Field1']) && is_numeric($_POST['Field2']))
    {
        $newCmd = $_POST['Field1'].'|'.$_POST['Field2'];
        if($actuators[$_POST['Tab']]['ControlFunc'] != $newCmd)
        {//control func changed
            $sql ="UPDATE Actuators set ControlFunc ='".$newCmd."' where Ind='".$_POST['ActId']."'";
            $ret = $sqlObj->exec($sql);
            if(!$ret)
            {
                $errmsg = 'Update of control function failed!';
                $newCmd = '';
            }
            else
            {
                $actuators[$_POST['Tab']]['ControlFunc'] = $newCmd;
            }
        }
        else
        {
            $newCmd = '';
        }
        exec("pgrep aGreenHouse", $output, $return);
        if($return == 0)
        {//backend is running
            if(($fifo = fopen('/var/lib/aGreenHouse/cmdFIFO', 'w')) == false)
            {
                $errmsg = "Error opening command FIFO!";
            }
            else
            {
                $cmdStr = $_POST['ActId'].' '.$_POST['Cmd'];
                if($newCmd != '')
                {
                    $cmdStr = $cmdStr.' '.$newCmd;
                }
                $cmdStr = $cmdStr."\n";
                
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
    else
    {//not numeric
        $errmsg = "Only numeric values!";
    }
}
else
{
    if(isset($_POST['pwd']))
    {
        if($_POST['pwd'] == 'kisk3rtesz')
        {
            $grant = true;
        }
        else
        {
            $errmsg = 'Incorrect password!';
        }
    }
}

if($grant == true)
{//access granted
    $content = '<h3>Select and control actuator</h3>
                      <font color="red">'.$errmsg.'</font>';
    $i = 0;
    $links = '<ul>';
    while($i < count($actuators))
    {
        if(isset($_POST['Cmd']) &&
           ($_POST['Tab'] == $i) &&
           ($errmsg == '') 
          )
        {
            $sel[$i] = $_POST['Cmd'];
        }
        $links = $links.'<li><a href="#tabs-'.$i.'">'.$actuators[$i]['Ind'].'-'.$actuators[$i]['Location'].'</a></li>';
        $tabs = $tabs.'<div id="tabs-'.$i.'">'.create_ctrl_form($actuators[$i], $i, $sel[$i]).'</div>';
        $i++;
    }
    $links = $links.'</ul>';
    $tabs = $links.$tabs;
    
    $jscript = '$(function() {
                        $( "#tabs" ).tabs({ active: '.$tab_ind.'});
                    });';
}
else
{
    $content = '<h3>Enter password to control actuators</h3>
                      <font color="red">'.$errmsg.'</font>'.create_login_form();
}
$sqlObj->close();
unset($sqlObj);
include 'Template.php';
?>

