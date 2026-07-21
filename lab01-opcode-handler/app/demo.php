<?php
/* Lab 1 target (runs fine on CLI).
 * With rasplab loaded, every hooked opcode gets dumped to stderr.
 *
 * Heads-up: strlen()/count() get compiled to *specialized* opcodes
 * (ZEND_STRLEN…), not DO_FCALL. So we use strtoupper() AND consume its
 * return value — that's what actually emits a call opcode. */

$name     = "AIS3";                    // ZEND_ASSIGN
$greeting = "Hello, " . $name;         // ZEND_CONCAT (+ ASSIGN)
echo $greeting . "\n";                 // ZEND_CONCAT + ZEND_ECHO
echo strtoupper($greeting) . "\n";     // ZEND_DO_ICALL + ZEND_CONCAT + ZEND_ECHO
