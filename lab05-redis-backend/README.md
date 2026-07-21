# Lab 5 — Redis Backend

> **Mission**: take the export/recover pattern from Lab 4 and move the carrier
> from a local file to **Redis**. One swap, three wins: taint crosses **services**
> (not just requests), it **expires** (TTL), and there's no local sidecar to rot.

```
svc_a  save.php:  file_put_contents(tainted) ─▶ SETEX taint:<loc> <uuid>  ─┐
                                                                           Redis
svc_b  view.php:  file_get_contents()        ─▶ GET   taint:<loc>  ────────┘
                                             ─▶ re-mark ─▶ echo → [ALERT] 🎯
```

`svc_b` is a **different container** that never touched `$_GET`. Redis carries the
taint across the boundary.

---

## Objectives

1. Why an out-of-process store makes taint cross-service "for free".
2. Speak just enough **RESP** to `SETEX` / `GET` against Redis.
3. Location-derived keys as the cross-service join (`taint:<location>`), plus TTL.

---

## Your job

Same two decisions as Lab 4, now over Redis. A tiny client is provided
(`tl_redis_command(argc, argv, reply, sz)` — raw socket + RESP encoding).

- **`TODO(lab5-1)`** EXPORT: `SETEX <key> <TTL> <uuid>`
  ```c
  const char *argv[] = { "SETEX", key, TAINT_TTL, id };
  tl_redis_command(4, argv, reply, sizeof(reply));
  ```
- **`TODO(lab5-2)`** RECOVER: `GET <key>`, and treat a non-nil reply as tainted
  ```c
  const char *argv[] = { "GET", key };
  tl_redis_command(2, argv, reply, sizeof(reply));
  // hit: "$<len>\r\n<id>\r\n"   miss: "$-1\r\n"
  if (reply[0]=='$' && strncmp(reply,"$-1",3)!=0) tl_mark(Z_STR_P(return_value));
  ```

Key + uuid + the whole socket/RESP layer are provided.

---

## Run it

```bash
cd lab05-redis-backend
docker compose up --build -d        # redis + svc_a + svc_b (shared /data volume)

# write on service A
docker compose exec svc_a php -r '@file_get_contents("http://127.0.0.1:8080/save.php?c=<script>alert(1)</script>");'
# read on service B
docker compose exec svc_b php -r '@file_get_contents("http://127.0.0.1:8080/view.php");'

docker compose logs svc_a   # [EXPORT] taint:/data/note.txt -> ...
docker compose logs svc_b   # [RECOVER] ... + [ALERT] ...

docker compose down -v

./verify.sh                  # does all of the above and grades it
```

Pass = `[EXPORT]` on A, `[RECOVER]`+`[ALERT]` on B.

---

## Peek at Redis

```bash
docker compose exec redis redis-cli KEYS 'taint:*'
docker compose exec redis redis-cli TTL  'taint:/data/note.txt'   # counting down
```

---

## Hints

<details><summary>Why SETEX and not SET?</summary>

`SETEX key ttl val` bundles a time-to-live. Taint shouldn't live forever — stale
taint = false positives later. Bare `SET` works too; you'd add `EXPIRE` after.
</details>

<details><summary>RESP in one breath</summary>

An array of bulk strings: `*<argc>\r\n` then, per arg, `$<len>\r\n<bytes>\r\n`.
`SET a b` → `*3\r\n$3\r\nSET\r\n$1\r\na\r\n$1\r\nb\r\n`. (Encoder is provided.)
</details>

---

## Known rough edges (discuss)

- Key is the raw location string — no normalization; two names for one file dodge it.
- One value per key (presence). The provenance chain (parent/merged) is Lab 6.
- New TCP connection per op — fine for a lab, a pool/persistent conn for real.
- No auth/TLS to Redis — lab network only.

---

## Checklist

- [ ] `[EXPORT]` on svc_a, `[RECOVER]`+`[ALERT]` on svc_b
- [ ] `redis-cli KEYS 'taint:*'` shows your key with a TTL
- [ ] `./verify.sh` prints `✓ PASS`

➡️ **Lab 6 — Python Correlation**: the extension has been emitting events; now
step out of C and reconstruct the **provenance graph** offline in Python to
pinpoint the second-order chain end to end.
