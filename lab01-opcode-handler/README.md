# Lab 1 — Opcode Handler

> **Mission**: write your first Zend Extension and wedge into the VM's dispatch
> loop so you can watch every opcode fly by. This is the beachhead — every later
> lab does its taint work *inside this same handler*.

---

## Objectives

By the end you can answer:

1. How does PHP source become opcodes that the Zend VM dispatches one by one?
2. How does `zend_set_user_opcode_handler()` let us cut in front of an opcode?
3. Why do we return `ZEND_USER_OPCODE_DISPATCH` after peeking?

---

## Background (30-second version)

```
PHP source ──compile──▶ opcodes ──▶ Zend VM dispatches a handler per op ──▶ run
```

Every opcode (`ZEND_ECHO`, `ZEND_CONCAT`, …) has a default handler. Call
`zend_set_user_opcode_handler(op, fn)` and the VM will run **your** `fn` first.
Do your thing, return `ZEND_USER_OPCODE_DISPATCH`, and Zend then runs the
**original** handler — so behaviour is unchanged, but you saw everything. That
"see everything, change nothing" hook *is* the core RASP trick.

---

## Your job

Open `ext/rasplab.c` and fill two TODOs:

- **`TODO(lab1-1)`** — in `tl_trace_handler()`, dump `opcode name + line` to **stderr**.
  Expected format (`verify.sh` greps it):
  ```
  [opcode] ZEND_ECHO         line=5
  ```
- **`TODO(lab1-2)`** — in `PHP_MINIT_FUNCTION`, register `tl_trace_handler` for
  every opcode in `tl_watch[]`.

Everything else (`config.m4`, header, module table, MSHUTDOWN teardown) is done.

---

## Run it

```bash
cd lab01-opcode-handler

# 1) Fill both TODOs in ext/rasplab.c

# 2) Build (phpize + make + load, inside Docker)
docker build -t rasplab-lab1 .

# 3) Run the target (opcode trace on stderr)
docker run --rm rasplab-lab1

# 4) Grade yourself
./verify.sh
```

Expected trace (exact count/order varies by PHP build):

```
[opcode] ZEND_ASSIGN      line=8
[opcode] ZEND_CONCAT      line=9
[opcode] ZEND_ASSIGN      line=9
[opcode] ZEND_CONCAT      line=10
[opcode] ZEND_ECHO        line=10
[opcode] ZEND_DO_ICALL    line=11
[opcode] ZEND_CONCAT      line=11
[opcode] ZEND_ECHO        line=11
```

`./verify.sh` prints `✓ PASS` when you're done.

---

## Hints

<details>
<summary>Hint 1: grabbing the opcode name</summary>

`zend_get_opcode_name(opline->opcode)` returns e.g. `"ZEND_ECHO"`.
Line number is `opline->lineno` (an int).
</details>

<details>
<summary>Hint 2: why stderr, not stdout?</summary>

`echo` / `php_printf` go to the program's stdout — you'd corrupt its real
output. Tracing is side-band telemetry: `fprintf(stderr, ...)`. `verify.sh`
reads stderr too.
</details>

<details>
<summary>Hint 3: the registration loop</summary>

```c
for (size_t i = 0; i < sizeof(tl_watch)/sizeof(tl_watch[0]); i++)
    zend_set_user_opcode_handler(tl_watch[i], tl_trace_handler);
```
</details>

<details>
<summary>Still stuck?</summary>

Ask your instructor for the reference solution — but understand each line before
you move on.
</details>

---

## Why you might see fewer/other opcodes than expected

- `strlen()`, `count()`, `is_*()` get compiled to **specialized opcodes**
  (`ZEND_STRLEN`, …), not calls — that's why the demo uses `strtoupper()`.
- A call isn't a single opcode: internal funcs → `ZEND_DO_ICALL`, userland →
  `ZEND_DO_UCALL`, by-name → `ZEND_DO_FCALL`. We hook the whole family.
- Unused-result pure calls can be optimized away entirely — consume the return
  value or it vanishes.

---

## Gotchas

| Symptom | Cause |
|---------|-------|
| build fails: `undefined ZEND_ECHO` | missing `#include "zend_vm_opcodes.h"` |
| `php -m` has no rasplab | `.ini` not loaded, or static build (no `get_module`) → use `=shared` |
| no trace at all | `TODO(lab1-2)` didn't register, or wrong opcode |
| trace shows but page output is mangled | you wrote to stdout — switch to stderr |

---

## Checklist

- [ ] `docker build` succeeds and prints `rasplab loaded OK`
- [ ] `docker run` shows `[opcode] ...` trace
- [ ] `./verify.sh` prints `✓ PASS`
- [ ] you can explain what `ZEND_USER_OPCODE_DISPATCH` does

➡️ Next: **Lab 2 — First Order Taint**, where this handler starts making taint decisions.
