#!/usr/bin/env bash
# Lab 1 grader: compile your ext/ and check the opcode trace shows up.
# Usage:
#   ./verify.sh            grade your work (ext/)
#   ./verify.sh solution   grade the reference impl (solution/)
set -euo pipefail
cd "$(dirname "$0")"

SRC="${1:-ext}"
LAB="$(basename "$(pwd)")"
if [ "$SRC" = "solution" ]; then
  SRC_DIR="../../solutions/$LAB"      # instructor-only, lives outside labs/
  if [ ! -f "$SRC_DIR/rasplab.c" ]; then
    echo "no solution at $SRC_DIR (instructor-only; not shipped to students)"; exit 1
  fi
else
  SRC_DIR="ext"
fi

# the solution ships only rasplab.c — stitch it with config.m4 + header.
BUILD_CTX=".build_ctx"
rm -rf "$BUILD_CTX"; mkdir -p "$BUILD_CTX"
cp ext/config.m4 ext/php_rasplab.h "$BUILD_CTX"/
cp "$SRC_DIR"/rasplab.c "$BUILD_CTX"/

echo "==> building ($SRC_DIR) ..."
docker build -q -t rasplab-lab1-verify --build-arg SRC="$BUILD_CTX" \
  -f Dockerfile . >/dev/null

echo "==> running demo.php ..."
OUT="$(docker run --rm rasplab-lab1-verify php demo.php 2>&1 1>/dev/null || true)"

echo "----- stderr (opcode trace) -----"
echo "$OUT"
echo "---------------------------------"

fail=0
for op in ZEND_ASSIGN ZEND_CONCAT ZEND_ECHO; do
  if echo "$OUT" | grep -q "\[opcode\] .*$op"; then
    echo "  ✓ hooked $op"
  else
    echo "  ✗ missed $op"
    fail=1
  fi
done
# calls: accept any member of the call family (internal=ICALL, user=UCALL, by-name=FCALL)
if echo "$OUT" | grep -qE "\[opcode\] .*ZEND_DO_(FCALL|ICALL|UCALL|FCALL_BY_NAME)"; then
  echo "  ✓ hooked a call opcode (ZEND_DO_*)"
else
  echo "  ✗ missed the call opcode"
  fail=1
fi

rm -rf "$BUILD_CTX"
if [ "$fail" -eq 0 ]; then
  echo "✓ PASS — Lab 1 owned."
else
  echo "✗ FAIL — check TODO(lab1-1) output format and TODO(lab1-2) registration."
  exit 1
fi
