<?php
// One-time DB setup. Flag comes from env (randomised by entrypoint) and lives
// ONLY in the secrets table — the sole way out is the second-order SQLi.
$p = new PDO('sqlite:/data/app.db');
$p->exec("CREATE TABLE IF NOT EXISTS notes(id INTEGER PRIMARY KEY, body TEXT)");
$p->exec("CREATE TABLE IF NOT EXISTS articles(id INTEGER PRIMARY KEY, tag TEXT, title TEXT)");
$p->exec("DELETE FROM articles");
$p->exec("INSERT INTO articles(tag,title) VALUES('news','Hello World')");
$p->exec("CREATE TABLE IF NOT EXISTS secrets(flag TEXT)");
$p->exec("DELETE FROM secrets");
$st = $p->prepare("INSERT INTO secrets(flag) VALUES(:f)");
$st->execute([':f' => getenv('FLAG')]);
