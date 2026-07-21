#!/usr/bin/env bash
# Lab 3 grader: prove the blind spot.
#   save.php?c=payload  → must log [STORE-TAINT] (taint reached storage)
#   view.php            → must NOT [ALERT] on the payload (detector is blind)
# That "correct failure" is the whole point — Lab 4 fixes it.
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
docker build -q -t rasplab-lab3-verify --build-arg SRC="$BUILD_CTX" -f Dockerfile . >/dev/null

cid="$(docker run -d rasplab-lab3-verify)"
cleanup(){ docker rm -f "$cid" >/dev/null 2>&1 || true; rm -rf "$BUILD_CTX"; }
trap cleanup EXIT
sleep 2
docker exec "$cid" php -r '@file_get_contents("http://127.0.0.1:8080/save.php?c=INJECTED_XSS");' || true
docker exec "$cid" php -r '@file_get_contents("http://127.0.0.1:8080/view.php");' || true
sleep 1
logs="$(docker logs "$cid" 2>&1)"
echo "----- server stderr -----"; echo "$logs"; echo "-------------------------"

fail=0
if echo "$logs" | grep -q '\[STORE-TAINT\].*INJECTED_XSS'; then
  echo "  ✓ taint detected reaching storage (file_put_contents hook)"
else
  echo "  ✗ storage sink not detected → check TODO(lab3-1)"; fail=1
fi
if echo "$logs" | grep -q '\[ALERT\].*INJECTED_XSS'; then
  echo "  ✗ unexpected: view echo alerted — for THIS lab it should be blind"; fail=1
else
  echo "  ✓ read-back echo went undetected (the second-order blind spot — expected)"
fi

if [ "$fail" -eq 0 ]; then echo "✓ PASS — Lab 3 owned. You've reproduced the failure."; else echo "✗ FAIL"; exit 1; fi
