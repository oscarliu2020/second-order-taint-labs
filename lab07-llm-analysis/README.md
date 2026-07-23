# Lab 7 — LLM Analysis

> **Mission**: close the loop. Take one correlated chain from Lab 6 and have an
> LLM explain the bug in plain English and draft a PoC — turning a raw trace into
> something a human (or a ticket) can act on. Ships with an **offline mock** so it
> runs with zero API keys; flip on a real model when you want.

```
finding (Lab 6)  ─▶  build prompt  ─▶  LLM  ─▶  { explanation, poc }  ─▶  report
```

---

## Objectives

1. Turn a structured finding into an effective **prompt** (facts in, JSON out).
2. Parse a model response robustly and render a report.
3. Understand the mock/real split — CI stays deterministic, prod uses a real LLM.

---

## Backend: opencode (course default)

This class uses the **course's unified setup**: `opencode` + the course model
**`ais3/gemma-4-26b`** + **your own API key** (configured once in opencode).
No vendor SDK, no key in code — `analyze.py` just shells out to `opencode run`.

It's the default automatically: whenever the `opencode` CLI is on your PATH,
`analyze.py` routes through it (using your configured default model). On a box
without opencode (like the CI Docker image) it falls back to a deterministic
offline **MockLLM**.

```bash
# one-time: configure opencode with YOUR key, and set your DEFAULT model to
# ais3/gemma-4-26b (its provider prefix is your own opencode config).
opencode auth login

# then just run — no model flag needed; opencode uses your default:
python3 app/analyze.py app/finding.json

# overrides if needed:
#   LLM_BACKEND=mock python3 app/analyze.py app/finding.json          # force mock
#   OPENCODE_MODEL=<your-provider>/ais3/gemma-4-26b python3 app/analyze.py app/finding.json
```

The MockLLM answers using the `KEY: value` facts it finds **in your prompt** — so
if your prompt omits the sink/payload it can't mention them and the grader fails.
That makes it a zero-setup prompt-quality check for CI.

---

## Your job

Edit `app/analyze.py`:

- **`TODO(lab7-1)`** `build_prompt(finding)` — emit the facts as `KEY: value`
  lines (`SOURCE`, `SINK`, `VIA`, `PAYLOAD`) and instruct the model to return
  **strict JSON** with `severity`, `explanation`, `poc`.
- **`TODO(lab7-2)`** `main()` — `build_prompt` → `llm.generate` → `json.loads` →
  `print_report`. (Bonus: salvage the largest `{...}` block if a real model wraps
  JSON in prose.)

Both LLM backends, the mock, and `print_report` are provided.

---

## Run it

```bash
cd lab07-llm-analysis

python3 app/analyze.py app/finding.json      # opencode + ais3/gemma-4-26b (or mock if no opencode)
# or (always mock — no opencode in the container):
docker build -t rasplab-lab7 . && docker run --rm rasplab-lab7

./verify.sh                                   # grades against the mock (deterministic)
```

Expected (mock backend, i.e. CI / no opencode):

```
=== rasplab LLM report (backend: mock) ===
severity : high
chain    : req.json[note] (svc_a) --redis:order:42--> mysqli_query (svc_b)

[explanation]
Second-order vulnerability. Untrusted input from req.json[note] is persisted at
redis:order:42 ... flows unsanitized into mysqli_query ...

[poc]
1) send note=' OR 1=1--  (stored at redis:order:42)
2) trigger the request that reads redis:order:42 and passes it to mysqli_query
   => mysqli_query executes the injected payload "' OR 1=1--"
```

---

## Hints

<details><summary>Hint: prompt shape</summary>

```
SOURCE: req.json[note]
SINK: mysqli_query
VIA: redis:order:42
PAYLOAD: ' OR 1=1--

...explain + PoC... Return STRICT JSON with keys "severity","explanation","poc".
```
</details>

<details><summary>Hint: real models are chatty</summary>

They sometimes wrap JSON in ```json fences or prose. `re.search(r"\{.*\}", raw, re.S)`
salvages the block; then `json.loads`.
</details>

---

## Checklist

- [ ] report explains it as **second-order**
- [ ] PoC uses the real payload `' OR 1=1--` and names `mysqli_query`
- [ ] `./verify.sh` prints `✓ PASS`
- [ ] with opencode installed, it runs against `ais3/gemma-4-26b` by default

🎉 That's the whole pipeline: **Runtime (C) → Redis → Python correlation → LLM
report.** You built a second-order detector from an opcode hook up.
