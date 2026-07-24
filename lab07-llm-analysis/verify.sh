#!/usr/bin/env bash
# Lab 7 grader: run the exploit against a live vulnerable target and check it
# exfiltrated the (randomised) flag via the second-order SQLi.
#
# The grader spins up its OWN isolated target on a separate host port, so it
# won't clash with a target you started manually on :8090 for exploit dev.
# Usage: ./verify.sh [solution]
set -euo pipefail
cd "$(dirname "$0")"

SRC="${1:-.}"
EXPLOIT="exploit.sh"
[ "$SRC" = "solution" ] && EXPLOIT="../../solutions/lab07-llm-analysis/exploit.sh"
[ -f "$EXPLOIT" ] || { echo "✗ no exploit at $EXPLOIT"; exit 1; }

export LAB7_PORT="${LAB7_PORT:-18090}"          # grader's own host port
BASE="http://localhost:${LAB7_PORT}"
PROJ="rasplab_lab7_verify"
compose() { docker compose -p "$PROJ" -f docker-compose.yml "$@"; }
cleanup() { compose down -v >/dev/null 2>&1 || true; }
trap cleanup EXIT

compose down -v >/dev/null 2>&1 || true          # clean slate
echo "==> building & starting target on :${LAB7_PORT} ..."
if ! compose up -d --build >/dev/null 2>&1; then
  echo "✗ target failed to start (port ${LAB7_PORT} busy? set LAB7_PORT=<free port>)"; exit 1
fi

# wait for the target to answer
up=0
for i in $(seq 1 30); do
  if curl -sf "${BASE}/" >/dev/null 2>&1; then up=1; break; fi
  sleep 1
done
if [ "$up" -ne 1 ]; then
  echo "✗ target never came up on ${BASE}"; compose logs target 2>&1 | tail -20; exit 1
fi

flag="$(compose exec -T target cat /flag.txt 2>/dev/null | tr -d '\r\n')"
[ -n "$flag" ] || { echo "✗ could not read target flag"; exit 1; }
echo "   target up on ${BASE}; flag hidden in DB (only the SQLi reaches it)"

echo "==> running exploit ($SRC) against ${BASE} ..."
OUT="$(TARGET="$BASE" bash "$EXPLOIT" 2>&1 || true)"
echo "----- exploit output -----"; echo "$OUT"; echo "--------------------------"

if grep -qF "$flag" <<<"$OUT"; then
  echo "  ✓ exploit exfiltrated the flag via second-order SQLi"
  echo "✓ PASS — Lab 7 owned. You weaponised the trace into a working exploit."
else
  echo "  ✗ flag not found in exploit output — the SQLi didn't fire"
  echo "    (fill exploit.sh via opencode: poison /note, then trigger /render)"
  echo "✗ FAIL"; exit 1
fi
