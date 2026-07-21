#!/usr/bin/env bash
# Lab 2 grader: boot the server, fire one tainted request + one clean request,
# assert the tainted one alerts and the clean one doesn't false-positive.
# Usage: ./verify.sh [solution]
set -euo pipefail
cd "$(dirname "$0")"

SRC="${1:-ext}"
LAB="$(basename "$(pwd)")"
SRC_DIR="ext"
if [ "$SRC" = "solution" ]; then
  SRC_DIR="../../solutions/$LAB"      # instructor-only, lives outside labs/
  if [ ! -f "$SRC_DIR/rasplab.c" ]; then
    echo "no solution at $SRC_DIR (instructor-only; not shipped to students)"; exit 1
  fi
fi

BUILD_CTX=".build_ctx"
rm -rf "$BUILD_CTX"; mkdir -p "$BUILD_CTX"
cp ext/config.m4 ext/php_rasplab.h "$BUILD_CTX"/
cp "$SRC_DIR"/rasplab.c "$BUILD_CTX"/

echo "==> building ($SRC_DIR) ..."
docker build -q -t rasplab-lab2-verify --build-arg SRC="$BUILD_CTX" -f Dockerfile . >/dev/null

echo "==> booting server ..."
cid="$(docker run -d rasplab-lab2-verify)"
cleanup() { docker rm -f "$cid" >/dev/null 2>&1 || true; rm -rf "$BUILD_CTX"; }
trap cleanup EXIT
sleep 2

# use the container's own php as the client (image has no curl)
docker exec "$cid" php -r '@file_get_contents("http://127.0.0.1:8080/demo.php?name=INJECTED_XSS");' || true
docker exec "$cid" php -r '@file_get_contents("http://127.0.0.1:8080/demo.php");' || true
sleep 1

logs="$(docker logs "$cid" 2>&1)"
echo "----- server stderr -----"
echo "$logs"
echo "-------------------------"

fail=0
if echo "$logs" | grep -q '\[ALERT\].*INJECTED_XSS'; then
  echo "  ✓ tainted echo raised an ALERT (with the payload)"
else
  echo "  ✗ tainted echo went undetected → check TODO(lab2-1)/(lab2-2)"
  fail=1
fi
# the clean request (no name → constant 'guest') must not show up in an ALERT
if echo "$logs" | grep -q '\[ALERT\].*guest'; then
  echo "  ✗ false positive: constant 'guest' should not be tainted"
  fail=1
else
  echo "  ✓ constant string did not false-positive"
fi

if [ "$fail" -eq 0 ]; then echo "✓ PASS — Lab 2 owned."; else echo "✗ FAIL"; exit 1; fi
