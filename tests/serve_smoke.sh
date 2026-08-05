#!/bin/sh
# tests/serve_smoke.sh — verify the dev server binds and serves a page.
# Used by ctest. Starts `hello`, curls it, checks the response, tears down.

set -u
HELLO="${1:?path to hello binary}"
PORT="${2:-8137}"

# Start the server. WAYA_NO_OPEN keeps it from spawning a browser in CI.
WAYA_PORT="$PORT" WAYA_NO_OPEN=1 "$HELLO" >/tmp/waya_serve.log 2>&1 &
PID=$!
trap 'kill "$PID" 2>/dev/null' EXIT

# Wait for it to come up (up to ~3s).
i=0
while [ "$i" -lt 30 ]; do
    if curl -s -o /dev/null "http://127.0.0.1:$PORT/" 2>/dev/null; then break; fi
    i=$((i + 1)); sleep 0.1
done

body="$(curl -s "http://127.0.0.1:$PORT/" 2>/dev/null)"
code="$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT/" 2>/dev/null)"

fail=0
case "$code" in 200) : ;; *) echo "FAIL: status $code (want 200)"; fail=1 ;; esac
case "$body" in *"<title>waya</title>"*) : ;; *) echo "FAIL: no <title>"; fail=1 ;; esac
case "$body" in *"api-gateway"*) : ;; *) echo "FAIL: dynamic table missing"; fail=1 ;; esac
case "$body" in *"<style>"*) : ;; *) echo "FAIL: generated stylesheet missing"; fail=1 ;; esac

if [ "$fail" -eq 0 ]; then echo "serve_smoke: OK (200, page rendered, styles present)"; fi
exit "$fail"
