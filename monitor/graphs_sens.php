<?php
    include './Include/functions.php';  
    //globals
    $title = "Sensor Graph";
    $tabs = '';
    $show_tabs = false;
    $show_graphs = true;
    $str_from = '';
    $str_to = '';
    $columns = '';
    $sqlObj = new SQLite3('../../../var/lib/sqlite3/Greenhouse');
    $content = '<h3>Select a sensor</h3>'.create_form('sensor');
    $jscript = '';
    date_default_timezone_set(TIMEZONE);
    if(isset($_POST['sensor']))
    {
        //Generate chart(s) for selected sensor
        $data = get_stored_data($_POST['sensor'], 'Measurements', strtotime($str_from.'00:00'), strtotime($str_to.'23:59'));
        $js_array = json_encode( $data);
        $js_array = str_replace('"', '', $js_array);
        //get timezone offset
        $tz = new DateTimeZone(TIMEZONE);
        $offs = $tz->getOffset(new DateTime("now"));
        $offs = round($offs / 3600);
        //start script
        $jscript = '
                    google.load("visualization", "1", {packages:["corechart"]});
                    google.setOnLoadCallback(drawChart);
                    function drawChart() {
                        var data = new google.visualization.DataTable();
                        data.addColumn("datetime", "Time"); '.$columns.' data.addRows('.$js_array.');
                        var options = {
                                        title: "Sensor Values",
                                        height: 400,
                                        colors:["#224222", "#70DB70", "#007A29"]
                                      };
                        var formatter = new google.visualization.DateFormat({formatType: "medium", timeZone: '.$offs.'});
                        formatter.format(data, 0);
                        var chart = new google.visualization.LineChart(document.getElementById("chart_area1"));
                        chart.draw(data, options);
                    }';
    }
    else 
    {
        //Page is loaded for the first time
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
