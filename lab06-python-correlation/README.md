# Lab 6 — Python Correlation

> **Mission**: step out of C. The extension has been streaming taint events; now
> reconstruct the **provenance graph** offline and pinpoint second-order chains
> end to end — even when the two halves happened in different requests/services.

```
SOURCE ─▶ EXPORT@loc  ┅┅ join by loc ┅┅  RECOVER@loc ─▶ SINK
(req A)                                             (req B)   ⇒ SECOND-ORDER
```

---

## Objectives

1. Why offline correlation is the right place to see the *whole* chain (the
   runtime only ever sees one request at a time).
2. Stitch per-request subgraphs together by **storage location** — the join key.
3. Classify: cross-request/service = second-order; same-request = first-order.

---

## The trace

`app/trace.jsonl` — one JSON event per line, as the extension would emit:

```json
{"req":"r1","event":"SOURCE","uuid":"t1","source":"$_GET[c]","value":"<script>…"}
{"req":"r1","event":"EXPORT","uuid":"t1","loc":"/data/note.txt"}
{"req":"r2","event":"RECOVER","uuid":"t2","loc":"/data/note.txt"}
{"req":"r2","event":"SINK","uuid":"t2","sink":"echo","value":"<script>…"}
```

Note `t2` (the recover in r2) has **no parent** — r2 never saw r1. Your job is to
recover that edge from the shared `loc`.

---

## Your job

Edit `app/correlate.py`:

- **`TODO(lab6-1)`** — for each `RECOVER`, find the `EXPORT` with the same `loc`
  and record the edge: `parent[recover.uuid] = export.uuid`.
- **`TODO(lab6-2)`** — for each `SINK`, walk back to its `SOURCE` (helper
  `trace_back` provided) and flag **second-order** when source and sink are in
  different requests.

Parsing, indexing, `trace_back`, and the report printer are provided.

---

## Run it

```bash
cd lab06-python-correlation

# quick local run (no Docker needed if you have python3)
python3 app/correlate.py app/trace.jsonl

# or containerized
docker build -t rasplab-lab6 . && docker run --rm rasplab-lab6

./verify.sh
```

Expected:

```
[SECOND-ORDER] $_GET[c] (req r1) --/data/note.txt--> echo (req r2)   payload='<script>alert(1)</script>'
[SECOND-ORDER] req.json[note] (req svc_a) --redis:order:42--> mysqli_query (req svc_b)   payload="' OR 1=1--"

summary: 2 second-order finding(s) across 3 sink(s)
```

The reflected case in `r3` (source and sink in the same request) must **not** be
reported as second-order.

---

## Hints

<details><summary>Hint: the location index</summary>

Build `loc -> [exports]` once, then each recover is a dict lookup:
```python
by_loc = {}
for ex in exports: by_loc.setdefault(ex["loc"], []).append(ex)
for rec in recovers:
    m = by_loc.get(rec["loc"])
    if m: parent[rec["uuid"]] = m[-1]["uuid"]
```
</details>

<details><summary>Hint: second-order test</summary>

`request_of[root_source_uuid] != request_of[sink_uuid]`. Same request → first-order.
</details>

---

## Going further (optional)

- Multi-hop chains (A→B→C across three requests) — `trace_back` already handles depth.
- Merge nodes (`CONCAT` of two tainted inputs) → a real DAG, not just a chain.
- Emit the graph as DOT and render it — this is the "trace visualization" slide.

---

## Checklist

- [ ] both second-order chains reported (file + redis)
- [ ] `r3` not flagged as second-order
- [ ] `summary: 2 second-order`
- [ ] `./verify.sh` prints `✓ PASS`

➡️ **Lab 7 — LLM Analysis**: feed a reconstructed chain to an LLM to explain the
bug in English and draft a PoC — with an offline stub so it runs without an API key.
