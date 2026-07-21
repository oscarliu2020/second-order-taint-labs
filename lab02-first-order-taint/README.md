# Lab 2 — First Order Taint

> **Mission**: turn Lab 1's opcode hook into a minimal taint tracker. Flag
> `$_GET` as a taint **Source**, and fire an alert when it hits the `echo` **Sink**.
>
> ```
> Input ($_GET) ──mark──▶ echo ──detect──▶ ALERT
> ```

---

## Objectives

1. What Source / Sink mean, and why "user input" is the canonical taint source.
2. How to tag a value as tainted at runtime and query it elsewhere.
3. How to grab "the thing being echoed" inside the `ECHO` handler and judge it.

---

## Taint model (this lab's edition)

Dead simple: keep a set of tainted `zend_string*` pointers (`RASP_tainted`).
- **mark**  → `tl_mark(zend_string*)`
- **query** → `tl_is_tainted(zend_string*)`

> ⚠️ Pointer-as-identity is crude. It only answers "tainted: yes/no", carries
> **zero** relationship info, and a freed string's address can be recycled into a
> false positive. That's exactly what the later labs (File/Redis backends,
> provenance graph) exist to fix.

Because we need `$_GET`, this lab runs under PHP's **built-in web server** (CLI has no `$_GET`).

---

## Your job

Open `ext/rasplab.c` and fill two blocks:

- **`TODO(lab2-1)`** in `tl_taint_array()` — walk the array, mark **string** values tainted. (Source)
- **`TODO(lab2-2)`** in `tl_echo_handler()` — if echoing a **tainted string**, print `[ALERT]` to stderr. (Sink)

Provided for you: the taint set, `tl_mark`/`tl_is_tainted`, operand fetch
`tl_get_zval()`, auto-marking of `$_GET`/`$_POST` in `RINIT`, and `ECHO` handler
registration in `MINIT`.

---

## Run it

```bash
cd lab02-first-order-taint

# 1) Fill both TODOs in ext/rasplab.c

# 2) Build + boot the built-in server
docker build -t rasplab-lab2 .
docker run --rm -p 8080:8080 rasplab-lab2      # foreground; logs stream here

# 3) In another terminal, throw a payload
curl 'http://localhost:8080/demo.php?name=<script>alert(1)</script>'

#    Back on the server, you should see:
#    [ALERT] tainted -> ECHO: "<script>alert(1)</script>" (line=13)

# 4) Auto-grade (boots, fires requests, diffs)
./verify.sh
```

Control group: `curl http://localhost:8080/demo.php` (no `name`) →
`$name='guest'` (a constant) → **no alert**. That's the point: we flag *user
input*, not every string.

---

## Hints

<details>
<summary>Hint 1: is-string check + get the zend_string</summary>

```c
if (Z_TYPE_P(val) == IS_STRING) {
    zend_string *s = Z_STR_P(val);
    tl_mark(s);
}
```
</details>

<details>
<summary>Hint 2: why does <code>$x = $_GET['name']; echo $x;</code> still catch?</summary>

String assignment is refcount-shared — `$x` and `$_GET['name']` point at the
**same `zend_string`**. Same pointer ⇒ same taint. That's why pointer-identity
works at all in the easy cases.
</details>

<details>
<summary>Hint 3: so why does <code>echo "hi ".$name;</code> slip through?</summary>

`.` (`ZEND_CONCAT`) mints a **new** `zend_string` — different pointer, unmarked.
This lab deliberately doesn't do propagation; that starts in Lab 3+. For now,
test with a direct `echo $var`.
</details>

<details>
<summary>Stuck</summary>

Ask your instructor for the reference solution. Make sure you get *why*
`tl_get_zval()` handles both `IS_CONST` and the var cases.
</details>

---

## Gotchas

| Symptom | Cause |
|---------|-------|
| no alert ever | `TODO(lab2-1)` didn't mark; or you ran it on CLI (no `$_GET`) → must go through the server |
| even constants alert | `tl_echo_handler` didn't check `IS_STRING`, or treats every echo as tainted |
| build fails `TRACK_VARS_GET` | missing `#include "php_variables.h"` |
| segfault | operand can be NULL / `IS_UNUSED` — guard `op1 == NULL` first |

---

## Checklist

- [ ] a `?name=...` request `[ALERT]`s with the payload
- [ ] a request with no `name` doesn't false-positive
- [ ] `./verify.sh` prints `✓ PASS`
- [ ] you can name **two** limits of this model (no relationships, pointer reuse)

➡️ **Lab 3 — Stored XSS Failure**: deliberately push taint into a DB, read it
back in the *next* request, and watch this first-order detector go blind. That
blind spot is the second-order problem — the thing persistent taint fixes.
