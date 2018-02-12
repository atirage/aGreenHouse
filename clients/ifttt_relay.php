<?php
$errmsg = '';

if(isset($_POST['Cmd']))
{/* submit */

    if(($_POST['Cmd'] == '1') || ($_POST['Cmd'] == '2')) 
    {/* valid command */
        shell_exec("wget -q -T 3 -O/dev/null --user=atirage --password=januar14 --post-data=Cmd=".$_POST['Cmd']." http://atirageipv6.barney.ro/monitor/kodi.php");
    }
}
?>

<!DOCTYPE html>
<html>
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
        <title> IFTTT IPv6 relay  </title>
        <link rel="stylesheet" type="text/css" href="Styles/Stylesheet.css" />
        <link rel="stylesheet" href="//code.jquery.com/ui/1.10.4/themes/smoothness/jquery-ui.css">
    </head>
    <body>
        <form action="ifttt_relay.php" method="POST">
            <input type="radio" name="Cmd" value="1">Dim In <br>
            <input type="radio" name="Cmd" value="2" checked>Dim Out <br>
            <input type="submit" value="Execute"> 
        </form>
        <br><font color="red"><?php echo $errmsg; ?></font>
    </body>
</html>