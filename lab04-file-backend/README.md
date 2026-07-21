# Lab 4 — File Backend

> **Mission**: kill Lab 3's blind spot. *Persist taint, not data.* When tainted
> bytes cross into storage, drop a taint record keyed by the storage location.
> When they come back out, recover it and re-mark the string. The stored-XSS
> `echo` finally screams — across two separate requests.

```
save.php:  file_put_contents(tainted)  ─▶ [EXPORT]  record → /data/.taint.jsonl
view.php:  file_get_contents()         ─▶ [RECOVER] record → re-mark string
                                       ─▶ echo → [ALERT]  🎯
```

---

## Objectives

1. Why the taint carrier must live **outside** the process (disk here, Redis next).
2. A real (if tiny) **data model**: `{loc, uuid, src}` keyed by storage location.
3. The symmetric **export / recover** pattern — the heart of persistent taint.

---

## The idea

Request #1's memory is gone by request #2 — so the bridge has to be external.
The *location* of the data (here, the filename) is the stable key both requests
agree on:

```
write("/data/note.txt", tainted)   →  store["/data/note.txt"] = {uuid, src}
read("/data/note.txt")             →  if store has it: re-taint the result
```

---

## Your job

- **`TODO(lab4-1)`** — EXPORT in the `file_put_contents` hook: on a tainted write,
  mint a uuid and `tl_persist_write(loc, uuid, "user_input")`.
- **`TODO(lab4-2)`** — RECOVER in the `file_get_contents` hook: after the real read,
  if `tl_persist_lookup(loc)`, `tl_mark(Z_STR_P(return_value))`.

Provided: taint set, echo sink, both function hooks, `tl_uuid()`, and the sidecar
read/write (`tl_persist_write` / `tl_persist_lookup`). You own the two decisions.

---

## Run it

```bash
cd lab04-file-backend
docker build -t rasplab-lab4 .
docker run --rm -p 8080:8080 rasplab-lab4

curl 'http://localhost:8080/save.php?c=<script>alert(1)</script>'   # [EXPORT]
curl 'http://localhost:8080/view.php'                              # [RECOVER] + [ALERT]

./verify.sh
```

`verify.sh` passes when **EXPORT**, **RECOVER**, and a cross-request **ALERT** all fire.

---

## Hints

<details><summary>Hint: recover runs AFTER the real read</summary>

`file_get_contents` fills `return_value`. So call the original first (already
done for you), *then* inspect `return_value` — that's the fresh string to mark.
</details>

<details><summary>Why key by location and not by content?</summary>

Location (filename / table.column / redis key) is stable and cheap. Content
hashing also works and dedups, but breaks the moment data is transformed. Real
engines often use both; location is the teachable minimum. (Content-hash keying
is a great bonus exercise.)
</details>

---

## Known rough edges (by design — discuss them)

- Sidecar is append-only JSONL and never GC'd → grows forever, and a recycled
  location reads stale taint. Redis + TTL (Lab 5) is the grown-up version.
- Location match is exact-string; symlinks / relative paths would dodge it.
- We store presence, not the full provenance chain yet — that's Lab 6.

---

## Checklist

- [ ] `[EXPORT]` on save, `[RECOVER]` on view
- [ ] cross-request `[ALERT]` on the payload
- [ ] `./verify.sh` prints `✓ PASS`
- [ ] you can explain why location is the join key

➡️ **Lab 5 — Redis Backend**: swap the file sidecar for Redis over RESP, so taint
survives not just across requests but across **services** — and gets a TTL.
