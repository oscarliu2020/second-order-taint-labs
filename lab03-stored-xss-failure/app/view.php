<?php
/* Request #2 — read it back and render it. This is the Stored XSS.
 * The echoed string is a BRAND NEW zend_string from disk → unmarked →
 * the Lab 2 echo sink stays silent. That silence is the bug we must fix. */
$c = file_get_contents('/data/note.txt');
echo $c;      // should be flagged... but isn't (yet). Fixed in Lab 4.
echo "\n";
