<?php
/* service-b — a DIFFERENT service. It never saw $_GET, but it reads the shared
 * note and Redis hands back the taint → echo must ALERT. */
$c = file_get_contents('/data/note.txt');
echo $c;
echo "\n";
