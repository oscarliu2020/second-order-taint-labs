<?php
/* Request #1 — persist attacker data; rasplab EXPORTs a taint record. */
$c = $_GET['c'] ?? '';
file_put_contents('/data/note.txt', $c);
echo "saved\n";
