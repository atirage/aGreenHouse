<?php
    define("SENSORS", 1);
    define("ACTUATORS", 2);
    define("TIMEZONE", 'Europe/Bucharest');

    function transpose($array)
    {
        $transposed_array = array();
        if ($array) {
            foreach ($array as $row_key => $row) {
                if (is_array($row) && !empty($row)) { //check to see if there is a second dimension
                    foreach ($row as $column_key => $element) {
                        $transposed_array[$column_key][$row_key] = $element;
                    }
                }
                else {
                        $transposed_array[0][$row_key] = $row;
                }
            }
            return $transposed_array;
        }
    }
    
    function create_form($type)
    {
        global $str_from, $str_to;
        global $sqlObj;
        if($type == 'sensor')
        {
            $sql = "SELECT Ind, Location, Type FROM Sensors";
            $post_to = "graphs_sens.php";
        }
        else if($type == 'actuator')
        {
            $sql = "SELECT Ind, Location, Type FROM Actuators";
            $post_to = "graphs_act.php";
        }
        else
        {
            return '';
        }
        $result = $sqlObj->query($sql);
        $form = '<form method="post" action='.$post_to.'> <select name='.$type.'>';
        
        while($res = $result->fetchArray(SQLITE3_ASSOC))
        {
            $form = $form.'<option value="'.$res['Ind'].'"';
            if(isset($_POST[$type]))
            {
                if($_POST[$type] == $res['Ind'])
                {
                    $form = $form.' selected';
                }
                $str_from = $_POST['from'];
                $str_to = $_POST['to'];
            }
            $form = $form.'>'.$res['Location'].'-'.$res['Type'].'</option>';
        }   
        $form = $form.'</select>
                       From: <input type="text" id ="date_from" maxlength="11" size="10" name="from" value="'.$str_from.'">
                       To: <input type="text" id ="date_to" maxlength="11" size="10" name="to" value="'.$str_to.'">';
        if($type == 'actuator')
        {
            $checked = isset($_POST['show_sensor']) ? 'checked' : '';
            $form = $form.'<input type="checkbox" name="show_sensor" '.$checked.'>Show related sensor';
        }
        $form = $form.'<input type = "submit" value = "Show" />
                       </form>';
        return $form;
    }
    
    function get_stored_data($db_id, $table, $from, $to)
    {
        global $sqlObj;
        global $columns;
        global $content;
        if($table == 'Measurements')
        {
            $clmn = 'SensorInd';
        }
        else if($table == 'Activations')
        {
            $clmn = 'ActuatorInd';
        }
        else
        {
            return '';
        }
        $sql = "SELECT Value, Unit, TimeStamp FROM '$table' 
                    WHERE $clmn='$db_id' AND
                    TimeStamp BETWEEN '$from' AND '$to' ORDER BY Unit, Timestamp";
        $result = $sqlObj->query($sql);
        //setup lines
        $time = array();
        $final = array(array());
        $val = array();
        $first = true;
        $last_unit = '';
        while($res = $result->fetchArray(SQLITE3_ASSOC))
        {
            if($res['Unit'] != $last_unit)
            {
                $unit = '';
                if($res['Unit'] == 'oC')
                {
                	$unit = "\u00BAC";
                }
                else
                {
                	$unit = $res['Unit'];
                }
                $columns = $columns.' data.addColumn("number", "'.$unit.'");';
                if($last_unit != '')
                {
                    $first = false;
                    $final[0] = $time;
                    $final[] = $val;
                    $val = array();
                }
                $last_unit = $res['Unit'];
            }
            if($first == true)
            {       
               $time[] = 'new Date('.(string)((int)$res["TimeStamp"] * 1000).')';
            }
            $val[] = $res['Value'];
        }
        if($first == true)
        {
            $final[0] = $time;
        }
        $final[] = $val;
        $final = transpose($final);
        return $final;
    }
    
    function create_login_form()
    {
        return '<form name="login" action="ctrl_act.php" method="POST">
                        <div align="center">
                            <input type="password" size="15" name="pwd">
                            <input type="submit" value="Submit">
                        </div>
                    </form>';
    }
    
    function create_ctrl_form($act, $i, $slctd)
    {
     /*CMD_RELEASE = 0
        CMD_ACTIVATE = 1
        CMD_DEACTIVATE = 2*/
        $checked0 = '';
        $checked1 = '';
        $checked2 = '';
        if($slctd=='0') {$checked0= 'checked';}
        else if($slctd=='1') {$checked1= 'checked';}
        else if($slctd=='2') {$checked2= 'checked';}
        else{}
        $form = '<form action="ctrl_act.php" method="POST">
                    <input type="hidden" name="Tab" value="'.$i.'">
                    <input type="hidden" name="ActId" value="'.$act['Ind'].'">
                    <input type="radio" name="Cmd" value="1"'.$checked1.'>On
                    <input type="radio" name="Cmd" value="2"'.$checked2.'>Off
                    <input type="radio" name="Cmd" value="0"'.$checked0.'>Release<br>';
        $substr = explode('|', $act['ControlFunc'], 2);
        if($act['Type'] == 'ON_OFF_TIME')
        {
            $form = $form.'Pattern<input type="text" size="15" name="Field1" value="'.$substr[0].'">';
            $form = $form.'Period<input type="text" size="3" name="Field2" value="'.$substr[1].'">';
        }
        else if(($act['Type'] == 'ON_OFF_FDB_T') || ($act['Type'] == 'ON_OFF_FDB_H'))
        {
            $form = $form.'Threshold<input type="text" size="3" name="Field1" value="'.$substr[0].'">';
            $form = $form.'Hysteresis<input type="text" size="3" name="Field2" value="'.$substr[1].'">';
        }
        else
        {/*dummy, if no ctrl fct*/
            $form = $form.'<input type="hidden" name="Field1" value="0">';
            $form = $form.'<input type="hidden" name="Field2" value="0">';
        }
        return $form.'<br><input type="submit" value="Execute"></form>';
    }
    
    function getCurrentState($obj)
    {
        date_default_timezone_set(TIMEZONE);
        /* connect to the db */
        $sqlObj = new SQLite3('../../../var/lib/sqlite3/Greenhouse');
        if($obj == SENSORS)
        {
            $sql = "SELECT Sensors.Location, Value, Unit, MAX(TimeStamp) AS Time FROM Measurements INNER JOIN Sensors ON SensorInd=Sensors.Ind GROUP BY SensorInd, Unit";
        }
        else if($obj == ACTUATORS)
        {
            $sql = "SELECT Actuators.Location, Value, Unit, MAX(TimeStamp) AS Time FROM Activations INNER JOIN Actuators ON ActuatorInd=Actuators.Ind GROUP BY ActuatorInd";
        }
        else
        {
            $sql = ''; 
        }
        $result = $sqlObj->query($sql);
        $php_array = array(array());
        $i = 0;
        while($res = $result->fetchArray(SQLITE3_ASSOC))
        {
            $php_array[$i][] = $res['Location'];
            if(($obj == ACTUATORS) && ($res['Unit'] == 'NONE'))
            {
                $php_array[$i][] = ($res['Value'] == 1)?'ON':'OFF'; 
            }
            else if(($obj == SENSORS) && ($res['Unit'] == 'oC'))
            {
                $php_array[$i][] = $res['Value']." \u00BAC";
            }
            else
            {
                $php_array[$i][] = $res['Value']." ".$res['Unit']; 
            }
            $php_array[$i][] = date('Y-m-d H:i:s', $res['Time']);
            $i++;
        }
        $sqlObj->close();
        return $php_array;
    }
?>
