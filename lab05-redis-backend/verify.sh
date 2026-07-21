#!/usr/bin/env bash
# Lab 5 grader: taint crosses SERVICES via Redis.
#   svc_a save.php?c=payload → [EXPORT] SETEX into Redis
#   svc_b view.php           → [RECOVER] GET from Redis → [ALERT]
# svc_b never saw the input; Redis carried the taint. That's the win.
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

# pre-build the image compose reuses (lets us pick ext/ vs solution/)
BUILD_CTX=".build_ctx"; rm -rf "$BUILD_CTX"; mkdir -p "$BUILD_CTX/ext" "$BUILD_CTX/app"
cp ext/config.m4 ext/php_rasplab.h "$BUILD_CTX/ext"/; cp "$SRC_DIR"/rasplab.c "$BUILD_CTX/ext"/
cp app/*.php "$BUILD_CTX/app"/; cp Dockerfile "$BUILD_CTX"/

echo "==> building image ($SRC_DIR) ..."
docker build -q -t rasplab-lab5:test "$BUILD_CTX" >/dev/null

PROJ="rasplab_lab5_verify"
compose() { docker compose -p "$PROJ" -f docker-compose.yml "$@"; }
cleanup(){ compose down -v >/dev/null 2>&1 || true; rm -rf "$BUILD_CTX"; }
trap cleanup EXIT

echo "==> starting redis + svc_a + svc_b ..."
compose up -d >/dev/null 2>&1
sleep 4

# hit svc_a (save) then svc_b (view), each via its own in-container server
compose exec -T svc_a php -r '@file_get_contents("http://127.0.0.1:8080/save.php?c=INJECTED_XSS");' || true
compose exec -T svc_b php -r '@file_get_contents("http://127.0.0.1:8080/view.php");' || true
sleep 1

logs_a="$(compose logs svc_a 2>&1)"
logs_b="$(compose logs svc_b 2>&1)"
echo "----- svc_a -----"; echo "$logs_a" | grep -E 'EXPORT|ALERT' || true
echo "----- svc_b -----"; echo "$logs_b" | grep -E 'RECOVER|ALERT' || true
echo "-----------------"

fail=0
grep -q '\[EXPORT\]'  <<<"$logs_a" && echo "  ✓ svc_a exported taint to Redis" || { echo "  ✗ svc_a no [EXPORT] → TODO(lab5-1)"; fail=1; }
grep -q '\[RECOVER\]' <<<"$logs_b" && echo "  ✓ svc_b recovered taint from Redis" || { echo "  ✗ svc_b no [RECOVER] → TODO(lab5-2)"; fail=1; }
if grep -q '\[ALERT\].*INJECTED_XSS' <<<"$logs_b"; then
  echo "  ✓ cross-SERVICE second-order DETECTED 🎯 (svc_b never saw the input)"
else
  echo "  ✗ svc_b did not alert → recovery/marking incomplete"; fail=1
fi

[ "$fail" -eq 0 ] && echo "✓ PASS — Lab 5 owned. Taint crossed the service boundary." || { echo "✗ FAIL"; exit 1; }
