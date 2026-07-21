<?php
/* Lab 2 target — hit it through the built-in server so $_GET is populated.
 *
 *   /demo.php?name=<script>alert(1)</script>
 *
 * Expected: `echo $name` fires an [ALERT] because $name came from $_GET (tainted).
 *           `echo "Profile page"` is a constant → stays quiet.
 */

$name = $_GET['name'] ?? 'guest';

echo "Profile page\n";   // constant — must NOT alert
echo $name;              // user input — must trigger [ALERT]
echo "\n";
