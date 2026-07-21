<?php
/* service-a — write attacker data to the shared store; rasplab SETEX's the
 * taint id into Redis under a location-derived key. */
$c = $_GET['c'] ?? '';
file_put_contents('/data/note.txt', $c);
echo "saved by service-a\n";
