# Second Order Vulnerability Detection — Hands-On Labs

Build a "Runtime Persistent Taint Tracking" engine from scratch. You'll write your
own PHP Zend Extension (`rasplab`) and grow it, lab by lab, from "print an opcode"
into a full RASP that tracks taint **across requests** and ships traces to Python / an LLM.

> Companion deck: *Second Order Vulnerability Detection · Runtime Persistent Taint Tracking*

---

## The kill chain (a.k.a. lab map)

| Lab | Topic | What you ship | Concept |
|-----|-------|---------------|---------|
| **Lab 1** | Opcode Handler | Hijack the VM, dump every opcode it runs | Zend VM / `zend_set_user_opcode_handler` |
| **Lab 2** | First Order Taint | Mark `$_GET` as tainted, scream on `echo` | Source → Sink |
| **Lab 3** | Stored XSS Failure | Watch your detector go blind across requests | broken execution graph |
| **Lab 4** | File Backend | Persist taint keyed by location, recover on read | Persist Taint |
| **Lab 5** | Redis Backend | Taint that survives requests **and services** | External Tracking |
| **Lab 6** | Python Correlation | Reconstruct the provenance graph, catch second-order | Provenance Graph |
| **Lab 7** | LLM Analysis | Let an LLM explain the trace and cook a PoC | Report |

All seven shipped — the C labs (1–5) grow one extension (`rasplab`) opcode-hook →
function-hook → file backend → Redis; the Python labs (6–7) run standalone.

---

## Layout

Docker is the only thing you need on your host — no PHP required.
Every lab is a self-contained box:

```
labXX-name/
├── README.md        # the brief: objective, steps, TODOs, how to verify
├── ext/             # Zend extension skeleton (TODOs are yours to fill)
│   ├── config.m4
│   ├── php_rasplab.h
│   └── rasplab.c
├── app/             # target PHP the extension runs against
├── Dockerfile
└── verify.sh        # one-shot: builds, runs, greps, tells you PASS/FAIL
```

Reference solutions and instructor notes live **outside** `labs/` (in a separate
`solutions/` tree) so this folder can be handed to students as-is.

---

## Quickstart (Lab 1)

```bash
cd lab01-opcode-handler

# 1) Open ext/rasplab.c and fill every TODO(lab1-*)

# 2) Build (compiles the extension + loads it, all inside Docker)
docker build -t rasplab-lab1 .

# 3) Run the target; opcode trace lands on stderr
docker run --rm rasplab-lab1

# 4) Grade yourself
./verify.sh
```

`✓ PASS` means the lab is owned. Stuck? Read the **Hints** in the lab's `README.md`.

---

## Fill-in philosophy

We hollow out **only the interesting bits** — the taint decision / propagation /
serialization logic. All the plumbing (build system, module boilerplate, operand
fetching) is handed to you so you spend brain cycles on concepts, not autotools.

Every blank is tagged:

```c
/* ============================================================
 * TODO(labN-M): one line on what to implement
 *   - hint 1
 *   - hint 2 / the API you want
 * ============================================================ */
```
