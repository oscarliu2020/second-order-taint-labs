#!/usr/bin/env bash
# Lab 4 grader: persistence closes the loop.
#   save.php?c=payload → [EXPORT]  (taint record written to sidecar)
#   view.php           → [RECOVER] then [ALERT] on the payload  🎯
# Usage: ./verify.sh [solution]
set -euo pipefail
cd "$(dirname "$0")"

SRC="${1:-ext}"
LAB="$(basename "$(pwd)")"
SRC_DIR="ext"
if [ "$SRC" = "solution" ]; then
  SRC_DIR="../../solutions/$LAB"
  [ -f "$SRC_DIR/rasplab.c" ] || { echo "no solution at $SRC_DIR (instructor-only)"; exit 1; }
fi

BUILD_CTX=".build_ctx"; rm -rf "$BUILD_CTX"; mkdir -p "$BUILD_CTX"
cp ext/config.m4 ext/php_rasplab.h "$BUILD_CTX"/; cp "$SRC_DIR"/rasplab.c "$BUILD_CTX"/

echo "==> building ($SRC_DIR) ..."
docker build -q -t rasplab-lab4-verify --build-arg SRC="$BUILD_CTX" -f Dockerfile . >/dev/null

cid="$(docker run -d rasplab-lab4-verify)"
cleanup(){ docker rm -f "$cid" >/dev/null 2>&1 || true; rm -rf "$BUILD_CTX"; }
trap cleanup EXIT
sleep 2
docker exec "$cid" php -r '@file_get_contents("http://127.0.0.1:8080/save.php?c=INJECTED_XSS");' || true
docker exec "$cid" php -r '@file_get_contents("http://127.0.0.1:8080/view.php");' || true
sleep 1
logs="$(docker logs "$cid" 2>&1)"
echo "----- server stderr -----"; echo "$logs"; echo "-------------------------"

fail=0
grep -q '\[EXPORT\]'  <<<"$logs" && echo "  ✓ taint exported on write" || { echo "  ✗ no [EXPORT] → check TODO(lab4-1)"; fail=1; }
grep -q '\[RECOVER\]' <<<"$logs" && echo "  ✓ taint recovered on read" || { echo "  ✗ no [RECOVER] → check TODO(lab4-2)"; fail=1; }
if grep -q '\[ALERT\].*INJECTED_XSS' <<<"$logs"; then
  echo "  ✓ cross-request stored XSS DETECTED 🎯"
else
  echo "  ✗ view echo still blind → recovery didn't re-mark the string"; fail=1
fi

[ "$fail" -eq 0 ] && echo "✓ PASS — Lab 4 owned. Taint survived the request." || { echo "✗ FAIL"; exit 1; }
