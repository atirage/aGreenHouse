<!DOCTYPE html>
<html>
    <head>
        <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
        <title><?php echo $title; ?></title>
        <link rel="stylesheet" type="text/css" href="Styles/Stylesheet.css" />
        <link rel="stylesheet" href="//code.jquery.com/ui/1.10.4/themes/smoothness/jquery-ui.css">
        <script type="text/javascript" src="https://www.google.com/jsapi"></script>
        <script src="//code.jquery.com/jquery-1.10.2.js"></script>
        <script src="//code.jquery.com/ui/1.10.4/jquery-ui.js"></script>
        <script type="text/javascript"><?php echo $jscript; ?></script>
    </head>
    <body>
        <div id="wrapper">
            <div id="banner">
            </div>
            
            <nav id="navigation">
                <ul id="nav">
                    <li><a href="index.php">Home</a></li>
                    <li><a href="current.php">Current State</a></li>
                    <li><a href="graphs_sens.php">Sensor Graphs</a></li>
                    <li><a href="graphs_act.php">Actuator Graphs</a></li>
                    <li><a href="ctrl_act.php">Manual Control</a></li>
                </ul>
            </nav>
            
            <div id="content_area">
                <?php 
                echo $content;
                ?>
            </div>
            
            <?php
            if($show_tabs == true)
            {            
                echo '<div id="tabs">'.$tabs.'</div>';
            }
            if($show_graphs == true)
            {
                echo '<div id="chart_area1">
                         </div>
                         <div id="chart_area2">
                         </div>';
            }
            ?>
            <footer>
                <p>All rights reserved Hydro Rose Srl</p>
            </footer>
        </div>
    </body>
</html>
