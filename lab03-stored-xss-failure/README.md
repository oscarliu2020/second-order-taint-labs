# Lab 3 — Stored XSS Failure

> **Mission**: watch your Lab 2 detector go **blind**. Push tainted data into
> storage in one request, read it back in the next, and render it. The echo sink
> stays silent — because taint died with request #1. This *is* the second-order
> problem, live.

```
save.php:  $_GET  ─▶ file_put_contents()   #1   [STORE-TAINT]  ✅ we see it enter
view.php:  file_get_contents() ─▶ echo     #2   (no alert)     ❌ we're blind
```

---

## Objectives

1. Hook an **internal function** (not just an opcode) — the second core RASP primitive.
2. Confirm taint reaches a storage sink on write.
3. See *why* the read-back is undetectable: a fresh `zend_string` off disk carries no taint, and the process from request #1 is long gone.

---

## New primitive: function hooking

Opcodes cover operators (`echo`, `.`). To watch library calls
(`file_put_contents`, `mysqli_query`, …) you swap the function's C handler:

```c
zend_function *fn = zend_hash_str_find_ptr(CG(function_table), "file_put_contents", ...);
orig = fn->internal_function.handler;        // stash original
fn->internal_function.handler = ours;        // install ours
...
ours(...) { /* inspect args */ orig(INTERNAL_FUNCTION_PARAM_PASSTHRU); }  // chain
```

The install + chaining is **provided**. You write the inspection.

---

## Your job

Fill **`TODO(lab3-1)`** in `tl_file_put_contents()`: if the data argument is a
tainted string, log

```
[STORE-TAINT] loc=/data/note.txt data="<payload>"
```

Grab args with `ZEND_CALL_ARG(execute_data, n)` (1-based). Everything else
(taint set, `$_GET` marking, echo sink, hook install) is carried over from Lab 2.

---

## Run it

```bash
cd lab03-stored-xss-failure
docker build -t rasplab-lab3 .
docker run --rm -p 8080:8080 rasplab-lab3

# request #1 — plant the payload
curl 'http://localhost:8080/save.php?c=<script>alert(1)</script>'
#   server logs: [STORE-TAINT] loc=/data/note.txt data="<script>alert(1)</script>"

# request #2 — render it back
curl 'http://localhost:8080/view.php'
#   server logs: ...nothing. No [ALERT]. That's the bug.

./verify.sh
```

`verify.sh` passes when **STORE-TAINT fires** *and* **the view echo does NOT alert** —
i.e. you've faithfully reproduced the blind spot.

---

## Hints

<details><summary>Hint: reading call args</summary>

```c
if (ZEND_NUM_ARGS() >= 2) {
    zval *data = ZEND_CALL_ARG(execute_data, 2);
    ZVAL_DEREF(data);
    if (Z_TYPE_P(data) == IS_STRING && tl_is_tainted(Z_STR_P(data))) { ... }
}
```
</details>

<details><summary>Why is view.php blind, exactly?</summary>

Two independent reasons, both fatal:
1. `file_get_contents()` allocates a **new** `zend_string` — different pointer,
   never marked.
2. request #1's worker process (and its `RASP_tainted` table) is **gone**.
Taint that only lives in process memory cannot cross a request. Fix = persist it.
</details>

---

## Checklist

- [ ] `save.php?c=...` logs `[STORE-TAINT]` with the payload
- [ ] `view.php` produces **no** `[ALERT]`
- [ ] `./verify.sh` prints `✓ PASS`
- [ ] you can state the two reasons the read-back is blind

➡️ **Lab 4 — File Backend**: stop losing taint. On write, persist a taint record
keyed by the storage location; on read, recover it and re-mark the string. Then
`view.php` finally screams.
