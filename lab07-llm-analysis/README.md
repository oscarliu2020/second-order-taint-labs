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

## The offline trick

`MockLLM` answers using the `KEY: value` facts it finds **in your prompt**. If
your prompt forgets the sink or the payload, the "model" literally can't mention
them and the grader fails. So the mock doubles as a prompt-quality check — no API
key, no flakiness, still meaningful.

To use a real model instead:

```bash
pip install anthropic
export ANTHROPIC_API_KEY=sk-ant-...
python3 app/analyze.py app/finding.json      # now hits claude-sonnet-5
```

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

python3 app/analyze.py app/finding.json      # offline mock
# or
docker build -t rasplab-lab7 . && docker run --rm rasplab-lab7

./verify.sh
```

Expected (mock backend):

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
- [ ] (optional) it works against a real model with `ANTHROPIC_API_KEY`

🎉 That's the whole pipeline: **Runtime (C) → Redis → Python correlation → LLM
report.** You built a second-order detector from an opcode hook up.
