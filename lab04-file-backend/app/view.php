<?php
/* Request #2 — read it back; rasplab RECOVERs taint → echo must now ALERT. */
$c = file_get_contents('/data/note.txt');
echo $c;
echo "\n";
