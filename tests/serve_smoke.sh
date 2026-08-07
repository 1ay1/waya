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
hdrs="$(curl -sD - -o /dev/null "http://127.0.0.1:$PORT/" 2>/dev/null)"

fail=0
case "$code" in 200) : ;; *) echo "FAIL: status $code (want 200)"; fail=1 ;; esac
case "$body" in *'id="root"'*) : ;; *) echo "FAIL: no #root shell"; fail=1 ;; esac
case "$body" in *"new WebSocket"*) : ;; *) echo "FAIL: no client runtime"; fail=1 ;; esac

# ── production HTTP correctness ──────────────────────────────────────────────
# Security headers a hardened SSR server always sends.
for H in "X-Content-Type-Options: nosniff" "X-Frame-Options" "Content-Security-Policy" "Referrer-Policy"; do
    case "$hdrs" in *"$H"*) : ;; *) echo "FAIL: missing header '$H'"; fail=1 ;; esac
done
# The live HTML must not be cached stale.
case "$hdrs" in *"Cache-Control: no-store"*) : ;; *) echo "FAIL: HTML not no-store"; fail=1 ;; esac
# Correct content type.
case "$hdrs" in *"text/html; charset=utf-8"*) : ;; *) echo "FAIL: wrong content-type"; fail=1 ;; esac

# HEAD returns headers with no body.
hbody="$(printf 'HEAD / HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n' | { command -v nc >/dev/null 2>&1 && nc 127.0.0.1 "$PORT"; } 2>/dev/null | awk 'f{print} /^\r?$/{f=1}')"
if command -v nc >/dev/null 2>&1; then
    case "$hbody" in "") : ;; *) echo "FAIL: HEAD returned a body"; fail=1 ;; esac
fi

# A non-GET/HEAD method is 405 with an Allow header.
pcode="$(curl -s -o /dev/null -w '%{http_code}' -X POST "http://127.0.0.1:$PORT/" 2>/dev/null)"
case "$pcode" in 405) : ;; *) echo "FAIL: POST got $pcode (want 405)"; fail=1 ;; esac

# favicon is a cheap 204, not an SSR of the app.
fcode="$(curl -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$PORT/favicon.ico" 2>/dev/null)"
case "$fcode" in 204) : ;; *) echo "FAIL: favicon got $fcode (want 204)"; fail=1 ;; esac

# robots.txt is served and cacheable.
rhdrs="$(curl -sD - -o /dev/null "http://127.0.0.1:$PORT/robots.txt" 2>/dev/null)"
case "$rhdrs" in *"200 OK"*) : ;; *) echo "FAIL: robots.txt not 200"; fail=1 ;; esac
case "$rhdrs" in *"max-age"*) : ;; *) echo "FAIL: robots.txt not cacheable"; fail=1 ;; esac

if [ "$fail" -eq 0 ]; then echo "serve_smoke: OK (200 shell+client, security headers, HEAD/405/favicon/robots)"; fi
exit "$fail"
