<?php
include './Include/functions.php';

$title = "Current state";
$show_tabs = false;
$show_graphs = true;
$content =  '<h3>Latest Sensor Readings and Activation States</h3>';
$tabs = '';
$cpu_temp = "NA";
$sig_qlty = "NA";
$rssi_dBm = "-";

//get CPU temp
if(($fd = fopen('/sys/class/thermal/thermal_zone0/temp', 'r')) != false)
{
    $cpu_temp = fread($fd,filesize('/sys/class/thermal/thermal_zone0/temp'));
    fclose($fd);
}
if($cpu_temp != "NA")
{
    $temp = (float)$cpu_temp/1000;
}
else
{
    $temp = $cpu_temp;
}

//get GSM signal quality - TODO: not always ttyUSB1!
exec('comgt -d /dev/modemZTEalt sig', $rssi, $ret_val);
//$rssi = 'Signal Quality: 24,99';

if(!$ret_val)
{
    $rssi = intval(substr($rssi[0], strpos($rssi[0], ':') + 1));
    $rssi_dBm = ($rssi * 2) - 113;
    if($rssi > 25)
    {
        $sig_qlty = "Excellent";
    }
    else if($rssi > 19)
    {
        $sig_qlty =  "Good";
    }
    else if($rssi > 13)
    {
        $sig_qlty =  "Average";
    }
    else if($rssi > 7)
    {
        $sig_qlty =  "Low";
    }
    else if($rssi > 0)
    {
        $sig_qlty =  "Very low";
    }
    else $sig_qlty = "No signal";
}
$data = shell_exec('uptime');
$uptime = explode(' up ', $data);
$uptime = explode(',', $uptime[1]);
$uptime = $uptime[0].', '.$uptime[1];

$content =  $content .'<br><b>CPU temperature:</b> '.$temp.' &degC<br>';
$content =  $content .'<br><b>GSM Signal Quality:</b> ' .$rssi_dBm.' dBm ('.$sig_qlty.')<br>';
$content =  $content .'<br><b>System Uptime:</b> ' .$uptime;

$js_array_sens = json_encode(getCurrentState(SENSORS), JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
$js_array_sens = stripslashes($js_array_sens); //needed due to unicode char in the string
$js_array_act = json_encode(getCurrentState(ACTUATORS), JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE);
$jscript = 'google.load("visualization", "1", {packages:["table"]});
                function drawChartSens() {
                    var data = new google.visualization.DataTable();
                    data.addColumn("string", "Location");
                    data.addColumn("string", "Value");
                    data.addColumn("string", "Time");
                    data.addRows('.$js_array_sens.');
                    var table = new google.visualization.Table(document.getElementById("chart_area1"));
                    table.draw(data, {showRowNumber: false});
                }
                function drawChartAct() {
                    var data = new google.visualization.DataTable();
                    data.addColumn("string", "Location");
                    data.addColumn("string", "Value");
                    data.addColumn("string", "Time");
                    data.addRows('.$js_array_act.');
                    var table = new google.visualization.Table(document.getElementById("chart_area2"));
                    table.draw(data, {showRowNumber: false});
                }
                google.setOnLoadCallback(drawChartSens);
                google.setOnLoadCallback(drawChartAct);';
                    
include 'Template.php';
/*

#!/bin/bash
cpuTemp0=$(cat /sys/class/thermal/thermal_zone0/temp)
cpuTemp1=$(($cpuTemp0/1000))
cpuTemp2=$(($cpuTemp0/100))
cpuTempM=$(($cpuTemp2 % $cpuTemp1))

echo CPU temp"="$cpuTemp1"."$cpuTempM"'C"
echo GPU $(/opt/vc/bin/vcgencmd measure_temp)

*/

?>

