<?php
// Tiny vulnerable app demonstrating a SECOND-ORDER SQL injection.
//   POST /note   (note=...)  -> stored SAFELY (parameterised)         [request #1]
//   GET  /render?id=N        -> stored note is re-queried UNSAFELY    [request #2]
$pdo = new PDO('sqlite:/data/app.db');
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_SILENT);
$path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);

if ($path === '/note' && $_SERVER['REQUEST_METHOD'] === 'POST') {
    $note = $_POST['note'] ?? '';
    $st = $pdo->prepare('INSERT INTO notes(body) VALUES(:b)');  // safe store
    $st->execute([':b' => $note]);
    header('Content-Type: application/json');
    echo json_encode(['id' => (int)$pdo->lastInsertId()]);
    exit;
}
if ($path === '/render') {
    $id = (int)($_GET['id'] ?? 0);                              // int-cast: safe
    $row = $pdo->query('SELECT body FROM notes WHERE id=' . $id)->fetch();
    $body = $row ? $row['body'] : '';
    // *** VULN: the STORED note is concatenated into a new query (second-order) ***
    $sql = "SELECT title FROM articles WHERE tag = '$body'";
    $res = $pdo->query($sql);
    header('Content-Type: text/plain');
    if ($res) { foreach ($res as $r) echo $r['title'] . "\n"; }
    else { echo "query error\n"; }
    exit;
}
echo "endpoints: POST /note (note=...), GET /render?id=N\n";
