#!/usr/bin/env bash
# Lab 7 grader: with the offline mock, the report must actually reference the
# real facts (sink + payload) — which only happens if the prompt carried them.
# Usage: ./verify.sh [solution]
set -euo pipefail
cd "$(dirname "$0")"
SRC="${1:-app}"; LAB="$(basename "$(pwd)")"
CODE="app/analyze.py"
[ "$SRC" = "solution" ] && CODE="../../solutions/$LAB/analyze.py"
[ -f "$CODE" ] || { echo "no source at $CODE"; exit 1; }

BUILD_CTX=".build_ctx"; rm -rf "$BUILD_CTX"; mkdir -p "$BUILD_CTX/app"
cp app/finding.json "$BUILD_CTX/app/"; cp "$CODE" "$BUILD_CTX/app/analyze.py"; cp Dockerfile "$BUILD_CTX/"
cleanup(){ rm -rf "$BUILD_CTX"; }; trap cleanup EXIT

echo "==> building ($SRC) ..."
docker build -q -t rasplab-lab7-verify "$BUILD_CTX" >/dev/null
OUT="$(docker run --rm rasplab-lab7-verify 2>&1 || true)"
echo "----- output -----"; echo "$OUT"; echo "------------------"

fail=0
grep -qi 'second-order'      <<<"$OUT" && echo "  ✓ explains it as second-order"      || { echo "  ✗ no second-order explanation"; fail=1; }
grep -q  'mysqli_query'      <<<"$OUT" && echo "  ✓ prompt carried the SINK"           || { echo "  ✗ sink missing → check build_prompt (TODO lab7-1)"; fail=1; }
grep -qF "' OR 1=1--"        <<<"$OUT" && echo "  ✓ PoC uses the real payload"         || { echo "  ✗ payload missing from report"; fail=1; }
grep -qi '\[poc\]'           <<<"$OUT" && echo "  ✓ report includes a PoC section"     || { echo "  ✗ no PoC section → check TODO(lab7-2)"; fail=1; }

[ "$fail" -eq 0 ] && echo "✓ PASS — Lab 7 owned. The engine now explains itself." || { echo "✗ FAIL"; exit 1; }
