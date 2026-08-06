#!/bin/sh
# tests/serve_smoke.sh - verify a waya app serves its shell + client.
# A waya app's page is a bare shell (#root) plus the client that streams the
# surface over a WebSocket; the UI itself never appears in the initial HTML.

set -u
APP="${1:?path to app binary}"
PORT="${2:-8137}"
LOG="${TMPDIR:-/tmp}/waya_serve.$$.log"

WAYA_PORT="$PORT" WAYA_NO_OPEN=1 "$APP" >"$LOG" 2>&1 &
PID=$!
trap 'kill "$PID" 2>/dev/null; rm -f "$LOG" 2>/dev/null' EXIT

i=0
while [ "$i" -lt 30 ]; do
    if curl -s -o /dev/null "http://127.0.0.1:$PORT/" 2>/dev/null; then break; fi
    i=$((i + 1)); sleep 0.1
done

body="$(curl -s "http://127.0.0.1:$PORT/" 2>/dev/null)"
code="$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT/" 2>/dev/null)"

fail=0
case "$code" in 200) : ;; *) echo "FAIL: status $code (want 200)"; fail=1 ;; esac
case "$body" in *'id="root"'*) : ;; *) echo "FAIL: no #root shell"; fail=1 ;; esac
case "$body" in *"new WebSocket"*) : ;; *) echo "FAIL: no client runtime"; fail=1 ;; esac

if [ "$fail" -eq 0 ]; then echo "serve_smoke: OK (200, shell + client present)"; fi
exit "$fail"
