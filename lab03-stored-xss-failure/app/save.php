<?php
/* Request #1 — persist attacker-controlled data to "storage" (a file here). */
$c = $_GET['c'] ?? '';
file_put_contents('/data/note.txt', $c);   // tainted write → [STORE-TAINT]
echo "saved\n";
