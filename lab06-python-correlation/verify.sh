#!/usr/bin/env bash
# Lab 6 grader: correlate.py must reconstruct the cross-request/service chains.
# Expected: 2 second-order findings (file /data/note.txt, redis:order:42);
#           the first-order case (req r3) must NOT be reported as second-order.
# Usage: ./verify.sh [solution]
set -euo pipefail
cd "$(dirname "$0")"
SRC="${1:-app}"; LAB="$(basename "$(pwd)")"
CODE="app/correlate.py"
[ "$SRC" = "solution" ] && CODE="../../solutions/$LAB/correlate.py"
[ -f "$CODE" ] || { echo "no source at $CODE"; exit 1; }

BUILD_CTX=".build_ctx"; rm -rf "$BUILD_CTX"; mkdir -p "$BUILD_CTX/app"
cp app/trace.jsonl "$BUILD_CTX/app/"; cp "$CODE" "$BUILD_CTX/app/correlate.py"; cp Dockerfile "$BUILD_CTX/"
cleanup(){ rm -rf "$BUILD_CTX"; }; trap cleanup EXIT

echo "==> building ($SRC) ..."
docker build -q -t rasplab-lab6-verify "$BUILD_CTX" >/dev/null
OUT="$(docker run --rm rasplab-lab6-verify 2>&1 || true)"
echo "----- output -----"; echo "$OUT"; echo "------------------"

fail=0
grep -q 'SECOND-ORDER.*/data/note.txt'  <<<"$OUT" && echo "  ✓ found file-based stored XSS (r1→r2)" || { echo "  ✗ missed file chain → TODO(lab6-1/2)"; fail=1; }
grep -q 'SECOND-ORDER.*redis:order:42'  <<<"$OUT" && echo "  ✓ found redis cross-service SQLi (svc_a→svc_b)" || { echo "  ✗ missed redis chain"; fail=1; }
if grep -qE 'SECOND-ORDER.*req r3' <<<"$OUT"; then echo "  ✗ first-order case r3 mislabeled as second-order"; fail=1; else echo "  ✓ first-order (r3) not mislabeled"; fi
grep -q 'summary: 2 second-order' <<<"$OUT" && echo "  ✓ exactly 2 second-order findings" || { echo "  ✗ wrong finding count"; fail=1; }

[ "$fail" -eq 0 ] && echo "✓ PASS — Lab 6 owned. Provenance graph reconstructed." || { echo "✗ FAIL"; exit 1; }
