<?php
    include './Include/functions.php';
    //globals
    $title = "Actuator Graph";
    $show_tabs = false;
    $show_graphs = true;
    $tabs = '';
    $sqlObj = new SQLite3('../../../var/lib/sqlite3/Greenhouse');
    $str_from = '';
    $str_to = '';
    $columns = '';
    $content = '<h3>Select an actuator</h3>'.create_form('actuator');
    $jscript = '';
    date_default_timezone_set(TIMEZONE);
    if(isset($_POST['actuator']))
    {
        //Generate chart(s) for selected actuator
        $from = strtotime($str_from.'00:00');
        $to = strtotime($str_to.'23:59');
        $data = get_stored_data($_POST['actuator'], 'Activations', $from, $to);
        $data_filt = array(array());
        //filter for transitions
        if(count($data[0]) == 2)
        {
            $data_filt[0] = $data[0];
            for ($i=1; $i<count($data); $i++)
            {
                if($data[$i][1] != $data[$i - 1][1])
                {//transition
                    $data_filt[] = $data[$i];
                    $data_filt[count($data_filt) - 1][1] = $data[$i - 1][1];
                    $data_filt[] = $data[$i];
                }
            }
        }
        //get timezone offset
        $tz = new DateTimeZone(TIMEZONE);
        $offs = $tz->getOffset(new DateTime("now"));
        $offs = round($offs / 3600);
        //start script
        $js_array = json_encode($data_filt);
        $js_array = str_replace('"', '', $js_array);
        $jscript = '
                    google.load("visualization", "1", {packages:["corechart"]});
                    google.setOnLoadCallback(drawChart);
                    function drawChart() {
                        var data = new google.visualization.DataTable();
                        data.addColumn("datetime", "Time");'.$columns.' data.addRows('.$js_array.');
                        var options = {
                                        title: "Actuator Values",
                                        height: 400,
                                        colors:["#224222", "#70DB70", "#007A29"]
                                      };
                        var formatter = new google.visualization.DateFormat({formatType: "medium", timeZone: '.$offs.'});
                        formatter.format(data, 0);
                        var chart = new google.visualization.LineChart(document.getElementById("chart_area1"));
                        chart.draw(data, options);
                    }';
        if(isset($_POST['show_sensor']))
        {
            $columns = '';
            $sql = "SELECT SensorInd FROM Actuators 
                    WHERE Ind='".$_POST['actuator']."'";
            $result = $sqlObj->query($sql);
            $res = $result->fetchArray(SQLITE3_ASSOC);
            $data = get_stored_data($res['SensorInd'], 'Measurements', $from, $to);
            $js_array = json_encode($data);
            $js_array = str_replace('"', '', $js_array);
            //start script
            $jscript = $jscript. '
                        google.setOnLoadCallback(drawChartSens);
                        function drawChartSens() {
                            var data = new google.visualization.DataTable();
                            data.addColumn("datetime", "Time");'.$columns.' data.addRows('.$js_array.');
                            var options = {
                                            title: "Sensor Values",
                                            height: 400,
                                            colors:["#224222", "#70DB70", "#007A29"]
                                          };
                            var chart = new google.visualization.LineChart(document.getElementById("chart_area2"));
                            chart.draw(data, options);
                        }'; 
        }
    }
    else 
    {/*Page is loaded for the first time*/
    }
    
    $jscript =  $jscript.'$(function() 
                                    { 
                                       $( "#date_from" ).datepicker();
                                       $( "#date_to" ).datepicker();
                                    });';
    $sqlObj->close();
    unset($sqlObj);
    include 'Template.php';
?>
